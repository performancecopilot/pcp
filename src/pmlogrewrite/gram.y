/*
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

%{
/*
 *  pmlogrewrite parser
 */
#include "pmapi.h"
#include "impl.h"
#include "logger.h"
#include <errno.h>
#include <assert.h>

static indomspec_t	*current_indomspec;
static int		current_star_indom;
static int		do_walk_indom;
static int		star_domain;

static metricspec_t	*current_metricspec;
static int		current_star_metric;
static int		star_cluster;
static int		do_walk_metric;
static int		output = OUTPUT_ALL;
static int		one_inst;
static char		*one_name;

indomspec_t *
walk_indom(int mode)
{
    static indomspec_t	*ip;

    if (do_walk_indom) {
	if (mode == W_START)
	    ip = indom_root;
	else
	    ip = ip->i_next;
	while (ip != NULL && pmInDom_domain(ip->old_indom) != star_domain)
	    ip = ip->i_next;
    }
    else {
	if (mode == W_START)
	    ip = current_indomspec;
	else
	    ip = NULL;
    }

    return ip;
}

metricspec_t *
walk_metric(int mode, int flag, char *which, int dupok)
{
    static metricspec_t	*mp;

    if (do_walk_metric) {
	if (mode == W_START)
	    mp = metric_root;
	else
	    mp = mp->m_next;
	while (mp != NULL) {
	    if (pmid_domain(mp->old_desc.pmid) == star_domain &&
		(star_cluster == PM_ID_NULL || star_cluster == pmid_cluster(mp->old_desc.pmid)))
		break;
	    mp = mp->m_next;
	}
    }
    else {
	if (mode == W_START)
	    mp = current_metricspec;
	else
	    mp = NULL;
    }

    if (mp != NULL) {
	if (!dupok && (mp->flags & flag)) {
	    pmsprintf(mess, sizeof(mess), "Duplicate %s clause for metric %s", which, mp->old_name);
	    yyerror(mess);
	}
	if (flag != METRIC_DELETE) {
	    if (mp->flags & METRIC_DELETE) {
		pmsprintf(mess, sizeof(mess), "Conflicting %s clause for deleted metric %s", which, mp->old_name);
		yyerror(mess);
	    }
	}
	else {
	    if (mp->flags & (~METRIC_DELETE)) {
		pmsprintf(mess, sizeof(mess), "Conflicting delete and other clauses for metric %s", mp->old_name);
		yyerror(mess);
	    }
	}
    }

    return mp;
}

%}

%union {
    char		*str;
    int			ival;
    double		dval;
    pmInDom		indom;
    pmID		pmid;
}

%token	TOK_LBRACE
	TOK_RBRACE
	TOK_PLUS
	TOK_MINUS
	TOK_COLON
	TOK_COMMA
	TOK_ASSIGN
	TOK_GLOBAL
	TOK_INDOM
	TOK_DUPLICATE
	TOK_METRIC
	TOK_HOSTNAME
	TOK_TZ
	TOK_TIME
	TOK_NAME
	TOK_INST
	TOK_INAME
	TOK_DELETE
	TOK_PMID
	TOK_NULL_INT
	TOK_TYPE
	TOK_IF
	TOK_SEM
	TOK_UNITS
	TOK_OUTPUT
	TOK_RESCALE

%token<str>	TOK_GNAME TOK_NUMBER TOK_STRING TOK_HNAME TOK_FLOAT
%token<str>	TOK_INDOM_STAR TOK_PMID_INT TOK_PMID_STAR
%token<ival>	TOK_TYPE_NAME TOK_SEM_NAME TOK_SPACE_NAME TOK_TIME_NAME
%token<ival>	TOK_COUNT_NAME TOK_OUTPUT_TYPE

%type<str>	hname
%type<indom>	indom_int null_or_indom
%type<pmid>	pmid_int pmid_or_name
%type<ival>	signnumber number rescaleopt duplicateopt
%type<dval>	float

