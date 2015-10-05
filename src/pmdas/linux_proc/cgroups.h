/*
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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
#ifndef _CGROUP_H
#define _CGROUP_H

/*
 * Per-cgroup data structures for each of the cgroup subsystems.
 */
typedef struct {
    int			cpus;
    int			mems;
} cgroup_cpuset_t;

enum {
    CG_CPUSET_CPUS	= 0,
    CG_CPUSET_MEMS	= 1,
};

typedef struct {
    __uint64_t		usage;
} cgroup_percpuacct_t;

typedef struct {
    __uint64_t		user;
    __uint64_t		system;
    __uint64_t		usage;
} cgroup_cpuacct_t;

enum {
    CG_CPUACCT_USER		= 0,
    CG_CPUACCT_SYSTEM		= 1,
    CG_CPUACCT_USAGE		= 2,
    CG_CPUACCT_PERCPU_USAGE	= 3,
};

typedef struct {
    __uint64_t		nr_periods;
    __uint64_t		nr_throttled;
    __uint64_t		throttled_time;
} cgroup_cpustat_t;

typedef struct {
    __uint64_t		shares;
    cgroup_cpustat_t	stat;
} cgroup_cpusched_t;

enum {
    CG_CPUSCHED_SHARES		= 0,
    CG_CPUSCHED_PERIODS		= 1,
    CG_CPUSCHED_THROTTLED	= 2,
    CG_CPUSCHED_THROTTLED_TIME	= 3,
};

typedef struct {
    __uint64_t		cache;
    __uint64_t		rss;
    __uint64_t		rss_huge;
    __uint64_t		mapped_file;
    __uint64_t		writeback;
    __uint64_t		swap;
    __uint64_t		pgpgin;
    __uint64_t		pgpgout;
    __uint64_t		pgfault;
    __uint64_t		pgmajfault;
    __uint64_t		inactive_anon;
    __uint64_t		active_anon;
    __uint64_t		inactive_file;
    __uint64_t		active_file;
    __uint64_t		unevictable;
} cgroup_memstat_t;

typedef struct {
    cgroup_memstat_t	stat;
    cgroup_memstat_t	total;
    __uint64_t		recent_rotated_anon;
    __uint64_t		recent_rotated_file;
    __uint64_t		recent_scanned_anon;
    __uint64_t		recent_scanned_file;
    __uint64_t		usage;
    __uint64_t		limit;
    __uint64_t		failcnt;
} cgroup_memory_t;

enum {
    CG_MEMORY_STAT_CACHE		= 0,
    CG_MEMORY_STAT_RSS			= 1,
    CG_MEMORY_STAT_RSS_HUGE		= 2,
    CG_MEMORY_STAT_MAPPED_FILE		= 3,
    CG_MEMORY_STAT_WRITEBACK		= 4,
    CG_MEMORY_STAT_SWAP			= 5,
    CG_MEMORY_STAT_PGPGIN		= 6,
    CG_MEMORY_STAT_PGPGOUT		= 7,
    CG_MEMORY_STAT_PGFAULT		= 8,
    CG_MEMORY_STAT_PGMAJFAULT		= 9,
    CG_MEMORY_STAT_INACTIVE_ANON	= 10,
    CG_MEMORY_STAT_ACTIVE_ANON		= 11,
    CG_MEMORY_STAT_INACTIVE_FILE	= 12,
    CG_MEMORY_STAT_ACTIVE_FILE		= 13,
    CG_MEMORY_STAT_UNEVICTABLE		= 14,

    CG_MEMORY_STAT_TOTAL_CACHE		= 30,
    CG_MEMORY_STAT_TOTAL_RSS		= 31,
    CG_MEMORY_STAT_TOTAL_RSS_HUGE	= 32,
    CG_MEMORY_STAT_TOTAL_MAPPED_FILE	= 33,
    CG_MEMORY_STAT_TOTAL_WRITEBACK	= 34,
    CG_MEMORY_STAT_TOTAL_SWAP		= 35,
    CG_MEMORY_STAT_TOTAL_PGPGIN		= 36,
    CG_MEMORY_STAT_TOTAL_PGPGOUT	= 37,
    CG_MEMORY_STAT_TOTAL_PGFAULT	= 38,
    CG_MEMORY_STAT_TOTAL_PGMAJFAULT	= 39,
    CG_MEMORY_STAT_TOTAL_INACTIVE_ANON	= 40,
    CG_MEMORY_STAT_TOTAL_ACTIVE_ANON	= 41,
    CG_MEMORY_STAT_TOTAL_INACTIVE_FILE	= 42,
    CG_MEMORY_STAT_TOTAL_ACTIVE_FILE	= 43,
    CG_MEMORY_STAT_TOTAL_UNEVICTABLE	= 44,

