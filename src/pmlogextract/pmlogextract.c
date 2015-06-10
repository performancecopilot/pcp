/*
 * pmlogextract - extract desired metrics from PCP archive logs
 *
 * Copyright (c) 2014 Red Hat.
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
#include <assert.h>
#include "pmapi.h"
#include "impl.h"
#include "logger.h"

#ifdef PCP_DEBUG
long totalmalloc;
#endif
static pmUnits nullunits;
static int desperate;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "config", 1, 'c', "FILE", "file to load configuration from" },
    { "desperate", 0, 'd', 0, "desperate, save output after fatal error" },
    { "first", 0, 'f', 0, "use timezone from first archive [default is last]" },
    PMOPT_START,
    { "samples", 1, 's', "NUM", "terminate after NUM log records have been written" },
    PMOPT_FINISH,
    { "", 1, 'v', "SAMPLES", "switch log volumes after this many samples" },
    { "", 0, 'w', 0, "ignore day/month/year" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:D:dfS:s:T:v:wZ:z?",
    .long_options = longopts,
    .short_usage = "[options] input-archive output-archive",
};

/*
 * extract metric name(s) from metadata pdu buffer
 */
void
printmetricnames(FILE *f, __pmPDU *pdubuf)
{
    if (ntohl(pdubuf[0]) > 8) {
	/*
	 * have at least one name ... names are packed
	 * <len><name>... at the end of the buffer
	 */
	int	numnames = ntohl(pdubuf[7]);
	char	*p = (char *)&pdubuf[8];
	int	i;
	__pmPDU	len;
	for (i = 0; i < numnames; i++) {
	    memmove((void *)&len, (void *)p, sizeof(__pmPDU));
	    len = ntohl(len);
	    p += sizeof(__pmPDU);
	    if (i > 0) fprintf(f, ", ");
	    fprintf(f, "%*.*s", len, len, p);
	    p += len;
	}
    }
    else
	fprintf(f, "<noname>");
}

#define MATCH_NONE	0
#define MATCH_SUBSET	1
#define MATCH_SUPERSET	2
#define MATCH_SOME	3
#define MATCH_EQUAL	4
/*
 * compare sets of metric name(s) from two metadata pdu buffers
 *
 * returns MATCH_NONE if no intersection
 * returns MATCH_SUBSET if {a names} is a proper subset of {b names}
 * returns MATCH_SUPERSET if {a names} is a proper superset of {b names}
 * returns MATCH_SOME if the intersection of {a names} and {b names} is
 * not empty
 * returns MATCH_EQUAL {a names} is identical to {b names}
 */
int
matchnames(__pmPDU *a, __pmPDU *b)
{
    int		num_a = 0;
    int		num_b = 0;
    int		num_a_eq = 0;
    int		num_b_eq = 0;
    int		i_a;
    int		i_b;
    char	*p_a;
    char	*p_b;
    int		sts;

    if (ntohl(a[0]) > 8) num_a = ntohl(a[7]);
    if (ntohl(b[0]) > 8) num_b = ntohl(b[7]);

    /*
     * count number of names from {a names} that match names in {b names}
     */
    p_a = (char *)&a[8];
    for (i_a = 0; i_a < num_a; i_a++) {
	__pmPDU		len_a;

	memmove((void *)&len_a, (void *)p_a, sizeof(__pmPDU));
	len_a = ntohl(len_a);
	p_a += sizeof(__pmPDU);
	p_b = (char *)&b[8];
	for (i_b = 0; i_b < num_b; i_b++) {
	    __pmPDU		len_b;
	    memmove((void *)&len_b, (void *)p_b, sizeof(__pmPDU));
	    len_b = ntohl(len_b);
	    p_b += sizeof(__pmPDU);

	    if (len_a == len_b && strncmp(p_a, p_b, len_a) == 0) {
		num_a_eq++;
		break;
	    }

	    p_b += len_b;
	}
	p_a += len_a;
    }


    /*
     * count number of names from {b names} that match names in {a names}
     * ... this would not be necessary, but due to an apparent pmlogger
     * bug, the _same_ name can appear in either or both of the {a names}
     * and {b names} sets, which complicates the logic here.
     */
    p_b = (char *)&b[8];
    for (i_b = 0; i_b < num_b; i_b++) {
	__pmPDU		len_b;

	memmove((void *)&len_b, (void *)p_b, sizeof(__pmPDU));
	len_b = ntohl(len_b);
	p_b += sizeof(__pmPDU);
	p_a = (char *)&a[8];
	for (i_a = 0; i_a < num_a; i_a++) {
	    __pmPDU		len_a;
	    memmove((void *)&len_a, (void *)p_a, sizeof(__pmPDU));
	    len_a = ntohl(len_a);
	    p_a += sizeof(__pmPDU);

	    if (len_b == len_a && strncmp(p_b, p_a, len_b) == 0) {
		num_b_eq++;
		break;
	    }

	    p_a += len_a;
	}
	p_b += len_b;
    }

    if (num_a_eq == 0 && num_b_eq == 0) sts = MATCH_NONE;
    else if (num_a == num_b) {
	if (num_a_eq == num_a && num_b_eq == num_b) sts = MATCH_EQUAL;
	else sts = MATCH_SOME;
    }
    else if (num_a > num_b) {
	if (num_b_eq == num_b) sts = MATCH_SUPERSET;
	else sts = MATCH_SOME;
    }
    else {
	/* num_a < num_b */
	if (num_a_eq == num_a) sts = MATCH_SUBSET;
	else sts = MATCH_SOME;
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	fprintf(stderr, "matchnames: ");
	printmetricnames(stderr, a);
	fprintf(stderr, " : ");
	printmetricnames(stderr, b);
	fprintf(stderr, " num_a=%d num_b=%d num_a_eq=%d num_b_eq=%d -> %d\n", num_a, num_b, num_a_eq, num_b_eq, sts);
    }
#endif

    return sts;
}

