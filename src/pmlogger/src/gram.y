/*
 * Copyright (c) 2013-2014,2018 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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

/* 
 * There is a shift/reduced conflict reported by yacc when it cannot
 * decide whatever it should take 'optinst' route or 'access' one in
 * the following sutiation:
 *
 * log on once foo [access] all
 *
 * This conflict considered to be benign, since yacc takes 'the right' option
 * if optinst is supplied. To work around the issue of access been treated 
 * as an option, enclose the list of metrics in the curly braces, i.e.
 *
 * log on once {foo} [access] all 
 */

%{
#include "pmapi.h"
#include "libpcp.h"
#include "logger.h"

int		mystate = GLOBAL;	/* config file parser state */

__pmHashCtl	pm_hash;
task_t		*tasklist;		/* task list for logging configuration */
dynroot_t	*dyn_roots;		/* dynamic root list for handling PMCD_NAMES_CHANGE */
int		n_dyn_roots = 0;

static task_t	*tp;
static int	numinst;
static int	*intlist;
static char	**extlist;
static int	state;			/* logging state, current block */
static char	*metricName;		/* current metric, current block */

typedef struct _hl {
    struct _hl	*hl_next;
    char	*hl_name;
    int		hl_line;
} hostlist_t;

static hostlist_t	*hl_root;
static hostlist_t	*hl_last;
static hostlist_t	*hlp;
static hostlist_t	*prevhlp;
static int		opmask;		/* operations mask */
static int		specmask;	/* specifications mask */
static int		allow;		/* host allow/disallow state */

static int lookup_metric_name(const char *);
static void activate_new_metric(const char *);
static void activate_cached_metric(const char *, int);
static task_t *findtask(int, struct timeval *);
static void append_dynroot_list(const char *, int, int, struct timeval *);

%}
%union {
    long lval;
    char * str;
}

%expect 1

%term	LSQB
	RSQB
	COMMA
	LBRACE
	RBRACE
	COLON
	SEMICOLON

	LOG
	MANDATORY ADVISORY
	ON OFF MAYBE
	EVERY ONCE DEFAULT
	MSEC SECOND MINUTE HOUR

	ACCESS ENQUIRE ALLOW DISALLOW ALL EXCEPT

%token<str>  NAME STRING IPSPEC HOSTNAME URL
%token<lval> NUMBER

%type<lval> frequency timeunits action
%type<str>  hostspec
%%

config		: specopt accessopt
		;

specopt		: spec
		| /* nothing */
		;

spec		: stmt
		| spec stmt
		;

stmt		: dowhat somemetrics				
		{
		    mystate = GLOBAL;
		    if (tp->t_numvalid)
			linkback(tp);
		    state = 0;
                }
		;

dowhat		: logopt action		
		{
		    struct timeval delta;

		    delta.tv_sec = $2 / 1000;
		    delta.tv_usec = 1000 * ($2 % 1000);

		    /*
		     * Search for an existing task for this state/interval;
		     * only allocate and setup a new task if none exists.
		     */
		    if ((tp = findtask(state, &delta)) == NULL) {
			if ((tp = (task_t *)calloc(1, sizeof(task_t))) == NULL) {
			    char emess[256];
			    pmsprintf(emess, sizeof(emess), "malloc failed: %s", osstrerror());
			    yyerror(emess);
			} else {
			    task_t	*ltp;

			    /*
			     * Add to end of tasklist ... this means the
			     * first records to appear in the archive
			     * are more likely to follow the order of
			     * clauses in the configuration.
			     * Exceptions are "log once" that will appear
			     * first, and clauses combined into the same
			     * task.
			     */
			    for (ltp = tasklist; ltp != NULL && ltp->t_next != NULL; ltp = ltp->t_next)
				;
			    if (ltp == NULL)
				tasklist = tp;
			    else
				ltp->t_next = tp;
			    tp->t_next = NULL;
			    tp->t_delta = delta;
			    tp->t_state = state;
			}
		    }
		    state = 0;
		}
		;

logopt		: LOG 
		| /* nothing */
		;

