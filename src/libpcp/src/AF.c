/*
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2009 Aconex.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

/*
 * general purpose asynchronous event management
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

#define MIN_ITIMER_USEC 100

typedef struct _qelt {
    struct _qelt	*q_next;
    int			q_afid;
    struct timeval	q_when;
    struct timeval	q_delta;
    void		*q_data;
    void		(*q_func)(int afid, void *data);
} qelt;

static qelt		*root;
static qelt		*gc;		/* garbage collection from onalarm() */
static int		afid = 0x8000;
static int		block;
static void		onalarm(int);

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	AF_lock;
#else
void			*AF_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == AF_lock
 */
int
__pmIsAFLock(void *lock)
{
    return lock == (void *)&AF_lock;
}
#endif

void
init_AF_lock(void)
{
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&AF_lock);
#endif
}

/*
 * Platform dependent routines follow, Windows is very different
 * to POSIX platforms in terms of signals and timers.  Note - we
 * attempted to use CreateTimerQueue API on Windows, but it does
 * not behave in the way we'd like unfortunately (QA slow_af.c -
 * shows quite different results & its un-debuggable cos its all
 * below the Win32 API).
 */
#ifdef IS_MINGW
VOID CALLBACK ontimer(LPVOID arg, DWORD lo, DWORD hi)
{
    onalarm(14);	/* 14 == POSIX SIGALRM */
}

static HANDLE	afblock;	/* mutex protecting callback */
static HANDLE	aftimer;	/* equivalent to ITIMER_REAL */
static int	afsetup;	/* one-time-setup: done flag */

static void AFinit(void)
{
    PM_INIT_LOCKS();
    PM_LOCK(AF_lock);
    if (afsetup) {
	PM_UNLOCK(AF_lock);
	return;
    }
    afsetup = 1;
    afblock = CreateMutex(NULL, FALSE, NULL);
    aftimer = CreateWaitableTimer(NULL, TRUE, NULL);
    PM_UNLOCK(AF_lock);
}
static void AFhold(void)
{ 
    AFinit();
    WaitForSingleObject(afblock, INFINITE);
}
static void AFrelse(void)
{
    if (afsetup)
	ReleaseMutex(afblock);
}
static void AFrearm(void)
{
    /* do nothing, callback is always "armed" (except when not setup) */
}

static void AFsetitimer(struct timeval *interval)
{
    LARGE_INTEGER duetime;
    long long inc;

    AFinit();

    inc = interval->tv_sec * 10000000ULL;	/* sec -> 100 nsecs */
    inc += (interval->tv_usec * 10ULL);		/* used -> 100 nsecs */
    if (inc > 0)	/* negative is relative, positive absolute */
	inc = -inc;	/* we will always want this to be relative */
    duetime.QuadPart = inc;
    SetWaitableTimer(aftimer, &duetime, 0, ontimer, NULL, FALSE);
}

#else /* POSIX */
static void
AFsetitimer(struct timeval *interval)
{
    struct itimerval val;

    val.it_value = *interval;
    val.it_interval.tv_sec = val.it_interval.tv_usec = 0;
    setitimer(ITIMER_REAL, &val, NULL);
}

static void
AFhold(void)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGALRM);
    sigprocmask(SIG_BLOCK, &s, NULL);
}

static void
AFrelse(void)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, SIGALRM);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
}

static void
AFrearm(void)
{
    struct sigaction action;

    action.sa_handler = onalarm;
    sigemptyset (&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGALRM, &action, NULL);
}
#endif	/* POSIX */


/*
 * Platform independent code follows
 */

static void
printdelta(FILE *f, struct timeval *tp)
{
    struct tm	*tmp;
    struct tm	gmtbuf;
    time_t	tt =  (time_t)tp->tv_sec;

    tmp = gmtime_r(&tt, &gmtbuf);
    fprintf(stderr, "%02d:%02d:%02d.%06ld", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (long)tp->tv_usec);
}

/*
 * a := a - b for struct timevals, but result is never less than zero
 */
static void
tsub(struct timeval *a, struct timeval *b)
{
    pmtimevalDec(a, b);
    if (a->tv_sec < 0) {
	/* clip negative values at zero */
	a->tv_sec = 0;
	a->tv_usec = 0;
    }
}

/*
 * a : b for struct timevals ... <0 for a<b, ==0 for a==b, >0 for a>b
 */
static int
tcmp(struct timeval *a, struct timeval *b)
{
    int		res;
    res = (int)(a->tv_sec - b->tv_sec);
    if (res == 0)
	res = (int)(a->tv_usec - b->tv_usec);
    return res;
}

