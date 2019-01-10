/*
 * Copyright (c) 2009 Ken McDonell.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#ifndef _DERIVE_H
#define _DERIVE_H

#include <sys/types.h>
#include <regex.h>

/*
 * Derived Metrics support
 */

typedef struct {		/* one value in the expression tree */
    int		inst;
    pmAtomValue	value;
    int		vlen;		/* from vlen of pmValueBlock for string and aggregates */
} val_t;

typedef struct {		/* dynamic information for an expression node */
    pmID		pmid;
    int			numval;		/* length of ivlist[] */
    int			mul_scale;	/* scale multiplier */
    int			div_scale;	/* scale divisor */
    val_t		*ivlist;	/* instance-value pairs */
    struct timeval	stamp;		/* timestamp from current fetch */
    double		time_scale;	/* time utilization scaling for rate() */
    int			last_numval;	/* length of last_ivlist[] */
    val_t		*last_ivlist;	/* values from previous fetch for delta() or rate() */
    struct timeval	last_stamp;	/* timestamp from previous fetch for rate() */
} info_t;

typedef struct {			/* for instance filtering */
    int			ftype;		/* F_REGEX or F_EXACT */
    int			inst;		/* internal instance id if ftype == F_EXACT */
    regex_t		regex;		/* compiled regex if ftype = F_REGEX */

    int			invert;		/* 0 for regex match, 1 for not regex match */
    __pmHashCtl		hash;		/* instance hash table for ftype == F_REGEX */
    int			used;		/* node fetch counter for garbage collection */
} pattern_t;

/* instance control for filtering, hangs off .data field of __pmHashNode */
typedef struct {
    int		inst;			/* internal instance id */
    int		match;			/* true if this instance matches */
    int		used;			/* inst fetch counter for garbage collection */
} instctl_t;

typedef struct node {		/* expression tree node */
    int		type;
    pmDesc	desc;
    int		save_last;
    struct node	*left;
    struct node	*right;
    char	*value;
    union {
	info_t		*info;
	pattern_t	*pattern;
    } data;
} node_t;

typedef struct {		/* one derived metric */
    char	*name;
    int		anon;		/* 1 for anonymous derived metrics */
    pmID	pmid;
    int		bind;		/* 0/1 if bind_expr() has been called */
    node_t	*expr;		/* NULL => invalid, e.g. dup or missing operands */
} dm_t;

/*
 * Control structure for a set of derived metrics.
 * This is used for the static definitions (registered) and the dynamic
 * tree of expressions maintained per context.
 */
typedef struct {
    __pmMutex		mutex;
    int			nmetric;	/* derived metrics */
    dm_t		*mlist;
    int			fetch_has_dm;	/* ==1 if pmResult rewrite needed */
    int			numpmid;	/* from pmFetch before rewrite */
} ctl_t;

/* node_t types */
#define N_INTEGER	1
#define N_NAME		2
#define N_PLUS		3
#define N_MINUS		4
#define N_STAR		5
#define N_SLASH		6
#define N_AVG		7
#define N_COUNT		8
#define N_DELTA		9
#define N_MAX		10
#define N_MIN		11
#define N_SUM		12
#define N_ANON		13
#define N_RATE		14
#define N_INSTANT	15
#define N_DOUBLE	16
#define N_LT		17
#define N_LEQ		18
#define N_EQ		19
#define N_GEQ		20
#define N_GT		21
#define N_NEQ		22
#define N_AND		23
#define N_OR		24
#define N_NOT		25
#define N_NEG		26
#define N_QUEST		27
#define N_COLON		28
#define N_RESCALE	29
#define N_SCALE		30
#define N_DEFINED	31
#define N_FILTERINST	32
#define N_PATTERN	33
#define N_SCALAR	34

/* instance filtering types */
#define F_REGEX		0		/* matchinst([!]pattern, expr) */
#define F_EXACT		1		/* metric[instance] */

/* fetch count for regex instances garbage collection */
#define REGEX_INST_COMPACT	100

extern int __dmtraverse(__pmContext *, const char *, char ***) _PCP_HIDDEN;
extern int __dmchildren(__pmContext *, int, const char *, char ***, int **) _PCP_HIDDEN;
extern int __dmgetpmid(int, const char *, pmID *) _PCP_HIDDEN;
extern int __dmgetname(pmID, char **) _PCP_HIDDEN;
extern void __dmopencontext(__pmContext *) _PCP_HIDDEN;
extern void __dmbind(int, __pmContext *, int) _PCP_HIDDEN;
extern void __dmclosecontext(__pmContext *) _PCP_HIDDEN;
extern int __dmdesc(__pmContext *, int, pmID, pmDesc *) _PCP_HIDDEN;
extern int __dmprefetch(__pmContext *, int, const pmID *, pmID **) _PCP_HIDDEN;
extern void __dmpostfetch(__pmContext *, pmResult **) _PCP_HIDDEN;
extern void __dmdumpexpr(node_t *, int) _PCP_HIDDEN;
extern char *__dmnode_type_str(int) _PCP_HIDDEN;

#endif	/* _DERIVE_H */