action		: cntrl ON frequency	
		{ 
		    char emess[256];
                    if ($3 < 0) {
			pmsprintf(emess, sizeof(emess),
				"Logging delta (%ld msec) must be positive",$3);
			yyerror(emess);
		    }
		    else if ($3 >  PMLC_MAX_DELTA) {
			pmsprintf(emess, sizeof(emess),
				"Logging delta (%ld msec) cannot be bigger "
				"than %d msec", $3, PMLC_MAX_DELTA);
			yyerror(emess);
		    }

                    PMLC_SET_ON(state, 1); 
                    $$ = $3;
                }
		| cntrl OFF			{ PMLC_SET_ON(state, 0);$$ = 0;}
		| MANDATORY MAYBE
		{
                    PMLC_SET_MAND(state, 0);
                    PMLC_SET_ON(state, 0);
                    PMLC_SET_MAYBE(state, 1);
                    $$ = 0;
                }
		;

cntrl		: MANDATORY			{ PMLC_SET_MAND(state, 1); }
		| ADVISORY			{ PMLC_SET_MAND(state, 0); }
		| /*nothing == advisory*/	{ PMLC_SET_MAND(state, 0); }
			;

frequency	: everyopt NUMBER timeunits	{ $$ = $2*$3; }
		| ONCE				{ $$ = 0; }
		| DEFAULT						
		{ 
                    extern struct timeval delta; /* default logging interval */
                    $$ = delta.tv_sec*1000 + delta.tv_usec/1000;
                }
		;

everyopt	: EVERY
		| /* nothing */
		;

timeunits	: MSEC		{ $$ = 1; }
		| SECOND	{ $$ = 1000; }
		| MINUTE	{ $$ = 60000; }
		| HOUR		{ $$ = 3600000; }
		;

somemetrics	: LBRACE { mystate = INSPEC; } metriclist RBRACE
		| metricspec
		;

metriclist	: metricspec
		| metriclist metricspec
		| metriclist COMMA metricspec
		;

metricspec	: NAME
		{
                    if ((metricName = strdup($1)) == NULL) {
			char emess[256];
			pmsprintf(emess, sizeof(emess), "malloc failed: %s", osstrerror());
                        yyerror(emess);
		    }
                }
		optinst
		{
		    int index, sts;
		    pmID id;

		    /*
		     * search names for previously seen metrics for this task
		     * (note that name may be non-terminal in the PMNS here);
		     * if already found in this task, skip namespace PDUs.
		     */
		    if ((index = lookup_metric_name(metricName)) < 0) {
			if ((sts = pmTraversePMNS(metricName, activate_new_metric)) < 0 ) {
			    char emess[256];
			    pmsprintf(emess, sizeof(emess),
				    "Problem with lookup for metric \"%s\" "
				    "... logging not activated", metricName);
			    yywarn(emess);
			    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));

			}

			/*
			 * Check if metricName is a potential dynamic root. If metricName
			 * is not a leaf, then it could be a dynamic root non-leaf node,
			 * even if it currently has no children. The PMAPI says that 
			 * pmTraversePMNS(name, func) returns 1 if name is a leaf node
			 * (or a derived metric) and _also_ 1 if it's a non-leaf node with
			 * exactly one child.
			 *
			 * So metricName is a potential dynamic root if
			 * sts < 0                   : unknown name or an error
			 * sts == 0                  : childless dynamic root
			 * sts > 1                   : non-leaf with children
			 * sts == 1 and not a leaf   : non-leaf with exactly one child
			 */
			if (sts <= 0 || sts > 1 || pmLookupName(1, &metricName, &id) != 1) {
			    /*
			     * Add it to the list for future traversal when a fetch returns
			     * with the PMCD_NAMES_CHANGE flag set.
			     */
			    append_dynroot_list(metricName,
				PMLC_GET_ON(tp->t_state) ? PM_LOG_ON : PM_LOG_MAYBE, /* TODO PMLOG_OFF? */
				PMLC_GET_MAND(tp->t_state) ? PM_LOG_MANDATORY : PM_LOG_ADVISORY,
				&tp->t_delta);
			}
		    }
		    else {	/* name is cached already, handle instances */
			activate_cached_metric(metricName, index);
		    }
		    freeinst(&numinst, intlist, extlist);
		    free(metricName);
		}
		;

