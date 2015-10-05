/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

/***********************************************************************
 * dstruct.h - central data structures snd associated operations
 ***********************************************************************/

#ifndef DSTRUCT_H
#define DSTRUCT_H

#include <stddef.h>
#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"
#include "symbol.h"
#include "stats.h"


/***********************************************************************
 * forward reference
 ***********************************************************************/

struct expr;
struct metric;
struct profile;
struct fetch;
struct host;
struct task;


/***********************************************************************
 * (Kleene) 3-valued boolean values
 ***********************************************************************/

typedef char Boolean;
#define B_FALSE 0
#define B_TRUE 1
#define B_UNKNOWN 2

extern double	mynan;	/* definitely not-a-number */


/***********************************************************************
 * time
 ***********************************************************************/

typedef unsigned int	TickTime;	/* time counted in deltas */

/* following in usec */
typedef double		RealTime;	/* wall clock time or interval */
#define MINUTE		60		/* one minute of real time */
#define DELAY_MAX	32		/* maximum initial evaluation delay */
#define RETRY		5		/* retry interval */
#define DELTA_DFLT	10		/* default sample interval */
#define DELTA_MIN	0.1		/* minimum sample interval */


/***********************************************************************
 * evaluator functions
 ***********************************************************************/

/* evaluator function */
typedef void (Eval)(struct expr *);


/***********************************************************************
 * internal representation of rule expressions and their values
 ***********************************************************************/

/* pointer to and timestamp for sample in ring buffer */
typedef struct {
    void	*ptr;		/* pointer into value ring buffer */
    RealTime	stamp;		/* timestamp for sample */
} Sample;

/* Expression Tree Node:
 * The parser fills in most of the fields, pragmatics analysis
 * fills in the rest.  The parser may leave 0 in e_idom or nvals
 * and NULL in ring to indicate it did not have enough information
 * to fill in the correct values.  These values are then patched
 * up during pragmatics analysis and dynamically if/when the number
 * of instances changes.
 *
 * Warning: The semantics of the hdom, e_idom and tdom fields are
 * quite subtle. A value of 1 or greater is the cardinality of
 * the corresponding domain.  -1 indicates that the corresponding
 * domain has collapsed, and there is one value.  0 indicates
 * that there is no value (only used for e_idom to indicate an
 * empty instance domain).
 */
typedef struct expr {
    /* expression syntax */
    int		    op;		/* operator */
    struct expr	    *arg1;	/* NULL || (Expr *) */
    struct expr     *arg2;	/* NULL || (Expr *) */
    struct expr	    *parent;	/* parent of this Expr */

    /* evaluator */
    Eval	    *eval;	/* evaluator function */
    int		    valid;	/* number of valid samples */

    /* description of value matrix */
    int		    hdom;	/* cardinality of host dimension */
    int		    e_idom;	/* cardinality of instance dimension */
    int		    tdom;	/* cardinality of time dimension */
    int 	    tspan;	/* number of values per sample */
    int		    nsmpls;	/* number of samples in ring buffer */
    int		    nvals;	/* total number of values in ring buffer */
    struct metric   *metrics;	/* array of per host metric info */

    /* description of single value */
    int   	    sem;	/* value semantics, see below */
    pmUnits	    units;	/* value units, as in pmDesc */

    /* value buffer */
    void    	    *ring;	/* base address of value ring buffer */
    Sample	    smpls[1];	/* array dynamically allocated */
} Expr;

/* per-host description of a performance metric */
typedef struct metric {
    struct expr     *expr;	/* Expr owning this Metric */
    struct profile  *profile;	/* Profile owning this Metric */
    struct host	    *host;	/* Host owning this Metric */
    struct metric   *next;	/* fetch/wait list forward pointer */
    struct metric   *prev;	/* fetch/wait list backward pointer */
    Symbol	    mname;	/* metric name */
    Symbol	    hname;	/* host name */
    pmDesc          desc;       /* pmAPI metric description */
    double	    conv;	/* conversion factor into canonical units */
    int		    specinst;	/* count of specific instances in rule and */
				/* 0 if all instances are to be considered */
    int		    m_idom;	/* cardinality of available instance domain */
    char            **inames;   /* array of instance names */
    int		    *iids;      /* array of instance ids */
    RealTime	    stamp;	/* time stamp for current values */
    pmValueSet	    *vset;	/* current values */
    RealTime	    stomp;	/* previous time stamp for rate calculation */
    double	    *vals;	/* vector of values for rate computation */
    int		    offset;	/* offset within sample in expr ring buffer */
} Metric;

/*
 * Note on instances in Metric:
 *
 * if specinst == 0, then m_idom, inames[] and iids[] are the
 *	currently available instances
 * otherwise, m_idom is the number of the specified instances currently
 *	available (and identified in inames[] and iids[] for 0 .. m_idom-1)
 *	and the unavailable instances are after that, i.e. elements
 *	m_idom ... specinst-1 of inames[] and iids[]
 */

