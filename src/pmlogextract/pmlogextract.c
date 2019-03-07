/*
 * pmlogextract - extract desired metrics from PCP archive logs
 *
 * Copyright (c) 2014-2018 Red Hat.
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
#include "libpcp.h"
#include "logger.h"

long totalmalloc;
static pmUnits nullunits;
static int desperate;

pmID pmid_pid;
pmID pmid_seqnum;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    { "config", 1, 'c', "FILE", "file to load configuration from" },
    { "desperate", 0, 'd', 0, "desperate, save output after fatal error" },
    { "first", 0, 'f', 0, "use timezone from first archive [default is last]" },
    { "mark", 0, 'm', 0, "ignore prologue/epilogue records and <mark> between archives" },
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
    .short_options = "c:D:dfmS:s:T:v:wZ:z?",
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

    if (num_a_eq == 0 && num_b_eq == 0)
	sts = MATCH_NONE;
    else if (num_a && num_a_eq == num_a && num_b  && num_b_eq == num_b) {
	/* all names match but more occurrences in one log (i.e. dups) */
	sts = MATCH_EQUAL;
    }
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
    if (pmDebugOptions.appl1) {
	fprintf(stderr, "matchnames: ");
	printmetricnames(stderr, a);
	fprintf(stderr, " : ");
	printmetricnames(stderr, b);
	fprintf(stderr, " num_a=%d num_b=%d num_a_eq=%d num_b_eq=%d -> %d\n", num_a, num_b, num_a_eq, num_b_eq, sts);
    }

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
    pmTimeval		timestamp;	/* when returned */
    int			numpmid;	/* zero PMIDs to follow */
} mark_t;


/*
 *  Global variables
 */
static int	exit_status;
static int	inarchvers = PM_LOG_VERS02;	/* version of input archive */
static int	outarchvers = PM_LOG_VERS02;	/* version of output archive */
static int	first_datarec = 1;		/* first record flag */
static int	pre_startwin = 1;		/* outside time win flag */
static int	written;			/* num log writes so far */
int		ml_numpmid;			/* num pmid in ml list */
int		ml_size;			/* actual size of ml array */
mlist_t		*ml;				/* list of pmids with indoms */
rlist_t		*rl;				/* list of pmResults */


off_t		new_log_offset;			/* new log offset */
off_t		new_meta_offset;		/* new meta offset */
off_t		old_log_offset;			/* old log offset */
off_t		old_meta_offset;		/* old meta offset */
static off_t	flushsize = 100000;		/* bytes before flush */


/* archive control stuff */
char			*outarchname;	/* name of output archive */
static __pmLogCtl	logctl;		/* output log control */
static __pmArchCtl	archctl;	/* output archive control */
inarch_t		*inarch;	/* input archive control(s) */
int			inarchnum;	/* number of input archives */

int			ilog;		/* index of earliest log */

static __pmHashCtl	rdesc;		/* meta desc records to be written */
static __pmHashCtl	rindom;		/* meta indom records to be written */
static __pmHashCtl	rindomoneline;	/* indom oneline records to be written */
static __pmHashCtl	rindomtext;	/* indom text records to be written */
static __pmHashCtl	rpmidoneline;	/* pmid oneline records to be written */
static __pmHashCtl	rpmidtext;	/* pmid text records to be written */
static __pmHashCtl	rlabelset;	/* label sets to be written */

static pmTimeval	curlog;		/* most recent timestamp in log */
static pmTimeval	current;	/* most recent timestamp overall */

/* time window stuff */
static struct timeval logstart_tval;	/* extracted log start */
static struct timeval logend_tval;	/* extracted log end */
static struct timeval winstart_tval;	/* window start tval*/
static struct timeval winend_tval;	/* window end tval*/

static pmTimeval 	winstart = {-1,0};	/* window start time */
static pmTimeval	winend = {-1,0};	/* window end time */
static pmTimeval	logend = {-1,0};	/* log end time */

/* command line args */
char	*configfile;			/* -c arg - name of config file */
int	farg;				/* -f arg - use first timezone */
int	old_mark_logic;			/* -m arg - <mark> b/n archives */
int	sarg = -1;			/* -s arg - finish after X samples */
char	*Sarg;				/* -S arg - window start */
char	*Targ;				/* -T arg - window end */
int	varg = -1;			/* -v arg - switch log vol every X */
int	warg;				/* -w arg - ignore day/month/year */
int	zarg;				/* -z arg - use archive timezone */
char	*tz;				/* -Z arg - use timezone from user */

/* cmd line args that could exist, but don't (needed for pmParseTimeWin) */
char	*Aarg;				/* -A arg - non-existent */
char	*Oarg;				/* -O arg - non-existent */

/*--- START FUNCTIONS -------------------------------------------------------*/

/*
 * return negative (lt), zero (eq) or positive (gt) as the pmTimeval's compare
 * a < b, a == b or a > b
 */
static int
tvcmp(pmTimeval *a, pmTimeval *b)
{
    if (a->tv_sec != b->tv_sec)
	return a->tv_sec - b->tv_sec;
    return a->tv_usec - b->tv_usec;
}

void
abandon_extract(void)
{
    char    fname[MAXNAMELEN];
    if (desperate == 0) {
	fprintf(stderr, "Archive \"%s\" not created.\n", outarchname);
	while (archctl.ac_curvol >= 0) {
	    pmsprintf(fname, sizeof(fname), "%s.%d", outarchname, archctl.ac_curvol);
	    unlink(fname);
	    archctl.ac_curvol--;
	}
	pmsprintf(fname, sizeof(fname), "%s.meta", outarchname);
	unlink(fname);
	pmsprintf(fname, sizeof(fname), "%s.index", outarchname);
	unlink(fname);
    }
    exit(1);
}


/*
 *  report that archive is corrupted
 */
static void
_report(__pmFILE *fp)
{
    off_t	here;
    struct stat	sbuf;

    here = __pmLseek(fp, 0L, SEEK_CUR);
    fprintf(stderr, "%s: Error occurred at byte offset %ld into a file of",
	    pmGetProgname(), (long int)here);
    if (__pmFstat(fp, &sbuf) < 0)
	fprintf(stderr, ": stat: %s\n", osstrerror());
    else
	fprintf(stderr, " %ld bytes.\n", (long int)sbuf.st_size);
    fprintf(stderr, "The last record, and the remainder of this file will not be extracted.\n");
}


/*
 *  switch output volumes
 */
static void
newvolume(char *base, pmTimeval *tvp)
{
    __pmFILE		*newfp;
    int			nextvol = archctl.ac_curvol + 1;

    if ((newfp = __pmLogNewFile(base, nextvol)) != NULL) {
	struct timeval	stamp;
	__pmFclose(archctl.ac_mfp);
	archctl.ac_mfp = newfp;
	logctl.l_label.ill_vol = archctl.ac_curvol = nextvol;
	__pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
	__pmFflush(archctl.ac_mfp);
	stamp.tv_sec = ntohl(tvp->tv_sec);
	stamp.tv_usec = ntohl(tvp->tv_usec);
	fprintf(stderr, "%s: New log volume %d, at ", pmGetProgname(), nextvol);
	pmPrintStamp(stderr, &stamp);
	fputc('\n', stderr);
    }
    else {
	fprintf(stderr, "%s: Error: volume %d: %s\n",
		pmGetProgname(), nextvol, pmErrStr(-oserror()));
	abandon_extract();
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
    int		indx;
    inarch_t	*iap;
    __pmLogLabel	*lp = &logctl.l_label;

    /* set outarch to inarch[0] to start off with */
    iap = &inarch[0];

    /* check version number */
    inarchvers = iap->label.ll_magic & 0xff;
    outarchvers = inarchvers;

    if (inarchvers != PM_LOG_VERS02) {
	fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		pmGetProgname(), inarchvers, iap->name);
	abandon_extract();
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
    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];

	/* Ensure all archives of the same version number */
        if ((iap->label.ll_magic & 0xff) != inarchvers) {
	    fprintf(stderr, 
		"%s: Error: input archives with different version numbers\n"
		"archive: %s version: %d\n"
		"archive: %s version: %d\n",
		    pmGetProgname(), inarch[0].name, inarchvers,
		    iap->name, (iap->label.ll_magic & 0xff));
	    abandon_extract();
        }

	/* Ensure all archives of the same host */
	if (strcmp(lp->ill_hostname, iap->label.ll_hostname) != 0) {
	    fprintf(stderr,"%s: Error: host name mismatch for input archives\n",
		    pmGetProgname());
	    fprintf(stderr, "archive: %s host: %s\n",
		    inarch[0].name, inarch[0].label.ll_hostname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    iap->name, iap->label.ll_hostname);
	    abandon_extract();
	}

	/* Ensure all archives of the same timezone */
	if (strcmp(lp->ill_tz, iap->label.ll_tz) != 0) {
	    fprintf(stderr,
		"%s: Warning: timezone mismatch for input archives\n",
		    pmGetProgname());
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
    } /*for(indx)*/
}


