/***********************************************************************
 * dstruct.c - central data structures and associated operations
 ***********************************************************************
 *
 * Copyright (c) 2013-2014 Red Hat.
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
#include "pmapi.h"
#include "impl.h"
#include <math.h>
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif
#include "dstruct.h"
#include "symbol.h"
#include "pragmatics.h"
#include "fun.h"
#include "eval.h"
#include "show.h"

#if defined(HAVE_VALUES_H)
#include <values.h>
#endif
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif

/***********************************************************************
 * constants
 ***********************************************************************/

double	mynan;				/* not-a-number run time initialized */


/***********************************************************************
 * user supplied parameters
 ***********************************************************************/

char		*pmnsfile = PM_NS_DEFAULT;	/* alternate name space */
Archive		*archives;			/* list of open archives */
RealTime	first = -1;			/* archive starting point */
RealTime	last = 0.0;			/* archive end point */
char		*dfltHostConn;			/* default host connection string */
char		*dfltHostName;			/* default host name */
RealTime	dfltDelta = DELTA_DFLT;		/* default sample interval */
char		*startFlag;			/* start time specified? */
char		*stopFlag;			/* end time specified? */
char		*alignFlag;			/* align time specified? */
char		*offsetFlag;			/* offset time specified? */
RealTime	runTime;			/* run time interval */
int		hostZone;			/* timezone from host? */
char		*timeZone;			/* timezone from command line */
int		quiet;				/* suppress default diagnostics */
int		verbose;			/* verbosity 0, 1 or 2 */
int		interactive;			/* interactive mode, -d */
int		isdaemon;			/* run as a daemon */
int		agent;				/* secret agent mode? */
int		applet;				/* applet mode? */
int		dowrap;				/* counter wrap? default no */
int		doexit;				/* time to exit stage left? */
int		dorotate;			/* is a log rotation pending? */
pmiestats_t	*perf;				/* live performance data */
pmiestats_t	instrument;			/* used if no mmap (archive) */


/***********************************************************************
 * this is where the action is
 ***********************************************************************/

Task		*taskq = NULL;		/* evaluator task queue */
Expr		*curr;			/* current executing rule expression */

SymbolTable	hosts;			/* currently known hosts */
SymbolTable	metrics;		/* currently known metrics */
SymbolTable	rules;			/* currently known rules */
SymbolTable	vars;			/* currently known variables */


/***********************************************************************
 * time
 ***********************************************************************/

RealTime	now;			/* current time */
RealTime	start;			/* start evaluation time */
RealTime	stop;			/* stop evaluation time */

Symbol		symDelta;		/* current sample interval */
Symbol		symMinute;		/* minutes after the hour 0..59 */
Symbol		symHour;		/* hours since midnight 0..23 */
Symbol		symDay;			/* day of the month 1..31 */
Symbol		symMonth;		/* month of the year 1..12 */
Symbol		symYear;		/* year 1996.. */
Symbol		symWeekday;		/* days since Sunday 0..6 */

static double	delta;			/* current sample interval */
static double	second;			/* seconds after the minute 0..59 */
static double	minute;			/* minutes after the hour 0..59 */
static double	hour;			/* hours since midnight 0..23 */
static double	day;			/* day of the month 1..31 */
static double	month;			/* month of the year 1..12 */
static double	year;			/* year 1996.. */
static double	weekday;		/* days since Sunday 0..6 */

/***********************************************************************
 * process creation control
 ***********************************************************************/
int		need_wait;

/* return real time */
RealTime
getReal(void)
{
    struct timeval t;

    __pmtimevalNow(&t);
    return __pmtimevalToReal(&t);
}


/* update time variables to reflect current time */
void
reflectTime(RealTime d)
{
    static time_t   then = 0;			/* previous time */
    int		    skip = now - then;
    struct tm	    tm;

    then = (time_t)now;

    /* sample interval */
    delta = d;

    /* try short path for current time */
    if (skip >= 0 && skip < 24 * 60 * 60) {
	second += skip;
	if (second < 60)
	    return;
	skip = (int)(second / 60);
	second -= (double)(60 * skip);
	minute += (double)skip;
	if (minute < 60)
	    return;
	skip = (int)(minute / 60);
	minute -= (double)(60 * skip);
	hour += (double)skip;
	if (hour < 24)
	    return;
    }

    /* long path for current time */
    pmLocaltime(&then, &tm);
    second = (double) tm.tm_sec;
    minute = (double) tm.tm_min;
    hour = (double) tm.tm_hour;
    day = (double) tm.tm_mday;
    month = (double) tm.tm_mon;
		    /* tm_year is years since 1900, so this is Y2K safe */
    year = (double) tm.tm_year + 1900;
    weekday = (double) tm.tm_wday;
}

/* convert RealTime to timespec */
static void
unrealizenano(RealTime rt, struct timespec *ts)
{
    ts->tv_sec = (time_t)rt;
    ts->tv_nsec = (int)(1000000000 * (rt - ts->tv_sec));
}

#define SLEEP_EVAL	0
#define SLEEP_RETRY	1

