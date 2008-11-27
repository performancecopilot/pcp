/*
 * Copyright (c) 1995-2001,2003 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 */

/*
 * general purpose asynchronous event management
 */

#include <signal.h>
#include "pmapi.h"
#include "impl.h"

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
static int		afid = 0x8000;
static int		block;
static struct itimerval	val;

#if !defined(HAVE_SIGHOLD)
static int
sighold(int sig)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, sig);
    sigprocmask(SIG_BLOCK, &s, NULL);

    return 0;
}
#else
/*
 * for Linux the prototype is hidden, even though the function is in
 * libc
 */
extern int sighold(int);
#endif

#if !defined(HAVE_SIGRELSE)
static int
sigrelse(int sig)
{
    sigset_t s;

    sigemptyset(&s);
    sigaddset(&s, sig);
    sigprocmask(SIG_UNBLOCK, &s, NULL);
    return 0;
}
#else
/*
 * for Linux the prototype is hidden, even though the function is in
 * libc
 */
extern int sigrelse(int);
#endif

#ifdef PCP_DEBUG
static void
printstamp(struct timeval *tp)
{
    static struct tm    *tmp;
    time_t		tt =  (time_t)tp->tv_sec;

    tmp = localtime(&tt);
    fprintf(stderr, "%02d:%02d:%02d.%06ld", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (long)tp->tv_usec);
}

static void
printdelta_pcp(struct timeval *tp)
{
    static struct tm    *tmp;
    time_t		tt =  (time_t)tp->tv_sec;

    tmp = gmtime(&tt);
    fprintf(stderr, "%02d:%02d:%02d.%06ld", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (long)tp->tv_usec);
}
#endif

/*
 * a := a + b for struct timevals
 */
static void
tadd(struct timeval *a, struct timeval *b)
{
    a->tv_usec += b->tv_usec;
    if (a->tv_usec > 1000000) {
	a->tv_usec -= 1000000;
	a->tv_sec++;
    }
    a->tv_sec += b->tv_sec;
}

/*
 * a := a - b for struct timevals
 */
static void
tsub_real(struct timeval *a, struct timeval *b)
{
    a->tv_usec -= b->tv_usec;
    if (a->tv_usec < 0) {
	a->tv_usec += 1000000;
	a->tv_sec--;
    }
    a->tv_sec -= b->tv_sec;
}

/*
 * a := a - b for struct timevals, but result is never less than zero
 */
static void
tsub(struct timeval *a, struct timeval *b)
{
    tsub_real(a, b);
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

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_AF) {
	struct timeval	now;
	gettimeofday(&now, NULL);
	printstamp(&now);
	fprintf(stderr, " AFenqueue " PRINTF_P_PFX "%p(%d, " PRINTF_P_PFX "%p) for ",
		qp->q_func, qp->q_afid, qp->q_data);
	printstamp(&qp->q_when);
	fputc('\n', stderr);
    }
#endif

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

static void
onalarm(int dummy)
{
    struct timeval	now;
    struct timeval	tmp;
    qelt		*qp;

    if (!block)
	sighold(SIGALRM);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_AF) {
	gettimeofday(&now, NULL);
	printstamp(&now);
	fprintf(stderr, " AFonalarm(%d)\n", dummy);
    }
