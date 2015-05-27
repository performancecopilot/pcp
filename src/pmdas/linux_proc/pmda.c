/*
 * proc PMDA
 *
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Portions Copyright (c) 2002 International Business Machines Corp.
 * Portions Copyright (c) 2007-2011 Aconex.  All Rights Reserved.
 * Portions Copyright (c) 2012-2015 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"
#include "contexts.h"

#include <ctype.h>
#include <unistd.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>

#include "../linux/convert.h"
#include "clusters.h"
#include "indom.h"
#include "hotproc.h"

#include "getinfo.h"
#include "proc_pid.h"
#include "proc_runq.h"
#include "proc_dynamic.h"
#include "ksym.h"
#include "cgroups.h"

/* globals */
static int			_isDSO = 1;	/* =0 I am a daemon */
static int			rootfd = -1;	/* af_unix pmdaroot */
static proc_pid_t		proc_pid;
static proc_pid_t		hotproc_pid;
static struct utsname		kernel_uname;
static proc_runq_t		proc_runq;
static int			all_access;	/* =1 no access checks */
static int			have_access;	/* =1 recvd uid/gid */
static size_t			_pm_system_pagesize;
static unsigned int		threads;	/* control.all.threads */
static char *			cgroups;	/* control.all.cgroups */
int				conf_gen;	/* hotproc config version, if zero hotproc not configured yet */
long				hz;

/*
 * Note on "jiffies".
 * In the Linux kernel jiffies are always "unsigned long" (aka
 * KERNEL_ULONG), so to scale from jiffies to milliseconds, the multiplier
 * is 1000 / hz.
 * Unfortunately this presents some overflow and precision challenges.
 * 1. Variables used to hold an intermediate jiffies value should
 *    be declared __pm_kernel_ulong_t
 * 2. If this value is instantiated from a string (as in the /proc
 *    stats files, it is safe to use strtoul().
 * 3. The resultant PCP metric should be in units of msec and may be
 *    of type KERNEL_ULONG or PM_TYPE_U64 or PM_TYPE_U32.  KERNEL_ULONG
 *    will become one of the other types depending on the platform. In
 *    the case of PM_TYPE+_32 overflow would occur in
 *    (2^32-1)/(1000/hz)/(3600*24) days, and for current settings of
 *    hz (100) this means 4971+ days or more than 13.5 years, so this is
 *    not a practical issue nor a reason to mandate PM_TYPE_U64.
 * 4. But if the calculation is done in 32-bit integer arithmetic we
 *    risk earlier overflow much earlier (49+ days) because the *1000
 *    exceeds 32-bits ... so the arithmetic should always be forced
 *    to 64-bits by
 *    (a) explicit cast of the jiffies (__int64_t)jiffies * 1000 / hz or
 *    (b) declaring the variable holding the jiffies to be of type
 *        __int64_t
 *    we choose option (b).
 */

extern struct timeval   hotproc_update_interval;

char *proc_statspath = "";	/* optional path prefix for all stats files */

/*
 * The proc instance domain table is direct lookup and sparse.
 * It is initialized in proc_init(), see below.
 */
static pmdaIndom indomtab[NUM_INDOMS];

/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric metrictab[] = {

/*
 * proc/<pid>/stat cluster
 */

/* proc.nprocs */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.pid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,0), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.cmd */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,1), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sname */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,2), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.ppid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,3), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.pgrp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,4), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.session */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,5), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.tty */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,6), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.tty_pgrp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,7), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.flags */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,8), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.minflt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,9), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cmin_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,10), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.maj_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,11), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cmaj_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,12), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.utime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,13), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.stime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,14), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.cutime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,15), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.cstime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,16), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.priority */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,17), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.nice */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,18), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

#if 0
/* invalid field */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,19), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },
#endif

/* proc.psinfo.it_real_value */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,20), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.start_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,21), PM_TYPE_U64, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.vsize */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,22), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,23), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.rss_rlim */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,24), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.start_code */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,25), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.end_code */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,26), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.start_stack */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,27), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.esp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,28), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.eip */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,29), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.signal */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,30), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.blocked */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,31), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sigignore */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,32), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sigcatch */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,33), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.wchan */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,34), KERNEL_ULONG, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.nswap */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,35), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cnswap */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,36), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.exit_signal */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,37), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.processor -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,38), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.ttyname */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,39), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.wchan_s -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,40), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.psargs -- modified by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,41), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.rt_priority */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,42), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.policy */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,43), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.delayacct_blkio_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,44), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.guest_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,45), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.cguest_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,46), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
/* proc.psinfo.environ */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,47), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},
/*
 * proc/<pid>/status cluster
 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
 */

/* proc.id.uid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_UID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.euid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_EUID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.suid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SUID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsuid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_FSUID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.gid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_GID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.egid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_EGID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.sgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SGID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_FSGID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.uid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_UID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.euid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_EUID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.suid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SUID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsuid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_FSUID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.gid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_GID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.egid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_EGID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.sgid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SGID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsgid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_FSGID_NM),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.signal_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SIGNAL),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.blocked_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_BLOCKED),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.sigignore_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SIGIGNORE),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.sigcatch_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_SIGCATCH),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.memory.vmsize */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMSIZE),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmlock */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMLOCK),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMRSS),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmdata */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMDATA),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmstack */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMSTACK),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmexe */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMEXE),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmlib */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMLIB),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmswap */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMSWAP),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.psinfo.threads */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_THREADS),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.vctxsw */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VCTXSW),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.nvctxsw */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NVCTXSW),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.cpusallowed */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_CPUSALLOWED),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.ngid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NGID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.tgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_TGID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.namespaces.envid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_ENVID),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.memory.vmpeak */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMPEAK),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmpin */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMPIN),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmhwn */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMHWN),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmpte */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_VMPTE),
    PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.psinfo.cgroups */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_CGROUP, PROC_PID_CGROUP),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.labels */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_LABEL, PROC_PID_LABEL),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.namespaces.tpid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NSTGID),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.namespaces.pid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NSPID),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.namespaces.pgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NSPGID),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.namespaces.sid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS, PROC_PID_STATUS_NSSID),
    PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},


/*
 * proc/<pid>/statm cluster
 */

/* proc.memory.size */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,0), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,1), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.share */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,2), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.textrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,3), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.librss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,4), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.datrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,5), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.dirty */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,6), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.maps -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,7), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * proc/<pid>/schedstat cluster
 */

/* proc.schedstat.cpu_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_SCHEDSTAT,0), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0)}},
/* proc.schedstat.run_delay */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_SCHEDSTAT,1), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0)}},
/* proc.schedstat.pcount */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_SCHEDSTAT,2), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)}},

/*
 * proc/<pid>/io cluster
 */
/* proc.io.rchar */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,0), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},
/* proc.io.wchar */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,1), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},
/* proc.io.syscr */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,2), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)}},
/* proc.io.syscw */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,3), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE)}},
/* proc.io.read_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,4), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},
/* proc.io.write_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,5), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},
/* proc.io.cancelled_write_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_IO,6), PM_TYPE_U64, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/*
 * proc.runq cluster
 */

