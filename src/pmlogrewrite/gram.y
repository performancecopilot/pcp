/*
 * Copyright (c) 1997-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
walk_metric(int mode, int flag, char *which)
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
	if (mp->flags & flag) {
	    snprintf(mess, sizeof(mess), "Duplicate %s clause for metric %s", which, mp->old_name);
	    yyerror(mess);
	}
	if (flag != METRIC_DELETE) {
	    if (mp->flags & METRIC_DELETE) {
		snprintf(mess, sizeof(mess), "Conflicting %s clause for deleted metric %s", which, mp->old_name);
		yyerror(mess);
	    }
	}
	else {
	    if (mp->flags & (~METRIC_DELETE)) {
		snprintf(mess, sizeof(mess), "Conflicting delete and other clauses for metric %s", mp->old_name);
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

%token	LBRACE
	RBRACE
	PLUS
	MINUS
	DOT
	COLON
	COMMA
	ASSIGN
	GLOBAL
	INDOM
	METRIC
	HOSTNAME
	TZ
	TIME
	NAME
	INST
	DELETE
	PMID
	NULL_INT
	TYPE
	SEM
	UNITS
	OUTPUT

%token<str>	GNAME NUMBER STRING HNAME FLOAT INDOM_STAR PMID_INT PMID_STAR
%token<ival>	TYPE_NAME SEM_NAME SPACE_NAME TIME_NAME COUNT_NAME OUTPUT_TYPE

%type<str>	hname
%type<indom>	indom_int null_or_indom
%type<pmid>	pmid_int pmid_or_name
%type<ival>	signnumber number
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

globalspec	: GLOBAL LBRACE globaloptlist RBRACE
		| GLOBAL LBRACE RBRACE
		;

globaloptlist	: globalopt
		| globalopt globaloptlist
		;

globalopt	: HOSTNAME ASSIGN hname
		    {
			if (global.flags & GLOBAL_CHANGE_HOSTNAME) {
			    snprintf(mess, sizeof(mess), "Duplicate global hostname clause");
			    yyerror(mess);
			}
			strncpy(global.hostname, $3, sizeof(global.hostname));
			free($3);
			global.flags |= GLOBAL_CHANGE_HOSTNAME;
		    }
		| TZ ASSIGN STRING
		    {
			if (global.flags & GLOBAL_CHANGE_TZ) {
			    snprintf(mess, sizeof(mess), "Duplicate global tz clause");
			    yyerror(mess);
			}
			strncpy(global.tz, $3, sizeof(global.tz));
			free($3);
			global.flags |= GLOBAL_CHANGE_TZ;
		    }
		| TIME ASSIGN signtime
		    {
			if (global.flags & GLOBAL_CHANGE_TIME) {
			    snprintf(mess, sizeof(mess), "Duplicate global time clause");
			    yyerror(mess);
			}
			global.flags |= GLOBAL_CHANGE_TIME;
		    }
		| HOSTNAME ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting hostname in hostname clause");
			yyerror(mess);
		    }
		| HOSTNAME
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in hostname clause");
			yyerror(mess);
		    }
		| TZ ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting timezone string in tz clause");
			yyerror(mess);
		    }
		| TZ
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in tz clause");
			yyerror(mess);
		    }
		| TIME ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting delta of the form [+-][HH:[MM:]]SS[.d...] in time clause");
			yyerror(mess);
		    }
		| TIME
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in time clause");
			yyerror(mess);
		    }
		;

	/*
	 * ambiguity in lexical scanner ... handle here
	 * abc.def - is HNAME or GNAME
	 * 123 - is HNAME or NUMBER
	 * 123.456 - is HNAME or FLOAT
	 */
hname		: HNAME
		| GNAME
		| NUMBER
		| FLOAT
		;

signnumber	: PLUS NUMBER
		    {
			$$ = atoi($2);
			free($2);
		    }
		| MINUS NUMBER
		    {
			$$ = -atoi($2);
			free($2);
		    }
		| NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

number		: NUMBER
		    {
			$$ = atoi($1);
			free($1);
		    }
		;

