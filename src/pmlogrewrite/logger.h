/*
 * Copyright (c) 2013,2018 Red Hat.
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
 * common data structures for pmlogextract
 */

#ifndef _LOGGER_H
#define _LOGGER_H

extern int	sflag;		/* -s from command line */
extern int	vflag;		/* -v from command line */
extern int	wflag;		/* -w from command line */

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
    int			indom_flags;	/* INDOM_* flags */
    int			*inst_flags;		/* INST_* flags */
    pmInDom		old_indom;
    pmInDom		new_indom;	/* old_indom if no change */
    int			numinst;
    int			*old_inst;	/* filled from pmGetInDomArchive() */
    char		**old_iname;	/* filled from pmGetInDomArchive() */
    int			*new_inst;
    char		**new_iname;
} indomspec_t;

/* values for indomspec_t indom_flags */
#define INDOM_ADD		 1
#define INDOM_DELETE		 2
#define INDOM_DUPLICATE		 4
/* values for indomspec_t inst_flags[] */
#define INST_CHANGE_INST	16
#define INST_CHANGE_INAME	32
#define INST_DELETE		64

extern indomspec_t	*indom_root;

/*
 * Rewrite specifications for a metric
 */
typedef struct metricspec {
    struct metricspec	*m_next;
    int			flags;		/* METRIC_* flags */
    int			output;		/* OUTPUT_* values */
    int			one_inst;	/* for OUTPUT_ONE INST */
    char		*one_name;	/* for OUTPUT_ONE NAME */
    char		*old_name;
    char		*new_name;
    pmDesc		old_desc;
    pmDesc		new_desc;
    indomspec_t		*ip;		/* for instance id changes */
} metricspec_t;

/* values for metricspec_t flags[] */
#define METRIC_CHANGE_PMID	  1
#define METRIC_CHANGE_NAME	  2
#define METRIC_CHANGE_TYPE	  4
#define METRIC_CHANGE_INDOM	  8
#define METRIC_CHANGE_SEM	 16
#define METRIC_CHANGE_UNITS	 32
#define METRIC_DELETE		 64
#define METRIC_RESCALE		128

/* values for output when indom (numval >= 1) => PM_INDOM_NULL (numval = 1) */
#define OUTPUT_ALL	0
#define OUTPUT_FIRST	1
#define OUTPUT_LAST	2
#define OUTPUT_ONE	3
#define OUTPUT_MIN	4
#define OUTPUT_MAX	5
#define OUTPUT_SUM	6
#define OUTPUT_AVG	7

extern metricspec_t	*metric_root;

/*
 * Rewrite specifications for a help text record
 */
typedef struct textspec {
    struct textspec	*t_next;
    int			flags;		/* TEXT_* flags */
    int			old_type;
    int			new_type;
    int			old_id;
    int			new_id;
    char		*old_text;
    char		*new_text;
    indomspec_t		*ip;		/* for instance id changes */
} textspec_t;

/* values for textspec_t flags[] */
#define TEXT_ACTIVE             1
#define TEXT_CHANGE_ID		2
#define TEXT_CHANGE_TYPE	4
#define TEXT_CHANGE_TEXT	8
#define TEXT_DELETE		16

extern textspec_t	*text_root;

/*
 * Rewrite specifications for a label record
 */
typedef struct labelspec {
    struct labelspec	*l_next;
    int			flags;		/* LABEL_* flags */
    int			old_type;
    int			new_type;
    int			old_id;
    int			new_id;
    int			old_instance;
    int			new_instance;
    char		*old_label;
    char		*old_value;
    char		*new_label;
    char		*new_value;
    pmLabelSet		*new_labels;
    indomspec_t		*ip;		/* for instance id changes */
} labelspec_t;

/* values for labelspec_t flags[] */
#define LABEL_ACTIVE            0x01
#define LABEL_CHANGE_ID		0x02
#define LABEL_CHANGE_LABEL	0x04
#define LABEL_CHANGE_INSTANCE	0x08
#define LABEL_CHANGE_VALUE	0x10
#define LABEL_DELETE		0x20
#define LABEL_NEW		0x40
#define LABEL_CHANGE_ANY        0x7e 

extern labelspec_t	*label_root;

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

extern inarch_t		inarch;		/* input archive */

/*
 * Output archive control
 */
typedef struct {
    char	*name;		/* base name of output archive */
    __pmArchCtl	archctl;	/* libpcp archive control */
    __pmLogCtl	logctl;		/* libpcp log control */
} outarch_t;

extern outarch_t	outarch;	/* output archive */

/* size of a string length field */
#define LENSIZE 4

/* generic error message buffer */
extern char	mess[256];

/* yylex() gets intput from here ... */
extern char	*configfile;
extern FILE	*fconfig;
extern int	lineno;
extern FILE	*yyin;

extern void	yyerror(char *);
extern void	yywarn(char *);
extern void	yysemantic(char *);
extern int	yylex(void);
extern int	yyparse(void);

#define W_START	1
#define W_NEXT	2
#define W_NONE	3

extern int	_pmLogGet(__pmArchCtl *, int, __pmPDU **);
extern int	_pmLogPut(FILE *, __pmPDU *);
extern int	_pmLogRename(const char *, const char *);
extern int	_pmLogRemove(const char *, int);
extern pmUnits	ntoh_pmUnits(pmUnits);
#define ntoh_pmInDom(indom) ntohl(indom)
#define ntoh_pmID(pmid)     ntohl(pmid)

extern metricspec_t	*start_metric(pmID);
extern indomspec_t	*start_indom(pmInDom);
extern textspec_t	*start_text(int, int, char *);

extern labelspec_t	*start_label(int, int, int, const char *, char *, char *);
extern labelspec_t	*create_label(int, int, int, char *, char *);
extern void		deactivate_labels(void);

extern int		change_inst_by_inst(pmInDom, int, int);
extern int		change_inst_by_name(pmInDom, char *, char *);
extern int		inst_name_eq(const char *, const char *);

extern char	*SemStr(int);
extern char	*add_quotes(const char *);
extern char	*dupcat(const char *, const char *);
extern void	newvolume(int);

extern void	do_desc(void);
extern void	do_indom(void);
extern void	do_labelset(void);
extern void	do_text(void);
extern void	do_result(void);

extern void	abandon(void);

#endif /* _LOGGER_H */
