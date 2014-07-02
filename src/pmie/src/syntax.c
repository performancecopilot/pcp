/***********************************************************************
 * syntax.c - inference rule language parser
 *********************************************************************** 
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2013 Red Hat, Inc.
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
#include "pmapi.h"
#include "impl.h"

#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include "dstruct.h"
#include "symbol.h"
#include "syntax.h"
#include "lexicon.h"
#include "grammar.h"
#include "pragmatics.h"
#include "eval.h"
#include "show.h"



/***********************************************************************
 * constants
 ***********************************************************************/

#define NAMEGEN_MAX 12



/***********************************************************************
 * variables
 ***********************************************************************/

Symbol	parse;			/* result of parse */
int	errs;			/* error count */


/***********************************************************************
 * miscellaneous local functions
 ***********************************************************************/

/* generate unique name, only good till next call */
static char *
nameGen(void)
{
    static int  state = 0;
    static char bfr[NAMEGEN_MAX];

    state++;
    snprintf(bfr, sizeof(bfr), "expr_%1d", state);

    return bfr;
}


/* check domains cardinality agreement */
static int
checkDoms(Expr *x1, Expr *x2)
{
    if ((x1->nvals != 1) && (x2->nvals != 1)) {
	if (x1->hdom != x2->hdom) {
	    synerr();
	    fprintf(stderr, "host domains have different size (%d and %d)\n",
			x1->hdom, x2->hdom);
	    return 0;
	}
	if (x1->e_idom != x2->e_idom) {
	    synerr();
	    fprintf(stderr, "instance domains have different size (%d and %d)\n",
			x1->e_idom, x2->e_idom);
	    return 0;
	}
	if (x1->tdom != x2->tdom) {
	    synerr();
	    fprintf(stderr, "time domains have different size (%d and %d)\n",
			x1->tdom, x2->tdom);
	    return 0;
	}
    }
    return 1;
}


/* evaluate constant expression */
static void
evalConst(Expr *x)
{
    if ((x->op < ACT_SHELL) &&
	(x->arg1->op == NOP) &&
	((x->arg2 == NULL) || (x->arg2->op == NOP))) {
	(x->eval)(x);
	x->op = NOP;
	x->eval = NULL;
	freeExpr(x->arg1);  x->arg1 = NULL;
	freeExpr(x->arg2);  x->arg2 = NULL;
    }
}



/***********************************************************************
 * error reporting
 ***********************************************************************/

static void
report(char *msg)
{
    LexIn *x = lin;

    fprintf(stderr, "%s: %s - ", pmProgname, msg);
    if (x) {
        fprintf(stderr, "near line %d of ", x->lno);
        if (x->stream) {
	    if (x->name) fprintf(stderr, "file %s", x->name);
	    else fprintf(stderr, "standard input");
        }
	else {
	    fprintf(stderr, "macro $%s", x->name);
	    do x = x->prev;
	    while (x && x->stream == NULL);
	    if (x) {
		fprintf(stderr, " called from ");
		if (x->name) fprintf(stderr, "file %s", x->name);
		else fprintf(stderr, "standard input");
	    }
	}
    }
    else
	fprintf(stderr, "at end of file");
    putc('\n', stderr);
}

void
synwarn(void)
{
    report("warning");
}

void
synerr(void)
{
    report("syntax error");
    errs++;
}

void
yyerror(char *s)
{
    synerr();
}


/***********************************************************************
 * post processing
 * - propagate instance information from fetch expressions towards
 *   the root of the expression tree, allocating ring buffers etc.
 *   along the way.
 ***********************************************************************/

static void
postExpr(Expr *x)
{
    if (x->op == CND_FETCH) {
	instFetchExpr(x);
    }
    else {
	if (x->arg1) {
	    postExpr(x->arg1);
	    if (x->arg2)
		postExpr(x->arg2);
	}
    }
}



/***********************************************************************
 * parser actions
 ***********************************************************************/

/* statement */
Symbol
statement(char *name, Expr *x)
{
    Symbol   s;

    /* error guard */
    if (x == NULL) return NULL;

    /* if name not given, make one up */
    if (name == NULL) name = nameGen();

    /* the parsed object is a rule (expression to evaluate) */
    if (x->op != NOP) {
	if (symLookup(&rules, name)) {
	    synerr();
	    fprintf(stderr, "rule \"%s\" multiply defined\n", name);
	    freeExpr(x);
	    return NULL;
	}
	else {
	    if (errs == 0) {
		postExpr(x);
		s = symIntern(&rules, name);
	    }
	    else return NULL;
	}
    }

    /* the parsed object is a non-rule */
    else {
	if ( (s = symLookup(&vars, name)) )
	    freeExpr(symValue(s));
	else
	    s = symIntern(&vars, name);
    }

    symValue(s) = x;
    return s;
}


