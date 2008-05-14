/*
 * pmlogextract - extract desired metrics from PCP archive logs
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "./logger.h"

#ifdef PCP_DEBUG
long totalmalloc = 0;
#endif

/*
 *  Usage
 */
static void
usage (void)
{
#ifdef SHARE_WITH_PMLOGMERGE
    if (!strcmp(pmProgname, "pmlogmerge")) {
	fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options:\n\
  -S starttime   start of the time window\n\
  -s samples     terminate after this many log records have been written\n\
  -T endtime     end of the time window\n\
  -v samples     switch log volumes after this many samples\n\
  -Z timezone    set reporting timezone\n\
  -z             set reporting timezone to local time of input-archive\n",
	pmProgname);
    }
    else 
#endif
    {
	fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options:\n\
  -c configfile  file to load configuration from\n\
  -n pmnsfile    use an alternative PMNS\n\
  -S starttime   start of the time window\n\
  -s samples     terminate after this many log records have been written\n\
  -T endtime     end of the time window\n\
  -v samples     switch log volumes after this many samples\n\
  -w             ignore day/month/year\n\
  -Z timezone    set reporting timezone\n\
  -z             set reporting timezone to local time of input-archive\n",
	pmProgname);
    }
}

/*
 *  global constants
 */
#define LOG			0
#define META			1
#define LOG_META		2
#define NUM_SEC_PER_DAY		86400

#define NOT_WRITTEN		0
#define MARK_FOR_WRITE		1
#define WRITTEN			2

#define BREAK_EOF		1
#define BREAK_LOGEND		2
#define BREAK_WINEND		3


/*
 *  reclist_t is in logger.h
 *	(list of pdu's to write out at start of time window)
 */

/*
 *  Input archive control is in logger.h
 */


/*
 *  PDU for pmResult (PDU_RESULT)
 */
typedef struct {
    pmID		pmid;
    int			numval;		/* no. of vlist els to follow, or err */
    int			valfmt;		/* insitu or pointer */
    __pmValue_PDU	vlist[1];	/* zero or more */
} vlist_t;

/*
 *  This struct is not needed?
 */
typedef struct {
    /* __pmPDUHdr		hdr; */
    __pmPDU		hdr;
    __pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* no. of PMIDs to follow */
    __pmPDU		data[1];	/* zero or more */
} result_t;

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


/*
 *  Global variables
 */
static int	exit_status = 0;
static int	inarchvers = PM_LOG_VERS02;	/* version of input archive */
static int	outarchvers = PM_LOG_VERS02;	/* version of output archive */
static int	first_datarec = 1;		/* first record flag */
static int	pre_startwin = 1;		/* outside time win flag */
static int	written = 0;			/* num log writes so far */
int		ml_numpmid = 0;			/* num pmid in ml list */
int		ml_size = 0;			/* actual size of ml array */
mlist_t		*ml = NULL;			/* list of pmids with indoms */
rlist_t		*rl = NULL;			/* list of pmResults */


off_t		new_log_offset;			/* new log offset */
off_t		new_meta_offset;		/* new meta offset */
off_t		old_log_offset;			/* old log offset */
off_t		old_meta_offset;		/* old meta offset */
static off_t	flushsize = 100000;		/* bytes before flush */


/* archive control stuff */
char			*outarchname = NULL;	/* name of output archive */
static __pmHashCtl	mdesc_hash;	/* pmids that have been written */
static __pmHashCtl	mindom_hash;	/* indoms that have been written */
static __pmLogCtl	logctl;		/* output archive control */
inarch_t		*inarch;	/* input archive control(s) */
int			inarchnum;	/* number of input archives */

int			ilog;		/* index of earliest log */

static reclist_t	*rlog;		/* log records to be written */
static reclist_t	*rdesc;		/* meta desc records to be written */
static reclist_t	*rindom;	/* meta indom records to be written */

static __pmTimeval	curlog;		/* most recent timestamp in log */
static __pmTimeval	current;	/* most recent timestamp overall */

/* time window stuff */
static struct timeval logstart_tval = {0,0};	/* extracted log start */
static struct timeval logend_tval = {0,0};	/* extracted log end */
static struct timeval winstart_tval = {0,0};	/* window start tval*/
static struct timeval winend_tval = {0,0};	/* window end tval*/

static double 	winstart_time = -1.0;		/* window start time */
static double	winend_time = -1.0;		/* window end time */
static double	logend_time = -1.0;		/* log end time */

/* command line args */
char	*configfile = NULL;		/* -c arg - name of config file */
char	*pmnsfile = PM_NS_DEFAULT;	/* -n arg - alternate namespace */
int	sarg = -1;			/* -s arg - finish after X samples */
char	*Sarg = NULL;			/* -S arg - window start */
char	*Targ = NULL;			/* -T arg - window end */
int	varg = -1;			/* -v arg - switch log vol every X */
int	warg = 0;			/* -w arg - ignore day/month/year */
int	zarg = 0;			/* -z arg - use archive timezone */
char	*tz = NULL;			/* -Z arg - use timezone from user */

/* cmd line args that could exist, but don't (needed for pmParseTimeWin) */
char	*Aarg = NULL;			/* -A arg - non-existent */
char	*Oarg = NULL;			/* -O arg - non-existent */

/*--- START FUNCTIONS -------------------------------------------------------*/

extern int _pmLogGet(__pmLogCtl *, int, __pmPDU **);
extern int _pmLogPut(FILE *, __pmPDU *);
extern void insertresult(rlist_t **, pmResult *);
extern pmResult *searchmlist(pmResult *);

/*
 *  convert timeval to double
 */
double
tv2double(struct timeval *tv)
{
    return tv->tv_sec + (double)tv->tv_usec / 1000000.0;
}


/*
 *  report that archive is corrupted
 */
static void
_report(FILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = lseek(fileno(fp), 0L, SEEK_CUR);
    fprintf(stderr, "%s: Error occurred at byte offset %ld into a file of",
	    pmProgname, (long int)here);
    if (fstat(fileno(fp), &sbuf) < 0)
	fprintf(stderr, ": stat: %s\n", strerror(errno));
    else
	fprintf(stderr, " %ld bytes.\n", (long int)sbuf.st_size);
    fprintf(stderr, "The last record, and the remainder of this file will not be extracted.\n");
    exit_status = 1;
}


/*
 *  switch output volumes
 */