/* sleep until eval or retry RealTime */
void
sleepTight(Task *t, int type)
{
    RealTime	sched;
    RealTime	delay;	/* interval to sleep */
    int		sts;
    RealTime	cur_entry = getReal();
#ifdef HAVE_WAITPID
    pid_t	pid;

    if (need_wait) {
	/* harvest terminated children */
	while ((pid = waitpid(-1, &sts, WNOHANG)) > (pid_t)0) {
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "sleepTight: wait: pid=%" FMT_PID " done status=0x%x", pid, sts);
		if (WIFEXITED(sts))
		    fprintf(stderr, " exit=%d", WEXITSTATUS(sts));
		if (WIFSIGNALED(sts))
		    fprintf(stderr, " signal=%d", WTERMSIG(sts));
		fprintf(stderr, "\n");
	    }
#endif
	    ;
	}
	need_wait = 0;
    }
#endif

    if (!archives) {
	struct timespec ts, tleft;
	static RealTime	last_sched = -1;
	static Task *last_t;
	static int last_type;
	RealTime cur = getReal();

	sched = type == SLEEP_EVAL ? t->eval : t->retry;

	delay = sched - cur;
	if (delay < 0) {
	    int		show_detail = 0;
	    if (delay <= -1) {
		fprintf(stderr, "sleepTight: negative delay (%f). sched=%f, cur=%f\n",
			    delay, sched, cur);
		show_detail = 1;
	    }
#if PCP_DEBUG
	    else {
		if (pmDebug & DBG_TRACE_APPL2) {
		    fprintf(stderr, "sleepTight: small negative delay (%f). sched=%f, cur=%f\n",
			    delay, sched, cur);
		    show_detail = 1;
		}
	    }
#endif
	    if (show_detail) {
		if (last_sched > 0) {
		    fprintf(stderr, "Last sleepTight (%s) until: ", last_type == SLEEP_EVAL ? "eval" : "retry");
		    showFullTime(stderr, last_sched);
		    fputc('\n', stderr);
		    fprintf(stderr, "Last ");
		    dumpTask(last_t);
		}
		fprintf(stderr, "This sleepTight() entry: ");
		showFullTime(stderr, cur_entry);
		fputc('\n', stderr);
		fprintf(stderr, "Harvest children done: ");
		showFullTime(stderr, cur);
		fputc('\n', stderr);
		fprintf(stderr, "Want sleepTight (%s) until: ", type == SLEEP_EVAL ? "eval" : "retry");
		showFullTime(stderr, sched);
		fputc('\n', stderr);
		fprintf(stderr, "This ");
		dumpTask(t);
	    }
	}
	else {
	    unrealizenano(delay, &ts);
	    for (;;) {	/* loop to catch early wakeup from nanosleep */
		if (ts.tv_sec < 0 || ts.tv_nsec > 999999999) {
		    fprintf(stderr, "sleepTight: invalid args: %ld %ld\n",
			    ts.tv_sec, ts.tv_nsec);
		    break;
		}
		sts = nanosleep(&ts, &tleft);
		/* deferred signal handling done immediately */
		if (doexit)
		    exit(doexit);
		if (dorotate) {
		    logRotate();
		    dorotate = 0;
		}
		if (sts == 0 || (sts < 0 && oserror() != EINTR))
		    break;
		ts = tleft;
	    }
	}
	last_t = t;
	last_type = type;
	last_sched = sched;
    }
}


/***********************************************************************
 * ring buffer management
 ***********************************************************************/

void
newRingBfr(Expr *x)
{
    size_t  sz;
    char    *p;
    int     i;

    sz = ((x->sem == SEM_BOOLEAN) || (x->sem == SEM_CHAR)) ?
	    sizeof(char) * x->tspan :
	    sizeof(double) * x->tspan;
    if (x->ring) free(x->ring);
    x->ring = alloc(x->nsmpls * sz);
    p = (char *) x->ring;
    for (i = 0; i < x->nsmpls; i++) {
	x->smpls[i].ptr = (void *) p;
	p += sz;
    }
}


void
newStringBfr(Expr *x, size_t length, char *bfr)
{
    if (x->e_idom != (int)length) {
	x->e_idom = (int)length;
	x->tspan = (int)length;
	x->nvals = (int)length;
    }
    if (x->ring)
	free(x->ring);
    x->ring = bfr;
    x->smpls[0].ptr = (void *) bfr;
}


/* Rotate ring buffer - safe to call only if x->nsmpls > 1 */
void
rotate(Expr *x)
{
    int     n = x->nsmpls-1;
    Sample *q = &x->smpls[n];
    Sample *p = q - 1;
    void   *t = q->ptr;
    int	    i;

    for (i = n; i > 0; i--)
	*q-- = *p--;
    x->smpls[0].ptr = t;
}


/***********************************************************************
 * memory allocation
 ***********************************************************************/

void *
alloc(size_t size)
{
    void *p;

    if ((p = malloc(size)) == NULL) {
	__pmNoMem("pmie.alloc", size, PM_FATAL_ERR);
    }
    return p;
}


void *
zalloc(size_t size)
{
    void *p;

    if ((p = calloc(1, size)) == NULL) {
	__pmNoMem("pmie.zalloc", size, PM_FATAL_ERR);
    }
    return p;
}


