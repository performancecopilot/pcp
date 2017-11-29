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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <libzfs.h>

#include "common.h"

static libzfs_handle_t *zh;
static int zf_added;

struct zfs_data {
    zfs_handle_t *zh;
    uint64_t nsnaps;
};

#if defined(HAVE_ZFS_ITER_SNAPSHOTS_4ARG)
# define zfs_iter_snapshots3(a,b,c)	zfs_iter_snapshots(a,B_FALSE,b,c)
#elif defined(HAVE_ZFS_ITER_SNAPSHOTS_3ARG)
# define zfs_iter_snapshots3(a,b,c)	zfs_iter_snapshots(a,b,c)
#else
bozo!
#endif

/*
 * For each filesystem or snapshot check if the name is in the
 * corresponding instance cache.  If it's not there then add it to the
 * cache. If we've cached the new instance then we keep the zfs_handle
 * which we've received in the argument, otherwise we need to close it
 * - zfs_iter_root() expects that from us.
 *
 * For filesystems iterate over their snapshots and update snapshot
 * count which is stored in the cached data for the instances in ZFS_INDOM
 * domain.
 */
static int
zfs_cache_inst(zfs_handle_t *zf, void *arg)
{
    const char *fsname = zfs_get_name(zf);
    pmInDom zfindom;
    int inst, rv;
    struct zfs_data *zdata = NULL;
    uint64_t *snapcnt = arg;

    switch (zfs_get_type(zf)) {
    case ZFS_TYPE_FILESYSTEM:
	zfindom = indomtab[ZFS_INDOM].it_indom;
	break;
    case ZFS_TYPE_SNAPSHOT:
	(*snapcnt)++;
	zfindom = indomtab[ZFS_SNAP_INDOM].it_indom;
	break;
    default:
	zfs_close(zf);
	return 0;
    }

    if ((rv = pmdaCacheLookupName(zfindom, fsname, &inst,
				  (void **)&zdata)) == PMDA_CACHE_ACTIVE) {
	zfs_close(zf);
	zfs_refresh_properties(zdata->zh);
	zf = zdata->zh;
    } else if ((rv == PMDA_CACHE_INACTIVE) && zdata) {
	rv = pmdaCacheStore(zfindom, PMDA_CACHE_ADD, fsname, zdata);
	if (rv < 0) {
	    pmNotifyErr(LOG_WARNING,
			  "Cannot reactivate cached data for '%s': %s\n",
			  fsname, pmErrStr(rv));
	    zfs_close(zf);
	    return 0;
	}
	zfs_close(zf);
	zfs_refresh_properties(zdata->zh);
	zf = zdata->zh;
    } else {
	if ((zdata = calloc(1, sizeof(*zdata))) == NULL) {
	    pmNotifyErr(LOG_WARNING,
			  "Out of memory for data of %s\n", fsname);
	    zfs_close(zf);
	    return 0;
	}
	zdata->zh = zf;
	rv = pmdaCacheStore(zfindom, PMDA_CACHE_ADD, fsname, zdata);
	if (rv < 0) {
	    pmNotifyErr(LOG_WARNING,
			  "Cannot cache data for '%s': %s\n",
			  fsname, pmErrStr(rv));
	    zfs_close(zf);
	    return 0;
	}
	zf_added++;
    }

    zfs_iter_filesystems(zf, zfs_cache_inst, NULL);
    if (zfs_get_type(zf) == ZFS_TYPE_FILESYSTEM) {
	zdata->nsnaps = 0;
	zfs_iter_snapshots3(zf, zfs_cache_inst, &zdata->nsnaps);
    }

    return 0;
}

void
zfs_refresh(void)
{
    zf_added = 0;

    pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(indomtab[ZFS_SNAP_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    zfs_iter_root(zh, zfs_cache_inst, NULL);

    if (zf_added) {
	pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(indomtab[ZFS_SNAP_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}

int
zfs_fetch(pmdaMetric *pm, int inst, pmAtomValue *atom)
{
    char *fsname;
    metricdesc_t *md = pm->m_user;
    struct zfs_data *zdata;
    uint64_t v;

    if (pmdaCacheLookup(pm->m_desc.indom, inst, &fsname,
			(void **)&zdata) != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    if (md->md_offset == -1) { /* nsnapshot */
	atom->ull = zdata->nsnaps;
	return 1;
    }

    v = zfs_prop_get_int(zdata->zh, md->md_offset);

    /* Special processing - compression ratio is in precent, we export
     * it as multiplier */
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
	pmdaCacheOp(indomtab[ZFS_SNAP_INDOM].it_indom, PMDA_CACHE_LOAD);
	zfs_iter_root(zh, zfs_cache_inst, &first);
	pmdaCacheOp(indomtab[ZFS_INDOM].it_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(indomtab[ZFS_SNAP_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}
