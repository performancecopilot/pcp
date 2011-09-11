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
 */

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "logger.h"

global_t	global;
indomspec_t	*indom_root = NULL;

#ifdef PCP_DEBUG
long totalmalloc = 0;
#endif

static pmUnits nullunits = { 0,0,0,0,0,0 };

static int desperate = 0;

/*
 *  Usage
 */
static void
usage (void)
{
    fprintf(stderr,
"Usage: %s [options] input-archive output-archive\n\
\n\
Options:\n\
  -c configfile  file to load config from\n\
  -C             parse config file(s) and quit\n\
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

const char *
metricname(pmID pmid)
{
    static char	*name = NULL;
    if (name != NULL) {
	free(name);
	name = NULL;
    }
    if (pmNameID(pmid, &name) == 0)
	return(name);
    name = NULL;
    return pmIDStr(pmid);
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
inarch_t		inarch;		/* input archive control */

int			ilog;		/* index of earliest log */

static reclist_t	*rlog;		/* log records to be written */
static reclist_t	*rdesc;		/* meta desc records to be written */
static reclist_t	*rindom;	/* meta indom records to be written */

static __pmTimeval	curlog;		/* most recent timestamp in log */
static __pmTimeval	current;	/* most recent timestamp overall */

static double 	winstart_time = -1.0;		/* window start time */
static double	winend_time = -1.0;		/* window end time */
static double	logend_time = -1.0;		/* log end time */

/* command line args */
char	*configfile = NULL;		/* -c arg - name of config file */
int	Carg = 0;			/* -C arg - parse config and quit */
int	sarg = -1;			/* -s arg - finish after X samples */

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
	fprintf(stderr, ": stat: %s\n", osstrerror());
    else
	fprintf(stderr, " %ld bytes.\n", (long int)sbuf.st_size);
    fprintf(stderr, "The last record, and the remainder of this file will not be extracted.\n");
    exit(1);
}


/*
 *  switch output volumes
 */
static void
newvolume(char *base, __pmTimeval *tvp)
{
    FILE		*newfp;
    int			nextvol = logctl.l_curvol + 1;

    if ((newfp = __pmLogNewFile(base, nextvol)) != NULL) {
	struct timeval	stamp;
	fclose(logctl.l_mfp);
	logctl.l_mfp = newfp;
	logctl.l_label.ill_vol = logctl.l_curvol = nextvol;
	__pmLogWriteLabel(logctl.l_mfp, &logctl.l_label);
	fflush(logctl.l_mfp);
	stamp.tv_sec = ntohl(tvp->tv_sec);
	stamp.tv_usec = ntohl(tvp->tv_usec);
	fprintf(stderr, "%s: New log volume %d, at ", pmProgname, nextvol);
	__pmPrintStamp(stderr, &stamp);
	fputc('\n', stderr);
    }
    else {
	fprintf(stderr, "%s: Error: volume %d: %s\n",
		pmProgname, nextvol, pmErrStr(-oserror()));
	exit(1);
    }
    flushsize = 100000;
}


/*
 * construct new external label, and check label records from
 * input archives
 */
static void
xxnewlabel(void)
{
    __pmLogLabel	*lp = &logctl.l_label;

    /* check version number */
    inarchvers = inarch.label.ll_magic & 0xff;
    outarchvers = inarchvers;

    if (inarchvers != PM_LOG_VERS01 && inarchvers != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmProgname, inarchvers, inarch.name);
	exit(1);
    }

    /* copy magic number, pid, host and timezone */
    lp->ill_magic = inarch.label.ll_magic;
    lp->ill_pid = inarch.label.ll_magic;
    // TODO rewrite hostname?
    strncpy(lp->ill_hostname, inarch.label.ll_hostname, PM_LOG_MAXHOSTLEN);
    // TODO rewrite tz?
    strcpy(lp->ill_tz, inarch.label.ll_tz);

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
    rec->desc.pmid = PM_ID_NULL;
    rec->desc.type = PM_TYPE_NOSUPPORT;
    rec->desc.indom = PM_IN_NULL;
    rec->desc.sem = 0;
    rec->desc.units = nullunits;	/* struct assignment */
    rec->written = NOT_WRITTEN;
    rec->ptr = NULL;
    rec->next = NULL;
    return(rec);
}

/*
 * find indom in indomreclist - if it isn't in the list then add it in
 * with no pdu buffer
 */
static reclist_t *
findnadd_indomreclist(int indom)
{
    reclist_t	*curr;

    if (rindom == NULL) {
	rindom = mk_reclist_t();
	rindom->desc.pmid = PM_ID_NULL;
	rindom->desc.type = PM_TYPE_NOSUPPORT;
	rindom->desc.indom = indom;
	rindom->desc.sem = 0;
	rindom->desc.units = nullunits;	/* struct assignment */
	return(rindom);
    }
    else {
	curr = rindom;

	/* find matching record or last record */
	while (curr->next != NULL && curr->desc.indom != indom)
	    curr = curr->next;

	if (curr->desc.indom == indom) {
	    /* we have found a matching record - return the pointer */
	    return(curr);
	}
	else {
	    /* we have not found a matching record - append new record */
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->desc.pmid = PM_ID_NULL;
	    curr->desc.type = PM_TYPE_NOSUPPORT;
	    curr->desc.indom = indom;
	    curr->desc.sem = 0;
	    curr->desc.units = nullunits;	/* struct assignment */
	    return(curr);
	}
    }

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
		pmProgname, rec->desc.pmid, rec->desc.indom);
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
	indom = PM_IN_NULL;
	curr_indom = NULL;

	curr_desc = rdesc;
	while (curr_desc != NULL && curr_desc->desc.pmid != pmid)
	    curr_desc = curr_desc->next;

	if (curr_desc == NULL) {
	    /* descriptor has not been found - this is bad
	     */
	    fprintf(stderr, "%s: Error: meta data (TYPE_DESC) for pmid %s has not been found.\n", pmProgname, pmIDStr(pmid));
	    exit(1);
	}
	else {
	    /* descriptor has been found
	     */
	    if (curr_desc->written == WRITTEN) {
		/* descriptor has been written before (no need to write again)
		 * but still need to check indom
		 */
		indom = curr_desc->desc.indom;
		curr_indom = curr_desc->ptr;
	    }
	    else if (curr_desc->pdu == NULL) {
		/* descriptor is in list, has not been written, but no pdu
		 *  - this is bad
		 */
		fprintf(stderr, "%s: Error: missing pdu for pmid %s\n",
			pmProgname, pmIDStr(pmid));
	        exit(1);
	    }
	    else {
		/* descriptor is in list, has not been written, and has pdu
		 * write!
		 */
		curr_desc->written = MARK_FOR_WRITE;
		write_rec(curr_desc);
		indom = curr_desc->desc.indom;
		curr_indom = curr_desc->ptr;
	    }
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
	    while (curr_indom != NULL && curr_indom->desc.indom == indom) {
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
		pmProgname, osstrerror());
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
    int		j;
    int		want;
    int		numeof = 0;
    int		sts;
    pmID	pmid;			/* pmid for TYPE_DESC */
    pmInDom	indom;			/* indom for TYPE_INDOM */
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;
    __pmHashNode	*hnp;

    /* if at the end of meta file then skip this archive
     */
    if (inarch.eof[META]) {
	++numeof;
	return -1;
    }

    /* we should never already have a meta record
     */
    if (inarch.pb[META] != NULL) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    inarch.pb[META] is not NULL\n");
	exit(1);
    }
    ctxp = __pmHandleToPtr(inarch.ctx);
    lcp = ctxp->c_archctl->ac_log;

againmeta:
    /* get next meta record */

    if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &inarch.pb[META])) < 0) {
	inarch.eof[META] = 1;
	++numeof;
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mdfp);
	}
	return -1;
    }

    /* pmDesc entries, if not seen before & wanted,
     *	then append to desc list
     */
    if (ntohl(inarch.pb[META][1]) == TYPE_DESC) {
	pmid = __ntohpmID(inarch.pb[META][2]);

	/* if ml is defined, then look for pmid in the list
	 * if pmid is not in the list then discard it immediately
	 */
	want = 0;
	if (ml == NULL)
	    want = 1;
	else {
	    for (j=0; j<ml_numpmid; j++) {
		if (pmid == ml[j].idesc->pmid)
		    want = 1;
	    }
	}

	if (want) {
	    if ((hnp = __pmHashSearch((int)pmid, &mdesc_hash)) == NULL) {
		__pmHashAdd((int)pmid, NULL, &mdesc_hash);
	    }
	}
	else {
	    /* not wanted */
	    free(inarch.pb[META]);
	    inarch.pb[META] = NULL;
	    goto againmeta;
	}
    }
    else if (ntohl(inarch.pb[META][1]) == TYPE_INDOM) {
	/* if ml is defined, then look for instance domain in the list
	 * if indom is not in the list then discard it immediately
	 */
	indom = __ntohpmInDom(inarch.pb[META][4]);
	want = 0;
	if (ml == NULL)
	    want = 1;
	else {
	    for (j=0; j<ml_numpmid; j++) {
		if (indom == ml[j].idesc->indom)
		    want = 1;
	    }
	}

	if (want) {
	    if (__pmHashSearch((int)indom, &mindom_hash) == NULL) {
		/* meta record has never been seen ... add it to the list */
		__pmHashAdd((int)indom, NULL, &mindom_hash);
	    }
	}
	else {
	    /* META: don't want this meta */
	    free(inarch.pb[META]);
	    inarch.pb[META] = NULL;
	    goto againmeta;
	}
    }
    else {
	fprintf(stderr, "%s: Error: unrecognised meta data type: %d\n",
		pmProgname, (int)ntohl(inarch.pb[META][1]));
	exit(1);
    }

    if (numeof) return(-1);
    return(0);
}