/* proc.runq.runnable */
  { &proc_runq.runnable,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.blocked */
  { &proc_runq.blocked,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.sleeping */
  { &proc_runq.sleeping,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.stopped */
  { &proc_runq.stopped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.swapped */
  { &proc_runq.swapped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.defunct */
  { &proc_runq.defunct,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.unknown */
  { &proc_runq.unknown,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.kernel */
  { &proc_runq.kernel,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * control groups cluster
 */

/* cgroups.subsys.hierarchy */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_SUBSYS, CG_SUBSYS_HIERARCHY), PM_TYPE_U32,
    CGROUP_SUBSYS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    
/* cgroups.subsys.count */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_SUBSYS, CG_SUBSYS_COUNT), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroups.subsys.num_cgroups */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_SUBSYS, CG_SUBSYS_NUMCGROUPS), PM_TYPE_U32,
    CGROUP_SUBSYS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    
/* cgroups.subsys.enabled */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_SUBSYS, CG_SUBSYS_ENABLED), PM_TYPE_U32,
    CGROUP_SUBSYS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    
/* cgroups.mounts.subsys */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_MOUNTS, CG_MOUNTS_SUBSYS), PM_TYPE_STRING,
    CGROUP_MOUNTS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* cgroups.mounts.count */
  { NULL, {PMDA_PMID(CLUSTER_CGROUP_MOUNTS, CG_MOUNTS_COUNT), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.cpuset.cpus */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUSET_GROUPS, CG_CPUSET_CPUS), PM_TYPE_STRING,
    CGROUP_CPUSET_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* cgroup.cpuset.mems */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUSET_GROUPS, CG_CPUSET_MEMS), PM_TYPE_STRING,
    CGROUP_CPUSET_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* cgroup.cpuacct.stat.user */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUACCT_GROUPS, CG_CPUACCT_USER), PM_TYPE_U64,
    CGROUP_CPUACCT_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* cgroup.cpuacct.stat.system */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUACCT_GROUPS, CG_CPUACCT_SYSTEM), PM_TYPE_U64,
    CGROUP_CPUACCT_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* cgroup.cpuacct.usage */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUACCT_GROUPS, CG_CPUACCT_USAGE), PM_TYPE_U64,
    CGROUP_CPUACCT_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.cpuacct.usage_percpu */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUACCT_GROUPS, CG_CPUACCT_PERCPU_USAGE), PM_TYPE_U64,
    CGROUP_PERCPUACCT_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.cpusched.shares */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUSCHED_GROUPS, CG_CPUSCHED_SHARES), PM_TYPE_U64,
    CGROUP_CPUSCHED_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* cgroup.memory.stat.cache */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_CACHE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RSS), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.rss_huge */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RSS_HUGE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.mapped_file */
  { NULL,
     { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_MAPPED_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.writeback */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_WRITEBACK), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.swap */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_SWAP), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.pgpgin */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_PGPGIN), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.pgpgout */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_PGPGOUT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.pgfault */
  { NULL,
    {PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_PGFAULT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.pgmajfault */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_PGMAJFAULT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.inactive_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_INACTIVE_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.active_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_ACTIVE_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.inactive_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_INACTIVE_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.active_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_ACTIVE_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.unevictable */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_UNEVICTABLE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.cache */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_CACHE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_RSS), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.rss_huge */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_RSS_HUGE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.mapped_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_MAPPED_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.writeback */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_WRITEBACK), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.swap */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_SWAP), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.pgpgin */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_PGPGIN), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.total.pgpgout */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_PGPGOUT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.total.pgfault */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_PGFAULT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.total.pgmajfault */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_PGMAJFAULT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.memory.stat.total.inactive_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_INACTIVE_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.active_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_ACTIVE_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.inactive_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_INACTIVE_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.active_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_ACTIVE_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.total.unevictable */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_TOTAL_UNEVICTABLE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.recent.rotated_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RECENT_ROTATED_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.recent.rotated_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RECENT_ROTATED_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.recent.scanned_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RECENT_SCANNED_ANON), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.stat.recent.scanned_file */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_STAT_RECENT_SCANNED_FILE), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.usage */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_USAGE_IN_BYTES), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.limit */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_LIMIT_IN_BYTES), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* cgroup.memory.failcnt */
  { NULL,
    { PMDA_PMID(CLUSTER_MEMORY_GROUPS, CG_MEMORY_FAILCNT), PM_TYPE_U64,
    CGROUP_MEMORY_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.netclass.classid */
  { NULL,
    { PMDA_PMID(CLUSTER_NETCLS_GROUPS, CG_NETCLS_CLASSID), PM_TYPE_U64,
    CGROUP_NETCLS_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* cgroup.blkio.dev.io_merged.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOMERGED_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_merged.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOMERGED_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_merged.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOMERGED_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_merged.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOMERGED_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_merged.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOMERGED_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_queued.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOQUEUED_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_queued.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOQUEUED_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_queued.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOQUEUED_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_queued.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOQUEUED_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_queued.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOQUEUED_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_service_bytes.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICEBYTES_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.io_service_bytes.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICEBYTES_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.io_service_bytes.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICEBYTES_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.io_service_bytes.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICEBYTES_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.io_service_bytes.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICEBYTES_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.io_serviced.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICED_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_serviced.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICED_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_serviced.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICED_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_serviced.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICED_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_serviced.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICED_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.dev.io_service_time.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICETIME_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_service_time.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICETIME_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_service_time.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICETIME_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_service_time.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICETIME_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_service_time.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOSERVICETIME_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_wait_time.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOWAITTIME_READ), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_wait_time.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOWAITTIME_WRITE), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_wait_time.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOWAITTIME_SYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_wait_time.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOWAITTIME_ASYNC), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.io_wait_time.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_IOWAITTIME_TOTAL), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.dev.sectors */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_SECTORS), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.dev.time */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_PERDEVBLKIO_TIME), PM_TYPE_U64,
    CGROUP_PERDEVBLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* cgroup.blkio.all.io_merged.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOMERGED_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_merged.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOMERGED_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_merged.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOMERGED_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_merged.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOMERGED_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_merged.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOMERGED_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_queued.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOQUEUED_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_queued.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOQUEUED_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_queued.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOQUEUED_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_queued.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOQUEUED_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_queued.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOQUEUED_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_service_bytes.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICEBYTES_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.io_service_bytes.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICEBYTES_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.io_service_bytes.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICEBYTES_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.io_service_bytes.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICEBYTES_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.io_service_bytes.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICEBYTES_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.io_serviced.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICED_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_serviced.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICED_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_serviced.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICED_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_serviced.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICED_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_serviced.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICED_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* cgroup.blkio.all.io_service_time.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICETIME_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_service_time.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICETIME_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_service_time.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICETIME_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_service_time.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICETIME_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_service_time.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOSERVICETIME_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_wait_time.read */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOWAITTIME_READ), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_wait_time.write */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOWAITTIME_WRITE), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_wait_time.sync */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOWAITTIME_SYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_wait_time.async */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOWAITTIME_ASYNC), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.io_wait_time.total */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_IOWAITTIME_TOTAL), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) } },

/* cgroup.blkio.all.sectors */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_SECTORS), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* cgroup.blkio.all.time */
  { NULL,
    { PMDA_PMID(CLUSTER_BLKIO_GROUPS, CG_BLKIO_TIME), PM_TYPE_U64,
    CGROUP_BLKIO_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/*
 * proc/<pid>/fd cluster
 */

/* proc.fd.count */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_FD,0), PM_TYPE_U32,
    PROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * Metrics control cluster
 */

/* proc.control.all.threads */
  { &threads,
    { PMDA_PMID(CLUSTER_CONTROL, 1), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.control.perclient.threads */
  { NULL,
    { PMDA_PMID(CLUSTER_CONTROL, 2), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.control.perclient.cgroups */
  { NULL,
    { PMDA_PMID(CLUSTER_CONTROL, 3), PM_TYPE_STRING,
    PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * hotproc specific clusters
 */
 
    /* hotproc.nprocs */
    { NULL,
      { PMDA_PMID(CLUSTER_HOTPROC_PID_STAT,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* hotproc.control.refresh */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_REFRESH),
      PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.control.config */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_CONFIG),
      PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.control.config_gen */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_CONFIG_GEN),
      PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.control.reload_config */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_RELOAD_CONFIG),
      PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuidle */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_CPUIDLE),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuburn */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_CPUBURN),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuother.transient */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_OTHER_TRANSIENT),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuother.not_cpuburn */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_OTHER_NOT_CPUBURN),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuother.total */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_OTHER_TOTAL),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* hotproc.total.cpuother.percent */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_GLOBAL,ITEM_HOTPROC_G_OTHER_PERCENT),
      PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0)} },
#if 0
    /* hotproc.predicate.syscalls */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_SYSCALLS),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,0)} },
#endif
    /* hotproc.predicate.ctxswitch */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_CTXSWITCH),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,-1,1, 0,PM_TIME_SEC,0)} },
    /* hotproc.predicate.virtualsize */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_VSIZE),
      PM_TYPE_U32, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0, PM_SPACE_KBYTE,0,0)} },
    /* hotproc.predicate.residentsize */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_RSIZE),
      PM_TYPE_U32, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0, PM_SPACE_KBYTE,0,0)} },
    /* hotproc.predicate.iodemand */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_IODEMAND),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(1,-1,0, PM_SPACE_KBYTE,PM_TIME_SEC,0)} },
    /* hotproc.predicate.iowait */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_IOWAIT),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0, 0,0,0)} },
    /* hotproc.predicate.schedwait */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_SCHEDWAIT),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0, 0,0,0)} },
    /* hotproc.predicate.cpuburn */
    { NULL, {PMDA_PMID(CLUSTER_HOTPROC_PRED,ITEM_HOTPROC_P_CPUBURN),
      PM_TYPE_FLOAT, HOTPROC_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0, 0,0,0)} },

};

pmInDom
proc_indom(int serial)
{
    return indomtab[serial].it_indom;
}

FILE *
proc_statsfile(const char *path, char *buffer, int size)
{
    snprintf(buffer, size, "%s%s", proc_statspath, path);
    buffer[size-1] = '\0';
    return fopen(buffer, "r");
}

static int
proc_refresh(pmdaExt *pmda, int *need_refresh)
{
    char cgroup[MAXPATHLEN];
    proc_container_t *container;
    int sts, cgrouplen = 0;

    if ((container = proc_ctx_container(pmda->e_context)) != NULL) {
	if ((sts = pmdaRootContainerCGroupName(rootfd,
				container->name, container->length,
				cgroup, sizeof(cgroup))) < 0)
	    return sts;
	cgrouplen = sts;
    }

    if (need_refresh[CLUSTER_CGROUP_SUBSYS] ||
	need_refresh[CLUSTER_CGROUP_MOUNTS] ||
	need_refresh[CLUSTER_CPUSET_GROUPS] || 
	need_refresh[CLUSTER_CPUACCT_GROUPS] ||
	need_refresh[CLUSTER_CPUSCHED_GROUPS] ||
	need_refresh[CLUSTER_MEMORY_GROUPS] ||
	need_refresh[CLUSTER_NETCLS_GROUPS] ||
	need_refresh[CLUSTER_BLKIO_GROUPS] ||
	container) {

	refresh_cgroup_subsys();
	refresh_cgroup_filesys();

	if (need_refresh[CLUSTER_CPUSET_GROUPS])
	    refresh_cgroups("cpuset", cgroup, cgrouplen,
			    setup_cpuset, refresh_cpuset);
	if (need_refresh[CLUSTER_CPUACCT_GROUPS])
	    refresh_cgroups("cpuacct", cgroup, cgrouplen,
			    setup_cpuacct, refresh_cpuacct);
	if (need_refresh[CLUSTER_CPUSCHED_GROUPS])
	    refresh_cgroups("cpusched", cgroup, cgrouplen,
			    setup_cpusched, refresh_cpusched);
	if (need_refresh[CLUSTER_MEMORY_GROUPS])
	    refresh_cgroups("memory", cgroup, cgrouplen,
			    setup_memory, refresh_memory);
	if (need_refresh[CLUSTER_NETCLS_GROUPS])
	    refresh_cgroups("netcls", cgroup, cgrouplen,
			    setup_netcls, refresh_netcls);
	if (need_refresh[CLUSTER_BLKIO_GROUPS])
	    refresh_cgroups("blkio", cgroup, cgrouplen,
			    setup_blkio, refresh_blkio);
    }

    if (need_refresh[CLUSTER_PID_STAT] ||
	need_refresh[CLUSTER_PID_STATM] || 
	need_refresh[CLUSTER_PID_STATUS] ||
	need_refresh[CLUSTER_PID_IO] ||
	need_refresh[CLUSTER_PID_LABEL] ||
	need_refresh[CLUSTER_PID_CGROUP] ||
	need_refresh[CLUSTER_PID_SCHEDSTAT] ||
	need_refresh[CLUSTER_PID_FD] ||
	need_refresh[CLUSTER_PROC_RUNQ]) {
	refresh_proc_pid(&proc_pid,
		need_refresh[CLUSTER_PROC_RUNQ]? &proc_runq : NULL,
		proc_ctx_threads(pmda->e_context, threads),
		proc_ctx_cgroups(pmda->e_context, cgroups),
		container ? cgroup : NULL, cgrouplen);

    }
    if (need_refresh[CLUSTER_HOTPROC_PID_STAT] ||
        need_refresh[CLUSTER_HOTPROC_PID_STATM] ||
        need_refresh[CLUSTER_HOTPROC_PID_STATUS] ||
        need_refresh[CLUSTER_HOTPROC_PID_IO] ||
        need_refresh[CLUSTER_HOTPROC_PID_LABEL] ||
        need_refresh[CLUSTER_HOTPROC_PID_CGROUP] ||
        need_refresh[CLUSTER_HOTPROC_PID_SCHEDSTAT] ||
        need_refresh[CLUSTER_HOTPROC_PID_FD] ||
        need_refresh[CLUSTER_HOTPROC_GLOBAL] ||
        need_refresh[CLUSTER_HOTPROC_PRED]){
        refresh_hotproc_pid(&hotproc_pid,
                        proc_ctx_threads(pmda->e_context, threads),
                        proc_ctx_cgroups(pmda->e_context, cgroups));
    }
    return 0;
}