void
printsem(FILE *f, int sem)
{
    switch (sem) {
	case PM_SEM_COUNTER:
	    fprintf(f, "counter");
	    break;
	case PM_SEM_INSTANT:
	    fprintf(f, "instant");
	    break;
	case PM_SEM_DISCRETE:
	    fprintf(f, "discrete");
	    break;
	default:
	    fprintf(f, "unknown (%d)", sem);
	    break;
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

static __pmTimeval 	winstart = {-1,0};	/* window start time */
static __pmTimeval	winend = {-1,0};	/* window end time */
static __pmTimeval	logend = {-1,0};	/* log end time */

/* command line args */
char	*configfile = NULL;		/* -c arg - name of config file */
int	farg = 0;			/* -f arg - use first timezone */
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

/*
 * return -1, 0 or 1 as the __pmTimeval's compare
 * a < b, a == b or a > b
 */
static int
tvcmp(__pmTimeval a, __pmTimeval b)
{
    if (a.tv_sec < b.tv_sec)
	return -1;
    if (a.tv_sec > b.tv_sec)
	return 1;
    if (a.tv_usec < b.tv_usec)
	return -1;
    if (a.tv_usec > b.tv_usec)
	return 1;
    return 0;
}

static void
abandon()
{
    char    fname[MAXNAMELEN];
    if (desperate == 0) {
	fprintf(stderr, "Archive \"%s\" not created.\n", outarchname);
	while (logctl.l_curvol >= 0) {
	    snprintf(fname, sizeof(fname), "%s.%d", outarchname, logctl.l_curvol);
	    unlink(fname);
	    logctl.l_curvol--;
	}
	snprintf(fname, sizeof(fname), "%s.meta", outarchname);
	unlink(fname);
	snprintf(fname, sizeof(fname), "%s.index", outarchname);
	unlink(fname);
    }
    exit(1);
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
	abandon();
    }
    flushsize = 100000;
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

    if (inarchvers != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmProgname, inarchvers, iap->name);
	abandon();
    }

    /* copy magic number, pid, host and timezone */
    lp->ill_magic = iap->label.ll_magic;
    lp->ill_pid = (int)getpid();
    strncpy(lp->ill_hostname, iap->label.ll_hostname, PM_LOG_MAXHOSTLEN);
    lp->ill_hostname[PM_LOG_MAXHOSTLEN-1] = '\0';
    if (farg) {
	/*
	 * use timezone from first archive ... this is the OLD default
	 */
	strcpy(lp->ill_tz, iap->label.ll_tz);
    }
    else {
	/*
	 * use timezone from last archive ... this is the NEW default
	 */
	strcpy(lp->ill_tz, inarch[inarchnum-1].label.ll_tz);
    }

    /* reset outarch as appropriate, depending on other input archives */
    for (i=0; i<inarchnum; i++) {
	iap = &inarch[i];

	/* Ensure all archives of the same version number */
        if ((iap->label.ll_magic & 0xff) != inarchvers) {
	    fprintf(stderr, 
		"%s: Error: input archives with different version numbers\n"
		"archive: %s version: %d\n"
		"archive: %s version: %d\n",
		    pmProgname, inarch[0].name, inarchvers,
		    iap->name, (iap->label.ll_magic & 0xff));
	    abandon();
        }

	/* Ensure all archives of the same host */
	if (strcmp(lp->ill_hostname, iap->label.ll_hostname) != 0) {
	    fprintf(stderr,"%s: Error: host name mismatch for input archives\n",
		    pmProgname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    inarch[0].name, inarch[0].label.ll_hostname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    iap->name, iap->label.ll_hostname);
	    abandon();
	}

	/* Ensure all archives of the same timezone */
	if (strcmp(lp->ill_tz, iap->label.ll_tz) != 0) {
	    fprintf(stderr,
		"%s: Warning: timezone mismatch for input archives\n",
		    pmProgname);
	    if (farg) {
		fprintf(stderr, "archive: %s timezone: %s [will be used]\n",
		    inarch[0].name, lp->ill_tz);
		fprintf(stderr, "archive: %s timezone: %s [will be ignored]\n",
		    iap->name, iap->label.ll_tz);
	    }
	    else {
		fprintf(stderr, "archive: %s timezone: %s [will be used]\n",
		    inarch[inarchnum-1].name, lp->ill_tz);
		fprintf(stderr, "archive: %s timezone: %s [will be ignored]\n",
		    iap->name, iap->label.ll_tz);
	    }
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
	abandon();
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += sizeof(reclist_t);
        fprintf(stderr, "mk_reclist_t: allocated %d\n", (int)sizeof(reclist_t));
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
}

/*
 *  append a new record to the desc meta record list if not seen
 *  before, else check the desc meta record is semantically the
 *  same as the last desc meta record for this pmid from this source
 */
void
update_descreclist(int i)
{
    inarch_t	*iap;
    reclist_t	*curr;
    pmUnits	pmu;
    pmUnits	*pmup;

    iap = &inarch[i];

    if (rdesc == NULL) {
	/* first time */
	curr = rdesc = mk_reclist_t();
    }
    else {
	curr = rdesc;
	/* find matching record or last record */
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    fprintf(stderr, "update_descreclist: looking for ");
	    printmetricnames(stderr, iap->pb[META]);
	    fprintf(stderr, " (pmid:%s)\n", pmIDStr(ntoh_pmID(iap->pb[META][2])));
	}
#endif
	while (curr->next != NULL && curr->desc.pmid != ntoh_pmID(iap->pb[META][2])) {
	    if (curr->pdu != NULL) {
		if (matchnames(curr->pdu, iap->pb[META]) != MATCH_NONE) {
		    fprintf(stderr, "%s: Error: metric ", pmProgname);
		    printmetricnames(stderr, curr->pdu);
		    fprintf(stderr, ": PMID changed from %s", pmIDStr(curr->desc.pmid));
		    fprintf(stderr, " to %s!\n", pmIDStr(ntoh_pmID(iap->pb[META][2])));
		    abandon();
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL1) {
		    fprintf(stderr, "update_descreclist: nomatch ");
		    printmetricnames(stderr, curr->pdu);
		    fprintf(stderr, " (pmid:%s)\n", pmIDStr(curr->desc.pmid));
		}
#endif
	    }
	    curr = curr->next;
	}
    }

    if (curr->desc.pmid == ntoh_pmID(iap->pb[META][2])) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1) {
	    fprintf(stderr, "update_descreclist: pmid match ");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, " type: old %s", pmTypeStr(curr->desc.type));
	    fprintf(stderr, " new %s", pmTypeStr(ntohl(iap->pb[META][3])));
	    fprintf(stderr, " indom: old %s", pmInDomStr(curr->desc.indom));
	    fprintf(stderr, " new %s", pmInDomStr(ntoh_pmInDom(iap->pb[META][4])));
	    fprintf(stderr, " sem: old ");
	    printsem(stderr, curr->desc.sem);
	    fprintf(stderr, " new ");
	    printsem(stderr, (int)ntohl(iap->pb[META][5]));
	    fprintf(stderr, " units: old %s", pmUnitsStr(&curr->desc.units));
	    pmup = (pmUnits *)&iap->pb[META][6];
	    pmu = ntoh_pmUnits(*pmup);
	    fprintf(stderr, " new %s", pmUnitsStr(&pmu));
	    fputc('\n', stderr);
	}