optinst		: LSQB instancelist RSQB
		| /* nothing */
		;

instancelist	: instance
		| instance instancelist
		| instance COMMA instancelist
		;

instance	: NAME		{ buildinst(&numinst, &intlist, &extlist, -1, $1); }
		| NUMBER	{ buildinst(&numinst, &intlist, &extlist, $1, NULL); }
		| STRING	{ buildinst(&numinst, &intlist, &extlist, -1, $1); }
		;

accessopt	: LSQB ACCESS RSQB ctllist
		| /* nothing */
		;

ctllist		: ctl
		| ctl ctllist
		;
		
ctl		: allow hostlist COLON operation SEMICOLON
		{
                    prevhlp = NULL;
                    for (hlp = hl_root; hlp != NULL; hlp = hlp->hl_next) {
			int sts;

                        if (prevhlp != NULL) {
                            free(prevhlp->hl_name);
                            free(prevhlp);
                        }
                        sts = __pmAccAddHost(hlp->hl_name, specmask, 
                                             opmask, 0);
                        if (sts < 0) {
                            fprintf(stderr, "error was on line %d\n", 
                                hlp->hl_line);
                            YYABORT;
                        }
                        prevhlp = hlp;
                    }
                    if (prevhlp != NULL) {
                        free(prevhlp->hl_name);
                        free(prevhlp);
                    }
                    opmask = 0;
                    specmask = 0;
                    hl_root = hl_last = NULL;
                }
		;

allow		: ALLOW			{ allow = 1; }
		| DISALLOW		{ allow = 0; }
		;

hostlist	: host
		| host COMMA hostlist
		;

host		: hostspec
		{
		    size_t sz = sizeof(hostlist_t);

                    hlp = (hostlist_t *)malloc(sz);
                    if (hlp == NULL) {
                        pmNoMem("adding new host", sz, PM_FATAL_ERR);
                    }
                    if (hl_last != NULL) {
                        hl_last->hl_next = hlp;
                        hl_last = hlp;
                    }
                    else
                        hl_root = hl_last = hlp;
                    hlp->hl_next = NULL;
                    hlp->hl_name = strdup($1);
                    hlp->hl_line = lineno;
                }
		;

hostspec	: IPSPEC
		| URL
		| HOSTNAME
		| NAME
		;

operation	: operlist
		{
                    specmask = opmask;
                    if (allow)
                        opmask = ~opmask;
                }
		| ALL			
		{
                    specmask = PM_OP_ALL;
                    if (allow)
                        opmask = PM_OP_NONE;
                    else
                        opmask = PM_OP_ALL;
                }
		| ALL EXCEPT operlist
		{
                    specmask = PM_OP_ALL;
                    if (!allow)
                        opmask = ~opmask;
                }
		;

operlist	: op
		| op COMMA operlist
		;

op		: ADVISORY		{ opmask |= PM_OP_LOG_ADV; }
		| MANDATORY		{ opmask |= PM_OP_LOG_MAND; }
		| ENQUIRE		{ opmask |= PM_OP_LOG_ENQ; }
		;

%%

/*
 * Search the cache for previously seen metrics for active task.
 * Returns -1 if not found, else an index into tp->t_namelist.
 */
static int
lookup_metric_name(const char *name)
{
    int		j;

    for (j = 0; j < tp->t_numpmid; j++)
	if (strcmp(tp->t_namelist[j], name) == 0)
	    return j;
    return -1;
}

/*
 * Assumed calling context ...
 *	tp		the correct task for the requested metric
 *	numinst		number of instances associated with this request
 *	extlist[]	external instance names if numinst > 0
 *	intlist[]	internal instance identifier if numinst > 0 and
 *			corresponding extlist[] entry is NULL
 */
