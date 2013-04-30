/*
 * Global Filesystem v2 (GFS2) PMDA
 *
 * Copyright (c) 2013 Red Hat.
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

#ifndef PMDAGFS2_H
#define PMDAGFS2_H

#include "glocks.h"
#include "sbstats.h"
#include "glstats.h"
#include "lock_time.h"

enum {
	CLUSTER_GLOCKS = 0,	/* 0 - /sys/kernel/debug/gfs2/<fs>/glocks */
	CLUSTER_SBSTATS,	/* 1 - /sys/kernel/debug/gfs2/<fs>/sbstats */
	CLUSTER_GLSTATS,	/* 2 - /sys/kernel/debug/gfs2/<fs>/glstats */
        CLUSTER_LOCKTIME,       /* 3 - /sys/kernel/debug/tracing/events/gfs2/gfs2_glock_lock_time */
	NUM_CLUSTERS
};

enum {
	GFS_FS_INDOM = 0,	/* 0 -- mounted gfs filesystem names */
	NUM_INDOMS
};

struct gfs2_fs {
	struct glocks	glocks;
	struct sbstats	sbstats;
        struct glstats  glstats;
        struct lock_time lock_time;
        dev_t dev_id;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif	/*PMDAGFS2_H*/