static void
newvolume(char *base, __pmTimeval *tvp)
{
    FILE		*newfp;
    int			nextvol = logctl.l_curvol + 1;
    struct timeval	stamp;

    if ((newfp = __pmLogNewFile(base, nextvol)) != NULL) {
	fclose(logctl.l_mfp);
	logctl.l_mfp = newfp;
	logctl.l_label.ill_vol = logctl.l_curvol = nextvol;
	__pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
	fflush(logctl.l_mfp);
	stamp.tv_sec = tvp->tv_sec;
	stamp.tv_usec = tvp->tv_usec;
	fprintf(stderr, "%s: New log volume %d, at ",
		pmProgname, nextvol);
	__pmPrintStamp(stderr, &stamp);
	fputc('\n', stderr);
	return;
    }
    else {
	fprintf(stderr, "%s: Error: volume %d: %s\n",
		pmProgname, nextvol, pmErrStr(-errno));
	exit(1);
    }
}


/*
 * construct new external label, and check label records from
 * input archives
 */
static void
newlabel(void)
{
    int		i;
    inarch_t	*iap;
    __pmLogLabel	*lp = &logctl.l_label;

    /* set outarch to inarch[0] to start off with */
    iap = &inarch[0];

    /* check version number */
    inarchvers = iap->label.ll_magic & 0xff;
    outarchvers = inarchvers;

    if (inarchvers != PM_LOG_VERS01 && inarchvers != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmProgname, inarchvers, iap->name);
	exit(1);
    }

    /* copy magic number, pid, host and timezone */
    lp->ill_magic = iap->label.ll_magic;
    lp->ill_pid = getpid();
    strncpy(lp->ill_hostname, iap->label.ll_hostname, PM_LOG_MAXHOSTLEN);
    strcpy(lp->ill_tz, iap->label.ll_tz);

    /* reset outarch as appropriate, depending on other input archives */
    for (i=1; i<inarchnum; i++) {
	iap = &inarch[i];

	/* Ensure all archives of the same version number */
        if ((iap->label.ll_magic & 0xff) != inarchvers) {
	    fprintf(stderr, 
		"%s: Error: input archives with different version numbers\n"
		"archive: %s version: %d\n"
		"archive: %s version: %d\n",
		    pmProgname, inarch[0].name, inarchvers,
		    iap->name, (iap->label.ll_magic & 0xff));
	    exit(1);
        }

	/* Ensure all archives of the same host */
	if (strcmp(lp->ill_hostname, iap->label.ll_hostname) != 0) {
	    fprintf(stderr,"%s: Error: host name mismatch for input archives\n",
		    pmProgname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    inarch[0].name, inarch[0].label.ll_hostname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    iap->name, iap->label.ll_hostname);
	    exit(1);
	}

	/* Ensure all archives of the same timezone */
	if (strcmp(lp->ill_tz, iap->label.ll_tz) != 0) {
	    fprintf(stderr,
		"%s: Warning: timezone mismatch for input archives\n",
		    pmProgname);
	    fprintf(stderr, "archive: %s timezone: %s [will be used]\n",
		    inarch[0].name, lp->ill_tz);
	    fprintf(stderr, "archive: %s timezone: %s [will be ignored]\n",
		    iap->name, iap->label.ll_tz);
	}
    } /*for(i)*/
}


/*
 *  
 */
void
writelabel_metati(int do_rewind)
{
    if (do_rewind) rewind(logctl.l_tifp);
    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);

    if (do_rewind) rewind(logctl.l_mdfp);
    logctl.l_label.ill_vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.l_mdfp, &logctl.l_label);
}


/*
 *  
 */
void
writelabel_data(void)
{
    logctl.l_label.ill_vol = 0;
    __pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
}


/* --- Start of reclist functions --- */

/*
 *  make a reclist_t record
 */
static reclist_t *
mk_reclist_t(void)
{
    reclist_t	*rec;

    if ((rec = (reclist_t *)malloc(sizeof(reclist_t))) == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space for record list.\n",
		pmProgname);
	exit(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += sizeof(reclist_t);
        printf ("mk_reclist_t: allocated %d\n", (int)sizeof(reclist_t));
    }
#endif
    rec->pdu = NULL;
    rec->pmid = (pmID)0;
    rec->indom = PM_INDOM_NULL;
    rec->written = NOT_WRITTEN;
    rec->ptr = NULL;
    rec->next = NULL;
    return(rec);
}

/*
curr->ptr = findnadd_indomreclist(curr->indom);
 *
 * find indom in indomreclist - if it isn't in the list then add it in
 * with no pdu buffer
 */
static reclist_t *
findnadd_indomreclist(int indom)
{
    reclist_t	*curr;

    if (rindom == NULL) {
	rindom = mk_reclist_t();
	rindom->pmid = 0;
	rindom->indom = indom;
	return(rindom);
    }
    else {
	curr = rindom;

	/* find matching record or last record */
	while (curr->next != NULL && curr->indom != indom)
	    curr = curr->next;

	if (curr->indom == indom) {
	    /* we have found a matching record - return the pointer */
	    return(curr);
	}
	else {
	    /* we have not found a matchin record - append new record */
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->pmid = 0;
	    curr->indom = indom;
	    return(curr);
	}
    }

}

/*
 *  append a new record to the log record list
 */
void
append_logreclist(int i)
{
    inarch_t	*iap;
    reclist_t	*curr;

    iap = &inarch[i];

    if (rlog == NULL) {
	rlog = mk_reclist_t();
	rlog->pdu = iap->pb[LOG];
    }
    else {
	curr = rlog;

	/* find matching record or last record */
	while (curr->next != NULL &&
		curr->pdu[4] != iap->pb[LOG][4]) curr = curr->next;

	if (curr->pdu[4] == iap->pb[LOG][4]) {
	    /* LOG: discard old record; insert new record */
	    __pmUnpinPDUBuf(curr->pdu);
	    curr->pdu = iap->pb[LOG];
	}
	else {
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->pdu = iap->pb[LOG];
	}
    } /*else*/

    iap->pb[LOG] = NULL;
    iap->pick[LOG] = 0;
}

/*
 *  append a new record to the desc meta record list
 */