%%

config		: speclist
    		;

speclist	: spec
		| spec speclist
		;

spec		: globalspec
		| indomspec
		| metricspec
		;

globalspec	: TOK_GLOBAL TOK_LBRACE globaloptlist TOK_RBRACE
		| TOK_GLOBAL TOK_LBRACE TOK_RBRACE
		;

globaloptlist	: globalopt
		| globalopt globaloptlist
		;

globalopt	: TOK_HOSTNAME TOK_ASSIGN hname
		    {
			if (global.flags & GLOBAL_CHANGE_HOSTNAME) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global hostname clause");
			    yyerror(mess);
			}
			if (strcmp(inarch.label.ll_hostname, $3) == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global hostname (%s): No change", inarch.label.ll_hostname);
				yywarn(mess);
			    }
			}
			else {
			    strncpy(global.hostname, $3, sizeof(global.hostname));
			    global.flags |= GLOBAL_CHANGE_HOSTNAME;
			}
			free($3);
		    }
		| TOK_TZ TOK_ASSIGN TOK_STRING
		    {
			if (global.flags & GLOBAL_CHANGE_TZ) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global tz clause");
			    yyerror(mess);
			}
			if (strcmp(inarch.label.ll_tz, $3) == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global timezone (%s): No change", inarch.label.ll_tz);
				yywarn(mess);
			    }
			}
			else {
			    strncpy(global.tz, $3, sizeof(global.tz));
			    global.flags |= GLOBAL_CHANGE_TZ;
			}
			free($3);
		    }
		| TOK_TIME TOK_ASSIGN signtime
		    {
			if (global.flags & GLOBAL_CHANGE_TIME) {
			    pmsprintf(mess, sizeof(mess), "Duplicate global time clause");
			    yyerror(mess);
			}
			if (global.time.tv_sec == 0 && global.time.tv_usec == 0) {
			    /* no change ... */
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Global time: No change");
				yywarn(mess);
			    }
			}
			else
			    global.flags |= GLOBAL_CHANGE_TIME;
		    }
		| TOK_HOSTNAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting hostname in hostname clause");
			yyerror(mess);
		    }
		| TOK_HOSTNAME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in hostname clause");
			yyerror(mess);
		    }
		| TOK_TZ TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting timezone string in tz clause");
			yyerror(mess);
		    }
		| TOK_TZ
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in tz clause");
			yyerror(mess);
		    }
		| TOK_TIME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting delta of the form [+-][HH:[MM:]]SS[.d...] in time clause");
			yyerror(mess);
		    }
		| TOK_TIME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in time clause");
			yyerror(mess);
		    }
		;

	/*
	 * ambiguity in lexical scanner ... handle here
	 * abc.def - is TOK_HNAME or TOK_GNAME
	 * 123 - is TOK_HNAME or TOK_NUMBER
	 * 123.456 - is TOK_HNAME or TOK_FLOAT
	 */
hname		: TOK_HNAME
		| TOK_GNAME
		| TOK_NUMBER
		| TOK_FLOAT
		;

signnumber	: TOK_PLUS TOK_NUMBER
		    {
			$$ = atoi($2);
			free($2);
		    }
		| TOK_MINUS TOK_NUMBER
		    {
			$$ = -atoi($2);
			free($2);
		    }
		| TOK_NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

number		: TOK_NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

float		: TOK_FLOAT
		    {
			$$ = atof($1);
			free($1);
		    }
		;

signtime	: TOK_PLUS time
		| TOK_MINUS time { global.time.tv_sec = -global.time.tv_sec; }
		| time
		;