void *
aalloc(size_t align, size_t size)
{
    void	*p = NULL;
    int		sts = 0;
#ifdef HAVE_POSIX_MEMALIGN
    sts = posix_memalign(&p, align, size);
#else
#ifdef HAVE_MEMALIGN
    p = memalign(align, size);
    if (p == NULL) sts = -1;
#else
    p = malloc(size);
    if (p == NULL) sts = -1;
#endif
#endif
    if (sts != 0) {
	__pmNoMem("pmie.aalloc", size, PM_FATAL_ERR);
    }
    return p;
}


void *
ralloc(void *p, size_t size)
{
    void *q;

    if ((q = realloc(p, size)) == NULL) {
	__pmNoMem("pmie.ralloc", size, PM_FATAL_ERR);
    }
    return q;
}

char *
sdup(char *p)
{
    char *q;

    if ((q = strdup(p)) == NULL) {
	__pmNoMem("pmie.sdup", strlen(p), PM_FATAL_ERR);
    }
    return q;
}


Expr *
newExpr(int op, Expr *arg1, Expr *arg2,
	int hdom, int idom, int tdom, int nsmpls,
	int sem)
{
    Expr *x;
    Expr *arg;

    x = (Expr *) zalloc(sizeof(Expr) + (nsmpls - 1) * sizeof(Sample));
    x->op = op;
    if (arg1) {
	x->arg1 = arg1;
	arg1->parent = x;
    }
    if (arg2) {
	x->arg2 = arg2;
	arg2->parent = x;
    }
    x->hdom = hdom;
    x->e_idom = idom;
    x->tdom = tdom;
    x->nsmpls = nsmpls;
    x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
    x->nvals = x->tspan * nsmpls;
    if (arg1) {
	arg = primary(arg1, arg2);
	x->metrics = arg->metrics;
    }
    if (sem == SEM_NUMVAR || sem == SEM_NUMCONST || sem == SEM_BOOLEAN ||
        sem == SEM_CHAR || sem == SEM_REGEX)
	x->units = noUnits;
    else {
	x->units = noUnits;
	SET_UNITS_UNKNOWN(x->units);
    }
    x->sem = sem;
    return x;
}


Profile *
newProfile(Fetch *owner, pmInDom indom)
{
    Profile *p = (Profile *) zalloc(sizeof(Profile));
    p->indom = indom;
    p->fetch = owner;
    return p;
}


Fetch *
newFetch(Host *owner)
{
    Fetch *f = (Fetch *) zalloc(sizeof(Fetch));
    f->host = owner;
    return f;
}


Host *
newHost(Task *owner, Symbol name)
{
    Host *h = (Host *) zalloc(sizeof(Host));

    h->name = symCopy(name);
    h->task = owner;
    return h;
}


Task *
newTask(RealTime delta, int nth)
{
    Task *t = (Task *) zalloc(sizeof(Task));
    t->nth = nth;
    t->delta = delta;
    return t;
}

/* translate new metric name to internal pmid for agent mode */
static pmID
agentId(char *name)
{
    int		sts;
    pmID	pmid;

    if ((sts = pmLookupName(1, &name, &pmid)) < 0) {
	fprintf(stderr, "%s: agentId: metric %s not found in namespace: %s\n",
		pmProgname, name, pmErrStr(sts));
	exit(1);
    }
    return pmid;
}


void
newResult(Task *t)
{
    pmResult	 *rslt;
    Symbol	 *sym;
    pmValueSet	 *vset;
    pmValueBlock *vblk;
    int		 i;
    int		 len;

    /* allocate pmResult */
    rslt = (pmResult *) zalloc(sizeof(pmResult) + (t->nrules - 1) * sizeof(pmValueSet *));
    rslt->numpmid = t->nrules;

    /* allocate pmValueSet's */
    sym = t->rules;
    for (i = 0; i < t->nrules; i++) {
	vset = (pmValueSet *)alloc(sizeof(pmValueSet));
	vset->pmid = agentId(symName(*sym));
	vset->numval = 0;
	vset->valfmt = PM_VAL_DPTR;
	vset->vlist[0].inst = PM_IN_NULL;
	len = PM_VAL_HDR_SIZE + sizeof(double);
	vblk = (pmValueBlock *)zalloc(len);
	vblk->vlen = len;
	vblk->vtype = PM_TYPE_DOUBLE;
	vset->vlist[0].value.pval = vblk;
	rslt->vset[i] = vset;
	sym++;
    }

    t->rslt = rslt;
}


/***********************************************************************
 * memory deallocation
 *
 * IMPORTANT: These functions free the argument structure plus any
 *            structures it owns below it in the expression tree.
 ***********************************************************************/

void
freeTask(Task *t)
{
    if ((t->hosts == NULL) && (t->rules == NULL)) {
	if (t->next) t->next->prev = t->prev;
	if (t->prev) t->prev->next = t->next;
	else taskq = t->next;
	free(t);
   }
}


