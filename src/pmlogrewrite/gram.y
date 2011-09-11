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

static int		sign;
static indomspec_t	*current_indomspec;
static int		current_star_indom;
static int		walk_indom;
static int		star_domain;

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
	    fprintf(stderr, "indomspec malloc(%d) failed: %s\n", sizeof(indomspec_t), strerror(errno));
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
	    fprintf(stderr, "indomspec flags malloc(%d) failed: %s\n", ip->numinst*sizeof(int), strerror(errno));
	    exit(1);
	}
	ip->new_inst = (int *)malloc(ip->numinst*sizeof(int));
	if (ip->new_inst == NULL) {
	    fprintf(stderr, "new_inst malloc(%d) failed: %s\n", ip->numinst*sizeof(int), strerror(errno));
	    exit(1);
	}
	ip->new_iname = (char **)malloc(ip->numinst*sizeof(char *));
	if (ip->new_iname == NULL) {
	    fprintf(stderr, "new_iname malloc(%d) failed: %s\n", ip->numinst*sizeof(char *), strerror(errno));
	    exit(1);
	}
	for (i = 0; i < ip->numinst; i++)
	    ip->flags[i] = 0;
    }
    current_indomspec = ip;
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
walk(int mode)
{
    static indomspec_t	*ip;

    if (walk_indom) {
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

%}

%union {
    char		*str;
    pmInDom		indom;
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
	PMID
	PMID_INT
	NULL_INT
	TYPE
	TYPE_NAME
	SEM
	SEM_NAME
	UNITS
	SPACE_NAME
	TIME_NAME

%token<str>	GNAME NUMBER STRING HNAME FLOAT INDOM_STAR

%type<str>	hname
%type<indom>    indom_int

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
		| TIME ASSIGN {sign = 1; } sign time
		    {
			if (global.flags & GLOBAL_CHANGE_TIME) {
			    snprintf(mess, sizeof(mess), "Duplicate global time clause");
			    yyerror(mess);
			}
			global.flags |= GLOBAL_CHANGE_TIME;
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

sign		: PLUS
		| MINUS			{ sign = -1; }
		| /* nothing */
		;

time		: NUMBER COLON NUMBER COLON FLOAT	/* HH:MM:SS.d.. format */
		    { 
			int	hr = atoi($1);
			int	min = atoi($3);
			double	sec = atof($5);
			if (min > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", min);
			    yywarn(mess);
			}
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * (hr * 3600 + min * 60 + (int)sec);
			global.time.tv_usec = (int)(1000000*((sec - (int)sec))+0.5);
			free($1);
			free($3);
			free($5);
		    }
		| NUMBER COLON NUMBER COLON NUMBER	/* HH:MM:SS format */
		    { 
			int	hr = atoi($1);
			int	min = atoi($3);
			int	sec = atoi($5);
			if (min > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", min);
			    yywarn(mess);
			}
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * (hr * 3600 + min * 60 + sec);
			free($1);
			free($3);
			free($5);
		    }
		| NUMBER COLON FLOAT		/* MM:SS.d.. format */
		    { 
			int	min = atoi($1);
			double	sec = atof($3);
			if (min > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", min);
			    yywarn(mess);
			}
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * (min * 60 + (int)sec);
			global.time.tv_usec = (int)(1000000*((sec - (int)sec))+0.5);
			free($1);
			free($3);
		    }
		| NUMBER COLON NUMBER		/* MM:SS format */
		    { 
			int	min = atoi($1);
			int	sec = atoi($3);
			if (min > 59) {
			    snprintf(mess, sizeof(mess), "Minutes (%d) in time clause more than 59", min);
			    yywarn(mess);
			}
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * (min * 60 + sec);
			free($1);
			free($3);
		    }
		| FLOAT			/* SS.d.. format */
		    {
			double	sec = atof($1);
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%.6f) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * (int)sec;
			global.time.tv_usec = (int)(1000000*((sec - (int)sec))+0.5);
			free($1);
		    }
		| NUMBER		/* SS format */
		    {
			int	sec = atoi($1);
			if (sec > 59) {
			    snprintf(mess, sizeof(mess), "Seconds (%d) in time clause more than 59", sec);
			    yywarn(mess);
			}
			global.time.tv_sec = sign * sec;
			global.time.tv_usec = 0;
			free($1);
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
			    walk_indom = 1;
			}
			else {
			    start_indom($2);
			    walk_indom = 0;
			}
		    }
			LBRACE optindomopt RBRACE
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
			if (domain >= 512) {
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
			if (domain >= 512) {
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
			for (ip = walk(W_START); ip != NULL; ip = walk(W_NEXT)) {
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
			for (ip = walk(W_START); ip != NULL; ip = walk(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, $4) < 0)
				yyerror(mess);
			}
			free($2);
			/* Note: $4 referenced from new_iname[] */
		    }
		| NAME STRING ASSIGN
		    {
			indomspec_t	*ip;
			for (ip = walk(W_START); ip != NULL; ip = walk(W_NEXT)) {
			    if (change_inst_by_name(ip->old_indom, $2, NULL) < 0)
			    	yyerror(mess);
			}
			free($2);
		    }
		| INST NUMBER ASSIGN NUMBER
		    {
			indomspec_t	*ip;
			for (ip = walk(W_START); ip != NULL; ip = walk(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, atoi($2), atoi($4)) < 0)
				yyerror(mess);
			}
			free($2);
			free($4);
		    }
		| INST NUMBER ASSIGN
		    {
			indomspec_t	*ip;
			for (ip = walk(W_START); ip != NULL; ip = walk(W_NEXT)) {
			    if (change_inst_by_inst(ip->old_indom, atoi($2), PM_IN_NULL) < 0)
				yyerror(mess);
			}
			free($2);
		    }
		;

metricspec	: METRIC metric LBRACE metricoptlist RBRACE
		| METRIC metric LBRACE RBRACE
		;

metric		: GNAME
		// TODO handle * in ordinal pos? | PMID_INT
		    {
			free($1);
		    }
		;

metricoptlist	: metricopt
		| metricopt metricoptlist
		;

metricopt	: PMID ASSIGN PMID_INT
		    {
			// TODO handle * in oridinal pos?
			;
		    }
		| NAME ASSIGN GNAME
		    {
			// $3 used or freed in TODO()
			;
		    }
		| TYPE ASSIGN TYPE_NAME
		    {
			// TODO
			;
		    }
		| INDOM ASSIGN NULL_INT
		    {
			// TODO
			;
		    }
		| INDOM ASSIGN FLOAT
		    {
			// TODO
			;
		    }
		| INDOM ASSIGN 
		    {
			// TODO
			;
		    }
		| SEM ASSIGN SEM_NAME
		    {
			// TODO
			;
		    }
		| UNITS ASSIGN NUMBER COMMA NUMBER COMMA NUMBER COMMA SPACE_NAME COMMA TIME_NAME COMMA NUMBER
		    {
			// TODO
			free($3);
			free($5);
			free($7);
			free($13);
		    }
		;

%%
