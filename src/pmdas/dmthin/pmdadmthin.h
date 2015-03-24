/*
 * Device Mapper - Thin Provisioning (dm-thin) PMDA
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

#ifndef PMDADMTHIN_H
#define PMDADMTHIN_H

#include "stats.h"

enum {
        CLUSTER_POOL = 0,	/* Thin Pools */
	CLUSTER_VOL = 1,	/* Thin volumes */
	NUM_CLUSTERS
};

enum {
	DMTHIN_POOL_INDOM = 0,	/* 0 -- Thin Pools */
        DMTHIN_VOL_INDOM = 1,	/* 1 -- Thin volumes */
	NUM_INDOMS
};

struct dmthin_pool {
        char name[PATH_MAX];
        struct pool_stats pool_stats;
};

struct dmthin_vol {
        char name[PATH_MAX];
        struct vol_stats vol_stats;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif	/*PMDADMTHIN_H*/