#endif
	if (matchnames(curr->pdu, iap->pb[META]) != MATCH_EQUAL) {
	    fprintf(stderr, "%s: Error: metric PMID %s", pmProgname, pmIDStr(curr->desc.pmid));
	    fprintf(stderr, ": name changed from ");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, " to ");
	    printmetricnames(stderr, iap->pb[META]);
	    fprintf(stderr, "!\n");
	    abandon();
	}
	if (curr->desc.type != ntohl(iap->pb[META][3])) {
	    fprintf(stderr, "%s: Error: metric ", pmProgname);
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": type changed from");
	    fprintf(stderr, " %s", pmTypeStr(curr->desc.type));
	    fprintf(stderr, " to %s!\n", pmTypeStr(ntohl(iap->pb[META][3])));
	    abandon();
	}
	if (curr->desc.indom != ntoh_pmInDom(iap->pb[META][4])) {
	    fprintf(stderr, "%s: Error: metric ", pmProgname);
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": indom changed from");
	    fprintf(stderr, " %s", pmInDomStr(curr->desc.indom));
	    fprintf(stderr, " to %s!\n", pmInDomStr(ntoh_pmInDom(iap->pb[META][4])));
	    abandon();
	}
	if (curr->desc.sem != ntohl(iap->pb[META][5])) {
	    fprintf(stderr, "%s: Error: metric ", pmProgname);
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": semantics changed from");
	    fprintf(stderr, " ");
	    printsem(stderr, curr->desc.sem);
	    fprintf(stderr, " to ");
	    printsem(stderr, (int)ntohl(iap->pb[META][5]));
	    fprintf(stderr, "!\n");
	    abandon();
	}
	pmup = (pmUnits *)&iap->pb[META][6];
	pmu = ntoh_pmUnits(*pmup);
	if (curr->desc.units.dimSpace != pmu.dimSpace ||
	    curr->desc.units.dimTime != pmu.dimTime ||
	    curr->desc.units.dimCount != pmu.dimCount ||
	    curr->desc.units.scaleSpace != pmu.scaleSpace ||
	    curr->desc.units.scaleTime != pmu.scaleTime ||
	    curr->desc.units.scaleCount != pmu.scaleCount) {
	    fprintf(stderr, "%s: Error: metric ", pmProgname);
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": units changed from");
	    fprintf(stderr, " %s", pmUnitsStr(&curr->desc.units));
	    fprintf(stderr, " to %s!\n", pmUnitsStr(&pmu));
	    abandon();
	}
	/* not adding, so META: discard new record */
	free(iap->pb[META]);
	iap->pb[META] = NULL;
    }
    else {
	/* append new record */
	curr->next = mk_reclist_t();
	curr = curr->next;
	curr->pdu = iap->pb[META];
	curr->desc.pmid = ntoh_pmID(iap->pb[META][2]);
	curr->desc.type = ntohl(iap->pb[META][3]);
	curr->desc.indom = ntoh_pmInDom(iap->pb[META][4]);
	curr->desc.sem = ntohl(iap->pb[META][5]);
	pmup =(pmUnits *)&iap->pb[META][6];
	curr->desc.units = ntoh_pmUnits(*pmup);
	curr->ptr = findnadd_indomreclist(curr->desc.indom);
	iap->pb[META] = NULL;
    }
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
	rindom->stamp.tv_sec = ntohl(rindom->pdu[2]);
	rindom->stamp.tv_usec = ntohl(rindom->pdu[3]);
	rindom->desc.pmid = PM_ID_NULL;
	rindom->desc.type = PM_TYPE_NOSUPPORT;
	rindom->desc.indom = ntoh_pmInDom(iap->pb[META][4]);
	rindom->desc.sem = 0;
	rindom->desc.units = nullunits;	/* struct assignment */
    }
    else {
	curr = rindom;

	/* find matching record or last record */
	while (curr->next != NULL && curr->desc.indom != ntoh_pmInDom(iap->pb[META][4])) {
	    curr = curr->next;
	}

	if (curr->desc.indom == ntoh_pmInDom(iap->pb[META][4])) {
	    if (curr->pdu == NULL) {
		/* insert new record */
		curr->pdu = iap->pb[META];
		curr->stamp.tv_sec = ntohl(curr->pdu[2]);
		curr->stamp.tv_usec = ntohl(curr->pdu[3]);
	    }
	    else {
		/* do NOT discard old record; insert new record */
		rec = mk_reclist_t();
		rec->pdu = iap->pb[META];
		rec->stamp.tv_sec = ntohl(rec->pdu[2]);
		rec->stamp.tv_usec = ntohl(rec->pdu[3]);
		rec->desc.pmid = PM_ID_NULL;
		rec->desc.type = PM_TYPE_NOSUPPORT;
		rec->desc.indom = ntoh_pmInDom(iap->pb[META][4]);
		rec->desc.sem = 0;
		rec->desc.units = nullunits;	/* struct assignment */
		rec->next = curr->next;
		curr->next = rec;
	    }
	}
	else {
	    /* append new record */
	    curr->next = mk_reclist_t();
	    curr = curr->next;
	    curr->pdu = iap->pb[META];
	    curr->stamp.tv_sec = ntohl(curr->pdu[2]);
	    curr->stamp.tv_usec = ntohl(curr->pdu[3]);
	    curr->desc.pmid = PM_ID_NULL;
	    curr->desc.type = PM_TYPE_NOSUPPORT;
	    curr->desc.indom = ntoh_pmInDom(iap->pb[META][4]);
	    curr->desc.sem = 0;
	    curr->desc.units = nullunits;	/* struct assignment */
	}
    } /*else*/

    iap->pb[META] = NULL;
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
	    abandon();
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_LOGMETA) {
	    __pmLogHdr	*h;
	    int		len;
	    int		type;
	    h = (__pmLogHdr *)rec->pdu;
	    len = ntohl(h->len);
	    type = ntohl(h->type);
	    fprintf(stderr, "write_rec: record len=%d, type=%d @ offset=%d\n",
	    	len, type, (int)(ftell(logctl.l_mdfp) - sizeof(__pmLogHdr)));
	    if (type == TYPE_DESC) {
		pmDesc	*dp;
		pmDesc	desc;
		int	*namelen;
		char	*name;	/* just first name for diag */
		dp = (pmDesc *)((void *)rec->pdu + sizeof(__pmLogHdr));
		desc.type = ntohl(dp->type);
		desc.sem = ntohl(dp->sem);
		desc.indom = ntoh_pmInDom(dp->indom);
		desc.units = ntoh_pmUnits(dp->units);
		desc.pmid = ntoh_pmID(dp->pmid);
		namelen = (int *)((void *)rec->pdu + sizeof(__pmLogHdr) + sizeof(pmDesc) + sizeof(int));
		len = ntohl(*namelen);
		name = (char *)((void *)rec->pdu + sizeof(__pmLogHdr) + sizeof(pmDesc) + sizeof(int) + sizeof(int));
		fprintf(stderr, "PMID: %s name: %*.*s\n", pmIDStr(desc.pmid), len, len, name);
		__pmPrintDesc(stderr, &desc);
	    }
	    else if (type == TYPE_INDOM) {
		__pmTimeval	*tvp;
		__pmTimeval	when;
		int		k = 2;
		pmInDom		indom;
		int		numinst;
		int		*instlist;
		int		inst;

		tvp = (__pmTimeval *)&rec->pdu[k];
		when.tv_sec = ntohl(tvp->tv_sec);
		when.tv_usec = ntohl(tvp->tv_usec);
		k += sizeof(__pmTimeval)/sizeof(rec->pdu[0]);
		indom = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "INDOM: %s when: ", pmInDomStr(indom));
		__pmPrintTimeval(stderr, &when);
		numinst = ntohl(rec->pdu[k++]);
		fprintf(stderr, " numinst: %d", numinst);
		if (numinst > 0) {
		    int		i;
		    instlist = (int *)&rec->pdu[k];
		    for (i = 0; i < numinst; i++) {
			inst = ntohl(instlist[i]);
			fprintf(stderr, " [%d] %d", i, inst);
		    }
		}
		fputc('\n', stderr);
	    }
	    else {
		fprintf(stderr, "Botch: bad type\n");
	    }
	}