static void
activate_cached_metric(const char *name, int index)
{
    int		sts = 0;
    int		inst;
    int		i;
    int		j;
    int		skip = 0;
    pmID	pmid;
    pmDesc	*dp;
    optreq_t	*rqp;
    char	emess[1024];

    /*
     * need new malloc'd pmDesc, even if metric found in cache, as
     * the fetchctl keeps its own (non-realloc-movable!) pointer.
     */
    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL)
	goto nomem;

    if (index < 0) {
	if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0 || pmid == PM_ID_NULL) {
	    pmsprintf(emess, sizeof(emess),
		    "Metric \"%s\" is unknown ... not logged", name);
	    goto snarf;
	}
	/* is this a derived metric? */
	if (IS_DERIVED(pmid))
	    tp->t_dm++;
	if ((sts = pmLookupDesc(pmid, dp)) < 0) {
	    pmsprintf(emess, sizeof(emess),
		    "Description unavailable for metric \"%s\" ... not logged",
		    name);
	    goto snarf;
	}
	tp->t_numpmid++;
	tp->t_namelist = (char **)realloc(tp->t_namelist, tp->t_numpmid * sizeof(char *));
	if (tp->t_namelist == NULL)
	    goto nomem;
	if ((tp->t_namelist[tp->t_numpmid-1] = strdup(name)) == NULL)
	    goto nomem;
	tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, tp->t_numpmid * sizeof(pmID));
	if (tp->t_pmidlist == NULL)
	    goto nomem;
	tp->t_desclist = (pmDesc *)realloc(tp->t_desclist, tp->t_numpmid * sizeof(pmDesc));
	if (tp->t_desclist == NULL)
	    goto nomem;
	tp->t_pmidlist[tp->t_numpmid-1] = pmid;
	tp->t_desclist[tp->t_numpmid-1] = *dp;	/* struct assignment */
    }
    else {
	*dp = tp->t_desclist[index];
	pmid = tp->t_pmidlist[index];
    }

    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL)
	goto nomem;
    rqp->r_desc = dp;
    rqp->r_numinst = numinst;

    if (numinst) {
	/*
	 * malloc here, and keep ... gets buried in optFetch data structures
	 */
	rqp->r_instlist = (int *)malloc(numinst * sizeof(rqp->r_instlist[0]));
	if (rqp->r_instlist == NULL)
	    goto nomem;
	j = 0;
	for (i = 0; i < numinst; i++) {
	    if (extlist[i] != NULL) {
		sts = pmLookupInDom(dp->indom, extlist[i]);
		if (sts < 0) {
                    pmsprintf(emess, sizeof(emess),
			"Instance \"%s\" is not defined for the metric \"%s\"",
			extlist[i], name);
                    yywarn(emess);
		    rqp->r_numinst--;
		    continue;
		}
		inst = sts;
	    }
	    else {
		char	*p;
		sts = pmNameInDom(dp->indom, intlist[i], &p);
		if (sts < 0) {
                    pmsprintf(emess, sizeof(emess),
			"Instance \"%d\" is not defined for the metric \"%s\"",
			intlist[i], name);
                    yywarn(emess);
		    rqp->r_numinst--;
		    continue;
		}
		free(p);
		inst = intlist[i];
	    }
	    if ((sts = chk_one(tp, pmid, inst)) < 0) {
                pmsprintf(emess, sizeof(emess),
			"Incompatible request for metric \"%s\" "
			"and instance \"%s\"", name, extlist[i]);
                yywarn(emess);
                fprintf(stderr, "%s\n", chk_emess[-sts]);
                rqp->r_numinst--;
	    }
	    else if (sts == 0)
		rqp->r_instlist[j++] = inst;
	    else	/* already have this instance */
		skip = 1;
	}
	if (rqp->r_numinst == 0)
	    skip = 1;
    }
    else {
	if ((sts = chk_all(tp, pmid)) < 0) {
            pmsprintf(emess, sizeof(emess),
		    "Incompatible request for metric \"%s\"", name);
            yywarn(emess);

	    skip = 1;
	}
    }

    if (!skip) {
	__pmOptFetchAdd(&tp->t_fetch, rqp);
	if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0) {
	    pmsprintf(emess, sizeof(emess), "__pmHashAdd failed "
		    "for metric \"%s\" ... logging not activated", name);
	    goto snarf;
	}
	tp->t_numvalid++;
    }
    else {
	free(dp);
	free(rqp);
    }
    return;