/* construct rule Expr */
Expr *
ruleExpr(Expr *cond, Expr *act)
{
    Expr    *x;

    if (cond == NULL) return act;
    if (act == NULL) return cond;

    if (cond->nvals != 1) {
        synerr();
        fprintf(stderr, "rule condition must have a single boolean value\n");
    }

    perf->numrules++;

    x = newExpr(RULE, cond, act, -1, -1, -1, 1, SEM_BOOLEAN);
    newRingBfr(x);
    findEval(x);
    return x;
}


/* action expression */
Expr *
actExpr(int op, Expr *arg1, Expr *arg2)
{
    Expr    *x;

    /* error guard */
    if (arg1 == NULL) return NULL;

    /* construct expression node */
    x = newExpr(op, arg1, arg2, -1, -1, -1, 1, SEM_BOOLEAN);
    newRingBfr(x);
    findEval(x);

    return x;
}


/* action argument expression */
Expr *
actArgExpr(Expr *arg1, Expr *arg2)
{
    Expr    *x;

    /* error guard */
    if (arg1 == NULL) return NULL;

    /* construct expression node */
    x = newExpr(ACT_ARG, arg1, arg2, -1, 1, -1, 1, SEM_CHAR);
    newRingBfr(x);
    findEval(x);

    return x;
}

/* action argument */
Expr *
actArgList(Expr *arg1, char *str)
{
    Expr    *x;

    /* construct expression node for an action argument string */
    x = (Expr *) zalloc(sizeof(Expr));
    x->op = NOP;
    x->smpls[0].ptr = x->ring = sdup(str);
    x->valid = x->nsmpls = x->nvals = 1;
    x->tspan = strlen(str);
    x->sem = SEM_CHAR;
    if (arg1) {
	x->arg1 = arg1;
	arg1->parent = x;
    }

    return x;
}

/* relational operator expression */
Expr *
relExpr(int op, Expr *arg1, Expr *arg2)
{
    Expr    	*x;
    Expr    	*arg;
    int		i;
    int		sts;

    /* error guard */
    if (arg1 == NULL || arg2 == NULL) return NULL;

    /* check domains */
    sts = checkDoms(arg1, arg2);

    /* decide primary argument for inheritance of Expr attributes */
    arg = primary(arg1, arg2);

    /* construct expression node */
    x = newExpr(op, arg1, arg2, arg->hdom, arg->e_idom, arg->tdom, abs(arg->tdom), SEM_BOOLEAN);
#if PCP_DEBUG
    if (sts == 0 && (pmDebug & DBG_TRACE_APPL1)) {
	fprintf(stderr, "relExpr: checkDoms(" PRINTF_P_PFX "%p, " PRINTF_P_PFX "%p) failed ...\n", arg1, arg2);
	__dumpTree(1, x);
    }
#endif
    newRingBfr(x);
    if (x->tspan > 0) {
	for (i = 0; i < x->nsmpls; i++)
	    *((char *)x->ring + i) = B_UNKNOWN;
    }
    findEval(x);

    /* evaluate constant expression now */
    evalConst(x);

    return x;
}


/* binary operator expression */
Expr *
binaryExpr(int op, Expr *arg1, Expr *arg2)
{
    Expr	*x;
    Expr	*arg = arg1;
    int		sts = 0;

    /* error guard */
    if (arg1 == NULL) return NULL;

    if (arg1 != NULL && arg2 != NULL) {
	if (op != CND_MATCH && op != CND_NOMATCH) {
	    /* check domains */
	    sts = checkDoms(arg1, arg2);

	    /* decide primary argument for inheritance of Expr attributes */
	    arg = primary(arg1, arg2);
	}
	else {
	    regex_t	*pat;

	    pat = alloc(sizeof(*pat));
	    if (regcomp(pat, (char *)arg2->ring, REG_EXTENDED|REG_NOSUB) != 0) {
		/* bad pattern */
		fprintf(stderr, "illegal regular expression \"%s\"\n", (char *)arg2->ring);
		free(pat);
		return NULL;
	    }
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL1) {
		fprintf(stderr, "binaryExpr: regex=\"%s\" handle=" PRINTF_P_PFX "%p\n", (char *)arg2->ring, pat);
	    }
#endif
	    /*
	     * change operand from the string form of the pattern to the
	     * compiled regex
	     */
	    free(arg2->ring);
	    arg2->tspan = 1;
	    arg2->ring = pat;
	    arg2->sem = SEM_REGEX;
	    sts = 1;
	}
    }

    /* construct expression node */
    x = newExpr(op, arg1, arg2, arg->hdom, arg->e_idom, arg->tdom, abs(arg->tdom), arg->sem);