/* per instance-domain part of bundled fetch request */
typedef struct profile {
    struct metric   *metrics;	/* list of Metrics for this Profile */
    struct fetch    *fetch;	/* Fetch bundle owning this Profile */
    struct profile  *next;	/* Profile list forward link */
    struct profile  *prev;	/* Profile list backward link */
    pmInDom         indom;	/* instance domain */
    int		    need_all;	/* all instances required */
} Profile;

/* bundled fetch request for multiple metrics */
typedef struct fetch {
    struct profile *profiles;   /* list of Profiles for this Fetch */
    struct host    *host;	/* Host owning this Fetch */
    struct fetch   *next;	/* fetch list forward pointer */
    struct fetch   *prev;	/* fetch list backward pointer */
    int            handle;      /* PMCS context handle */
    int		   npmids;	/* number of metrics in fetch */
    pmID	   *pmids;	/* array of metric ids to fetch */
    pmResult       *result;     /* result of fetch */
} Fetch;

/* set of bundled fetches for single host (may be archive or live):
   The field waits contains a list of Metrics for which descriptors
   were not available during pragmatics analysis. */
typedef struct host {
    struct fetch    *fetches;	/* list of Fetches for this Host */
    struct task     *task;      /* Task owning this host */
    struct host	    *next;	/* Host list forward pointer */
    struct host	    *prev;	/* Host list backward pointer */
    Symbol          name;       /* host machine */
    int	    	    down;	/* host is not delivering metrics */
    Metric	    *waits;	/* wait list of Metrics */
    Metric          *duds;	/* bad Metrics discovered during evaluation */
} Host;

/* element of evaluator task queue */
typedef struct task {
    int		  nth;		/* initial (syntactic) position in task queue */
    struct task	  *next;	/* task list forward link */
    struct task	  *prev;	/* task list backward link */
    RealTime      epoch;	/* bottom-line for timing calculations */
    RealTime      delta;	/* sample interval */
    TickTime      tick;		/* count up deltas */
    RealTime      eval;		/* scheduled evaluation time */
    RealTime	  retry;	/* scheduled retry down Hosts and Metrics */
    int		  nrules;	/* number of rules in this task */
    Symbol	  *rules;	/* array of rules to be evaluated */
    Host          *hosts;	/* fetches to be executed and waiting */
    pmResult	  *rslt;	/* for secret agent mode */
} Task;

/* value semantics - as in pmDesc plus following */
#define SEM_UNKNOWN	0	/* semantics not yet available */
#define SEM_NUMVAR	10	/* numeric variable value */
#define SEM_BOOLEAN	11	/* boolean (3-state) value */
#define SEM_CHAR	12	/* character (string) */
#define SEM_NUMCONST	13	/* numeric constant value */
#define SEM_REGEX	14	/* compiled regular expression */

/* Expr operator (op) tokens */
typedef int Op;
#define RULE		0
/* basic conditions */
#define CND_FETCH	1
#define CND_DELAY	2
#define CND_RATE	3
#define CND_INSTANT	9
/* arithmetic */
#define CND_NEG		4
#define CND_ADD		5
#define CND_SUB		6
#define CND_MUL		7
#define CND_DIV		8
/* aggregation */
#define CND_SUM_HOST	10
#define CND_SUM_INST	11
#define CND_SUM_TIME	12
#define CND_AVG_HOST	13
#define CND_AVG_INST	14
#define CND_AVG_TIME	15
#define CND_MAX_HOST	16
#define CND_MAX_INST	17
#define CND_MAX_TIME	18
#define CND_MIN_HOST	19
#define CND_MIN_INST	20
#define CND_MIN_TIME	21
/* relational */
#define CND_EQ		30
#define CND_NEQ		31
#define CND_LT		32
#define CND_LTE		33
#define CND_GT		34
#define CND_GTE		35
/* boolean */
#define CND_NOT		40
#define CND_RISE	41
#define CND_FALL	42
#define CND_AND		43
#define CND_OR		44
#define CND_MATCH	45
#define CND_NOMATCH	46
#define CND_RULESET	47
#define CND_OTHER	48
/* quantification */
#define CND_ALL_HOST	50
#define CND_ALL_INST	51
#define CND_ALL_TIME	52
#define CND_SOME_HOST	53
#define CND_SOME_INST	54
#define CND_SOME_TIME	55
#define CND_PCNT_HOST	56
#define CND_PCNT_INST	57
#define CND_PCNT_TIME	58
#define CND_COUNT_HOST	59
#define CND_COUNT_INST	60
#define CND_COUNT_TIME	61
/* actions */
#define ACT_SEQ		70
#define ACT_ALT		71
#define ACT_SHELL	72
#define ACT_ALARM	73
#define ACT_SYSLOG	74
#define ACT_PRINT	75
#define ACT_ARG		76
#define ACT_STOMP	77
/* no operation (extension) */
#define NOP		80
/* dereferenced variable */
#define OP_VAR		90

int unary(Op);		/* unary operator */
int binary(Op);		/* binary operator */

/***********************************************************************
 * archives
 ***********************************************************************/

typedef struct archive {
    struct archive  *next;	/* list link */
    char	    *fname;	/* file name */
    char            *hname;	/* host name */
    RealTime	    first;	/* timestamp for first pmResult */
    RealTime	    last;	/* timestamp for last pmResult */
} Archive;


