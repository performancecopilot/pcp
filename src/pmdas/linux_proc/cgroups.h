/*
 * Copyright (c) 2013-2019,2013 Red Hat.
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

#define MAXCIDLEN	128	/* maximum container name length */
#define SHA256CIDLEN	64	/* length of a SHA256 container ID */
#define MAXMNTOPTSLEN	256	/* maximum mount option string length */

/*
 * Per-cgroup data structures for each of the cgroup subsystems.
 */
typedef struct {
    int			cpus;
    int			mems;
    int			container;
} cgroup_cpuset_t;

enum {
    CG_CPUSET_CPUS		= 0,
    CG_CPUSET_MEMS		= 1,
    CG_CPUSET_ID_CONTAINER	= 2,
};

typedef struct {
    __uint64_t		user;
    __uint64_t		system;
    __uint64_t		usage;
} cgroup_cputime_t;

typedef struct {
    __uint64_t		usage;
} cgroup_percpuacct_t;

typedef struct {
    cgroup_cputime_t	cputime;
    int			container;
} cgroup_cpuacct_t;

enum {
    CG_CPUACCT_USER		= 0,
    CG_CPUACCT_SYSTEM		= 1,
    CG_CPUACCT_USAGE		= 2,
    CG_CPUACCT_PERCPU_USAGE	= 3,
    CG_CPUACCT_ID_CONTAINER	= 4,
};

typedef struct {
    cgroup_cputime_t	cputime;
    __uint64_t		nr_periods;
    __uint64_t		nr_throttled;
    __uint64_t		throttled_time;
} cgroup_cpustat_t;

typedef struct {
    __uint64_t		shares;
    cgroup_cpustat_t	stat;
    __uint64_t		cfs_period;
    __int64_t		cfs_quota;
    int			container;
} cgroup_cpusched_t;

enum {
    CG_CPUSCHED_SHARES		= 0,
    CG_CPUSCHED_PERIODS		= 1,
    CG_CPUSCHED_THROTTLED	= 2,
    CG_CPUSCHED_THROTTLED_TIME	= 3,
    CG_CPUSCHED_CFS_PERIOD	= 4,
    CG_CPUSCHED_CFS_QUOTA	= 5,
    CG_CPUSCHED_ID_CONTAINER	= 6,
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
    __uint64_t		total_cache;
    __uint64_t		total_rss;
    __uint64_t		total_rss_huge;
    __uint64_t		total_mapped_file;
    __uint64_t		total_writeback;
    __uint64_t		total_swap;
    __uint64_t		total_pgpgin;
    __uint64_t		total_pgpgout;
    __uint64_t		total_pgfault;
    __uint64_t		total_pgmajfault;
    __uint64_t		total_inactive_anon;
    __uint64_t		total_active_anon;
    __uint64_t		total_inactive_file;
    __uint64_t		total_active_file;
    __uint64_t		total_unevictable;
    __uint64_t		recent_rotated_anon;
    __uint64_t		recent_rotated_file;
    __uint64_t		recent_scanned_anon;
    __uint64_t		recent_scanned_file;
    __uint64_t		anon;
    __uint64_t		anon_thp;
    __uint64_t		file;
    __uint64_t		file_dirty;
    __uint64_t		file_mapped;
    __uint64_t		file_writeback;
    __uint64_t		kernel_stack;
    __uint64_t		pgactivate;
    __uint64_t		pgdeactivate;
    __uint64_t		pglazyfree;
    __uint64_t		pglazyfreed;
    __uint64_t		pgrefill;
    __uint64_t		pgscan;
    __uint64_t		pgsteal;
    __uint64_t		shmem;
    __uint64_t		slab;
    __uint64_t		slab_reclaimable;
    __uint64_t		slab_unreclaimable;
    __uint64_t		sock;
    __uint64_t		thp_collapse_alloc;
    __uint64_t		thp_fault_alloc;
    __uint64_t		workingset_activate;
    __uint64_t		workingset_nodereclaim;
    __uint64_t		workingset_refault;
} cgroup_memstat_t;

