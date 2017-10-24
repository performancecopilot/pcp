/***********************************************************************
 * eval.c - task scheduling and expression evaluation
 ***********************************************************************
 *
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Red Hat
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <limits.h>
#include "dstruct.h"
#include "eval.h"
#include "fun.h"
#include "pragmatics.h"
#include "show.h"

/***********************************************************************
 * scheduling
 ***********************************************************************/

/* enter Task into task queue */
static void
enque(Task *t)
{
    Task	*q;

    q = taskq;
    if (q == NULL) {
	taskq = t;
	t->next = NULL;
	t->prev = NULL;
    }
    else {
	while (q) {
	    if (t->eval <= q->eval) {
		t->next = q;
		t->prev = q->prev;
		if (q->prev) q->prev->next = t;
		else taskq = t;
		q->prev = t;
		break;
	    }
	    if (q->next == NULL) {
		q->next = t;
		t->next = NULL;
		t->prev = q;
		break;
	    }
	    q = q->next;
	}
    }
}


/***********************************************************************
 * reconnect
 ***********************************************************************/

/* any hosts down or unavailable metrics in this task? */
static int
waiting(Task *t)
{
    Host        *h;

    h = t->hosts;
    while (h) {
	if (h->down || h->waits)
	    return 1;
	h = h->next;
    }
    return 0;
}

/*
 * state values
 *	STATE_INIT
 *	STATE_FAILINIT
 *	STATE_RECONN
 *	STATE_LOSTCONN
 */

typedef struct hstate {
    struct hstate	*next;
    char		*name;
    int			state;
} hstate_t;

static hstate_t	*host_map = NULL;

int
host_state_changed(const char *host, int state) /* NB: host == connection string */
{
    hstate_t	*hsp;

    for (hsp = host_map; hsp != NULL; hsp = hsp->next) {
	if (strcmp(host, hsp->name) == 0)
	    break;
    }

    if (hsp == NULL) {
	hsp = (hstate_t *)alloc(sizeof(*hsp));
	hsp->next = host_map;
	hsp->name = sdup(host);
	hsp->state = STATE_INIT;
	host_map = hsp;
    }

    if (state == hsp->state) return 0;

    if (quiet)
	; /* be quiet */
    else if (state == STATE_FAILINIT)
	__pmNotifyErr(LOG_INFO, "Cannot connect to pmcd %s\n", host);
    else if (state == STATE_RECONN && hsp->state != STATE_INIT)
	__pmNotifyErr(LOG_INFO, "Re-established connection to pmcd %s\n", host);
    else if (state == STATE_LOSTCONN)
	__pmNotifyErr(LOG_INFO, "Lost connection to pmcd %s\n", host);

    hsp->state = state;
    return 1;
}

/* try to reconnect to hosts and initialize missing metrics */
static void
enable(Task *t)
{
    Host	*h;
    Metric	*m;
    Metric	**p;

    h = t->hosts;
    while (h) {

	/* reconnect to host */
	if (h->down) {
	    if (reconnect(h)) {
		h->down = 0;
		host_state_changed(symName(h->conn), STATE_RECONN);
	    }
	}

	/* reinitialize waiting Metrics */
	if ((! h->down) && (h->waits)) {
	    p = &h->waits;
	    m = *p;
	    while (m) {
		switch (reinitMetric(m)) {
		case 1:
		    *p = m->next;
		    unwaitMetric(m);
		    bundleMetric(h,m);
		    break;
		case 0:
		    p = &m->next;
		    break;
		case -1:
		    *p = m->next;
		    m->next = h->duds;
		    h->duds = m;
		    break;
		}
		m = *p;
	    }
	}

	h = h->next;
    }

    if (waiting(t) == 0) {
	/* all clear now ... */
	t->epoch = t->eval;
	t->retry = 0;
    }
}


/***********************************************************************
 * evaluation
 ***********************************************************************/

int	showTimeFlag = 0;	/* set when -e used on the command line */

