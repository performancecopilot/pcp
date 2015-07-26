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

struct vdev_stats {
    int vdev_stats_fresh;
    vdev_stat_t vds;
};

static libzfs_handle_t *zh;
static int vdev_added;

#if defined(HAVE_ZPOOL_VDEV_NAME_5ARG)
# define zpool_vdev_name3(a,b,c)	zpool_vdev_name(a,b,c,B_FALSE,B_FALSE)
#elif defined(HAVE_ZPOOL_VDEV_NAME_4ARG)
# define zpool_vdev_name3(a,b,c)	zpool_vdev_name(a,b,c,B_FALSE)
#else	/* assume original 3-argument form */
# define zpool_vdev_name3(a,b,c)	zpool_vdev_name(a,b,c)
#endif

static char *
make_vdev_name(zpool_handle_t *zp, const char *pname, nvlist_t *child)
{
	char *name = NULL;
	char *cname = zpool_vdev_name3(zh, zp, child);
	uint_t size;

	if (cname == NULL) {
	    __pmNotifyErr(LOG_WARNING, "Cannot get the name of %s\'s "
			  "child\n", pname);
	    goto out;
	}
	size = strlen(pname) + strlen(cname) + 2;
	name = malloc(size);
	if (name == NULL) {
	    __pmNotifyErr(LOG_WARNING, "Cannot allocate memory for %s.%s\n",
			  pname, cname);
	    goto free_out;
	}
	snprintf(name, size, "%s.%s", pname, cname);
free_out:
	free(cname);
out:
	return name;
}

/*
 * get the names and stats of those vdevs in the pool that are disks and are
 * either children, cache or spare devices
 */
static int
zp_get_vdevs(zpool_handle_t *zp, char *zpname, nvlist_t *vdt,
		char ***vdev_names,
		vdev_stat_t ***vds, int *num)
{
	int rv = 0;
	uint_t cnt;
	char **new_vdev_names;	
	vdev_stat_t **new_vds;
	int nelem = *num;

	char *name;
	vdev_stat_t *stats;
	nvlist_t **children;
	uint_t nchildren;

 	static const char *prop[] = {
	    ZPOOL_CONFIG_CHILDREN,
	    ZPOOL_CONFIG_L2CACHE,
	    ZPOOL_CONFIG_SPARES
	};
	int i;
	int j;
	char *vdev_type;

	rv = nvlist_lookup_string(vdt, ZPOOL_CONFIG_TYPE, &vdev_type);	
	/* we've found disk, look no further */
	if (rv == 0 && strcmp(vdev_type, "disk") == 0) {

	    /* accommodate zpool api changes ... */
#ifdef ZPOOL_CONFIG_VDEV_STATS
	    rv = nvlist_lookup_uint64_array(vdt, ZPOOL_CONFIG_VDEV_STATS,
		    (uint64_t **)&stats, &cnt);
#else
	    rv = nvlist_lookup_uint64_array(vdt, ZPOOL_CONFIG_STATS,
		    (uint64_t **)&stats, &cnt);
#endif
	    if (rv != 0) {
		__pmNotifyErr(LOG_WARNING, "Cannot get the stats of %s\'s "
			"child\n", zpname);
		goto out;
	    }
	    name = make_vdev_name(zp, zpname, vdt);
	    if (name == NULL) {
		__pmNotifyErr(LOG_WARNING, "Cannot get the name of a %s\'s "
			"disk\n", zpname);
		goto out;
	    }
	    nelem++;
	    new_vdev_names = realloc(*vdev_names, nelem *
		    sizeof(*new_vdev_names));
	    if (new_vdev_names == NULL) {
		__pmNotifyErr(LOG_WARNING, "Cannot realloc memory for %s\n",
			name);
		goto free_out;
	    }
	    new_vdev_names[nelem - 1] = NULL;
	    *vdev_names = new_vdev_names;

	    new_vds = realloc(*vds, nelem * sizeof(*new_vds));
	    if (new_vds == NULL) {
		__pmNotifyErr(LOG_WARNING, "Cannot realloc memory for vds %s\n",
			name);
		goto free_out;
	    }
	    new_vds[nelem - 1] = stats;
	    new_vdev_names[nelem - 1] = name;

	    *vds = new_vds;
	    *num = nelem;
	    goto out;
	}    

	/* not a disk, traversing children until we find all the disks */
	for (i = 0; i < sizeof(prop) / sizeof(prop[0]); i++) {
	    rv = nvlist_lookup_nvlist_array(vdt, prop[i], &children,
		    &nchildren);
	    if (rv != 0)
		nchildren = 0;		
	    for (j = 0; j < nchildren; j++) {
		zp_get_vdevs(zp, zpname, children[j], vdev_names, vds, num);
	    }
	}
	return 0;
out:
	return rv;
free_out:
	free(name);
	return rv;
}