#endif

	/* write out the pdu ; exit if write failed */
	if ((sts = _pmLogPut(logctl.l_mdfp, rec->pdu)) < 0) {
	    fprintf(stderr, "%s: Error: _pmLogPut: meta data : %s\n",
		    pmProgname, pmErrStr(sts));
	    abandon();
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
	    abandon();
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
	        abandon();
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
	     */
	    othr_indom = NULL;
	    while (curr_indom != NULL && curr_indom->desc.indom == indom) {
		if (curr_indom->stamp.tv_sec < this->tv_sec ||
		         (curr_indom->stamp.tv_sec == this->tv_sec &&
		          curr_indom->stamp.tv_usec <= this->tv_usec))
		{
		    /* indom is in list, indom has pdu
		     * and timestamp in pdu suits us
		     */
		    if (othr_indom == NULL) {
			othr_indom = curr_indom;
		    }
		    else if (othr_indom->stamp.tv_sec < curr_indom->stamp.tv_sec ||
			     (othr_indom->stamp.tv_sec == curr_indom->stamp.tv_sec &&
			      othr_indom->stamp.tv_usec <= curr_indom->stamp.tv_usec))
		    {
			/* we already have a perfectly good indom,
			 * but curr_indom has a better timestamp
			 */
			othr_indom = curr_indom;
		    }
		}
		curr_indom = curr_indom->next;
	    } /*while()*/

	    if (othr_indom != NULL && othr_indom->pdu != NULL && othr_indom->written != WRITTEN) {
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

    /*
     * add space for trailer in case __pmLogPutResult2() is called with
     * this PDU buffer
     */
    markp = (mark_t *)malloc(sizeof(mark_t)+sizeof(int));
    if (markp == NULL) {
	fprintf(stderr, "%s: Error: mark_t malloc: %s\n",
		pmProgname, osstrerror());
	abandon();
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += sizeof(mark_t);
        fprintf(stderr, "_createmark : allocated %d\n", (int)sizeof(mark_t));
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
    int		want;
    int		numeof = 0;
    int		sts;
    pmID	pmid;			/* pmid for TYPE_DESC */
    pmInDom	indom;			/* indom for TYPE_INDOM */
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;
    inarch_t	*iap;			/* pointer to input archive control */

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
	    abandon();
	}
	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmProgname, iap->ctx);
	    abandon();
	}
	lcp = ctxp->c_archctl->ac_log;