/* evaluate Task */
static void
eval(Task *task)
{
    Symbol	*s;
    pmValueSet  *vset;
    int		i;

    if (pmDebugOptions.appl2) {
	fprintf(stderr, "Evaluating task:\n");
	dumpTask(task);
    }

    /* fetch metrics */
    taskFetch(task);

    /* evaluate rule expressions */
    s = task->rules;
    for (i = 0; i < task->nrules; i++) {
	curr = symValue(*s);
	if (curr->op < NOP) {
	    (curr->eval)(curr);
	    perf->eval_actual++;
	}
	s++;
    }

    if (verbose) {

	/* send binary values */
	if (agent) {
	    int	sts;
	    s = task->rules;
	    for (i = 0; i < task->nrules; i++) {
		vset = task->rslt->vset[i];
		fillVSet(symValue(*s), vset);
		s++;
	    }
	    __pmOverrideLastFd(PDU_OVERRIDE2);
	    sts = __pmSendResult(STDOUT_FILENO, pmWhichContext(), task->rslt);
	    if (sts < 0) {
		fprintf(stderr, "Error: __pmSendResult to summary agent failed: %s\n", pmErrStr(sts));
		exit(0);
	    }

	}

        /* send values to applet */
        else if (applet) {
            s = task->rules;
            for (i = 0; i < task->nrules; i++) {
                showValue(stdout, symValue(*s));
                putchar(' ');
                s++;
            }
            putchar('\n');
        }

	/* print values in ASCII */
	else {
	    s = task->rules;
	    for (i = 0; i < task->nrules; i++) {
		printf("%s", symName(*s));
		if (archives || showTimeFlag) {
		    printf(" (");
		    showTime(stdout, now);
		    putchar(')');
		}
		printf(": ");
		switch (verbose) {
		case 1:
		    showValue(stdout, symValue(*s));
		    break;
		case 2:
		    showAnnotatedValue(stdout, symValue(*s));
		    break;
		case 3:
		    showSatisfyingValue(stdout, symValue(*s));
		    break;
		}
		putchar('\n');
		s++;
	    }
	    putchar('\n');
	}
    }
}


/* Mark expression as having invalid values */
void
clobber(Expr *x)
{
    int		i;
    Boolean	*t;
    double	*d;

    if (x->op < NOP) {
	if (x->arg1)
	    clobber(x->arg1);
	if (x->arg2)
	    clobber(x->arg2);
	x->valid = 0;
	/*
	 * numeric variable or variable?
	 */
	if (x->sem == PM_SEM_COUNTER ||
	    x->sem == PM_SEM_INSTANT || x->sem == PM_SEM_DISCRETE ||
	    x->sem == SEM_NUMVAR) {
	    d = (double *) x->ring;
	    for (i = 0; i < x->nvals; i++)
		*d++ = mynan;
	}
	else if (x->sem == SEM_BOOLEAN) {
	    t = (Boolean *) x->ring;
	    for (i = 0; i < x->nvals; i++)
		*t++ = B_UNKNOWN;
	}
    }
}


/***********************************************************************
 * exported functions
 ***********************************************************************/