void
freeHost(Host *h)
{
    if ((h->fetches == NULL) && (h->waits == NULL)) {
	if (h->next) h->next->prev = h->prev;
	if (h->prev) h->prev->next = h->next;
	else {
	    h->task->hosts = h->next;
	    freeTask(h->task);
	}
	symFree(h->name);
	free(h);
    }
}


void
freeFetch(Fetch *f)
{
    if (f->profiles == NULL) {
	if (f->next) f->next->prev = f->prev;
	if (f->prev) f->prev->next = f->next;
	else {
	    f->host->fetches = f->next;
	    freeHost(f->host);
	}
	pmDestroyContext(f->handle);
	if (f->result) pmFreeResult(f->result);
	if (f->pmids) free(f->pmids);
	free(f);
    }
}


void
FreeProfile(Profile *p)
{
    if (p->metrics == NULL) {
	if (p->next) p->next->prev = p->prev;
	if (p->prev) p->prev->next = p->next;
	else {
	    p->fetch->profiles = p->next;
	    freeFetch(p->fetch);
	}
	free(p);
    }
}


void
freeMetric(Metric *m)
{
    int		numinst;

    /* Metric is on fetch list */
    if (m->profile) {
	if (m->prev) {
	    m->prev->next = m->next;
	    if (m->next) m->next->prev = m->prev;
	}
	else {
	    m->host->waits = m->next;
	    if (m->next) m->next->prev = NULL;
	}
	if (m->host) freeHost(m->host);
    }

    symFree(m->hname);
    numinst =  m->specinst == 0 ? m->m_idom : m->specinst;
    if (numinst > 0 && m->inames) {
	int	i;
	for (i = 0; i < numinst; i++) {
	    if (m->inames[i] != NULL) free(m->inames[i]);
	}
	free(m->inames);
    }
    if (numinst && m->iids) free(m->iids);
    if (m->vals) free(m->vals);
}


void
freeExpr(Expr *x)
{
    Metric	*m;
    int		i;

    if (x) {
	if (x->arg1 && x->arg1->parent == x)
	    freeExpr(x->arg1);
	if (x->arg2 && x->arg2->parent == x)
	    freeExpr(x->arg2);
	if (x->metrics && x->op == CND_FETCH) {
	    for (m = x->metrics, i = 0; i < x->hdom; m++, i++)
		freeMetric(m);
	    /*
	     * x->metrics allocated in a block, one element per host, so
	     * free as one after all other freeing has been done.
	     */
	    free(x->metrics);
	}
	if (x->ring) free(x->ring);
	free(x);
    }
}


/***********************************************************************
 * comparison functions (for use by qsort)
 ***********************************************************************/

/* Compare two instance identifiers.
   - This function is passed as an argument to qsort, hence the casts. */
int	/* -1 less, 0 equal, 1 greater */
compid(const void *i1, const void *i2)
{
    if (*(int *)i1 < *(int *)i2) return -1;
    if (*(int *)i1 > *(int *)i2) return 1;
    return 0;
}


/* Compare two pmValue's on their inst fields
   - This function is passed as an argument to qsort, hence the casts. */
int	/* -1 less, 0 equal, 1 greater */
compair(const void *pmv1, const void *pmv2)
{
    if (((pmValue *)pmv1)->inst < ((pmValue *)pmv2)->inst) return -1;
    if (((pmValue *)pmv1)->inst > ((pmValue *)pmv2)->inst) return 1;
    return 0;
}


/***********************************************************************
 * Expr manipulation
 ***********************************************************************/

/* Decide primary argument for inheritance of Expr attributes */
Expr *
primary(Expr *arg1, Expr *arg2)
{
    if (arg2 == NULL || arg1->nvals > 1)
	return arg1;
    if (arg2->nvals > 1)
	return arg2;
    if (arg1->metrics &&
	(arg1->hdom != -1 || arg1->e_idom != -1 || arg1->tdom != -1))
	return arg1;
    if (arg2->metrics &&
	(arg2->hdom != -1 || arg2->e_idom != -1 || arg2->tdom != -1))
	return arg2;
    return arg1;
}


/* change number of samples allocated in ring buffer */
void
changeSmpls(Expr **p, int nsmpls)
{
    Expr   *x = *p;
    Metric *m;
    int    i;

    if (nsmpls == x->nsmpls) return;
    x = (Expr *) ralloc(x, sizeof(Expr) + (nsmpls - 1) * sizeof(Sample));
    x->nsmpls = nsmpls;
    x->nvals = x->tspan * nsmpls;
    x->valid = 0;
    if (x->op == CND_FETCH) {
	/* node relocated, fix pointers from associated Metric structures */
	m = x->metrics;
	for (i = 0; i < x->hdom; i++) {
	    m->expr = x;
	    m++;
	}
    }
    /*
     * we have relocated node from *p to x ... need to fix the
     * parent pointers in any descendent nodes
     */
    if (x->arg1 != NULL)
	x->arg1->parent = x;
    if (x->arg2 != NULL)
	x->arg2->parent = x;

    *p = x;
    newRingBfr(x);
}


/* propagate instance domain, semantics and units from
   argument expressions to parents */
