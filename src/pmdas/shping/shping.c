/*
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 1995-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "shping.h"
#include "domain.h"
#include <sys/stat.h>
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif
#if defined(HAVE_SCHED_H)
#include <sched.h>
#endif
#if defined(HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#define LOG_PRI(p)      ((p) & LOG_PRIMASK)

static int	cycles;		/* completed cycles */
static int	numskip;	/* skipped cycles */
static int	numcmd;		/* number of commands */
static int	timedout;	/* command timed out */
static pid_t	shpid;		/* for /sbin/sh running command */

#if defined(HAVE_PTHREAD_H)
static pthread_t 	sprocpid;
#elif defined(HAVE_SCHED_H)
pid_t		sprocpid;	/* for refresh() */
#else
#error "Need pthreads or sproc"
#endif

/* 
 * only one instance domain here ...
 */
#define SHPING_INDOM 0
pmdaIndom	indomtab = { 0, 0, NULL };

/*
 * all metrics supported in this PMDA - one table entry for each
 */
static pmdaMetric	metrics[] = 
{
/* time.real */
    { NULL,
	{ PMDA_PMID(0,0), PM_TYPE_FLOAT, SHPING_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0), }, },
/* time.cpu_usr */
    { NULL,
	{ PMDA_PMID(0,1), PM_TYPE_FLOAT, SHPING_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0), }, },
/* time.cpu_sys */
    { NULL,
	{ PMDA_PMID(0,2), PM_TYPE_FLOAT, SHPING_INDOM, PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0), }, },
/* control.numcmd */
    { NULL,
	{ PMDA_PMID(0,3),PM_TYPE_U32,PM_INDOM_NULL,PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0), }, },
/* control.cycletime */
    { NULL,
	{ PMDA_PMID(0,4),PM_TYPE_U32,PM_INDOM_NULL,PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0), }, },
/* control.timeout */
    { NULL,
	{ PMDA_PMID(0,5),PM_TYPE_U32,PM_INDOM_NULL,PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0), }, },
/* status */
    { NULL,
	{ PMDA_PMID(0,6),PM_TYPE_32,SHPING_INDOM,PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0), }, },
/* cmd */
    { NULL,
	{ PMDA_PMID(0,7),PM_TYPE_STRING,SHPING_INDOM,PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0), }, },
/* control.debug */
    { NULL,
	{ PMDA_PMID(0,8),PM_TYPE_32,PM_INDOM_NULL,PM_SEM_DISCRETE,
	    PMDA_PMUNITS(0,0,0,0,0,0), }, },
/* control.cycles */
    { NULL,
	{ PMDA_PMID(0,9),PM_TYPE_U32,PM_INDOM_NULL,PM_SEM_COUNTER,
	    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE), }, },
/* error */
    { NULL,
	{ PMDA_PMID(0,10),PM_TYPE_32,SHPING_INDOM,PM_SEM_INSTANT,
	    PMDA_PMUNITS(0,0,0,0,0,0), }, },

};

static int      	nummetric = (sizeof(metrics)/sizeof(metrics[0]));

void
logmessage(int priority, const char *format, ...)
{
    va_list     arglist;
    char        buffer[2048];
    char        *level;
    char        *p;
    time_t      now;

    buffer[0] = '\0';
    time(&now);

    priority = LOG_PRI(priority);
    switch (priority) {
        case LOG_EMERG :
            level = "Emergency";
            break;
        case LOG_ALERT :
            level = "Alert";
            break;
        case LOG_CRIT :
            level = "Critical";
            break;
        case LOG_ERR :
            level = "Error";
            break;
        case LOG_WARNING :
            level = "Warning";
            break;
        case LOG_NOTICE :
            level = "Notice";
            break;
        case LOG_INFO :
            level = "Info";
            break;
        case LOG_DEBUG :
            level = "Debug";
            break;
        default:
            level = "???";
            break;
    }

    va_start (arglist, format);
    vsnprintf (buffer, sizeof(buffer), format, arglist);

    /* strip unwanted '\n' at end of text so's not to double up */
    for (p = buffer; *p; p++);
    if (*(--p) == '\n') *p = '\0';

    fprintf (stderr, "[%.19s] %s(%" FMT_PID ") %s: %s\n", ctime(&now), pmProgname, getpid(), level, buffer) ;
    va_end (arglist) ;
}


static void
onhup(int dummy)
{
    _exit(0);
}

static void
onalarm(int dummy)
{
    timedout = 1;
    if (shpid > 1) {		/* something other than error, self or init */
	kill(-shpid, SIGTERM);	/* nuke process group */
	sleep(1);
	kill(-shpid, SIGKILL);
    }
    signal(SIGALRM, onalarm);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "Timeout!\n");
