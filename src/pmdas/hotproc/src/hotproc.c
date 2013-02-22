/*
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>
#include <sys/procfs.h>
#include <sys/immu.h>
#include <sys/sysmacros.h>
#include <pwd.h>
#include <grp.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

/* local stuff */
#include "./cluster.h"
#include "../domain.h"
#include "./proc.h"
#include "./proc_aux.h"
#include "./psinfo.h"
#include "./psusage.h"
#include "./pglobal.h"
#include "./pcpu.h"
#include "./ctltab.h"
#include "./config.h"
#include "./hotproc.h"
#include "./pracinfo.h"
#include "./ppred_values.h"

#define MIN_REFRESH 1
#define INIT_PROC_MAX 200

#if (_MIPS_SZLONG == 32)
#define IO_PMTYPE       PM_TYPE_U32  /* ulong_t from procfs.h/prusage */
#define CTX_PMTYPE      PM_TYPE_U32  /* ulong_t from procfs.h/prusage */
#define SYSCALLS_PMTYPE PM_TYPE_U32  /* ulong_t from procfs.h/prusage */
#define SYSIDLE_PMTYPE  PM_TYPE_32   /* long=time_t from sysinfo.h/sysinfo */
#define TIME_PMTYPE     PM_TYPE_32   /* long from time.h or types.h */
#define ACCUM_PMTYPE    PM_TYPE_U64  /* accum_t from types.h */
#endif /* _MIPS_SZLONG == 32 */

#if (_MIPS_SZLONG == 64)
#define IO_PMTYPE       PM_TYPE_U64  /* ulong_t from procfs.h/prusage */
#define CTX_PMTYPE      PM_TYPE_U64  /* ulong_t from procfs.h/prusage */
#define SYSCALLS_PMTYPE PM_TYPE_U64  /* ulong_t from procfs.h/prusage */
#define SYSIDLE_PMTYPE  PM_TYPE_64   /* long=time_t from sysinfo.h/sysinfo */
#define TIME_PMTYPE     PM_TYPE_64   /* long from time.h or types.h */
#define ACCUM_PMTYPE    PM_TYPE_U64  /* accum_t from types.h */
#endif  /* _MIPS_SZLONG == 64 */

extern char *conf_buffer;
extern char *conf_buffer_ptr;
extern char *pred_buffer;
char	*configfile = NULL;
static int conf_gen = 1; /* configuration file generation number */

static int allow_stores = 1; /* allow stores or not */

static int 	hotproc_domain = 7; /* set in hotproc_init */
static int	pred_testing; /* just do predicate testing or not */
static int	parse_only; /* just parse config file and exit */
static char*	testing_fname; /* filename root for testing */ 
static char	*username; /* user account for pmda */

/* handle on /proc */
static DIR *procdir;

/* active list */
static pid_t *active_list = NULL; /* generated per refresh */
static int numactive = 0;
static int maxactive = INIT_PROC_MAX;

/* process lists for current and previous */
static process_t *proc_list[2] = {NULL, NULL};

/* array size allocated */
static int maxprocs[2] = {INIT_PROC_MAX, INIT_PROC_MAX};

/* number of procs used in list (<= maxprocs) */
static int numprocs[2] = {0, 0};

/* index into proc_list etc.. */
static int current = 0;
static int previous = 1;

/* refresh time interval in seconds - cmd arg */
static struct timeval refresh_delta;

/* event id for refreshing */
static int refresh_afid;

/* various cpu time totals  */
static int num_cpus = 0;
static int have_totals = 0;
static double transient;
static double cpuidle;
static double total_active;
static double total_inactive;

/* number of refreshes */
/* will wrap back to 2 */
static unsigned long refresh_count = 0;

/* format of an entry in /proc */
char proc_fmt[8]; /* export for procfs fname conversions */
int proc_entry_len;

char *log = NULL;

#ifndef USEC_PER_SEC
#define USEC_PER_SEC    1000000     /* number of usecs for 1 second */
#endif
#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC    1000000000  /* number of nsecs for 1 second */
#endif

#define ntime2double(x) \
    (((double)(x).tv_sec) + ((double)((ulong_t)((x).tv_nsec)))/NSEC_PER_SEC)

#define utime2double(x) \
    (((double)(x).tv_sec) + ((double)((ulong_t)((x).tv_usec)))/USEC_PER_SEC)

#define TWOe32 (double) 4.295e9


/*
 * Make initial allocation for proc list.
 */

static int
init_proc_list(void)
{
    active_list = (pid_t*)malloc(INIT_PROC_MAX * sizeof(pid_t));
    proc_list[0] = (process_t*)malloc(INIT_PROC_MAX * sizeof(process_t));
    proc_list[1] = (process_t*)malloc(INIT_PROC_MAX * sizeof(process_t));
    if (proc_list[0] == NULL || proc_list[1] == NULL || active_list == NULL)
        return -oserror();
    return 0;
}

/*
 * Work out the active list from the constraints.
 */


static void
init_active_list(void)
{
    numactive = 0;
}

/*
 * add_active_list:
 * If unsuccessful in add - due to memory then return neg status.
 * If member of active list return 1
 * If non-member of active list return 0
 */

static int
add_active_list(process_t *node, config_vars *vars)
{
    if (eval_tree(vars) == 0) {
        return 0;
    }

    if (numactive == maxactive) {
	pid_t *res;
	maxactive = numactive*2;
	res = (pid_t *)realloc(active_list, maxactive * sizeof(pid_t));
	if (res == NULL)
	    return -osoerror();
	active_list = res;
    } 
    active_list[numactive++] = node->pid;
    return 1;
}

#ifdef PCP_DEBUG