/* fill in appropriate evaluator function for given Expr */
void
findEval(Expr *x)
{
    int		arity = 0;
    Metric	*m;

    /* 
     * arity values constructed from bit masks
     *	1	arg1 has tspan 1, and must always have one metric value
     *	2	arg2 has tspan 1, and must always have one metric value
     */
    if (x->arg1) {
	if (x->arg1->tspan == 1) {
	    for (m = x->arg1->metrics; m; m = m->next) {
		if (m->desc.indom == PM_INDOM_NULL) continue;
		if (m->specinst == 0) break;
	    }
	    if (m == NULL) arity |= 1;
	}
	else if (x->arg1->sem == SEM_CHAR) {
	    /* string-valued constant */
	    arity |= 1;
	}
    }
    if (x->arg2) {
	if (x->arg2->tspan == 1) {
	    for (m = x->arg2->metrics; m; m = m->next) {
		if (m->desc.indom == PM_INDOM_NULL) continue;
		if (m->specinst == 0) break;
	    }
	    if (m == NULL) arity |= 2;
	}
	else if (x->arg2->sem == SEM_CHAR) {
	    /* string-valued constant */
	    arity |= 2;
	}
    }

    /*
     * never come here with x->op == NULL or OP_VAR
     */
    switch (x->op) {

    case RULE:
	x->eval = rule;
	break;

    case CND_RULESET:
	x->eval = ruleset;
	break;

    case CND_FETCH:
	if (x->metrics->desc.indom == PM_INDOM_NULL ||
	    x->metrics->conv == 0)
	    x->eval = cndFetch_1;
	else if (x->metrics->specinst == 0)
	    x->eval = cndFetch_all;
	else
	    x->eval = cndFetch_n;
	break;

    case CND_SUM_HOST:
	x->eval = cndSum_host;
	break;

    case CND_SUM_INST:
	x->eval = cndSum_inst;
	break;

    case CND_SUM_TIME:
	x->eval = cndSum_time;
	break;

    case CND_AVG_HOST:
	x->eval = cndAvg_host;
	break;

    case CND_AVG_INST:
	x->eval = cndAvg_inst;
	break;

    case CND_AVG_TIME:
	x->eval = cndAvg_time;
	break;

    case CND_MAX_HOST:
	x->eval = cndMax_host;
	break;

    case CND_MAX_INST:
	x->eval = cndMax_inst;
	break;

    case CND_MAX_TIME:
	x->eval = cndMax_time;
	break;

    case CND_MIN_HOST:
	x->eval = cndMin_host;
	break;

    case CND_MIN_INST:
	x->eval = cndMin_inst;
	break;

    case CND_MIN_TIME:
	x->eval = cndMin_time;
	break;

    case CND_ALL_HOST:
	x->eval = cndAll_host;
	break;

    case CND_ALL_INST:
	x->eval = cndAll_inst;
	break;

    case CND_ALL_TIME:
	x->eval = cndAll_time;
	break;

    case CND_SOME_HOST:
	x->eval = cndSome_host;
	break;

    case CND_SOME_INST:
	x->eval = cndSome_inst;
	break;

    case CND_SOME_TIME:
	x->eval = cndSome_time;
	break;

    case CND_PCNT_HOST:
	x->eval = cndPcnt_host;
	break;

    case CND_PCNT_INST:
	x->eval = cndPcnt_inst;
	break;

    case CND_PCNT_TIME:
	x->eval = cndPcnt_time;
	break;

    case CND_COUNT_HOST:
	x->eval = cndCount_host;
	break;

    case CND_COUNT_INST:
	x->eval = cndCount_inst;
	break;

    case CND_COUNT_TIME:
	x->eval = cndCount_time;
	break;

    case ACT_SEQ:
	x->eval = actAnd;
	break;

    case ACT_ALT:
	x->eval = actOr;
	break;

    case ACT_SHELL:
	x->eval = actShell;
	break;

    case ACT_ALARM:
	x->eval = actAlarm;
	break;

    case ACT_STOMP:
	x->eval = actStomp;
	break;

    case ACT_SYSLOG:
	x->eval = actSyslog;
	break;

    case ACT_PRINT:
	x->eval = actPrint;
	break;

    case ACT_ARG:
	x->eval = actArg;
	break;

    case CND_DELAY:
	if (arity & 1)
	    x->eval = cndDelay_1;
	else
	    x->eval = cndDelay_n;
	break;

    case CND_RATE:
	if (arity & 1)
	    x->eval = cndRate_1;
	else
	    x->eval = cndRate_n;
	break;

    case CND_INSTANT:
	if (arity & 1)
	    x->eval = cndInstant_1;
	else
	    x->eval = cndInstant_n;
	break;

    case CND_NEG:
	if (arity & 1)
	    x->eval = cndNeg_1;
	else
	    x->eval = cndNeg_n;
	break;

    case CND_NOT:
	if (arity & 1)
	    x->eval = cndNot_1;
	else
	    x->eval = cndNot_n;
	break;

    case CND_RISE:
	if (arity & 1)
	    x->eval = cndRise_1;
	else
	    x->eval = cndRise_n;
	break;

    case CND_FALL:
	if (arity & 1)
	    x->eval = cndFall_1;
	else
	    x->eval = cndFall_n;
	break;

    case CND_ADD:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndAdd_1_1;
	    else
		x->eval = cndAdd_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndAdd_n_1;
	    else
		x->eval = cndAdd_n_n;
	}
	break;

    case CND_SUB:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndSub_1_1;
	    else
		x->eval = cndSub_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndSub_n_1;
	    else
		x->eval = cndSub_n_n;
	}
	break;

    case CND_MUL:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndMul_1_1;
	    else
		x->eval = cndMul_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndMul_n_1;
	    else
		x->eval = cndMul_n_n;
	}
	break;

    case CND_DIV:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndDiv_1_1;
	    else
		x->eval = cndDiv_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndDiv_n_1;
	    else
		x->eval = cndDiv_n_n;
	}
	break;

    case CND_EQ:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndEq_1_1;
	    else
		x->eval = cndEq_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndEq_n_1;
	    else
		x->eval = cndEq_n_n;
	}
	break;

    case CND_EQ_STR:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndEqStr_1_1;
	    else
		x->eval = cndEqStr_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndEqStr_n_1;
	    else
		x->eval = cndEqStr_n_n;
	}
	break;

    case CND_NEQ:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndNeq_1_1;
	    else
		x->eval = cndNeq_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndNeq_n_1;
	    else
		x->eval = cndNeq_n_n;
	}
	break;

    case CND_NEQ_STR:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndNeqStr_1_1;
	    else
		x->eval = cndNeqStr_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndNeqStr_n_1;
	    else
		x->eval = cndNeqStr_n_n;
	}
	break;

    case CND_LT:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndLt_1_1;
	    else
		x->eval = cndLt_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndLt_n_1;
	    else
		x->eval = cndLt_n_n;
	}
	break;

    case CND_LTE:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndLte_1_1;
	    else
		x->eval = cndLte_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndLte_n_1;
	    else
		x->eval = cndLte_n_n;
	}
	break;

    case CND_GT:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndGt_1_1;
	    else
		x->eval = cndGt_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndGt_n_1;
	    else
		x->eval = cndGt_n_n;
	}
	break;

    case CND_GTE:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndGte_1_1;
	    else
		x->eval = cndGte_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndGte_n_1;
	    else
		x->eval = cndGte_n_n;
	}
	break;

    case CND_AND:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndAnd_1_1;
	    else
		x->eval = cndAnd_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndAnd_n_1;
	    else
		x->eval = cndAnd_n_n;
	}
	break;

    case CND_OR:
	if (arity & 1) {
	    if (arity & 2)
		x->eval = cndOr_1_1;
	    else
		x->eval = cndOr_1_n;
	}
	else {
	    if (arity & 2)
		x->eval = cndOr_n_1;
	    else
		x->eval = cndOr_n_n;
	}
	break;

    case CND_MATCH:
    case CND_NOMATCH:
	x->eval = cndMatch_inst;
	break;

    case CND_OTHER:
	/* OTHER is not really evaluated in this sense, see ruleset() */
    	x->eval = NULL;
	break;

    default:
	__pmNotifyErr(LOG_ERR, "findEval: internal error: bad op (%d) %s\n", x->op, opStrings(x->op));
	dumpExpr(x);
	exit(1);
    }

    /* patch in fake actions for archive mode */
    if (archives &&
	(x->op == ACT_SHELL || x->op == ACT_ALARM || x->op == ACT_SYSLOG ||
	 x->op == ACT_PRINT || x->op == ACT_STOMP)) {
	x->eval = actFake;
    }
}