void
append_descreclist(int i)
{
    inarch_t	*iap;
    reclist_t	*curr;

    iap = &inarch[i];

    if (rdesc == NULL) {
	rdesc = mk_reclist_t();
	rdesc->pmid = __ntohpmID(iap->pb[META][2]);
	rdesc->indom = __ntohpmInDom(iap->pb[META][4]);
	rdesc->pdu = iap->pb[META];
	rdesc->ptr = findnadd_indomreclist(rdesc->indom);
    }
    else {
	curr = rdesc;

	/* find matching record or last record */
	while (curr->next != NULL && curr->pmid != __ntohpmID(iap->pb[META][2]))
	    curr = curr->next;

	if (curr->pmid == __ntohpmID(iap->pb[META][2])) {
	    if (curr->pdu == NULL) {
		curr->pdu = iap->pb[META];
		curr->indom = __ntohpmInDom(iap->pb[META][4]);
		curr->written = MARK_FOR_WRITE;
		if (curr->ptr == NULL) {
		    curr->ptr = findnadd_indomreclist(curr->indom);
		}
	    }
	    else if (curr->indom == __ntohpmInDom(iap->pb[META][4])) {
		/* META: discard new record */
		free(iap->pb[META]);
	    }
	    else {
		fprintf(stderr,
		    "%s: Error: meta data description records do not match.\n",
			pmProgname);
		exit(1);
	    }
	}
	else {
	    /* append new record */
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->pdu = iap->pb[META];
	    curr->pmid = __ntohpmID(iap->pb[META][2]);
	    curr->indom = __ntohpmInDom(iap->pb[META][4]);
	    curr->ptr = findnadd_indomreclist(curr->indom);
	}
    } /*else*/

    iap->pb[META] = NULL;
    iap->pick[META] = 0;
}

/*
 *  append a new record to the indom meta record list
 */
void
append_indomreclist(int i)
{
    inarch_t	*iap;
    reclist_t	*curr;
    reclist_t	*rec;

    iap = &inarch[i];

    if (rindom == NULL) {
	rindom = mk_reclist_t();
	rindom->pdu = iap->pb[META];
	rindom->pmid = 0;
	rindom->indom = __ntohpmInDom(iap->pb[META][4]);
    }
    else {
	curr = rindom;

	/* find matching record or last record */
	while (curr->next != NULL && curr->indom != __ntohpmInDom(iap->pb[META][4])) {
	    curr = curr->next;
	}

	if (curr->indom == __ntohpmInDom(iap->pb[META][4])) {
	    if (curr->pdu == NULL) {
		/* insert new record */
		curr->pdu = iap->pb[META];
	    }
	    else {
		/* do NOT discard old record; insert new record */
		rec = mk_reclist_t();
		rec->pdu = iap->pb[META];
		rec->pmid = 0;
		rec->indom = __ntohpmInDom(iap->pb[META][4]);
		rec->next = curr->next;
		curr->next = rec;
	    }
	}
	else {
	    /* append new record */
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->pdu = iap->pb[META];
	    curr->pmid = 0;
	    curr->indom = __ntohpmInDom(iap->pb[META][4]);
	}
    } /*else*/

    iap->pb[META] = NULL;
    iap->pick[META] = 0;
}

/*
 *  write out one desc/indom record
 */
void
write_rec(reclist_t *rec)
{
    int		sts;

    if (rec->written == MARK_FOR_WRITE) {
	if (rec->pdu == NULL) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr,"    record is marked for write, but pdu is NULL\n");
	    exit(1);
	}

	/* write out the pdu ; exit if write failed */
	if ((sts = _pmLogPut(logctl.l_mdfp, rec->pdu)) < 0) {
	    fprintf(stderr, "%s: Error: _pmLogPut: meta data : %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
	/* META: free PDU buffer */
	free(rec->pdu);
	rec->pdu = NULL;
	rec->written = WRITTEN;
    }
    else {
	fprintf(stderr,
		"%s : Warning: attempting to write out meta record (%d,%d)\n",
		pmProgname, rec->pmid, rec->indom);
	fprintf(stderr, "        when it is not marked for writing (%d)\n",
		rec->written);
    }
}

void
write_metareclist(pmResult *result, int *needti)
{
    int			i;
    reclist_t		*curr_desc;	/* current desc record */
    reclist_t		*curr_indom;	/* current indom record */
    reclist_t   	*othr_indom;	/* other indom record */
    pmID		pmid;
    pmInDom		indom;
    struct timeval	*this;		/* ptr to timestamp in result */

    this = &result->timestamp;

    /* if pmid in result matches a pmid in desc then write desc
     */
    for (i=0; i<result->numpmid; i++) {
	pmid = result->vset[i]->pmid;
	indom = PM_INDOM_NULL;
	curr_indom = NULL;

	curr_desc = rdesc;
	while (curr_desc != NULL && curr_desc->pmid != pmid)
	    curr_desc = curr_desc->next;

	if (curr_desc == NULL) {
	    /* descriptor has not been found - this is bad
	     */
	    fprintf(stderr, "%s: Error: meta data (TYPE_DESC) for pmid %d has not been found.\n", pmProgname, pmid);
	    exit(1);
	}
	else if (curr_desc->pmid == pmid) {
	    /* descriptor has been found
	     */
	    if (curr_desc->written == WRITTEN) {
		/* descriptor has been written before (no need to write again)
		 * but still need to check indom
		 */
		indom = curr_desc->indom;
		curr_indom = curr_desc->ptr;
	    }
	    else if (curr_desc->pdu == NULL) {
		/* descriptor is in list, has not been written, but no pdu
		 *  - this is bad
		 */
		fprintf(stderr, "%s: Error: missing pdu for pmid %d\n",
			pmProgname, pmid);
	        exit(1);
	    }
	    else {
		/* descriptor is in list, has not been written, and has pdu
		 * write!
		 */
		curr_desc->written = MARK_FOR_WRITE;
		write_rec(curr_desc);
		indom = curr_desc->indom;
		curr_indom = curr_desc->ptr;
	    }
	}
	else {
	    /* unexpected code
	     */
	    fprintf(stderr,
		"%s: Error: reached unexpected code in `write_metareclist'.\n",
		    pmProgname);
	}

	/* descriptor has been found and written,
	 * now go and find & write the indom
	 */
	if (indom != PM_INDOM_NULL) {
	    /* there may be more than one indom in the list, so we need
	     * to traverse the entire list
	     *	- we can safely ignore all indoms after the current timestamp
	     *	- we want the latest indom at, or before the current timestamp
	     *  - all others before the current timestamp can be discarded(?)
	     */
	    othr_indom = NULL;
	    while (curr_indom != NULL && curr_indom->indom == indom) {
		if (curr_indom->written != WRITTEN) {
		    if (curr_indom->pdu == NULL) {
			/* indom is in list, has not been written,
			 * but has no pdu - this is possible and acceptable
			 * behaviour; do nothing
			 */
	    	    }
		    else if (ntohl(curr_indom->pdu[2]) < this->tv_sec ||
			(ntohl(curr_indom->pdu[2]) == this->tv_sec &&
			ntohl(curr_indom->pdu[3]) <= this->tv_usec))
		    {
			/* indom is in list, indom has pdu
			 * and timestamp in pdu suits us
			 */
			if (othr_indom == NULL) {
			    othr_indom = curr_indom;
			}
			else if (ntohl(othr_indom->pdu[2]) < ntohl(curr_indom->pdu[2]) ||
				 (ntohl(othr_indom->pdu[2]) == ntohl(curr_indom->pdu[2]) &&
				 ntohl(othr_indom->pdu[3]) <= ntohl(curr_indom->pdu[3])))
			{
			    /* we already have a perfectly good indom,
			     * but curr_indom has a better timestamp
			     */
			    othr_indom = curr_indom;
			}
		    }
		}
		curr_indom = curr_indom->next;
	    } /*while()*/

	    if (othr_indom != NULL) {
		othr_indom->written = MARK_FOR_WRITE;
		othr_indom->pdu[2] = htonl(this->tv_sec);
		othr_indom->pdu[3] = htonl(this->tv_usec);

		/* make sure to set needti, when writing out the indom
		 */
		*needti = 1;
		write_rec(othr_indom);
	    }
	}
    } /*for(i)*/
}