static void
dump_active_list(void)
{
   int i = 0;

   (void)fprintf(stderr, "--- active list ---\n");
   for(i = 0; i < numactive; i++) {
       (void)fprintf(stderr, "[%d] = %" FMT_PID "\n", i, active_list[i]);
   }/*for*/
   (void)fprintf(stderr, "--- end active list ---\n");
}


static void
dump_proc_list(void)
{
   int i;
   process_t *node;

   (void)fprintf(stderr, "--- proc list ---\n");
   for (i = 0; i < numprocs[current]; i++) {
       node = &proc_list[current][i]; 
       (void)fprintf(stderr, "[%d] = %" FMT_PID " ", i, node->pid);
       (void)fprintf(stderr, "(syscalls = %ld) ", node->r_syscalls); 
       (void)fputc('\n', stderr);
   }/*for*/
   (void)fprintf(stderr, "--- end proc list ---\n");
}

static void
dump_cputime(double pre_usr, double pre_sys, double post_usr, double post_sys)
{
  fprintf(stderr, "CPU Time: user = %f, sys = %f\n",
          post_usr - pre_usr, post_sys - pre_sys);
}

static void
dump_pred(derived_pred_t *pred)
{
  (void)fprintf(stderr, "--- pred vars ---\n");
  (void)fprintf(stderr, "syscalls = %f\n", pred->syscalls);
  (void)fprintf(stderr, "ctxswitch = %f\n", pred->ctxswitch);
  (void)fprintf(stderr, "virtualsize = %f\n", pred->virtualsize);
  (void)fprintf(stderr, "residentsize = %f\n", pred->residentsize);
  (void)fprintf(stderr, "iodemand = %f\n", pred->iodemand);
  (void)fprintf(stderr, "iowait = %f\n", pred->iowait);
  (void)fprintf(stderr, "schedwait = %f\n", pred->schedwait);
  (void)fprintf(stderr, "--- end pred vars ---\n");
}

#endif

/*
 * Return 1 if pid is in active list.
 * Return 0 if pid is NOT in active list.
 */

static int
in_active_list(pid_t pid)
{
    int i;

    for(i = 0; i < numactive; i++) {
	if (pid == active_list[i])
	    return 1;
    }

    return 0;
}

static int 
compar_pids(const void *n1, const void *n2)
{
    return ((process_t*)n2)->pid - ((process_t*)n1)->pid;
}


static void
set_proc_fmt(void)
{
    struct dirent *directp;	/* go thru /proc directory */

    for (rewinddir(procdir); directp=readdir(procdir);) {
	if (!isdigit((int)directp->d_name[0]))
	    continue;
	proc_entry_len = (int)strlen(directp->d_name);
	(void)sprintf(proc_fmt, "%%0%dd", proc_entry_len);
        break;
    }
}

/*
 * look up node in proc list
 */
static process_t *
lookup_node(int curr_prev, pid_t pid)
{
    process_t key;
    process_t *node;

    key.pid = pid;

    if ( (numprocs[curr_prev] > 0) &&
	 ((node = bsearch(&key, proc_list[curr_prev], numprocs[curr_prev], 
	   sizeof(process_t), compar_pids)) != NULL) ) {
	return node;
    }
    return NULL;
}

/*
 * look up current node
 */
process_t *
lookup_curr_node(pid_t pid)
{
    return lookup_node(current, pid);
}

/*
 * Calculate difference allowing for single wrapping of counters
 */

static double
DiffCounter(double current, double previous, int pmtype)
{
    double	outval = current-previous;

    if (outval < 0.0) {
	switch (pmtype) {
	    case PM_TYPE_32:
	    case PM_TYPE_U32:
		outval += (double)UINT_MAX+1;
		break;
	    case PM_TYPE_64:
	    case PM_TYPE_U64:
		outval += (double)ULONGLONG_MAX+1;
		break;
	}
    }

    return outval;
}

static void
set_psinfo(prpsinfo_t *psinfo, config_vars *vars)
{
    psinfo->pr_size   = (long)vars->pr_size;
    psinfo->pr_rssize = (long)vars->pr_rssize;
}

static void
set_psusage(prusage_t *psusage, config_vars *vars)
{
    psusage->pu_sysc   = vars->pu_sysc;
    psusage->pu_vctx   = vars->pu_vctx;
    psusage->pu_ictx   = vars->pu_ictx;
    psusage->pu_gbread = vars->pu_gbread;
    psusage->pu_bread  = vars->pu_bread;
    psusage->pu_gbwrit = vars->pu_gbwrit;
    psusage->pu_bwrit  = vars->pu_bwrit;
}

/* If have a test file then read in values
 * and overwrite some variables in the proc buffer.
 * Only read from file for each refresh =>
 *   make each process have same line from file as values
 * If read fails, then return -1
 * else if read something then return 0
 * else if not time to read then return 1
 */

static int
read_buf_file(int *rcount, char *suffix, FILE **testing_file, config_vars *vars)
{
    if (*testing_file == NULL) {
	char fname[80];
	strcpy(fname, testing_fname);
	strcat(fname, suffix);
	*testing_file = fopen(fname, "r"); 
	if (*testing_file == NULL) {
	    fprintf(stderr, "%s: Unable to open test file \"%s\": %s\n",
	    pmProgname, fname, osstrerror());
	    return -1;
	}
    }
    /* only read new values for each refresh */
    if (refresh_count != *rcount) {
	*rcount = (int)refresh_count;
	if (read_test_values(*testing_file, vars) != 1)
	    return -1; 
	return 0;
    }

    return 1; /* no values actually read */
}

