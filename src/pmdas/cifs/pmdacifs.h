/*
 * Common Internet File System (CIFS) PMDA
 *
 * Copyright (c) 2014 Red Hat.
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

#ifndef PMDACIFS_H
#define PMDACIFS_H

#include "stats.h"

enum {
        CLUSTER_GLOBAL_STATS = 0,	/* /proc/fs/cifs/Stats - global cifs*/
	CLUSTER_FS_STATS = 1,		/* /proc/fs/cifs/Stats - per fs*/
	NUM_CLUSTERS
};

enum {
	CIFS_FS_INDOM = 0,       	/* 0 -- mounted cifs filesystem names */
	NUM_INDOMS
};

struct cifs_fs {
        char name[PATH_MAX];
        struct fs_stats fs_stats;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif	/*PMDACIFS_H*/
