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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * common data structures for pmlogextract
 */

#ifndef _LOGGER_H
#define _LOGGER_H

#include "pmapi.h"

/*
 *  list of pdu's to write out at start of time window
 */
typedef struct _reclist_t {
    __pmPDU		*pdu;		/* PDU ptr */
    __pmTimeval		stamp;		/* for indom records */
    pmDesc		desc;
    int			written;	/* written status */
    struct _reclist_t	*ptr;		/* ptr to record in another reclist */
    struct _reclist_t	*next;		/* ptr to next reclist_t record */
} reclist_t;

/*
 *  Input archive control
 */
typedef struct {
    int		ctx;
    char	*name;
    pmLogLabel	label;
    __pmPDU	*pb[2];
    pmResult	*_result;
    pmResult	*_Nresult;
    int		eof[2];
    int		mark;		/* need EOL marker */
} inarch_t;

extern inarch_t	*inarch;	/* input archive control(s) */
extern int	inarchnum;	/* number of input archives */

/*
 *  metric [instance] list
 */
typedef struct {
    char	*name;		/* metric name */
    /* normally idesc and odesc will point to the same descriptor ...
     * however, if the "-t" flag is specified, then in the case of
     * counters and instantaneous values, odesc will be different
     */
    pmDesc	*idesc;		/* input  metric descriptor - pmid, pmindom */
    pmDesc	*odesc;		/* output metric descriptor - pmid, pmindom */
    int		numinst;	/* number of instances (0 means all) */
    int		*instlist;	/* instance ids */
} mlist_t;


/*
 *  pmResult list
 */
typedef struct __rlist_t {
    pmResult		*res;		/* ptr to pmResult */
    struct __rlist_t	*next;		/* ptr to next element in list */
} rlist_t;


extern int	ml_numpmid;		/* num pmid in ml list */
extern int	ml_size;		/* actual size of ml array */
extern mlist_t	*ml;			/* list of pmids with indoms */
extern rlist_t	*rl;			/* list of pmResults */

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
extern int _pmLogGet(__pmLogCtl *, int, __pmPDU **);
extern int _pmLogPut(__pmFILE *, __pmPDU *);
extern pmUnits ntoh_pmUnits(pmUnits);
#define ntoh_pmInDom(indom) ntohl(indom)
#define ntoh_pmID(pmid) ntohl(pmid)

/* internal routines */
extern void insertresult(rlist_t **, pmResult *);
extern pmResult *searchmlist(pmResult *);
extern void abandon_extract(void);


#endif /* _LOGGER_H */