static void
enqueue(qelt *qp)
{
    qelt		*tqp;
    qelt		*priorp;

    if (pmDebugOptions.af) {
	struct timeval	now;

	pmtimevalNow(&now);
	pmPrintStamp(stderr, &now);
	fprintf(stderr, " AFenqueue " PRINTF_P_PFX "%p(%d, " PRINTF_P_PFX "%p) for ",
		qp->q_func, qp->q_afid, qp->q_data);
	pmPrintStamp(stderr, &qp->q_when);
	fputc('\n', stderr);
    }

    for (tqp = root, priorp = NULL;
	 tqp != NULL && tcmp(&qp->q_when, &tqp->q_when) >= 0;
	 tqp = tqp->q_next)
	    priorp = tqp;

    if (priorp == NULL) {
	qp->q_next = root;
	root = qp;
    }
    else {
	qp->q_next = priorp->q_next;
	priorp->q_next = qp;
    }
}

/*
 * must be async-signal-safe, see man signal-safety(7) on Linux
 *
 * called routines (for POSIX variant of libpcp code)
 *
 * AFhold		- ok
 * AFsetitimer		- ?
 * AFrearm		- ok
 * AFrelse		- ok
 * pmtimevalNow calls clock_gettime()
 *			- ok
 * pmtimevalInc		- ok
 * pmtimevalDec		- ok
 * tsub			- ok
 * enqueue		- ok
 * qp->q_func		- potential problem if application func() does
 *                        not restrict itself to async-signal-safe
 *                        routines
 *
 * in debug code
 *   fprintf    - problem, but we are not going to rewrite all of debug
 *                code, so accept that if PCP -Daf debugging is enabled
 *		  this code is no longer async-signal-safe
 */
static void
onalarm(int dummy)
{
    struct timeval	now;
    struct timeval	tmp;
    struct timeval	interval;
    qelt		*qp;

    if (!block)
	AFhold();

    if (pmDebugOptions.af) {
	pmtimevalNow(&now);
	pmPrintStamp(stderr, &now);
	fprintf(stderr, " AFonalarm(%d)\n", dummy);
    }
    if (root != NULL) {
	/* something to do ... */
	while (root != NULL) {
	    /* compute difference between scheduled time and now */
	    pmtimevalNow(&now);
	    tmp = root->q_when;
	    tsub(&tmp, &now);
	    if (tmp.tv_sec == 0 && tmp.tv_usec <= 10000) {
		/*
		 * within one 10msec tick, the time has passed for this one,
		 * execute the callback and reschedule
		 */

		qp = root;
		root = root->q_next;
		if (pmDebugOptions.af) {
		    pmPrintStamp(stderr, &now);
		    fprintf(stderr, " AFcallback " PRINTF_P_PFX "%p(%d, " PRINTF_P_PFX "%p)\n",
			    qp->q_func, qp->q_afid, qp->q_data);
		}
		qp->q_func(qp->q_afid, qp->q_data);
             
		if (qp->q_delta.tv_sec == 0 && qp->q_delta.tv_usec == 0) {
		    /*
                     * if delta is zero, this is a single-shot event, so
                     * do not reschedule it ... garbage collection is
                     * delayed until __pmAFblock() or __pmAFunblock() when
                     * we are no longer in an signal handler
		     */
		    qp->q_next = gc;
		    gc = qp;
		}
		else {
		    /*
		     * avoid falling too far behind
		     * if the scheduled time is more than q_delta in the
		     * past we will never catch up ... better to skip some
		     * events to catch up ...
		     *
		     * <------------ next q_when range ----------------->
		     *
		     *      cannot catchup | may catchup   | on schedule
		     *                     |               |
		     * --------------------|---------------|------------> time
		     *                     |               |
		     *      		   |               +-- now
		     *      		   +-- now - q_delta
		     */
		    pmtimevalNow(&now);
		    for ( ; ; ) {
			pmtimevalInc(&qp->q_when, &qp->q_delta);
			tmp = qp->q_when;
			pmtimevalDec(&tmp, &now);
			pmtimevalInc(&tmp, &qp->q_delta);
			if (tmp.tv_sec >= 0)
			    break;
			if (pmDebugOptions.af) {
			    pmPrintStamp(stderr, &now);
			    fprintf(stderr, " AFcallback event %d too slow, skip callback for ", qp->q_afid);
			    pmPrintStamp(stderr, &qp->q_when);
			    fputc('\n', stderr);
			}
		    }
		    enqueue(qp);
		}
	    }
	    else
		/*
		 * head of the queue (and hence all others) are too far in
		 * the future ... done for this time
		 */
		break;
	}

	if (root == NULL) {
	    if (pmDebugOptions.af) {
		pmPrintStamp(stderr, &now);
		fprintf(stderr, "Warning: AF event queue is empty, nothing more will be scheduled\n");
	    }
	    ;
	}
	else {
	    /* set itimer for head of queue */
	    interval = root->q_when;
	    pmtimevalNow(&now);
	    tsub(&interval, &now);
	    if (interval.tv_sec == 0 && interval.tv_usec < MIN_ITIMER_USEC)
		/* use minimal delay (platform dependent) */
		interval.tv_usec = MIN_ITIMER_USEC;
	    if (pmDebugOptions.af) {
		pmPrintStamp(stderr, &now);
		fprintf(stderr, " AFsetitimer for delta ");
		printdelta(stderr, &interval);
		fputc('\n', stderr);
	    }
	    AFsetitimer(&interval);
	}
    }
    if (!block) {
	AFrearm();
	AFrelse();
    }
}