/* run evaluator */
void
run(void)
{
    Task	*t;

    /* empty task queue */
    if (taskq == NULL)
	return;

    inrun = 1;

    /* initialize task scheduling */
    t = taskq;
    while (t) {
	t->eval = t->epoch = start;
	if (waiting(t))
	    t->retry = RETRY;
	else
	    t->retry = 0;
	t->tick = 0;
	t = t->next;
    }

    /* evaluate and reschedule */
    t = taskq;
    for (;;) {
	now = t->eval;
	if (now > stop)
	    break;
	sleepTight(t);
	if (t->retry)
	    enable(t);
	reflectTime(t->delta);
	eval(t);
	if (waiting(t) && t->retry == 0) {
	    /* just failed host or metric availability */
	    t->retry = RETRY;
	}
	if (t->retry > 0) {
	    if (t->retry < t->delta) {
		/* exponential back-off, ... */
		t->eval = now + t->retry;
		t->retry *= 2;
	    }
	    else {
		/* ... capped at delta */
		t->eval = now + t->delta;
	    }
	}
	else {
	    /* regular eval, host and metrics available */
	    t->tick++;
	    t->eval = t->epoch + t->tick * t->delta;
	}
	taskq = t->next;
	if (taskq) taskq->prev = NULL;
	enque(t);
	t = taskq;
    }

    if (!quiet)
	__pmNotifyErr(LOG_INFO, "evaluator exiting\n");
}


/* invalidate all expressions being evaluated
   i.e. mark values as unknown */
void
invalidate(void)
{
    Task    *t;
    Expr    *x;
    Symbol  *s;
    int	    i;

    t = taskq;
    while (t) {
	s = t->rules;
	for (i = 0; i < t->nrules; i++) {
	    x = symValue(*s);
	    clobber(x);
	    s++;
	}
	t = t->next;
    }
}