nomem:
    pmsprintf(emess, sizeof(emess), "malloc failed: %s", osstrerror());
    yyerror(emess);

snarf:
    yywarn(emess);
    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    if (dp != NULL)
        free(dp);
    return;
}

static void
activate_new_metric(const char *name)
{
    activate_cached_metric(name, lookup_metric_name(name));
}

/*
 * Given a logging state and an interval, return a matching task
 * or NULL if none exists for that value pair.
 */
task_t *
findtask(int state, struct timeval *delta)
{
    task_t	*tp;

    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	if (state == tp->t_state &&
	    delta->tv_sec == tp->t_delta.tv_sec &&
	    delta->tv_usec == tp->t_delta.tv_usec)
	    break;
    }
    return tp;
}

/*
 * Append 'name' to the list of non-leaf PMNS nodes to be traversed for
 * new metrics when a fetch returns with the PMCD_NAMES_CHANGE flag set.
 * Note: 'name' is not a PMNS leaf and may or may not be a non-leaf,
 * since it may appear dynamically in the future. This list is traversed
 * by check_dynamic_metrics() when a fetch returns with the PMCD_NAMES_CHANGE
 * flag set.
 */
static void
append_dynroot_list(const char *name, int state, int control, struct timeval *timedelta)
{
    int i;
    dynroot_t *d;

    for (i=0, d=dyn_roots; i < n_dyn_roots; i++, d++) {
	if (strcmp(d->name, name) == 0)
	    return; /* already present, don't add it again */
    }
    if ((dyn_roots = (dynroot_t *)realloc(dyn_roots, ++n_dyn_roots * sizeof(dynroot_t))) == NULL)
	pmNoMem("extending dyn_roots list", n_dyn_roots * sizeof(dynroot_t), PM_FATAL_ERR);
    d = &dyn_roots[n_dyn_roots-1];
    if ((d->name = strdup(name)) == NULL)
	pmNoMem("strdup name in dyn_roots list", strlen(name) + 1, PM_FATAL_ERR);
    d->state = state;
    d->control = control;
    d->delta = *timedelta;

    if (pmDebugOptions.log) {
	fprintf(stderr, "pmlogger: possible dynamic root \"%s\", state=0x%x, control=0x%x, delta=%ld.%06ld\n",
	    name, state, control, (long)timedelta->tv_sec, (long)timedelta->tv_usec);
    }
}

/*
 * Complete the delayed processing of task elements, which can only
 * be done once all configuration file parsing is complete.
 */
void
yyend(void)
{
    /*
     * The value of blink is chosen to ensure the tasks that require
     * repeated scheduling are not all done with the "log once" tasks.
     * In onlalarm() in the AF.c code of libpcp, there is a 10msec
     * window in which any task that is scheduled for within 10msec
     * of the time that the alarm goes off will be run, and we need
     * to go outside this window, hence blink is 20msec.
     */
    struct timeval blink = { 0, 20000 };
    /*
     * First pass to initialize and do the "log once" tasks.
     * We want the "log once" cases to come first in the
     * archive so that things like the hinv.* metrics are defined
     * _before_ any of the useful data, otherwise the first useful
     * data sample will be wasted, e.g. if hinv.ncpu does not
     * (yet) have a value in the archive.
     */
    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	if (tp->t_numvalid == 0)
	    continue;
	PMLC_SET_MAYBE(tp->t_state, 0);	/* clear req */
	if (PMLC_GET_ON(tp->t_state) && (tp->t_delta.tv_sec == 0 && tp->t_delta.tv_usec == 0)) {
	    tp->t_afid = __pmAFregister(&tp->t_delta, (void *)tp, log_callback);
	}
    }
    /* Second pass for the other tasks */
    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	if (tp->t_numvalid == 0)
	    continue;
	if (PMLC_GET_ON(tp->t_state) && (tp->t_delta.tv_sec != 0 || tp->t_delta.tv_usec != 0)) {
	    /*
	     * log as soon as possible and then every t_delta units of
	     * time thereafter
	     */
	    tp->t_afid = __pmAFsetup(&blink, &tp->t_delta, (void *)tp, log_callback);
	}
    }
}