static int
proc_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS] = { 0 };
    char		newname[16];		/* see Note below */
    int			sts;

    switch (indomp->serial) {
    case PROC_INDOM:
    	need_refresh[CLUSTER_PID_STAT]++;
    	need_refresh[CLUSTER_PID_STATM]++;
        need_refresh[CLUSTER_PID_STATUS]++;
        need_refresh[CLUSTER_PID_LABEL]++;
        need_refresh[CLUSTER_PID_CGROUP]++;
        need_refresh[CLUSTER_PID_SCHEDSTAT]++;
        need_refresh[CLUSTER_PID_IO]++;
        need_refresh[CLUSTER_PID_FD]++;
	break;
    case HOTPROC_INDOM:
        need_refresh[CLUSTER_HOTPROC_PID_STAT]++;
        need_refresh[CLUSTER_HOTPROC_PID_STATM]++;
        need_refresh[CLUSTER_HOTPROC_PID_STATUS]++;
        need_refresh[CLUSTER_HOTPROC_PID_LABEL]++;
        need_refresh[CLUSTER_HOTPROC_PID_CGROUP]++;
        need_refresh[CLUSTER_HOTPROC_PID_SCHEDSTAT]++;
        need_refresh[CLUSTER_HOTPROC_PID_IO]++;
        need_refresh[CLUSTER_HOTPROC_PID_FD]++;
        need_refresh[CLUSTER_HOTPROC_GLOBAL]++;
        need_refresh[CLUSTER_HOTPROC_PRED]++;
        break;

    case CGROUP_CPUSET_INDOM:
	need_refresh[CLUSTER_CPUSET_GROUPS]++;
	break;
    case CGROUP_CPUACCT_INDOM:
    case CGROUP_PERCPUACCT_INDOM:
	need_refresh[CLUSTER_CPUACCT_GROUPS]++;
	break;
    case CGROUP_CPUSCHED_INDOM:
	need_refresh[CLUSTER_CPUSCHED_GROUPS]++;
	break;
    case CGROUP_MEMORY_INDOM:
	need_refresh[CLUSTER_MEMORY_GROUPS]++;
	break;
    case CGROUP_NETCLS_INDOM:
	need_refresh[CLUSTER_NETCLS_GROUPS]++;
	break;
    case CGROUP_BLKIO_INDOM:
    case CGROUP_PERDEVBLKIO_INDOM:
	need_refresh[CLUSTER_BLKIO_GROUPS]++;
	break;
    case CGROUP_SUBSYS_INDOM:
	need_refresh[CLUSTER_CGROUP_SUBSYS]++;
	break;
    case CGROUP_MOUNTS_INDOM:
    	need_refresh[CLUSTER_CGROUP_MOUNTS]++;
	break;
    /* no default label : pmdaInstance will pick up errors */
    }

    if ((indomp->serial == PROC_INDOM || indomp->serial == HOTPROC_INDOM) &&
	inst == PM_IN_NULL && name != NULL) {
    	/*
	 * For the proc indoms if the name is a pid (as a string), and it
	 * contains only digits (i.e. it's not a full instance name) then
	 * reformat it to be exactly six digits, with leading zeros.
	 *
	 * Note that although format %06d is used here and in proc_pid.c,
	 *      the pid could be longer than this (in which case there
	 *      are no leading zeroes.  The size of newname[] is chosen
	 *	to comfortably accommodate a 32-bit pid (Linux maximum),
	 *      or max value of 4294967295 (10 digits)
	 */
	char *p;
	for (p = name; *p != '\0'; p++) {
	    if (!isdigit((int)*p))
	    	break;
	}
	if (*p == '\0') {
	    snprintf(newname, sizeof(newname), "%06d", atoi(name));
	    name = newname;
	}
    }

    sts = PM_ERR_PERMISSION;
    have_access = all_access || proc_ctx_access(pmda->e_context);
    if (have_access ||
	((indomp->serial != PROC_INDOM) && (indomp->serial != HOTPROC_INDOM))) {
	if ((sts = proc_refresh(pmda, need_refresh)) == 0)
	    sts = pmdaInstance(indom, inst, name, result, pmda);
    }
    have_access = all_access || proc_ctx_revert(pmda->e_context);

    return sts;
}

/*
 * callback provided to pmdaFetch
 */