#endif
}

/*
 * the sproc starts here to refresh the metric values periodically
 */
static void
refresh(void *dummy)
{
    int			i;
    int			sts;
    int			waittime;
    struct timeval	startcycle;
    struct timeval	now;
    struct timeval	then;
    struct rusage	cpu_now;
    struct rusage	cpu_then;
    char		*argv[4];

    signal(SIGHUP, onhup);
#if defined (PR_TERMCHILD)
    prctl(PR_TERMCHILD);          	/* SIGHUP when the parent dies */
#elif defined (PR_SET_PDEATHSIG)
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#elif defined(IS_SOLARIS) || defined(IS_DARWIN) || defined(IS_MINGW) || defined(IS_AIX) || defined(IS_FREEBSD) || defined(IS_NETBSD)
    /* no signals here for child exit */
#else
!bozo: cant decide between PR_TERMCHILD and PR_SET_PDEATHSIG
#endif

    signal(SIGALRM, onalarm);

    putenv("IFS= \t\n");
    putenv("PATH=/usr/sbin:/usr/bsd:/sbin:/usr/bin:/bin");

    argv[0] = "sh";
    argv[1] = "-c";
    argv[2] = "";
    argv[3] = NULL;

    for ( ; ; ) {
	cycles++;
	__pmtimevalNow(&startcycle);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    fprintf(stderr, "\nStart cycle @ %s", ctime(&startcycle.tv_sec));
#endif
	for (i = 0; i < numcmd; i++) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		fprintf(stderr, "[%s] %s ->\n", cmdlist[i].tag, cmdlist[i].cmd);
#endif
	    getrusage(RUSAGE_CHILDREN, &cpu_then);
	    __pmtimevalNow(&then);
	    fflush(stderr);
	    fflush(stdout);
	    shpid = fork();
	    if (shpid == 0) {
		int	j;
		setsid();	/* make new process group */
		for (j = 0; j < NOFILE; j++)
		    close(j);
		open("/dev/null", O_RDONLY, 0);
		sts = -1;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL2) {
		    FILE	*f;
		    if ((f = fopen("shping.out", "a")) != NULL) {
			fprintf(f, "\n[%s] %s\n", cmdlist[i].tag, cmdlist[i].cmd);
			fclose(f);
		    }
		    sts = open("shping.out", O_WRONLY|O_APPEND|O_CREAT, 0644);
		}
#endif
		if (sts == -1)
		    open("/dev/null", O_WRONLY, 0);
		if (dup(1) == -1) {
		    fprintf(stderr, "Warning: dup() failed: %s\n", pmErrStr(-oserror()));
		}
		argv[2] = cmdlist[i].cmd;
		sts = execv("/bin/sh", argv);
		exit(-1);
	    }
	    else if (shpid < 0) {
		logmessage(LOG_CRIT, "refresh: fork() failed: %s", osstrerror());
		cmdlist[i].status = STATUS_SYS;
		cmdlist[i].error = oserror();
		cmdlist[i].real = cmdlist[i].usr = cmdlist[i].sys = -1;
		continue;

	    }
	    timedout = 0;
	    alarm(timeout);
	    waitpid(shpid, &sts, 0);
	    __pmtimevalNow(&now);
	    getrusage(RUSAGE_CHILDREN, &cpu_now);
	    alarm(0);

	    if (timedout) {
		cmdlist[i].error = PM_ERR_TIMEOUT;
		cmdlist[i].status = STATUS_TIMEOUT;
		cmdlist[i].real = cmdlist[i].usr = cmdlist[i].sys = -1;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    logmessage(LOG_INFO, "[%s] timeout\n", cmdlist[i].tag);
#endif
	    }
	    else {
		if (WIFEXITED(sts)) {
		    cmdlist[i].error = WEXITSTATUS(sts);
		    if (cmdlist[i].error == 0)
			cmdlist[i].status = STATUS_OK;
		    else {
			cmdlist[i].status = STATUS_EXIT;
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    logmessage(LOG_INFO, "[%s] exit status: %d\n",
				    cmdlist[i].tag, cmdlist[i].error);
#endif
		    }
		}
		else if (WIFSIGNALED(sts)) {
		    cmdlist[i].error = WTERMSIG(sts);
		    cmdlist[i].status = STATUS_SIG;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			logmessage(LOG_INFO, "[%s] killed signal: %d\n",
				cmdlist[i].tag, cmdlist[i].error);
#endif
		}
		else {
		    /* assume WIFSTOPPED(sts) */
		    cmdlist[i].error = WSTOPSIG(sts);
		    cmdlist[i].status = STATUS_SIG;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL0)
			logmessage(LOG_INFO, "[%s] stopped signal: %d\n",
				cmdlist[i].tag, cmdlist[i].error);
#endif
		}
		cmdlist[i].real = 1000 * (float)(now.tv_sec - then.tv_sec) + 
			    (float)(now.tv_usec - then.tv_usec) / 1000;
		cmdlist[i].usr = 1000 * (float)(cpu_now.ru_utime.tv_sec - cpu_then.ru_utime.tv_sec) +
			    (float)(cpu_now.ru_utime.tv_usec - cpu_then.ru_utime.tv_usec) / 1000;
		cmdlist[i].sys = 1000 * (float)(cpu_now.ru_stime.tv_sec - cpu_then.ru_stime.tv_sec) +
			    (float)(cpu_now.ru_stime.tv_usec - cpu_then.ru_stime.tv_usec) / 1000;
	    }

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1)
		fprintf(stderr, "status: %d error: %d real: %.3f usr: %.3f sys: %.3f\n\n",
		    cmdlist[i].status, cmdlist[i].error,
		    cmdlist[i].real, cmdlist[i].usr, cmdlist[i].sys);
