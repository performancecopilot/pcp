/***********************************************************************
 * show.c - display expressions and their values
 ***********************************************************************
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <math.h>
#include <ctype.h>
#include <assert.h>
#include "show.h"
#include "impl.h"
#include "dstruct.h"
#include "lexicon.h"
#include "pragmatics.h"
#if defined(HAVE_IEEEFP_H)
#include <ieeefp.h>
#endif

/***********************************************************************
 * local declarations
 ***********************************************************************/

static struct {
    int		op;
    char	*str;
} opstr[] = {
	{ RULE,	"->" },
	{ CND_FETCH,	"<fetch node>" },
	{ CND_DELAY,	"<delay node>" },
	{ CND_RATE,	"rate" },
	{ CND_INSTANT,	"instant" },
	{ CND_NEG,	"-" },
	{ CND_ADD,	"+" },
	{ CND_SUB,	"-" },
	{ CND_MUL,	"*" },
	{ CND_DIV,	"/" },
/* aggregation */
	{ CND_SUM_HOST,	"sum_host" },
	{ CND_SUM_INST,	"sum_inst" },
	{ CND_SUM_TIME,	"sum_sample" },
	{ CND_AVG_HOST,	"avg_host" },
	{ CND_AVG_INST,	"avg_inst" },
	{ CND_AVG_TIME,	"avg_sample" },
	{ CND_MAX_HOST,	"max_host" },
	{ CND_MAX_INST,	"max_inst" },
	{ CND_MAX_TIME,	"max_sample" },
	{ CND_MIN_HOST,	"min_host" },
	{ CND_MIN_INST,	"min_inst" },
	{ CND_MIN_TIME,	"min_sample" },
/* relational */
	{ CND_EQ,	"==" },
	{ CND_NEQ,	"!=" },
	{ CND_LT,	"<" },
	{ CND_LTE,	"<=" },
	{ CND_GT,	">" },
	{ CND_GTE,	">=" },
/* boolean */
	{ CND_NOT,	"!" },
	{ CND_RISE,	"rising" },
	{ CND_FALL,	"falling" },
	{ CND_AND,	"&&" },
	{ CND_OR,	"||" },
	{ CND_MATCH,	"match_inst" },
	{ CND_NOMATCH,	"nomatch_inst" },
	{ CND_RULESET,	"ruleset" },
	{ CND_OTHER,	"other" },
/* quantification */
	{ CND_ALL_HOST,	"all_host" },
	{ CND_ALL_INST,	"all_inst" },
	{ CND_ALL_TIME,	"all_sample" },
	{ CND_SOME_HOST,	"some_host" },
	{ CND_SOME_INST,	"some_inst" },
	{ CND_SOME_TIME,	"some_sample" },
	{ CND_PCNT_HOST,	"pcnt_host" },
	{ CND_PCNT_INST,	"pcnt_inst" },
	{ CND_PCNT_TIME,	"pcnt_sample" },
	{ CND_COUNT_HOST,	"count_host" },
	{ CND_COUNT_INST,	"count_inst" },
	{ CND_COUNT_TIME,	"count_sample" },
	{ ACT_SEQ,	"&" },
	{ ACT_ALT,	"|" },
	{ ACT_SHELL,	"shell" },
	{ ACT_ALARM,	"alarm" },
	{ ACT_SYSLOG,	"syslog" },
	{ ACT_PRINT,	"print" },
	{ ACT_STOMP,	"stomp" },
	{ ACT_ARG,	"<action arg node>" },
	{ NOP,		"<nop node>" },
	{ OP_VAR,	"<op_var node>" },
};

static int numopstr = sizeof(opstr) / sizeof(opstr[0]);

/***********************************************************************
 * local utility functions
 ***********************************************************************/

/* Concatenate string1 to existing string2 whose original length is given. */
static size_t	 /* new length of *string2 */
concat(char *string1, size_t pos, char **string2)
{
    size_t	slen;
    size_t	tlen;
    char	*cat;
    char	*dog;

    if ((slen = strlen(string1)) == 0)
	return pos;
    tlen = pos + slen;
    cat = (char *) ralloc(*string2, tlen + 1);
    dog = cat + pos;
    strcpy(dog, string1);
    dog += slen;
    *dog = '\0';

    *string2 = cat;
    return tlen;
}