static int
proc_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    pmInDom		indom;
    int			sts;
    int			have_totals;
    __int64_t		jiffies;
    const char		*cp;
    char		*f;
    proc_pid_entry_t	*entry;
    char 		*tail;
    char 		*tmpbuf;
    proc_pid_t		*active_proc_pid;
    double		ta, ti, tt, tci;
    process_t		*hotnode;

    active_proc_pid = &proc_pid;

    if (mdesc->m_user != NULL) {
	/* 
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not all metrics support this - those that
	 * don't have NULL for the m_user field in their respective
         * metrictab slot.
	 */

	switch (mdesc->m_desc.type) {
	case PM_TYPE_32:
	    atom->l = *(__int32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_64:
	    atom->ll = *(__int64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)mdesc->m_user;
	    break;
	case PM_TYPE_DOUBLE:
	    atom->d = *(double *)mdesc->m_user;
	    break;
	case PM_TYPE_STRING:
	    cp = *(char **)mdesc->m_user;
	    atom->cp = (char *)(cp ? cp : "");
	    break;
	default:
	    return 0;
	}
    }
    else
    switch (idp->cluster) {
    case CLUSTER_HOTPROC_GLOBAL:
	have_totals = get_hot_totals(&ta, &ti, &tt, &tci);

	switch (idp->item) {
	case ITEM_HOTPROC_G_REFRESH: /* hotproc.control.refresh */
	    atom->ul = hotproc_update_interval.tv_sec;
	    break;
	case ITEM_HOTPROC_G_CONFIG: /* hotproc.control.config */
	    tmpbuf = get_conf_buffer();
	    atom->cp = tmpbuf ? tmpbuf : "";
	    break;
	case ITEM_HOTPROC_G_CONFIG_GEN: /* hotproc.control.config_gen */
	    atom->ul = conf_gen;
	    break;
	case ITEM_HOTPROC_G_RELOAD_CONFIG: /* hotproc.control.reload_config */
	    atom->ul = 0;
	    break;
	case ITEM_HOTPROC_G_CPUIDLE: /* hotproc.total.cpuidle */
	    atom->f = have_totals ? tci : 0;
	    break;
	case ITEM_HOTPROC_G_CPUBURN: /* hotproc.total.cpuburn */
	    atom->f = have_totals ? ta : 0;
	    break;
	case ITEM_HOTPROC_G_OTHER_TRANSIENT: /* hotproc.total.cpuother.transient */
	    atom->f = have_totals ? tt : 0;
	    break;
	case ITEM_HOTPROC_G_OTHER_NOT_CPUBURN: /* hotproc.total.cpuother.not_cpuburn */
	    atom->f = have_totals ? ti : 0;
	    break;
	case ITEM_HOTPROC_G_OTHER_TOTAL: /* hotproc.total.cpuother.total */
	    atom->f = have_totals ? ti + tt : 0;
	    break;
	case ITEM_HOTPROC_G_OTHER_PERCENT: { /* hotproc.total.cpuother.percent */
	    double other = tt + ti;
	    double non_idle = other + ta;

	    /* if non_idle = 0, very unlikely,
	     * then the value here is meaningless
	     *
	     * Also if all the numbers are very small
	     * this is not accurate. Might want to dump this original metric
	     */
	    if (!have_totals || non_idle == 0)
		atom->f = 0;
	    else
		atom->f = other / non_idle * 100;
	    break;
	}

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_HOTPROC_PRED:
	sts = get_hotproc_node(inst, &hotnode);
	if (sts == 0)
	    return PM_ERR_INST;

	switch (idp->item) {
	    case ITEM_HOTPROC_P_SYSCALLS: /* No way to get this right now (maybe from systemtap?)*/
		return PM_ERR_PMID;
		break;
	    case ITEM_HOTPROC_P_CTXSWITCH: /* hotproc.predicate.ctxswitch */
		atom->f = hotnode->preds.ctxswitch;
		break;
	    case ITEM_HOTPROC_P_VSIZE: /* hotproc.predicate.virtualsize */
		atom->ul = hotnode->preds.virtualsize;
		break;
	    case ITEM_HOTPROC_P_RSIZE: /* hotproc.predicate.residentsize */
		atom->ul = hotnode->preds.residentsize;
		break;
	    case ITEM_HOTPROC_P_IODEMAND: /* hotproc.predicate.iodemand */
		atom->f = hotnode->preds.iodemand;
		break;
	    case ITEM_HOTPROC_P_IOWAIT: /* hotproc.predicate.iowait */
		atom->f = hotnode->preds.iowait;
		break;
	    case ITEM_HOTPROC_P_SCHEDWAIT: /* hotproc.predicate.schedwait */
		atom->f = hotnode->preds.schedwait;
		break;
	    case ITEM_HOTPROC_P_CPUBURN: /* (not in orig hotproc) hotproc.predicate.cpuburn */
		atom->f = hotnode->r_cpuburn;
		break;
	    default:
		return PM_ERR_PMID;
	}
	break;

    case CLUSTER_HOTPROC_PID_STAT:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_STAT:
	if (idp->item == 99) /* proc.nprocs */
	    atom->ul = active_proc_pid->indom->it_numinst;
	else {
	    static char ttyname[MAXPATHLEN];

	    if (!have_access)
		return PM_ERR_PERMISSION;
	    entry = fetch_proc_pid_stat(inst, active_proc_pid, &sts);
	    if (entry == NULL)
		return sts;

	    switch (idp->item) {
	    case PROC_PID_STAT_PID: /* proc.psinfo.pid */
		atom->ul = entry->id;
		break;

	    case PROC_PID_STAT_TTYNAME: /* proc.psinfo.tty */
		f = _pm_getfield(entry->stat_buf, PROC_PID_STAT_TTY);
		if (f == NULL)
		    atom->cp = "?";
		else {
		    dev_t dev = (dev_t)atoi(f);
		    atom->cp = get_ttyname_info(inst, dev, ttyname);
		}
		break;

	    case PROC_PID_STAT_CMD: /* proc.psinfo.cmd */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
		atom->cp = f + 1;
		atom->cp[strlen(atom->cp)-1] = '\0';
		break;

	    case PROC_PID_STAT_PSARGS: /* proc.psinfo.psargs */
		atom->cp = entry->name + 7;
		break;

	    case PROC_PID_STAT_STATE: /* string */ /* proc.psinfo.sname */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
	    	atom->cp = f;
		break;

	    case PROC_PID_STAT_VSIZE: /* proc.psinfo.vsize */
	    case PROC_PID_STAT_RSS_RLIM: /* bytes converted to kbytes */ /* proc.psinfo.rss_rlim */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
		atom->ul = (__uint32_t)strtoul(f, &tail, 0);
		atom->ul /= 1024;
		break;

	    case PROC_PID_STAT_RSS: /* pages converted to kbytes */ /* proc.psinfo.rss */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
		atom->ul = (__uint32_t)strtoul(f, &tail, 0);
		atom->ul *= _pm_system_pagesize / 1024;
		break;

	    case PROC_PID_STAT_UTIME: /* proc.psinfo.utime */
	    case PROC_PID_STAT_STIME: /* proc.psinfo.stime */
	    case PROC_PID_STAT_CUTIME: /* proc.psinfo.cutime */
	    case PROC_PID_STAT_CSTIME: /* proc.psinfo.cstime */
		/* unsigned jiffies converted to unsigned msecs */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
		jiffies = (__int64_t)strtoul(f, &tail, 0);
		_pm_assign_ulong(atom, jiffies * 1000 / hz);
		break;

	    case PROC_PID_STAT_PRIORITY: /* proc.psinfo.priority */
	    case PROC_PID_STAT_NICE: /* signed decimal int */ /* proc.psinfo.nice */
		f = _pm_getfield(entry->stat_buf, idp->item);
		if (f == NULL)
		    return 0;
		atom->l = (__int32_t)strtol(f, &tail, 0);
		break;

	    case PROC_PID_STAT_WCHAN: /* proc.psinfo.wchan */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return 0;
		_pm_assign_ulong(atom, (__pm_kernel_ulong_t)strtoull(f, &tail, 0));
		break;
 
	    case PROC_PID_STAT_ENVIRON: /* proc.psinfo.environ */
		atom->cp = entry->environ_buf ? entry->environ_buf : "";
		break;

	    case PROC_PID_STAT_WCHAN_SYMBOL: /* proc.psinfo.wchan_s */
		if (entry->wchan_buf)	/* 2.6 kernel, /proc/<pid>/wchan */
		    atom->cp = entry->wchan_buf;
		else {		/* old school (2.4 kernels, at least) */
		    char *wc;
		    /*
		     * Convert address to symbol name if requested
		     * Added by Mike Mason <mmlnx@us.ibm.com>
		     */
		    f = _pm_getfield(entry->stat_buf, PROC_PID_STAT_WCHAN);
		    if (f == NULL)
			return 0;
		    _pm_assign_ulong(atom, (__pm_kernel_ulong_t)strtoull(f, &tail, 0));
#if defined(HAVE_64BIT_LONG)
		    if ((wc = wchan(atom->ull)))
			atom->cp = wc;
		    else
			atom->cp = atom->ull ? f : "";
#else
		    if ((wc = wchan((__psint_t)atom->ul)))
			atom->cp = wc;
		    else
			atom->cp = atom->ul ? f : "";
#endif
		}
		break;

	    /* The following 2 case groups need to be here since the #defines don't match the index into the buffer */
	    case PROC_PID_STAT_RTPRIORITY: /* proc.psinfo.rt_priority */
	    case PROC_PID_STAT_POLICY: /* proc.psinfo.policy */
	    	if ((f = _pm_getfield(entry->stat_buf, idp->item - 3)) == NULL) /* Note the offset */
		    	return 0;
		    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	    	break;

	    case PROC_PID_STAT_DELAYACCT_BLKIO_TICKS: /* proc.psinfo.delayacct_blkio_time */
	    case PROC_PID_STAT_GUEST_TIME: /* proc.psinfo.guest_time */
	    case PROC_PID_STAT_CGUEST_TIME: /* proc.psinfo.cguest_time */
	    	/*
		 * unsigned jiffies converted to unsigned milliseconds
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item - 3)) == NULL)  /* Note the offset */
		    return 0;

		jiffies = (__uint64_t)strtoul(f, &tail, 0);
		atom->ull = jiffies * 1000 / hz;
	    	break;
	    case PROC_PID_STAT_START_TIME: /* proc.psinfo.start_time */
	    	/*
		 * unsigned jiffies converted to unsigned milliseconds
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return 0;

		jiffies = (__uint64_t)strtoul(f, &tail, 0);
		atom->ull = jiffies * 1000 / hz;
	    	break;

	    default: /* All the rest. Direct index by item */
		/*
		 * unsigned decimal int
		 */
		if (idp->item < NR_PROC_PID_STAT) {
		    if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    	return 0;
		    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
		}
		else
		    return PM_ERR_PMID;
		break;
	    }
	}
	break;

    case CLUSTER_HOTPROC_PID_STATM:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_STATM:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if (idp->item == PROC_PID_STATM_MAPS) {	/* proc.memory.maps */
	    if ((entry = fetch_proc_pid_maps(inst, active_proc_pid, &sts)) == NULL)
		return sts;
	    atom->cp = (entry->maps_buf ? entry->maps_buf : "");
	} else {
	    if ((entry = fetch_proc_pid_statm(inst, active_proc_pid, &sts)) == NULL)
		return sts;

	    if (idp->item <= PROC_PID_STATM_DIRTY) {
		/* unsigned int */
		if ((f = _pm_getfield(entry->statm_buf, idp->item)) == NULL)
		    return 0;
		atom->ul = (__uint32_t)strtoul(f, &tail, 0);
		atom->ul *= _pm_system_pagesize / 1024;
	    }
	    else
		return PM_ERR_PMID;
	}
    	break;

    case CLUSTER_HOTPROC_PID_SCHEDSTAT:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_SCHEDSTAT:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((entry = fetch_proc_pid_schedstat(inst, active_proc_pid, &sts)) == NULL)
	    return sts;

	if (idp->item < NR_PROC_PID_SCHED) {
	    if ((f = _pm_getfield(entry->schedstat_buf, idp->item)) == NULL)
		return 0;
	    if (idp->item == PROC_PID_SCHED_PCOUNT)
		_pm_assign_ulong(atom, (__pm_kernel_ulong_t)strtoul(f, &tail, 0));
	    else
		atom->ull  = (__uint64_t)strtoull(f, &tail, 0);
	}
	else
	    return PM_ERR_PMID;
    	break;

    case CLUSTER_HOTPROC_PID_IO:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_IO:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((entry = fetch_proc_pid_io(inst, active_proc_pid, &sts)) == NULL)
	    return sts;

	switch (idp->item) {

	case PROC_PID_IO_RCHAR: /* proc.io.rchar */
	    if ((f = _pm_getfield(entry->io_lines.rchar, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_WCHAR: /* proc.io.wchar */
	    if ((f = _pm_getfield(entry->io_lines.wchar, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_SYSCR: /* proc.io.syscr */
	    if ((f = _pm_getfield(entry->io_lines.syscr, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_SYSCW: /* proc.io.syscw */
	    if ((f = _pm_getfield(entry->io_lines.syscw, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_READ_BYTES: /* proc.io.read_bytes */
	    if ((f = _pm_getfield(entry->io_lines.readb, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_WRITE_BYTES: /* proc.io.write_bytes */
	    if ((f = _pm_getfield(entry->io_lines.writeb, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;
	case PROC_PID_IO_CANCELLED_BYTES: /* proc.io.cancelled_write_bytes */
	    if ((f = _pm_getfield(entry->io_lines.cancel, 1)) == NULL)
		atom->ull = 0;
	    else
		atom->ull = (__uint64_t)strtoull(f, &tail, 0);
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_HOTPROC_PID_STATUS:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_STATUS:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if ((entry = fetch_proc_pid_status(inst, active_proc_pid, &sts)) == NULL)
		return sts;

	switch (idp->item) {

	case PROC_PID_STATUS_UID: /* proc.id.uid */
	case PROC_PID_STATUS_EUID: /* proc.id.euid */
	case PROC_PID_STATUS_SUID: /* proc.id.suid */
	case PROC_PID_STATUS_FSUID: /* proc.id.fsuid */
	case PROC_PID_STATUS_UID_NM: /* proc.id.uid_nm */
	case PROC_PID_STATUS_EUID_NM: /* proc.id.euid_nm */
	case PROC_PID_STATUS_SUID_NM: /* proc.id.suid_nm */
	case PROC_PID_STATUS_FSUID_NM: { /* proc.id.fsuid_nm */
	    struct passwd *pwe;

	    if ((f = _pm_getfield(entry->status_lines.uid, (idp->item % 4) + 1)) == NULL)
		return 0;
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	    if (idp->item > PROC_PID_STATUS_FSUID) {
		if ((pwe = getpwuid((uid_t)atom->ul)) != NULL)
		    atom->cp = pwe->pw_name;
		else
		    atom->cp = "UNKNOWN";
	    }
	}
	break;

	case PROC_PID_STATUS_GID: /* proc.id.gid */
	case PROC_PID_STATUS_EGID: /* proc.id.egid */
	case PROC_PID_STATUS_SGID: /* proc.id.sgid */
	case PROC_PID_STATUS_FSGID: /* proc.id.fsgid */
	case PROC_PID_STATUS_GID_NM: /* proc.id.gid_nm */
	case PROC_PID_STATUS_EGID_NM: /* proc.id.egid_nm */
	case PROC_PID_STATUS_SGID_NM: /* proc.id.sgid_nm */
	case PROC_PID_STATUS_FSGID_NM: { /* proc.id.fsgid_nm */
	    struct group *gre;

	    if ((f = _pm_getfield(entry->status_lines.gid, (idp->item % 4) + 1)) == NULL)
		return 0;
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	    if (idp->item > PROC_PID_STATUS_FSGID) {
		if ((gre = getgrgid((gid_t)atom->ul)) != NULL) {
		    atom->cp = gre->gr_name;
		} else {
		    atom->cp = "UNKNOWN";
		}
	    }
	}
	break;

	case PROC_PID_STATUS_SIGNAL: /* proc.psinfo.signal_s */
	if ((atom->cp = _pm_getfield(entry->status_lines.sigpnd, 1)) == NULL)
	    return 0;
	break;

	case PROC_PID_STATUS_BLOCKED: /* proc.psinfo.blocked_s */
	if ((atom->cp = _pm_getfield(entry->status_lines.sigblk, 1)) == NULL)
	    return 0;
	break;

	case PROC_PID_STATUS_SIGCATCH: /* proc.psinfo.sigcatch_s */
	if ((atom->cp = _pm_getfield(entry->status_lines.sigcgt, 1)) == NULL)
	    return 0;
	break;

	case PROC_PID_STATUS_SIGIGNORE: /* proc.psinfo.sigignore_s */
	if ((atom->cp = _pm_getfield(entry->status_lines.sigign, 1)) == NULL)
	    return 0;
	break;

	case PROC_PID_STATUS_VMPEAK: /* proc.memory.vmpeak */
	if ((f = _pm_getfield(entry->status_lines.vmpeak, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMSIZE: /* proc.memory.vmsize */
	if ((f = _pm_getfield(entry->status_lines.vmsize, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMPIN: /* proc.memory.vmpin */
	if ((f = _pm_getfield(entry->status_lines.vmpin, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMHWN: /* proc.memory.vmhwn */
	if ((f = _pm_getfield(entry->status_lines.vmhwn, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMPTE: /* proc.memory.vmpte */
	if ((f = _pm_getfield(entry->status_lines.vmpte, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMLOCK: /* proc.memory.vmlock */
	if ((f = _pm_getfield(entry->status_lines.vmlck, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMRSS: /* proc.memory.vmrss */
        if ((f = _pm_getfield(entry->status_lines.vmrss, 1)) == NULL)
            atom->ul = 0;
        else
            atom->ul = (__uint32_t)strtoul(f, &tail, 0);
        break;

	case PROC_PID_STATUS_VMDATA: /* proc.memory.vmdata */
	if ((f = _pm_getfield(entry->status_lines.vmdata, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMSTACK: /* proc.memory.vmstack */
	if ((f = _pm_getfield(entry->status_lines.vmstk, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMEXE: /* proc.memory.vmexe */
	if ((f = _pm_getfield(entry->status_lines.vmexe, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMLIB: /* proc.memory.vmlib */
	if ((f = _pm_getfield(entry->status_lines.vmlib, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VMSWAP: /* proc.memory.vmswap */
	if ((f = _pm_getfield(entry->status_lines.vmswap, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_THREADS: /* proc.psinfo.threads */
	if ((f = _pm_getfield(entry->status_lines.threads, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_VCTXSW: /* proc.psinfo.vctxsw */
	if ((f = _pm_getfield(entry->status_lines.vctxsw, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_NVCTXSW: /* proc.psinfo.nvctxsw */
	if ((f = _pm_getfield(entry->status_lines.nvctxsw, 1)) == NULL)
	    atom->ul = 0;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_CPUSALLOWED: /* proc.psinfo.cpusallowed */
	if ((atom->cp = _pm_getfield(entry->status_lines.cpusallowed, 1)) == NULL)
	    return PM_ERR_APPVERSION;
	break;

	case PROC_PID_STATUS_NGID: /* proc.psinfo.ngid */
	if ((f = _pm_getfield(entry->status_lines.ngid, 1)) == NULL)
	    atom->ul = 0;	/* default to NUMA group zero */
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_TGID: /* proc.psinfo.tgid */
	if ((f = _pm_getfield(entry->status_lines.tgid, 1)) == NULL)
	    return PM_ERR_APPVERSION;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_ENVID: /* proc.psinfo.envid */
	if ((f = _pm_getfield(entry->status_lines.envid, 1)) == NULL)
	    return PM_ERR_APPVERSION;
	else
	    atom->ul = (__uint32_t)strtoul(f, &tail, 0);
	break;

	case PROC_PID_STATUS_NSTGID: /* proc.namespaces.tgid */
	if ((atom->cp = entry->status_lines.nstgid) == NULL)
	    return PM_ERR_APPVERSION;
	break;

	case PROC_PID_STATUS_NSPID: /* proc.namespaces.pid */
	if ((atom->cp = entry->status_lines.nspid) == NULL)
	    return PM_ERR_APPVERSION;
	break;

	case PROC_PID_STATUS_NSPGID: /* proc.namespaces.pgid */
	if ((atom->cp = entry->status_lines.nspgid) == NULL)
	    return PM_ERR_APPVERSION;
	break;

	case PROC_PID_STATUS_NSSID: /* proc.namespaces.sid */
	if ((atom->cp = entry->status_lines.nssid) == NULL)
	    return PM_ERR_APPVERSION;
	break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_CGROUP_SUBSYS: {
	subsys_t *ssp;

	indom = INDOM(CGROUP_SUBSYS_INDOM);
	if (idp->item == 1) { /* cgroup.subsys.count */
	    atom->ul = pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE);
	    break;
	}
	if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&ssp)) < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return 0;
	switch (idp->item) {
	case CG_SUBSYS_HIERARCHY: /* cgroup.subsys.hierarchy */
	    atom->ul = ssp->hierarchy;
	    break;
	case CG_SUBSYS_NUMCGROUPS: /* cgroup.subsys.num_cgroups */
	    atom->ul = ssp->num_cgroups;
	    break;
	case CG_SUBSYS_ENABLED: /* cgroup.subsys.enabled */
	    atom->ul = ssp->enabled;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_CGROUP_MOUNTS: {
	filesys_t *fsp;

	indom = INDOM(CGROUP_MOUNTS_INDOM);
	switch (idp->item) {
	case CG_MOUNTS_SUBSYS: /* cgroup.mounts.subsys */
	    if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&fsp)) < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return 0;
	    atom->cp = cgroup_find_subsys(INDOM(CGROUP_SUBSYS_INDOM), fsp);
	    break;
	case CG_MOUNTS_COUNT: /* cgroup.mounts.count */
	    atom->ul = pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE);
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_CPUSET_GROUPS: {
	cgroup_cpuset_t *cpuset;

	indom = INDOM(CGROUP_CPUSET_INDOM);
	if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&cpuset)) < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_CPUSET_CPUS: /* cgroup.cpuset.cpus */
	    atom->cp = proc_strings_lookup(cpuset->cpus);
	    break;
	case CG_CPUSET_MEMS: /* cgroup.cpuset.mems */
	    atom->cp = proc_strings_lookup(cpuset->mems);
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_CPUACCT_GROUPS: {
	cgroup_percpuacct_t *percpuacct;
	cgroup_cpuacct_t *cpuacct;

	if (idp->item == CG_CPUACCT_PERCPU_USAGE) {
	    indom = INDOM(CGROUP_PERCPUACCT_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&percpuacct);
	} else {
	    indom = INDOM(CGROUP_CPUACCT_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&cpuacct);
	}
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_CPUACCT_USER: /* cgroup.cpuacct.stat.user */
	    atom->ull = cpuacct->user;
	    break;
	case CG_CPUACCT_SYSTEM: /* cgroup.cpuacct.stat.system */
	    atom->ull = cpuacct->system;
	    break;
	case CG_CPUACCT_USAGE: /* cgroup.cpuacct.usage */
	    atom->ull = cpuacct->usage;
	    break;
	case CG_CPUACCT_PERCPU_USAGE: /* cgroup.cpuacct.usage_percpu */
	    atom->ull = percpuacct->usage;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_CPUSCHED_GROUPS: {
	cgroup_cpusched_t *cpusched;

	indom = INDOM(CGROUP_CPUSCHED_INDOM);
	if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&cpusched)) < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_CPUSCHED_SHARES: /* cgroup.cpusched.shares */
	    atom->ull = cpusched->shares;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_MEMORY_GROUPS: {
	cgroup_memory_t *memory;

	indom = INDOM(CGROUP_MEMORY_INDOM);
	if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&memory)) < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_MEMORY_STAT_CACHE: /* cgroup.memory.stat.cache */
	    atom->ull = memory->stat.cache;
	    break;
	case CG_MEMORY_STAT_RSS: /* cgroup.memory.stat.rss */
	    atom->ull = memory->stat.rss;
	    break;
	case CG_MEMORY_STAT_RSS_HUGE: /* cgroup.memory.stat.rss_huge */
	    atom->ull = memory->stat.rss_huge;
	    break;
	case CG_MEMORY_STAT_MAPPED_FILE: /* cgroup.memory.stat.mapped_file */
	    atom->ull = memory->stat.mapped_file;
	    break;
	case CG_MEMORY_STAT_WRITEBACK: /* cgroup.memory.stat.writeback */
	    atom->ull = memory->stat.writeback;
	    break;
	case CG_MEMORY_STAT_SWAP: /* cgroup.memory.stat.swap */
	    atom->ull = memory->stat.swap;
	    break;
	case CG_MEMORY_STAT_PGPGIN: /* cgroup.memory.stat.pgpgin */
	    atom->ull = memory->stat.pgpgin;
	    break;
	case CG_MEMORY_STAT_PGPGOUT: /* cgroup.memory.stat.pgpgout */
	    atom->ull = memory->stat.pgpgout;
	    break;
	case CG_MEMORY_STAT_PGFAULT: /* cgroup.memory.stat.pgfault */
	    atom->ull = memory->stat.pgfault;
	    break;
	case CG_MEMORY_STAT_PGMAJFAULT: /* cgroup.memory.stat.pgmajfault */
	    atom->ull = memory->stat.pgmajfault;
	    break;
	case CG_MEMORY_STAT_INACTIVE_ANON: /* cgroup.memory.stat.inactive_anon */
	    atom->ull = memory->stat.inactive_anon;
	    break;
	case CG_MEMORY_STAT_ACTIVE_ANON: /* cgroup.memory.stat.active_anon */
	    atom->ull = memory->stat.active_anon;
	    break;
	case CG_MEMORY_STAT_INACTIVE_FILE: /* cgroup.memory.stat.inactive_file */
	    atom->ull = memory->stat.inactive_file;
	    break;
	case CG_MEMORY_STAT_ACTIVE_FILE: /* cgroup.memory.stat.active_file */
	    atom->ull = memory->stat.active_file;
	    break;
	case CG_MEMORY_STAT_UNEVICTABLE: /* cgroup.memory.stat.unevictable */
	    atom->ull = memory->stat.unevictable;
	    break;
	case CG_MEMORY_STAT_TOTAL_CACHE: /* cgroup.memory.stat.total.cache */
	    atom->ull = memory->total.cache;
	    break;
	case CG_MEMORY_STAT_TOTAL_RSS: /* cgroup.memory.stat.total.rss */
	    atom->ull = memory->total.rss;
	    break;
	case CG_MEMORY_STAT_TOTAL_RSS_HUGE: /* cgroup.memory.stat.total.rss_huge */
	    atom->ull = memory->total.rss_huge;
	    break;
	case CG_MEMORY_STAT_TOTAL_MAPPED_FILE: /* cgroup.memory.stat.total.mapped_file */
	    atom->ull = memory->total.mapped_file;
	    break;
	case CG_MEMORY_STAT_TOTAL_WRITEBACK: /* cgroup.memory.stat.total.writeback */
	    atom->ull = memory->total.writeback;
	    break;
	case CG_MEMORY_STAT_TOTAL_SWAP: /* cgroup.memory.stat.total.swap */
	    atom->ull = memory->total.swap;
	    break;
	case CG_MEMORY_STAT_TOTAL_PGPGIN: /* cgroup.memory.stat.total.pgpgin */
	    atom->ull = memory->total.pgpgin;
	    break;
	case CG_MEMORY_STAT_TOTAL_PGPGOUT: /* cgroup.memory.stat.total.pgpgout */
	    atom->ull = memory->total.pgpgout;
	    break;
	case CG_MEMORY_STAT_TOTAL_PGFAULT: /* cgroup.memory.stat.total.pgfault */
	    atom->ull = memory->total.pgfault;
	    break;
	case CG_MEMORY_STAT_TOTAL_PGMAJFAULT: /* cgroup.memory.stat.total.pgmajfault */
	    atom->ull = memory->total.pgmajfault;
	    break;
	case CG_MEMORY_STAT_TOTAL_INACTIVE_ANON: /* cgroup.memory.stat.total.inactive_anon */
	    atom->ull = memory->total.inactive_anon;
	    break;
	case CG_MEMORY_STAT_TOTAL_ACTIVE_ANON: /* cgroup.memory.stat.total.active_anon */
	    atom->ull = memory->total.active_anon;
	    break;
	case CG_MEMORY_STAT_TOTAL_INACTIVE_FILE: /* cgroup.memory.stat.total.inactive_file */
	    atom->ull = memory->total.inactive_file;
	    break;
	case CG_MEMORY_STAT_TOTAL_ACTIVE_FILE: /* cgroup.memory.stat.total.active_file */
	    atom->ull = memory->total.active_file;
	    break;
	case CG_MEMORY_STAT_TOTAL_UNEVICTABLE: /* cgroup.memory.stat.total.unevictable */
	    atom->ull = memory->total.unevictable;
	    break;
	case CG_MEMORY_STAT_RECENT_ROTATED_ANON: /* cgroup.memory.stat.recent.rotated_anon */
	    atom->ull = memory->recent_rotated_anon;
	    break;
	case CG_MEMORY_STAT_RECENT_ROTATED_FILE: /* cgroup.memory.stat.recent.rotated_file */
	    atom->ull = memory->recent_rotated_file;
	    break;
	case CG_MEMORY_STAT_RECENT_SCANNED_ANON: /* cgroup.memory.stat.recent.scanned_anon */
	    atom->ull = memory->recent_scanned_anon;
	    break;
	case CG_MEMORY_STAT_RECENT_SCANNED_FILE: /* cgroup.memory.stat.recent.scanned_file */
	    atom->ull = memory->recent_scanned_file;
	    break;
	case CG_MEMORY_USAGE_IN_BYTES: /* cgroup.memory.usage */
	    atom->ull = memory->usage;
	    break;
	case CG_MEMORY_LIMIT_IN_BYTES: /* cgroup.memory.limit */
	    atom->ull = memory->limit;
	    break;
	case CG_MEMORY_FAILCNT: /* cgroup.memory.failcnt */
	    atom->ull = memory->failcnt;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_NETCLS_GROUPS: {
	cgroup_netcls_t *netcls;

	indom = INDOM(CGROUP_NETCLS_INDOM);
	if ((sts = pmdaCacheLookup(indom, inst, NULL, (void **)&netcls)) < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_NETCLS_CLASSID: /* cgroup.netclass.classid */
	    atom->ull = netcls->classid;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_BLKIO_GROUPS: {
	cgroup_perdevblkio_t *blkdev = NULL;
	cgroup_blkio_t *blkio = NULL;

	if (mdesc->m_desc.indom == INDOM(CGROUP_PERDEVBLKIO_INDOM)) {
	    indom = INDOM(CGROUP_PERDEVBLKIO_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&blkdev);
	} else {
	    indom = INDOM(CGROUP_BLKIO_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&blkio);
	}
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	   return 0;
	switch (idp->item) {
	case CG_PERDEVBLKIO_IOMERGED_READ: /* cgroup.blkio.dev.io_merged.read */
	    atom->ull = blkdev->stats.io_merged.read;
	    break;
	case CG_PERDEVBLKIO_IOMERGED_WRITE: /* cgroup.blkio.dev.io_merged.write */
	    atom->ull = blkdev->stats.io_merged.write;
	    break;
	case CG_PERDEVBLKIO_IOMERGED_SYNC: /* cgroup.blkio.dev.io_merged.sync */
	    atom->ull = blkdev->stats.io_merged.sync;
	    break;
	case CG_PERDEVBLKIO_IOMERGED_ASYNC: /* cgroup.blkio.dev.io_merged.async */
	    atom->ull = blkdev->stats.io_merged.async;
	    break;
	case CG_PERDEVBLKIO_IOMERGED_TOTAL: /* cgroup.blkio.dev.io_merged.total */
	    atom->ull = blkdev->stats.io_merged.total;
	    break;
	case CG_PERDEVBLKIO_IOQUEUED_READ: /* cgroup.blkio.dev.io_queued.read */
	    atom->ull = blkdev->stats.io_queued.read;
	    break;
	case CG_PERDEVBLKIO_IOQUEUED_WRITE: /* cgroup.blkio.dev.io_queued.write */
	    atom->ull = blkdev->stats.io_queued.write;
	    break;
	case CG_PERDEVBLKIO_IOQUEUED_SYNC: /* cgroup.blkio.dev.io_queued.sync */
	    atom->ull = blkdev->stats.io_queued.sync;
	    break;
	case CG_PERDEVBLKIO_IOQUEUED_ASYNC: /* cgroup.blkio.dev.io_queued.async */
	    atom->ull = blkdev->stats.io_queued.async;
	    break;
	case CG_PERDEVBLKIO_IOQUEUED_TOTAL: /* cgroup.blkio.dev.io_queued.total */
	    atom->ull = blkdev->stats.io_queued.total;
	    break;
	case CG_PERDEVBLKIO_IOSERVICEBYTES_READ: /* cgroup.blkio.dev.io_service_bytes.read */
	    atom->ull = blkdev->stats.io_service_bytes.read;
	    break;
	case CG_PERDEVBLKIO_IOSERVICEBYTES_WRITE: /* cgroup.blkio.dev.io_service_bytes.write */
	    atom->ull = blkdev->stats.io_service_bytes.write;
	    break;
	case CG_PERDEVBLKIO_IOSERVICEBYTES_SYNC: /* cgroup.blkio.dev.io_service_bytes.sync */
	    atom->ull = blkdev->stats.io_service_bytes.sync;
	    break;
	case CG_PERDEVBLKIO_IOSERVICEBYTES_ASYNC: /* cgroup.blkio.dev.io_service_bytes.async */
	    atom->ull = blkdev->stats.io_service_bytes.async;
	    break;
	case CG_PERDEVBLKIO_IOSERVICEBYTES_TOTAL: /* cgroup.blkio.dev.io_service_bytes.total */
	    atom->ull = blkdev->stats.io_service_bytes.total;
	    break;
	case CG_PERDEVBLKIO_IOSERVICED_READ: /* cgroup.blkio.dev.io_serviced.read */
	    atom->ull = blkdev->stats.io_serviced.read;
	    break;
	case CG_PERDEVBLKIO_IOSERVICED_WRITE: /* cgroup.blkio.dev.io_serviced.write */
	    atom->ull = blkdev->stats.io_serviced.write;
	    break;
	case CG_PERDEVBLKIO_IOSERVICED_SYNC: /* cgroup.blkio.dev.io_serviced.sync */
	    atom->ull = blkdev->stats.io_serviced.sync;
	    break;
	case CG_PERDEVBLKIO_IOSERVICED_ASYNC: /* cgroup.blkio.dev.io_serviced.async */
	    atom->ull = blkdev->stats.io_serviced.async;
	    break;
	case CG_PERDEVBLKIO_IOSERVICED_TOTAL: /* cgroup.blkio.dev.io_serviced.total */
	    atom->ull = blkdev->stats.io_serviced.total;
	    break;
	case CG_PERDEVBLKIO_IOSERVICETIME_READ: /* cgroup.blkio.dev.io_service_time.read */
	    atom->ull = blkdev->stats.io_service_time.read;
	    break;
	case CG_PERDEVBLKIO_IOSERVICETIME_WRITE: /* cgroup.blkio.dev.io_service_time.write */
	    atom->ull = blkdev->stats.io_service_time.write;
	    break;
	case CG_PERDEVBLKIO_IOSERVICETIME_SYNC: /* cgroup.blkio.dev.io_service_time.sync */
	    atom->ull = blkdev->stats.io_service_time.sync;
	    break;
	case CG_PERDEVBLKIO_IOSERVICETIME_ASYNC: /* cgroup.blkio.dev.io_service_time.async */
	    atom->ull = blkdev->stats.io_service_time.async;
	    break;
	case CG_PERDEVBLKIO_IOSERVICETIME_TOTAL: /* cgroup.blkio.dev.io_service_time.total */
	    atom->ull = blkdev->stats.io_service_time.total;
	    break;
	case CG_PERDEVBLKIO_IOWAITTIME_READ: /* cgroup.blkio.dev.io_wait_time.read */
	    atom->ull = blkdev->stats.io_wait_time.read;
	    break;
	case CG_PERDEVBLKIO_IOWAITTIME_WRITE: /* cgroup.blkio.dev.io_wait_time.write */
	    atom->ull = blkdev->stats.io_wait_time.write;
	    break;
	case CG_PERDEVBLKIO_IOWAITTIME_SYNC: /* cgroup.blkio.dev.io_wait_time.sync */
	    atom->ull = blkdev->stats.io_wait_time.sync;
	    break;
	case CG_PERDEVBLKIO_IOWAITTIME_ASYNC: /* cgroup.blkio.dev.io_wait_time.async */
	    atom->ull = blkdev->stats.io_wait_time.async;
	    break;
	case CG_PERDEVBLKIO_IOWAITTIME_TOTAL: /* cgroup.blkio.dev.io_wait_time.total */
	    atom->ull = blkdev->stats.io_wait_time.total;
	    break;
	case CG_PERDEVBLKIO_SECTORS: /* cgroup.blkio.dev.sectors */
	    /* sectors are 512 bytes - we export here in kilobytes */
	    atom->ull = blkdev->stats.sectors >> 1;
	    break;
	case CG_PERDEVBLKIO_TIME: /* cgroup.blkio.dev.time */
	    /* unsigned jiffies converted to unsigned milliseconds */
	    atom->ull = (__uint64_t)blkdev->stats.time * 1000 / hz;
	    break;

	case CG_BLKIO_IOMERGED_READ: /* cgroup.blkio.all.io_merged.read */
	    atom->ull = blkio->total.io_merged.read;
	    break;
	case CG_BLKIO_IOMERGED_WRITE: /* cgroup.blkio.all.io_merged.write */
	    atom->ull = blkio->total.io_merged.write;
	    break;
	case CG_BLKIO_IOMERGED_SYNC: /* cgroup.blkio.all.io_merged.sync */
	    atom->ull = blkio->total.io_merged.sync;
	    break;
	case CG_BLKIO_IOMERGED_ASYNC: /* cgroup.blkio.all.io_merged.async */
	    atom->ull = blkio->total.io_merged.async;
	    break;
	case CG_BLKIO_IOMERGED_TOTAL: /* cgroup.blkio.all.io_merged.total */
	    atom->ull = blkio->total.io_merged.total;
	    break;
	case CG_BLKIO_IOQUEUED_READ: /* cgroup.blkio.all.io_queued.read */
	    atom->ull = blkio->total.io_queued.read;
	    break;
	case CG_BLKIO_IOQUEUED_WRITE: /* cgroup.blkio.all.io_queued.write */
	    atom->ull = blkio->total.io_queued.write;
	    break;
	case CG_BLKIO_IOQUEUED_SYNC: /* cgroup.blkio.all.io_queued.sync */
	    atom->ull = blkio->total.io_queued.sync;
	    break;
	case CG_BLKIO_IOQUEUED_ASYNC: /* cgroup.blkio.all.io_queued.async */
	    atom->ull = blkio->total.io_queued.async;
	    break;
	case CG_BLKIO_IOQUEUED_TOTAL: /* cgroup.blkio.all.io_queued.total */
	    atom->ull = blkio->total.io_queued.total;
	    break;
	case CG_BLKIO_IOSERVICEBYTES_READ: /* cgroup.blkio.all.io_service_bytes.read */
	    atom->ull = blkio->total.io_service_bytes.read;
	    break;
	case CG_BLKIO_IOSERVICEBYTES_WRITE: /* cgroup.blkio.all.io_service_bytes.wrie */
	    atom->ull = blkio->total.io_service_bytes.write;
	    break;
	case CG_BLKIO_IOSERVICEBYTES_SYNC: /* cgroup.blkio.all.io_service_bytes.sync */
	    atom->ull = blkio->total.io_service_bytes.sync;
	    break;
	case CG_BLKIO_IOSERVICEBYTES_ASYNC: /* cgroup.blkio.all.io_service_bytes.async */
	    atom->ull = blkio->total.io_service_bytes.async;
	    break;
	case CG_BLKIO_IOSERVICEBYTES_TOTAL: /* cgroup.blkio.all.io_service_bytes.total */
	    atom->ull = blkio->total.io_service_bytes.total;
	    break;
	case CG_BLKIO_IOSERVICED_READ: /* cgroup.blkio.all.io_serviced.read */
	    atom->ull = blkio->total.io_serviced.read;
	    break;
	case CG_BLKIO_IOSERVICED_WRITE: /* cgroup.blkio.all.io_serviced.write */
	    atom->ull = blkio->total.io_serviced.write;
	    break;
	case CG_BLKIO_IOSERVICED_SYNC: /* cgroup.blkio.all.io_serviced.sync */
	    atom->ull = blkio->total.io_serviced.sync;
	    break;
	case CG_BLKIO_IOSERVICED_ASYNC: /* cgroup.blkio.all.io_serviced.async */
	    atom->ull = blkio->total.io_serviced.async;
	    break;
	case CG_BLKIO_IOSERVICED_TOTAL: /* cgroup.blkio.all.io_serviced.total */
	    atom->ull = blkio->total.io_serviced.total;
	    break;
	case CG_BLKIO_IOSERVICETIME_READ: /* cgroup.blkio.all.io_service_time.read */
	    atom->ull = blkio->total.io_service_time.read;
	    break;
	case CG_BLKIO_IOSERVICETIME_WRITE: /* cgroup.blkio.all.io_service_time.write */
	    atom->ull = blkio->total.io_service_time.write;
	    break;
	case CG_BLKIO_IOSERVICETIME_SYNC: /* cgroup.blkio.all.io_service_time.sync */
	    atom->ull = blkio->total.io_service_time.sync;
	    break;
	case CG_BLKIO_IOSERVICETIME_ASYNC: /* cgroup.blkio.all.io_service_time.async */
	    atom->ull = blkio->total.io_service_time.async;
	    break;
	case CG_BLKIO_IOSERVICETIME_TOTAL: /* cgroup.blkio.all.io_service_time.total */
	    atom->ull = blkio->total.io_service_time.total;
	    break;
	case CG_BLKIO_IOWAITTIME_READ: /* cgroup.blkio.all.io_wait_time.read */
	    atom->ull = blkio->total.io_wait_time.read;
	    break;
	case CG_BLKIO_IOWAITTIME_WRITE: /* cgroup.blkio.all.io_wait_time.write */
	    atom->ull = blkio->total.io_wait_time.write;
	    break;
	case CG_BLKIO_IOWAITTIME_SYNC: /* cgroup.blkio.all.io_wait_time.sync */
	    atom->ull = blkio->total.io_wait_time.sync;
	    break;
	case CG_BLKIO_IOWAITTIME_ASYNC: /* cgroup.blkio.all.io_wait_time.async */
	    atom->ull = blkio->total.io_wait_time.async;
	    break;
	case CG_BLKIO_IOWAITTIME_TOTAL: /* cgroup.blkio.all.io_wait_time.total */
	    atom->ull = blkio->total.io_wait_time.total;
	    break;
	case CG_BLKIO_SECTORS: /* cgroup.blkio.all.sectors */
	    /* sectors are 512 bytes - we export here in kilobytes */
	    atom->ull = blkio->total.sectors >> 1;
	    break;
	case CG_BLKIO_TIME: /* cgroup.blkio.all.time */
	    /* unsigned jiffies converted to unsigned milliseconds */
	    atom->ull = (__uint64_t)blkio->total.time * 1000 / hz;
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;
    }

    case CLUSTER_HOTPROC_PID_FD:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_FD:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if (idp->item > PROC_PID_FD_COUNT)
	    return PM_ERR_PMID;
	if ((entry = fetch_proc_pid_fd(inst, active_proc_pid, &sts)) == NULL) /* proc.fd.count */
	    return sts;
	atom->ul = entry->fd_count;
	break;

    case CLUSTER_HOTPROC_PID_CGROUP:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_CGROUP:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if (idp->item > PROC_PID_CGROUP)
	    return PM_ERR_PMID;
	if ((entry = fetch_proc_pid_cgroup(inst, active_proc_pid, &sts)) == NULL) /* proc.psinfo.cgroups */
	    return sts;
	atom->cp = proc_strings_lookup(entry->cgroup_id);
	break;

    case CLUSTER_HOTPROC_PID_LABEL:
	active_proc_pid = &hotproc_pid;
	/*FALLTHROUGH*/
    case CLUSTER_PID_LABEL:
	if (!have_access)
	    return PM_ERR_PERMISSION;
	if (idp->item > PROC_PID_LABEL)
	    return PM_ERR_PMID;
	if ((entry = fetch_proc_pid_label(inst, active_proc_pid, &sts)) == NULL) /* proc.psinfo.labels */
	    return sts;
	atom->cp = proc_strings_lookup(entry->label_id);
	break;

    case CLUSTER_CONTROL:
	switch (idp->item) {
	/* case 1: not reached -- proc.control.all.threads is direct */
	case 2:	/* proc.control.perclient.threads */
	    atom->ul = proc_ctx_threads(pmdaGetContext(), threads);
	    break;
	case 3:	/* proc.control.perclient.cgroups */
	    cp = proc_ctx_cgroups(pmdaGetContext(), cgroups);
	    atom->cp = (char *)(cp ? cp : "");
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return PMDA_FETCH_STATIC;
}

static int
proc_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    __pmID_int	*idp;
    int		i, sts, cluster;
    int		need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	idp = (__pmID_int *)&(pmidlist[i]);
	cluster = idp->cluster;
	if (cluster >= MIN_CLUSTER && cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }

    have_access = all_access || proc_ctx_access(pmda->e_context);
    if ((sts = proc_refresh(pmda, need_refresh)) == 0)
	sts = pmdaFetch(numpmid, pmidlist, resp, pmda);
    have_access = all_access || proc_ctx_revert(pmda->e_context);
    return sts;
}

static int
proc_store(pmResult *result, pmdaExt *pmda)
{
    int i, sts = 0;
    int isroot;

    have_access = all_access || proc_ctx_access(pmda->e_context);
    isroot = (proc_ctx_getuid(pmda->e_context) == 0);

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;

	switch (idp->cluster) {
	case CLUSTER_CONTROL:
	    if (vsp->numval != 1)
		sts = PM_ERR_INST;
	    else switch (idp->item) {
	    case 1: /* proc.control.all.threads */
		if (!have_access)
		    sts = PM_ERR_PERMISSION;
		else if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
		    if (av.ul > 1)	/* only zero or one allowed */
			sts = PM_ERR_BADSTORE;
		    else
			threads = av.ul;
		}
		break;
	    case 2: /* proc.control.perclient.threads */
		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
		    sts = proc_ctx_set_threads(pmda->e_context, av.ul);
		}
		break;
	    case 3:	/* proc.control.perclient.cgroups */
		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0) {
		    if ((sts = proc_ctx_set_cgroups(pmda->e_context, av.cp)) < 0)
			free(av.cp);
		}
		break;
	    default:
		sts = PM_ERR_PERMISSION;
		break;
	    }
	    break;

	case CLUSTER_HOTPROC_GLOBAL:
	    if (!isroot)
		sts = PM_ERR_PERMISSION;
	    else switch (idp->item) {
	    case ITEM_HOTPROC_G_REFRESH: /* hotproc.control.refresh */
		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
		    hotproc_update_interval.tv_sec = av.ul;
		    reset_hotproc_timer();
		}
		break;
	    case ITEM_HOTPROC_G_CONFIG: { /* hotproc.control.config */
		bool_node *tree = NULL;
		char *savebuffer;
                int lsts;

		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0) {
		    savebuffer = get_conf_buffer() ? strdup(get_conf_buffer()) : NULL;
		    set_conf_buffer(av.cp);
                    lsts = parse_config(&tree);
		    if (lsts < 0) {
                        /* Bad config */
			if (savebuffer)
			    set_conf_buffer(savebuffer);
                        sts = PM_ERR_BADSTORE;
		    }
                    else if ( lsts == 0 ){
                        /* Empty Config */
                        disable_hotproc();
                    }
		    else {
			conf_gen++;
			new_tree(tree);
			if (conf_gen == 1) {
			    /* There was no config to start with.
			     * This is the first one, so enable the timer.
			     */
			    reset_hotproc_timer();
			}
		    }
		    if (savebuffer)
			free(savebuffer);
		    free(av.cp);
		}
		break;
	    }
            case ITEM_HOTPROC_G_RELOAD_CONFIG: /* hotproc.control.reload_config */
		if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
				PM_TYPE_U32, &av, PM_TYPE_U32)) >= 0) {
                    hotproc_init();
                    reset_hotproc_timer();
		}
		break;

	    default:
		sts = PM_ERR_PERMISSION;
		break;
	    }
	    break;

	default:
	    sts = PM_ERR_PERMISSION;
	    break;
	}
	if (sts < 0)
	    break;
    }

    have_access = all_access || proc_ctx_revert(pmda->e_context);
    return sts;
}

static int
proc_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
proc_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    if (tree == NULL)
	return PM_ERR_NAME;
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "proc_pmid: name=%s tree:\n", name);
	__pmDumpNameNode(stderr, tree->root, 1);
    }
    return pmdaTreePMID(tree, name, pmid);
}

static int
proc_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupPMID(pmda, pmid);
    if (tree == NULL)
	return PM_ERR_PMID;
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "proc_name: pmid=%s tree:\n", pmIDStr(pmid));
	__pmDumpNameNode(stderr, tree->root, 1);
    }
    return pmdaTreeName(tree, pmid, nameset);
}

static int
proc_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    if (tree == NULL)
	return PM_ERR_NAME;
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "proc_children: name=%s flag=%d tree:\n", name, flag);
	__pmDumpNameNode(stderr, tree->root, 1);
    }
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Helper routines for accessing a generic static string dictionary
 */

char *
proc_strings_lookup(int index)
{
    char *value;
    pmInDom dict = INDOM(STRINGS_INDOM);

    if (pmdaCacheLookup(dict, index, &value, NULL) == PMDA_CACHE_ACTIVE)
	return value;
    return "";
}

int
proc_strings_insert(const char *buf)
{
    pmInDom dict = INDOM(STRINGS_INDOM);
    return pmdaCacheStore(dict, PMDA_CACHE_ADD, buf, NULL);
}

/*
 * Initialise the agent (both daemon and DSO).
 */

void 
__PMDA_INIT_CALL
proc_init(pmdaInterface *dp)
{
    int		nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    int		nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);
    char	*envpath;

    hz = sysconf(_SC_CLK_TCK);
    _pm_system_pagesize = getpagesize();
    if ((envpath = getenv("PROC_STATSPATH")) != NULL)
	proc_statspath = envpath;

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "proc" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "proc DSO", helppath);
    }

    if (dp->status != 0)
	return;
    dp->comm.flags |= (PDU_FLAG_AUTH|PDU_FLAG_CONTAINER);

    dp->version.six.instance = proc_instance;
    dp->version.six.store = proc_store;
    dp->version.six.fetch = proc_fetch;
    dp->version.six.text = proc_text;
    dp->version.six.pmid = proc_pmid;
    dp->version.six.name = proc_name;
    dp->version.six.children = proc_children;
    dp->version.six.attribute = proc_ctx_attrs;
    pmdaSetEndContextCallBack(dp, proc_ctx_end);
    pmdaSetFetchCallBack(dp, proc_fetchCallBack);

    /*
     * Initialize the instance domain table.
     */
    indomtab[DEVT_INDOM].it_indom = DEVT_INDOM;
    indomtab[DISK_INDOM].it_indom = DISK_INDOM;
    indomtab[PROC_INDOM].it_indom = PROC_INDOM;
    indomtab[STRINGS_INDOM].it_indom = STRINGS_INDOM;
    indomtab[CGROUP_CPUSET_INDOM].it_indom = CGROUP_CPUSET_INDOM;
    indomtab[CGROUP_CPUACCT_INDOM].it_indom = CGROUP_CPUACCT_INDOM;
    indomtab[CGROUP_CPUSCHED_INDOM].it_indom = CGROUP_CPUSCHED_INDOM;
    indomtab[CGROUP_PERCPUACCT_INDOM].it_indom = CGROUP_PERCPUACCT_INDOM;
    indomtab[CGROUP_MEMORY_INDOM].it_indom = CGROUP_MEMORY_INDOM;
    indomtab[CGROUP_NETCLS_INDOM].it_indom = CGROUP_NETCLS_INDOM;
    indomtab[CGROUP_BLKIO_INDOM].it_indom = CGROUP_BLKIO_INDOM;
    indomtab[CGROUP_PERDEVBLKIO_INDOM].it_indom = CGROUP_PERDEVBLKIO_INDOM;
    indomtab[CGROUP_SUBSYS_INDOM].it_indom = CGROUP_SUBSYS_INDOM;
    indomtab[CGROUP_MOUNTS_INDOM].it_indom = CGROUP_MOUNTS_INDOM;

    proc_pid.indom = &indomtab[PROC_INDOM];

    indomtab[HOTPROC_INDOM].it_indom = HOTPROC_INDOM;
    hotproc_pid.indom = &indomtab[HOTPROC_INDOM];

    hotproc_init();
    init_hotproc_pid(&hotproc_pid);
 
    /* 
     * Read System.map and /proc/ksyms. Used to translate wait channel
     * addresses to symbol names. 
     * Added by Mike Mason <mmlnx@us.ibm.com>
     */
    read_ksym_sources(kernel_uname.release);

    proc_ctx_init();
    proc_dynamic_init(metrictab, nmetrics);

    rootfd = pmdaRootConnect(NULL);
    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);

    /* cgroup metrics use the pmdaCache API for indom indexing */
    pmdaCacheOp(INDOM(CGROUP_CPUSET_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_CPUACCT_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_CPUSCHED_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_PERCPUACCT_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_MEMORY_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_NETCLS_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_BLKIO_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_PERDEVBLKIO_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_SUBSYS_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(CGROUP_MOUNTS_INDOM), PMDA_CACHE_CULL);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "no-access-checks", 0, 'A', 0, "no access checks will be performed (insecure, beware!)" },
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    { "with-threads", 0, 'L', 0, "include threads in the all-processes instance domain" },
    { "from-cgroup", 1, 'r', "NAME", "restrict monitoring to processes in the named cgroup" },
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "AD:d:l:Lr:U:?",
    .long_options = longopts,
};

int
main(int argc, char **argv)
{
    int			c, sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];
    char		*username = "root";

    _isDSO = 0;
    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "proc" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, PROC, "proc.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch (c) {
	case 'A':
	    all_access = 1;
	    break;
	case 'L':
	    threads = 1;
	    break;
	case 'r':
	    cgroups = opts.optarg;
	    break;
	}
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    __pmSetProcessIdentity(username);

    proc_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