/* --- End of reclist functions --- */

/*
 *  create a mark record
 */
__pmPDU *
_createmark(void)
{
    mark_t	*markp;

    markp = (mark_t *)malloc(sizeof(mark_t));
    if (markp == NULL) {
	fprintf(stderr, "%s: Error: mark_t malloc: %s\n",
		pmProgname, strerror(errno));
	exit(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += sizeof(mark_t);
        printf ("_createmark : allocated %d\n", (int)sizeof(mark_t));
    }
#endif

    markp->len = (int)sizeof(mark_t);
    markp->type = markp->from = 0;
    markp->timestamp = current;
    markp->timestamp.tv_usec += 1000;	/* + 1msec */
    if (markp->timestamp.tv_usec > 1000000) {
	markp->timestamp.tv_usec -= 1000000;
	markp->timestamp.tv_sec++;
    }
    markp->numpmid = 0;
    return((__pmPDU *)markp);
}

void
checklogtime(__pmTimeval *this, int i)
{
    if ((curlog.tv_sec == 0 && curlog.tv_usec == 0) ||
	(curlog.tv_sec > this->tv_sec ||
	(curlog.tv_sec == this->tv_sec && curlog.tv_usec > this->tv_usec))) {
	    ilog = i;
	    curlog.tv_sec = this->tv_sec;
	    curlog.tv_usec = this->tv_usec;
    }
}


/*
 * pick next meta record - if all meta is at EOF return -1
 * (normally this function returns 0)
 */
static int
nextmeta(void)
{
    int		i;
    int		j;
    int		found;
    int		numeof = 0;
    int		sts;
    pmID	pmid;			/* pmid for TYPE_DESC */
    pmInDom	indom;			/* indom for TYPE_INDOM */
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;
    inarch_t	*iap;			/* pointer to input archive control */
    __pmHashNode	*hnp;

    for (i=0; i<inarchnum; i++) {
	iap = &inarch[i];

	/* if at the end of meta file then skip this archive
	 */
	if (iap->eof[META]) {
	    ++numeof;
	    continue;
	}

	/* we should never already have a meta record
	 */
	if (iap->pb[META] != NULL) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr, "    iap->pb[META] is not NULL\n");
	    exit(1);
	}

againmeta:
	/* get next meta record */
	ctxp = __pmHandleToPtr(iap->ctx);
	lcp = ctxp->c_archctl->ac_log;

	if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &iap->pb[META])) < 0) {
	    iap->eof[META] = 1;
	    ++numeof;
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
			pmProgname, iap->name, pmErrStr(sts));
		_report(lcp->l_mdfp);
	    }
	    continue;
	}

	/* pmDesc entries, if not seen before & wanted,
	 *	then append to desc list
	 */
	if (ntohl(iap->pb[META][1]) == TYPE_DESC) {
	    pmid = __ntohpmID(iap->pb[META][2]);

	    /* if ml is defined, then look for pmid in the list
	     * if pmid is not in the list then discard it immediately
	     */
	    found = 0;
	    if (ml == NULL)
		found = 1;
	    else {
		for (j=0; j<ml_numpmid; j++) {
		    if (pmid == ml[j].idesc->pmid)
			found = 1;
		}
	    }

	    if (found) {
		if ((hnp = __pmHashSearch((int)pmid, &mdesc_hash)) != NULL) {
		    /*
		     * meta record has already been processed ...
		     * TODO check that metadata is consistent ... if not, abort,
		     * else skip this one
		     */
		    found = 0;
		}
	    }

	    if (found) {
		/* we want meta */
		/* add to hash list */
		__pmHashAdd((int)pmid, NULL, &mdesc_hash);

		/* add to desc list */
		/* append_descreclist() sets
		 *	pb[META] to NULL and pick[META] to 0
		 */
		append_descreclist(i);
	    }
	    else {
		/* META: don't want this meta */
		free(iap->pb[META]);
		iap->pb[META] = NULL;
		goto againmeta;
	    }
	}
	else if (ntohl(iap->pb[META][1]) == TYPE_INDOM) {
	    /* if ml is defined, then look for instance domain in the list
	     * if indom is not in the list then discard it immediately
	     */
	    indom = __ntohpmInDom(iap->pb[META][4]);
	    found = 0;
	    if (ml == NULL)
	        found = 1;
	    else {
	        for (j=0; j<ml_numpmid; j++) {
		    if (indom == ml[j].idesc->indom)
		        found = 1;
	        }
	    }

	    if (found) {
	        if (__pmHashSearch((int)indom, &mindom_hash) == NULL) {
		    /* meta record has never been seen ... add it to the list */
		    __pmHashAdd((int)indom, NULL, &mindom_hash);
	        }
		/* add to indom list */
		/* append_indomreclist() sets
		 *	pb[META] to NULL and pick[META] to 0
		 * append_indomreclist() may unpin the pdu buffer
		 */
		append_indomreclist(i);
	    }
	    else {
	        /* META: don't want this meta */
	        free(iap->pb[META]);
	        iap->pb[META] = NULL;
	        goto againmeta;
	    }
	}
	else {
	    fprintf(stderr, "%s: Error: unrecognised meta data type: %d\n",
		    pmProgname, ntohl(iap->pb[META][1]));
	    exit(1);
	}

    } /*for(i)*/

    if (numeof == inarchnum) return(-1);
    return(0);
}


