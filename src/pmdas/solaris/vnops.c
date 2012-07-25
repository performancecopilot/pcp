/*
 * Copyright (C) 2010 Max Matveev. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/* kstat has counters for vnode operations for each filesystem.
 *
 * Unfortunately the counters for mounted fileystems are mixed with counters
 * for the filesystem types and there is no obvious way to distinguish
 * between the two except by trying to convert the kstat's name to the number
 * and see if works */

#include <stdio.h>
#include <kstat.h>
#include <sys/mnttab.h>
#include <sys/stat.h>
#include "common.h"

struct mountpoint {
    struct mountpoint *next;
    dev_t dev;
    char mountpoint[];
};

static struct mountpoint *mountpoints;
static struct timespec mtime;

/* NB! The order of entires in mountopoints list is important:
 * lofs mounts use the same device number but appear later
 * in /etc/mnttab then the target filesystem - keeping the
 * order the same as /etc/mnttab means that more "logical"
 * mountpoints are reported, in particular the counters
 * for "/" are not reported as /lib/libc.so.1 */
static void
cache_mnttab(void)
{
    FILE *f;
    struct mnttab m;
    struct mountpoint *mp;
    struct stat sb;
    struct mountpoint **tail = &mountpoints;

    if (stat("/etc/mnttab", &sb) < 0)
	return;

    if (mountpoints &&
	(sb.st_mtim.tv_sec == mtime.tv_sec) &&
	(sb.st_mtim.tv_nsec == mtime.tv_nsec))
	return;

    if ((f = fopen("/etc/mnttab", "r")) == NULL)
	    return;

    for (mp = mountpoints; mp; mp = mountpoints) {
	mountpoints = mp->next;
	free(mp);
    }

    while(getmntent(f, &m) == 0) {
	char *devop= hasmntopt(&m, "dev");
	if (devop) {
	    char *end;
	    dev_t d = strtoul(devop+4, &end, 16);

	    if ((end != devop+4) && (*end != '\0')) {
		fprintf(stderr, "Bogus device number %s for filesystem %s\n",
			devop+4, m.mnt_mountp);
		continue;
	    }

	    mp = malloc(sizeof(*mp) + strlen(m.mnt_mountp) + 1);
	    if (mp == NULL) {
		fprintf(stderr,
			"Cannot allocate memory for cache entry of %s\n",
			m.mnt_mountp);
		continue;
	    }
	    mp->next = NULL;
	    mp->dev = d;
	    strcpy(mp->mountpoint, m.mnt_mountp);
	    *tail = mp;
	    tail = &mp->next;
	}
    }
    fclose(f);
    mtime = sb.st_mtim;
}

static const char *
mountpoint_bydev(dev_t dev)
{
    int i;
    for (i=0; i < 2; i++) {
	struct mountpoint *mp = mountpoints;
	while(mp) {
	    if (mp->dev == dev)
		return mp->mountpoint;
	    mp = mp->next;
	}
	cache_mnttab();
    }
    return NULL;
}

int
vnops_fetch(pmdaMetric *pm, int inst, pmAtomValue *av)
{
    char *fsname;
    metricdesc_t *md = pm->m_user;
    kstat_t *k;
    char *stat = (char *)md->md_offset;

    if (pmid_item(pm->m_desc.pmid) == 1023) { /* hinv.nfilesys */
	int	sts;
	sts = pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_SIZE_ACTIVE);
	if (sts < 0)
	    return 0;
	else {
	    av->ul = sts;
	    return 1;
	}
    }

    if (pmdaCacheLookup(pm->m_desc.indom, inst, &fsname,
                        (void **)&k) != PMDA_CACHE_ACTIVE)
        return PM_ERR_INST;

    if (k) {
	kstat_named_t *kn = kstat_data_lookup(k, stat);

	if (kn == NULL) {
	    fprintf(stderr, "No kstat called %s for %s\n", stat, fsname);
	    return 0;
	}

	return kstat_named_to_typed_atom(kn, pm->m_desc.type, av);
    }

    return 0;
}

static void
vnops_update_stats(int fetch)
{
    kstat_t *k;
    kstat_ctl_t *kc = kstat_ctl_update();

    if (kc == NULL)
	return;

    for (k = kc->kc_chain; k != NULL; k = k->ks_next) {
	int rv;
	kstat_t *cached;
	const char *key;
	dev_t dev;
	char *end;
        pmInDom indom;

	if (strcmp(k->ks_module, "unix") ||
	    strncmp(k->ks_name, "vopstats_", sizeof("vopstats_")-1))
	    continue;

	key = k->ks_name + 9;
	dev = strtoul(key, &end, 16);
	if ((end != key) && (*end == '\0')) {
	    indom = indomtab[FILESYS_INDOM].it_indom;
	    if ((key = mountpoint_bydev(dev)) == NULL)
		continue;
	} else {
	    indom = indomtab[FSTYPE_INDOM].it_indom;
	}

	if (pmdaCacheLookupName(indom, key, &rv,
				(void **)&cached) != PMDA_CACHE_ACTIVE) {
	    rv = pmdaCacheStore(indom, PMDA_CACHE_ADD, key, k);
	    if (rv < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "Cannot create instance for "
			      "filesystem '%s': %s\n",
			      key, pmErrStr(rv));
		    continue;
	    }
        }

        if (fetch)
	    kstat_read(kc, k, NULL);
    }
}

void
vnops_refresh(void)
{
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(indomtab[FSTYPE_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    vnops_update_stats(1);
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_SAVE);
    pmdaCacheOp(indomtab[FSTYPE_INDOM].it_indom, PMDA_CACHE_SAVE);
}

void
vnops_init(int first)
{
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_LOAD);
    pmdaCacheOp(indomtab[FSTYPE_INDOM].it_indom, PMDA_CACHE_LOAD);
    vnops_update_stats(0);
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_SAVE);
    pmdaCacheOp(indomtab[FSTYPE_INDOM].it_indom, PMDA_CACHE_SAVE);
}