#endif
    if (root != NULL) {
	/* something to do ... */
	while (root != NULL) {
	    /* compute difference between scheduled time and now */
	    gettimeofday(&now, NULL);
	    tmp = root->q_when;
	    tsub(&tmp, &now);
	    if (tmp.tv_sec == 0 && tmp.tv_usec <= 10000) {
		/*
		 * within one 10msec tick, the time has passed for this one,
		 * execute the callback and reschedule
		 */

		qp = root;
		root = root->q_next;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_AF) {
		    printstamp(&now);
		    fprintf(stderr, " AFcallback " PRINTF_P_PFX "%p(%d, " PRINTF_P_PFX "%p)\n",
			    qp->q_func, qp->q_afid, qp->q_data);
		}
#endif
		qp->q_func(qp->q_afid, qp->q_data);
             
		if (qp->q_delta.tv_sec == 0 && qp->q_delta.tv_usec == 0) {
		    /*
		     * if delta is zero, this is a single-shot event,
		     * so do not reschedule it
		     */
		    free(qp);
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
		    gettimeofday(&now, NULL);
		    for ( ; ; ) {
			tadd(&qp->q_when, &qp->q_delta);
			tmp = qp->q_when;
			tsub_real(&tmp, &now);
			tadd(&tmp, &qp->q_delta);
			if (tmp.tv_sec >= 0)
			    break;
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_AF) {
			    printstamp(&now);
			    fprintf(stderr, " AFcallback event %d too slow, skip callback for ", qp->q_afid);
			    printstamp(&qp->q_when);
			    fputc('\n', stderr);
			}
#endif
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
	    pmprintf("Warning: AF event queue is empty, nothing more will be scheduled\n");
	    pmflush();
	}
	else {
	    /* set itimer for head of queue */
	    val.it_value = root->q_when;
	    gettimeofday(&now, NULL);
	    tsub(&val.it_value, &now);
	    if (val.it_value.tv_sec == 0 && val.it_value.tv_usec < MIN_ITIMER_USEC)
		/* use minimal delay (platform dependent) */
		val.it_value.tv_usec = MIN_ITIMER_USEC;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_AF) {
		printstamp(&now);
		fprintf(stderr, " AFsetitimer for delta ");
		printdelta_pcp(&val.it_value);
		fputc('\n', stderr);
	    }
#endif
	    setitimer(ITIMER_REAL, &val, NULL);
	}
    }
    if (!block) {
	signal(SIGALRM, onalarm);
	sigrelse(SIGALRM);
    }
}

int
__pmAFregister(const struct timeval *delta, void *data, void (*func)(int, void *))
{
    qelt		*qp;
    struct timeval	now;

    if (!block)
	sighold(SIGALRM);
    if (afid == 0x8000 && !block) {
	/* first time */
	signal(SIGALRM, onalarm);
    }
    if ((qp = (qelt *)malloc(sizeof(qelt))) == NULL) {
	return -errno;
    }
    qp->q_afid = ++afid;
    qp->q_data = data;
    qp->q_delta = *delta;
    qp->q_func = func;
    gettimeofday(&qp->q_when, NULL);
    tadd(&qp->q_when, &qp->q_delta);

    enqueue(qp);
    if (root == qp) {
	/* we ended up at the head of the list, set itimer */
	val.it_value = qp->q_when;
	gettimeofday(&now, NULL);
	tsub(&val.it_value, &now);

	if (val.it_value.tv_sec == 0 && val.it_value.tv_usec < MIN_ITIMER_USEC)
	    /* use minimal delay (platform dependent) */
	    val.it_value.tv_usec = MIN_ITIMER_USEC;

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_AF) {
	    printstamp(&now);
	    fprintf(stderr, " AFsetitimer for delta ");
	    printdelta_pcp(&val.it_value);
	    fputc('\n', stderr);
	}
#endif
	setitimer(ITIMER_REAL, &val, NULL);
    }

    if (!block)
	sigrelse(SIGALRM);
    return qp->q_afid;
}

int
__pmAFunregister(int afid)
{
    qelt		*qp;
    qelt		*priorp;
    struct timeval	now;

    if (!block)
	sighold(SIGALRM);
    for (qp = root, priorp = NULL; qp != NULL && qp->q_afid != afid; qp = qp->q_next)
	    priorp = qp;

    if (qp == NULL) {
	if (!block)
	    sigrelse(SIGALRM);
	return -1;
    }

    if (priorp == NULL) {
	root = qp->q_next;
	if (root != NULL) {
	    /*
	     * we removed the head of the queue, set itimer for the
	     * new head of queue
	     */
	    val.it_value = root->q_when;
	    gettimeofday(&now, NULL);
	    tsub(&val.it_value, &now);
	    if (val.it_value.tv_sec == 0 && val.it_value.tv_usec == 0)
		/* arbitrary 0.1 msec as minimal delay */
		val.it_value.tv_usec = 100;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_AF) {
		printstamp(&now);
		fprintf(stderr, " AFsetitimer for delta ");
		printdelta_pcp(&val.it_value);
		fputc('\n', stderr);
	    }
#endif
	    setitimer(ITIMER_REAL, &val, NULL);
	}
    }
    else
	priorp->q_next = qp->q_next;

    free(qp);

    if (!block)
	sigrelse(SIGALRM);
    return 0;
}

void
__pmAFblock(void)
{
    block = 1;
    sighold(SIGALRM);
}

void
__pmAFunblock(void)
{
    block = 0;
    signal(SIGALRM, onalarm);
    sigrelse(SIGALRM);
}

int
__pmAFisempty(void)
{
    return (root==NULL);
}