#if PCP_DEBUG
    if (sts == 0 && (pmDebug & DBG_TRACE_APPL1)) {
	fprintf(stderr, "binaryExpr: checkDoms(" PRINTF_P_PFX "%p, " PRINTF_P_PFX "%p) failed ...\n", arg1, arg2);
	__dumpTree(1, x);
    }
#endif
    newRingBfr(x);
    findEval(x);

    /* evaluate constant expression now */
    evalConst(x);

    return x;
}


/* unary operator expression */
Expr *
unaryExpr(int op, Expr *arg)
{
    Expr    *x;

    /* error guard */
    if (arg == NULL) return NULL;

    /* construct expression node */
    x = newExpr(op, arg, NULL, arg->hdom, arg->e_idom, arg->tdom, abs(arg->tdom), arg->sem);
    newRingBfr(x);
    findEval(x);

    /* evaluate constant expression now */
    evalConst(x);

    return x;
}


/* aggregation/quantification operator expression */
Expr *
domainExpr(int op, int dom, Expr *arg)
{
    Expr    *x;
    int	    hdom;
    int	    idom;
    int	    tdom;

    /* error guard */
    if (arg == NULL) return NULL;

    hdom = arg->hdom;
    idom = arg->e_idom;
    tdom = arg->tdom;

    switch (dom) {
    case HOST_DOM:
        if (hdom == -1) {
            synerr();
            fprintf(stderr, "no host domain\n");
	    freeExpr(arg);
	    return NULL;
        }
        hdom = -1;
	idom = -1;
	dom = 0;
        break;
    case INST_DOM:
#if 0
	/*
	 * I believe this test is no longer correct ... the instance
	 * domain may be unobtainable at this point, or may change
	 * later so checking at the syntactic level is not helpful
	 */
        if (idom == -1) {
            synerr();
            fprintf(stderr, "no instance domain\n");
	    freeExpr(arg);
	    return NULL;
        }
#endif
        idom = -1;
	dom = 1;
        break;
    case TIME_DOM:
        if (tdom == -1) {
            synerr();
            fprintf(stderr, "no time domain\n");
	    freeExpr(arg);
	    return NULL;
        }
        tdom = -1;
	dom = 2;
    }

    if (op == CND_COUNT_HOST) {
	x = newExpr(op + dom, arg, NULL, hdom, idom, tdom, abs(tdom), PM_SEM_INSTANT);
	newRingBfr(x);
	x->units = countUnits;
    }
    else {
	x = newExpr(op + dom, arg, NULL, hdom, idom, tdom, abs(tdom), arg->sem);
	newRingBfr(x);
    }

    findEval(x);
    return x;
}


/* percentage quantifier */
Expr *
percentExpr(double pcnt, int dom, Expr *arg)
{
    Expr    *x;

    /* error guard */
    if (arg == NULL) return NULL;

    x = domainExpr(CND_PCNT_HOST, dom, arg);
    x->arg2 = newExpr(NOP, NULL, NULL, -1, -1, -1, 1, SEM_NUMCONST);
    newRingBfr(x->arg2);
    *(double *)x->arg2->ring = pcnt / 100.0;
    x->arg2->valid = 1;
    return x;
}


/* merge expression */
static Expr *
mergeExpr(int op, Expr *arg)
{
    Expr    *x;

    /* force argument to buffer at least two samples */
    if (arg->nsmpls < 2)
        changeSmpls(&arg, 2);

    /* construct expression node */
    x = newExpr(op, arg, NULL, arg->hdom, arg->e_idom, arg->tdom, abs(arg->tdom), arg->sem);
    newRingBfr(x);
    findEval(x);
    return x;
}


/* numeric merge expression */
Expr *
numMergeExpr(int op, Expr *arg)
{
    /* error guard */
    if (arg == NULL) return NULL;

    /* check argument semantics */
    if ((arg->sem == SEM_BOOLEAN) || (arg->sem == SEM_CHAR)) {
	synerr();
	fprintf(stderr, "operator \"%s\" requires numeric valued argument\n", opStrings(op));
	return NULL;
    }

    return mergeExpr(op, arg);
}


/* boolean merge expression */
Expr *
boolMergeExpr(int op, Expr *arg)
{
    /* error guard */
    if (arg == NULL) return NULL;

    /* check argument semantics */
    if (arg->sem != SEM_BOOLEAN) {
	synerr();
	fprintf(stderr, "operator \"%s\" requires boolean valued argument\n", opStrings(op));
	return NULL;
    }

    return mergeExpr(op, arg);
}


