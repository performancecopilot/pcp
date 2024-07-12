/*
 * Linux /proc/<pid>/... Clusters
 *
 * Copyright (c) 2013-2015,2018-2022 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifndef _PROC_PID_H
#define _PROC_PID_H

#include "hotproc.h"

/*
 * Maximim length of psargs and proc instance name.
 */
#define PROC_PID_STAT_CMD_MAXLEN	4096

/*
 * metrics in /proc/<pid>/stat
 */
typedef struct {
    char	*cmd;
    char	state[4];
    uint32_t	ppid;
    uint32_t	pgrp;
    uint32_t	session;
    uint32_t	tty;
    int32_t	tty_pgrp;
    uint32_t	flags;
    uint32_t	minflt;
    uint32_t	cminflt;
    uint32_t	majflt;
    uint32_t	cmajflt;
    uint64_t	utime;
    uint64_t	stime;
    uint64_t	cutime;
    uint64_t	cstime;
    int32_t	priority;
    int32_t	nice;
    uint32_t	it_real_value;
    uint64_t	start_time;
    uint64_t	vsize;
    uint64_t	rss;
    uint64_t	rss_rlim;
    uint32_t	start_code;
    uint32_t	end_code;
    uint32_t	start_stack;
    uint32_t	esp;
    uint32_t	eip;
    uint32_t	signal;
    uint32_t	blocked;
    uint32_t	sigignore;
    uint32_t	sigcatch;
    uint64_t	wchan;
    uint32_t	nswap;
    uint32_t	cnswap;
    uint32_t	exit_signal;
    uint32_t	processor;
    uint32_t	rtpriority;
    uint32_t	policy;
    uint64_t	delayacct_blkio_time;
    uint64_t	guest_time;
    uint64_t	cguest_time;
} proc_pid_stat_t;

typedef struct {
    int32_t	runnable;
    int32_t	blocked;
    int32_t	sleeping;
    int32_t	stopped;
    int32_t	swapped;
    int32_t	kernel;
    int32_t	defunct;
    int32_t	unknown;
} proc_runq_t;

/*
 * flags for optional metrics in /proc/<pid>/status
 * (IOW these metrics are kernel version dependent)
 */
enum {
    PROC_STATUS_FLAG_ENVID		= 1<<1,
    PROC_STATUS_FLAG_TGID		= 1<<2,
    PROC_STATUS_FLAG_NGID		= 1<<3,
    PROC_STATUS_FLAG_NSTGID		= 1<<4,
    PROC_STATUS_FLAG_NSPID		= 1<<5,
    PROC_STATUS_FLAG_NSPGID		= 1<<6,
    PROC_STATUS_FLAG_NSSID		= 1<<7,
    PROC_STATUS_FLAG_CPUSALLOWED	= 1<<8,
};

/*
 * metrics in /proc/<pid>/status
 */
typedef struct {
    int		flags;
    uint32_t	ngid;
    uint32_t	tgid;
    uint32_t	uid;
    uint32_t	euid;
    uint32_t	suid;
    uint32_t	fsuid;
    uint32_t	gid;
    uint32_t	egid;
    uint32_t	sgid;
    uint32_t	fsgid;
    uint32_t	envid;
    int		nstgid;
    int		nspid;
    int		nspgid;
    int		nssid;
    int		sigpnd;
    int		sigblk;
    int		sigign;
    int		sigcgt;
    uint32_t	vmpeak;
    uint32_t	vmsize;
    uint32_t	vmlck;
    uint32_t	vmpin;
    uint32_t	vmhwm;
    uint32_t	vmrss;
    uint32_t	vmdata;
    uint32_t	vmstk;
    uint32_t	vmexe;
    uint32_t	vmlib;
    uint32_t	vmpte;
    uint32_t	vmswap;
    uint32_t	threads;
    uint32_t	vctxsw;
    uint32_t	nvctxsw;
    int		cpusallowed;
} proc_pid_status_t;

/*
 * metrics in /proc/<pid>/statm & /proc/<pid>/maps
 */