againmeta:
	/* get next meta record */

	if ((sts = _pmLogGet(lcp, PM_LOG_VOL_META, &iap->pb[META])) < 0) {
	    iap->eof[META] = 1;
	    ++numeof;
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
			pmProgname, iap->name, pmErrStr(sts));
		_report(lcp->l_mdfp);
		abandon();
	    }
	    PM_UNLOCK(ctxp->c_lock);
	    continue;
	}

	/* pmDesc entries, if not seen before & wanted,
	 *	then append to desc list
	 */
	if (ntohl(iap->pb[META][1]) == TYPE_DESC) {
	    pmid = ntoh_pmID(iap->pb[META][2]);

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
		if (__pmHashSearch((int)pmid, &mdesc_hash) == NULL)
		    __pmHashAdd((int)pmid, NULL, &mdesc_hash);
		/*
		 * update the desc list (add first time, check on subsequent
		 * sightings of desc for this pmid from this source
		 * update_descreclist() sets pb[META] to NULL
		 */
		update_descreclist(i);
	    }
	    else {
		/* not wanted */
		free(iap->pb[META]);
		iap->pb[META] = NULL;
		goto againmeta;
	    }
	}
	else if (ntohl(iap->pb[META][1]) == TYPE_INDOM) {
	    /* if ml is defined, then look for instance domain in the list
	     * if indom is not in the list then discard it immediately
	     */
	    indom = ntoh_pmInDom(iap->pb[META][4]);
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
		/* add to indom list */
		/* append_indomreclist() sets pb[META] to NULL
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
		    pmProgname, (int)ntohl(iap->pb[META][1]));
	    abandon();
	}

	PM_UNLOCK(ctxp->c_lock);
    }

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
    __pmTimeval	curtime;
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

	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmProgname, iap->ctx);
	    abandon();
	}
	lcp = ctxp->c_archctl->ac_log;

againlog:
	if ((sts=__pmLogRead(lcp, PM_MODE_FORW, NULL, &iap->_result, PMLOGREAD_NEXT)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
			pmProgname, iap->name, pmErrStr(sts));
		_report(lcp->l_mfp);
		if (sts != PM_ERR_LOGREC)
		    abandon();
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
	    PM_UNLOCK(ctxp->c_lock);
	    continue;
	}
	assert(iap->_result != NULL);


	/* set current log time - this is only done so that we can
	 * determine whether to keep or discard the log
	 */
	curtime.tv_sec = iap->_result->timestamp.tv_sec;
	curtime.tv_usec = iap->_result->timestamp.tv_usec;

	/* if log time is greater than (or equal to) the current window
	 * start time, then we may want it
	 *	(irrespective of the current window end time)
	 */
	if (tvcmp(curtime, winstart) < 0) {
	    /* log is not in time window - discard result and get next record
	     */
	    pmFreeResult(iap->_result);
	    iap->_result = NULL;
	    goto againlog;
	}
        else {
            /* log is within time window - check whether we want this record
             */
            if (iap->_result->numpmid == 0) {
		/* mark record, process this one as is
		 */
                iap->_Nresult = iap->_result;
	    }
            else if (ml == NULL) {
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
		pmFreeResult(iap->_result);
		iap->_result = NULL;
                goto againlog;
            }
	}

	PM_UNLOCK(ctxp->c_lock);
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
    int			c;
    int			sts;
    char		*endnum;
    struct stat		sbuf;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'c':	/* config file */
	    configfile = opts.optarg;
	    if (stat(configfile, &sbuf) < 0) {
		pmprintf("%s: %s - invalid file\n", pmProgname, configfile);
		opts.errors++;
	    }
	    break;

	case 'D':	/* debug flag */
	    sts = __pmParseDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'd':	/* desperate to save output archive, even after error */
	    desperate = 1;
	    break;

	case 'f':	/* use timezone from first archive */
	    farg = 1;
	    break;

	case 's':	/* number of samples to write out */
	    sarg = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || sarg < 0) {
		pmprintf("%s: -s requires numeric argument\n", pmProgname);
		opts.errors++;
	    }
	    break;

	case 'S':	/* start time for extracting */
	    Sarg = opts.optarg;
	    break;

	case 'T':	/* end time for extracting */
	    Targ = opts.optarg;
	    break;

	case 'v':	/* number of samples per volume */
	    varg = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || varg < 0) {
		pmprintf("%s: -v requires numeric argument\n", pmProgname);
		opts.errors++;
	    }
	    break;

	case 'w':	/* ignore day/month/year */
	    warg++;
	    break;

	case 'Z':	/* use timezone from command line */
	    if (zarg) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		opts.errors++;
	    }
	    tz = opts.optarg;
	    break;

	case 'z':	/* use timezone from archive */
	    if (tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmProgname);
		opts.errors++;
	    }
	    zarg++;
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (warg) {
	if (Sarg == NULL || Targ == NULL) {
	    fprintf(stderr, "%s: Warning: -w flag requires that both -S and -T are specified.\nIgnoring -w flag.\n", pmProgname);
	    warg = 0;
	}
    }


    if (opts.errors == 0 && opts.optind > argc - 2) {
	pmprintf("%s: Error: insufficient arguments\n", pmProgname);
	opts.errors++;
    }

    return -opts.errors;
}