static int
psusage_getbuf_file(pid_t pid, prusage_t *psusage)
{
    static config_vars vars;	    /* configuration variable values */
    static FILE *testing_file = NULL;
    static int rcount = -1;          /* last refresh_count */
    static int dont_read = 0;
    int sts;

    if ((sts = psusage_getbuf(pid, psusage)) != 0)
	return sts;

    if (testing_fname != NULL && !dont_read) {
	sts = read_buf_file(&rcount, ".psusage", &testing_file, &vars); 
	if (sts >= 0)
	    set_psusage(psusage, &vars);
	else
	    dont_read = 1;
    }
    return 0;
}

static int
psinfo_getbuf_file(pid_t pid, prpsinfo_t *psinfo)
{
    static config_vars vars;	    /* configuration variable values */
    static FILE *testing_file = NULL;
    static int rcount = -1;          /* last refresh_count */
    static int dont_read = 0;
    int sts;

    if ((sts = psinfo_getbuf(pid, psinfo)) != 0)
	return sts;

    if (testing_fname != NULL && !dont_read) {
	sts = read_buf_file(&rcount, ".psinfo", &testing_file, &vars); 
        if (sts >= 0)
	    set_psinfo(psinfo, &vars);
	else
	    dont_read = 1;
    }
    return 0;
}

static void
set_pracinfo(pracinfo_t *acct, config_vars *vars)
{
    acct->pr_timers.ac_bwtime = vars->ac_bwtime;
    acct->pr_timers.ac_rwtime = vars->ac_rwtime;
    acct->pr_timers.ac_qwtime = vars->ac_qwtime;
}

static int
pracinfo_getbuf_file(pid_t pid, pracinfo_t *acct)
{
    static config_vars vars;	    /* configuration variable values */
    static FILE *testing_file = NULL;
    static int rcount = -1;          /* last refresh_count */
    static int dont_read = 0;
    int sts;

    if ((sts = pracinfo_getbuf(pid, acct)) != 0)
	return sts;

    if (testing_fname != NULL && !dont_read) {
	sts = read_buf_file(&rcount, ".pracinfo", &testing_file, &vars); 
	if (sts >= 0)
	    set_pracinfo(acct, &vars);
	else
	    dont_read = 1;
    }
    return 0;
}

/*
 * Refresh the process list for /proc entries.
 */