static void
instExpr(Expr *x)
{
    int	    up = 0;
    Expr    *arg1 = x->arg1;
    Expr    *arg2 = x->arg2;
    Expr    *arg = primary(arg1, arg2);

    /* semantics ... */
    if (x->sem == SEM_UNKNOWN) {
	if (arg2 == NULL) {
	    /* unary expression */
	    if (arg1->sem != SEM_UNKNOWN) {
		up = 1;
		x->sem = arg1->sem;
	    }
	}
	else if ((arg1->sem != SEM_UNKNOWN) &&
		 (arg2->sem != SEM_UNKNOWN)) {
	    /* binary expression with known args */
	    up = 1;
	    x->sem = arg->sem;
	}
	/* binary expression with unknown arg */
	else
	    return;
    }

    /* units ... */
    if (UNITS_UNKNOWN(x->units)) {
	if (arg2 == NULL) {
	    /* unary expression */
	    if (!UNITS_UNKNOWN(arg1->units)) {
		up = 1;
		x->units = arg1->units;
	    }
	    if (x->op == CND_INSTANT && x->metrics->desc.sem == PM_SEM_COUNTER) {
		up = 1;
		x->units.dimTime++;
	    }
	    if (x->op == CND_RATE) {
		up = 1;
		x->units.dimTime--;
	    }
	}
	else if (!UNITS_UNKNOWN(arg1->units) &&
		 !UNITS_UNKNOWN(arg2->units)) {
	    /* binary expression with known args */
	    up = 1;
	    x->units = arg->units;
	    if (x->op == CND_MUL) {
		x->units.dimSpace = arg1->units.dimSpace + arg2->units.dimSpace;
		x->units.dimTime = arg1->units.dimTime + arg2->units.dimTime;
		x->units.dimCount = arg1->units.dimCount + arg2->units.dimCount;
	    }
	    else if (x->op == CND_DIV) {
		x->units.dimSpace = arg1->units.dimSpace - arg2->units.dimSpace;
		x->units.dimTime = arg1->units.dimTime - arg2->units.dimTime;
		x->units.dimCount = arg1->units.dimCount - arg2->units.dimCount;
	    }
	}
    }

    /* instance domain */
    if ((x->e_idom != -1) && (x->e_idom != arg->e_idom)) {
	up = 1;
	x->e_idom = arg->e_idom;
	x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
	x->nvals = x->tspan * x->nsmpls;
	x->valid = 0;
	newRingBfr(x);
    }

    if (up && x->parent)
	instExpr(x->parent);
}


/* propagate instance domain, semantics and units from given
   fetch expression to its parents */
void
instFetchExpr(Expr *x)
{
    Metric  *m;
    int     ninst;
    int	    up = 0;
    int     i;

    /* update semantics and units */
    if (x->sem == SEM_UNKNOWN) {
	m = x->metrics;
	for (i = 0; i < x->hdom; i++) {
	    if (m->desc.sem != SEM_UNKNOWN) {
		if (m->desc.sem == PM_SEM_COUNTER) {
		    x->sem = PM_SEM_INSTANT;
		    x->units = canon(m->desc.units);
		    x->units.dimTime--;
		}
		else {
		    x->sem = m->desc.sem;
		    x->units = canon(m->desc.units);
		}
		up = 1;
		break;
	    }
	}
    }

    /*
     * update number of instances ... need to be careful because may be more
     * than one host, and instances may not be fully available ...
     *   m_idom < 0 =>	no idea how many instances there might be (cannot
     *			contact pmcd, unknown metric, can't get indom, ...)
     *   m_idom == 0 =>	no values, but otherwise OK
     *   m_idom > 0 =>	have this many values (and hence instances)
     */
    m = x->metrics;
    ninst = -1;
    for (i = 0; i < x->hdom; i++) {
	m->offset = ninst;
	if (m->m_idom >= 0) {
	    if (ninst == -1)
		ninst = m->m_idom;
	    else
		ninst += m->m_idom;
	}
	m++;
    }
    if (x->e_idom != ninst) {
	/* number of instances is different */
	x->e_idom = ninst;
	x->tspan = (x->e_idom >= 0) ? x->e_idom : abs(x->hdom);
	x->nvals = x->nsmpls * x->tspan;
	x->valid = 0;
	newRingBfr(x);
	up = 1;
    }
    if (x->parent) {
	/* do we need to propagate changes? */
	if (up ||
	    (UNITS_UNKNOWN(x->parent->units) && !UNITS_UNKNOWN(x->units))) {
	    instExpr(x->parent);
	}
    }
}


/***********************************************************************
 * compulsory initialization
 ***********************************************************************/

