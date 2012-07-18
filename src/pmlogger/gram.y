/*
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
#include "impl.h"
#include "logger.h"

int		mystate = GLOBAL;

__pmHashCtl	pm_hash;
task_t		*tasklist;
fetchctl_t	*fetchroot;

static int	sts;
static int	numinst;
static int	*intlist;
static char	**extlist;
static task_t	*tp;
static fetchctl_t	*fp;
static int	numvalid;
static int	warn = 1;

extern int	lineno;

typedef struct _hl {
    struct _hl	*hl_next;
    char	*hl_name;
    int		hl_line;
} hostlist_t;

static hostlist_t	*hl_root = NULL;
static hostlist_t	*hl_last = NULL;
static hostlist_t	*hlp;
static hostlist_t	*prevhlp;
static int		opmask = 0;
static int		specmask = 0;
static int		allow;
static int		state = 0;
static char		* metricName;
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

%token<str>  NAME STRING IPSPEC HOSTNAME
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
                    if (numvalid) {
                        PMLC_SET_MAYBE(tp->t_state, 0);	/* clear req */
                        tp->t_next = tasklist;
                        tasklist = tp;
                        tp->t_fetch = fetchroot;
                        for (fp = fetchroot; fp != NULL; fp = fp->f_next)
                            /* link fetchctl back to task */
                            fp->f_aux = (void *)tp;

                        if (PMLC_GET_ON(state))
                            tp->t_afid = __pmAFregister(&tp->t_delta, 
                                                        (void *)tp, 
                                                        log_callback);
		    }
		    else
			free(tp);
                    
                    fetchroot = NULL;
                    state = 0;
                }
		;

dowhat		: logopt action		
		{
                    if ((tp = (task_t *)calloc(1, sizeof(task_t))) == NULL) {
			char emess[256];
			sprintf(emess, "malloc failed: %s", osstrerror());
			yyerror(emess);
                    }
                    tp->t_delta.tv_sec = $2 / 1000;
                    tp->t_delta.tv_usec = 1000 * ($2 % 1000);
                    tp->t_state =  state;
                }
		;

logopt		: LOG 
		| /* nothing */
		;