static int
refresh_proc_list(void)
{
    extern int need_psusage;        /* is psusage buffer needed or not */
    extern int need_accounting;     /* is pracinfo buffer needed or not */

    int sts;
    int sysmp_sts;
    pid_t pid;
    struct dirent *directp;	    /* go thru /proc directory */
    prpsinfo_t psinfo;		    /* read in proc info */
    prusage_t psusage;              /* read in proc usage */
    pracinfo_t acct;		    /* read in acct info */
    struct sysinfo sinfo;	    /* sysinfo from sysmp */
    process_t *oldnode = NULL;      /* node from previous proc list */
    process_t *newnode = NULL;      /* new node in current proc list */
    int np = 0; 		    /* number of procs index */
    struct timeval p_timestamp;     /* timestamp pre getting proc info */
    config_vars vars;		    /* configuration variable values */

    struct timeval ts;
    static double refresh_time[2];  /* timestamp after refresh */
    static time_t sysidle[2];	    /* sys idle from sysmp */
    double sysidle_delta;           /* system idle delta time since last refresh */
    double actual_delta;            /* actual delta time since last refresh */
    double transient_delta;         /* calculated delta time of transient procs */
    double cputime_delta;	    /* delta cpu time for a process */
    double syscalls_delta;	    /* delta num of syscalls for a process */
    double vctx_delta;	    	    /* delta num of valid ctx switches for a process */
    double ictx_delta;	    	    /* delta num of invalid ctx switches for a process */
    double bread_delta;		    /* delta num of bytes read */
    double gbread_delta;	    /* delta num of gigabytes read */
    double bwrit_delta;		    /* delta num of bytes written */
    double gbwrit_delta;	    /* delta num of gigabytes written */
    double bwtime_delta;            /* delta num of nanosesc for waiting for blocked io */
    double rwtime_delta;            /* delta num of nanosesc for waiting for raw io */
    double qwtime_delta;            /* delta num of nanosesc waiting on run queue */
    double timestamp_delta;         /* real time delta b/w refreshes for process */
    double total_cputime = 0;	    /* total of cputime_deltas for each process */
    double total_activetime = 0;    /* total of cputime_deltas for active processes */
    double total_inactivetime = 0;  /* total of cputime_deltas for inactive processes */

#ifdef PCP_DEBUG
    double curr_time;	/* for current time */
    double pre_usr, pre_sys;
    double post_usr, post_sys;
#endif

    /* switch current and previous */
    if (current == 0) {
        current = 1; previous = 0;
    }
    else {
        current = 0; previous = 1;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	__pmtimevalNow(&ts);
	curr_time = utime2double(ts);
        fprintf(stderr, "refresh_proc_list():\n");
        __pmProcessRunTimes(&pre_usr, &pre_sys); 
    }
#endif

#ifdef PCP_DEBUG
	/* used to verify externally (approx.) the values */
	/* note: doing this is _slow_ */
	if (pmDebug & DBG_TRACE_APPL2) {
	    static char cmd[80];
	    sprintf(cmd, "(date ; ps -le )>> %s.refresh", log);
	    fprintf(stderr, "refresh: cmd = %s\n", cmd);
	    system(cmd);
	}
#endif

    init_active_list();

    (void)memset(&vars, 0, sizeof(config_vars));

    for (np = 0, rewinddir(procdir); directp=readdir(procdir);) {
	if (!isdigit((int)directp->d_name[0]))
	    continue;
	(void)sscanf(directp->d_name, proc_fmt, &pid);

	__pmtimevalNow(&p_timestamp);

        if (psinfo_getbuf_file(pid, &psinfo) != 0)
            continue;

        /* reallocate if run out of room */
	if (np == maxprocs[current]) {
            process_t *res;
	    maxprocs[current] = np*2;
	    res = (process_t *)realloc(proc_list[current], 
                                       maxprocs[current] * sizeof(process_t));
	    if (res == NULL)
		return -oserror();
	    proc_list[current] = res;
	} 

        newnode = &proc_list[current][np++]; 
        newnode->pid = pid;
	newnode->r_cputime      = ntime2double(psinfo.pr_time);
	newnode->r_cputimestamp = utime2double(p_timestamp);

	if (need_psusage) {
	    if (psusage_getbuf_file(pid, &psusage) != 0)
		continue;
	    newnode->r_syscalls = psusage.pu_sysc;
	    newnode->r_vctx = psusage.pu_vctx;
	    newnode->r_ictx = psusage.pu_ictx;
	    newnode->r_bread = psusage.pu_bread;
	    newnode->r_gbread = psusage.pu_gbread;
	    newnode->r_bwrit = psusage.pu_bwrit;
	    newnode->r_gbwrit = psusage.pu_gbwrit;
	}

	if (need_accounting) {
	    if (pracinfo_getbuf_file(pid, &acct) != 0)
		continue;
	    newnode->r_bwtime = acct.pr_timers.ac_bwtime;
	    newnode->r_rwtime = acct.pr_timers.ac_rwtime;
	    newnode->r_qwtime = acct.pr_timers.ac_qwtime;
	}
        
        /* if we have a previous i.e. not the first time */
        if ((oldnode = lookup_node(previous, pid)) != NULL) {


            cputime_delta = DiffCounter(newnode->r_cputime, oldnode->r_cputime, TIME_PMTYPE);
	    timestamp_delta = DiffCounter(newnode->r_cputimestamp, oldnode->r_cputimestamp, TIME_PMTYPE);
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		(void)fprintf(stderr, "refresh_proc_list: "
		    "fname = \"%s\"; cputime = %f; elapsed = %f\n",  
		    psinfo.pr_fname, cputime_delta, timestamp_delta);
            }
#endif
#ifdef PCP_DEBUG
	    /* used to verify externally (approx.) the values */
	    /* note: doing this is _slow_ */
	    if (pmDebug & DBG_TRACE_APPL2) {
		static char cmd[80];
		if (cputime_delta > timestamp_delta) {
		    sprintf(cmd, "(date ; ps -lp %" FMT_PID " )>> %s.refresh", pid, log);
		    fprintf(stderr, "refresh: cmd = %s\n", cmd);
		    system(cmd);
		}
	    }
#endif
	    newnode->r_cpuburn = cputime_delta / timestamp_delta;
	    vars.cpuburn = newnode->r_cpuburn;

	    if (need_psusage) {

		/* rate convert syscalls */
		syscalls_delta = DiffCounter((double)newnode->r_syscalls, 
				    (double)oldnode->r_syscalls, SYSCALLS_PMTYPE);
		vars.preds.syscalls = syscalls_delta / timestamp_delta;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1) {
		    (void)fprintf(stderr, "refresh_proc_list: "
			    "syscalls = %f\n", vars.preds.syscalls);
		}
#endif

		/* rate convert ctxswitch */
		vctx_delta = DiffCounter((double)newnode->r_vctx, 
				    (double)oldnode->r_vctx, CTX_PMTYPE);
		ictx_delta = DiffCounter((double)newnode->r_ictx, 
				    (double)oldnode->r_ictx, CTX_PMTYPE);
		vars.preds.ctxswitch = (vctx_delta + ictx_delta) / timestamp_delta;

		/* rate convert reading/writing (iodemand) */
		bread_delta = DiffCounter((double)newnode->r_bread, 
				    (double)oldnode->r_bread, IO_PMTYPE);
		gbread_delta = DiffCounter((double)newnode->r_gbread, 
				    (double)oldnode->r_gbread, IO_PMTYPE);
		bwrit_delta = DiffCounter((double)newnode->r_bwrit, 
				    (double)oldnode->r_bwrit, IO_PMTYPE);
		gbwrit_delta = DiffCounter((double)newnode->r_gbwrit, 
				    (double)oldnode->r_gbwrit, IO_PMTYPE);
		vars.preds.iodemand = (gbread_delta * 1024 * 1024 + 
                                 (double)bread_delta / 1024 +
				 gbwrit_delta * 1024 * 1024 + 
				 (double)bwrit_delta / 1024) / 
				timestamp_delta;

	    }

	    if (need_accounting) {

		/* rate convert iowait */
		bwtime_delta = DiffCounter((double)newnode->r_bwtime, 
				    (double)oldnode->r_bwtime, ACCUM_PMTYPE);
		bwtime_delta /= 1000000000.0; /* nanosecs -> secs */
		rwtime_delta = DiffCounter((double)newnode->r_rwtime, 
				    (double)oldnode->r_rwtime, ACCUM_PMTYPE);
		rwtime_delta /= 1000000000.0; /* nanosecs -> secs */
		vars.preds.iowait = (bwtime_delta + rwtime_delta) / timestamp_delta;


		/* rate convert schedwait */
		qwtime_delta = DiffCounter((double)newnode->r_qwtime, 
				    (double)oldnode->r_qwtime, ACCUM_PMTYPE);
		qwtime_delta /= 1000000000.0; /* nanosecs -> secs */
		vars.preds.schedwait = qwtime_delta / timestamp_delta;

	    }

	    newnode->preds = vars.preds; /* struct copy */

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		dump_pred(&newnode->preds);
	    }
