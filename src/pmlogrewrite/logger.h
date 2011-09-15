/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * common data structures for pmlogextract
 */

#ifndef _LOGGER_H
#define _LOGGER_H

extern int	wflag;		/* -w from command line */
extern int	vflag;		/* -v from command line */

/*
 * Global rewrite specifications
 */
typedef struct {
    int			flags;		/* GLOBAL_* flags */
    struct timeval	time;		/* timestamp shift */
    char		hostname[PM_LOG_MAXHOSTLEN];
    char		tz[PM_TZ_MAXLEN]; 
} global_t;

/* values for global_t flags */
#define GLOBAL_CHANGE_TIME	1
#define GLOBAL_CHANGE_HOSTNAME	2
#define GLOBAL_CHANGE_TZ	4

extern global_t global;

/*
 * Rewrite specifications for an instance domain
 */
typedef struct indomspec {
    struct indomspec	*i_next;
    int			*flags;		/* INST_* flags * */
    pmInDom		old_indom;
    pmInDom		new_indom;	/* PM_INDOM_NULL if no change */
    int			numinst;
    int			*old_inst;	/* filled from pmGetInDomArchive() */
    char		**old_iname;	/* filled from pmGetInDomArchive() */
    int			*new_inst;
    char		**new_iname;
} indomspec_t;

/* values for indomspec_t flags[] */
#define INST_CHANGE_INST	16
#define INST_CHANGE_INAME	32
#define INST_DELETE		64

extern indomspec_t	*indom_root;

/*
 * Rewrite specifications for a metric
 */
typedef struct metricspec {
    struct metricspec	*m_next;
    int			flags;		/* METRIC_* flags * */
    char		*old_name;
    char		*new_name;
    pmDesc		old_desc;
    pmDesc		new_desc;
} metricspec_t;

/* values for metricspec_t flags[] */
#define METRIC_CHANGE_PMID	 1
#define METRIC_CHANGE_NAME	 2
#define METRIC_CHANGE_TYPE	 4
#define METRIC_CHANGE_INDOM	 8
#define METRIC_CHANGE_SEM	16
#define METRIC_CHANGE_UNITS	32
#define METRIC_DELETE		64

extern metricspec_t	*metric_root;

/*
 *  Input archive control
 */
typedef struct {
    int		ctx;
    __pmContext	*ctxp;
    char	*name;
    pmLogLabel	label;
    __pmPDU	*metarec;
    __pmPDU	*logrec;
    pmResult	*rp;
    int		mark;		/* need EOL marker */
} inarch_t;

extern inarch_t	inarch;	/* input archive */

/*
 *  Mark record
 */
typedef struct {
    __pmPDU		len;
    __pmPDU		type;
    __pmPDU		from;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* zero PMIDs to follow */
} mark_t;

/* generic error message buffer */
extern char	mess[256];

/* yylex() gets intput from here ... */
extern FILE	*fconfig;

extern void	yyerror(char *);
extern void	yywarn(char *);
extern int	yylex(void);
extern int	yyparse(void);

extern char	*SemStr(int);
extern int	_pmLogGet(__pmLogCtl *, int, __pmPDU **);
extern int	_pmLogPut(FILE *, __pmPDU *);

#endif /* _LOGGER_H */