/*
 * read in next log record for every archive
 */
static int
nextlog(void)
{
    int		eoflog = 0;	/* number of log files at eof */
    int		sts;
    double	curtime;
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;

    /* if at the end of log file then skip this archive
     */
    if (inarch.eof[LOG]) {
	++eoflog;
	return -1;
    }

    /* if mark has been written out, then log is at EOF
     */
    if (inarch.mark) {
	inarch.eof[LOG] = 1;
	++eoflog;
	return -1;
    }

againlog:
    ctxp = __pmHandleToPtr(inarch.ctx);
    lcp = ctxp->c_archctl->ac_log;

    if ((sts=__pmLogRead(lcp,PM_MODE_FORW,NULL,&inarch._result)) < 0) {
	if (sts != PM_ERR_EOL) {
	    fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
		    pmProgname, inarch.name, pmErrStr(sts));
	    _report(lcp->l_mfp);
	}
	/* if the first data record has not been written out, then
	 * do not generate a mark record, and you may as well ignore
	 * this archive
	 */
	if (first_datarec) {
	    inarch.mark = 1;
	    inarch.eof[LOG] = 1;
	    ++eoflog;
	}
	else {
	    inarch.mark = 1;
	    inarch.pb[LOG] = _createmark();
	}
    }


    /* set current log time - this is only done so that we can
     * determine whether to keep or discard the log
     */
    curtime = inarch._result->timestamp.tv_sec +
		    (double)inarch._result->timestamp.tv_usec / 1000000.0;

    /* if log time is greater than (or equal to) the current window
     * start time, then we may want it
     *	(irrespective of the current window end time)
     */
    if (curtime < winstart_time) {
	/* log is not in time window - discard result and get next record
	 */
	if (inarch._result != NULL) {
	    pmFreeResult(inarch._result);
	    inarch._result = NULL;
	}
	goto againlog;
    }
    else {
	/* log is within time window - check whether we want this record
	 */
	if (ml == NULL) {
	    /* ml is NOT defined, we want everything
	     */
	    inarch._Nresult = inarch._result;
	}
	else {
	    /* ml is defined, need to search metric list for wanted pmid's
	     *   (searchmlist may return a NULL pointer - this is fine)
	     */
	    inarch._Nresult = searchmlist(inarch._result);
	}

	if (inarch._Nresult == NULL) {
	    /* dont want any of the metrics in _result, try again
	     */
	    if (inarch._result != NULL) {
		    pmFreeResult(inarch._result);
		    inarch._result = NULL;
	    }
	    goto againlog;
	}
    }

    /* if we are here, then each archive control struct should either
     * be at eof, or it should have a _result, or it should have a mark PDU
     * (if we have a _result, we may want all/some/none of the pmid's in it)
     */

    if (eoflog) return(-1);
    return 0;
}