#endif

        }
	else {
	    newnode->r_cpuburn = 0;
	    bzero(&newnode->preds, sizeof(newnode->preds));
	    vars.cpuburn = 0;
	    vars.preds.syscalls = 0;
	    vars.preds.ctxswitch = 0;
	    vars.preds.iowait = 0;
	    vars.preds.schedwait = 0;
	    vars.preds.iodemand = 0;
	    cputime_delta = 0;
	}

	total_cputime += cputime_delta;

	/* fix up vars record from psinfo */
	(void)strcpy(vars.fname, psinfo.pr_fname);
	(void)strcpy(vars.psargs, psinfo.pr_psargs);
	vars.uid = psinfo.pr_uid;
	vars.gid = psinfo.pr_gid;
	vars.uname = NULL;
	vars.gname = NULL;
	vars.preds.virtualsize = (double)psinfo.pr_size * (_pagesize / 1024);
	vars.preds.residentsize = (double)psinfo.pr_rssize * (_pagesize / 1024);

        if ((sts = add_active_list(newnode, &vars)) < 0) {
	    return sts;
        }

	if (sts == 0)
	    total_inactivetime += cputime_delta;
	else
	    total_activetime += cputime_delta;

    }/*for each pid*/

    numprocs[current] = np;

    __pmtimevalNow(&ts);
    refresh_time[current] = utime2double(ts);

    refresh_count++;
    /* If we wrap then say that we atleast have 2, 
     * inorder to indicate we have seen two successive refreshes.
     */
    if (refresh_count == 0) 
	refresh_count = 2;

    bzero(&sinfo, sizeof(sinfo)); /* for purify */
    if ((sysmp_sts = (int)sysmp(MP_SAGET, MPSA_SINFO, &sinfo, sizeof(struct sysinfo))) < 0) {
   	__pmNotifyErr(LOG_ERR, "sysmp failed in refresh: %s\n", osstrerror());
        sysidle[current] = -1;
    }
    else {
        sysidle[current] = sinfo.cpu[CPU_IDLE];
    }


    have_totals = 0;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	(void)fprintf(stderr, "refresh_proc_list: refresh_count = %lu\n", 
			refresh_count);
    }
#endif

    if (refresh_count > 1 && sysmp_sts != -1 && sysidle[previous] != -1) {
	sysidle_delta = DiffCounter(sysidle[current], sysidle[previous], SYSIDLE_PMTYPE) / (double)HZ; 
	actual_delta = DiffCounter(refresh_time[current], refresh_time[previous], TIME_PMTYPE);
	transient_delta = num_cpus * actual_delta - (total_cputime + sysidle_delta);
	if (transient_delta < 0) /* sysidle is only accurate to 0.01 second */
	    transient_delta = 0;

	have_totals = 1;
	transient = transient_delta / actual_delta;
        cpuidle = sysidle_delta / actual_delta;
	total_active = total_activetime / actual_delta;
	total_inactive = total_inactivetime / actual_delta;

#ifdef PCP_DEBUG
        if (pmDebug & DBG_TRACE_APPL1) {
	    (void)fprintf(stderr, "refresh_proc_list: "
		"total_cputime = %f\n", total_cputime);
	    (void)fprintf(stderr, "refresh_proc_list: "
		"total_activetime = %f, total_inactivetime = %f\n",
		total_activetime, total_inactivetime);
	    (void)fprintf(stderr, "refresh_proc_list: "
		"sysidle_delta = %f, actual_delta = %f\n",
		sysidle_delta, actual_delta);	
	    (void)fprintf(stderr, "refresh_proc_list: "
		"transient_delta = %f, transient = %f\n",
		transient_delta, transient);	
        }
#endif
    }


    /* sort it for bsearching later */
    qsort(proc_list[current], numprocs[current], 
          sizeof(process_t), compar_pids);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        __pmProcessRunTimes(&post_usr, &post_sys); 
        dump_proc_list();
        fprintf(stderr, "refresh_proc_list: duration = %f secs; ", 
                refresh_time[current] - curr_time);
        dump_cputime(pre_usr, pre_sys, post_usr, post_sys);
        dump_active_list();
    }
#endif

    return 0;
}/*refresh_proc_list*/


static int
restart_proc_list(void)
{
    int sts;

    refresh_count = 0;
    numprocs[current] = 0; /* clear the current list */
    sts = refresh_proc_list();

    return sts;
}

static void
timer_callback(int afid, void *data)
{
    int sts = refresh_proc_list();

    if (sts < 0) {
   	__pmNotifyErr(LOG_ERR, "timer_callback: refresh list failed: %s\n", pmErrStr(sts));
    }
}


static int
hotproc_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInResult		*res;
    int			j;
    int			sts;


    if (indom != proc_indom)
	return PM_ERR_INDOM;

    if ((res = (__pmInResult *)malloc(sizeof(*res))) == NULL)
	return -oserror();
    res->indom = indom;
    res->instlist = NULL;
    res->namelist = NULL;
    sts = 0;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = numactive;
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
    }

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
	for (j = 0; j < res->numinst; j++)
	    res->namelist[j] = NULL;
    }

    /* --> names and ids (the whole instance map) */
    if (name == NULL && inst == PM_IN_NULL) {
	/* find all instance ids and names for the PROC indom */

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    (void)fprintf(stderr, "hotproc_instance: Getting whole instance map...\n");
	}
#endif

        /* go thru active list */
	for(j = 0; j < numactive; j++) {
            res->instlist[j] = active_list[j]; 
	    if ((sts = proc_id_to_mapname(active_list[j], &res->namelist[j])) < 0)
		   break; 
	}
    }
    /* id --> name */
    else if (name == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    (void)fprintf(stderr, "hotproc_instance: id --> name\n");
	}