/***********************************************************************
 * host and instance names
 ***********************************************************************/

/* Return host and instance name for nth value in expression *x */
static int
lookupHostInst(Expr *x, int nth, char **host, char **inst)
{
    Metric	*m = NULL;
    int		mi;
    int		sts = 0;
    int		pick = -1;
    int		matchaggr = 0;
    int		aggrop = NOP;
    double      *aggrval = NULL;
#if PCP_DEBUG
    static Expr	*lastx = NULL;
    int		dbg_dump = 0;
#endif

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	if (x != lastx) {
	    fprintf(stderr, "lookupHostInst(x=" PRINTF_P_PFX "%p, nth=%d, ...)\n", x, nth);
	    lastx = x;
	    dbg_dump = 1;
	}
    }
#endif
    if (x->op == CND_MIN_HOST || x->op == CND_MAX_HOST ||
        x->op == CND_MIN_INST || x->op == CND_MAX_INST ||
        x->op == CND_MIN_TIME || x->op == CND_MAX_TIME) {
	/*
	 * extrema operators ... value is here, but the host, instance, sample
	 * context is in the child expression ... go one level deeper and try
	 * to match the value
	 */
	aggrop = x->op;
	aggrval = (double *)x->smpls[0].ptr;
	matchaggr = 1;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "lookupHostInst look for extrema val=%f @ " PRINTF_P_PFX "%p\n", *aggrval, x);
	}
	x = x->arg1;
#endif
    }

    /* check for no host and instance available e.g. constant expression */
    if ((x->e_idom <= 0 && x->hdom <= 0) || ! x->metrics) {
	*host = NULL;
	*inst = NULL;
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "lookupHostInst(x=" PRINTF_P_PFX "%p, nth=%d, ...) -> %%h and %%i undefined\n", x, nth);
	}
#endif
	return sts;
    }

    /* find Metric containing the nth instance */
    if (matchaggr == 0) {
	pick = nth;
	mi = 0;
	for (;;) {
	    m = &x->metrics[mi];
#if PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_APPL2) && dbg_dump) {
		fprintf(stderr, "lookupHostInst: metrics[%d]\n", mi);
		dumpMetric(m);
	    }
#endif
	    if (pick < m->m_idom)
		break;
	    if (m->m_idom > 0)
		pick -= m->m_idom;
	    mi++;
	}
    }
    else {
	if (aggrop == CND_MIN_HOST || aggrop == CND_MAX_HOST) {
	    int		k;
#if PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_APPL2) && dbg_dump) {
		fprintf(stderr, "lookupHostInst [extrema_host]:\n");
	    }
#endif
	    for (k = 0; k < x->tspan; k++) {
#if DESPERATE
		fprintf(stderr, "smpls[0][%d]=%g\n", k, *((double *)x->smpls[0].ptr+k));
#endif
		if (*aggrval == *((double *)x->smpls[0].ptr+k)) {
		    m = &x->metrics[k];
		    goto done;
		}
	    }
	    fprintf(stderr, "Internal error: LookupHostInst: %s\n", opStrings(aggrop));
	}
	else if (aggrop == CND_MIN_INST || aggrop == CND_MAX_INST) {
	    int		k;
	    for (k = 0; k < x->tspan; k++) {
#if DESPERATE
		fprintf(stderr, "smpls[0][%d]=%g\n", k, *((double *)x->smpls[0].ptr+k));
#endif
		if (*aggrval == *((double *)x->smpls[0].ptr+k)) {
		    pick = k;
		    m = &x->metrics[0];
#if PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_APPL2) && dbg_dump) {
			fprintf(stderr, "lookupHostInst [extrema_inst]:\n");
			dumpMetric(m);
		    }
#endif
		    goto done;
		}
	    }
	    fprintf(stderr, "Internal error: LookupHostInst: %s\n", opStrings(aggrop));
	}
	else if (aggrop == CND_MIN_TIME || aggrop == CND_MAX_TIME) {
	    int		k;
	    for (k = 0; k < x->nsmpls; k++) {
#if DESPERATE
		fprintf(stderr, "smpls[%d][0]=%g\n", k, *((double *)x->smpls[k].ptr));
#endif
		if (*aggrval == *((double *)x->smpls[k].ptr)) {
		    pick = nth;
		    m = &x->metrics[0];
#if PCP_DEBUG
		    if ((pmDebug & DBG_TRACE_APPL2) && dbg_dump) {
			fprintf(stderr, "lookupHostInst [extrema_sample]:\n");
			dumpMetric(m);
		    }
#endif
		    goto done;
		}
	    }
	    fprintf(stderr, "Internal error: LookupHostInst: %s\n", opStrings(aggrop));
	}
    }

