/*
 * Device Mapper PMDA - DM (device-mapper) stats with dmstats API
 *
 * Copyright (c) 2017 Fumiya Shigemitsu.
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
#ifndef DMSTATS_H
#define DMSTATS_H

#ifdef HAVE_DEVMAPPER
#include <libdevmapper.h>
#endif

struct dm_histogram;

enum {
    PM_DM_STATS_READS = 0,
    PM_DM_STATS_READS_MERGED,
    PM_DM_STATS_READ_SECTORS,
    PM_DM_STATS_READ_NSECS,
    PM_DM_STATS_WRITES,
    PM_DM_STATS_WRITES_MERGED,
    PM_DM_STATS_WRITE_SECTORS,
    PM_DM_STATS_WRITE_NSECS,
    PM_DM_STATS_IO_IN_PROGRESS,
    PM_DM_STATS_IO_NSECS,
    PM_DM_STATS_WEIGHTED_IO_NSECS,
    PM_DM_STATS_TOTAL_READ_NSECS,
    PM_DM_STATS_TOTAL_WRITE_NSECS,

    PM_DM_STATS_NR_COUNTERS
};

struct pm_dm_stats_counter {
	uint64_t pm_reads;		    /* Num reads completed */
	uint64_t pm_reads_merged;	    /* Num reads merged */
	uint64_t pm_read_sectors;	    /* Num sectors read */
	uint64_t pm_read_nsecs;	    /* Num milliseconds spent reading */
	uint64_t pm_writes;	    /* Num writes completed */
	uint64_t pm_writes_merged;	    /* Num writes merged */
	uint64_t pm_write_sectors;	    /* Num sectors written */
	uint64_t pm_write_nsecs;	    /* Num milliseconds spent writing */
	uint64_t pm_io_in_progress;    /* Num I/Os currently in progress */
	uint64_t pm_io_nsecs;	    /* Num milliseconds spent doing I/Os */
	uint64_t pm_weighted_io_nsecs; /* Weighted num milliseconds doing I/Os */
	uint64_t pm_total_read_nsecs;  /* Total time spent reading in milliseconds */
	uint64_t pm_total_write_nsecs; /* Total time spent writing in milliseconds */
	struct dm_histogram *histogram; /* Histogram. */
};

#ifdef HAVE_DEVMAPPER
extern int pm_dm_stats_fetch(int, struct pm_dm_stats_counter *, pmAtomValue *);
extern int pm_dm_refresh_stats_counter(const char *, struct pm_dm_stats_counter *);
extern int pm_dm_stats_instance_refresh(void);
#else
#define pm_dm_stats_fetch(item, ctr, atom)      (PM_ERR_APPVERSION)
#define pm_dm_refresh_stats_counter(s, ctr)     do { } while (0)
#define pm_dm_stats_instance_refresh(void)      (0)
#endif

#endif
