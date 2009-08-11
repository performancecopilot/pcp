/*
 * Copyright (C) 2009 Max Matveev. All Rights Reserved 
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

#include <libzfs.h>

#include "common.h"

static libzfs_handle_t *zh;
static int zf_added;

/*
 * For each zfs check if the name is in the instance cache.  If it's
 * not there then add it to the cache. If we've cached the new
 * instance then we keep the zfs_handle which we've received in the
 * argument, otherwise we need to close it - zfs_iter_root() expects that
 * from us.
 */
static int
zfs_cache_inst(zfs_handle_t *zf, void *arg)
{
    char *fsname = (char *)zfs_get_name(zf);
    pmInDom zfindom = indomtab[ZFS_INDOM].it_indom;
    zfs_handle_t *cached = NULL;
    uint_t cnt = 0;
    vdev_stat_t *vds;
    int inst, rv;
    nvlist_t *vdt;

    if ((rv = pmdaCacheLookupName(zfindom, fsname, &inst,
				  (void **)&cached)) == PMDA_CACHE_ACTIVE) {
	zfs_close(zf);
	if (arg == NULL)
	    zfs_refresh_properties(cached);

	zf = cached;
    } else if ((rv == PMDA_CACHE_INACTIVE) && cached) {
	rv = pmdaCacheStore(zfindom, PMDA_CACHE_ADD, fsname, cached);
	if (rv < 0) {
	    __pmNotifyErr(LOG_WARNING, 
			  "Cannot reactivate cached ZFS handle for '%s': %s\n",
			  fsname, pmErrStr(rv));
	    zfs_close(zf);
	    return 0;
	}
	zfs_close(zf);
	if (arg == NULL)
	    zfs_refresh_properties(cached);
	zf = cached;
    } else {
	rv = pmdaCacheStore(zfindom, PMDA_CACHE_ADD, fsname, zf);
	if (rv < 0) {
	    __pmNotifyErr(LOG_WARNING, 
			  "Cannot cache ZFS handle for '%s': %s\n",
			  fsname, pmErrStr(rv));
	    zfs_close(zf);
	    return 0;
	}
	zf_added++;
    }

    zfs_iter_filesystems(zf, zfs_cache_inst, NULL);

    return 0;
}

void
zfs_refresh(void)
{
    zf_added = 0;

    pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    zfs_iter_root(zh, zfs_cache_inst, NULL);

    if (zf_added) {
	pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}

int
zfs_fetch(pmdaMetric *pm, int inst, pmAtomValue *atom)
{
    char *fsname;
    metricdesc_t *md = pm->m_user;
    zfs_handle_t *zf;
    uint64_t v;

    if (pmdaCacheLookup(indomtab[ZFS_INDOM].it_indom, inst, &fsname,
			(void **)&zf) != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    v = zfs_prop_get_int(zf, md->md_offset);

    /* Special processing - compression ratio is in precent, we export it
     * as multiplier */
    switch (md->md_offset) {
    case ZFS_PROP_COMPRESSRATIO:
	atom->d = v / 100.0;
	break;
    default:
	atom->ull = v;
	break;
    }

    return 1;
}

void
zfs_init(int first)
{
    if (zh)
	return;

    zh = libzfs_init();
    if (zh) {
	pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_LOAD);
	zfs_iter_root(zh, zfs_cache_inst, &first);
	pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}
