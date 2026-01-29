/*
 * Copyright (c) 2025 Red Hat.
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
#ifndef _KINFO_PROC_H
#define _KINFO_PROC_H

/*
 * Metrics defined in kernel data structures, typically found hereabouts:
 * /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include/sys/{sysctl,proc}.h
*/

#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/mach_time.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#define PROC_CMD_MAXLEN  4096	/* Maximum length of psargs and process name */
#define PROC_FLAG_VALID  (1<<0)	/* Process observed during the current sample */
#define PROC_FLAG_PINFO  (1<<1)	/* Success in calling proc_pidinfo for the PID */
#define PROC_FLAG_THREAD (1<<2)	/* Given process structure represents a thread */

/*
 * Process metrics from kinfo_proc and libproc
 */
typedef struct {
    int		id;	/* pid, hash key and internal instance id */
    int		flags;

    char	*psargs;   /* offset to start of process arguments in name */
    char	*instname; /* external instance name (truncated <pid> cmd) */

    char	comm[MAXCOMLEN+1];
    char	wchan[WMESGLEN+1];
    char	state[4];

    uint32_t	ppid;
    uint32_t	pgid;
    uint32_t	tpgid;
    uint32_t	uid;
    uint32_t	gid;
    uint32_t	ngid;
    uint32_t	euid;
    uint32_t	egid;
    uint32_t	suid;
    uint32_t	sgid;
    uint32_t	majflt;
    uint32_t	threads;
    uint32_t	translated;
    uint32_t	fd_count;	/* open file descriptor count */
    int32_t	usrpri;
    int32_t	priority;
    int32_t	nice;
    dev_t	tty;
    uint64_t	utime;
    uint64_t	stime;
    uint64_t	start_time;
    uint64_t	wchan_addr;
    uint64_t	rss;
    uint64_t	size;
    uint64_t	textrss;
    uint64_t	textsize;
    uint64_t	pswitch;
    uint64_t	read_bytes;	/* disk I/O bytes read */
    uint64_t	write_bytes;	/* disk I/O bytes written */
    uint64_t	logical_writes;	/* logical disk write operations */
    uint64_t	phys_footprint;	/* physical memory footprint */

    /* string hash keys */
    int		cwd_id;
    int		exe_id;
    int		cmd_id;
    int		msg_id;
} darwin_proc_t;

typedef struct {
    int32_t	runnable;
    int32_t	blocked;
    int32_t	sleeping;
    int32_t	stopped;
    int32_t	swapped;
    int32_t	defunct;
    int32_t	unknown;
    int32_t	kernel;
} darwin_runq_t;

typedef __pmHashCtl darwin_procs_t;	/* hash table for current pids */

extern char *proc_strings_lookup(int);
extern int proc_strings_insert(const char *);

extern void darwin_refresh_processes(pmdaIndom *,
			darwin_procs_t *, darwin_runq_t *, int);

#endif /* _KINFO_PROC_H */
