/*
 * Copyright (c) 2018,2022 Red Hat.
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

#ifndef PCP_LOGGER_H
#define PCP_LOGGER_H

#include "pmapi.h"
#include "libpcp.h"

/*
 *  list of pdu's to write out at start of time window
 */
typedef struct reclist {
    __int32_t		*pdu;		/* PDU ptr */
    __pmTimestamp	stamp;		/* for indom and label records */
    pmDesc		desc;
    unsigned int	written : 16;	/* written PDU status */
    unsigned int	sorted : 16;	/* sorted indom status */
    unsigned int	nrecs;		/* indom array record size */
    struct reclist	*recs;		/* time-sorted array of records */
    struct reclist	*next;		/* ptr to next reclist_t record */
} reclist_t;

/*
 *  Input archive control
 */
typedef struct {
    int			ctx;
    char		*name;
    __pmLogLabel	label;
    __int32_t		*pb[2];		/* current physical record buffer */
    __pmResult		*_result;
    __pmResult		*_Nresult;
    __pmTimestamp	laststamp;
    int			eof[2];
    int			mark;		/* need EOL marker */
    int			recnum;
    int64_t		pmcd_pid;	/* from prologue/epilogue records */
    int32_t		pmcd_seqnum;	/* from prologue/epilogue records */
} inarch_t;
#define LOG			0	/* pb[] & eof[] index for data volume */
#define META			1	/* pb[] & eof[] index for metadata */

extern inarch_t	*inarch;	/* input archive control(s) */
extern int	inarchnum;	/* number of input archives */

/*
 *  metric [instance] selection list
 */
typedef struct {
    char	*name;		/* metric name */
    pmDesc	*desc;		/* metric descriptor - pmid, indom, etc. */
    int		numinst;	/* number of instances (0 means all, -1 means skip) */
    int		*instlist;	/* instance ids */
} mlist_t;


/*
 *  __pmResult list
 */
typedef struct __rlist_t {
    __pmResult		*res;		/* ptr to __pmResult */
    struct __rlist_t	*next;		/* ptr to next element in list */
} rlist_t;


/*
 * metrics explicitly requested via the -c configfile option ...
 * ml is NULL if no -c on the command line
 */
extern int	ml_numpmid;		/* num pmid in ml list */
extern int	ml_size;		/* actual size of ml array */
extern mlist_t	*ml;			/* list of pmids with indoms */
extern rlist_t	*rl;			/* list of __pmResults */

/*
 * metrics with mismatched metadata across archives that are to be
 * skipped if -x is used on the command line
 * skip_ml is NULL if no -x on the command line
 */
extern int	skip_ml_numpmid;
extern pmID	*skip_ml;

extern int	ilog;

/* config file parser states */
#define GLOBAL	0
#define INSPEC	1

/* generic error message buffer */
extern char	emess[240];

/* yylex() gets intput from here ... */
extern FILE	*fconfig;
extern FILE	*yyin;

extern void	yyerror(char *);
extern void	yywarn(char *);
extern int	yylex(void);
extern int	yyparse(void);
extern void	dometric(const char *);

/* log I/O helper routines */
#define ntoh_pmInDom(indom) ntohl(indom)
#define ntoh_pmID(pmid) ntohl(pmid)
#define ntoh_pmLabelType(ltype) ntohl(ltype)
#define ntoh_pmTextType(ltype) ntohl(ltype)

/* internal routines */
extern void insertresult(rlist_t **, __pmResult *);
extern __pmResult *searchmlist(__pmResult *);
extern void abandon_extract(void);

/* command line args needed across source files */
extern int	xarg;

/*
 * parse -v arg
 */
extern int
ParseSize(char *, int *, __int64_t *, struct timespec *);

#endif /* PCP_LOGGER_H */