float		: FLOAT
		    {
			$$ = atof($1);
			free($1);
		    }
		;

signtime	: PLUS time
		| MINUS time { global.time.tv_sec = -global.time.tv_sec; }
		| time
		;

time		: number COLON number COLON float	/* HH:MM:SS.d.. format */
		    { 
			if ($3 > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + (int)$5;
			global.time.tv_usec = (int)(1000000*(($5 - (int)$5))+0.5);
		    }
		| number COLON number COLON number	/* HH:MM:SS format */
		    { 
			if ($3 > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			if ($5 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $5);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 3600 + $3 * 60 + $5;
		    }
		| number COLON float		/* MM:SS.d.. format */
		    { 
			if ($1 > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + (int)$3;
			global.time.tv_usec = (int)(1000000*(($3 - (int)$3))+0.5);
		    }
		| number COLON number		/* MM:SS format */
		    { 
			if ($1 > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			if ($3 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $3);
			    yywarn(mess);
			}
			global.time.tv_sec = $1 * 60 + $3;
		    }
		| float			/* SS.d.. format */
		    {
			if ($1 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = (int)$1;
			global.time.tv_usec = (int)(1000000*(($1 - (int)$1))+0.5);
		    }
		| number		/* SS format */
		    {
			if ($1 > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", $1);
			    yywarn(mess);
			}
			global.time.tv_sec = $1;
			global.time.tv_usec = 0;
		    }
		;

indomspec	: INDOM indom_int
		    {
			if (current_star_indom) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*this;
			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
			    hcp = &ctxp->c_archctl->ac_log->l_hashindom;
			    star_domain = pmInDom_domain($2);
			    for (this = __pmHashWalk(hcp, W_START); this != NULL; this = __pmHashWalk(hcp, W_NEXT)) {
				if (pmInDom_domain((pmInDom)(this->key)) == star_domain)
				    current_indomspec = start_indom((pmInDom)(this->key));
			    }
			    do_walk_indom = 1;
			}
			else {
			    current_indomspec = start_indom($2);
			    do_walk_indom = 0;
			}
		    }
			LBRACE optindomopt RBRACE
		| INDOM
		    {
			snprintf(mess, sizeof(mess), "Expecting <d>.<s> or <d>.* in indom clause");
			yyerror(mess);
		    }
		;

indom_int	: FLOAT
		    {
			int		domain;
			int		serial;
			int		sts;
			sts = sscanf($1, "%d.%d", &domain, &serial);
			if (sts < 2) {
			    snprintf(mess, sizeof(mess), "Missing serial field for indom");
			    yyerror(mess);
			}
			if (domain >= DYNAMIC_PMID) {
			    snprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
			    yyerror(mess);
			}
			if (serial >= 4194304) {
			    snprintf(mess, sizeof(mess), "Illegal serial field (%d) for indom", serial);
			    yyerror(mess);
			}
			current_star_indom = 0;
			free($1);
			$$ = pmInDom_build(domain, serial);
		    }
		| INDOM_STAR
		    {
			int		domain;
			sscanf($1, "%d.", &domain);
			if (domain >= DYNAMIC_PMID) {
			    snprintf(mess, sizeof(mess), "Illegal domain field (%d) for indom", domain);
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

indomopt	: INDOM ASSIGN indom_int
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    pmInDom	indom;
			    if (indom_root->new_indom != indom_root->old_indom) {
				snprintf(mess, sizeof(mess), "Duplicate indom clause for indom %s", pmInDomStr(indom_root->old_indom));
				yyerror(mess);
			    }
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(ip->old_indom));
			    else
				indom = $3;
			    if (indom != ip->old_indom)
				ip->new_indom = indom;
			    else {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Instance domain %s: indom: No change", pmInDomStr(ip->old_indom));
				    yywarn(mess);
				}
			    }
			}
		    }
		| NAME STRING ASSIGN STRING
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
			free($2);
			/* Note: $4 referenced from new_iname[] */
		    }
		| NAME STRING ASSIGN DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, NULL) < 0)
			    	yyerror(mess);
			}
			free($2);
		    }
		| INST number ASSIGN number
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
		    }
		| INST number ASSIGN DELETE
		    {
			indomspec_t	*ip;
			for (ip = walk_indom(W_START); ip != NULL; ip = walk_indom(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, $2, PM_IN_NULL) < 0)
				yyerror(mess);
			}
		    }
		| INDOM ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting <d>.<s> or <d>.* in indom clause");
			yyerror(mess);
		    }
		| INDOM
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in indom clause");
			yyerror(mess);
		    }
		| NAME STRING ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting new external instance name string or DELETE in name clause");
			yyerror(mess);
		    }
		| NAME STRING
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in name clause");
			yyerror(mess);
		    }
		| NAME
		    {
			snprintf(mess, sizeof(mess), "Expecting old external instance name string in name clause");
			yyerror(mess);
		    }
		| INST number ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting new internal instance identifier or DELETE in inst clause");
			yyerror(mess);
		    }
		| INST number
		    {
			snprintf(mess, sizeof(mess), "Expecting -> in inst clause");
			yyerror(mess);
		    }
		| INST
		    {
			snprintf(mess, sizeof(mess), "Expecting old internal instance identifier in inst clause");
			yyerror(mess);
		    }
		;