/*
 * read in next log record for every archive
 */
static int
nextlog(void)
{
    int		i;
    int		eoflog = 0;	/* number of log files at eof */
    int		sts;
    double	curtime;
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;
    inarch_t	*iap;


    for (i=0; i<inarchnum; i++) {
	iap = &inarch[i];

	/* if at the end of log file then skip this archive
	 */
	if (iap->eof[LOG]) {
	    ++eoflog;
	    continue;
	}

	/* if we already have a log record then skip this archive
	 */
	if (iap->_Nresult != NULL) {
	    continue;
	}

	/* if mark has been written out, then log is at EOF
	 */
	if (iap->mark) {
	    iap->eof[LOG] = 1;
	    ++eoflog;
	    continue;
	}

againlog:
	ctxp = __pmHandleToPtr(iap->ctx);
	lcp = ctxp->c_archctl->ac_log;

	if ((sts=__pmLogRead(lcp,PM_MODE_FORW,NULL,&iap->_result)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
			pmProgname, iap->name, pmErrStr(sts));
		_report(lcp->l_mfp);
	    }
	    /* if the first data record has not been written out, then
	     * do not generate a mark record, and you may as well ignore
	     * this archive
	     */
	    if (first_datarec) {
		iap->mark = 1;
		iap->eof[LOG] = 1;
		++eoflog;
	    }
	    else {
		iap->mark = 1;
		iap->pb[LOG] = _createmark();
	    }
	    continue;
	}


	/* set current log time - this is only done so that we can
	 * determine whether to keep or discard the log
	 */
	curtime = iap->_result->timestamp.tv_sec +
			(double)iap->_result->timestamp.tv_usec / 1000000.0;

	/* if log time is greater than (or equal to) the current window
	 * start time, then we may want it
	 *	(irrespective of the current window end time)
	 */
	if (curtime < winstart_time) {
	    /* log is not in time window - discard result and get next record
	     */
	    if (iap->_result != NULL) {
		pmFreeResult(iap->_result);
		iap->_result = NULL;
	    }
	    goto againlog;
	}
        else {
            /* log is within time window - check whether we want this record
             */
            if (ml == NULL) {
                /* ml is NOT defined, we want everything
                 */
                iap->_Nresult = iap->_result;
            }
            else {
                /* ml is defined, need to search metric list for wanted pmid's
                 *   (searchmlist may return a NULL pointer - this is fine)
                 */
                iap->_Nresult = searchmlist(iap->_result);
            }

            if (iap->_Nresult == NULL) {
                /* dont want any of the metrics in _result, try again
                 */
                if (iap->_result != NULL) {
                        pmFreeResult(iap->_result);
                        iap->_result = NULL;
                }
                goto againlog;
            }
	}
    } /*for(i)*/

    /* if we are here, then each archive control struct should either
     * be at eof, or it should have a _result, or it should have a mark PDU
     * (if we have a _result, we may want all/some/none of the pmid's in it)
     */

    if (eoflog == inarchnum) return(-1);
    return 0;
}

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    extern int		pmDebug;	/* used in parseargs() */
    extern char		*optarg;	/* used in parseargs() */

    int			c;
    int			sts;
    int			errflag = 0;
    char		*endnum;
    struct stat		sbuf;

    while ((c = getopt(argc, argv, "c:D:n:S:s:T:v:wZ:z?")) != EOF) {
	switch (c) {

	case 'c':	/* config file */
	    configfile = optarg;
	    if (stat(configfile, &sbuf) < 0) {
		fprintf(stderr, "%s: %s - invalid file\n",
			pmProgname, configfile);
		errflag++;
	    }
	    break;


	case 'D':	/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'n':	/* namespace */
			/* if namespace is reassigned from config file,
			 * must reload namespace */
	    pmnsfile = optarg;
	    break;

	case 's':	/* number of samples to write out */
	    sarg = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || sarg < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 'S':	/* start time for extracting */
	    Sarg = optarg;
	    break;

	case 'T':	/* end time for extracting */
	    Targ = optarg;
	    break;

	case 'v':	/* number of samples per volume */
	    varg = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || varg < 0) {
		fprintf(stderr, "%s: -v requires numeric argument\n",
			pmProgname);
		errflag++;
	    }
	    break;

	case 'w':	/* ignore day/month/year */
	    warg++;
	    break;

	case 'Z':	/* use timezone from command line */
	    if (zarg) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflag++;

	    }
	    tz = optarg;
	    break;

	case 'z':	/* use timezone from archive */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		errflag++;
	    }
	    zarg++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (warg) {
	if (Sarg == NULL || Targ == NULL) {
	    fprintf(stderr, "%s: Warning: -w flag requires that both -S and -T are specified.\nIgnoring -w flag.\n", pmProgname);
	    warg = 0;
	}
    }


    if (errflag == 0 && optind > argc-2) {
	fprintf(stderr, "%s: Error: insufficient arguments\n", pmProgname);
	errflag++;
    }

    return(-errflag);
}

int
parseconfig(void)
{
    int		errflag = 0;
    extern FILE * yyin;

    if ((yyin = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmProgname, configfile, strerror(errno));
	exit(1);
    }

    if (yyparse() != 0)
	exit(1);

    fclose(yyin);
    yyin = NULL;

    return(-errflag);
}