typedef struct {
    uint32_t	size;
    uint32_t	rss;
    uint32_t	share;
    uint32_t	textrs;
    uint32_t	librs;
    uint32_t	datrs;
    uint32_t	dirty;
} proc_pid_statm_t;

/*
 * metrics in /proc/<pid>/schedstat
 */
typedef struct {
    uint64_t	cputime;
    uint64_t	rundelay;
    uint64_t	count;
} proc_pid_schedstat_t;

/*
 * metrics in /proc/<pid>/io
 */
typedef struct {
    uint64_t	rchar;
    uint64_t	wchar;
    uint64_t	syscr;
    uint64_t	syscw;
    uint64_t	readb;
    uint64_t	writeb;
    uint64_t	cancel;
} proc_pid_io_t;

/*
 * metrics in /proc/<pid>/smaps_rollup
 */
typedef struct {
    uint64_t	rss;
    uint64_t	pss;
    uint64_t	pss_anon;
    uint64_t	pss_dirty;
    uint64_t	pss_file;
    uint64_t	pss_shmem;
    uint64_t	shared_clean;
    uint64_t	shared_dirty;
    uint64_t	private_clean;
    uint64_t	private_dirty;
    uint64_t	referenced;
    uint64_t	anonymous;
    uint64_t	lazyfree;
    uint64_t	anonhugepages;
    uint64_t	shmempmdmapped;
    uint64_t	filepmdmapped;
    uint64_t	shared_hugetlb;
    uint64_t	private_hugetlb;
    uint64_t	swap;
    uint64_t	swappss;
    uint64_t	locked;
} proc_pid_smaps_t;

/*
 * metrics in /proc/<pid>/fdinfo/
 */
typedef struct {
    /* Generic DRM data */
    char	drm_driver[32];
    uint64_t	drm_client_id;
    char	drm_pdev[32];
    uint64_t	drm_memory_vram;
    uint64_t	drm_memory_gtt;
    uint64_t	drm_memory_cpu;
    uint64_t	drm_shared_vram;
    uint64_t	drm_shared_gtt;
    uint64_t	drm_shared_cpu;
    /* AMD GPU specific data */
    uint64_t	amd_memory_visible_vram;
    uint64_t	amd_evicted_vram;
    uint64_t	amd_evicted_visible_vram;
    uint64_t	amd_requested_vram;
    uint64_t	amd_requested_visible_vram;
    uint64_t	amd_requested_gtt;
} proc_pid_fdinfo_t;

enum {
    PROC_PID_FLAG_VALID		= 1<<0,

    PROC_PID_FLAG_STAT		= 1<<1,
    PROC_PID_FLAG_STATM		= 1<<2,
    PROC_PID_FLAG_MAPS		= 1<<3,
    PROC_PID_FLAG_STATUS	= 1<<4,
    PROC_PID_FLAG_SCHEDSTAT	= 1<<5,
    PROC_PID_FLAG_IO		= 1<<6,
    PROC_PID_FLAG_WCHAN		= 1<<7,
    PROC_PID_FLAG_FD		= 1<<8,
    PROC_PID_FLAG_CGROUP	= 1<<9,
    PROC_PID_FLAG_LABEL		= 1<<10,
    PROC_PID_FLAG_ENVIRON	= 1<<11,
    PROC_PID_FLAG_OOM_SCORE	= 1<<12,
    PROC_PID_FLAG_SMAPS		= 1<<13,
    PROC_PID_FLAG_CWD		= 1<<14,
    PROC_PID_FLAG_EXE		= 1<<15,
    PROC_PID_FLAG_AUTOGROUP	= 1<<16,
    PROC_PID_FLAG_FDINFO	= 1<<17,
};

