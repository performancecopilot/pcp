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

extern int	lineno;

static void
start_indom(pmInDom indom)
{
    indomspec_t	*ip;
    int		i;
    int		sts;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    if (ip == NULL) {
	ip = (indomspec_t *)malloc(sizeof(indomspec_t));
	if (ip == NULL) {
	    fprintf(stderr, "indomspec malloc(%d) failed: %s\n", (int)sizeof(indomspec_t), strerror(errno));
	    exit(1);
	}
	ip->i_next = indom_root;
	indom_root = ip;
	ip->old_indom = indom;
	ip->new_indom = PM_INDOM_NULL;
	sts = pmGetInDomArchive(indom, &ip->old_inst, &ip->old_iname);
	if (sts < 0) {
	    /* no need to clean up, this is fatal */
	    fprintf(stderr, "Instance domain %s: %s\n", pmInDomStr(indom), pmErrStr(sts));
	    exit(1);
	}
	ip->numinst = sts;
	ip->flags = (int *)malloc(ip->numinst*sizeof(int));
	if (ip->flags == NULL) {
	    fprintf(stderr, "indomspec flags malloc(%d) failed: %s\n", ip->numinst*(int)sizeof(int), strerror(errno));
	    exit(1);
	}
	ip->new_inst = (int *)malloc(ip->numinst*sizeof(int));
	if (ip->new_inst == NULL) {
	    fprintf(stderr, "new_inst malloc(%d) failed: %s\n", ip->numinst*(int)sizeof(int), strerror(errno));
	    exit(1);
	}
	ip->new_iname = (char **)malloc(ip->numinst*sizeof(char *));
	if (ip->new_iname == NULL) {
	    fprintf(stderr, "new_iname malloc(%d) failed: %s\n", ip->numinst*(int)sizeof(char *), strerror(errno));
	    exit(1);
	}
	for (i = 0; i < ip->numinst; i++)
	    ip->flags[i] = 0;
    }
    current_indomspec = ip;
}

static void
start_metric(pmID pmid)
{
    metricspec_t	*mp;
    int			sts;

    for (mp = metric_root; mp != NULL; mp = mp->m_next) {
	if (pmid == mp->old_desc.pmid)
	    break;
    }
    if (mp == NULL) {
	mp = (metricspec_t *)malloc(sizeof(metricspec_t));
	if (mp == NULL) {
	    fprintf(stderr, "metricspec malloc(%d) failed: %s\n", (int)sizeof(metricspec_t), strerror(errno));
	    exit(1);
	}
	mp->m_next = metric_root;
	metric_root = mp;
	sts = pmNameID(pmid, &mp->old_name);
	if (sts < 0) {
	    /* no need to clean up, this is fatal */
	    fprintf(stderr, "Metric %s: pmNameID failed: %s\n", pmIDStr(pmid), pmErrStr(sts));
	    exit(1);
	}
	sts = pmLookupDesc(pmid, &mp->old_desc);
	if (sts < 0) {
	    /* no need to clean up, this is fatal */
	    fprintf(stderr, "Metric %s: pmLookupDesc failed: %s\n", mp->old_name, pmErrStr(sts));
	    exit(1);
	}
	mp->new_desc = mp->old_desc;
	mp->flags = 0;
    }
    current_metricspec = mp;
}