/* fetch expression */
Expr *
fetchExpr(char *mname,
          StringArray hnames,
          StringArray inames,
          Interval times)
{
    Expr        *x;
    Metric      *marr, *m;
    int         fsz, dsz;
    int         sum;
    int         i;

    /* calculate samplecounts for fetch and delay */
    if (times.t1 == 0) {
        fsz = times.t2 - times.t1 + 1;
        dsz = 0;
    }
    else {
        fsz = times.t1 + 1;
        dsz = times.t2 - times.t1 + 1;
    }

    /* default host */
    if (hnames.n == 0) {
	hnames.n = 1;
    }

    /* construct Metrics array */
    marr = m = (Metric *) zalloc(hnames.n * sizeof(Metric));
    sum = 0;
    for (i = 0; i < hnames.n; i++) {
        m->mname = symIntern(&metrics, mname);
	if (hnames.ss) m->hname = symIntern(&hosts, hnames.ss[i]);
	else m->hname = symIntern(&hosts, dfltHostName);
	m->desc.sem = SEM_UNKNOWN;
	m->m_idom = -1;
	if (inames.n > 0) {
	    m->specinst = inames.n;
	    m->iids = alloc(inames.n * sizeof(int));
	    m->inames = inames.ss;
	}
	else {
	    m->specinst = 0;
	    m->iids = NULL;
	    m->inames = NULL;
	}
        if (errs == 0) {
	    int		sts = initMetric(m);
	    if (sts < 0) errs++;
	    if (m->m_idom > 0)
		sum += m->m_idom;
	}
        m++;
    }
    if (sum == 0)
	sum = -1;

    /* error exit */
    if (errs) {
	m = marr;
	for (i = 0; i < hnames.n; i++) {
	    if (m->iids) free(m->iids);
	    m++;
	}
	free(marr);
	return NULL;
    }

    /* construct fetch node */
    x = newExpr(CND_FETCH, NULL, NULL, hnames.n, sum, fsz, fsz, SEM_UNKNOWN);
    newRingBfr(x);
    x->metrics = marr;
    findEval(x);
    instFetchExpr(x);

    /* patch in fetch node reference in each Metric */
    m = marr;
    for (i = 0; i < hnames.n; i++) {
	m->expr = x;
	m++;
    }

    /* construct delay node */
    if (dsz) {
        x = newExpr(CND_DELAY, x, NULL, x->hdom, x->e_idom, dsz, dsz, SEM_UNKNOWN);
	newRingBfr(x);
	findEval(x);
    }
    return x;
}


/* numeric constant */
Expr *
numConst(double v, pmUnits u)
{
    Expr    *x;

    x = newExpr(NOP, NULL, NULL, -1, -1, -1, 1, SEM_NUMCONST);
    newRingBfr(x);
    x->units = canon(u);
    x->valid = 1;
    *(double *) x->ring = scale(u) * v;
    return x;
}


/* string constant */
Expr *
strConst(char *s)
{
    Expr    *x;
    int	    n = (int) strlen(s) + 1;

    x = newExpr(NOP, NULL, NULL, -1, -1, -1, 1, SEM_CHAR);
    x->valid = 1;
    x->tspan = n;
    newRingBfr(x);
    strcpy((char *)x->ring, s);
    return x;
}


/* boolean constant */
Expr *
boolConst(Boolean v)
{
    Expr    *x;

    x = newExpr(NOP, NULL, NULL, -1, -1, -1, 1, SEM_BOOLEAN);
    newRingBfr(x);
    x->valid = 1;
    *(Boolean *) x->ring = v;
    return x;
}


/* numeric valued variable */
Expr *
numVar(Expr *x)
{
    if (x->sem == SEM_BOOLEAN || x->sem == SEM_CHAR) {
	synerr();
	fprintf(stderr, "numeric valued variable expected\n");
	return NULL;
    }
    return x;
}


/* truth valued variable */
Expr *
boolVar(Expr *x)
{
    if (x->sem == SEM_CHAR) {
	synerr();
	fprintf(stderr, "truth valued variable expected\n");
	return NULL;
    }
    return x;
}


/***********************************************************************
 * parser
 ***********************************************************************/

/* Initialization to be called at the start of new input file. */
int
synInit(char *fname)
{
    return lexInit(fname);
}


/* parse single statement */
Symbol
syntax(void)
{
    while (lexMore()) {
	errs = 0;
	parse = NULL;
	yyparse();
	if (parse) 
	    return parse;
    }
    lexFinal();
    return NULL;
}