/*
 *  
 */
void
writelabel_metati(int do_rewind)
{
    if (do_rewind) __pmRewind(logctl.l_tifp);
    logctl.l_label.ill_vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.l_tifp, &logctl.l_label);

    if (do_rewind) __pmRewind(logctl.l_mdfp);
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
    __pmLogWriteLabel(archctl.ac_mfp, &logctl.l_label);
}


/* --- Start of reclist functions --- */

static void
init_reclist_t(reclist_t *rec)
{
    memset(rec, 0, sizeof(*rec));
    rec->desc.pmid = PM_ID_NULL;
    rec->desc.type = PM_TYPE_NOSUPPORT;
    rec->desc.indom = PM_IN_NULL;
    rec->desc.units = nullunits;	/* struct assignment */
    rec->written = NOT_WRITTEN;
}

/*
 *  make a reclist_t record
 */
static reclist_t *
mk_reclist_t(void)
{
    reclist_t	*rec;

    if ((rec = (reclist_t *)malloc(sizeof(reclist_t))) == NULL) {
	fprintf(stderr, "%s: Error: cannot malloc space for record list.\n",
		pmGetProgname());
	abandon_extract();
    }
    if (pmDebugOptions.appl0) {
        totalmalloc += sizeof(reclist_t);
        fprintf(stderr, "mk_reclist_t: allocated %d\n", (int)sizeof(reclist_t));
    }
    init_reclist_t(rec);
    return rec;
}

/*
 *  append to embedded array of reclist_t records in a given reclist
 */
static reclist_t *
add_reclist_t(reclist_t *rec)
{
    reclist_t	*rp;
    size_t	bytes;

    bytes = (rec->nrecs + 1) * sizeof(reclist_t);
    if (rec->nrecs == 0)
	bytes += sizeof(reclist_t);

    if ((rp = (reclist_t *)realloc(rec->recs, bytes)) == NULL) {
	fprintf(stderr, "%s: Error: cannot realloc space for record list.\n",
		pmGetProgname());
	abandon_extract();
    }
    if (pmDebugOptions.appl0) {
	totalmalloc += sizeof(reclist_t);
	fprintf(stderr, "add_reclist_t: allocated %d\n",
			(int)(rec->nrecs ? sizeof(reclist_t) : 2*sizeof(reclist_t)));
    }
    if (!rec->nrecs)
	rp[rec->nrecs++] = *rec;
    init_reclist_t(&rp[rec->nrecs]);
    rec->sorted = 0;
    return rp;
}

/*
 * find indom in indomreclist - if it isn't in the list then add it in
 * with no pdu buffer
 */
static void
findnadd_indomreclist(int indom)
{
    reclist_t		*curr;
    __pmHashNode	*hp;

    if ((hp = __pmHashSearch(indom, &rindom)) != NULL) {
	return;
    }

    curr = mk_reclist_t();
    curr->desc.indom = indom;

    if (__pmHashAdd(indom, (void *)curr, &rindom) < 0) {
	fprintf(stderr, "%s: Error: cannot add to indom hash table.\n",
		pmGetProgname());
	abandon_extract();
    }
}

/*
 *  append a new record to the desc meta record list if not seen
 *  before, else check the desc meta record is semantically the
 *  same as the last desc meta record for this pmid from this source
 */
void
update_descreclist(int indx)
{
    inarch_t		*iap;
    reclist_t		*curr;
    __pmHashNode	*hp;
    pmUnits		pmu;
    pmUnits		*pmup;
    pmID		pmid;

    iap = &inarch[indx];
    pmid = ntoh_pmID(iap->pb[META][2]);

    /* check every metric name associated with other PMIDs */
    if (pmDebugOptions.appl1) {
	fprintf(stderr, "update_descreclist: looking for ");
	printmetricnames(stderr, iap->pb[META]);
	fprintf(stderr, " (pmid:%s)\n", pmIDStr(pmid));
    }
    for (hp = __pmHashWalk(&rdesc, PM_HASH_WALK_START);
	 hp != NULL;
	 hp = __pmHashWalk(&rdesc, PM_HASH_WALK_NEXT)) {
	if (hp->key == pmid)
	    continue;
	curr = (reclist_t *)hp->data;
	if (curr->pdu != NULL) {
	    if (matchnames(curr->pdu, iap->pb[META]) != MATCH_NONE) {
		fprintf(stderr, "%s: Error: metric ", pmGetProgname());
		printmetricnames(stderr, curr->pdu);
		fprintf(stderr, ": PMID changed from %s", pmIDStr(curr->desc.pmid));
		fprintf(stderr, " to %s!\n", pmIDStr(pmid));
		abandon_extract();
	    }
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "update_descreclist: nomatch ");
		printmetricnames(stderr, curr->pdu);
		fprintf(stderr, " (pmid:%s)\n", pmIDStr(curr->desc.pmid));
	    }
	}
    }

    if ((hp = __pmHashSearch(pmid, &rdesc)) == NULL) {
	curr = mk_reclist_t();

	/* append new record */
	curr->pdu = iap->pb[META];
	curr->desc.pmid = pmid;
	curr->desc.type = ntohl(iap->pb[META][3]);
	curr->desc.indom = ntoh_pmInDom(iap->pb[META][4]);
	curr->desc.sem = ntohl(iap->pb[META][5]);
	pmup = (pmUnits *)&iap->pb[META][6];
	curr->desc.units = ntoh_pmUnits(*pmup);
	findnadd_indomreclist(curr->desc.indom);

	if (__pmHashAdd(pmid, (void *)curr, &rdesc) < 0) {
	    fprintf(stderr, "%s: Error: cannot add to desc hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	}
    } else {
	curr = (reclist_t *)hp->data;

	if (pmDebugOptions.appl1) {
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
	if (matchnames(curr->pdu, iap->pb[META]) != MATCH_EQUAL) {
	    fprintf(stderr, "%s: Error: metric PMID %s", pmGetProgname(), pmIDStr(curr->desc.pmid));
	    fprintf(stderr, ": name changed from ");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, " to ");
	    printmetricnames(stderr, iap->pb[META]);
	    fprintf(stderr, "!\n");
	    abandon_extract();
	}
	if (curr->desc.type != ntohl(iap->pb[META][3])) {
	    fprintf(stderr, "%s: Error: metric ", pmGetProgname());
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": type changed from");
	    fprintf(stderr, " %s", pmTypeStr(curr->desc.type));
	    fprintf(stderr, " to %s!\n", pmTypeStr(ntohl(iap->pb[META][3])));
	    abandon_extract();
	}
	if (curr->desc.indom != ntoh_pmInDom(iap->pb[META][4])) {
	    fprintf(stderr, "%s: Error: metric ", pmGetProgname());
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": indom changed from");
	    fprintf(stderr, " %s", pmInDomStr(curr->desc.indom));
	    fprintf(stderr, " to %s!\n", pmInDomStr(ntoh_pmInDom(iap->pb[META][4])));
	    abandon_extract();
	}
	if (curr->desc.sem != ntohl(iap->pb[META][5])) {
	    fprintf(stderr, "%s: Error: metric ", pmGetProgname());
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": semantics changed from");
	    fprintf(stderr, " ");
	    printsem(stderr, curr->desc.sem);
	    fprintf(stderr, " to ");
	    printsem(stderr, (int)ntohl(iap->pb[META][5]));
	    fprintf(stderr, "!\n");
	    abandon_extract();
	}
	pmup = (pmUnits *)&iap->pb[META][6];
	pmu = ntoh_pmUnits(*pmup);
	if (curr->desc.units.dimSpace != pmu.dimSpace ||
	    curr->desc.units.dimTime != pmu.dimTime ||
	    curr->desc.units.dimCount != pmu.dimCount ||
	    curr->desc.units.scaleSpace != pmu.scaleSpace ||
	    curr->desc.units.scaleTime != pmu.scaleTime ||
	    curr->desc.units.scaleCount != pmu.scaleCount) {
	    fprintf(stderr, "%s: Error: metric ", pmGetProgname());
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": units changed from");
	    fprintf(stderr, " %s", pmUnitsStr(&curr->desc.units));
	    fprintf(stderr, " to %s!\n", pmUnitsStr(&pmu));
	    abandon_extract();
	}
	/* not adding, so META: discard new record */
	free(iap->pb[META]);
    }

    iap->pb[META] = NULL;
}

/*
 *  append a new record to the indom meta record list
 */