static int
change_inst_by_name(pmInDom indom, char *old, char *new)
{
    int		i;
    indomspec_t	*ip;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    assert(ip != NULL);

    for (i = 0; i < ip->numinst; i++) {
	char	*p;
	char	*q;
	int	match = 0;
	for (p = ip->old_iname[i], q = old; ; p++, q++) {
	    if (*p == '\0' || *p == ' ') {
		if (*q == '\0' || *q == ' ')
		    match = 1;
		break;
	    }
	    if (*q == '\0' || *q == ' ') {
		if (*p == '\0' || *p == ' ')
		    match = 1;
		break;
	    }
	    if (*p != *q)
		break;
	}
	if (match) {
	    if ((new == NULL && ip->flags[i]) ||
	        (ip->flags[i] & (INST_CHANGE_INAME|INST_DELETE))) {
		sprintf(mess, "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	sprintf(mess, "Unknown instance \"%s\" in name clause for indom %s", old, pmInDomStr(indom));
	return -1;
    }

    if (new == NULL) {
	ip->flags[i] |= INST_DELETE;
    }
    else {
	ip->flags[i] |= INST_CHANGE_INAME;
    }
    ip->new_iname[i] = new;

    return 0;
}

static int
change_inst_by_inst(pmInDom indom, int old, int new)
{
    int		i;
    indomspec_t	*ip;

    for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	if (indom == ip->old_indom)
	    break;
    }
    assert(ip != NULL);

    for (i = 0; i < ip->numinst; i++) {
	if (ip->old_inst[i] == old) {
	    if ((new == PM_IN_NULL && ip->flags[i]) ||
	        (ip->flags[i] & (INST_CHANGE_INST|INST_DELETE))) {
		sprintf(mess, "Duplicate or conflicting clauses for instance [%d] \"%s\" of indom %s",
		    ip->old_inst[i], ip->old_iname[i], pmInDomStr(indom));
		return -1;
	    }
	    break;
	}
    }
    if (i == ip->numinst) {
	sprintf(mess, "Unknown instance %d in inst clause for indom %s", old, pmInDomStr(indom));
	return -1;
    }

    if (new == PM_IN_NULL) {
	ip->flags[i] |= INST_DELETE;
    }
    else {
	ip->flags[i] |= INST_CHANGE_INST;
    }
    ip->new_inst[i] = new;

    return 0;
}

#define W_START	1
#define W_NEXT	2

__pmHashNode *
__pmHashWalk(__pmHashCtl *hcp, int mode)
{
    static int		hash_idx;
    static __pmHashNode	*next;
    __pmHashNode	*this;

    if (mode == W_START) {
	hash_idx = 0;
	next = hcp->hash[0];
    }

    while (next == NULL) {
	hash_idx++;
	if (hash_idx >= hcp->hsize)
	    return NULL;
	next = hcp->hash[hash_idx];
    }

    this = next;
    next = next->next;

    return this;
}

static indomspec_t *
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

static metricspec_t *
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

%token<str>	GNAME NUMBER STRING HNAME FLOAT INDOM_STAR PMID_INT PMID_STAR
%token<ival>	TYPE_NAME SEM_NAME SPACE_NAME TIME_NAME COUNT_NAME

%type<str>	hname
%type<indom>	indom_int null_or_indom
%type<pmid>	pmid_int
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
				    start_indom((pmInDom)(this->key));
			    }
			    do_walk_indom = 1;
			}
			else {
			    start_indom($2);
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
			    if (indom_root->new_indom != PM_INDOM_NULL) {
				snprintf(mess, sizeof(mess), "Duplicate indom clause for indom %s", pmInDomStr(indom_root->old_indom));
				yyerror(mess);
			    }
			    if (current_star_indom)
				ip->new_indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(ip->old_indom));
			    else
				ip->new_indom = $3;
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

metricspec	: METRIC pmid_int 
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
				    start_metric((pmID)(this->key));
			    }
			    do_walk_metric = 1;
			}
			else {
			    start_metric($2);
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

pmid_int	: GNAME
		    {
			int	sts;
			pmID	pmid;
			sts = pmLookupName(1, &$1, &pmid);
			if (sts < 0) {
			    /* no need to clean up, this is fatal */
			    fprintf(stderr, "Metric %s: %s\n", $1, pmErrStr(sts));
			    exit(1);
			}
			current_star_metric = 0;
			free($1);
			$$ = pmid;
		    }
		| PMID_INT
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
			for (mp = walk_metric(W_START, METRIC_CHANGE_PMID, "pmid"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_PMID, "pmid")) {
			    if (current_star_metric == 1)
				mp->new_desc.pmid = pmid_build(pmid_domain($3), pmid_cluster($3), pmid_item(mp->old_desc.pmid));
			    else if (current_star_metric == 2)
				mp->new_desc.pmid = pmid_build(pmid_domain($3), pmid_cluster(mp->old_desc.pmid), pmid_item(mp->old_desc.pmid));
			    else
				mp->new_desc.pmid = $3;
			    mp->flags |= METRIC_CHANGE_PMID;
			}
		    }
		| NAME ASSIGN GNAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_NAME, "name"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_NAME, "name")) {
			    mp->new_name = $3;
			    mp->flags |= METRIC_CHANGE_NAME;
			}
		    }
		| TYPE ASSIGN TYPE_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_TYPE, "type"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_TYPE, "type")) {
			    mp->new_desc.type = $3;
			    mp->flags |= METRIC_CHANGE_TYPE;
			}
		    }
		| INDOM ASSIGN null_or_indom
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_INDOM, "indom"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_INDOM, "indom")) {
			    if (current_star_indom)
				mp->new_desc.indom = pmInDom_build(pmInDom_domain($3), pmInDom_serial(mp->old_desc.indom));
			    else
				mp->new_desc.indom = $3;
			    mp->flags |= METRIC_CHANGE_INDOM;
			}
		    }
		| SEM ASSIGN SEM_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_SEM, "sem"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_SEM, "sem")) {
			    mp->new_desc.sem = $3;
			    mp->flags |= METRIC_CHANGE_SEM;
			}
		    }
		| UNITS ASSIGN signnumber COMMA signnumber COMMA signnumber COMMA SPACE_NAME COMMA TIME_NAME COMMA COUNT_NAME
		    {
			metricspec_t	*mp;
			for (mp = walk_metric(W_START, METRIC_CHANGE_UNITS, "units"); mp != NULL; mp = walk_metric(W_NEXT, METRIC_CHANGE_UNITS, "units")) {
			    mp->new_desc.units.dimSpace = $3;
			    mp->new_desc.units.dimTime = $5;
			    mp->new_desc.units.dimCount = $7;
			    mp->new_desc.units.scaleSpace = $9;
			    mp->new_desc.units.scaleTime = $11;
			    mp->new_desc.units.scaleCount = $13;
			    mp->flags |= METRIC_CHANGE_UNITS;
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
		| indom_int
		;

%%