done:
    /* host and instance names */
    if (m == NULL) {
	*host = NULL;
	*inst = NULL;
    }
    else {
	*host = symName(m->hname);
	sts++;
	if (pick >= 0 && x->e_idom > 0 && m->inames) {
	    *inst = m->inames[pick];
	    sts++;
	}
	else
	    *inst = NULL;
    }

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "lookupHostInst(x=" PRINTF_P_PFX "%p, nth=%d, ...) -> sts=%d %%h=%s %%i=%s\n",
	    x, nth, sts, *host, *inst == NULL ? "undefined" : *inst);
    }
#endif

    return sts;
}


/***********************************************************************
 * expression value
 ***********************************************************************/

#define	BOOLEAN_SPACE 8

static size_t
showBoolean(Expr *x, int nth, size_t length, char **string)
{
    int		smpl;
    size_t	tlen;
    char	*cat;
    char	*dog;
    int		val;

    tlen = length + (x->nsmpls * BOOLEAN_SPACE);
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    for (smpl = 0; smpl < x->nsmpls; smpl++) {
	if (smpl > 0) {
	    strcpy(dog, " ");
	    dog += 1;
	}

	if (x->valid == 0) {
	    strncpy(dog, "unknown", BOOLEAN_SPACE);
	    dog += strlen("unknown");
	    continue;
	}

	val = *((char *)x->smpls[smpl].ptr + nth);
	if (val == B_FALSE) {
	    strncpy(dog, "false", BOOLEAN_SPACE);
	    dog += strlen("false");
	}
	else if (val == B_TRUE) {
	    strncpy(dog, "true", BOOLEAN_SPACE);
	    dog += strlen("true");
	}
	else if (val == B_UNKNOWN) {
	    strncpy(dog, "unknown", BOOLEAN_SPACE);
	    dog += strlen("unknown");
	}
	else {
	    sprintf(dog, "0x%02x?", val & 0xff);
	    dog += 5;
	}
    }
    *dog = '\0';

    *string = cat;
    return dog - cat;
}


static size_t
showString(Expr *x, size_t length, char **string)
{
    size_t	slen;
    size_t	tlen;
    char	*cat;
    char	*dog;

    slen = strlen((char *)x->smpls[0].ptr);
    tlen = length + slen + 2;
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    *dog++ = '"';
    strcpy(dog, (char *)x->smpls[0].ptr);
    dog += slen;
    *dog++ = '"';
    *dog = '\0';

    *string = cat;
    return tlen;
}

#define	DBL_SPACE 24