metricspec	: METRIC pmid_or_name
		    {
			if (current_star_metric) {
			    __pmContext		*ctxp;
			    __pmHashCtl		*hcp;
			    __pmHashNode	*this;
			    ctxp = __pmHandleToPtr(pmWhichContext());
			    assert(ctxp != NULL);
			    hcp = &ctxp->c_archctl->ac_log->l_hashpmid;
			    star_domain = pmid_domain($2);
			    if (current_star_metric == 1)
				star_cluster = pmid_cluster($2);
			    else
				star_cluster = PM_ID_NULL;
			    for (this = __pmHashWalk(hcp, W_START); this != NULL; this = __pmHashWalk(hcp, W_NEXT)) {
				if (pmid_domain((pmID)(this->key)) == star_domain &&
				    (star_cluster == PM_ID_NULL ||
				     star_cluster == pmid_cluster((pmID)(this->key))))
				    current_metricspec = start_metric((pmID)(this->key));
			    }
			    do_walk_metric = 1;
			}
			else {
			    if ($2 != PM_ID_NULL)
				current_metricspec = start_metric($2);
			    do_walk_metric = 0;
			}
		    }
			LBRACE optmetricoptlist RBRACE
		| METRIC
		    {
			snprintf(mess, sizeof(mess), "Expecting metric name or <d>.<c>.<i> or <d>.<c>.* or <d>.*.* in metric clause");
			yyerror(mess);
		    }
		;