void dstructInit(void)
{
    Expr   *x;
    double zero = 0.0;

    /* not-a-number initialization */
    mynan = zero / zero;

    /* don't initialize dfltHost*; let pmie.c do it after getopt. */

    /* set up symbol tables */
    symSetTable(&hosts);
    symSetTable(&metrics);
    symSetTable(&rules);
    symSetTable(&vars);

    /* set yp inter-sample interval (delta) symbol */
    symDelta = symIntern(&vars, "delta");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &delta;
    x->valid = 1;
    symValue(symDelta) = x;

    /* set up time symbols */
    symMinute = symIntern(&vars, "minute");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &minute;
    x->valid = 1;
    symValue(symMinute) = x;
    symHour = symIntern(&vars, "hour");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &hour;
    x->valid = 1;
    symValue(symHour) = x;
    symDay = symIntern(&vars, "day");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &day;
    x->valid = 1;
    symValue(symDay) = x;
    symMonth = symIntern(&vars, "month");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &month;
    x->valid = 1;
    symValue(symMonth) = x;
    symYear = symIntern(&vars, "year");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &year;
    x->valid = 1;
    symValue(symYear) = x;
    symWeekday = symIntern(&vars, "day_of_week");
    x = newExpr(OP_VAR, NULL,NULL, -1, -1, -1, 1, SEM_NUMVAR);
    x->smpls[0].ptr = &weekday;
    x->valid = 1;
    symValue(symWeekday) = x;
}