void
append_indomreclist(int indx)
{
    inarch_t		*iap;
    reclist_t		*curr;
    reclist_t		*rec;
    __pmHashNode	*hp;
    __pmPDU		*pdu;
    int			indom;

    iap = &inarch[indx];
    pdu = iap->pb[META];
    indom = ntoh_pmInDom(pdu[4]);

    if ((hp = __pmHashSearch(indom, &rindom)) == NULL) {
	/* append new record */
	curr = mk_reclist_t();
	curr->pdu = pdu;
	curr->stamp.tv_sec = ntohl(pdu[2]);
	curr->stamp.tv_usec = ntohl(pdu[3]);
	curr->desc.indom = indom;

	if (__pmHashAdd(indom, (void *)curr, &rindom) < 0) {
	    fprintf(stderr, "%s: Error: cannot add to indom hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	}
    } else {
	curr = (reclist_t *)hp->data;

	if (curr->pdu == NULL) {
	    /* insert new record */
	    curr->pdu = iap->pb[META];
	    curr->stamp.tv_sec = ntohl(curr->pdu[2]);
	    curr->stamp.tv_usec = ntohl(curr->pdu[3]);
	}
	else {
	    /* do NOT discard old record; append new record */
	    curr->recs = add_reclist_t(curr);
	    rec = &curr->recs[curr->nrecs];
	    rec->pdu = pdu;
	    rec->stamp.tv_sec = ntohl(pdu[2]);
	    rec->stamp.tv_usec = ntohl(pdu[3]);
	    rec->desc.indom = indom;
	    curr->nrecs++;
	}
    }

    iap->pb[META] = NULL;
}

/*
 * Append a new record to the label set meta record hash
 */
void
append_labelsetreclist(int i)
{
    inarch_t		*iap;
    __pmHashNode	*hp;
    __pmHashCtl		*hash2;
    reclist_t		*rec;
    int			sts;
    int			type;
    int			id;

    iap = &inarch[i];

    /* Initialize the new record. */
    rec = mk_reclist_t();
    rec->pdu = iap->pb[META];
    rec->stamp.tv_sec = ntohl(rec->pdu[2]);
    rec->stamp.tv_usec = ntohl(rec->pdu[3]);

    /*
     * Label sets are stored in a 2 level hash table. First hashed by type.
     */
    type = ntoh_pmLabelType(rec->pdu[4]);
    if ((hp = __pmHashSearch(type, &rlabelset)) == NULL) {
	/* This label type was not found. Create a hash table for it. */
	if ((hash2 = (__pmHashCtl *) malloc(sizeof(*hash2))) == NULL) {
	    fprintf(stderr, "%s: Error: cannot malloc space for hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	}
	__pmHashInit(hash2);

	sts = __pmHashAdd(type, (void *)hash2, &rlabelset);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: cannot add secondary hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	}
    }
    else
	hash2 = (__pmHashCtl *)hp->data;

    /*
     * Add the new label set record, even if one with the same type and id
     * already exists.
     */
    id = ntoh_pmID(iap->pb[META][5]);
    sts = __pmHashAdd(id, (void *)rec, hash2);
    if (sts < 0) {
	fprintf(stderr, "%s: Error: cannot add label set record.\n",
		pmGetProgname());
	abandon_extract();
    }

    iap->pb[META] = NULL;
}

/*
 * Search for text records of the given type associated with the
 * given identifier.  There are two classes of text records -
 * PM_TEXT_ONELINE / PM_TEXT_HELP, and two classes of identifier
 * - PM_TEXT_INDOM / PM_TEXT_PMID; so, four hash tables in all.
 */
static reclist_t *
text_lookup(int type, int ident)
{
    __pmHashNode	*hp = NULL;

    if ((type & PM_TEXT_PMID)) {
	if ((type & PM_TEXT_ONELINE))
	    hp = __pmHashSearch(ident, &rpmidoneline);
	else if ((type & PM_TEXT_HELP))
	    hp = __pmHashSearch(ident, &rpmidtext);
    } else if ((type & PM_TEXT_INDOM)) {
	if ((type & PM_TEXT_ONELINE))
	    hp = __pmHashSearch(ident, &rindomoneline);
	else if ((type & PM_TEXT_HELP))
	    hp = __pmHashSearch(ident, &rindomtext);
    }
    if (hp == NULL)
	return NULL;
    return (reclist_t *)hp->data;
}

/*
 *  append a new record to the text meta record list if not seen
 *  before, else check the text meta record is the
 *  same as the existing text record for this pmid/indom from this source
 */
void
append_textreclist(int i)
{
    inarch_t	*iap;
    reclist_t	*curr;
    int		sts = 0;
    int		type;
    int		ident;
    const char *str1, *str2;

    iap = &inarch[i];
    type = ntoh_pmTextType(iap->pb[META][2]);
    if ((type & PM_TEXT_PMID))
	ident = ntoh_pmID(iap->pb[META][3]);
    else /* (type & PM_TEXT_INDOM) */
	ident = ntoh_pmInDom(iap->pb[META][3]);

    if (pmDebugOptions.appl1) {
	fprintf(stderr, "update_textreclist: looking for ");
	if ((type & PM_TEXT_PMID))
	    fprintf(stderr, "(pmid:%s)", pmIDStr(ident));
	else /* (type & PM_TEXT_INDOM) */
	    fprintf(stderr, "(pmid:%s)", pmInDomStr(ident));
	fprintf(stderr, "(type:%s)\n",
		(type & PM_TEXT_ONELINE) ? "oneline" : "help");
    }

    /*
     * Find matching record, if any. We want the record with the same
     * target (pmid vs indom and class (one line vs help).
     */
    curr = text_lookup(type, ident);

    /* Did we find an existing record? */
    if (curr != NULL) {
	/* We did. Check whether the text is still the same */
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "update_textreclist: ");
	    if ((type & PM_TEXT_PMID)) {
		fprintf(stderr, "pmid match ");
		printmetricnames(stderr, curr->pdu);
		fputc('\n', stderr);
	    }
	    else if ((type & PM_TEXT_INDOM)) {
		fprintf(stderr, "indom match %s\n", pmInDomStr(curr->desc.indom));
	    }
	}
	str1 = (const char *)&curr->pdu[4];
	str2 = (const char *)&iap->pb[META][4];
	if (strcmp(str1, str2) != 0) {
	    fprintf(stderr, "%s: Warning: ", pmGetProgname());
	    if ((type & PM_TEXT_PMID))
		fprintf(stderr, "metric PMID %s", pmIDStr(curr->desc.pmid));
	    else if ((type & PM_TEXT_INDOM))
		fprintf(stderr, "instance domain %s",pmInDomStr(curr->desc.indom));
	    if ((type & PM_TEXT_ONELINE)) {
		fprintf(stderr, " one line text changed from\n");
		fprintf(stderr, "  \"%s\" to\n", str1);
		fprintf(stderr, "  \"%s\"!\n", str2);
	    }
	    else if ((type & PM_TEXT_HELP)) {
		/*
		 * It's not practical to print the entire help text of each as
		 * part of an error message.
		 */
		fprintf(stderr, " help text changed!\n");
	    }
	}
	/*
	 * Tolerate change for the purpose of making
	 * corrections over time. Do this by keeping the latest version and
	 * discarding the original.
	 */
	free(curr->pdu);
    }
    else {
	/* No existing record found. Add a new record to the list. */
	curr = mk_reclist_t();

	/* Populate the new record. */
	curr->desc.type = type;
	if ((type & PM_TEXT_PMID)) {
	    curr->desc.pmid = ident;
	    if ((type & PM_TEXT_ONELINE))
		sts = __pmHashAdd(ident, (void *)curr, &rpmidoneline);
	    else if ((type & PM_TEXT_HELP))
		sts = __pmHashAdd(ident, (void *)curr, &rpmidtext);
	} else if ((type & PM_TEXT_INDOM)) {
	    curr->desc.indom = ident;
	    if ((type & PM_TEXT_ONELINE))
		sts = __pmHashAdd(ident, (void *)curr, &rindomoneline);
	    else if ((type & PM_TEXT_HELP))
		sts = __pmHashAdd(ident, (void *)curr, &rindomtext);
	}
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: cannot add to help text hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	}
    }

    /*
     * Regardless of whether the help text already existed, we're keeping
     * the latest pdu.
     */
    curr->pdu = iap->pb[META];
    iap->pb[META] = NULL;
}

/*
 *  write out one desc/indom/label/text record
 */