typedef struct {
    cgroup_memstat_t	stat;
    __uint64_t		current;
    __uint64_t		usage;
    __uint64_t		limit;
    __uint64_t		failcnt;
    int			container;
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

    CG_MEMORY_STAT_ANON			= 17,
    CG_MEMORY_STAT_ANON_THP		= 18,
    CG_MEMORY_STAT_FILE			= 19,
    CG_MEMORY_STAT_FILE_DIRTY		= 20,
    CG_MEMORY_STAT_FILE_MAPPED		= 21,
    CG_MEMORY_STAT_FILE_WRITEBACK	= 22,

    CG_MEMORY_STAT_KERNEL_STACK		= 25,
    CG_MEMORY_STAT_PGACTIVATE		= 26,
    CG_MEMORY_STAT_PGDEACTIVATE		= 27,
    CG_MEMORY_STAT_PGLAZYFREE		= 28,

    CG_MEMORY_ID_CONTAINER		= 29,

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

    CG_MEMORY_STAT_PGLAZYFREED		= 50,
    CG_MEMORY_STAT_PGREFILL		= 51,
    CG_MEMORY_STAT_PGSCAN		= 52,
    CG_MEMORY_STAT_PGSTEAL		= 53,

    CG_MEMORY_STAT_RECENT_ROTATED_ANON	= 60,
    CG_MEMORY_STAT_RECENT_ROTATED_FILE	= 61,
    CG_MEMORY_STAT_RECENT_SCANNED_ANON	= 62,
    CG_MEMORY_STAT_RECENT_SCANNED_FILE	= 63,
    CG_MEMORY_STAT_SHMEM		= 64,
    CG_MEMORY_STAT_SLAB			= 65,
    CG_MEMORY_STAT_SLAB_RECLAIMABLE	= 66,
    CG_MEMORY_STAT_SLAB_UNRECLAIMABLE	= 67,
    CG_MEMORY_STAT_SOCK			= 68,

    CG_MEMORY_STAT_THP_COLLAPSE_ALLOC	= 70,
    CG_MEMORY_STAT_THP_FAULT_ALLOC	= 71,
    CG_MEMORY_STAT_WORKINGSET_ACTIVATE	= 72,
    CG_MEMORY_STAT_WORKINGSET_NODERECLAIM=73,
    CG_MEMORY_STAT_WORKINGSET_REFAULT	= 74,

    CG_MEMORY_USAGE_IN_BYTES		= 80,
    CG_MEMORY_LIMIT_IN_BYTES		= 81,
    CG_MEMORY_FAILCNT			= 82,

    CG_MEMORY_CURRENT			= 90,
};

typedef struct {
    __uint64_t		classid;
    int			container;
} cgroup_netcls_t;

enum {
    CG_NETCLS_CLASSID			= 0,
    CG_NETCLS_ID_CONTAINER		= 1,
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
    cgroup_blkiops_t	throttle_io_service_bytes;
    cgroup_blkiops_t	throttle_io_serviced;
} cgroup_blkstats_t;

typedef struct {
    cgroup_blkstats_t	stats;
} cgroup_perdevblkio_t;

typedef struct {
    cgroup_blkstats_t	total;
    int			container;
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
    CG_PERDEVBLKIO_THROTTLEIOSERVICEBYTES_READ	= 32,
    CG_PERDEVBLKIO_THROTTLEIOSERVICEBYTES_WRITE	= 33,
    CG_PERDEVBLKIO_THROTTLEIOSERVICEBYTES_SYNC	= 34,
    CG_PERDEVBLKIO_THROTTLEIOSERVICEBYTES_ASYNC	= 35,
    CG_PERDEVBLKIO_THROTTLEIOSERVICEBYTES_TOTAL	= 36,
    CG_PERDEVBLKIO_THROTTLEIOSERVICED_READ	= 37,
    CG_PERDEVBLKIO_THROTTLEIOSERVICED_WRITE	= 38,
    CG_PERDEVBLKIO_THROTTLEIOSERVICED_SYNC	= 39,
    CG_PERDEVBLKIO_THROTTLEIOSERVICED_ASYNC	= 40,
    CG_PERDEVBLKIO_THROTTLEIOSERVICED_TOTAL	= 41,

    CG_BLKIO_ID_CONTAINER			= 42,

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
    CG_BLKIO_THROTTLEIOSERVICEBYTES_READ	= 92,
    CG_BLKIO_THROTTLEIOSERVICEBYTES_WRITE	= 93,
    CG_BLKIO_THROTTLEIOSERVICEBYTES_SYNC	= 94,
    CG_BLKIO_THROTTLEIOSERVICEBYTES_ASYNC	= 95,
    CG_BLKIO_THROTTLEIOSERVICEBYTES_TOTAL	= 96,
    CG_BLKIO_THROTTLEIOSERVICED_READ		= 97,
    CG_BLKIO_THROTTLEIOSERVICED_WRITE		= 98,
    CG_BLKIO_THROTTLEIOSERVICED_SYNC		= 99,
    CG_BLKIO_THROTTLEIOSERVICED_ASYNC		= 100,
    CG_BLKIO_THROTTLEIOSERVICED_TOTAL		= 101,
};