/*
 * parse command line arguments
 */
int
parseargs(int argc, char *argv[])
{
    int			c;
    int			sts;
    int			errflag = 0;
    struct stat		sbuf;

    while ((c = getopt(argc, argv, "c:CD:d?")) != EOF) {
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

	case 'd':	/* desperate to save output archive, even after error */
	    desperate = 1;
	    break;

	case 'C':	/* parse configs and quit */
	    Carg = 1;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag == 0 && optind != argc-2)
	errflag++;

    return -errflag;
}

int
parseconfig(void)
{
    int		errflag = 0;
    extern FILE * yyin;

    if ((yyin = fopen(configfile, "r")) == NULL) {
	fprintf(stderr, "%s: Cannot open config file \"%s\": %s\n",
		pmProgname, configfile, osstrerror());
	exit(1);
    }

    if (yyparse() != 0)
	exit(1);

    fclose(yyin);
    yyin = NULL;

    return(-errflag);
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
    unsigned long	peek_offset;

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

	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes
	 */
	peek_offset = ftell(logctl.l_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
	    newvolume(outarchname, (__pmTimeval *)&pb[3]);
	}

	/* write out the descriptor and instance domain pdu's first
	 */
	write_metareclist(elm->res, &needti);


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

	/* LOG: free PDU buffer */
	__pmUnpinPDUBuf(pb);
	pb = NULL;

	elm->res = NULL;
	elm->next = NULL;
	free(elm);

    } /*while(*rlready)*/
}