void
write_rec(reclist_t *rec)
{
    int		sts;

    if (rec->written == MARK_FOR_WRITE) {
	if (rec->pdu == NULL) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	    fprintf(stderr,"    record is marked for write, but pdu is NULL\n");
	    abandon_extract();
	}

	if (pmDebugOptions.logmeta) {
	    __pmLogHdr	*h;
	    int		len;
	    int		type;
	    h = (__pmLogHdr *)rec->pdu;
	    len = ntohl(h->len);
	    type = ntohl(h->type);
	    fprintf(stderr, "write_rec: record len=%d, type=%d @ offset=%d\n",
	    	len, type, (int)(__pmFtell(logctl.l_mdfp) - sizeof(__pmLogHdr)));
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
		pmPrintDesc(stderr, &desc);
	    }
	    else if (type == TYPE_INDOM) {
		pmTimeval	*tvp;
		pmTimeval	when;
		int		k = 2;
		pmInDom		indom;
		int		numinst;
		int		*instlist;
		int		inst;

		tvp = (pmTimeval *)&rec->pdu[k];
		when.tv_sec = ntohl(tvp->tv_sec);
		when.tv_usec = ntohl(tvp->tv_usec);
		k += sizeof(pmTimeval)/sizeof(rec->pdu[0]);
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
	    else if (type == TYPE_LABEL) {
		pmTimeval	*tvp;
		pmTimeval	when;
		int		k = 2;
		int		type;
		int		ident;
		char		buf[1024];

		tvp = (pmTimeval *)&rec->pdu[k];
		when.tv_sec = ntohl(tvp->tv_sec);
		when.tv_usec = ntohl(tvp->tv_usec);
		k += sizeof(pmTimeval)/sizeof(rec->pdu[0]);
		type = ntoh_pmLabelType((unsigned int)rec->pdu[k++]);
		ident = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "LABELSET: %s when: ",
			__pmLabelIdentString(ident, type, buf, sizeof(buf)));
		__pmPrintTimeval(stderr, &when);
		fputc('\n', stderr);
	    }
	    else if (type == TYPE_TEXT) {
		int		k = 2;
		int		type;
		int		ident;

		type = ntoh_pmTextType((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "TEXT: type: %s ",
			((type & PM_TEXT_ONELINE)) ? "oneline" : "help");
		if ((type & PM_TEXT_PMID)) {
		    ident = ntoh_pmID((unsigned int)rec->pdu[k++]);
		    fprintf(stderr, "TEXT: PMID: %s", pmIDStr(ident));
		}
		else { /* (type & PM_TEXT_PMINDOM) */
		    ident = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		    fprintf(stderr, "TEXT: INDOM: %s", pmInDomStr(ident));
		}
		if ((type & PM_TEXT_DIRECT))
		    fprintf(stderr, " DIRECT");
		fputc('\n', stderr);
	    }
	    else {
		fprintf(stderr, "Botch: bad type\n");
	    }
	}

	/* write out the pdu ; exit if write failed */
	if ((sts = _pmLogPut(logctl.l_mdfp, rec->pdu)) < 0) {
	    fprintf(stderr, "%s: Error: _pmLogPut: meta data : %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	}
	/* META: free PDU buffer */
	free(rec->pdu);
	rec->pdu = NULL;
	rec->written = WRITTEN;
    }
    else {
	fprintf(stderr,
		"%s : Warning: attempting to write out meta record (%d,%d)\n",
		pmGetProgname(), rec->desc.pmid, rec->desc.indom);
	fprintf(stderr, "        when it is not marked for writing (%d)\n",
		rec->written);
    }
}

/*
 * Write out the label set record associated with this indom or pmid
 * at the given time.
 */
static void
write_priorlabelset(int type, int ident, const struct timeval *now)
{
    __pmHashNode	*hp;
    reclist_t		*curr_labelset;	/* current labelset record */
    reclist_t   	*other_labelset;/* other labelset record */

    /* Find the label sets of this type. */
    if ((hp = __pmHashSearch(type, &rlabelset)) == NULL) {
	/* No label sets of this type. That's OK. */
	return;
    }

    switch(type) {
    case PM_LABEL_DOMAIN:
	/* ident is the full pmid, but we only want the domain. */
	ident = pmID_domain(ident);
	break;
    case PM_LABEL_CLUSTER:
	/* ident is the full pmid, but we only want the domain and cluster. */
	ident = pmID_build(pmID_domain(ident), pmID_cluster(ident), 0);
	break;
    default:
	/* We can hash against the ident directly. */
	break;
    }

    if ((hp = __pmHashSearch(ident, (__pmHashCtl *)hp->data)) == NULL) {
	/* No label sets of this type with this ident. That's also OK. */
	return;
    }

    /*
     * There may be more than one record.
     *	- we can safely ignore all labet sets after the current timestamp
     *	- we want the latest label set at, or before the current timestamp
     */
    other_labelset = NULL;
    curr_labelset = (reclist_t *)hp->data;
    while (curr_labelset != NULL) {
	if (curr_labelset->stamp.tv_sec < now->tv_sec ||
	    (curr_labelset->stamp.tv_sec == now->tv_sec &&
	     curr_labelset->stamp.tv_usec <= now->tv_usec)) {
	    /*
	     * labelset is in list, labelset has pdu
	     * and timestamp in pdu suits us
	     */
	    if (other_labelset == NULL ||
		other_labelset->stamp.tv_sec < curr_labelset->stamp.tv_sec ||
		(other_labelset->stamp.tv_sec == curr_labelset->stamp.tv_sec &&
		 other_labelset->stamp.tv_usec <= curr_labelset->stamp.tv_usec)){
		/*
		 * We already have a perfectly good labelset,
		 * but curr_labelset has a better timestamp
		 */
		other_labelset = curr_labelset;
	    }
	}
	curr_labelset = curr_labelset->next;
    }

    /* Write the chosen record, if it has not already been written. */
    if (other_labelset != NULL && other_labelset->pdu != NULL &&
	other_labelset->written != WRITTEN) {
	other_labelset->written = MARK_FOR_WRITE;
	other_labelset->pdu[2] = htonl(now->tv_sec);
	other_labelset->pdu[3] = htonl(now->tv_usec);
	write_rec(other_labelset);
    }
}

/*
 * Write out the text records associated with this indom or pmid.
 */
static void
write_textreclist(int type, int ident)
{
    reclist_t		*curr = text_lookup(type, ident);

    if (curr && curr->pdu && curr->written != WRITTEN) {
	curr->written = MARK_FOR_WRITE;
	write_rec(curr);
    }
}

static int
indom_compare(const void *a, const void *b)
{
    reclist_t		*ar = (reclist_t *)a;
    reclist_t		*br = (reclist_t *)b;

    return tvcmp(&ar->stamp, &br->stamp);
}

static reclist_t *
indom_lookup(int indom)
{
    __pmHashNode	*hp;
    reclist_t		*ip;

    if ((hp = __pmHashSearch(indom, &rindom)) == NULL)
	return NULL;
    ip = (reclist_t *)hp->data;
    if (ip->nrecs > 0 && ip->sorted == 0) {
	qsort(ip->recs, ip->nrecs, sizeof(reclist_t), indom_compare);
	ip->sorted = 1;
    }
    return ip;
}

/*
 * binary search through the indom array to find indom with the
 * closest previous timestamp to the one from the current result.
 */
static reclist_t *
indom_closest(reclist_t *recs, struct timeval *stamp)
{
    unsigned int	first, last, count, middle, previous;
    reclist_t		*indom, *array = &recs->recs[0];
    pmTimeval		tv = { stamp->tv_sec, stamp->tv_usec };
    int			sts = -1;

    first = 0;
    count = last = recs->nrecs - 1;
    middle = previous = (first + last) / 2;

    while (first <= last) {
	indom = &array[middle];
	sts = tvcmp(&indom->stamp, &tv);
	if (sts == 0)
	    return indom;
	previous = middle;
	if (sts < 0) {	/* right */
	    if (middle == count)
		break;
	    first = middle + 1;
	}
	else {		/* left */
	    if (middle == 0)
		break;
	    last = middle - 1;
	}
	middle = (first + last) / 2;
    }
    indom = &array[middle];
    if (previous != middle)	/* avoid unnecessary comparison */
	sts = tvcmp(&indom->stamp, &tv);
    if (sts < 0)
	return indom;
    return NULL;
}

void
write_metareclist(pmResult *result, int *needti)
{
    int			n, count;
    reclist_t		*curr_desc;	/* current desc record */
    reclist_t		*curr_indom;	/* current indom record */
    reclist_t   	*other_indom;	/* other indom record */
    pmID		pmid;
    pmInDom		indom;
    __pmHashNode	*hp;
    struct timeval	*stamp;		/* ptr to timestamp in result */

    stamp = &result->timestamp;

    /* if pmid in result matches a pmid in desc then write desc */
    for (n = 0; n < result->numpmid; n++) {
	pmid = result->vset[n]->pmid;
	indom = PM_IN_NULL;
	count = 0;
	curr_indom = NULL;

	if ((hp = __pmHashSearch(pmid, &rdesc)) == NULL) {
	    /* descriptor has not been found - this is bad */
	    fprintf(stderr, "%s: Error: meta data (TYPE_DESC) for pmid %s has not been found.\n", pmGetProgname(), pmIDStr(pmid));
	    abandon_extract();
	} else {
	    curr_desc = (reclist_t *)hp->data;
	    /* descriptor has been found */
	    if (curr_desc->written == WRITTEN) {
		/*
		 * descriptor has been written before (no need to write again)
		 * but still need to check indom and help text.
		 */
		indom = curr_desc->desc.indom;
	    }
	    else if (curr_desc->pdu == NULL) {
		/*
		 * descriptor is in list, has not been written, but no pdu
		 *  - this is bad
		 */
		fprintf(stderr, "%s: Error: missing pdu for pmid %s\n",
			pmGetProgname(), pmIDStr(pmid));
	        abandon_extract();
	    }
	    else {
		/*
		 * descriptor is in list, has not been written, and has pdu
		 * write!
		 */
		curr_desc->written = MARK_FOR_WRITE;
		write_rec(curr_desc);
		indom = curr_desc->desc.indom;
	    }
	}

	/* Write out the label set records associated with this pmid. */
	write_priorlabelset(PM_LABEL_ITEM, pmid, stamp);
	write_priorlabelset(PM_LABEL_DOMAIN, pmid, stamp);
	write_priorlabelset(PM_LABEL_CLUSTER, pmid, stamp);

	/*
	 * Write out any help text records associated with this pmid.
	 */
	write_textreclist(PM_TEXT_PMID | PM_TEXT_ONELINE, pmid);
	write_textreclist(PM_TEXT_PMID | PM_TEXT_HELP, pmid);

	/*
	 * descriptor has been found and written,
	 * now go and find & write the indom
	 */
	if (indom != PM_INDOM_NULL) {
	    /*
	     * there may be more than one indom in the list, so we need
	     * to traverse the entire list
	     *	- we can safely ignore all indoms after the current timestamp
	     *	- we want the latest indom at, or before the current timestamp
	     */
	    curr_indom = indom_lookup(indom);
	    assert(curr_indom->desc.indom == indom);

	    if ((count = curr_indom->nrecs) == 0)
		other_indom = curr_indom;
	    else {
		assert(curr_indom->sorted == 1);
		other_indom = indom_closest(curr_indom, stamp);
	    }

	    if (other_indom != NULL && other_indom->written != WRITTEN) {
		/*
		 * There may be indoms which are referenced in desc records
		 * which have no pdus. This is because the corresponding indom
		 * record does not exist. There's no record to write, but we
		 * still need to output the associated labels and help text.
		 */
		if (other_indom->pdu != NULL) { 
		    other_indom->written = MARK_FOR_WRITE;
		    other_indom->pdu[2] = htonl(stamp->tv_sec);
		    other_indom->pdu[3] = htonl(stamp->tv_usec);

		    /* make sure to set needti, when writing out the indom */
		    *needti = 1;
		    write_rec(other_indom);
		}

		assert(other_indom->desc.indom == indom);

		/* Write out the label set records associated with this indom */
		write_priorlabelset(PM_LABEL_INDOM, indom, stamp);
		write_priorlabelset(PM_LABEL_INSTANCES, indom, stamp);

		/* Write out any help text records associated with this indom */
		write_textreclist(PM_TEXT_INDOM | PM_TEXT_ONELINE, indom);
		write_textreclist(PM_TEXT_INDOM | PM_TEXT_HELP, indom);
	    }
	}
    }
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
		pmGetProgname(), osstrerror());
	abandon_extract();
    }
    if (pmDebugOptions.appl0) {
        totalmalloc += sizeof(mark_t);
        fprintf(stderr, "_createmark : allocated %d\n", (int)sizeof(mark_t));
    }

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
checklogtime(pmTimeval *this, int indx)
{
    if ((curlog.tv_sec == 0 && curlog.tv_usec == 0) ||
	(curlog.tv_sec > this->tv_sec ||
	(curlog.tv_sec == this->tv_sec && curlog.tv_usec > this->tv_usec))) {
	    ilog = indx;
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
    int		indx;
    int		j;
    int		type;
    int		want;
    int		numeof = 0;
    int		sts;
    pmID	pmid;			/* pmid for TYPE_DESC */
    pmInDom	indom;			/* indom for TYPE_INDOM */
    __pmLogCtl	*lcp;
    __pmContext	*ctxp;
    inarch_t	*iap;			/* pointer to input archive control */

    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];

	/* if at the end of meta file then skip this archive */
	if (iap->eof[META]) {
	    ++numeof;
	    continue;
	}

	/* we should never already have a meta record */
	if (iap->pb[META] != NULL) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	    fprintf(stderr, "    iap->pb[META] is not NULL\n");
	    abandon_extract();
	}
	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), iap->ctx);
	    abandon_extract();
	}
	/* Need to hold c_lock for _pmLogGet() */

	lcp = ctxp->c_archctl->ac_log;