/* get ready to run evaluator */
void
agentInit(void)
{
    Task	*t;
    int		sts;

    /* Set up local name space for agent */
    /* Only load PMNS if it's default and hence not already loaded */
    if (pmnsfile == PM_NS_DEFAULT && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	fprintf(stderr, "%s: agentInit: cannot load metric namespace: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* allocate pmResult's and send pmDescs for secret agent mode */
    t = taskq;
    while (t) {
	newResult(t);
	sendDescs(t);
	t = t->next;
    }
}

/*
 * useful for diagnostics and with dbx
 */

static struct {
    void	(*addr)(Expr *);
    char	*name;
} fn_map[] = {
    { actAlarm,		"actAlarm" },
    { actAnd,		"actAnd" },
    { actArg,		"actArg" },
    { actFake,		"actFake" },
    { actOr,		"actOr" },
    { actPrint,		"actPrint" },
    { actShell,		"actShell" },
    { actStomp,		"actStomp" },
    { actSyslog,	"actSyslog" },
    { cndAdd_1_1,	"cndAdd_1_1" },
    { cndAdd_1_n,	"cndAdd_1_n" },
    { cndAdd_n_1,	"cndAdd_n_1" },
    { cndAdd_n_n,	"cndAdd_n_n" },
    { cndAll_host,	"cndAll_host" },
    { cndAll_inst,	"cndAll_inst" },
    { cndAll_time,	"cndAll_time" },
    { cndAnd_1_1,	"cndAnd_1_1" },
    { cndAnd_1_n,	"cndAnd_1_n" },
    { cndAnd_n_1,	"cndAnd_n_1" },
    { cndAnd_n_n,	"cndAnd_n_n" },
    { cndAvg_host,	"cndAvg_host" },
    { cndAvg_inst,	"cndAvg_inst" },
    { cndAvg_time,	"cndAvg_time" },
    { cndCount_host,	"cndCount_host" },
    { cndCount_inst,	"cndCount_inst" },
    { cndCount_time,	"cndCount_time" },
    { cndDelay_1,	"cndDelay_1" },
    { cndDelay_n,	"cndDelay_n" },
    { cndDiv_1_1,	"cndDiv_1_1" },
    { cndDiv_1_n,	"cndDiv_1_n" },
    { cndDiv_n_1,	"cndDiv_n_1" },
    { cndDiv_n_n,	"cndDiv_n_n" },
    { cndEq_1_1,	"cndEq_1_1" },
    { cndEq_1_n,	"cndEq_1_n" },
    { cndEq_n_1,	"cndEq_n_1" },
    { cndEq_n_n,	"cndEq_n_n" },
    { cndFall_1,	"cndFall_1" },
    { cndFall_n,	"cndFall_n" },
    { cndFetch_1,	"cndFetch_1" },
    { cndFetch_all,	"cndFetch_all" },
    { cndFetch_n,	"cndFetch_n" },
    { cndGt_1_1,	"cndGt_1_1" },
    { cndGt_1_n,	"cndGt_1_n" },
    { cndGt_n_1,	"cndGt_n_1" },
    { cndGt_n_n,	"cndGt_n_n" },
    { cndGte_1_1,	"cndGte_1_1" },
    { cndGte_1_n,	"cndGte_1_n" },
    { cndGte_n_1,	"cndGte_n_1" },
    { cndGte_n_n,	"cndGte_n_n" },
    { cndLt_1_1,	"cndLt_1_1" },
    { cndLt_1_n,	"cndLt_1_n" },
    { cndLt_n_1,	"cndLt_n_1" },
    { cndLt_n_n,	"cndLt_n_n" },
    { cndLte_1_1,	"cndLte_1_1" },
    { cndLte_1_n,	"cndLte_1_n" },
    { cndLte_n_1,	"cndLte_n_1" },
    { cndLte_n_n,	"cndLte_n_n" },
    { cndMax_host,	"cndMax_host" },
    { cndMax_inst,	"cndMax_inst" },
    { cndMax_time,	"cndMax_time" },
    { cndMin_host,	"cndMin_host" },
    { cndMin_inst,	"cndMin_inst" },
    { cndMin_time,	"cndMin_time" },
    { cndMul_1_1,	"cndMul_1_1" },
    { cndMul_1_n,	"cndMul_1_n" },
    { cndMul_n_1,	"cndMul_n_1" },
    { cndMul_n_n,	"cndMul_n_n" },
    { cndNeg_1,		"cndNeg_1" },
    { cndNeg_n,		"cndNeg_n" },
    { cndNeq_1_1,	"cndNeq_1_1" },
    { cndNeq_1_n,	"cndNeq_1_n" },
    { cndNeq_n_1,	"cndNeq_n_1" },
    { cndNeq_n_n,	"cndNeq_n_n" },
    { cndNot_1,		"cndNot_1" },
    { cndNot_n,		"cndNot_n" },
    { cndOr_1_1,	"cndOr_1_1" },
    { cndOr_1_n,	"cndOr_1_n" },
    { cndOr_n_1,	"cndOr_n_1" },
    { cndOr_n_n,	"cndOr_n_n" },
    { cndPcnt_host,	"cndPcnt_host" },
    { cndPcnt_inst,	"cndPcnt_inst" },
    { cndPcnt_time,	"cndPcnt_time" },
    { cndRate_1,	"cndRate_1" },
    { cndRate_n,	"cndRate_n" },
    { cndInstant_1,	"cndInstant_1" },
    { cndInstant_n,	"cndInstant_n" },
    { cndRise_1,	"cndRise_1" },
    { cndRise_n,	"cndRise_n" },
    { cndSome_host,	"cndSome_host" },
    { cndSome_inst,	"cndSome_inst" },
    { cndSome_time,	"cndSome_time" },
    { cndSub_1_1,	"cndSub_1_1" },
    { cndSub_1_n,	"cndSub_1_n" },
    { cndSub_n_1,	"cndSub_n_1" },
    { cndSub_n_n,	"cndSub_n_n" },
    { cndSum_host,	"cndSum_host" },
    { cndSum_inst,	"cndSum_inst" },
    { cndSum_time,	"cndSum_time" },
    { rule,		"rule" },
    { NULL,		NULL },
};

static struct {
    int			val;
    char		*name;
} sem_map[] = {
    { SEM_UNKNOWN,	"UNKNOWN" },
    { SEM_NUMVAR,	"NUMVAR" },
    { SEM_NUMCONST,	"NUMCONST" },
    { SEM_BOOLEAN,	"TRUTH" },
    { SEM_CHAR,		"CHAR" },
    { SEM_REGEX,	"REGEX" },
    { PM_SEM_COUNTER,	"COUNTER" },
    { PM_SEM_INSTANT,	"INSTANT" },
    { PM_SEM_DISCRETE,	"DISCRETE" },
    { 0,		NULL },
};

void
__dumpExpr(int level, Expr *x)
{
    int		i;
    int		j;
    int		k;

    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "Expr dump @ " PRINTF_P_PFX "%p\n", x);
    if (x == NULL) return;
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  op=%d (%s) arg1=" PRINTF_P_PFX "%p arg2=" PRINTF_P_PFX "%p parent=" PRINTF_P_PFX "%p\n",
	x->op, opStrings(x->op), x->arg1, x->arg2, x->parent);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  eval=");
    for (j = 0; fn_map[j].addr; j++) {
	if (x->eval == fn_map[j].addr) {
	    fprintf(stderr, "%s", fn_map[j].name);
	    break;
	}
    }
    if (fn_map[j].addr == NULL)
	fprintf(stderr, "" PRINTF_P_PFX "%p()", x->eval);
    fprintf(stderr, " metrics=" PRINTF_P_PFX "%p ring=" PRINTF_P_PFX "%p\n", x->metrics, x->ring);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  valid=%d cardinality[H,I,T]=[%d,%d,%d] tspan=%d\n",
	x->valid, x->hdom, x->e_idom, x->tdom, x->tspan);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "  nsmpls=%d nvals=%d sem=", x->nsmpls, x->nvals);
    for (j = 0; sem_map[j].name; j++) {
	if (x->sem == sem_map[j].val) {
	    fprintf(stderr, "%s", sem_map[j].name);
	    break;
	}
    }
    if (sem_map[j].name == NULL)
	fprintf(stderr, "%d", x->sem);
    if (UNITS_UNKNOWN(x->units))
	fprintf(stderr, " units=UNKNOWN\n");
    else
	fprintf(stderr, " units=%s\n", pmUnitsStr(&x->units));
    if (x->valid > 0) {
	if (x->sem == SEM_BOOLEAN || x->sem == SEM_CHAR ||
	    x->sem == SEM_NUMVAR || x->sem == SEM_NUMCONST ||
	    x->sem == PM_SEM_COUNTER || x->sem == PM_SEM_INSTANT ||
	    x->sem == PM_SEM_DISCRETE) {
	    for (j = 0; j < x->nsmpls; j++) {
		for (i = 0; i < level; i++) fprintf(stderr, ".. ");
		fprintf(stderr, "  smpls[%d].ptr " PRINTF_P_PFX "%p ", j, x->smpls[j].ptr);
		for (k = 0; k < x->tspan; k++) {
		    if (x->tspan > 1 && x->sem != SEM_CHAR) {
			if (k > 0)
			    fprintf(stderr, ", ");
			fprintf(stderr, "{%d} ", k);
		    }
		    if (x->sem == SEM_BOOLEAN) {
			char 	c = *((char *)x->smpls[j].ptr+k);
			if ((int)c == B_TRUE)
			    fprintf(stderr, "true");
			else if ((int)c == B_FALSE)
			    fprintf(stderr, "false");
			else if ((int)c == B_UNKNOWN)
			    fprintf(stderr, "unknown");
			else
			    fprintf(stderr, "bogus (0x%x)", c & 0xff);
		    }
		    else if (x->sem == SEM_CHAR) {
			if (k == 0)
			    fprintf(stderr, "\"%s\"", (char *)x->smpls[j].ptr);
		    }
		    else {
			double	v = *((double *)x->smpls[j].ptr+k);
			int		fp_bad = 0;
#ifdef HAVE_FPCLASSIFY
			fp_bad = fpclassify(v) == FP_NAN;
#else
#ifdef HAVE_ISNAN
			fp_bad = isnan(v);
#endif
#endif
			if (fp_bad)
			    fputc('?', stderr);
			else
			    fprintf(stderr, "%g", v);
		    }
		}
		fputc('\n', stderr);
	    }
	}
	else if (x->sem == SEM_REGEX) {
	    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
	    fprintf(stderr, "  handle=" PRINTF_P_PFX "%p\n", x->ring);
	}
    }
}

