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
#include "worst_glock.h"
#include "ftrace.h"
#include "control.h"

enum {
	CLUSTER_GLOCKS = 0,	/* 0 - /sys/kernel/debug/gfs2/<fs>/glocks */
	CLUSTER_SBSTATS,	/* 1 - /sys/kernel/debug/gfs2/<fs>/sbstats */
	CLUSTER_GLSTATS,	/* 2 - /sys/kernel/debug/gfs2/<fs>/glstats */
        CLUSTER_TRACEPOINTS,	/* 3 - /sys/kernel/debug/tracing/events/gfs2/ */
        CLUSTER_WORSTGLOCK,     /* 4 - Custom metric for worst glock */
        CLUSTER_CONTROL,        /* 5 - Control for specific tracepoint enabling (for installs without all of the GFS2 tracepoints) */
	NUM_CLUSTERS
};

enum {
	GFS_FS_INDOM = 0,       /* 0 -- mounted gfs filesystem names */
	NUM_INDOMS
};

struct gfs2_fs {
        dev_t              dev_id;
	struct glocks      glocks;
	struct sbstats     sbstats;
        struct glstats     glstats;
        struct ftrace      ftrace;
        struct worst_glock worst_glock;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif	/*PMDAGFS2_H*/
