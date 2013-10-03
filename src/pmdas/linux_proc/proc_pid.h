/*
 * Linux /proc/<pid>/... Clusters
 *
 * Copyright (c) 2013 Red Hat.
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

/*
 * /proc/<pid>/stat metrics
 */
#define PROC_PID_STAT_PID 		 0
#define PROC_PID_STAT_CMD 		 1
#define PROC_PID_STAT_STATE 		 2
#define PROC_PID_STAT_PPID 		 3
#define PROC_PID_STAT_PGRP 		 4
#define PROC_PID_STAT_SESSION 		 5
#define PROC_PID_STAT_TTY 		 6
#define PROC_PID_STAT_TTY_PGRP 		 7
#define PROC_PID_STAT_FLAGS 		 8
#define PROC_PID_STAT_MINFLT 		 9
#define PROC_PID_STAT_CMIN_FLT 		 10
#define PROC_PID_STAT_MAJ_FLT 		 11
#define PROC_PID_STAT_CMAJ_FLT 		 12
#define PROC_PID_STAT_UTIME 		 13
#define PROC_PID_STAT_STIME 		 14
#define PROC_PID_STAT_CUTIME 		 15
#define PROC_PID_STAT_CSTIME 		 16
#define PROC_PID_STAT_PRIORITY 		 17
#define PROC_PID_STAT_NICE 		 18
#define PROC_PID_STAT_REMOVED 		 19
#define PROC_PID_STAT_IT_REAL_VALUE 	 20
#define PROC_PID_STAT_START_TIME 	 21
#define PROC_PID_STAT_VSIZE 		 22
#define PROC_PID_STAT_RSS 		 23
#define PROC_PID_STAT_RSS_RLIM 		 24
#define PROC_PID_STAT_START_CODE 	 25
#define PROC_PID_STAT_END_CODE 		 26
#define PROC_PID_STAT_START_STACK 	 27
#define PROC_PID_STAT_ESP 		 28
#define PROC_PID_STAT_EIP 		 29
#define PROC_PID_STAT_SIGNAL 		 30
#define PROC_PID_STAT_BLOCKED 		 31
#define PROC_PID_STAT_SIGIGNORE 	 32
#define PROC_PID_STAT_SIGCATCH 		 33
#define PROC_PID_STAT_WCHAN 		 34
#define PROC_PID_STAT_NSWAP 		 35
#define PROC_PID_STAT_CNSWAP 		 36
#define PROC_PID_STAT_EXIT_SIGNAL	 37
#define PROC_PID_STAT_PROCESSOR      38
#define PROC_PID_STAT_TTYNAME        39
#define PROC_PID_STAT_WCHAN_SYMBOL   40
#define PROC_PID_STAT_PSARGS         41

/* number of fields in proc_pid_stat_entry_t */
#define NR_PROC_PID_STAT             42

/*
 * metrics in /proc/<pid>/status
 * Added by Mike Mason <mmlnx@us.ibm.com>
 */
#define PROC_PID_STATUS_UID          0
#define PROC_PID_STATUS_EUID         1
#define PROC_PID_STATUS_SUID         2
#define PROC_PID_STATUS_FSUID        3
#define PROC_PID_STATUS_GID          4
#define PROC_PID_STATUS_EGID         5
#define PROC_PID_STATUS_SGID         6
#define PROC_PID_STATUS_FSGID        7
#define PROC_PID_STATUS_UID_NM       8
#define PROC_PID_STATUS_EUID_NM      9
#define PROC_PID_STATUS_SUID_NM      10
#define PROC_PID_STATUS_FSUID_NM     11
#define PROC_PID_STATUS_GID_NM       12
#define PROC_PID_STATUS_EGID_NM      13
#define PROC_PID_STATUS_SGID_NM      14
#define PROC_PID_STATUS_FSGID_NM     15
#define PROC_PID_STATUS_SIGNAL       16
#define PROC_PID_STATUS_BLOCKED      17
#define PROC_PID_STATUS_SIGIGNORE    18
#define PROC_PID_STATUS_SIGCATCH     19
#define PROC_PID_STATUS_VMSIZE       20
#define PROC_PID_STATUS_VMLOCK       21
#define PROC_PID_STATUS_VMRSS        22
#define PROC_PID_STATUS_VMDATA       23
#define PROC_PID_STATUS_VMSTACK      24
#define PROC_PID_STATUS_VMEXE        25
#define PROC_PID_STATUS_VMLIB        26
#define PROC_PID_STATUS_VMSWAP	     27
#define PROC_PID_STATUS_THREADS	     28

/* number of metrics from /proc/<pid>/status */
#define NR_PROC_PID_STATUS           27

/*
 * metrics in /proc/<pid>/statm & /proc/<pid>/maps
 */
#define PROC_PID_STATM_SIZE		 0
#define PROC_PID_STATM_RSS		 1
#define PROC_PID_STATM_SHARE		 2
#define PROC_PID_STATM_TEXTRS		 3
#define PROC_PID_STATM_LIBRS		 4
#define PROC_PID_STATM_DATRS		 5
#define PROC_PID_STATM_DIRTY		 6
#define PROC_PID_STATM_MAPS      7