#endif
        if (!in_active_list(inst)) {
	    __pmNotifyErr(LOG_ERR, "proc_instance: pid not in active list %d\n", inst);
	    sts = PM_ERR_INST;
	}
        else {
            sts = proc_id_to_name(inst, &res->namelist[0]);
        }
    }
    /* name --> id */
    else {
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0) {
		    (void)fprintf(stderr, "hotproc_instance: name --> id\n");
		}
#endif
	/* find the instance for the given indom/name */
	sts = proc_name_to_id(name, &res->instlist[0]);
	if (!in_active_list(res->instlist[0])) {
	    __pmNotifyErr(LOG_ERR, "proc_instance: pid not in active list %d\n", 
                         res->instlist[0]);
	    sts = PM_ERR_INST;
        }
    }

    if (sts == 0)
	*result = res;
    else
	__pmFreeInResult(res);

    return sts;
}

static int
hotproc_desc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    int pmidErr;
    int ctl_i;
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    pmidErr = PM_ERR_PMID;
    if (pmidp->domain == hotproc_domain) {
	ctl_i = lookup_ctltab(pmidp->cluster);
	if (ctl_i < 0)
	    pmidErr = ctl_i;
	else
	    pmidErr = ctltab[ctl_i].getdesc(pmid, desc);
    }

    return pmidErr;

}


static int
hotproc_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int			i;		/* over pmidlist[] */
    int			ctl_i;		/* over ctltab[] and fetched[] */
    int			j;
    int			n;
    int			numvals;
    int			sts;
    size_t		need;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    pmValueSet		*vset;
    pmDesc		dp;
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    int			aggregate_len;
    static int 		max_numactive = 0;
    static int 		**fetched = NULL;

    if (fetched == NULL) {
	fetched = (int **)malloc(nctltab * sizeof(fetched[0]));
	if (fetched == NULL)
	    return -oserror();
        memset(fetched, 0, nctltab * sizeof(fetched[0]));
    }

    /* allocate for result structure */
    if (numpmid > maxnpmids) {
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == NULL)
	    return -oserror();
	maxnpmids = numpmid;
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;


    /* fix up allocations for fetched array */
    for (ctl_i=1; ctl_i < nctltab; ctl_i++) {
        if (numactive > max_numactive) {
	    int *f = (int*)realloc(fetched[ctl_i], numactive * sizeof(int));
	    int	ctl_j;		/* over ctltab[] and fetched[] */
	    if (f == NULL) {
		max_numactive = 0;
    		for (ctl_j=1; ctl_j < nctltab; ctl_j++) {
                    if (fetched[ctl_j])
		        free(fetched[ctl_j]);
		    fetched[ctl_j] = NULL;
		}
		return -oserror();
            }
	    fetched[ctl_i] = f;
        }
        (void)memset(fetched[ctl_i], 0, numactive * sizeof(int));
	if ((sts = ctltab[ctl_i].allocbuf(numactive)) < 0)
	    return sts;
    }/*for*/
    if (numactive > max_numactive) {
    	max_numactive = numactive;
    }

    sts = 0;
    for (i = 0; i < numpmid; i++) {
	int	pmidErr = 0;

	pmidp = (__pmID_int *)&pmidlist[i];
	pmidErr = hotproc_desc(pmidlist[i], &dp, pmda);

        /* create a vset with the error code in it */
	if (pmidErr < 0) {
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet));
	    if (vset == NULL) {
		if (i) {
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -oserror();
	    }
	    vset->pmid = pmidlist[i];
	    vset->numval = pmidErr;
	    continue;
	}

	if (pmidp->cluster == CLUSTER_GLOBAL) {

	    /* global metrics, singular instance domain */
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet));
	    if (vset == NULL) {
		if (i > 0) {
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -oserror();
	    }
	    vset->pmid = pmidlist[i];
	    vset->numval = 1;

	    switch (pmidp->item) {
	    case ITEM_NPROCS:
                {
		    char *path;
		
		    /* pv#589180
		     * Reduce the active list if processes have exitted.
		     */
		    for (j=0; j < numactive; j++) {
			pid_t pid = active_list[j];
			proc_pid_to_path(pid, NULL, &path, PINFO_PATH);
			/* if process not found then remove from active list */
			if (access(path, R_OK) < 0) {
			    int ctl_k;
			    /* remove from active list */
			    /* replace with end one */
			    active_list[j] = active_list[numactive-1]; 
			    /* also do same thing to fetched array */
			    for(ctl_k = 1; ctl_k < nctltab; ctl_k++) {
				fetched[ctl_k][j] = fetched[ctl_k][numactive-1];
			    }
			    numactive--;
			    j--; /* test this slot again */
			}
		    }

		    atom.l = numactive;
		    break;
		}
	    case ITEM_REFRESH:
		atom.l = (__int32_t)refresh_delta.tv_sec;
		break;
	    case ITEM_CONFIG:
		atom.cp = pred_buffer;
		break;
	    case ITEM_CONFIG_GEN:
		atom.l = conf_gen;
		break;
	    case ITEM_TRANSIENT:
		if (!have_totals)
	    	    vset->numval = 0;
		else
		    atom.f = transient;
		break;
	    case ITEM_CPUIDLE:
		if (!have_totals)
	    	    vset->numval = 0;
		else
		    atom.f = cpuidle;
		break;
	    case ITEM_TOTAL_CPUBURN:
		if (!have_totals)
	    	    vset->numval = 0;
		else
		    atom.f = total_active;
		break;
	    case ITEM_TOTAL_NOTCPUBURN:
		if (!have_totals)
	    	    vset->numval = 0;
		else
		    atom.f = total_inactive;
		break;
	    case ITEM_OTHER_TOTAL:
		if (!have_totals)
	    	    vset->numval = 0;
		else
		    atom.f = transient + total_inactive;
		break;
	    case ITEM_OTHER_PERCENT:
		{
		    double other = transient + total_inactive;
		    double non_idle = other + total_active;

		    /* if non_idle = 0, very unlikely,
		     * then the value here is meaningless
		     */
		    if (!have_totals || non_idle == 0)
			vset->numval = 0;
		    else 
			atom.f = other / non_idle * 100; 
		}
		break;
	    }

	    if (vset->numval == 1 ) {
		sts = __pmStuffValue(&atom, 0, &vset->vlist[0], dp.type);
		vset->valfmt = sts;
		vset->vlist[0].inst = PM_IN_NULL;
	    }
	}
	else {
	    /*
	     * Multiple instance domain metrics.
	     */
	    if ((ctl_i = lookup_ctltab(pmidp->cluster)) < 0)
		return ctl_i;
	    if (!ctltab[ctl_i].supported) {
		numvals = 0;
	    }
	    else if (pmidp->cluster == CLUSTER_PRED && !ppred_available(pmidp->item)) {
		numvals = 0;
	    }
	    else {
		pid_t pid;

		numvals = 0;
		for (j = 0; j < numactive; j++) {
		    pid = active_list[j];
		    if (!__pmInProfile(proc_indom, pmda->e_prof, pid))
		        continue;
		    if (fetched[ctl_i][j] == 0) {
			int sts;
			if ((sts = ctltab[ctl_i].getinfo(pid, j)) == 0) {
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_APPL0) {
				fprintf(stderr, "hotproc_fetch: getinfo succeeded: "
					"pid=%" FMT_PID ", j=%d\n", pid, j);
			    }
#endif
			    fetched[ctl_i][j] = 1;
			    numvals++;
			}
			else if (sts == -ENOENT) {
			    int ctl_k;
#ifdef PCP_DEBUG
			    if (pmDebug & DBG_TRACE_APPL0) {
				fprintf(stderr, "hotproc_fetch: "
					"getinfo failed: pid=%" FMT_PID ", j=%d\n", pid, j);
			    }
#endif
			    /* remove from active list */
			    /* replace with end one */
			    active_list[j] = active_list[numactive-1]; 
			    /* also do same thing to fetched array */
			    for(ctl_k = 1; ctl_k < nctltab; ctl_k++) {
				fetched[ctl_k][j] = fetched[ctl_k][numactive-1];
			    }
			    numactive--;
			    j--; /* test this slot again */
			}
		    }
		    else {
			numvals++;
		    }
		}
	    }
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		fprintf(stderr, "hotproc_fetch: numvals = %d\n", numvals);
	    }