/*
 * For each zpool, check the leaf vdev names that are disks in the instance
 * cache, if one is not there then add it to the cache. Regardless if it's the
 * first time we've seen it or if it was in the cache before refresh the stats
 */
static int
zp_cache_vdevs(zpool_handle_t *zp, void *arg)
{
	nvlist_t *cfg = zpool_get_config(zp, NULL);
	char *zpname = (char *)zpool_get_name(zp);
	struct vdev_stats *zps = NULL;
	pmInDom zpindom = indomtab[ZPOOL_PERDISK_INDOM].it_indom;
	int rv;
	int inst;
        nvlist_t *vdt;

	int i;        
	char **vdev_names = NULL;
        vdev_stat_t **vds = NULL;
        int num = 0;

	rv = nvlist_lookup_nvlist(cfg, ZPOOL_CONFIG_VDEV_TREE, &vdt);
	if (rv != 0) {
	    __pmNotifyErr(LOG_ERR, "Cannot get vdev tree for '%s': %d %d\n",
		    zpname, rv, oserror());
	    goto done;
	}

	rv = zp_get_vdevs(zp, zpname, vdt, &vdev_names, &vds, &num);
	if (rv != 0) {
	    __pmNotifyErr(LOG_WARNING, "Cannot get vdevs for zpool '%s'\n",
	                  zpname);
	    goto free_done;
	}

	for (i = 0; i < num; i++) {
	    if (vdev_names[i] == NULL)
		continue;
	    rv = pmdaCacheLookupName(zpindom, vdev_names[i], &inst,
		    (void **)&zps);
	    if (rv != PMDA_CACHE_ACTIVE) {
		int new_vdev = (zps == NULL);

		if (rv != PMDA_CACHE_INACTIVE || new_vdev) {
		    zps = malloc(sizeof(*zps));
		    if (zps == NULL) {
			__pmNotifyErr(LOG_WARNING,
				  "Cannot allocate memory to hold stats for "
				  "vdev '%s'\n", vdev_names[i]);
			goto free_done;
		    }
		}

		rv = pmdaCacheStore(zpindom, PMDA_CACHE_ADD, vdev_names[i],
			zps);
		if (rv < 0) {
		    __pmNotifyErr(LOG_WARNING,
			        "Cannot add '%s' to the cache "
			        "for instance domain %s: %s\n",
			        vdev_names[i], pmInDomStr(zpindom),
			        pmErrStr(rv));
		    free(zps);
		    goto free_done;
		}
		vdev_added += new_vdev;
	    }

	    if (rv >= 0) {
	        memcpy(&zps->vds, vds[i], sizeof(zps->vds));
	        zps->vdev_stats_fresh = 1;
	    } else {
		__pmNotifyErr(LOG_ERR,
			"Cannot get stats for '%s': %d %d\n",
			vdev_names[i], rv, oserror());
		zps->vdev_stats_fresh = 0;
	    }
	}
free_done:
	for (i = 0; i < num; i++)
		free(vdev_names[i]);
	free(vdev_names);
	free(vds);
done:
	zpool_close(zp);
	return 0;
}

void
zpool_perdisk_refresh(void)
{
    vdev_added = 0;

    pmdaCacheOp(indomtab[ZPOOL_PERDISK_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    zpool_iter(zh, zp_cache_vdevs, NULL);

    if (vdev_added) {
	pmdaCacheOp(indomtab[ZPOOL_PERDISK_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}

int
zpool_perdisk_fetch(pmdaMetric *pm, int inst, pmAtomValue *atom)
{
    struct vdev_stats *stats;
    char *vdev_name;
    metricdesc_t *md = pm->m_user;

    if (pmdaCacheLookup(indomtab[ZPOOL_PERDISK_INDOM].it_indom, inst,
			&vdev_name, (void **)&stats) != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    if (stats->vdev_stats_fresh) {
	switch (pmid_item(md->md_desc.pmid)) {
	case 0: /* zpool.perdisk.state */
	    atom->cp = zpool_state_to_name(stats->vds.vs_state,
					   stats->vds.vs_aux);
	    break;
	case 1: /* zpool.perdisk.state_int */
	    atom->ul = (stats->vds.vs_aux << 8) | stats->vds.vs_state;
	    break;
	default:
	    memcpy(&atom->ull, ((char *)&stats->vds) + md->md_offset,
		    sizeof(atom->ull));
	}
    }

    return stats->vdev_stats_fresh;
}

void
zpool_perdisk_init(int first)
{
    if (zh)
	return;

    zh = libzfs_init();
    if (zh) {
	pmdaCacheOp(indomtab[ZPOOL_PERDISK_INDOM].it_indom, PMDA_CACHE_LOAD);
	zpool_iter(zh, zp_cache_vdevs, NULL);
	pmdaCacheOp(indomtab[ZPOOL_PERDISK_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}