againmeta:
	/* get next meta record */

	if ((sts = _pmLogGet(ctxp->c_archctl, PM_LOG_VOL_META, &iap->pb[META])) < 0) {
	    iap->eof[META] = 1;
	    ++numeof;
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: _pmLogGet[meta %s]: %s\n",
			pmGetProgname(), iap->name, pmErrStr(sts));
		_report(lcp->l_mdfp);
		abandon_extract();
	    }
	    PM_UNLOCK(ctxp->c_lock);
	    continue;
	}
	
	type = ntohl(iap->pb[META][1]);

	/*
	 * pmDesc entries, if not seen before & wanted,
	 *	then append to desc list
	 */
	if (type == TYPE_DESC) {
	    pmid = ntoh_pmID(iap->pb[META][2]);

	    /*
	     * if ml is defined, then look for pmid in the list
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
		/*
		 * update the desc list (add first time, check on subsequent
		 * sightings of desc for this pmid from this source
		 * update_descreclist() sets pb[META] to NULL
		 */
		update_descreclist(indx);
	    }
	    else {
		/* not wanted */
		free(iap->pb[META]);
		iap->pb[META] = NULL;
		goto againmeta;
	    }
	}
	else if (type == TYPE_INDOM) {
	    /*
	     * if ml is defined, then look for instance domain in the list
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
		/*
		 * add to indom list 
		 * append_indomreclist() sets pb[META] to NULL
		 * append_indomreclist() may unpin the pdu buffer
		 */
		append_indomreclist(indx);
	    }
	    else {
	        /* META: don't want this meta */
	        free(iap->pb[META]);
	        iap->pb[META] = NULL;
	        goto againmeta;
	    }
	}
	else if (type == TYPE_LABEL) {
	    /* Decide which label sets we want to keep. */
	    want = 0;
	    if (ml == NULL) {
		/* ml is not defined, then all metrics and indoms are being kept.
		   Keep all label sets as well. */
	        want = 1;
	    }
	    else {
		type = ntoh_pmLabelType(iap->pb[META][4]);
		switch (type) {
		case PM_LABEL_CONTEXT:
		    /*
		     * Keep all label sets not associated with a specific metric
		     * or indom. We can assume that any referenced indoms have
		     * already been processed.
		     */
		    want = 1;
		    break;
		case PM_LABEL_DOMAIN:
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][5]);
		    pmid = pmID_domain(pmid); /* Extract the domain */
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == pmID_domain(ml[j].idesc->pmid))
			    want = 1;
		    }
		    break;
		case PM_LABEL_CLUSTER:
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][5]);
		    pmid = pmID_build(pmID_domain(pmid), pmID_cluster(pmid), 0);
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == pmID_build(pmID_domain(ml[j].idesc->pmid),
					       pmID_cluster(ml[j].idesc->pmid), 0))
			    want = 1;
		    }
		    break;
		case PM_LABEL_ITEM:
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][5]);
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == ml[j].idesc->pmid)
			    want = 1;
		    }
		    break;
		case PM_LABEL_INDOM:
		case PM_LABEL_INSTANCES:
		    /*
		     * Keep only the label sets whose instance domains are also being kept.
		     * These are the domains of the metrics which are being kept.
		     */
		    indom = ntoh_pmInDom(iap->pb[META][5]);
		    for (j=0; j<ml_numpmid; j++) {
			if (indom == ml[j].idesc->indom)
			    want = 1;
		    }
		    break;
		default:
		    fprintf(stderr, "%s: Error: invalid label set type: %d\n",
			    pmGetProgname(), type);
		    abandon_extract();
		    break;
		}
	    }

	    if (want) {
		/*
		 * Add to label set list.
		 * append_labelsetreclist() sets pb[META] to NULL
		 */
		append_labelsetreclist(indx);
	    }
	    else {
	        /* META: don't want this meta */
	        free(iap->pb[META]);
	        iap->pb[META] = NULL;
	        goto againmeta;
	    }
	}
	else if (type == TYPE_TEXT) {
	    /* Decide which text we want to keep. */
	    want = 0;
	    if (ml == NULL) {
		/* ml is not defined, then all metrics and indoms are being kept.
		   Keep all text as well. */
	        want = 1;
	    }
	    else {
		type = ntoh_pmTextType(iap->pb[META][2]);
		if ((type & PM_TEXT_PMID)) {
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][3]);
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == ml[j].idesc->pmid)
			    want = 1;
		    }
		}
		else if ((type & PM_TEXT_INDOM)) {
		    /*
		     * Keep only the label sets whose instance domains are also being kept.
		     * These are the domains of the metrics which are being kept.
		     */
		    indom = ntoh_pmInDom(iap->pb[META][3]);
		    for (j=0; j<ml_numpmid; j++) {
			if (indom == ml[j].idesc->indom)
			    want = 1;
		    }
		}
		else {
		    fprintf(stderr, "%s: Error: invalid text type: %d\n",
			    pmGetProgname(), type);
		    abandon_extract();
		    break;
		}
	    }

	    if (want) {
		/*
		 * Add to text list.
		 * append_textreclist() sets pb[META] to NULL
		 */
		append_textreclist(indx);
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
		    pmGetProgname(), type);
	    abandon_extract();
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
    int		indx;
    int		eoflog = 0;	/* number of log files at eof */
    int		sts;
    pmTimeval	curtime;
    __pmArchCtl	*acp;
    __pmContext	*ctxp;
    inarch_t	*iap;


    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];

	/* if at the end of log file then skip this archive */
	if (iap->eof[LOG]) {
	    ++eoflog;
	    continue;
	}

	/* if we already have a log record then skip this archive */
	if (iap->_Nresult != NULL) {
	    continue;
	}

	/* if mark has been written out, then log is at EOF */
	if (iap->mark) {
	    iap->eof[LOG] = 1;
	    ++eoflog;
	    continue;
	}

	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), iap->ctx);
	    abandon_extract();
	}
	/* Need to hold c_lock for __pmLogRead_ctx() */
	acp = ctxp->c_archctl;