    CG_MEMORY_STAT_RECENT_ROTATED_ANON	= 60,
    CG_MEMORY_STAT_RECENT_ROTATED_FILE	= 61,
    CG_MEMORY_STAT_RECENT_SCANNED_ANON	= 62,
    CG_MEMORY_STAT_RECENT_SCANNED_FILE	= 63,

    CG_MEMORY_USAGE_IN_BYTES		= 80,
    CG_MEMORY_LIMIT_IN_BYTES		= 81,
    CG_MEMORY_FAILCNT			= 82,
};

typedef struct {
    __uint64_t		classid;
} cgroup_netcls_t;

enum {
    CG_NETCLS_CLASSID	= 0,
};

typedef struct {
    __uint32_t		major;
    __uint32_t		minor;
    int			inst;
    char		*name;
} device_t;

typedef struct {
    __uint64_t		read;
    __uint64_t		write;
    __uint64_t		sync;
    __uint64_t		async;
    __uint64_t		total;
} cgroup_blkiops_t;

typedef struct {
    cgroup_blkiops_t	io_merged;
    cgroup_blkiops_t	io_queued;
    cgroup_blkiops_t	io_service_bytes;
    cgroup_blkiops_t	io_serviced;
    cgroup_blkiops_t	io_service_time;
    cgroup_blkiops_t	io_wait_time;
    __uint64_t		sectors;
    __uint64_t		time;
} cgroup_blkstats_t;

typedef struct {
    device_t		dev;
    cgroup_blkstats_t	stats;
} cgroup_perdevblkio_t;

typedef struct {
    cgroup_blkstats_t	total;
} cgroup_blkio_t;

enum {
    CG_PERDEVBLKIO_IOMERGED_READ	= 0,
    CG_PERDEVBLKIO_IOMERGED_WRITE	= 1,
    CG_PERDEVBLKIO_IOMERGED_SYNC	= 2,
    CG_PERDEVBLKIO_IOMERGED_ASYNC	= 3,
    CG_PERDEVBLKIO_IOMERGED_TOTAL	= 4,
    CG_PERDEVBLKIO_IOQUEUED_READ	= 5,
    CG_PERDEVBLKIO_IOQUEUED_WRITE	= 6,
    CG_PERDEVBLKIO_IOQUEUED_SYNC	= 7,
    CG_PERDEVBLKIO_IOQUEUED_ASYNC	= 8,
    CG_PERDEVBLKIO_IOQUEUED_TOTAL	= 9,
    CG_PERDEVBLKIO_IOSERVICEBYTES_READ	= 10,
    CG_PERDEVBLKIO_IOSERVICEBYTES_WRITE	= 11,
    CG_PERDEVBLKIO_IOSERVICEBYTES_SYNC	= 12,
    CG_PERDEVBLKIO_IOSERVICEBYTES_ASYNC	= 13,
    CG_PERDEVBLKIO_IOSERVICEBYTES_TOTAL	= 14,
    CG_PERDEVBLKIO_IOSERVICED_READ	= 15,
    CG_PERDEVBLKIO_IOSERVICED_WRITE	= 16,
    CG_PERDEVBLKIO_IOSERVICED_SYNC	= 17,
    CG_PERDEVBLKIO_IOSERVICED_ASYNC	= 18,
    CG_PERDEVBLKIO_IOSERVICED_TOTAL	= 19,
    CG_PERDEVBLKIO_IOSERVICETIME_READ	= 20,
    CG_PERDEVBLKIO_IOSERVICETIME_WRITE	= 21,
    CG_PERDEVBLKIO_IOSERVICETIME_SYNC	= 22,
    CG_PERDEVBLKIO_IOSERVICETIME_ASYNC	= 23,
    CG_PERDEVBLKIO_IOSERVICETIME_TOTAL	= 24,
    CG_PERDEVBLKIO_IOWAITTIME_READ	= 25,
    CG_PERDEVBLKIO_IOWAITTIME_WRITE	= 26,
    CG_PERDEVBLKIO_IOWAITTIME_SYNC	= 27,
    CG_PERDEVBLKIO_IOWAITTIME_ASYNC	= 28,
    CG_PERDEVBLKIO_IOWAITTIME_TOTAL	= 29,
    CG_PERDEVBLKIO_SECTORS		= 30,
    CG_PERDEVBLKIO_TIME			= 31,

