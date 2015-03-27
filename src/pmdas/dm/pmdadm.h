/*
 * Device Mapper PMDA
 *
 * Copyright (c) 2015 Red Hat.
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

#ifndef PMDADM_H
#define PMDADM_H

#include "dmcache.h"
#include "dmthin.h"

enum {
    CLUSTER_CACHE = 0,		/* DM-Cache Caches */
    CLUSTER_POOL = 1,		/* DM-Thin Pools */
    CLUSTER_VOL = 2,		/* DM-Thin Volumes */
    NUM_CLUSTERS
};

enum {
    DM_CACHE_INDOM = 0,		/* 0 -- Caches */
    DM_THIN_POOL_INDOM = 1,	/* 1 -- Thin Pools */
    DM_THIN_VOL_INDOM = 2,	/* 2 -- Thin Volumes */
    NUM_INDOMS
};

struct dm_cache {
    char name[PATH_MAX];
    struct cache_stats cache_stats;
};

struct dm_thin_pool {
    char name[PATH_MAX];
    struct pool_stats pool_stats;
};

struct dm_thin_vol {
    char name[PATH_MAX];
    struct vol_stats vol_stats;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif /* PMDADM_H */