#endif

	    res->vset[i] = vset = (pmValueSet *)malloc(
		sizeof(pmValueSet) + (numvals-1) * sizeof(pmValue));
	    if (vset == NULL) {
		if (i > 0) { /* not the first malloc failing */
		    res->numpmid = i;
		    __pmFreeResultValues(res);
		}
		return -oserror();
	    }
	    vset->pmid = pmidlist[i];
	    vset->numval = numvals;
	    vset->valfmt = PM_VAL_INSITU;

	    if (!ctltab[ctl_i].supported) {
		vset->numval = PM_ERR_APPVERSION;
            }
	    else {
		if (numvals != 0) {
		    pid_t pid;
		    for (n=j=0; j < numactive; j++) {
		        pid = active_list[j];
			if (fetched[ctl_i][j] == 0)
			    continue;

			aggregate_len = ctltab[ctl_i].setatom(pmidp->item, &atom, j);
			sts = __pmStuffValue(&atom, aggregate_len, &vset->vlist[n], dp.type);
			vset->valfmt = sts;
			vset->vlist[n].inst = pid;
			n++;
		    }/*for*/
		}/*if*/
	    }/*if*/

	}/*if*/
    }/*for*/

    *resp = res;
    return 0;
}/*hotproc_fetch*/

static int
restart_refresh(void)
{
    int sts;

    /* 
     * Reschedule as if starting from init.
     */
    sts = restart_proc_list();
    if (sts >= 0) {
	/* Get rid of current event from queue, register new one */ 
	__pmAFunregister(refresh_afid);
	refresh_afid = __pmAFregister(&refresh_delta, NULL, timer_callback);
    }

    return sts;
}

static int
hotproc_store(pmResult *result, pmdaExt *pmda)
{
    int i;
    pmValueSet  *vsp;
    pmDesc      desc;
    __pmID_int   *pmidp;
    int         sts = 0;
    pmAtomValue av;

    if (!allow_stores) {
	return PM_ERR_PERMISSION;
    }

    for (i = 0; i < result->numpmid; i++) {
        vsp = result->vset[i];
        pmidp = (__pmID_int *)&vsp->pmid;
	sts = hotproc_desc(vsp->pmid, &desc, pmda);
	if (sts < 0)
	    break;
	if (pmidp->cluster == CLUSTER_GLOBAL) {
	    switch (pmidp->item) {
		case ITEM_REFRESH:
		    if (vsp->numval != 1 || vsp->valfmt != PM_VAL_INSITU) {
			sts = PM_ERR_CONV;
		    }
		    break;
		case ITEM_CONFIG:
		    if (vsp->numval != 1 || vsp->valfmt == PM_VAL_INSITU) {
			sts = PM_ERR_CONV;
		    }
		    break;
		default:
		    sts = PM_ERR_PERMISSION;
		    break;
	    }
	}
	else {
	    sts = PM_ERR_PERMISSION;
	}

	if (sts < 0)
	    break;

	if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0], 
                                  desc.type, &av, desc.type)) < 0)
            break;

	switch (pmidp->item) {
	    case ITEM_REFRESH:
		if (av.l < MIN_REFRESH) {
		    sts = PM_ERR_CONV;
		}
		else {
		    refresh_delta.tv_sec = av.l;

		    if ((sts = restart_refresh()) < 0)
			__pmNotifyErr(LOG_ERR, "hotproc_store: refresh list failed: %s\n",
			 pmErrStr(sts));
		}
		break;
	    case ITEM_CONFIG:
		{
		    bool_node *tree;

		    conf_buffer_ptr = av.cp;

		    /* Only accept the new config if its predicate is parsed ok */
		    if (parse_config(&tree) != 0) {
			conf_buffer_ptr = conf_buffer;
			free(av.cp);
			sts = PM_ERR_CONV;
		    }
		    else {
			conf_gen++;
			new_tree(tree);
			free(conf_buffer); /* free old one */
			conf_buffer = av.cp; /* use the new one */
			if ((sts = restart_refresh()) < 0)
			    __pmNotifyErr(LOG_ERR, "hotproc_store: refresh list failed: %s\n",
			     pmErrStr(sts));
		    }
		}
		break;
	}/*switch*/

    }/*for*/

    return sts;
}



