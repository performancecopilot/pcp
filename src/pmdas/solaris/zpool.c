/*
 * Copyright (C) 2009 Max Matveev.  All rights reserved.
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
 */

#include <libzfs.h>

#include "common.h"

struct zpool_stats {
    int vdev_stats_fresh;
    vdev_stat_t vds;
};

static libzfs_handle_t *zh;
static int zp_added;

/*
 * For each zpool check the name in the instance cache, if it's not there then
 * add it to the cache. Regardless if it's the first time we've seen this one
 * or if it was in the cache before refresh the stats
 */
static int
zp_cache_pool(zpool_handle_t *zp, void *arg)
{
	nvlist_t *cfg = zpool_get_config(zp, NULL);
	char *zpname = (char *)zpool_get_name(zp);
	struct zpool_stats *zps = NULL;
	pmInDom zpindom = indomtab[ZPOOL_INDOM].it_indom;
        uint_t cnt = 0;
        vdev_stat_t *vds;
	int rv;
	int inst;
        nvlist_t *vdt;

	if ((rv = pmdaCacheLookupName(zpindom, zpname, &inst,
					(void **)&zps)) != PMDA_CACHE_ACTIVE) {
	    int newpool = (zps == NULL);

	    if (rv != PMDA_CACHE_INACTIVE || zps == NULL) {
		zps = malloc(sizeof(*zps));
		if (zps == NULL) {
		    pmNotifyErr(LOG_WARNING,
				  "Cannot allocate memory to hold stats for "
				  "zpool '%s'\n",
				  zpname);
		    goto done;
		}
	    }

	    rv = pmdaCacheStore(zpindom, PMDA_CACHE_ADD, zpname, zps);
	    if (rv < 0) {
		pmNotifyErr(LOG_WARNING,
			      "Cannot add '%s' to the cache "
			      "for instance domain %s: %s\n",
			      zpname, pmInDomStr(zpindom), pmErrStr(rv));
		free(zps);
		goto done;
	    }
	    zp_added += newpool;
	}

	rv = nvlist_lookup_nvlist(cfg, ZPOOL_CONFIG_VDEV_TREE, &vdt);
	if (rv != 0) {
	    pmNotifyErr(LOG_ERR, "Cannot get vdev tree for '%s': %d %d\n",
			  zpname, rv, oserror());
	    zps->vdev_stats_fresh = 0;
	} else {
	    /* accommodate zpool api changes ... */
#ifdef ZPOOL_CONFIG_VDEV_STATS
	    rv = nvlist_lookup_uint64_array(vdt, ZPOOL_CONFIG_VDEV_STATS,
					    (uint64_t **)&vds, &cnt);
#else
	    rv = nvlist_lookup_uint64_array(vdt, ZPOOL_CONFIG_STATS,
					    (uint64_t **)&vds, &cnt);
#endif
	    if (rv == 0) {
		memcpy(&zps->vds, vds, sizeof(zps->vds));
		zps->vdev_stats_fresh = 1;
	    } else {
		pmNotifyErr(LOG_ERR,
			      "Cannot get zpool stats for '%s': %d %d\n",
			      zpname, rv, oserror());
		zps->vdev_stats_fresh = 0;
	    }
	}

done:
	zpool_close(zp);
	return 0;
}

void
zpool_refresh(void)
{
    zp_added = 0;

    pmdaCacheOp(indomtab[ZPOOL_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    zpool_iter(zh, zp_cache_pool, NULL);

    if (zp_added) {
	pmdaCacheOp(indomtab[ZPOOL_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}

int
zpool_fetch(pmdaMetric *pm, int inst, pmAtomValue *atom)
{
    struct zpool_stats *zps;
    char *zpname;
    metricdesc_t *md = pm->m_user;

    if (pmdaCacheLookup(indomtab[ZPOOL_INDOM].it_indom, inst, &zpname,
			(void **)&zps) != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    if (zps->vdev_stats_fresh) {
	switch (pmID_item(md->md_desc.pmid)) {
	case 0: /* zpool.state */
	    atom->cp = (char *)zpool_state_to_name(zps->vds.vs_state, zps->vds.vs_aux);
	    break;
	case 1: /* zpool.state_int */
	    atom->ul = (zps->vds.vs_aux << 8) | zps->vds.vs_state;
	    break;
	default:
	    memcpy(&atom->ull, ((char *)&zps->vds) + md->md_offset,
			sizeof(atom->ull));
	}
    }
    return zps->vdev_stats_fresh;
}

void
zpool_init(int first)
{
    if (zh)
	return;

    zh = libzfs_init();
    if (zh) {
	pmdaCacheOp(indomtab[ZPOOL_INDOM].it_indom, PMDA_CACHE_LOAD);
	zpool_iter(zh, zp_cache_pool, NULL);
	pmdaCacheOp(indomtab[ZPOOL_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}