againlog:
	if ((sts=__pmLogRead_ctx(ctxp, PM_MODE_FORW, NULL, &iap->_result, PMLOGREAD_NEXT)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
			pmGetProgname(), iap->name, pmErrStr(sts));
		_report(acp->ac_mfp);
		if (sts != PM_ERR_LOGREC)
		    abandon_extract();
	    }
	    /*
	     * if the first data record has not been written out, then
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
	iap->recnum++;
	assert(iap->_result != NULL);

	/*
	 * set current log time - this is only done so that we can
	 * determine whether to keep or discard the log
	 */
	curtime.tv_sec = iap->_result->timestamp.tv_sec;
	curtime.tv_usec = iap->_result->timestamp.tv_usec;

	/*
	 * check for prologue/epilogue records ... 
	 *
	 * Warning: If pmlogger changes the contents of the prologue
	 *          and/or epilogue records, then the 5 below will need
	 *          to be adjusted.
	 *          If the type of pmcd.pid changes from U64 or the type
	 *          of pmcd.seqnum changes from U32, the extraction will
	 *          have to change as well.
	 */
	if (iap->_result->numpmid == 5) {
	    int		i;
	    pmAtomValue	av;
	    int		lsts;
	    for (i=0; i<iap->_result->numpmid; i++) {
		if (iap->_result->vset[i]->pmid == pmid_pid) {
		    lsts = pmExtractValue(iap->_result->vset[i]->valfmt, &iap->_result->vset[i]->vlist[0], PM_TYPE_U64, &av, PM_TYPE_64);
		    if (lsts != 0) {
			fprintf(stderr,
			    "%s: Warning: failed to get pmcd.pid from %s at record %d: %s\n",
				pmGetProgname(), iap->name, iap->recnum, pmErrStr(lsts));
			if (pmDebugOptions.desperate) {
			    PM_UNLOCK(ctxp->c_lock);
			    __pmDumpResult(stderr, iap->_result);
			    PM_LOCK(ctxp->c_lock);
			}
		    }
		    else
			iap->pmcd_pid = av.ll;
		}
		else if (iap->_result->vset[i]->pmid == pmid_seqnum) {
		    lsts = pmExtractValue(iap->_result->vset[i]->valfmt, &iap->_result->vset[i]->vlist[0], PM_TYPE_U32, &av, PM_TYPE_32);
		    if (lsts != 0) {
			fprintf(stderr,
			    "%s: Warning: failed to get pmcd.seqnum from %s at record %d: %s\n",
				pmGetProgname(), iap->name, iap->recnum, pmErrStr(lsts));
			if (pmDebugOptions.desperate) {
			    PM_UNLOCK(ctxp->c_lock);
			    __pmDumpResult(stderr, iap->_result);
			    PM_LOCK(ctxp->c_lock);
			}
		    }
		    else
			iap->pmcd_seqnum = av.l;
		}
	    }
	}

	/*
	 * if log time is greater than (or equal to) the current window
	 * start time, then we may want it
	 *	(irrespective of the current window end time)
	 */
	if (tvcmp(&curtime, &winstart) < 0) {
	    /*
	     * log is not in time window - discard result and get next record
	     */
	    pmFreeResult(iap->_result);
	    iap->_result = NULL;
	    goto againlog;
	}
        else {
            /*
	     * log is within time window - check whether we want this record
             */
            if (iap->_result->numpmid == 0) {
		/* mark record, process this one as is */
                iap->_Nresult = iap->_result;
	    }
            else if (ml == NULL) {
                /* ml is NOT defined, we want everything */
                iap->_Nresult = iap->_result;
            }
            else {
                /*
		 * ml is defined, need to search metric list for wanted pmid's
                 *   (searchmlist may return a NULL pointer - this is fine)
                 */
                iap->_Nresult = searchmlist(iap->_result);
            }

            if (iap->_Nresult == NULL) {
                /* dont want any of the metrics in _result, try again */
		pmFreeResult(iap->_result);
		iap->_result = NULL;
                goto againlog;
            }
	}
	PM_UNLOCK(ctxp->c_lock);

    } /*for(indx)*/

    /*
     * if we are here, then each archive control struct should either
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
		pmprintf("%s: %s - invalid file\n", pmGetProgname(), configfile);
		opts.errors++;
	    }
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(opts.optarg);
	    if (sts < 0) {
		pmprintf("%s: unrecognized debug options specification (%s)\n",
			pmGetProgname(), opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'd':	/* desperate to save output archive, even after error */
	    desperate = 1;
	    break;

	case 'f':	/* use timezone from first archive */
	    farg = 1;
	    break;

	case 'm':	/* always add <mark> between archives */
	    old_mark_logic = 1;
	    break;

	case 's':	/* number of samples to write out */
	    sarg = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || sarg < 0) {
		pmprintf("%s: -s requires numeric argument\n", pmGetProgname());
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
		pmprintf("%s: -v requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    break;

	case 'w':	/* ignore day/month/year */
	    warg++;
	    break;

	case 'Z':	/* use timezone from command line */
	    if (zarg) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmGetProgname());
		opts.errors++;
	    }
	    tz = opts.optarg;
	    break;

	case 'z':	/* use timezone from archive */
	    if (tz != NULL) {
		pmprintf("%s: at most one of -Z and/or -z allowed\n",
			pmGetProgname());
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
	    fprintf(stderr, "%s: Warning: -w flag requires that both -S and -T are specified.\nIgnoring -w flag.\n", pmGetProgname());
	    warg = 0;
	}
    }


    if (opts.errors == 0 && opts.optind > argc - 2) {
	pmprintf("%s: Error: insufficient arguments\n", pmGetProgname());
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
		pmGetProgname(), configfile, osstrerror());
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
checkwinend(pmTimeval now)
{
    int		indx;
    int		sts;
    pmTimeval	tmptime;
    inarch_t	*iap;
    __pmPDU	*markpdu;	/* mark b/n time windows */

    if (winend.tv_sec < 0 || tvcmp(&now, &winend) <= 0)
	return(0);

    /*
     * we have reached the end of a window
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

    /*
     * if start of next window is later than max termination
     * then bail out here
     */
    if (tvcmp(&winstart, &logend) > 0)
	    return(-1);

    ilog = -1;
    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];
	if (iap->_Nresult != NULL) {
	    tmptime.tv_sec = iap->_Nresult->timestamp.tv_sec;
	    tmptime.tv_usec = iap->_Nresult->timestamp.tv_usec;
	    if (tvcmp(&tmptime, &winstart) < 0) {
		/* free _result and _Nresult */
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
	    if (tvcmp(&tmptime, &winstart) < 0) {
		/*
		 * free PDU buffer ... it is probably a mark
		 * and has not been pinned
		 */
		free(iap->pb[LOG]);
		iap->pb[LOG] = NULL;
	    }
	}
    } /*for(indx)*/

    /* must create "mark" record and write it out */
    /* (need only one mark record) */
    markpdu = _createmark();
    if ((sts = __pmLogPutResult2(&archctl, markpdu)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		pmGetProgname(), pmErrStr(sts));
	abandon_extract();
    }
    written++;
    free(markpdu);
    return(1);
}