time		: number TOK_COLON number TOK_COLON float	/* HH:MM:SS.d.. format */
		    { 
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + (int)$5;
			global.time.tv_usec = (int)(1000000*(($5 - (int)$5))+0.5);
		    }
		| number TOK_COLON number TOK_COLON number	/* HH:MM:SS format */
		    { 
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + $5;
		    }
		| number TOK_COLON float		/* MM:SS.d.. format */
		    { 
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + (int)$3;
			global.time.tv_usec = (int)(1000000*(($3 - (int)$3))+0.5);
		    }
		| number TOK_COLON number		/* MM:SS format */
		    { 
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + $3;
		    }
		| float			/* SS.d.. format */
		    {
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = (int)$1;
			global.time.tv_usec = (int)(1000000*(($1 - (int)$1))+0.5);
		    }
		| number		/* SS format */
		    {
			if ($1 > 59) {
			    pmsprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = $1;
			global.time.tv_usec = 0;
		    }
		;

indomspec	: TOK_INDOM indom_int
		    {
			if (current_star_indom) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*node;

			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			    PM_UNLOCK(ctxp->c_lock);
			    hcp = &ctxp->c_archctl->ac_log->l_hashindom;
			    star_domain = pmInDom_domain($2);
			    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
				 node != NULL;
				 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
				if (pmInDom_domain((pmInDom)(node->key)) == star_domain)
				    current_indomspec = start_indom((pmInDom)(node->key));
			    }
			    do_walk_indom = 1;
			}
			else {
			    current_indomspec = start_indom($2);
			    do_walk_indom = 0;
			}
		    }
			TOK_LBRACE optindomopt TOK_RBRACE
		| TOK_INDOM
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or <domain>.* in indom rule");
			yyerror(mess);
		    }
		;

indom_int	: TOK_FLOAT
		    {
			int		domain;
			int		serial;
			int		sts;
			sts = sscanf($1, "%d.%d", &domain, &serial);
			if (sts < 2) {
			    pmsprintf(mess, sizeof(mess), "Missing serial field for indom");
			    yyerror(mess);
			}
			if (domain < 1 || domain >= DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
			    yyerror(mess);
			}
			if (serial < 0 || serial >= 4194304) {
			    pmsprintf(mess, sizeof(mess), "Illegal serial field (%d) for indom", serial);
			    yyerror(mess);
			}
			current_star_indom = 0;
			free($1);
			$$ = pmInDom_build(domain, serial);
		    }
		| TOK_INDOM_STAR
		    {
			int		domain;
			sscanf($1, "%d.", &domain);
			if (domain < 1 || domain >= DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
			    yyerror(mess);
			}
			current_star_indom = 1;
			free($1);
			$$ = pmInDom_build(domain, 0);
		    }
		;

optindomopt	: indomoptlist
		|
		;

indomoptlist	: indomopt
		| indomopt indomoptlist
		;

indomopt	: TOK_INDOM TOK_ASSIGN duplicateopt indom_int
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    pmInDom	indom;
			    if (indom_root->new_indom != indom_root->old_indom) {
				pmsprintf(mess, sizeof(mess), "Duplicate indom clause for indom %s", pmInDomStr(indom_root->old_indom));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($4), pmInDom_serial(ip->old_indom));
			    else
				indom = $4;
			    if (indom != ip->old_indom)
				ip->new_indom = indom;
			    else {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Instance domain %s: indom: No change", pmInDomStr(ip->old_indom));
				    yywarn(mess);
				}
			    }
			    ip->indom_flags |= $3;
			}
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN TOK_STRING
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
			free($2);
			/* Note: $4 referenced from new_iname[] */
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN TOK_DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, NULL) < 0)
			    	yyerror(mess);
			}
			free($2);
		    }
		| TOK_INST number TOK_ASSIGN number
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
		    }
		| TOK_INST number TOK_ASSIGN TOK_DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, PM_IN_NULL) < 0)
				yyerror(mess);
			}
		    }
		| TOK_INDOM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or <domain>.* in indom clause");
			yyerror(mess);
		    }
		| TOK_INDOM
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in indom clause");
			yyerror(mess);
		    }
		| TOK_INAME TOK_STRING TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting new external instance name string or DELETE in iname clause");
			yyerror(mess);
		    }
		| TOK_INAME TOK_STRING
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in iname clause");
			yyerror(mess);
		    }
		| TOK_INAME
		    {
			pmsprintf(mess, sizeof(mess), "Expecting old external instance name string in iname clause");
			yyerror(mess);
		    }
		| TOK_INST number TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting new internal instance identifier or DELETE in inst clause");
			yyerror(mess);
		    }
		| TOK_INST number
		    {
			pmsprintf(mess, sizeof(mess), "Expecting -> in inst clause");
			yyerror(mess);
		    }
		| TOK_INST
		    {
			pmsprintf(mess, sizeof(mess), "Expecting old internal instance identifier in inst clause");
			yyerror(mess);
		    }
		;