static size_t
showNum(Expr *x, int nth, size_t length, char **string)
{
    int		smpl;
    size_t	tlen;
    char	*cat;
    char	*dog;
    char	*fmt;
    double	v;
    double	abs_v;
    int		sts;

    tlen = length + (x->nsmpls * DBL_SPACE);
    cat = (char *)ralloc(*string, tlen + 1);
    dog = cat + length;
    for (smpl = 0; smpl < x->nsmpls; smpl++) {
	int	noval = 0;
	if (smpl > 0) {
	    strcpy(dog, " ");
	    dog++;
	}
	if (x->valid <= smpl)
	    noval = 1;
	else {
#ifdef HAVE_FPCLASSIFY
	    noval = fpclassify(*((double *)x->smpls[smpl].ptr + nth)) == FP_NAN;
#else
#ifdef HAVE_ISNAN
	    noval = isnan(*((double *)x->smpls[smpl].ptr + nth));
#endif
#endif
	}
	if (noval) {
	    if (x->sem == SEM_BOOLEAN) {
		strcpy(dog, "unknown");
		dog += strlen("unknown");
	    }
	    else {
		strcpy(dog, "?");
		dog++;
	    }
	}
	else {
	    v = *((double *)x->smpls[smpl].ptr+nth);
	    if (v == (int)v)
		sts = sprintf(dog, "%d", (int)v);
	    else {
		abs_v = v < 0 ? -v : v;
		if (abs_v < 0.5)
		    fmt = "%g";
		else if (abs_v < 5)
		    fmt = "%.2f";
		else if (abs_v < 50)
		    fmt = "%.1f";
		else
		    fmt = "%.0f";
		sts = sprintf(dog, fmt, v);
	    }
	    if (sts > 0)
		dog += sts;
	    else {
		strcpy(dog, "!");
		dog += 1;
	    }
	}
    }
    *dog = '\0';

    *string = cat;
    return dog - cat;
}

static char *
showConst(Expr *x)
{
    char	*string = NULL;
    size_t	length = 0;
    int		i;
    int		first = 1;

    /* construct string representation */
    if (x->nvals > 0) {
	for (i = 0; i < x->tspan; i++) {
	    if (first) 
		first = 0;
	    else
		length = concat(" ", length, &string);
	    if (x->sem == SEM_BOOLEAN)
		length = showBoolean(x, i, length, &string);
	    else if (x->sem == SEM_REGEX) {
		/* regex is compiled, cannot recover original string */
		length = concat("/<regex>/", length, &string);
	    }
	    else if (x->sem == SEM_CHAR) {
		length = showString(x, length, &string);
		/* tspan is string length, not an iterator in this case */
		break;
	    }
	    else
		length = showNum(x, i, length, &string);
	}
    }
    return string;
}



/***********************************************************************
 * expression syntax
 ***********************************************************************/