/*
 *  mark record has been created and assigned to inarch.pb[LOG]
 *  write it out
 */
void
writemark(void)
{
    int		sts;
    mark_t      *p = (mark_t *)inarch.pb[LOG];

    if (!inarch.mark) {
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

    if ((sts = __pmLogPutResult(&logctl, inarch.pb[LOG])) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }
    written++;
    free(inarch.pb[LOG]);
    inarch.pb[LOG] = NULL;
}

/*--- END FUNCTIONS ---------------------------------------------------------*/
/*
 * cni == currently not implemented
 */

int
main(int argc, char **argv)
{
    int		i;
    int		j;
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta;		/* sts from nextmeta() */

    char	*msg;

    double 	now = 0.0;		/* the current time */
    double 	mintime = 0.0;
    double 	tmptime = 0.0;

    __pmTimeval		tstamp;		/* temporary timestamp */
    rlist_t		*rlready;	/* list of results ready for writing */


    rlog = NULL;	/* list of log records to write */
    rdesc = NULL;	/* list of meta desc records to write */
    rindom = NULL;	/* list of meta indom records to write */
    rlready = NULL;


    __pmSetProgname(argv[0]);

    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	usage();
	exit(1);
    }

    /* input archive */
    inarch.name = argv[argc-2];
    inarch.pb[LOG] = inarch.pb[META] = NULL;
    inarch.eof[LOG] = inarch.eof[META] = 0;
    inarch.mark = 0;
    inarch._result = NULL;
    inarch._Nresult = NULL;

    if ((inarch.ctx = pmNewContext(PM_CONTEXT_ARCHIVE, inarch.name)) < 0) {
	fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		pmProgname, inarch.name, pmErrStr(inarch.ctx));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&inarch.label)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmProgname, inarch.name, pmErrStr(sts));
	exit(1);
    }

    /* output archive */
    outarchname = argv[argc-1];

    /*
     * process config file
     * TODO - more than one configfile via multiple -c options
     * TODO - configfile is dir => readdir and process all files
     */
    if (configfile != NULL) {
	if (parseconfig() < 0) {
	    usage();
	    exit(1);
	}
    }

    if (Carg) {
	indomspec_t	*ip;
	printf("PCP Archive Log Rewrite Specifications Summary\n");
	if (global.flags == 0)
	    printf("No global changes\n");
	else {
	    if ((global.flags & GLOBAL_CHANGE_HOSTNAME) != 0)
		printf("Hostname:\t%s -> %s\n", inarch.label.ll_hostname, global.hostname);
	    if ((global.flags & GLOBAL_CHANGE_TZ) != 0)
		printf("Timezone:\t%s -> %s\n", inarch.label.ll_tz, global.tz);
	    if ((global.flags & GLOBAL_CHANGE_TIME) != 0) {
		static struct tm	*tmp;
		char			*sign = "";
		time_t			time;
		if (global.time.tv_sec < 0) {
		    time = (time_t)(-global.time.tv_sec);
		    sign = "-";
		}
		else
		    time = (time_t)global.time.tv_sec;
		tmp = gmtime(&time);
		tmp->tm_hour += 24 * tmp->tm_yday;
		if (tmp->tm_hour < 10)
		    printf("Delta:\t\t-> %s%02d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
		else
		    printf("Delta:\t\t-> %s%d:%02d:%02d.%06d\n", sign, tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)global.time.tv_usec);
	    }
	}
	for (ip = indom_root; ip != NULL; ip = ip->i_next) {
	    // TODO handle serial is * case
	    printf("\nInstance Domain: %s\n", pmInDomStr(ip->old_indom));
	    if (ip->new_indom != PM_INDOM_NULL) {
		// TODO handle serial is * case
		printf("pmInDom:\t-> %s\n", pmInDomStr(ip->new_indom));
	    }
	    for (i = 0; i < ip->numinst; i++) {
		if (ip->flags[i]) {
		    printf("Instance:\t\[%d] \"%s\" -> ", ip->old_inst[i], ip->old_iname[i]);
		    if (ip->flags[i] & INST_DELETE)
			printf("DELETE\n");
		    else {
			if (ip->flags[i] & INST_CHANGE_INST)
			    printf("[%d] ", ip->new_inst[i]);
			else
			    printf("[%d] ", ip->old_inst[i]);
			if (ip->flags[i] & INST_CHANGE_INAME)
			    printf("\"%s\"\n", ip->new_iname[i]);
			else
			    printf("\"%s\"\n", ip->old_iname[i]);
		    }
		}
	    }
	}
	exit(0);
    }