duplicateopt	: TOK_DUPLICATE 	{ $$ = INDOM_DUPLICATE; }
		|			{ $$ = 0; }
		;

metricspec	: TOK_METRIC pmid_or_name
		    {
			if (current_star_metric) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*node;

			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
    /*
     * Note: This application is single threaded, and once we have ctxp
     *	     the associated __pmContext will not move and will only be
     *	     accessed or modified synchronously either here or in libpcp.
     *	     We unlock the context so that it can be locked as required
     *	     within libpcp.
     */
			    PM_UNLOCK(ctxp->c_lock);
			    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
			    star_domain = pmid_domain($2);
			    if (current_star_metric == 1)
				star_cluster = pmid_cluster($2);
			    else
				star_cluster = PM_ID_NULL;
			    for (node = __pmHashWalk(hcp, PM_HASH_WALK_START);
				 node != NULL;
				 node = __pmHashWalk(hcp, PM_HASH_WALK_NEXT)) {
				if (pmid_domain((pmID)(node->key)) == star_domain &&
				    (star_cluster == PM_ID_NULL ||
				     star_cluster == pmid_cluster((pmID)(node->key))))
				    current_metricspec = start_metric((pmID)(node->key));
			    }
			    do_walk_metric = 1;
			}
			else {
			    if ($2 == PM_ID_NULL)
				/* metric not in archive */
				current_metricspec = NULL;
			    else
				current_metricspec = start_metric($2);
			    do_walk_metric = 0;
			}
		    }
			TOK_LBRACE optmetricoptlist TOK_RBRACE
		| TOK_METRIC
		    {
			pmsprintf(mess, sizeof(mess), "Expecting metric name or <domain>.<cluster>.<item> or <domain>.<cluster>.* or <domain>.*.* in metric rule");
			yyerror(mess);
		    }
		;