int
parseconfig(void)
{
    int		errflag = 0;

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
 *  we are within time window ... return 0
 *  we are outside of time window & mk new window ... return 1
 *  we are outside of time window & exit ... return -1
 */
static int
checkwinend(__pmTimeval now)
{
    int		i;
    int		sts;
    __pmTimeval	tmptime;
    inarch_t	*iap;
    __pmPDU	*markpdu;	/* mark b/n time windows */

    if (winend.tv_sec < 0 || tvcmp(now, winend) <= 0)
	return(0);

    /* we have reached the end of a window
     *	- if warg is not set, then we have finished (break)
     *	- otherwise, calculate start and end of next window,
     *		     set pre_startwin, discard logs before winstart,
     * 		     and write out mark
     */
    if (!warg)
	return(-1);

    winstart.tv_sec += NUM_SEC_PER_DAY;
    winend.tv_sec += NUM_SEC_PER_DAY;
    pre_startwin = 1;

    /* if start of next window is later than max termination
     * then bail out here
     */
    if (tvcmp(winstart, logend) > 0)
	    return(-1);

    ilog = -1;
    for (i=0; i<inarchnum; i++) {
	iap = &inarch[i];
	if (iap->_Nresult != NULL) {
	    tmptime.tv_sec = iap->_Nresult->timestamp.tv_sec;
	    tmptime.tv_usec = iap->_Nresult->timestamp.tv_usec;
	    if (tvcmp(tmptime, winstart) < 0) {
		/* free _result and _Nresult
		 */
		if (iap->_result != iap->_Nresult) {
		    free(iap->_Nresult);
		}
		if (iap->_result != NULL) {
		    pmFreeResult(iap->_result);
		    iap->_result = NULL;
		}
		iap->_Nresult = NULL;
		iap->pb[LOG] = NULL;
	    }
	}
	if (iap->pb[LOG] != NULL) {
	    tmptime.tv_sec = ntohl(iap->pb[LOG][3]);
	    tmptime.tv_usec = ntohl(iap->pb[LOG][4]);
	    if (tvcmp(tmptime, winstart) < 0) {
		/* free PDU buffer ... it is probably a mark
		 * and has not been pinned
		 */
		free(iap->pb[LOG]);
		iap->pb[LOG] = NULL;
	    }
	}
    } /*for(i)*/

    /* must create "mark" record and write it out */
    /* (need only one mark record) */
    markpdu = _createmark();
    if ((sts = __pmLogPutResult2(&logctl, markpdu)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		pmProgname, pmErrStr(sts));
	abandon();
    }
    written++;
    free(markpdu);
    return(1);
}


/*
 *
 */
void
writerlist(rlist_t **rlready, __pmTimeval mintime)
{
    int		sts;
    int		needti = 0;	/* need to flush/update */
    __pmTimeval	titime  = {0,0};/* time of last temporal index write */
    __pmTimeval	restime;	/* time of result */
    rlist_t	*elm;		/* element of rlready to be written out */
    __pmPDU	*pb;		/* pdu buffer */
    unsigned long	peek_offset;

    while (*rlready != NULL) {
	restime.tv_sec = (*rlready)->res->timestamp.tv_sec;
	restime.tv_usec = (*rlready)->res->timestamp.tv_usec;

        if (tvcmp(restime, mintime) > 0) {
#if 0
fprintf(stderr, "writelist: restime %d.%06d mintime %d.%06d ", restime.tv_sec, restime.tv_usec, mintime.tv_sec, mintime.tv_usec);
fprintf(stderr, " break!\n");
#endif
	    break;
	}

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
	sts = __pmEncodeResult(PDU_OVERRIDE2, elm->res, &pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmProgname, pmErrStr(sts));
	    abandon();
	}

        /* switch volumes if required */
        if (varg > 0) {
            if (written > 0 && (written % varg) == 0) {
                newvolume(outarchname, (__pmTimeval *)&pb[3]);
	    }
        }
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
	old_log_offset = ftell(logctl.l_mfp);
	assert(old_log_offset >= 0);
	if ((sts = __pmLogPutResult2(&logctl, pb)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		    pmProgname, pmErrStr(sts));
	    abandon();
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
	if (needti && tvcmp(titime, restime) >= 0)
	    needti = 0;

	/* flush/update */
	if (needti) {
	    titime = restime;

	    fflush(logctl.l_mfp);
	    fflush(logctl.l_mdfp);

	    if (old_log_offset == 0)
		old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

            new_log_offset = ftell(logctl.l_mfp);
	    assert(new_log_offset >= 0);
            new_meta_offset = ftell(logctl.l_mdfp);
	    assert(new_meta_offset >= 0);

            fseek(logctl.l_mfp, (long)old_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);

            __pmLogPutIndex(&logctl, &restime);

            fseek(logctl.l_mfp, (long)new_log_offset, SEEK_SET);
            fseek(logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);

            old_log_offset = ftell(logctl.l_mfp);
	    assert(old_log_offset >= 0);
            old_meta_offset = ftell(logctl.l_mdfp);
	    assert(old_meta_offset >= 0);

            flushsize = ftell(logctl.l_mfp) + 100000;
        }

	/* free PDU buffer */
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
	abandon();
    }

    if (p == NULL) {
	fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	fprintf(stderr, "    writemark called, but no pdu\n");
	abandon();
    }

    p->timestamp.tv_sec = htonl(p->timestamp.tv_sec);
    p->timestamp.tv_usec = htonl(p->timestamp.tv_usec);

    if ((sts = __pmLogPutResult2(&logctl, iap->pb[LOG])) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		pmProgname, pmErrStr(sts));
	abandon();
    }
    written++;
    free(iap->pb[LOG]);
    iap->pb[LOG] = NULL;
}