    CG_BLKIO_IOMERGED_READ		= 60,
    CG_BLKIO_IOMERGED_WRITE		= 61,
    CG_BLKIO_IOMERGED_SYNC		= 62,
    CG_BLKIO_IOMERGED_ASYNC		= 63,
    CG_BLKIO_IOMERGED_TOTAL		= 64,
    CG_BLKIO_IOQUEUED_READ		= 65,
    CG_BLKIO_IOQUEUED_WRITE		= 66,
    CG_BLKIO_IOQUEUED_SYNC		= 67,
    CG_BLKIO_IOQUEUED_ASYNC		= 68,
    CG_BLKIO_IOQUEUED_TOTAL		= 69,
    CG_BLKIO_IOSERVICEBYTES_READ	= 70,
    CG_BLKIO_IOSERVICEBYTES_WRITE	= 71,
    CG_BLKIO_IOSERVICEBYTES_SYNC	= 72,
    CG_BLKIO_IOSERVICEBYTES_ASYNC	= 73,
    CG_BLKIO_IOSERVICEBYTES_TOTAL	= 74,
    CG_BLKIO_IOSERVICED_READ		= 75,
    CG_BLKIO_IOSERVICED_WRITE		= 76,
    CG_BLKIO_IOSERVICED_SYNC		= 77,
    CG_BLKIO_IOSERVICED_ASYNC		= 78,
    CG_BLKIO_IOSERVICED_TOTAL		= 79,
    CG_BLKIO_IOSERVICETIME_READ		= 80,
    CG_BLKIO_IOSERVICETIME_WRITE	= 81,
    CG_BLKIO_IOSERVICETIME_SYNC		= 82,
    CG_BLKIO_IOSERVICETIME_ASYNC	= 83,
    CG_BLKIO_IOSERVICETIME_TOTAL	= 84,
    CG_BLKIO_IOWAITTIME_READ		= 85,
    CG_BLKIO_IOWAITTIME_WRITE		= 86,
    CG_BLKIO_IOWAITTIME_SYNC		= 87,
    CG_BLKIO_IOWAITTIME_ASYNC		= 88,
    CG_BLKIO_IOWAITTIME_TOTAL		= 89,
    CG_BLKIO_SECTORS			= 90,
    CG_BLKIO_TIME			= 91,
};

typedef struct filesys {
    int			id;
    char		*device;
    char		*path;
    char		*options;
} filesys_t;

enum {
    CG_MOUNTS_SUBSYS			= 0,
    CG_MOUNTS_COUNT			= 1,
};

typedef struct subsys {
    unsigned int	hierarchy;
    unsigned int	num_cgroups;
    unsigned int	enabled;
} subsys_t;

enum {
    CG_SUBSYS_HIERARCHY			= 0,
    CG_SUBSYS_COUNT			= 1,
    CG_SUBSYS_NUMCGROUPS		= 2,
    CG_SUBSYS_ENABLED			= 3,
};

/*
 * General cgroup interfaces
 */
typedef void (*cgroup_setup_t)(void);
typedef void (*cgroup_refresh_t)(const char *, const char *);
extern void refresh_cgroups(const char *, const char *, int,
			    cgroup_setup_t, cgroup_refresh_t);
extern char *cgroup_find_subsys(pmInDom, filesys_t *);
extern int cgroup_mounts_subsys(const char *, char *, int);

extern void refresh_cgroup_subsys(void);
extern void refresh_cgroup_filesys(void);

/*
 * Indom-specific interfaces (iteratively populating)
 */
extern void setup_cpuset(void);
extern void setup_cpuacct(void);
extern void setup_cpusched(void);
extern void setup_memory(void);
extern void setup_netcls(void);
extern void setup_blkio(void);
extern void refresh_cpuset(const char *, const char *);
extern void refresh_cpuacct(const char *, const char *);
extern void refresh_cpusched(const char *, const char *);
extern void refresh_memory(const char *, const char *);
extern void refresh_netcls(const char *, const char *);
extern void refresh_blkio(const char *, const char *);

#endif /* _CGROUP_H */