int
__pmAFsetup(const struct timeval *start, const struct timeval *delta, void *data, void (*func)(int, void *))
{
    qelt		*qp;
    struct timeval	now;
    struct timeval	interval;

    __pmInitLocks();

    if (PM_MULTIPLE_THREADS(PM_SCOPE_AF))
	return PM_ERR_THREAD;

    if (!block)
	AFhold();
    if (afid == 0x8000 && !block)	/* first time */
	AFrearm();
    if ((qp = (qelt *)malloc(sizeof(qelt))) == NULL) {
	return -oserror();
    }
    qp->q_afid = ++afid;
    qp->q_data = data;
    qp->q_delta = *delta;
    qp->q_func = func;
    pmtimevalNow(&qp->q_when);
    if (start != NULL)
	pmtimevalInc(&qp->q_when, start);

    enqueue(qp);
    if (root == qp) {
	/* we ended up at the head of the list, set itimer */
	interval = qp->q_when;
	pmtimevalNow(&now);
	tsub(&interval, &now);

	if (interval.tv_sec == 0 && interval.tv_usec < MIN_ITIMER_USEC)
	    /* use minimal delay (platform dependent) */
	    interval.tv_usec = MIN_ITIMER_USEC;

	if (pmDebugOptions.af) {
	    pmPrintStamp(stderr, &now);
	    fprintf(stderr, " AFsetitimer for delta ");
	    printdelta(stderr, &interval);
	    fputc('\n', stderr);
	}
	AFsetitimer(&interval);
    }

    if (!block)
	AFrelse();
    return qp->q_afid;
}

int
__pmAFregister(const struct timeval *delta, void *data, void (*func)(int, void *))
{
    return __pmAFsetup(delta, delta, data, func);
}

int
__pmAFunregister(int id)
{
    qelt		*qp;
    qelt		*priorp;
    struct timeval	now;
    struct timeval	interval;

    __pmInitLocks();

    if (PM_MULTIPLE_THREADS(PM_SCOPE_AF))
	return PM_ERR_THREAD;

    if (!block)
	AFhold();
    for (qp = root, priorp = NULL; qp != NULL && qp->q_afid != id; qp = qp->q_next)
	    priorp = qp;

    if (qp == NULL) {
	if (!block)
	    AFrelse();
	return -1;
    }

    if (priorp == NULL) {
	root = qp->q_next;
	if (root != NULL) {
	    /*
	     * we removed the head of the queue, set itimer for the
	     * new head of queue
	     */
	    interval = root->q_when;
	    pmtimevalNow(&now);
	    tsub(&interval, &now);
	    if (interval.tv_sec == 0 && interval.tv_usec < MIN_ITIMER_USEC)
		/* use minimal delay (platform dependent) */
		interval.tv_usec = MIN_ITIMER_USEC;
	    if (pmDebugOptions.af) {
		pmPrintStamp(stderr, &now);
		fprintf(stderr, " AFsetitimer for delta ");
		printdelta(stderr, &interval);
		fputc('\n', stderr);
	    }
	    AFsetitimer(&interval);
	}
    }
    else
	priorp->q_next = qp->q_next;

    free(qp);

    if (!block)
	AFrelse();
    return 0;
}

void
__pmAFblock(void)
{
    __pmInitLocks();

    if (PM_MULTIPLE_THREADS(PM_SCOPE_AF))
	return;
    block = 1;
    AFhold();
    /* garbage collection from onalarm() */
    while (gc != NULL) {
	qelt	*qp = gc;
	gc = qp->q_next;
	free(qp);
    }
}

void
__pmAFunblock(void)
{
    __pmInitLocks();

    if (PM_MULTIPLE_THREADS(PM_SCOPE_AF))
	return;
    /* garbage collection from onalarm() */
    while (gc != NULL) {
	qelt	*qp = gc;
	gc = qp->q_next;
	free(qp);
    }
    block = 0;
    AFrearm();
    AFrelse();
}

int
__pmAFisempty(void)
{
    return (root==NULL);
}