typedef struct filesys {
    int			id;
    int			version;
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
 * Cgroup v2 metrics - .stat and .pressure sysfs files
 */
typedef struct {
    unsigned int	updated;
    float		avg10sec;
    float		avg1min;
    float		avg5min;
    __uint64_t		total;
} cgroup_pressure_t;

typedef struct {
    cgroup_pressure_t	some;
    cgroup_pressure_t	full;
} cgroup_pressures_t;

enum {
    CG_PRESSURE_CPU_SOME_AVG10SEC	= 0,
    CG_PRESSURE_CPU_SOME_AVG1MIN	= 1,
    CG_PRESSURE_CPU_SOME_AVG5MIN	= 2,
    CG_PRESSURE_CPU_SOME_TOTAL		= 3,
};

enum {
    CG_PRESSURE_MEM_SOME_AVG10SEC	= 0,
    CG_PRESSURE_MEM_SOME_AVG1MIN	= 1,
    CG_PRESSURE_MEM_SOME_AVG5MIN	= 2,
    CG_PRESSURE_MEM_SOME_TOTAL		= 3,
    CG_PRESSURE_MEM_FULL_AVG10SEC	= 4,
    CG_PRESSURE_MEM_FULL_AVG1MIN	= 5,
    CG_PRESSURE_MEM_FULL_AVG5MIN	= 6,
    CG_PRESSURE_MEM_FULL_TOTAL		= 7,
};

enum {
    CG_PRESSURE_IO_SOME_AVG10SEC	= 0,
    CG_PRESSURE_IO_SOME_AVG1MIN		= 1,
    CG_PRESSURE_IO_SOME_AVG5MIN		= 2,
    CG_PRESSURE_IO_SOME_TOTAL		= 3,
    CG_PRESSURE_IO_FULL_AVG10SEC	= 4,
    CG_PRESSURE_IO_FULL_AVG1MIN		= 5,
    CG_PRESSURE_IO_FULL_AVG5MIN		= 6,
    CG_PRESSURE_IO_FULL_TOTAL		= 7,
};

enum {
    CG_PRESSURE_IRQ_FULL_AVG10SEC	= 0,
    CG_PRESSURE_IRQ_FULL_AVG1MIN	= 1,
    CG_PRESSURE_IRQ_FULL_AVG5MIN	= 2,
    CG_PRESSURE_IRQ_FULL_TOTAL		= 3,
};

typedef struct {
    __uint64_t		rbytes;
    __uint64_t		rios;
    __uint64_t		wbytes;
    __uint64_t		wios;
    __uint64_t		dbytes;
    __uint64_t		dios;
} cgroup_iostat_t;

typedef struct {
    cgroup_iostat_t	stats;
} cgroup_perdev_iostat_t;

enum {
    CG_IO_STAT_RBYTES			= 0,
    CG_IO_STAT_WBYTES			= 1,
    CG_IO_STAT_RIOS			= 2,
    CG_IO_STAT_WIOS			= 3,
    CG_IO_STAT_DBYTES			= 4,
    CG_IO_STAT_DIOS			= 5,
};

typedef struct {
    cgroup_pressures_t	cpu_pressures;
    cgroup_pressures_t	io_pressures;
    cgroup_pressures_t	mem_pressures;
    cgroup_pressures_t	irq_pressures;
    cgroup_cputime_t	cputime;
    /* I/O stats are per-cgroup::per-device */
    int			container;
} cgroup2_t;

enum {
    CG_CPU_STAT_USER			= 0,
    CG_CPU_STAT_SYSTEM			= 1,
    CG_CPU_STAT_USAGE			= 2,
};

enum {
    CG_PSI_SOME	= 0x1,
    CG_PSI_FULL	= 0x2,
};

/*
 * General cgroup interfaces
 */
typedef void (*cgroup_setup_t)(void *);
typedef void (*cgroup_refresh_t)(const char *, const char *, void *);
extern char *cgroup_find_subsys(pmInDom, filesys_t *);

extern void refresh_cgroup_subsys(void);
extern void refresh_cgroup_filesys(void);
extern void refresh_cgroups1(const char *, size_t, void *);
extern void refresh_cgroups2(const char *, size_t, void *);

extern char *cgroup_container_path(char *, size_t, const char *);
extern char *cgroup_container_search(const char *, char *, int);

extern unsigned int cgroup_version;

#endif /* _CGROUP_H */