#if 0

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
    xxnewlabel();

    /* write label record */
    writelabel_metati(0);

    ilog = -1;
    written = 0;
    curlog.tv_sec = 0;
    curlog.tv_usec = 0;
    current.tv_sec = 0;
    current.tv_usec = 0;
    first_datarec = 1;
    pre_startwin = 1;

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

	if (stslog < 0)
	    break;

	/* find the _Nresult (or mark pdu) with the earliest timestamp;
	 * set ilog
	 * (this is a bit more complex when tflag is specified)
	 */
	mintime = 0.0;
	if (inarch._Nresult != NULL) {
	    tstamp.tv_sec = inarch._Nresult->timestamp.tv_sec;
	    tstamp.tv_usec = inarch._Nresult->timestamp.tv_usec;
	    checklogtime(&tstamp, i);

	    if (ilog == i) {
		tmptime = curlog.tv_sec + (double)curlog.tv_usec/1000000.0;
		if (mintime <= 0 || mintime > tmptime)
		    mintime = tmptime;
	    }
	}
	else if (inarch.pb[LOG] != NULL) {
	    tstamp.tv_sec = inarch.pb[LOG][3]; /* no swab needed */
	    tstamp.tv_usec = inarch.pb[LOG][4]; /* no swab needed */
	    checklogtime(&tstamp, i);

	    if (ilog == i) {
		tmptime = curlog.tv_sec + (double)curlog.tv_usec/1000000.0;
		if (mintime <= 0 || mintime > tmptime)
		    mintime = tmptime;
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
	if (now > logend_time)
	    break;

	current.tv_sec = (long)now;
	current.tv_usec = (now - (double)current.tv_sec) * 1000000.0;

	/* prepare to write out log record
	 */
	if (ilog != 0) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    exit(1);
	}

	if (inarch.mark)
	    writemark();
	else {
	    /* result is to be written out, but there is no _Nresult
	     */
	    if (inarch._Nresult == NULL) {
		fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
		fprintf(stderr, "    pick == LOG and _Nresult = NULL\n");
		exit(1);
	    }
	    insertresult(&rlready, inarch._Nresult);
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
	    if (inarch._result != inarch._Nresult && inarch._Nresult != NULL) {
		pmValueSet	*vsetp;
		for (i=0; i<inarch._Nresult->numpmid; i++) {
		    vsetp = inarch._Nresult->vset[i];
		    if (vsetp->valfmt == PM_VAL_SPTR) {
			for (j=0; j<vsetp->numval; j++) {
			    free(vsetp->vlist[j].value.pval);
			}
		    }
		}
		free(inarch._Nresult);
	    }
	    if (inarch._result != NULL)
		pmFreeResult(inarch._result);
	    inarch._result = NULL;
	    inarch._Nresult = NULL;
	}
    } /*while()*/

    if (first_datarec) {
        fprintf(stderr, "%s: Warning: no qualifying records found.\n",
                pmProgname);
cleanup:
	exit(1);
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

#endif
    exit(exit_status);
    return(0);
}