pmid_or_name	: pmid_int
		|  TOK_GNAME
		    {
			int	sts;
			pmID	pmid;
			sts = pmLookupName(1, &$1, &pmid);
			if (sts < 0) {
			    if (wflag) {
				pmsprintf(mess, sizeof(mess), "Metric: %s: %s", $1, pmErrStr(sts));
				yywarn(mess);
			    }
			    pmid = PM_ID_NULL;
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid;
		    }
		;

pmid_int	: TOK_PMID_INT
		    {
			int	domain;
			int	cluster;
			int	item;
			int	sts;
			sts = sscanf($1, "%d.%d.%d", &domain, &cluster, &item);
			assert(sts == 3);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			if (cluster < 0 || cluster >= 4096) {
			    pmsprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
			    yyerror(mess);
			}
			if (item < 0 || item >= 1024) {
			    pmsprintf(mess, sizeof(mess), "Illegal item field (%d) for pmid", item);
			    yyerror(mess);
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid_build(domain, cluster, item);
		    }
		| TOK_PMID_STAR
		    {
			int	domain;
			int	cluster;
			int	sts;
			sts = sscanf($1, "%d.%d.", &domain, &cluster);
			if (domain < 1 || domain > DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			else if (domain == DYNAMIC_PMID) {
			    pmsprintf(mess, sizeof(mess), "Dynamic metric domain field (%d) for pmid", domain);
			    yywarn(mess);
			}
			if (sts == 2) {
			    if (cluster >= 4096) {
				pmsprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
				yyerror(mess);
			    }
			    current_star_metric = 1;
			}
			else {
			    cluster = 0;
			    current_star_metric = 2;
			}
			free($1);
			$$ = pmid_build(domain, cluster, 0);
		    }
		;

optmetricoptlist	: metricoptlist
			| /* nothing */
			;

metricoptlist	: metricopt
		| metricopt metricoptlist
		;

metricopt	: TOK_PMID TOK_ASSIGN pmid_int
		    {
			metricspec_t	*mp;
			pmID		pmid;
			for (mp = walk_metric(W_START, METRIC_CHANGE_PMID, "pmid", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_PMID, "pmid", 0)) {
			    if (current_star_metric == 1)
				pmid = pmid_build(pmid_domain($3), pmid_cluster($3), pmid_item(mp->old_desc.pmid));
			    else if (current_star_metric == 2)
				pmid = pmid_build(pmid_domain($3), pmid_cluster(mp->old_desc.pmid), pmid_item(mp->old_desc.pmid));
			    else
				pmid = $3;
			    if (pmid == mp->old_desc.pmid) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): pmid: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.pmid = pmid;
				mp->flags |= METRIC_CHANGE_PMID;
			    }
			}
		    }
		| TOK_NAME TOK_ASSIGN TOK_GNAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_NAME, "name", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_NAME, "name", 0)) {
			    if (strcmp($3, mp->old_name) == 0) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): name: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				int	sts;
				pmID	pmid;
				sts = pmLookupName(1, &$3, &pmid);
				if (sts >= 0) {
				    pmsprintf(mess, sizeof(mess), "Metric name %s already assigned for PMID %s", $3, pmIDStr(pmid));
				    yyerror(mess);
				}
				mp->new_name = $3;
				mp->flags |= METRIC_CHANGE_NAME;
			    }
			}
		    }
		| TOK_TYPE TOK_ASSIGN TOK_TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type", 0)) {
			    if ($3 == mp->old_desc.type) {
				/* old == new, so no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): type: PM_TYPE_%s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmTypeStr(mp->old_desc.type));
				    yywarn(mess);
				}
			    }
			    else {
				if (mp->old_desc.type == PM_TYPE_32 ||
				    mp->old_desc.type == PM_TYPE_U32 ||
				    mp->old_desc.type == PM_TYPE_64 ||
				    mp->old_desc.type == PM_TYPE_U64 ||
				    mp->old_desc.type == PM_TYPE_FLOAT ||
				    mp->old_desc.type == PM_TYPE_DOUBLE) {
				    mp->new_desc.type = $3;
				    mp->flags |= METRIC_CHANGE_TYPE;
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Old type (PM_TYPE_%s) must be numeric", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
			    }
			}
		    }
		| TOK_TYPE TOK_IF TOK_TYPE_NAME TOK_ASSIGN TOK_TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type", 1); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type", 1)) {
			    if (mp->old_desc.type != $3) {
				if (wflag) {
				    char	tbuf0[20];
				    char	tbuf1[20];
				    pmTypeStr_r(mp->old_desc.type, tbuf0, sizeof(tbuf0));
				    pmTypeStr_r($5, tbuf1, sizeof(tbuf1));
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): type: PM_TYPE_%s: No conditional change to PM_TYPE_%s", mp->old_name, pmIDStr(mp->old_desc.pmid), tbuf0, tbuf1);
				    yywarn(mess);
				}
			    }
			    else {
				if (mp->old_desc.type == PM_TYPE_32 ||
				    mp->old_desc.type == PM_TYPE_U32 ||
				    mp->old_desc.type == PM_TYPE_64 ||
				    mp->old_desc.type == PM_TYPE_U64 ||
				    mp->old_desc.type == PM_TYPE_FLOAT ||
				    mp->old_desc.type == PM_TYPE_DOUBLE) {
				    mp->new_desc.type = $5;
				    mp->flags |= METRIC_CHANGE_TYPE;
				}
				else {
				    pmsprintf(mess, sizeof(mess), "Old type (PM_TYPE_%s) must be numeric", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
			    }
			}
		    }
		| TOK_INDOM TOK_ASSIGN null_or_indom pick
		    {
			metricspec_t	*mp;
			pmInDom		indom;
			for (mp = walk_metric(W_START, METRIC_CHANGE_INDOM, "indom", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_INDOM, "indom", 0)) {
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(mp->old_desc.indom));
			    else
				indom = $3;
			    if (indom == mp->old_desc.indom) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): indom: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmInDomStr(mp->old_desc.indom));
				    yywarn(mess);
				}
			    }
			    else {
				if ((output == OUTPUT_MIN ||
					  output == OUTPUT_MAX ||
					  output == OUTPUT_SUM ||
					  output == OUTPUT_AVG) &&
					 mp->old_desc.type != PM_TYPE_32 &&
					 mp->old_desc.type != PM_TYPE_U32 &&
					 mp->old_desc.type != PM_TYPE_64 &&
					 mp->old_desc.type != PM_TYPE_U64 &&
					 mp->old_desc.type != PM_TYPE_FLOAT &&
					 mp->old_desc.type != PM_TYPE_DOUBLE) {
				    pmsprintf(mess, sizeof(mess), "OUTPUT option MIN, MAX, AVG or SUM requires type to be numeric, not PM_TYPE_%s", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
				mp->new_desc.indom = indom;
				mp->flags |= METRIC_CHANGE_INDOM;
				mp->output = output;
				if (output == OUTPUT_ONE) {
				    mp->one_name = one_name;
				    mp->one_inst = one_inst;
				    if (mp->old_desc.indom == PM_INDOM_NULL)
					/*
					 * singular input, pick first (only)
					 * value, not one_inst matching ...
					 * one_inst used for output instance
					 * id
					 */
					mp->output = OUTPUT_FIRST;
				}
				if (output == OUTPUT_ALL) {
				    /*
				     * No OUTPUT clause, set up the defaults
				     * based on indom types:
				     * non-NULL -> NULL
				     *		OUTPUT_FIRST, inst PM_IN_NULL
				     * NULL -> non-NULL
				     *		OUTPUT_FIRST, inst 0
				     * non-NULL -> non-NULL
				     * 		all instances selected
				     *		(nothing to do for defaults)
				     * NULL -> NULL
				     *		caught above in no change case
				     */
				    if (mp->old_desc.indom != PM_INDOM_NULL &&
				        mp->new_desc.indom == PM_INDOM_NULL) {
					mp->output = OUTPUT_FIRST;
					mp->one_inst = PM_IN_NULL;
				    }
				    else if (mp->old_desc.indom == PM_INDOM_NULL &&
				             mp->new_desc.indom != PM_INDOM_NULL) {
					mp->output = OUTPUT_FIRST;
					mp->one_inst = 0;
				    }
				}
			    }
			}
			output = OUTPUT_ALL;	/* for next time */
		    }
		| TOK_SEM TOK_ASSIGN TOK_SEM_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_SEM, "sem", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_SEM, "sem", 0)) {
			    if ($3 == mp->old_desc.sem) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): sem: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), SemStr(mp->old_desc.sem));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.sem = $3;
				mp->flags |= METRIC_CHANGE_SEM;
			    }
			}
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA TOK_TIME_NAME TOK_COMMA TOK_COUNT_NAME rescaleopt
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_UNITS, "units", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_UNITS, "units", 0)) {
			    if ($3 == mp->old_desc.units.dimSpace &&
			        $5 == mp->old_desc.units.dimTime &&
			        $7 == mp->old_desc.units.dimCount &&
			        $9 == mp->old_desc.units.scaleSpace &&
			        $11 == mp->old_desc.units.scaleTime &&
			        $13 == mp->old_desc.units.scaleCount) {
				/* no change ... */
				if (wflag) {
				    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): units: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmUnitsStr(&mp->old_desc.units));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.units.dimSpace = $3;
				mp->new_desc.units.dimTime = $5;
				mp->new_desc.units.dimCount = $7;
				mp->new_desc.units.scaleSpace = $9;
				mp->new_desc.units.scaleTime = $11;
				mp->new_desc.units.scaleCount = $13;
				mp->flags |= METRIC_CHANGE_UNITS;
				if ($14 == 1) {
				    if ($3 == mp->old_desc.units.dimSpace &&
					$5 == mp->old_desc.units.dimTime &&
					$7 == mp->old_desc.units.dimCount)
					/* OK, no dim changes */
					mp->flags |= METRIC_RESCALE;
				    else {
					if (wflag) {
					    pmsprintf(mess, sizeof(mess), "Metric: %s (%s): Dimension changed, cannot rescale", mp->old_name, pmIDStr(mp->old_desc.pmid));
					    yywarn(mess);
					}
				    }
				}
				else if (sflag) {
				    if ($3 == mp->old_desc.units.dimSpace &&
					$5 == mp->old_desc.units.dimTime &&
					$7 == mp->old_desc.units.dimCount)
					mp->flags |= METRIC_RESCALE;
				}
			    }
			}
		    }
		| TOK_DELETE
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_DELETE, "delete", 0); mp != NULL; mp = walk_metric(W_NEXT, METRIC_DELETE, "delete", 0)) {
			    mp->flags |= METRIC_DELETE;
			}
		    }
		| TOK_PMID TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<cluster>.<item> or <domain>.<cluster>.* or <domain>.*.* in pmid clause");
			yyerror(mess);
		    }
		| TOK_NAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting metric name in iname clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) in type clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_IF
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) after if in type clause");
			yyerror(mess);
		    }
		| TOK_TYPE TOK_IF TOK_TYPE_NAME TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) in type clause");
			yyerror(mess);
		    }
		| TOK_INDOM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting <domain>.<serial> or NULL in indom clause");
			yyerror(mess);
		    }
		| TOK_SEM TOK_ASSIGN
		    {
			pmsprintf(mess, sizeof(mess), "Expecting XXX (from PM_SEM_XXX) in sem clause");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN 
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 3 numeric values for dim* fields of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_SPACE_XXX) for scaleSpace field of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_TIME_XXX) for scaleTime field of units");
			yyerror(mess);
		    }
		| TOK_UNITS TOK_ASSIGN signnumber TOK_COMMA signnumber TOK_COMMA signnumber TOK_COMMA TOK_SPACE_NAME TOK_COMMA TOK_TIME_NAME TOK_COMMA
		    {
			pmsprintf(mess, sizeof(mess), "Expecting 0 or ONE for scaleCount field of units");
			yyerror(mess);
		    }
		;

null_or_indom	: indom_int
		| TOK_NULL_INT
		    {
			$$ = PM_INDOM_NULL;
		    }
		;

pick		: TOK_OUTPUT TOK_INST number
		    {
			output = OUTPUT_ONE;
			one_inst = $3;
			one_name = NULL;
		    }
		| TOK_OUTPUT TOK_INAME TOK_STRING
		    {
			output = OUTPUT_ONE;
			one_inst = PM_IN_NULL;
			one_name = $3;
		    }
		| TOK_OUTPUT TOK_OUTPUT_TYPE
		    {
			output = $2;
		    }
		| TOK_OUTPUT
		    {
			pmsprintf(mess, sizeof(mess), "Expecting FIRST or LAST or INST or INAME or MIN or MAX or AVG for OUTPUT instance option");
			yyerror(mess);
		    }
		| /* nothing */
		;

rescaleopt	: TOK_RESCALE { $$ = 1; }
		| /* nothing */
		    { $$ = 0; }
		;


%%