void
writerlist(rlist_t **rlready, pmTimeval mintime)
{
    int		sts;
    int		needti = 0;	/* need to flush/update */
    pmTimeval	titime  = {0,0};/* time of last temporal index write */
    pmTimeval	restime;	/* time of result */
    struct timeval tstamp;	/* temporary time stamp */
    rlist_t	*elm;		/* element of rlready to be written out */
    __pmPDU	*pb;		/* pdu buffer */
    unsigned long	peek_offset;

    while (*rlready != NULL) {
	restime.tv_sec = (*rlready)->res->timestamp.tv_sec;
	restime.tv_usec = (*rlready)->res->timestamp.tv_usec;

        if (tvcmp(&restime, &mintime) > 0) {
#if 0
fprintf(stderr, "writelist: restime %d.%06d mintime %d.%06d ", restime.tv_sec, restime.tv_usec, mintime.tv_sec, mintime.tv_usec);
fprintf(stderr, " break!\n");
#endif
	    break;
	}

	/* get the first element from the list */
	elm = *rlready;
	*rlready = elm->next;

	/*
	 * if this is the first record (for output archive) then do some
	 * admin stuff
	 */
	if (first_datarec) {
	    first_datarec = 0;
	    logctl.l_label.ill_start.tv_sec = elm->res->timestamp.tv_sec;
	    logctl.l_label.ill_start.tv_usec = elm->res->timestamp.tv_usec;
            logctl.l_state = PM_LOG_STATE_INIT;
            writelabel_data();
        }

	/*
	 * if we are in a pre_startwin state, and we are writing
	 * something out, then we are not in a pre_startwin state any more
	 * (it also means that there may be some discrete metrics to be
	 * written out)
	 */
	if (pre_startwin)
	    pre_startwin = 0;

	/* We need to write out the relevant context labelsm if any. */
	tstamp.tv_sec = mintime.tv_sec;
	tstamp.tv_usec = mintime.tv_usec;
	write_priorlabelset(PM_LABEL_CONTEXT, PM_IN_NULL, &tstamp);

	/* convert log record to a pdu */
	sts = __pmEncodeResult(PDU_OVERRIDE2, elm->res, &pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	}

        /* switch volumes if required */
        if (varg > 0) {
            if (written > 0 && (written % varg) == 0) {
                newvolume(outarchname, (pmTimeval *)&pb[3]);
	    }
        }
	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes
	 */
	peek_offset = __pmFtell(archctl.ac_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
	    newvolume(outarchname, (pmTimeval *)&pb[3]);
	}

	/* write out the descriptor and instance domain pdu's first */
	write_metareclist(elm->res, &needti);

	/* write out log record */
	old_log_offset = __pmFtell(archctl.ac_mfp);
	assert(old_log_offset >= 0);
	if ((sts = __pmLogPutResult2(&archctl, pb)) < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	}
	written++;


	/* check whether we need to write TI (temporal index) */
	if (old_log_offset == 0 ||
	    old_log_offset == sizeof(__pmLogLabel)+2*sizeof(int) ||
	    __pmFtell(archctl.ac_mfp) > flushsize)
		needti = 1;

	/*
	 * make sure that we do not write out the temporal index more
	 * than once for the same timestamp
	 */
	if (needti && tvcmp(&titime, &restime) >= 0)
	    needti = 0;

	/* flush/update */
	if (needti) {
	    titime = restime;

	    __pmFflush(archctl.ac_mfp);
	    __pmFflush(logctl.l_mdfp);

	    if (old_log_offset == 0)
		old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

            new_log_offset = __pmFtell(archctl.ac_mfp);
	    assert(new_log_offset >= 0);
            new_meta_offset = __pmFtell(logctl.l_mdfp);
	    assert(new_meta_offset >= 0);

            __pmFseek(archctl.ac_mfp, (long)old_log_offset, SEEK_SET);
            __pmFseek(logctl.l_mdfp, (long)old_meta_offset, SEEK_SET);

            __pmLogPutIndex(&archctl, &restime);

            __pmFseek(archctl.ac_mfp, (long)new_log_offset, SEEK_SET);
            __pmFseek(logctl.l_mdfp, (long)new_meta_offset, SEEK_SET);

            old_log_offset = __pmFtell(archctl.ac_mfp);
	    assert(old_log_offset >= 0);
            old_meta_offset = __pmFtell(logctl.l_mdfp);
	    assert(old_meta_offset >= 0);

            flushsize = __pmFtell(archctl.ac_mfp) + 100000;
        }

	/* free PDU buffer */
	__pmUnpinPDUBuf(pb);
	pb = NULL;

	elm->res = NULL;
	elm->next = NULL;
	free(elm);
    }
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
	fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	fprintf(stderr, "    writemark called, but mark not set\n");
	abandon_extract();
    }

    if (p == NULL) {
	fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	fprintf(stderr, "    writemark called, but no pdu\n");
	abandon_extract();
    }

    p->timestamp.tv_sec = htonl(p->timestamp.tv_sec);
    p->timestamp.tv_usec = htonl(p->timestamp.tv_usec);

    if ((sts = __pmLogPutResult2(&archctl, iap->pb[LOG])) < 0) {
	fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		pmGetProgname(), pmErrStr(sts));
	abandon_extract();
    }
    written++;
    free(iap->pb[LOG]);
    iap->pb[LOG] = NULL;
}

static int
do_not_need_mark(inarch_t *iap)
{
    int			indx, j;
    struct timeval	tstamp;
    struct timeval	smallest_tstamp;

    if (old_mark_logic || iap->pmcd_pid == -1 || iap->pmcd_seqnum == -1)
	/* no epilogue/prologue for me ... */
	return 0;

    j = -1;
    smallest_tstamp.tv_sec = INT_MAX;
    smallest_tstamp.tv_usec = 999999;
    for (indx=0; indx<inarchnum; indx++) {
	if (&inarch[indx] == iap)
	    continue;
	if (inarch[indx]._result != NULL) {
	    tstamp.tv_sec = inarch[indx]._result->timestamp.tv_sec;
	    tstamp.tv_usec = inarch[indx]._result->timestamp.tv_usec;
	    if (tstamp.tv_sec < smallest_tstamp.tv_sec || 
		(tstamp.tv_sec == smallest_tstamp.tv_sec && tstamp.tv_usec < smallest_tstamp.tv_usec)) {
		j = indx;
		smallest_tstamp.tv_sec = tstamp.tv_sec;
		smallest_tstamp.tv_usec = tstamp.tv_usec;
	    }
	}
    }
    if (j != -1) {
	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "EOF this pid=%" FMT_INT64 " seqnum=%d next[%d] pid=%" FMT_INT64 " seqnum=%d\n", (__int64_t)iap->pmcd_pid, iap->pmcd_seqnum, j, (__int64_t)inarch[j].pmcd_pid, inarch[j].pmcd_seqnum);
	}
	if (iap->pmcd_pid == inarch[j].pmcd_pid &&
	    iap->pmcd_seqnum == inarch[j].pmcd_seqnum)
	    return 1;
    }

    return 0;
}

/*--- END FUNCTIONS ---------------------------------------------------------*/