action		: cntrl ON frequency	
		{ 
		    char emess[256];
                    if ($3 < 0) {
			sprintf(emess, 
				"Logging delta (%ld msec) must be positive",$3);
			yyerror(emess);
		    }
		    else if ($3 >  PMLC_MAX_DELTA) {
			sprintf(emess, 
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

somemetrics	: LBRACE { numvalid = 0; mystate = INSPEC; } metriclist RBRACE
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
			sprintf(emess, "malloc failed: %s", osstrerror());
                        yyerror(emess);
		    }
                }
		optinst	
		{
		    /*
		     * search cache for previously seen metrics for this task
		     */
		    int		j;
		    for (j = 0; j < tp->t_numpmid; j++) {
			if (tp->t_namelist[j] != NULL &&
			    strcmp(tp->t_namelist[j], metricName) == 0) {
			    break;
			}
		    }
		    if (j < tp->t_numpmid) {
			/* found in cache */
			dometric(metricName);
		    }
		    else {
		        /*
			 * either a new metric, and so it may be a
			 * non-terminal in the PMNS
			 */
			if ((sts = pmTraversePMNS(metricName, dometric)) < 0 ) {
			    char emess[256];
			    sprintf(emess, "Problem with lookup for metric \"%s\" "
					    "... logging not activated",metricName);
			    yywarn(emess);
			    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
			}
		    }
                    freeinst(&numinst, intlist, extlist);
                    free (metricName);
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
                    hlp = (hostlist_t *)malloc(sts = sizeof(hostlist_t));
                    if (hlp == NULL) {
                        __pmNoMem("adding new host", sts, PM_FATAL_ERR);
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
 * Assumed calling context ...
 *	tp		the correct task for the requested metric
 *	numinst		number of instances associated with this request
 *	extlist[]	external instance names if numinst > 0
 *	intlist[]	internal instance identifier if numinst > 0 and
 *			corresponding extlist[] entry is NULL
 */

void
dometric(const char *name)
{
    int		sts = 0;	/* initialize to pander to gcc */
    int		inst;
    int		i;
    int		j;
    int		dup = -1;
    int		skip;
    pmID	pmid;
    pmDesc	*dp;
    optreq_t	*rqp;
    extern char	*chk_emess[];
	char emess[1024];

    /*
     * search cache for previously seen metrics for this task
     */
    for (j = 0; j < tp->t_numpmid; j++) {
	if (tp->t_namelist[j] != NULL &&
	    strcmp(tp->t_namelist[j], name) == 0) {
	    dup = j;
	    break;
	}
    }

    /*
     * need new malloc'd pmDesc, even if metric found in cache
     */
    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL)
	goto nomem;

    if (dup == -1) {

	/* Cast away const, pmLookupName should never modify name */
	if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0 || pmid == PM_ID_NULL) {
	   sprintf(emess, "Metric \"%s\" is unknown ... not logged", name);
	    goto defer;
	}

	if ((sts = pmLookupDesc(pmid, dp)) < 0) {
	    sprintf(emess, "Description unavailable for metric \"%s\" ... not logged", name);

	    goto defer;
	}
    }
    else {
	*dp = tp->t_desclist[dup];
	pmid = tp->t_pmidlist[dup];
    }

    tp->t_numpmid++;
    tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, tp->t_numpmid * sizeof(pmID));
    if (tp->t_pmidlist == NULL)
	goto nomem;
    tp->t_namelist = (char **)realloc(tp->t_namelist, tp->t_numpmid * sizeof(char *));
    if (tp->t_namelist == NULL)
	goto nomem;
    tp->t_desclist = (pmDesc *)realloc(tp->t_desclist, tp->t_numpmid * sizeof(pmDesc));
    if (tp->t_desclist == NULL)
	goto nomem;
    if ((tp->t_namelist[tp->t_numpmid-1] = strdup(name)) == NULL)
	goto nomem;
    tp->t_pmidlist[tp->t_numpmid-1] = pmid;
    tp->t_desclist[tp->t_numpmid-1] = *dp;	/* struct assignment */

    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL)
	goto nomem;
    rqp->r_desc = dp;
    rqp->r_numinst = numinst;
    skip = 0;
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
			sprintf(emess, "Instance \"%s\" is not defined for the metric \"%s\"", extlist[i], name);
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
           sprintf(emess, "Instance \"%d\" is not defined for the metric \"%s\"", intlist[i], name);
           yywarn(emess);

		    rqp->r_numinst--;
		    continue;
		}
		free(p);
		inst = intlist[i];
	    }
	    if ((sts = chk_one(tp, pmid, inst)) < 0) {
			sprintf(emess, "Incompatible request for metric \"%s\" and instance \"%s\"", name, extlist[i]);
			yywarn(emess);

			fprintf(stderr, "%s\n", chk_emess[-sts]);
			rqp->r_numinst--;
	    }
	    else
		rqp->r_instlist[j++] = inst;
	}
	if (rqp->r_numinst == 0)
	    skip = 1;
    }
    else {
	if ((sts = chk_all(tp, pmid)) < 0) {
            sprintf(emess, "Incompatible request for metric \"%s\"", name);
            yywarn(emess);

	    skip = 1;
	}
    }

    if (!skip) {
	__pmOptFetchAdd(&fetchroot, rqp);
	if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0) {
	    sprintf(emess, "__pmHashAdd failed for metric \"%s\" ... logging not activated", name);

	    goto snarf;
	}
	numvalid++;
    }
    else {
	free(dp);
	free(rqp);
    }

    return;

defer:
    /* EXCEPTION PCP 2.0
     * The idea here is that we will sort all logging request into "good" and
     * "bad" (like pmie) ... the "bad" ones are "deferred" and at some point
     * later pmlogger would (periodically) revisit the "deferred" ones and see
     * if they can be added to the "good" set.
     */
    if (warn) {
        yywarn(emess);
        fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    }
    if (dp != NULL)
        free(dp);
    return;

nomem:
    sprintf(emess, "malloc failed: %s", osstrerror());
    yyerror(emess);

snarf:
    yywarn(emess);
    fprintf(stderr, "Reason: %s\n", pmErrStr(sts));
    free(dp);
    return;
}