static void
showSyn(FILE *f, Expr *x)
{
    char	*s;
    char	*c;
    Metric	*m;
    char	**n;
    int		i;
    int		paren;

    if (x->op == NOP) {
	/* constant */
	s = showConst(x);
	if (s) {
	    c = s;
	    while(isspace((int)*c))
		c++;
	    fputs(c, f);
	    free(s);
	}
    }
    else if ((x->op == CND_FETCH) || (x->op == CND_DELAY)) {
	/* fetch expression (perhaps with delay) */
	m = x->metrics;
	fprintf(f, "%s", symName(m->mname));
	for (i = 0; i < x->hdom; i++) {
	    fprintf(f, " :%s", symName(m->hname));
	    m++;
	}
	m = x->metrics;
	if (m->inames) {
	    n = m->inames;
	    for (i = 0; i < m->m_idom; i++) {
		fprintf(f, " #%s", *n);
		n++;
	    }
	}
	if (x->op == CND_FETCH) {
	    if (x->tdom > 1)
		fprintf(f, " @0..%d", x->tdom - 1);
	}
	else {
	    if (x->tdom == x->arg1->tdom - 1) fprintf(f, " @%d", x->tdom);
	    else fprintf(f, " @%d..%d", x->tdom, x->tdom + x->arg1->tdom - 1);
	}
    }
    else if (x->arg1 && x->arg2) {
	/* binary operator */
	if (x->op == ACT_SHELL || x->op == ACT_ALARM || x->op == ACT_PRINT ||
	    x->op == ACT_STOMP) {
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    showSyn(f, x->arg2);
	    fputc(' ', f);
	    showSyn(f, x->arg1);
	}
	else if (x->op == ACT_ARG && x->parent->op == ACT_SYSLOG) {
	    int		*ip;
	    char	*cp;
	    ip = x->arg2->ring;
	    cp = (char *)&ip[1];
	    fprintf(f, "[level=%d tag=\"%s\"]", *ip, cp);
	    fputc(' ', f);
	    showSyn(f, x->arg1);
	}
	else if (x->op == CND_PCNT_HOST || x->op == CND_PCNT_INST || x->op == CND_PCNT_TIME) {
	    int		pcnt;
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    /*
	     * used to showSyn(f, x->arg2) here, but formatting is a little
	     * better if we punt on there being a single double representation
	     * of the % value at the end of arg2
	     */
	    pcnt = (int)(*((double *)x->arg2->smpls[0].ptr)*100+0.5);
	    fprintf(f, "%d%%", pcnt);
	    fputc(' ', f);
	    if (x->arg1->op == NOP || x->arg1->op == CND_DELAY || x->arg1->op == CND_FETCH)
		showSyn(f, x->arg1);
	    else {
		fputc('(', f);
		showSyn(f, x->arg1);
		fputc(')', f);
	    }
	}
	else if (x->op == CND_MATCH || x->op == CND_NOMATCH) {
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    showSyn(f, x->arg2);
	    fputc(' ', f);
	    fputc('(', f);
	    showSyn(f, x->arg1);
	    fputc(')', f);
	}
	else {
	    paren = 1 -
		    (x->arg1->op == NOP || x->arg1->op == CND_DELAY ||
		     x->arg1->op == CND_FETCH ||
		     x->arg1->op == CND_RATE || x->arg1->op == CND_INSTANT ||
		     x->arg1->op == CND_SUM_HOST || x->arg1->op == CND_SUM_INST ||
		     x->arg1->op == CND_SUM_TIME || x->arg1->op == CND_AVG_HOST ||
		     x->arg1->op == CND_AVG_INST || x->arg1->op == CND_AVG_TIME ||
		     x->arg1->op == CND_MAX_HOST || x->arg1->op == CND_MAX_INST ||
		     x->arg1->op == CND_MAX_TIME || x->arg1->op == CND_MIN_HOST ||
		     x->arg1->op == CND_MIN_INST || x->arg1->op == CND_MIN_TIME ||
		     x->arg1->op == CND_COUNT_HOST || x->arg1->op == CND_COUNT_INST ||
		     x->arg1->op == CND_COUNT_TIME ||
		     x->op == RULE);
	    if (paren)
		fputc('(', f);
	    showSyn(f, x->arg1);
	    if (paren)
		fputc(')', f);
	    fputc(' ', f);
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    paren = 1 -
		    (x->arg2->op == NOP || x->arg2->op == CND_DELAY ||
		     x->arg2->op == CND_FETCH ||
		     x->arg2->op == CND_RATE || x->arg2->op == CND_INSTANT ||
		     x->arg2->op == CND_SUM_HOST || x->arg2->op == CND_SUM_INST ||
		     x->arg2->op == CND_SUM_TIME || x->arg2->op == CND_AVG_HOST ||
		     x->arg2->op == CND_AVG_INST || x->arg2->op == CND_AVG_TIME ||
		     x->arg2->op == CND_MAX_HOST || x->arg2->op == CND_MAX_INST ||
		     x->arg2->op == CND_MAX_TIME || x->arg2->op == CND_MIN_HOST ||
		     x->arg2->op == CND_MIN_INST || x->arg2->op == CND_MIN_TIME ||
		     x->arg2->op == CND_COUNT_HOST || x->arg2->op == CND_COUNT_INST ||
		     x->arg2->op == CND_COUNT_TIME ||
		     x->op == RULE);
	    if (paren)
		fputc('(', f);
	    showSyn(f, x->arg2);
	    if (paren)
		fputc(')', f);
	}
    }
    else {
	/* unary operator */
	assert(x->arg1 != NULL);
	if (x->op == ACT_ARG) {
	    /* parameters for an action */
	    Expr	*y = x->arg1;
	    while (y != NULL) {
		if (y != x->arg1)
		    fputc(' ', f);
		showSyn(f, y);
		// fprintf(f, "\"%s\"", (char *)y->smpls[0].ptr);
		y = y->arg1;
	    }
	}
	else {
	    fputs(opStrings(x->op), f);
	    fputc(' ', f);
	    paren = 1 -
		    (x->arg1->op == ACT_SEQ || x->arg1->op == ACT_ALT ||
		     x->op == ACT_SHELL || x->op == ACT_ALARM ||
		     x->op == ACT_SYSLOG || x->op == ACT_PRINT ||
		     x->op == ACT_STOMP || x->op == CND_DELAY);
	    if (paren)
		fputc('(', f);
	    showSyn(f, x->arg1);
	    if (paren)
		fputc(')', f);
	}
    }
}