/*
 * Initialise the agent (only daemon and NOT DSO).
 */

static void
hotproc_init(pmdaInterface *dp)
{
    int sts;

    __pmSetProcessIdentity(username);

    dp->version.two.fetch    = hotproc_fetch;
    dp->version.two.store    = hotproc_store;
    dp->version.two.desc     = hotproc_desc;
    dp->version.two.instance = hotproc_instance;

    /* 
     * Maintaining my own desc and indom table/info.
     * This is why the passed in tables are NULL.
     */
    pmdaInit(dp, NULL, 0, NULL, 0);
    init_tables(dp->domain);
    hotproc_domain = dp->domain;

    if ((procdir = opendir(PROCFS)) == NULL) {
	dp->status = -oserror();
	__pmNotifyErr(LOG_ERR, "opendir(%s) failed: %s\n", PROCFS, osstrerror());
        return;
    }

    set_proc_fmt();

    
    if ((num_cpus = (int)sysmp(MP_NPROCS)) < 0) {
	dp->status = -oserror();
   	__pmNotifyErr(LOG_ERR, "sysmp failed to get NPROCS: %s\n", osstrerror());
        return;
    }

    if ((sts = init_proc_list()) < 0) {
        dp->status = sts;
	return;
    }

    if ((sts = restart_proc_list()) < 0) {
        dp->status = sts;
	return;
    }
}



static void
usage(void)
{
    (void)fprintf(stderr, "Usage: %s [options] configfile\n\n", pmProgname);
    (void)fputs("Options:\n"
	  "  -C           parse configfile and exit\n"
	  "  -U username  user account to run under (default \"pcp\")\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -s           do NOT allow dynamic changes to the selection predicate\n"
	  "  -t refresh   set the refresh time interval in seconds\n",
	  stderr);		
    exit(1);
}


static void
get_options(int argc, char *argv[], pmdaInterface *dispatch)
{
    int n;
    int err = 0;
    char *err_msg;


    while ((n = pmdaGetOpt(argc, argv, "CD:d:l:t:U:xX:?s",
			dispatch, &err)) != EOF) {
	switch (n) {

	    case 'C':
		parse_only = 1;
		break;	

	    case 's':
		allow_stores = 0;
		break;	

	    case 't':
		if (pmParseInterval(optarg, &refresh_delta, &err_msg) < 0) {
		    (void)fprintf(stderr, 
		    	    "%s: -t requires a time interval: %s\n",
			    err_msg, pmProgname);
		    free(err_msg);
		    err++;
		}
		break;

	    case 'U':
		username = optarg;
		break;

	    case 'x':
		/* hidden option for predicate testing */
		pred_testing = 1;
		break;

	    case 'X':
		/* hidden option for predicate testing with iocntl buffers */
		testing_fname = optarg;
		break;

	    case '?':
		err++;
	}
    }

    if (err || optind != argc -1) {
    	usage();
    }

    configfile = argv[optind];
}



/*
 * Set up the agent for running as a daemon.
 */

int
main(int argc, char **argv)
{
    pmdaInterface	dispatch;
    char		*p;
    int			sep = __pmPathSeparator();
    int			infd;
    fd_set		fds;
    fd_set 		readyfds;        
    int			nready;
    int			numfds;
    FILE		*conf;
    char		mypath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    refresh_delta.tv_sec = 10;
    refresh_delta.tv_usec = 0;

    snprintf(mypath, sizeof(mypath), "%s%c" "hotproc" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_2, pmProgname, HOTPROC,
		"hotproc.log", mypath);

    get_options(argc, argv, &dispatch);

    if (pred_testing) {
        do_pred_testing();
	exit(0);
    }

    if (!parse_only) {
	pmdaOpenLog(&dispatch);
	log = dispatch.version.two.ext->e_logfile;
    }

    conf = open_config();
    read_config(conf);
    (void)fclose(conf);

    if (parse_only)
	exit(0);

    hotproc_init(&dispatch);
    pmdaConnect(&dispatch);

    if ((infd = __pmdaInFd(&dispatch)) < 0)
	exit(1);
    FD_ZERO(&fds);
    FD_SET(infd, &fds);
    numfds = infd+1;

    refresh_afid = __pmAFregister(&refresh_delta, NULL, timer_callback);

    /* custom pmda main loop */
    for (;;) {
	(void)memcpy(&readyfds, &fds, sizeof(readyfds));
        nready = select(numfds, &readyfds, NULL, NULL, NULL);

        if (nready > 0) {
	    __pmAFblock();
	    if (__pmdaMainPDU(&dispatch) < 0)
		break;
	    __pmAFunblock();
	}
	else if (nready < 0 && neterror() != EINTR) {
	    __pmNotifyErr(LOG_ERR, "select failed: %s\n", netstrerror());
	}
    }

    exit(0);
}