int
main(int argc, char **argv)
{
    int		indx;
    int		j;
    int		sts;
    int		stslog;			/* sts from nextlog() */
    int		stsmeta;		/* sts from nextmeta() */

    char	*msg;

    pmTimeval 	now = {0,0};		/* the current time */
    pmTimeval 	mintime = {0,0};
    pmTimeval 	tmptime = {0,0};

    pmTimeval	tstamp;			/* temporary timestamp */
    inarch_t	*iap;			/* ptr to archive control */
    rlist_t	*rlready = NULL;	/* results ready for writing */

    struct timeval	unused;


    __pmHashInit(&rdesc);	/* hash of meta desc records to write */
    __pmHashInit(&rindom);	/* hash of meta indom records to write */
    __pmHashInit(&rindomoneline);	/* indom oneline records to write */
    __pmHashInit(&rindomtext);	/* indom help text records to write */
    __pmHashInit(&rpmidoneline);	/* pmid oneline records to write */
    __pmHashInit(&rpmidtext);	/* pmid help text records to write */
    __pmHashInit(&rlabelset);	/* hash of meta label set records to write */

    /*
     * These come from the PMCD PMDA and pmlogger's epilogue/prologue
     * code.
     */
    pmid_pid = pmID_build(2,0,23);
    pmid_seqnum = pmID_build(2,0,24);

    /* no derived or anon metrics, please */
    __pmSetInternalState(PM_STATE_PMCS);

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
		pmGetProgname(), osstrerror());
	exit(1);
    }
    if (pmDebugOptions.appl0) {
        totalmalloc += (inarchnum * sizeof(inarch_t));
        fprintf(stderr, "main        : allocated %d\n",
			(int)(inarchnum * sizeof(inarch_t)));
    }


    for (indx=0; indx<inarchnum; indx++, opts.optind++) {
	iap = &inarch[indx];

	iap->name = argv[opts.optind];

	iap->pb[LOG] = iap->pb[META] = NULL;
	iap->eof[LOG] = iap->eof[META] = 0;
	iap->mark = 0;
	iap->pmcd_pid = -1;
	iap->pmcd_seqnum = -1;
	iap->recnum = 0;
	iap->_result = NULL;
	iap->_Nresult = NULL;

	if ((iap->ctx = pmNewContext(PM_CONTEXT_ARCHIVE, iap->name)) < 0) {
	    fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
		    pmGetProgname(), iap->name, pmErrStr(iap->ctx));
	    exit(1);
	}

	if ((sts = pmUseContext(iap->ctx)) < 0) {
	    fprintf(stderr, "%s: Error: cannot use context (%s): %s\n",
		    pmGetProgname(), iap->name, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmGetArchiveLabel(&iap->label)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmGetProgname(), iap->name, pmErrStr(sts));
	    exit(1);
	}

	if ((sts = pmGetArchiveEnd(&unused)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get end of archive (%s): %s\n",
		pmGetProgname(), iap->name, pmErrStr(sts));
	    if (desperate) {
		unused.tv_sec = INT_MAX;
		unused.tv_usec = 0;
	    }
	    else
		exit(1);
	}

	if (indx == 0) {
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
    } /*for(indx)*/

    logctl.l_label.ill_start.tv_sec = logstart_tval.tv_sec;
    logctl.l_label.ill_start.tv_usec = logstart_tval.tv_usec;

    /*
     * process config file
     *	- this includes a list of metrics and their instances
     */
    if (configfile && parseconfig() < 0)
	exit(1);

    if (zarg) {
	/* use TZ from metrics source (input-archive) */
	if ((sts = pmNewZone(inarch[0].label.ll_tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		    pmGetProgname(), pmErrStr(sts));
            exit_status = 1;
            goto cleanup;
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n", inarch[0].label.ll_hostname);
    }
    else if (tz != NULL) {
	/* use TZ as specified by user */
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		    pmGetProgname(), tz, pmErrStr(sts));
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
		    pmGetProgname(), pmErrStr(sts));
	    exit_status = 1;
	    goto cleanup;
	}
    }


    /* create output log - must be done before writing label */
    archctl.ac_log = &logctl;
    if ((sts = __pmLogCreate("", outarchname, outarchvers, &archctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /*
     * This must be done after log is created:
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
		pmGetProgname(), msg);
	abandon_extract();
    }
    winstart.tv_sec = winstart_tval.tv_sec;
    winstart.tv_usec = winstart_tval.tv_usec;
    winend.tv_sec = winend_tval.tv_sec;
    winend.tv_usec = winend_tval.tv_usec;
    logend.tv_sec = logend_tval.tv_sec;
    logend.tv_usec = logend_tval.tv_usec;

    if (warg) {
	if (winstart.tv_sec + NUM_SEC_PER_DAY < winend.tv_sec) {
	    fprintf(stderr, "%s: Warning: -S and -T must specify a time window within\nthe same day, for -w to be used.  Ignoring -w flag.\n", pmGetProgname());
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

    /*
     * get all meta data first
     * nextmeta() should return 0 (will return -1 when all meta is eof)
     */
    do {
	stsmeta = nextmeta();
    } while (stsmeta >= 0);


    /*
     * get log record - choose one with earliest timestamp
     * write out meta data (required by this log record)
     * write out log
     * do ti update if necessary
     */
    while (sarg == -1 || written < sarg) {
	ilog = -1;
	curlog.tv_sec = 0;
	curlog.tv_usec = 0;
	old_meta_offset = __pmFtell(logctl.l_mdfp);
	assert(old_meta_offset >= 0);

	/* nextlog() resets ilog, and curlog (to the smallest timestamp) */
	stslog = nextlog();

	if (stslog < 0)
	    break;

	/*
	 * find the _Nresult (or mark pdu) with the earliest timestamp;
	 * set ilog
	 * (this is a bit more complex when tflag is specified)
	 */
	mintime.tv_sec = mintime.tv_usec = 0;
	for (indx=0; indx<inarchnum; indx++) {
	    if (inarch[indx]._Nresult != NULL) {
		tstamp.tv_sec = inarch[indx]._Nresult->timestamp.tv_sec;
		tstamp.tv_usec = inarch[indx]._Nresult->timestamp.tv_usec;
		checklogtime(&tstamp, indx);

		if (ilog == indx) {
		    tmptime = curlog;
		    if (mintime.tv_sec <= 0 || tvcmp(&mintime, &tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	    else if (inarch[indx].pb[LOG] != NULL) {
		tstamp.tv_sec = inarch[indx].pb[LOG][3]; /* no swab needed */
		tstamp.tv_usec = inarch[indx].pb[LOG][4]; /* no swab needed */
		checklogtime(&tstamp, indx);

		if (ilog == indx) {
		    tmptime = curlog;
		    if (mintime.tv_sec <= 0 || tvcmp(&mintime, &tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	}

	/*
	 * now     == the earliest timestamp of the archive(s)
	 *		and/or mark records
	 * mintime == now or timestamp of the earliest mark
	 *		(whichever is smaller)
	 */
	now = curlog;

	/*
	 * note - mark (after last archive) will be created, but this
	 * break, will prevent it from being written out
	 */
	if (tvcmp(&now, &logend) > 0)
	    break;

	sts = checkwinend(now);
	if (sts < 0)
	    break;
	if (sts > 0)
	    continue;

	current = curlog;

	/* prepare to write out log record */
	if (ilog < 0 || ilog >= inarchnum) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    abandon_extract();
	}


	iap = &inarch[ilog];
	if (iap->mark) {
	    if (do_not_need_mark(iap)) {
		free(iap->pb[LOG]);
		iap->pb[LOG] = NULL;
	    }
	    else
		writemark(iap);
	}
	else {
	    /* result is to be written out, but there is no _Nresult */
	    if (iap->_Nresult == NULL) {
		fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
		fprintf(stderr, "    pick == LOG and _Nresult = NULL\n");
		abandon_extract();
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

	    /*
	     * writerlist frees elm (elements of rlready) but does not
	     * free _result & _Nresult
	     *
	     * free _result & _Nresult
	     *	_Nresult may contain space that was allocated
	     *	in __pmStuffValue this space has PM_VAL_SPTR format,
	     *	and has to be freed first
	     *	(in order to avoid memory leaks)
	     */
	    if (iap->_result != iap->_Nresult && iap->_Nresult != NULL) {
		int		i;
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
                pmGetProgname());
cleanup:
	abandon_extract();
    }
    else {
	/* write the last time stamp */
	__pmFflush(archctl.ac_mfp);
	__pmFflush(logctl.l_mdfp);

	if (old_log_offset == 0)
	    old_log_offset = sizeof(__pmLogLabel)+2*sizeof(int);

	new_log_offset = __pmFtell(archctl.ac_mfp);
	assert(new_log_offset >= 0);
	new_meta_offset = __pmFtell(logctl.l_mdfp);
	assert(new_meta_offset >= 0);

#if 0
	fprintf(stderr, "*** last tstamp: \n\tmintime=%d.%06d \n\ttmptime=%d.%06d \n\tlogend=%d.%06d \n\twinend=%d.%06d \n\tcurrent=%d.%06d\n",
	    mintime.tv_sec, mintime.tv_usec, tmptime.tv_sec, tmptime.tv_usec, logend.tv_sec, logend.tv_usec, winend.tv_sec, winend.tv_usec, current.tv_sec, current.tv_usec);
#endif

	__pmFseek(archctl.ac_mfp, old_log_offset, SEEK_SET);
	__pmLogPutIndex(&archctl, &current);


	/* need to fix up label with new start-time */
	writelabel_metati(1);
    }
    if (pmDebugOptions.appl0) {
        fprintf(stderr, "main        : total allocated %ld\n", totalmalloc);
    }

    exit(exit_status);
}