/*
 * recursive descent to find a conjunct from the root of
 * the expression that has associated metrics (not constants)
 */
static Expr *
findMetrics(Expr *y)
{
    Expr	*z;

    if (y == NULL) return NULL;
    if (y->metrics) return y;		/* success */

    /* give up if not a conjunct */
    if (y->op != CND_AND) return NULL;

    /* recurse left and then right */
    z = findMetrics(y->arg1);
    if (z != NULL) return z;
    return findMetrics(y->arg2);
}

/***********************************************************************
 * satisfying bindings and values
 ***********************************************************************/

/* Find sub-expression that reveals host and instance bindings
   that satisfy the given expression *x. */
static Expr *
findBindings(Expr *x)
{
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "call findBindings(x=" PRINTF_P_PFX "%p)\n", x);
    }
#endif

    if (x->metrics == NULL) {
	/*
	 * this Expr node has no metrics (involves only constants)
	 * ... try and find a conjunct at the top level that has
	 * associated metrics
	 */
	Expr	*y = findMetrics(x->arg1);
	if (y != NULL) x = y;
    }
    while (x->metrics && (x->e_idom <= 0 || x->hdom <= 0)) {
	if (x->op == CND_SUM_HOST || x->op == CND_SUM_INST || x->op == CND_SUM_TIME ||
	    x->op == CND_AVG_HOST || x->op == CND_AVG_INST || x->op == CND_AVG_TIME ||
	    x->op == CND_MAX_HOST || x->op == CND_MAX_INST || x->op == CND_MAX_TIME ||
	    x->op == CND_MIN_HOST || x->op == CND_MIN_INST || x->op == CND_MIN_TIME ||
	    x->op == CND_COUNT_HOST || x->op == CND_COUNT_INST || x->op == CND_COUNT_TIME) {
	    /*
	     * don't descend below an aggregation operator with a singular
	     * value, ... value you seek is right here
	     */
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "findBindings: found %s @ x=" PRINTF_P_PFX "%p\n", opStrings(x->op), x);
	    }
#endif
	    break;
	}
	if (x->arg1 && x->metrics == x->arg1->metrics) {
	    x = x->arg1;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "findBindings: try x->arg1=" PRINTF_P_PFX "%p\n", x);
	    }
#endif
	}
	else if (x->arg2) {
	    x = x->arg2;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "findBindings: try x->arg2=" PRINTF_P_PFX "%p\n", x);
	    }
#endif
	}
	else
	    break;
    }
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "findBindings finish @ " PRINTF_P_PFX "%p\n", x);
	dumpTree(x);
    }
#endif
    return x;
}


/* Find sub-expression that reveals the values that satisfy the
   given expression *x. */
static Expr *
findValues(Expr *x)
{
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "call findValues(x=" PRINTF_P_PFX "%p)\n", x);
    }
#endif
    while (x->sem == SEM_BOOLEAN && x->metrics) {
	if (x->metrics == x->arg1->metrics) {
	    x = x->arg1;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "findValues: try x->arg1=" PRINTF_P_PFX "%p\n", x);
	    }
#endif
	}
	else {
	    x = x->arg2;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "findValues: try x->arg2=" PRINTF_P_PFX "%p\n", x);
	    }
#endif
	}
    }
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "findValues finish @ " PRINTF_P_PFX "%p\n", x);
	dumpTree(x);
    }