void
__dumpMetric(int level, Metric *m)
{
    int		i;
    int		j;
    int		numinst;

    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "Metric dump @ " PRINTF_P_PFX "%p\n", m);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "expr=" PRINTF_P_PFX "%p profile=" PRINTF_P_PFX "%p host=" PRINTF_P_PFX "%p next=" PRINTF_P_PFX "%p prev=" PRINTF_P_PFX "%p\n",
	m->expr, m->profile, m->host, m->next, m->prev);
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fprintf(stderr, "metric=%s host=%s conv=%g specinst=%d m_indom=%d\n",
	symName(m->mname), symName(m->hname), m->conv, m->specinst, m->m_idom);
    if (m->desc.indom != PM_INDOM_NULL) {
	numinst =  m->specinst == 0 ? m->m_idom : m->specinst;
	for (j = 0; j < numinst; j++) {
	    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
	    fprintf(stderr, "[%d] iid=", j);
	    if (m->iids[j] == PM_IN_NULL) 
		fprintf(stderr, "?missing");
	    else
		fprintf(stderr, "%d", m->iids[j]);
	    fprintf(stderr, " iname=\"%s\"\n", m->inames[j]);
	}
    }
    for (i = 0; i < level; i++) fprintf(stderr, ".. ");
    fputc('\n', stderr);

#if 0
    pmDesc          desc;       /* pmAPI metric description */
    RealTime	    stamp;	/* time stamp for current values */
    pmValueSet	    *vset;	/* current values */
    RealTime	    stomp;	/* previous time stamp for rate calculation */
    double	    *vals;	/* vector of values for rate computation */
    int		    offset;	/* offset within sample in expr ring buffer */
... Metric;
#endif

}


void
__dumpTree(int level, Expr *x)
{
    __dumpExpr(level, x);
    if (x->arg1 != NULL) __dumpTree(level+1, x->arg1);
    if (x->arg2 != NULL) __dumpTree(level+1, x->arg2);
}

void
dumpTree(Expr *x)
{
    __dumpTree(0, x);
}

void
dumpRules(void)
{
    Task	*t;
    Symbol	*s;
    int		i;

    for (t = taskq; t != NULL; t = t->next) {
	s = t->rules;
	for (i = 0; i < t->nrules; i++, s++) {
	    fprintf(stderr, "\nRule: %s\n", symName(*s));
	    dumpTree((Expr *)symValue(*s));
	}
    }
}

void
dumpExpr(Expr *x)
{
    __dumpExpr(0, x);
}

void
dumpMetric(Metric *m)
{
    __dumpMetric(0, m);
}

void
dumpTask(Task *t)
{
    int	i;
    fprintf(stderr, "Task dump @ " PRINTF_P_PFX "%p\n", t);
    fprintf(stderr, "  nth=%d delta=%.3f tick=%d next=" PRINTF_P_PFX "%p prev=" PRINTF_P_PFX "%p\n", t->nth, t->delta, t->tick, t->next, t->prev);
    fprintf(stderr, "  eval time: ");
    showFullTime(stderr, t->eval);
    fputc('\n', stderr);
    fprintf(stderr, "  retry time: ");
    showFullTime(stderr, t->retry);
    fputc('\n', stderr);
    if (t->hosts == NULL)
	fprintf(stderr, "  host=<null>\n");
    else
	fprintf(stderr, "  host=%s (%s)\n", symName(t->hosts->name), t->hosts->down ? "down" : "up");
    fprintf(stderr, "  rules:\n");
    for (i = 0; i < t->nrules; i++) {
	fprintf(stderr, "    %s\n", symName(t->rules[i]));
    }
}