void
adminarch(void)
{
    int		i;
    int		sts;

    if (pmnsfile != PM_NS_DEFAULT || inarchvers == PM_LOG_VERS01) {
	if ((sts = pmLoadNameSpace (pmnsfile)) < 0) {
	    fprintf(stderr, "%s: Error: cannot load name space: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}

	for (i=0; i<inarchnum; i++) {
	    if ((sts = pmUseContext(inarch[i].ctx)) < 0) {
		fprintf(stderr,
		    "%s: Error: cannot use context (%d) from archive \"%s\"\n",
			pmProgname, inarch[i].ctx, inarch[i].name);
		exit(1);
	    }

	    if ((sts = pmTrimNameSpace ()) < 0) {
		fprintf(stderr, "%s: Error: cannot trim name space: %s\n", 
			pmProgname, pmErrStr(sts));
		exit(1);
	    }
	}
    }
}

/*
 *  we are within time window ... return 0
 *  we are outside of time window & mk new window ... return 1
 *  we are outside of time window & exit ... return -1
 */
static int
checkwinend(double now)
{
    int		i;
    int		sts;
    double	tmptime;
    inarch_t	*iap;
    __pmPDU	*markpdu;	/* mark b/n time windows */

    if (winend_time < 0 || now <= winend_time)
	return(0);

    /* we have reached the end of a window
     *	- if warg is not set, then we have finished (break)
     *	- otherwise, calculate start and end of next window,
     *		     set pre_startwin, discard logs before winstart,
     * 		     and write out mark
     */
    if (!warg)
	return(-1);

    winstart_time += NUM_SEC_PER_DAY;
    winend_time += NUM_SEC_PER_DAY;
    pre_startwin = 1;

    /* if start of next window is later than max termination
     * then bail out here
     */
    if (winstart_time > logend_time)
	    return(-1);

    ilog = -1;
    for (i=0; i<inarchnum; i++) {
	iap = &inarch[i];
	if (iap->_Nresult != NULL) {
	    tmptime = iap->_Nresult->timestamp.tv_sec
			+ (double)iap->_Nresult->timestamp.tv_usec/1000000.0;
	    if (tmptime < winstart_time) {
		/* free _result and _Nresult
		 */
		if (iap->_result != iap->_Nresult && iap->_Nresult != NULL) {
		    free(iap->_Nresult);
		}
		if (iap->_result != NULL) {
		    pmFreeResult(iap->_result);
		    iap->_result = NULL;
		}
		iap->_Nresult = NULL;
		iap->pick[LOG] = 0;
		iap->pb[LOG] = NULL;
	    }
	}
	if (iap->pb[LOG] != NULL) {
	    tmptime = ntohl(iap->pb[LOG][3]) + (double)ntohl(iap->pb[LOG][4])/1000000.0;
	    if (tmptime < winstart_time) {
		/* free PDU buffer ... it is probably a mark
		 * and has not been pinned
		 */
		free(iap->pb[LOG]);
		iap->pb[LOG] = NULL;
		iap->pick[LOG] = 0;
	    }
	}
    } /*for(i)*/

    /* must create "mark" record and write it out */
    /* (need only one mark record) */
    markpdu = _createmark();
    if ((sts = __pmLogPutResult(&logctl, markpdu)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }
    written++;
    free(markpdu);
    return(1);
}


/*
 *
 */
void
writerlist(rlist_t **rlready, double mintime)
{
    int		sts;
    int		needti;		/* need to flush/update */
    double	titime;		/* time of last temporal index write */
    double	restime;	/* time of result */
    rlist_t	*elm;		/* element of rlready to be written out */
    __pmPDU	*pb;		/* pdu buffer */
    __pmTimeval	this;		/* timeval of this record */

    needti = 0;
    titime = 0.0;

    while (*rlready != NULL) {
	this.tv_sec = (*rlready)->res->timestamp.tv_sec;
	this.tv_usec = (*rlready)->res->timestamp.tv_usec;
	restime = this.tv_sec + (double)this.tv_usec / 1000000.0;

        if (restime > mintime)
	    break;

	/* get the first element from the list
	 */
	elm = *rlready;
	*rlready = elm->next;

	/* if this is the first record (for output archive) then do some
	 * admin stuff
	 */
	if (first_datarec) {
	    first_datarec = 0;
	    logctl.l_label.ill_start.tv_sec = elm->res->timestamp.tv_sec;
	    logctl.l_label.ill_start.tv_usec = elm->res->timestamp.tv_usec;
            logctl.l_state = PM_LOG_STATE_INIT;
            writelabel_data();
        }

	/* if we are in a pre_startwin state, and we are writing
	 * something out, then we are not in a pre_startwin state any more
	 * (it also means that there may be some discrete metrics to be
	 * written out)
	 */
	if (pre_startwin)
	    pre_startwin = 0;


	/* write out the descriptor and instance domain pdu's first
	 */
	write_metareclist(elm->res, &needti);


	/* convert log record to a pdu
	 */
	if (outarchvers == 1)
	    sts = __pmEncodeResult(PDU_OVERRIDE1, elm->res, &pb);
	else
	    sts = __pmEncodeResult(PDU_OVERRIDE2, elm->res, &pb);

	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}

	/* __pmEncodeResult doesn't pin the PDU buffer, so we have to
	 */
	__pmPinPDUBuf(pb);


	/* write out log record */
	if ((sts = __pmLogPutResult(&logctl, pb)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(1);
	}
	written++;


	/* check whether we need to write TI (temporal index) */
	if (old_log_offset == 0 ||
	    old_log_offset == sizeof(__pmLogLabel)+2*sizeof(int) ||
	    ftell(logctl.l_mfp) > flushsize)
		needti = 1;

	/* make sure that we do not write out the temporal index more
	 * than once for the same timestamp
	 */
	if (needti && titime >= restime)
	    needti = 0;

	/* flush/update */
	if (needti) {
	    titime = restime;

	    fflush(logctl.l_mfp);
	    fflush(logctl.l_mdfp);

	    if (old_log_offset == 0)
		old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

            new_log_offset = ftell(logctl.l_mfp);
            new_meta_offset = ftell(logctl.l_mdfp);

            fseek(logctl.l_mfp, (long)old_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);

            __pmLogPutIndex(&logctl, &this);

            fseek(logctl.l_mfp, (long)new_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);

            old_log_offset = ftell(logctl.l_mfp);
            old_meta_offset = ftell(logctl.l_mdfp);

            flushsize = ftell(logctl.l_mfp) + 100000;
        }

        /* switch volumes if required */
        if (varg > 0) {
            if (written % varg == 0)
                newvolume(outarchname, (__pmTimeval *)&pb[3]);
        }

	/* LOG: free PDU buffer */
	__pmUnpinPDUBuf(pb);
	pb = NULL;

	elm->res = NULL;
	elm->next = NULL;
	free(elm);

    } /*while(*rlready)*/
}


/*
 *  mark record has been created and assigned to iap->pb[LOG]
 *  write it out
 */
void
writemark(inarch_t *iap)
{
    int		sts;
    mark_t      *p = (mark_t *)iap->pb[LOG];

    if (!iap->mark) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    writemark called, but mark not set\n");
	exit(1);
    }

    if (p == NULL) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    writemark called, but no pdu\n");
	exit(1);
    }

    p->timestamp.tv_sec = htonl(p->timestamp.tv_sec);
    p->timestamp.tv_usec = htonl(p->timestamp.tv_usec);

    if ((sts = __pmLogPutResult(&logctl, iap->pb[LOG])) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }
    written++;
    free(iap->pb[LOG]);
    iap->pb[LOG] = NULL;
    iap->pick[LOG] = 0;
}

/*--- END FUNCTIONS ---------------------------------------------------------*/
/*
 * cni == currently not implemented
 */

int
main(int argc, char **argv)
{
    extern int		optind;		/* used in main() */

    int		i;
    int		j;
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta;		/* sts from nextmeta() */
    int		breakflag;		/* reason for breaking out of while */

    char	*p;
    char	*msg;

    double 	now = 0.0;		/* the current time */
    double 	mintime = 0.0;
    double 	tmptime = 0.0;

    __pmTimeval		tstamp;		/* temporary timestamp */
    inarch_t		*iap;		/* ptr to archive control */
    rlist_t		*rlready;	/* list of results ready for writing */
    struct timeval	unused;


    rlog = NULL;	/* list of log records to write */
    rdesc = NULL;	/* list of meta desc records to write */
    rindom = NULL;	/* list of meta indom records to write */
    rlready = NULL;


    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }


    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	usage();
	exit(1);
    }


    /* input  archive names are argv[optind] ... argv[argc-2]) */
    /* output archive name  is  argv[argc-1]) */

    /* output archive */
    outarchname = argv[argc-1];

    /* input archive(s) */
    inarchnum = argc - 1 - optind;
    inarch = (inarch_t *) malloc(inarchnum * sizeof(inarch_t));
    if (inarch == NULL) {
	fprintf(stderr, "%s: Error: mallco inarch: %s\n",
		pmProgname, strerror(errno));
	exit(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += (inarchnum * sizeof(inarch_t));
        printf ("main        : allocated %d\n",
			(int)(inarchnum * sizeof(inarch_t)));
    }
#endif


    for (i=0; i<inarchnum; i++, optind++) {
	iap = &inarch[i];

	iap->name = argv[optind];

	iap->pb[LOG] = iap->pb[META] = NULL;
	iap->eof[LOG] = iap->eof[META] = 0;
	iap->pick[LOG] = iap->pick[META] = 0;
	iap->mark = 0;
	iap->_result = NULL;
	iap->_Nresult = NULL;

	if ((iap->ctx = pmNewContext(PM_CONTEXT_ARCHIVE, iap->name)) < 0) {
	    fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		    pmProgname, iap->name, pmErrStr(iap->ctx));
	    exit(1);
	}

	if ((sts = pmUseContext(iap->ctx)) < 0) {
	    fprintf(stderr, "%s: Error: cannot use context (%s): %s\n",
		    pmProgname, iap->name, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmGetArchiveLabel(&iap->label)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmProgname, iap->name, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmGetArchiveEnd(&unused)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get end of archive (%s): %s\n",
		    pmProgname, iap->name, pmErrStr(sts));
	    exit(1);
	}

	if (i == 0) {
	    /* start time */
	    logstart_tval.tv_sec = iap->label.ll_start.tv_sec;
	    logstart_tval.tv_usec = iap->label.ll_start.tv_usec;

	    /* end time */
	    logend_tval.tv_sec = unused.tv_sec;
	    logend_tval.tv_usec = unused.tv_usec;
	}
	else {
	    /* get the earlier start time */
	    if (logstart_tval.tv_sec > iap->label.ll_start.tv_sec ||
		(logstart_tval.tv_sec == iap->label.ll_start.tv_sec &&
		logstart_tval.tv_usec > iap->label.ll_start.tv_usec)) {
		    logstart_tval.tv_sec = iap->label.ll_start.tv_sec;
		    logstart_tval.tv_usec = iap->label.ll_start.tv_usec;
	    }

	    /* get the later end time */
	    if (logend_tval.tv_sec < unused.tv_sec ||
		(logend_tval.tv_sec == unused.tv_sec &&
		logend_tval.tv_usec < unused.tv_usec)) {
		    logend_tval.tv_sec = unused.tv_sec;
		    logend_tval.tv_usec = unused.tv_usec;
	    }
	}
    } /*for(i)*/

    logctl.l_label.ill_start.tv_sec = logstart_tval.tv_sec;
    logctl.l_label.ill_start.tv_usec = logstart_tval.tv_usec;


    /* admin archive loads the namespace if required, and trims it
     * according to the context of each archive
     */
    adminarch();


    /* process config file
     *	- this includes a list of metrics and their instances
     */
    if (configfile != NULL) {
	if (parseconfig() < 0) {
	    usage();
	    exit(1);
	}
    }

    if (zarg) {
	/* use TZ from metrics source (input-archive) */
	if ((sts = pmNewZone(inarch[0].label.ll_tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		    pmProgname, pmErrStr(sts));
            exit_status = 1;
            goto cleanup;
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n", inarch[0].label.ll_hostname);
    }
    else if (tz != NULL) {
	/* use TZ as specified by user */
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		    pmProgname, tz, pmErrStr(sts));
	    exit_status = 1;
	    goto cleanup;
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else {
	char	*tz;
        tz = __pmTimezone();
	/* use TZ from local host */
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set local host's timezone: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit_status = 1;
	    goto cleanup;
	}
    }


    /* create output log - must be done before writing label */
    if ((sts = __pmLogCreate("", outarchname, outarchvers, &logctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* This must be done after log is created:
     *		- checks that archive version, host, and timezone are ok
     *		- set archive version, host, and timezone of output archive
     */
    newlabel();

    /* write label record */
    writelabel_metati(0);


    /* set winstart and winend timevals */
    sts = pmParseTimeWindow(Sarg, Targ, Aarg, Oarg,
			    &logstart_tval, &logend_tval,
			    &winstart_tval, &winend_tval, &unused, &msg);
    if (sts < 0) {
	fprintf(stderr, "%s: Invalid time window specified: %s\n",
		pmProgname, msg);
	exit(1);
    }
    winstart_time = tv2double(&winstart_tval);
    winend_time = tv2double(&winend_tval);
    logend_time = tv2double(&logend_tval);

    if (warg) {
	if (winstart_time + NUM_SEC_PER_DAY < winend_time) {
	    fprintf(stderr, "%s: Warning: -S and -T must specify a time window within\nthe same day, for -w to be used.  Ignoring -w flag.\n", pmProgname);
	    warg = 0;
	}
    }


#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif


    ilog = -1;
    written = 0;
    curlog.tv_sec = 0;
    curlog.tv_usec = 0;
    current.tv_sec = 0;
    current.tv_usec = 0;
    first_datarec = 1;
    pre_startwin = 1;
    breakflag = 0;

    /* get all meta data first
     * nextmeta() should return 0 (will return -1 when all meta is eof)
     */
    do {
	stsmeta = nextmeta();
    } while (stsmeta >= 0);


    /* get log record - choose one with earliest timestamp
     * write out meta data (required by this log record)
     * write out log
     * do ti update if necessary
     */
    while (sarg == -1 || written < sarg) {
	ilog = -1;
	curlog.tv_sec = 0;
	curlog.tv_usec = 0;
	old_log_offset = ftell(logctl.l_mfp);
	old_meta_offset = ftell(logctl.l_mdfp);

	/* nextlog() resets ilog, and curlog (to the smallest timestamp)
	 */
	stslog = nextlog();

	if (stslog < 0) {
	    breakflag = BREAK_EOF;
	    break;
	}

	/* find the _Nresult (or mark pdu) with the earliest timestamp;
	 * set ilog
	 * (this is a bit more complex when tflag is specified)
	 */
	mintime = 0.0;
	for (i=0; i<inarchnum; i++) {
	    if (inarch[i]._Nresult != NULL) {
		tstamp.tv_sec = inarch[i]._Nresult->timestamp.tv_sec;
		tstamp.tv_usec = inarch[i]._Nresult->timestamp.tv_usec;
		checklogtime(&tstamp, i);

		if (ilog == i) {
		    tmptime = curlog.tv_sec + (double)curlog.tv_usec/1000000.0;
		    if (mintime <= 0 || mintime > tmptime)
		        mintime = tmptime;
		}
	    }
	    else if (inarch[i].pb[LOG] != NULL) {
		tstamp.tv_sec = inarch[i].pb[LOG][3]; /* no swab needed */
		tstamp.tv_usec = inarch[i].pb[LOG][4]; /* no swab needed */
		checklogtime(&tstamp, i);

		if (ilog == i) {
		    tmptime = curlog.tv_sec + (double)curlog.tv_usec/1000000.0;
		    if (mintime <= 0 || mintime > tmptime)
		        mintime = tmptime;
		}
	    }
	}

	/* now     == the earliest timestamp of the archive(s)
	 *		and/or mark records
	 * mintime == now or timestamp of the earliest mark
	 *		(whichever is smaller)
	 */
	now = curlog.tv_sec + (double)curlog.tv_usec / 1000000.0;

	/* note - mark (after last archive) will be created, but this
	 * break, will prevent it from being written out
	 */
	if (now > logend_time) {
	    breakflag = BREAK_LOGEND;
	    break;
	}

	sts = checkwinend(now);
	if (sts < 0) {
	    breakflag = BREAK_WINEND;
	    break;
	}
	if (sts > 0)
	    continue;

	current.tv_sec = (long)now;
	current.tv_usec = (now - (double)current.tv_sec) * 1000000.0;

	/* prepare to write out log record
	 */
	if (ilog < 0 || ilog >= inarchnum) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    exit(1);
	}

	iap = &inarch[ilog];
	if (iap->mark)
	    writemark(iap);
	else {
	    /* result is to be written out, but there is no _Nresult
	     */
	    if (iap->_Nresult == NULL) {
		fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
		fprintf(stderr, "    pick == LOG and _Nresult = NULL\n");
		exit(1);
	    }
	    insertresult(&rlready, iap->_Nresult);
	    writerlist(&rlready, now);

	    /* writerlist frees elm (elements of rlready) but does not
	     * free _result & _Nresult
	     */

	    /* free _result & _Nresult
	     *	_Nresult may contain space that was allocated
	     *	in __pmStuffValue this space has PM_VAL_SPTR format,
	     *	and has to be freed first
	     *	(in order to avoid memory leaks)
	     */
	    if (iap->_result != iap->_Nresult && iap->_Nresult != NULL) {
		pmValueSet	*vsetp;
		for (i=0; i<iap->_Nresult->numpmid; i++) {
		    vsetp = iap->_Nresult->vset[i];
		    if (vsetp->valfmt == PM_VAL_SPTR) {
			for (j=0; j<vsetp->numval; j++) {
			    free(vsetp->vlist[j].value.pval);
			}
		    }
		}
		free(iap->_Nresult);
	    }
	    if (iap->_result != NULL)
		pmFreeResult(iap->_result);
	    iap->_result = NULL;
	    iap->_Nresult = NULL;
	}
    } /*while()*/

    if (first_datarec) {
	char    fname[MAXNAMELEN];
        fprintf(stderr, "%s: Warning: no qualifying records found.\n",
                pmProgname);
cleanup:
        fprintf(stderr, "Archive \"%s\" not created.\n", outarchname);
        snprintf(fname, sizeof(fname), "%s.0", outarchname);
        unlink(fname);
        snprintf(fname, sizeof(fname), "%s.meta", outarchname);
        unlink(fname);
        snprintf(fname, sizeof(fname), "%s.index", outarchname);
        unlink(fname);
        exit_status = 1;
    }
    else {
	/* write the last time stamp */
	fflush(logctl.l_mfp);
	fflush(logctl.l_mdfp);

	if (old_log_offset == 0)
	    old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

	new_log_offset = ftell(logctl.l_mfp);
	new_meta_offset = ftell(logctl.l_mdfp);

#if 0
	fprintf(stderr, "*** last tstamp: \n\tmintime=%g \n\ttmptime=%g \n\tlogend_time=%g \n\twinend_time=%g \n\tcurrent=%d.%06d\n",
	    mintime, tmptime, logend_time, winend_time, current.tv_sec, current.tv_usec);
#endif

	__pmLogPutIndex(&logctl, &current);


	/* need to fix up label with new start-time */
	writelabel_metati(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        printf ("main        : total allocated %ld\n", totalmalloc);
    }
#endif

    exit(exit_status);
    return(0);
}