#endif
    return x;
}


/***********************************************************************
 * format string
 ***********************************************************************/

/* Locate next %h, %i or %v in format string. */
static int	/* 0 -> not found, 1 -> host, 2 -> inst, 3 -> value */
findFormat(char *format, char **pos)
{
    for (;;) {
	if (*format == '\0')
	    return 0;
	if (*format == '%') {
	    switch (*(format + 1)) {
	    case 'h':
		*pos = format;
		return 1;
	    case 'i':
		*pos = format;
		return 2;
	    case 'v':
		*pos = format;
		return 3;
	    }
	}
	format++;
    }

}


/***********************************************************************
 * exported functions
 ***********************************************************************/

void
showSyntax(FILE *f, Symbol s)
{
    char *name = symName(s);
    Expr *x = symValue(s);

    fprintf(f, "%s =\n", name);
    showSyn(f, x);
    fprintf(f, ";\n\n");
}


void
showSubsyntax(FILE *f, Symbol s)
{
    char *name = symName(s);
    Expr *x = symValue(s);
    Expr *x1;
    Expr *x2;

    fprintf(f, "%s (subexpression for %s, %s and %s bindings) =\n",
	   name, "%h", "%i", "%v");
    x1 = findBindings(x);
    x2 = findValues(x1);
    showSyn(f, x2);
    fprintf(f, "\n\n");
}


/* Print value of expression */
void
showValue(FILE *f, Expr *x)
{
    char    *string = NULL;

    string = showConst(x);
    if (string) {
	fputs(string, f);
	free(string);
    }
    else
	fputs("?", f);
}


/* Print value of expression together with any host and instance bindings */
void
showAnnotatedValue(FILE *f, Expr *x)
{
    char    *string = NULL;
    size_t  length = 0;
    char    *host;
    char    *inst;
    int	    i;

    /* no annotation possible */
    if ((x->e_idom <= 0 && x->hdom <= 0) ||
	 x->sem == SEM_CHAR ||
	 x->metrics == NULL ||
	 x->valid == 0) {
	showValue(f, x);
	return;
    }

    /* construct string representation */
    for (i = 0; i < x->tspan; i++) {
	length = concat("\n    ", length, &string);
	lookupHostInst(x, i, &host, &inst);
	length = concat(host, length,  &string);
	if (inst) {
	    length = concat(": [", length,  &string);
	    length = concat(inst, length,  &string);
	    length = concat("] ", length,  &string);
	}
	else
	    length = concat(": ", length,  &string);
	if (x->sem == SEM_BOOLEAN)
	    length = showBoolean(x, i, length, &string);
	else	/* numeric value */
	    length = showNum(x, i, length, &string);
    }

    /* print string representation */
    if (string) {
	fputs(string, f);
	free(string);
    }
}


void
showTime(FILE *f, RealTime rt)
{
    time_t t = (time_t)rt;
    char   bfr[26];

    pmCtime(&t, bfr);
    bfr[24] = '\0';
    fprintf(f, "%s", bfr);
}


void
showFullTime(FILE *f, RealTime rt)
{
    time_t t = (time_t)rt;
    char   bfr[26];

    pmCtime(&t, bfr);
    bfr[24] = '\0';
    fprintf(f, "%s.%06d", bfr, (int)((rt-t)*1000000));
}