typedef struct {
    int			id;	/* pid, hash key and internal instance id */
    int			pad;
    unsigned int	fetched;   /* PROC_PID_FLAG_* values (sample attempt) */
    unsigned int	success;   /* PROC_PID_FLAG_* values (sample success) */
    char		*name;	/* full command line and args prefixed by PID */
    char		*psargs;   /* offset to start of process arguments in name */
    char		*instname; /* external instance name (truncated <pid> cmdline) */

    /* buffers for which length is held below (here for struct alignment) */
    char		*maps_buf;
    char		*wchan_buf;
    char		*environ_buf;

    /* /proc/<pid>/stat cluster */
    proc_pid_stat_t	stat;

    /* /proc/<pid>/statm cluster */
    proc_pid_statm_t	statm;

    /* /proc/<pid>/status cluster */
    proc_pid_status_t	status;

    /* /proc/<pid>/schedstat cluster */
    proc_pid_schedstat_t schedstat;

    /* /proc/<pid>/io cluster */
    proc_pid_io_t	io;

    /* /proc/<pid>/smaps_rollup cluster */
    proc_pid_smaps_t	smaps;

    /* /proc/<pid>/maps cluster */
    size_t		maps_buflen;

    /* /proc/<pid>/wchan cluster */
    size_t		wchan_buflen;

    /* /proc/<pid>/environ cluster */
    size_t 		environ_buflen;

    /* /proc/<pid>/fd cluster */
    uint32_t		fd_count;

    /* /proc/<pid>/cgroup cluster */
    int			cgroup_id;
    int			container_id;

    /* /proc/<pid>/attr/current cluster */
    int			label_id;

    /* /proc/<pid>/oom_score cluster */
    uint32_t		oom_score;

    /* /proc/<pid>/cwd cluster */
    int			cwd_id;

    /* /proc/<pid>/exe cluster */
    int			exe_id;

    /* /proc/<pid>/autogroup cluster */
    uint32_t		autogroup_id;
    int32_t		autogroup_nice;

    /* /proc/<pid>/fdinfo cluster */
    proc_pid_fdinfo_t	fdinfo;
} proc_pid_entry_t;

typedef struct {
    __pmHashCtl		pidhash;	/* hash table for current pids */
    pmdaIndom		*indom;		/* instance domain table */
} proc_pid_t;

typedef struct {
    int			count;		/* number of processes in the list */
    int			size;		/* size of the buffer (pids) allocated */
    int			*pids;		/* array of process identifiers */
    int			threads;	/* /proc/PID/{xxx,task/PID/xxx} flag */
} proc_pid_list_t;

/* lookup a proc hash entry */
extern proc_pid_entry_t *proc_pid_entry_lookup(int, proc_pid_t *);

/* refresh the proc indom, reset all "fetched" flags */
extern int refresh_proc_pid(proc_pid_t *, proc_runq_t *, int, const char *, const char *, int);

/* refresh the hotproc indom, checking against the current configuration */
extern int refresh_hotproc_pid(proc_pid_t *, int, const char *);

/* return aggregate statistics about the hotproc metrics */
extern int get_hot_totals(double * ta, double * ti, double * tt, double * tci );

/* get the hotproc statistics for a pid if it is currently "hot" */
extern int get_hotproc_node(pid_t pid, process_t **getnode);

/* restart the timer that calculates the hotproc statistics for each process */
extern void reset_hotproc_timer(void);

/* clear the hotlist and stop the timer */
extern void disable_hotproc();

/* init the hotproc data structures */
extern void init_hotproc_pid(proc_pid_t *);

/* fetch a proc/<pid>/stat entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_stat(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/wchan entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_wchan(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/environ entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_environ(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/statm entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_statm(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/status entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_status(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/smaps_rollup entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_smaps(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/maps entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_maps(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/schedstat entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_schedstat(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/io entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_io(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/fd entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_fd(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/cgroup entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_cgroup(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/attr/current entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_label(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/oom_score entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_oom_score(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/cwd entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_cwd(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/exe entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_exe(int, proc_pid_t *, int *);

/* fetch a proc/<pid>/autogroup entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_autogroup(int, proc_pid_t *, int *);

/* fetch data from proc/<pid>/fdinfo/ entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_fdinfo(int, proc_pid_t *, int *);

#endif /* _PROC_PID_H */