pmid_or_name	: pmid_int
		|  GNAME
		    {
			int	sts;
			pmID	pmid;
			sts = pmLookupName(1, &$1, &pmid);
			if (sts < 0) {
			    if (wflag) {
				snprintf(mess, sizeof(mess), "Metric: %s: %s", $1, pmErrStr(sts));
				yywarn(mess);
			    }
			    pmid = PM_ID_NULL;
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid;
		    }
		;

pmid_int	: PMID_INT
		    {
			int	domain;
			int	cluster;
			int	item;
			int	sts;
			sts = sscanf($1, "%d.%d.%d", &domain, &cluster, &item);
			assert(sts == 3);
			if (domain >= DYNAMIC_PMID) {
			    snprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			if (cluster >= 4096) {
			    snprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
			    yyerror(mess);
			}
			if (item >= 1024) {
			    snprintf(mess, sizeof(mess), "Illegal item field (%d) for pmid", item);
			    yyerror(mess);
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid_build(domain, cluster, item);
		    }
		| PMID_STAR
		    {
			int	domain;
			int	cluster;
			int	sts;
			sts = sscanf($1, "%d.%d.", &domain, &cluster);
			if (domain >= DYNAMIC_PMID) {
			    snprintf(mess, sizeof(mess), "Illegal domain field (%d) for pmid", domain);
			    yyerror(mess);
			}
			if (sts == 2) {
			    if (cluster >= 4096) {
				snprintf(mess, sizeof(mess), "Illegal cluster field (%d) for pmid", cluster);
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

metricopt	: PMID ASSIGN pmid_int
		    {
			metricspec_t	*mp;
			pmID		pmid;
			for (mp = walk_metric(W_START, METRIC_CHANGE_PMID, "pmid"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_PMID, "pmid")) {
			    if (current_star_metric == 1)
				pmid = pmid_build(pmid_domain($3), pmid_cluster($3), pmid_item(mp->old_desc.pmid));
			    else if (current_star_metric == 2)
				pmid = pmid_build(pmid_domain($3), pmid_cluster(mp->old_desc.pmid), pmid_item(mp->old_desc.pmid));
			    else
				pmid = $3;
			    if (pmid == mp->old_desc.pmid) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): pmid: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.pmid = pmid;
				mp->flags |= METRIC_CHANGE_PMID;
			    }
			}
		    }
		| NAME ASSIGN GNAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_NAME, "name"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_NAME, "name")) {
			    if (strcmp($3, mp->old_name) == 0) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): name: No change", mp->old_name, pmIDStr(mp->old_desc.pmid));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_name = $3;
				mp->flags |= METRIC_CHANGE_NAME;
			    }
			}
		    }
		| TYPE ASSIGN TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type")) {
			    if ($3 == mp->old_desc.type) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): type: PM_TYPE_%s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmTypeStr(mp->old_desc.type));
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
				    snprintf(mess, sizeof(mess), "Old type (PM_TYPE_%s) must be numeric", pmTypeStr(mp->old_desc.type));
				    yyerror(mess);
				}
			    }
			}
		    }
		| INDOM ASSIGN null_or_indom
		    {
			metricspec_t	*mp;
			pmInDom		indom;
			for (mp = walk_metric(W_START, METRIC_CHANGE_INDOM, "indom"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_INDOM, "indom")) {
			    if (current_star_indom)
				indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(mp->old_desc.indom));
			    else
				indom = $3;
			    if (indom == mp->old_desc.indom) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): indom: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmInDomStr(mp->old_desc.indom));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.indom = indom;
				mp->flags |= METRIC_CHANGE_INDOM;
				mp->output = output;
				if (mp->old_desc.indom == PM_INDOM_NULL) {
				    if (output == OUTPUT_ONE) {
					mp->output = OUTPUT_FIRST;
					mp->one_inst = one_inst;
				    }
				    else if (output == OUTPUT_ALL) {
					/* default */
					mp->output = OUTPUT_FIRST;
					mp->one_inst = 0;
				    }
				    else {
					snprintf(mess, sizeof(mess), "OUTPUT option requires metric to be singular, not indom %s", pmInDomStr(mp->old_desc.indom));
					yyerror(mess);
				    }
				}
				if (mp->new_desc.indom == PM_INDOM_NULL) {
				    if (output == OUTPUT_ALL)
					mp->output = OUTPUT_FIRST;
				    else if (output == OUTPUT_ONE)
					mp->one_inst = one_inst;
				    else if ((output == OUTPUT_MIN ||
					      output == OUTPUT_MAX ||
					      output == OUTPUT_AVG) &&
					     mp->old_desc.type != PM_TYPE_32 &&
					     mp->old_desc.type != PM_TYPE_U32 &&
					     mp->old_desc.type != PM_TYPE_64 &&
					     mp->old_desc.type != PM_TYPE_U64 &&
					     mp->old_desc.type != PM_TYPE_FLOAT &&
					     mp->old_desc.type != PM_TYPE_DOUBLE) {
					snprintf(mess, sizeof(mess), "OUTPUT option MIN, MAX or AVG requires type to be numeric, not PM_TYPE_%s", pmTypeStr(mp->old_desc.type));
					yyerror(mess);
				    }
				}
				if (mp->old_desc.indom != PM_INDOM_NULL &&
				    mp->new_desc.indom != PM_INDOM_NULL &&
				    output != OUTPUT_ALL) {
				    snprintf(mess, sizeof(mess), "OUTPUT option requires input or output metric to be singular");
				    yyerror(mess);
				}
			    }
			}
			output = OUTPUT_ALL;	/* for next time */
		    }
		| SEM ASSIGN SEM_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_SEM, "sem"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_SEM, "sem")) {
			    if ($3 == mp->old_desc.sem) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): sem: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), SemStr(mp->old_desc.sem));
				    yywarn(mess);
				}
			    }
			    else {
				mp->new_desc.sem = $3;
				mp->flags |= METRIC_CHANGE_SEM;
			    }
			}
		    }
		| UNITS ASSIGN signnumber COMMA signnumber COMMA signnumber COMMA SPACE_NAME COMMA TIME_NAME COMMA COUNT_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_UNITS, "units"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_UNITS, "units")) {
			    if ($3 == mp->old_desc.units.dimSpace &&
			        $5 == mp->old_desc.units.dimTime &&
			        $7 == mp->old_desc.units.dimCount &&
			        $9 == mp->old_desc.units.scaleSpace &&
			        $11 == mp->old_desc.units.scaleTime &&
			        $13 == mp->old_desc.units.scaleCount) {
				/* no change ... */
				if (wflag) {
				    snprintf(mess, sizeof(mess), "Metric: %s (%s): units: %s: No change", mp->old_name, pmIDStr(mp->old_desc.pmid), pmUnitsStr(&mp->old_desc.units));
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
			    }
			}
		    }
		| DELETE
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_DELETE, "delete"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_DELETE, "delete")) {
			    mp->flags |= METRIC_DELETE;
			}
		    }
		| PMID ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting <d>.<c>.<i> or <d>.<c>.* or <d>.*.* in pmid clause");
			yyerror(mess);
		    }
		| NAME ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting metric name in name clause");
			yyerror(mess);
		    }
		| TYPE ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting XXX (from PM_TYPE_XXX) in type clause");
			yyerror(mess);
		    }
		| INDOM ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting <d>.<s> or <d>.* or NULL in indom clause");
			yyerror(mess);
		    }
		| SEM ASSIGN
		    {
			snprintf(mess, sizeof(mess), "Expecting XXX (from PM_SEM_XXX) in sem clause");
			yyerror(mess);
		    }
		| UNITS ASSIGN 
		    {
			snprintf(mess, sizeof(mess), "Expecting 3 numeric values for dim* fields of units");
			yyerror(mess);
		    }
		| UNITS ASSIGN signnumber COMMA signnumber COMMA signnumber COMMA
		    {
			snprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_SPACE_XXX) for scaleSpace field of units");
			yyerror(mess);
		    }
		| UNITS ASSIGN signnumber COMMA signnumber COMMA signnumber COMMA SPACE_NAME COMMA
		    {
			snprintf(mess, sizeof(mess), "Expecting 0 or XXX (from PM_TIME_XXX) for scaleTime field of units");
			yyerror(mess);
		    }
		| UNITS ASSIGN signnumber COMMA signnumber COMMA signnumber COMMA SPACE_NAME COMMA TIME_NAME COMMA
		    {
			snprintf(mess, sizeof(mess), "Expecting 0 or ONE for scaleCount field of units");
			yyerror(mess);
		    }
		;

null_or_indom	: NULL_INT
		    {
			$$ = PM_INDOM_NULL;
		    }
		| NULL_INT OUTPUT INST number
		    {
			output = OUTPUT_ONE;
			one_inst = $4;
			$$ = PM_INDOM_NULL;
		    }
		| NULL_INT OUTPUT OUTPUT_TYPE
		    {
			output = $3;
			$$ = PM_INDOM_NULL;
		    }
		| NULL_INT OUTPUT
		    {
			snprintf(mess, sizeof(mess), "Expecting FIRST or LAST or INST or MIN or MAX or AVG for OUTPUT instance option");
			yyerror(mess);
		    }
		| indom_int
		| indom_int OUTPUT INST number
		    {
			output = OUTPUT_ONE;
			one_inst = $4;
		    }
		| indom_int OUTPUT
		    {
			snprintf(mess, sizeof(mess), "Expecting INST for OUTPUT instance option");
			yyerror(mess);
		    }
		;

%%