void
showSatisfyingValue(FILE *f, Expr *x)
{
    char    *string = NULL;
    size_t  length = 0;
    char    *host;
    char    *inst;
    int	    i;
    Expr    *x1;
    Expr    *x2;

    /* no satisfying values possible */
    if (x->metrics == NULL || x->valid == 0) {
	showValue(f, x);
	return;
    }

    if (x->sem != SEM_BOOLEAN) {
	showAnnotatedValue(f, x);
	return;
    }

    x1 = findBindings(x);
    x2 = findValues(x1);
    if (!x1->valid) {
	/*
	 * subexpression for %h, %i and %v is not valid but rule is
	 * true, return string without substitution ... rare case
	 * for <bad-or-not evaluated expr> || <true expr> rule
	 */
	concat(" <no bindings available>", length, &string);
	goto done;
    }

    /* construct string representation */
    for (i = 0; i < x1->tspan; i++) {
	if ((x1->sem == SEM_BOOLEAN && *((char *)x1->smpls[0].ptr + i) == B_TRUE)
	    || (x1->sem != SEM_BOOLEAN && x1->sem != SEM_UNKNOWN)) {
	    length = concat("\n    ", length, &string);
	    lookupHostInst(x1, i, &host, &inst);
	    length = concat(host, length,  &string);
	    if (inst) {
		length = concat(": [", length,  &string);
		length = concat(inst, length,  &string);
		length = concat("] ", length,  &string);
	    }
	    else
		length = concat(": ", length,  &string);
	    if (x2->sem == SEM_BOOLEAN)
		length = showBoolean(x2, i, length, &string);
	    else	/* numeric value */
		length = showNum(x2, i, length, &string);
	}
    }

done:
    /* print string representation */
    if (string) {
	fputs(string, f);
	free(string);
    }
}


/*
 * Instantiate format string for each satisfying binding and value
 * of the current rule ... enumerate and insert %h, %v and %v values
 *
 * WARNING: This is not thread safe, it dinks with the format string.
 */
size_t	/* new length of string */
formatSatisfyingValue(char *format, size_t length, char **string)
{
    char    *host;
    char    *inst;
    char    *first;
    char    *prev;
    char    *next;
    int     i;
    Expr    *x1;
    Expr    *x2;
    int	    sts1;
    int	    sts2;

    /* no formatting present? */
    if ((sts1 = findFormat(format, &first)) == 0)
	return concat(format, length, string);

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "formatSatisfyingValue: curr=" PRINTF_P_PFX "%p\n", curr);
	dumpExpr(curr);
    }
#endif
    x1 = findBindings(curr);
    x2 = findValues(x1);
    if (!x1->valid)
	/*
	 * subexpression for %h, %i and %v is not valid but rule is
	 * true, return string without substitution ... rare case
	 * for <bad-or-not evaluated expr> || <true expr> rule
	 */
	return concat(format, length, string);

    for (i = 0; i < x1->tspan; i++) {
	if ((x1->sem == SEM_BOOLEAN && *((char *)x1->smpls[0].ptr + i) == B_TRUE)
	    || (x1->sem != SEM_BOOLEAN && x1->sem != SEM_UNKNOWN)) {
	    prev = format;
	    next = first;
	    sts2 = sts1;
	    lookupHostInst(x1, i, &host, &inst);
	    do {
		*next = '\0';
		length = concat(prev, length, string);
		*next = '%';

		switch (sts2) {
		case 1:
		    if (host)
			length = concat(host, length, string);
		    else
			length = concat("<%h undefined>", length, string);
		    break;
		case 2:
		    if (inst)
			length = concat(inst, length, string);
		    else
			length = concat("<%i undefined>", length, string);
		    break;
		case 3:
		    if (x2->sem == SEM_BOOLEAN)
			length = showBoolean(x2, i, length, string);
		    else	/* numeric value */
			length = showNum(x2, i, length, string);
		    break;
		}
		prev = next + 2;
	    } while ((sts2 = findFormat(prev, &next)));
	    length = concat(prev, length, string);
	}
    }

    return length;
}

char *
opStrings(int op)
{
    int		i;
    /*
     * sizing of "eh" is a bit tricky ...
     * XXXXXXXXX is the number of digits in the largest possible value
     * for "op", to handle the default "<unknown op %d>" case, but also
     * "eh" must be long enough to accommodate the longest string from
     * opstr[i].str ... currently "<action arg node>"
     */
    static char	*eh = "<unknown op XXXXXXXXX>";

    for (i = 0; i < numopstr; i++) {
	if (opstr[i].op == op)
	    break;
    }

    if (i < numopstr)
	return opstr[i].str;
    else {
	sprintf(eh, "<unknown op %d>", op);
	return eh;
    }
}