/* number of fields in proc_pid_statm_entry_t */
#define NR_PROC_PID_STATM        8

/*
 * metrics in /proc/<pid>/schedstat
 */
#define PROC_PID_SCHED_CPUTIME		0
#define PROC_PID_SCHED_RUNDELAY		1
#define PROC_PID_SCHED_PCOUNT		2
#define NR_PROC_PID_SCHED		3

/*
 * metrics in /proc/<pid>/io
 */
#define PROC_PID_IO_RCHAR		0
#define PROC_PID_IO_WCHAR		1
#define PROC_PID_IO_SYSCR		2
#define PROC_PID_IO_SYSCW		3
#define PROC_PID_IO_READ_BYTES		4
#define PROC_PID_IO_WRITE_BYTES		5
#define PROC_PID_IO_CANCELLED_BYTES	6

/*
 * metrics in /proc/<pid>/fd
 */
#define PROC_PID_FD_COUNT		0


/*
 * metrics in /proc/<pid>/cgroup
 */
#define PROC_PID_CGROUP			0

/*
 * metrics in /proc/<pid>/attr/current
 */
#define PROC_PID_LABEL			0

typedef struct {	/* /proc/<pid>/status */
    char *uid;
    char *gid;
    char *sigpnd;
    char *sigblk;
    char *sigign;
    char *sigcgt;
    char *vmsize;
    char *vmlck;
    char *vmrss;
    char *vmdata;
    char *vmstk;
    char *vmexe;
    char *vmlib;
    char *vmswap;
    char *threads;
} status_lines_t;

typedef struct {	/* /proc/<pid>/io */
    char *rchar;
    char *wchar;
    char *syscr;
    char *syscw;
    char *readb;
    char *writeb;
    char *cancel;
} io_lines_t;

enum {
    PROC_PID_FLAG_VALID			= 1<<0,
    PROC_PID_FLAG_STAT_FETCHED		= 1<<1,
    PROC_PID_FLAG_STATM_FETCHED		= 1<<2,
    PROC_PID_FLAG_MAPS_FETCHED		= 1<<3,
    PROC_PID_FLAG_STATUS_FETCHED	= 1<<4,
    PROC_PID_FLAG_SCHEDSTAT_FETCHED	= 1<<5,
    PROC_PID_FLAG_IO_FETCHED		= 1<<6,
    PROC_PID_FLAG_WCHAN_FETCHED		= 1<<7,
    PROC_PID_FLAG_FD_FETCHED		= 1<<8,
    PROC_PID_FLAG_CGROUP_FETCHED	= 1<<9,
    PROC_PID_FLAG_LABEL_FETCHED		= 1<<10,
};

typedef struct {
    int			id;	/* pid, hash key and internal instance id */
    int			flags;	/* combinations of PROC_PID_FLAG_* values */
    char		*name;	/* external instance name (<pid> cmdline) */

    /* /proc/<pid>/stat cluster */
    int			stat_buflen;
    char		*stat_buf;

    /* /proc/<pid>/statm and /proc/<pid>/maps cluster */
    int			statm_buflen;
    char		*statm_buf;
    int			maps_buflen;
    char		*maps_buf;

    /* /proc/<pid>/status cluster */
    int			status_buflen;
    char		*status_buf;
    status_lines_t	status_lines;

    /* /proc/<pid>/schedstat cluster */
    int			schedstat_buflen;
    char		*schedstat_buf;

    /* /proc/<pid>/io cluster */
    int			io_buflen;
    char		*io_buf;
    io_lines_t		io_lines;

    /* /proc/<pid>/wchan cluster */
    int			wchan_buflen;
    char		*wchan_buf;

    /* /proc/<pid>/fd cluster */
    int			fd_buflen;
    uint32_t		fd_count;
    char		*fd_buf;

    /* /proc/<pid>/cgroup cluster */
    int			cgroup_id;

    /* /proc/<pid>/attr/current cluster */
    int			label_id;
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

/* refresh the proc indom, reset all "fetched" flags */
extern int refresh_proc_pid(proc_pid_t *, int, const char *);

/* fetch a proc/<pid>/stat entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_stat(int, proc_pid_t *);

/* fetch a proc/<pid>/statm entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_statm(int, proc_pid_t *);

/* fetch a proc/<pid>/status entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_status(int, proc_pid_t *);

/* fetch a proc/<pid>/maps entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_maps(int, proc_pid_t *);

/* fetch a proc/<pid>/schedstat entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_schedstat(int, proc_pid_t *);

/* fetch a proc/<pid>/io entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_io(int, proc_pid_t *);

/* fetch a proc/<pid>/fd entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_fd(int, proc_pid_t *);

/* fetch a proc/<pid>/cgroup entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_cgroup(int, proc_pid_t *);

/* fetch a proc/<pid>/attr/current entry for pid */
extern proc_pid_entry_t *fetch_proc_pid_label(int, proc_pid_t *);

/* extract the ith space separated field from a buffer */
extern char *_pm_getfield(char *, int);

#endif /* _PROC_PID_H */