/*--- END FUNCTIONS ---------------------------------------------------------*/

int
main(int argc, char **argv)
{
    int		i;
    int		j;
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta;		/* sts from nextmeta() */

    char	*msg;

    __pmTimeval 	now = {0,0};	/* the current time */
    __pmTimeval 	mintime = {0,0};
    __pmTimeval 	tmptime = {0,0};

    __pmTimeval		tstamp;		/* temporary timestamp */
    inarch_t		*iap;		/* ptr to archive control */
    rlist_t		*rlready;	/* list of results ready for writing */
    struct timeval	unused;


    rlog = NULL;	/* list of log records to write */
    rdesc = NULL;	/* list of meta desc records to write */
    rindom = NULL;	/* list of meta indom records to write */
    rlready = NULL;


    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	pmUsageMessage(&opts);
	exit(1);
    }


    /* input  archive names are argv[opts.optind] ... argv[argc-2]) */
    /* output archive name  is  argv[argc-1]) */

    /* output archive */
    outarchname = argv[argc-1];

    /* input archive(s) */
    inarchnum = argc - 1 - opts.optind;
    inarch = (inarch_t *) malloc(inarchnum * sizeof(inarch_t));
    if (inarch == NULL) {
	fprintf(stderr, "%s: Error: mallco inarch: %s\n",
		pmProgname, osstrerror());
	exit(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        totalmalloc += (inarchnum * sizeof(inarch_t));
        fprintf(stderr, "main        : allocated %d\n",
			(int)(inarchnum * sizeof(inarch_t)));
    }
#endif


    for (i=0; i<inarchnum; i++, opts.optind++) {
	iap = &inarch[i];

	iap->name = argv[opts.optind];

	iap->pb[LOG] = iap->pb[META] = NULL;
	iap->eof[LOG] = iap->eof[META] = 0;
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
	    if (desperate) {
		unused.tv_sec = INT_MAX;
		unused.tv_usec = 0;
	    }
	    else
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

    /* process config file
     *	- this includes a list of metrics and their instances
     */
    if (configfile && parseconfig() < 0)
	exit(1);

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
	abandon();
    }
    winstart.tv_sec = winstart_tval.tv_sec;
    winstart.tv_usec = winstart_tval.tv_usec;
    winend.tv_sec = winend_tval.tv_sec;
    winend.tv_usec = winend_tval.tv_usec;
    logend.tv_sec = logend_tval.tv_sec;
    logend.tv_usec = logend_tval.tv_usec;

    if (warg) {
	if (winstart.tv_sec + NUM_SEC_PER_DAY < winend.tv_sec) {
	    fprintf(stderr, "%s: Warning: -S and -T must specify a time window within\nthe same day, for -w to be used.  Ignoring -w flag.\n", pmProgname);
	    warg = 0;
	}
    }

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
	old_meta_offset = ftell(logctl.l_mdfp);
	assert(old_meta_offset >= 0);

	/* nextlog() resets ilog, and curlog (to the smallest timestamp)
	 */
	stslog = nextlog();

	if (stslog < 0)
	    break;

	/* find the _Nresult (or mark pdu) with the earliest timestamp;
	 * set ilog
	 * (this is a bit more complex when tflag is specified)
	 */
	mintime.tv_sec = mintime.tv_usec = 0;
	for (i=0; i<inarchnum; i++) {
	    if (inarch[i]._Nresult != NULL) {
		tstamp.tv_sec = inarch[i]._Nresult->timestamp.tv_sec;
		tstamp.tv_usec = inarch[i]._Nresult->timestamp.tv_usec;
		checklogtime(&tstamp, i);

		if (ilog == i) {
		    tmptime = curlog;
		    if (mintime.tv_sec <= 0 || tvcmp(mintime, tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	    else if (inarch[i].pb[LOG] != NULL) {
		tstamp.tv_sec = inarch[i].pb[LOG][3]; /* no swab needed */
		tstamp.tv_usec = inarch[i].pb[LOG][4]; /* no swab needed */
		checklogtime(&tstamp, i);

		if (ilog == i) {
		    tmptime = curlog;
		    if (mintime.tv_sec <= 0 || tvcmp(mintime, tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	}

	/* now     == the earliest timestamp of the archive(s)
	 *		and/or mark records
	 * mintime == now or timestamp of the earliest mark
	 *		(whichever is smaller)
	 */
	now = curlog;

	/* note - mark (after last archive) will be created, but this
	 * break, will prevent it from being written out
	 */
	if (tvcmp(now, logend) > 0)
	    break;

	sts = checkwinend(now);
	if (sts < 0)
	    break;
	if (sts > 0)
	    continue;

	current = curlog;

	/* prepare to write out log record
	 */
	if (ilog < 0 || ilog >= inarchnum) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmProgname);
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    abandon();
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
		abandon();
	    }
	    insertresult(&rlready, iap->_Nresult);
#if 0
{
    rlist_t	*rp;
    int		i;

    fprintf(stderr, "rlready");
    for (i = 0, rp = rlready; rp != NULL; i++, rp = rp->next) {
	fprintf(stderr, " [%d] t=%d.%06d numpmid=%d", i, (int)rp->res->timestamp.tv_sec, (int)rp->res->timestamp.tv_usec, rp->res->numpmid);
    }
    fprintf(stderr, " now=%d.%06d\n", now.tv_sec, now.tv_usec);
}
#endif

	    writerlist(&rlready, curlog);

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
	    if (iap->_result != NULL) {
		pmFreeResult(iap->_result);
		iap->_result = NULL;
	    }
	    iap->_Nresult = NULL;
	}
    } /*while()*/

    if (first_datarec) {
        fprintf(stderr, "%s: Warning: no qualifying records found.\n",
                pmProgname);
cleanup:
	abandon();
    }
    else {
	/* write the last time stamp */
	fflush(logctl.l_mfp);
	fflush(logctl.l_mdfp);

	if (old_log_offset == 0)
	    old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

	new_log_offset = ftell(logctl.l_mfp);
	assert(new_log_offset >= 0);
	new_meta_offset = ftell(logctl.l_mdfp);
	assert(new_meta_offset >= 0);

#if 0
	fprintf(stderr, "*** last tstamp: \n\tmintime=%d.%06d \n\ttmptime=%d.%06d \n\tlogend=%d.%06d \n\twinend=%d.%06d \n\tcurrent=%d.%06d\n",
	    mintime.tv_sec, mintime.tv_usec, tmptime.tv_sec, tmptime.tv_usec, logend.tv_sec, logend.tv_usec, winend.tv_sec, winend.tv_usec, current.tv_sec, current.tv_usec);
#endif

	fseek(logctl.l_mfp, old_log_offset, SEEK_SET);
	__pmLogPutIndex(&logctl, &current);


	/* need to fix up label with new start-time */
	writelabel_metati(1);
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
        fprintf(stderr, "main        : total allocated %ld\n", totalmalloc);
    }
#endif

    exit(exit_status);
}