/***********************************************************************
 * memory allocation / deallocation
 ***********************************************************************/

void *alloc(size_t);
void *zalloc(size_t);
void *ralloc(void *, size_t);
void *aalloc(size_t, size_t);
char *sdup(char *);

Expr *newExpr(int, Expr *, Expr *, int, int, int, int, int);
Profile *newProfile(Fetch *, pmInDom);
Fetch *newFetch(Host *);
Host *newHost(Task *, Symbol);
Task *newTask(RealTime, int);
void newResult(Task *);

void freeExpr(Expr *);

void freeMetric(Metric *);

void FreeProfile(Profile *);
void freeFetch(Fetch *);
void freeTask(Task *);


/***********************************************************************
 * ring buffer management
 ***********************************************************************/

void newRingBfr(Expr *);
void newStringBfr(Expr *, size_t, char *);
void rotate(Expr *);


/***********************************************************************
 * Expr manipulation
 ***********************************************************************/

Expr *primary(Expr *, Expr *);
void changeSmpls(Expr **, int);
void instFetchExpr(Expr *);

/***********************************************************************
 * time methods
 ***********************************************************************/

RealTime getReal(void);			/* return current time */
void reflectTime(RealTime);		/* update time vars to reflect now */
#define SLEEP_EVAL	0
#define SLEEP_RETRY	1
void sleepTight(Task *, int);		/* sleep until retry or eval time */
void logRotate(void);			/* close current, start a new log */

/*
 * diagnostic tracing
 */
void dumpRules(void);
void dumpExpr(Expr *);
void dumpTree(Expr *);
void dumpMetric(Metric *);
void dumpTask(Task *);
void __dumpExpr(int, Expr *);
void __dumpTree(int, Expr *);
void __dumpMetric(int, Metric *);

/***********************************************************************
 * comparison functions (for use by qsort)
 ***********************************************************************/

/* compare two instance identifiers. */
int compid(const void *, const void *);

/* compare two pmValue's on their inst fields */
int compair(const void *, const void *);


/***********************************************************************
 * global data structures
 ***********************************************************************/

extern char        *pmnsfile;	/* alternate namespace */
extern Archive	   *archives;	/* archives given on command line */
extern RealTime	   first;	/* archive starting point */
extern RealTime	   last;	/* archive end point */
extern char	   *dfltHostConn;  /* default PM_CONTEXT_HOST parameter  */
extern char	   *dfltHostName;  /* pmContextGetHostName of host name */
extern RealTime	   dfltDelta;	/* default sample interval */
extern RealTime    runTime;	/* run time interval */
extern int	   hostZone;	/* timezone from host? */
extern char	   *timeZone;	/* timezone from command line */
extern int	   quiet;	/* suppress default diagnostics */
extern int	   verbose;	/* verbosity 0, 1 or 2 */
extern int	   interactive;	/* interactive mode, -d */
extern int	   isdaemon;	/* run as a daemon */
extern int         agent;	/* secret agent mode? */
extern int         applet;	/* applet mode? */
extern int	   dowrap;	/* counter wrap? default no */
extern int	   doexit;	/* signalled its time to exit */
extern int	   dorotate;	/* log rotation was requested */
extern pmiestats_t *perf;	/* pmie performance data ptr */
extern pmiestats_t instrument;	/* pmie performance data struct */


extern SymbolTable rules;	/* currently known rules */
extern SymbolTable vars;	/* currently known variables */
extern SymbolTable hosts;	/* currently known hosts */
extern SymbolTable metrics;	/* currently known metrics */

extern Task	   *taskq;	/* evaluator task queue */
extern Expr	   *curr;	/* current executing rule expression */

extern RealTime	   now;		/* current time */
extern RealTime    start;	/* start evaluation */
extern RealTime    stop;	/* stop evaluation */


/***********************************************************************
 * reserved symbols
 ***********************************************************************/

extern Symbol symDelta;		/* current sample interval */
extern Symbol symMinute;	/* minutes after the hour 0..59 */
extern Symbol symHour;		/* hours since midnight 0..23 */
extern Symbol symDay;		/* day of the month 1..31 */
extern Symbol symMonth;		/* month of the year 1..12 */
extern Symbol symYear;		/* year 1996.. */
extern Symbol symWeekday;	/* days since Sunday 0..6 */


/***********************************************************************
 * compulsory initialization
 ***********************************************************************/

void dstructInit(void);		/* initialize central data structures */
void timeInit(void);		/* initialize time keeping data structures */
void agentInit(void);		/* initialize evaluation parameters */

/***********************************************************************
 * unknown units handling
 * We don't have a good sentinal value, but 1 / count x 10 ^ 7 does
 * not appear in any PMDA and cannot come from the pmie lexical scanner
 ***********************************************************************/
#define SET_UNITS_UNKNOWN(u) { u.dimCount = -1; u.scaleCount = 7; }
#define UNITS_UNKNOWN(u) (u.dimCount == -1 && u.scaleCount == 7)

#endif /* DSTRUCT_H */