#endif
	}

	/*
	 * harvest delinquent children ... includes those who were fork()'d
	 * above, and timed out.
	 */
	 for ( ; ; ) {
	    sts = waitpid(-1, NULL, WNOHANG);
	    if (sts <= 0)
		break;
	 }

	__pmtimevalNow(&now);
	if (cycletime) {
	    waittime = (int)cycletime - now.tv_sec + startcycle.tv_sec;
	    if (waittime < 0) {
		/* can't keep up */
		while (waittime < 0) {
		    numskip++;
		    waittime += cycletime;
		}
	    }
	    sleep(waittime);
	}
    }
}

static int
shping_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *ext)
{
    int			i;		/* over pmidlist[] */
    int			j;		/* over vset->vlist[] */
    int			sts;
    int			need;
    int			inst;
    int			numval;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;
    pmValueSet		*vset;
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    pmDesc		*dp = NULL;
    int			type;

#ifndef HAVE_SPROC
    /* In the pthread world we don't have asyncronous notification that
     * a thread has died, so we use pthread_kill to check is refresh
     * is still running. */
    int err;

    if ( (err = pthread_kill (sprocpid, 0)) != 0 ) {
	logmessage (LOG_CRIT, "'refresh' thread is not responding: %s\n", 
		    strerror (err));
	exit (1);
    }
#endif


    if (numpmid > maxnpmids) 
    {
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


    for (i = 0; i < numpmid; i++) {

    	pmidp = (__pmID_int*)&pmidlist[i];
	dp = NULL;

	if (ext->e_direct) {
 	    if (pmidp->cluster == 0 && pmidp->item < nummetric)
		dp = &metrics[pmidp->item].m_desc;
	}
	else {
	    for (j = 1; j<nummetric; j++) {
		if (pmidp->cluster == 0 && 
		    metrics[j].m_desc.pmid == pmidlist[i]) {
		    dp = &metrics[j].m_desc;
		    break;
		}
	    }
	}

	if (dp == NULL)
	    numval = PM_ERR_PMID;
	else {
	    if (dp->indom != PM_INDOM_NULL) {
		numval = 0;
		__pmdaStartInst(dp->indom, ext);
		while(__pmdaNextInst(&inst, ext)) {
		    numval++;
		}
	    }
	    else {
		numval = 1;
	    }
	}

	if (numval >= 1)
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) + 
					    (numval - 1)*sizeof(pmValue));
	else
	    res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) -
						       sizeof(pmValue));

	if (vset == NULL) {
	    if (i) {
		res->numpmid = i;
		__pmFreeResultValues(res);
	    }
	    return -oserror();
	}
	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;
	
	if (dp->indom == PM_INDOM_NULL)
	    inst = PM_IN_NULL;
	else {
	    __pmdaStartInst(dp->indom, ext);
	    __pmdaNextInst(&inst, ext);
	}

	type = dp->type;
	pmidp = (__pmID_int *)&pmidlist[i];
	j = 0;

	do {
	    if (j == numval) {
		/* more instances than expected! */
		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			    sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == NULL) {
		    if (i) {
			res->numpmid = i;
			__pmFreeResultValues(res);
		    }
		    return -oserror();
		}
	    }
	    vset->vlist[j].inst = inst;

	    if (pmidp->cluster == 0) {
		switch (pmidp->item) {

		    case 0:	/* shping.time.real PMID: ...0.0 */
			atom.f = cmdlist[inst].real;
			break;

		    case 1:	/* shping.time.cpu_usr PMID: ...0.1 */
			atom.f = cmdlist[inst].usr;
			break;

		    case 2:	/* shping.time.cpu_sys PMID: ...0.2 */
			atom.f = cmdlist[inst].sys;
			break;

		    case 3:	/* shping.control.numcmd PMID: ...0.3 */
			atom.ul = numcmd;
			break;

		    case 4:	/* shping.control.cycletime PMID: ...0.4 */
			atom.ul = cycletime;
			break;

		    case 5:	/* shping.control.timeout PMID: ...0.5 */
			atom.ul = timeout;
			break;

		    case 6:	/* shping.status PMID: ...0.6 */
			atom.l = cmdlist[inst].status;
			break;

		    case 7:	/* shping.cmd PMID: ...0.7 */
			atom.cp = cmdlist[inst].cmd;
			break;

		    case 8:	/* shping.control.debug PMID: ...0.8 */
			atom.l = pmDebug;
			break;

		    case 9:	/* shping.control.cycles PMID: ...0.9 */
			atom.ul = cycles;
			break;

		    case 10:	/* shping.error PMID: ...0.10 */
			atom.ul = cmdlist[inst].error;
			break;

		    default:
			j = PM_ERR_PMID;
			break;
		}
	    }
	    if (j < 0)
		break;

	    sts = __pmStuffValue(&atom, &vset->vlist[j], type);
	    if (sts < 0) {
		__pmFreeResultValues(res);
		return sts;
	    }

	    vset->valfmt = sts;
	    j++;	/* next element in vlist[] for next instance */
	} while (dp->indom != PM_INDOM_NULL && __pmdaNextInst(&inst, ext));

	vset->numval = j;
    }
    *resp = res;
    return 0;
}

