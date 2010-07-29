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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

/* kstat has counters for vnode operations for each filesystem. */

#include <stdio.h>
#include <kstat.h>
#include <sys/mnttab.h>
#include "common.h"

const char *
vnops_iname(const char *dev)
{
	static char iname[128];
	struct mnttab m;
	FILE *f = fopen("/etc/mnttab", "r");
	if (!f)
		return dev;

	while(getmntent(f, &m) == 0) {
		char *devop= hasmntopt(&m, "dev");
		if (devop && (strcmp(devop+4, dev) == 0)) {
			snprintf(iname, sizeof(iname), "%s %s",
				 dev, m.mnt_mountp);
			fclose(f);
			return iname;
		}
	}
	fclose(f);
	return dev;
}

int
vnops_fetch(pmdaMetric *pm, int inst, pmAtomValue *av)
{
    char *fsname;
    metricdesc_t *md = pm->m_user;
    kstat_t *k;
    char *stat = (char *)md->md_offset;

    if (pmdaCacheLookup(indomtab[FILESYS_INDOM].it_indom, inst, &fsname,
                        (void **)&k) != PMDA_CACHE_ACTIVE)
        return PM_ERR_INST;

    if (k) {
	kstat_named_t *kn = kstat_data_lookup(k, stat);

	if (kn == NULL) {
	    fprintf(stderr, "No kstat called %s for %s\n", stat, fsname);
	    return 0;
	}

        switch (pm->m_desc.type) {
        case PM_TYPE_32:
	    if (kn->data_type == KSTAT_DATA_INT32) {
		av->l = kn->value.i32;
		return 1;
	    }
	    break;
        case PM_TYPE_U32:
	    if (kn->data_type == KSTAT_DATA_UINT32) {
		av->ul = kn->value.ui32;
		return 1;
	    }
	    break;
        case PM_TYPE_64:
	    if (kn->data_type == KSTAT_DATA_INT64) {
		av->ll = kn->value.i64;
		return 1;
	    }
	    break;
        case PM_TYPE_U64:
	    if (kn->data_type == KSTAT_DATA_UINT64) {
		av->ull = kn->value.ui64;
		return 1;
	    }
	    break;
	}
    }

    return 0;
}

void
vnops_update_stats(int fetch)
{
    kstat_t *k;
    pmInDom indom = indomtab[FILESYS_INDOM].it_indom;

    kstat_chain_update(kc);

    for (k = kc->kc_chain; k != NULL; k = k->ks_next) {
	int rv;
	kstat_t *cached;

	if (strcmp(k->ks_module, "unix") ||
	    strncmp(k->ks_name, "vopstats_", sizeof("vopstats_")-1))
	    continue;

	if (pmdaCacheLookupName(indom, k->ks_name + 9, &rv,
				(void **)&cached) != PMDA_CACHE_ACTIVE) {
	    const char *iname = vnops_iname(k->ks_name + 9);
	    rv = pmdaCacheStore(indom, PMDA_CACHE_ADD, iname, k);
	    if (rv < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "Cannot create instance for ilesystem '%s': %s\n",
			      k->ks_name, pmErrStr(rv));
		    continue;
	    } else {
		fprintf(stderr, "%s @ %p\n", k->ks_name, k);
	    }
        }

        if (fetch)
	    kstat_read(kc, k, NULL);
    }
}

void
vnops_refresh(void)
{
    vnops_update_stats(1);
}

void
vnops_init(int first)
{
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_LOAD);
    vnops_update_stats(0);
    pmdaCacheOp(indomtab[FILESYS_INDOM].it_indom, PMDA_CACHE_SAVE);
}
