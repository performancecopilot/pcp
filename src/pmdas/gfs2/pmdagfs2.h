/*
 * Global Filesystem v2 (GFS2) PMDA
 *
 * Copyright (c) 2013 - 2025 Red Hat.
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
#include "glockfd.h"
#include "worst_glock.h"
#include "ftrace.h"
#include "latency.h"
#include "control.h"

extern pmInDom gfs2_indom(int);
#define INDOM(i) gfs2_indom(i)

enum {
	CLUSTER_GLOCKS = 0,	/* 0 - /sys/kernel/debug/gfs2/<fs>/glocks */
	CLUSTER_SBSTATS,	/* 1 - /sys/kernel/debug/gfs2/<fs>/sbstats */
	CLUSTER_GLSTATS,	/* 2 - /sys/kernel/debug/gfs2/<fs>/glstats */
        CLUSTER_TRACEPOINTS,	/* 3 - /sys/kernel/debug/tracing/events/gfs2/ */
        CLUSTER_WORSTGLOCK,     /* 4 - Custom metric for worst glock */
	CLUSTER_LATENCY,	/* 5 - Custom metric for working out latency of filesystem operations */
        CLUSTER_CONTROL,        /* 6 - Control for specific tracepoint enabling (for installs without all of the GFS2 tracepoints) */
        CLUSTER_GLOCKFD,
        CLUSTER_HOLDER_STATS,
	NUM_CLUSTERS
};

enum {
	GFS_FS_INDOM = 0,       /* 0 -- mounted gfs filesystem names */
	GFS_HOLDER_INDOM,
	NUM_INDOMS
};

struct gfs2_fs {
        dev_t              dev_id;
	struct glocks      glocks;
	struct sbstats     sbstats;
        struct glstats     glstats;
        struct glockfd     glockfd;
        struct ftrace      ftrace;
        struct worst_glock worst_glock;
        struct latency     latency;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif	/*PMDAGFS2_H*/