static int
shping_store(pmResult *result, pmdaExt *ext)
{
    int		i;
    pmValueSet	*vsp;
    int		sts = 0;
    int		ival;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;
	if (pmidp->cluster == 0) {
	    switch (pmidp->item) {
		case 4:	/* shping.control.cycletime PMID: ...0.4 */
		    ival = vsp->vlist[0].value.lval;
		    if (ival < 0) {
			sts = PM_ERR_SIGN;
			break;
		    }
		    cycletime = ival;
		    break;

		case 5:	/* shping.control.timeout PMID: ...0.5 */
		    ival = vsp->vlist[0].value.lval;
		    if (ival < 0) {
			sts = PM_ERR_SIGN;
			break;
		    }
		    timeout = ival;
		    break;

		case 8:	/* shping.control.debug PMID: ...0.8 */
		    ival = vsp->vlist[0].value.lval;
		    if (ival < 0) {
			sts = PM_ERR_SIGN;
			break;
		    }
		    pmDebug = ival;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else {
	    /* not one of the metrics we are willing to change */
	    sts = PM_ERR_PMID;
	    break;
	}
    }
    return sts;
}

void
shping_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    unlink("shping.out");		/* just in case */

    dp->version.two.fetch = shping_fetch;
    dp->version.two.store = shping_store;

    pmdaInit(dp, &indomtab, 1, metrics, nummetric);

    if (dp->version.two.ext->e_direct == 0) {
	logmessage(LOG_CRIT, "Metric tables require direct mapping.\n");
	dp->status = -1;
	return;
    }

    numcmd = indomtab.it_numinst;

    /* start the sproc for async fetches */
#ifdef HAVE_SPROC
    if ( (sprocpid = sproc(refresh, PR_SADDR, NULL)) < 0 ) {
	logmessage(LOG_CRIT, "sproc failed: %s\n", osstrerror());
	dp->status = sprocpid;
    }
    else {
	dp->status = 0;
        logmessage(LOG_INFO, "Started sproc (spid=%" FMT_PID ")\n", sprocpid);
    }

    /* we're talking to pmcd ... no timeout's for us thanks */
    signal(SIGALRM, SIG_IGN);

#elif defined (HAVE_PTHREAD_H)
    {
	int err = pthread_create(&sprocpid, NULL, (void (*))refresh, NULL);
	if ( err != 0 ) {
	    logmessage (LOG_CRIT, "Cannot spawn a new thread: %s\n", 
			strerror (err));
	    dp->status = err;
	} else {
	    dp->status = 0;
	    logmessage (LOG_INFO, "Started refresh thread\n");
	}
    }
#else
#error "Need pthreads or sproc"
#endif
}
