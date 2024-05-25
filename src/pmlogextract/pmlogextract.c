/*
 * pmlogextract - extract desired metrics from PCP archives
 *
 * Copyright (c) 2014-2018,2021-2022 Red Hat.
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
 * Debug flags:
 * appl0	parser if -c configfile specified
 * appl1	malloc's and reclist operations
 * appl2	time window and EOF tests
 * appl3	in/out version decisions
 * appl4	indom juggling
 */

#include <math.h>
#include <ctype.h>
#include <sys/stat.h>
#include <assert.h>
#include "pmapi.h"
#include "libpcp.h"
#include "archive.h"
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
    { "outputversion", 1, 'V', "VERSION", "output archive version" },
    { "", 1, 'v', "SAMPLES", "switch log volumes after this many samples" },
    { "", 0, 'w', 0, "ignore day/month/year" },
    { "", 0, 'x', 0, "skip metrics with mismatched metadata" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "c:D:dfmS:s:T:V:v:wxZ:z?",
    .long_options = longopts,
    .short_usage = "[options] input-archive output-archive",
};

/*
 *  global constants
 */
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
 *  Global variables
 */
static int	exit_status;
static int	outarchvers = -1;		/* version of output archive */
static int	first_datarec = 1;		/* first record flag */
static int	pre_startwin = 1;		/* outside time win flag */
static int	written;			/* num log writes so far */
int		ml_numpmid;			/* num pmid in ml list */
int		ml_size;			/* actual size of ml array */
mlist_t		*ml;				/* list of pmids with indoms */
rlist_t		*rl;				/* list of __pmResults */
int		skip_ml_numpmid;		/* num entries in skip_ml list */
pmID		*skip_ml;


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

static __pmTimestamp	curlog;		/* most recent timestamp in log */
static __pmTimestamp	current;	/* most recent timestamp overall */

/* time window stuff */
static struct timeval logstart_tv;	/* extracted log start */
static struct timeval logend_tv;	/* extracted log end */
static struct timeval winstart_tv;	/* window start tval*/
static struct timeval winend_tv;	/* window end tval*/

static __pmTimestamp	logstart;		/* real earliest start time */
static __pmTimestamp	logend;
static __pmTimestamp	winstart = {-1,0};	/* window start time */
static __pmTimestamp	winend = {-1,0};	/* window end time */
static __pmTimestamp	logend = {-1,0};	/* log end time */

/* command line args */
char	*configfile;			/* -c arg - name of config file */
int	farg;				/* -f arg - use first timezone */
int	old_mark_logic;			/* -m arg - <mark> b/n archives */
int	sarg = -1;			/* -s arg - finish after X samples */
char	*Sarg;				/* -S arg - window start */
char	*Targ;				/* -T arg - window end */
int	varg = -1;			/* -v arg - switch log vol every X */
int	warg;				/* -w arg - ignore day/month/year */
int	xarg;				/* -x arg - skip metrics with mismatched metadata */
int	zarg;				/* -z arg - use archive timezone */
char	*tz;				/* -Z arg - use timezone from user */

/* cmd line args that could exist, but don't (needed for pmParseTimeWin) */
char	*Aarg;				/* -A arg - non-existent */
char	*Oarg;				/* -O arg - non-existent */

/*--- START FUNCTIONS -------------------------------------------------------*/

/*
 * fatal error, dump state of input and output streams
 */
void
dump_state(void)
{
    int		i;

    for (i = 0; i < inarchnum; i++) {
	fprintf(stderr, "in [%d] %s: LOG PDU", i, inarch[i].name);
	if (inarch[i].pb[LOG] == NULL)
	    fprintf(stderr, " NULL");
	else
	    fprintf(stderr, " %p", inarch[i].pb[LOG]);
	fprintf(stderr, " META PDU");
	if (inarch[i].pb[META] == NULL)
	    fprintf(stderr, " NULL");
	else
	    fprintf(stderr, " %p", inarch[i].pb[META]);
	fprintf(stderr, " result");
	if (inarch[i]._result == NULL)
	    fprintf(stderr, " NULL");
	else
	    fprintf(stderr, " %p", inarch[i]._result);
	fprintf(stderr, " Nresult");
	if (inarch[i]._Nresult == NULL)
	    fprintf(stderr, " NULL");
	else
	    fprintf(stderr, " %p", inarch[i]._Nresult);
	fprintf(stderr, " laststamp ");
	__pmPrintTimestamp(stderr, &inarch[i].laststamp);
	fprintf(stderr, " mark %d", inarch[i].mark);
	fprintf(stderr, " LOG eof %d META eof %d", inarch[i].eof[LOG], inarch[i].eof[META]);
	fprintf(stderr, " recnum %d pmcd_pid %" FMT_INT64 " pmcd_seqnum %d\n",
	    inarch[i].recnum, inarch[i].pmcd_pid, inarch[i].pmcd_seqnum);
    }

    fprintf(stderr, "out %s: version %d written %d\n", 
	outarchname, outarchvers, written);
}

/*
 * extract metric name(s) from metadata pdu buffer
 */
void
printmetricnames(FILE *f, __int32_t *pdubuf)
{
    if (ntohl(pdubuf[0]) > 8) {
	/*
	 * have at least one name ... names are packed
	 * <len><name>... at the end of the buffer
	 */
	int		numnames = ntohl(pdubuf[7]);
	char		*p = (char *)&pdubuf[8];
	int		i;
	__int32_t	len;

	for (i = 0; i < numnames; i++) {
	    memmove((void *)&len, (void *)p, sizeof(__int32_t));
	    len = ntohl(len);
	    p += sizeof(__int32_t);
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
matchnames(__int32_t *a, __int32_t *b)
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
	__int32_t		len_a;

	memmove((void *)&len_a, (void *)p_a, sizeof(__int32_t));
	len_a = ntohl(len_a);
	p_a += sizeof(__int32_t);
	p_b = (char *)&b[8];
	for (i_b = 0; i_b < num_b; i_b++) {
	    __int32_t		len_b;
	    memmove((void *)&len_b, (void *)p_b, sizeof(__int32_t));
	    len_b = ntohl(len_b);
	    p_b += sizeof(__int32_t);

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
	__int32_t		len_b;

	memmove((void *)&len_b, (void *)p_b, sizeof(__int32_t));
	len_b = ntohl(len_b);
	p_b += sizeof(__int32_t);
	p_a = (char *)&a[8];
	for (i_a = 0; i_a < num_a; i_a++) {
	    __int32_t		len_a;
	    memmove((void *)&len_a, (void *)p_a, sizeof(__int32_t));
	    len_a = ntohl(len_a);
	    p_a += sizeof(__int32_t);

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
 * -x and found something inconsistent in the metadata, add this pmid to
 * skip_ml so it is omitted from the output archive
 */
void
skip_metric(pmID pmid)
{
    pmID	*skip_ml_tmp;
    int		j;

    /* avoid dups */
    for (j=0; j<skip_ml_numpmid; j++) {
	if (pmid == skip_ml[j])
	    break;
    }
    if (j == skip_ml_numpmid) {
	/* not already on the list, append it */
	skip_ml_numpmid++;
	skip_ml_tmp = realloc(skip_ml, skip_ml_numpmid*sizeof(pmID));
	if (skip_ml_tmp == NULL) {
	    fprintf(stderr, "skip_metric: Error: cannot realloc %ld bytes for skip_ml[]\n",
		    (long)skip_ml_numpmid*sizeof(pmID));
	    abandon_extract();
	    /*NOTREACHED*/
	} else {
	    skip_ml = skip_ml_tmp;
	    skip_ml[skip_ml_numpmid-1] = pmid;
	}
    }
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
newvolume(char *base, __pmTimestamp *tsp)
{
    __pmFILE		*newfp;
    int			nextvol = archctl.ac_curvol + 1;

    if ((newfp = __pmLogNewFile(base, nextvol)) != NULL) {
	__pmFclose(archctl.ac_mfp);
	archctl.ac_mfp = newfp;
	logctl.label.vol = archctl.ac_curvol = nextvol;
	__pmLogWriteLabel(archctl.ac_mfp, &logctl.label);
	__pmFflush(archctl.ac_mfp);
	fprintf(stderr, "%s: New log volume %d, at ", pmGetProgname(), nextvol);
	__pmPrintTimestamp(stderr, tsp);
	fputc('\n', stderr);
    }
    else {
	fprintf(stderr, "%s: Error: volume %d: %s\n",
		pmGetProgname(), nextvol, pmErrStr(-oserror()));
	abandon_extract();
	/*NOTREACHED*/
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
    int			indx;
    int			max_inarchvers = -1;
    inarch_t		*f_iap = NULL;		/* first non-empty archive */
    inarch_t		*l_iap = NULL;		/* last non-empty archive */
    inarch_t		*iap;
    __pmLogLabel	*lp = &logctl.label;

    /*
     * check archive version numbers
     * - output version is max(input versions) || as per -V command line
     * - input versions must all be <= output version
     * - only verion 2 and 3 is supported
     */
    for (indx=0; indx<inarchnum; indx++) {
	int		vers;
	iap = &inarch[indx];
	
	if (iap->ctx == PM_ERR_NODATA) {
	    /* empty input archive, nothing to check here ... */
	    continue;
	}

	vers = (iap->label.magic & 0xff);
	if (pmDebugOptions.appl3)
	    fprintf(stderr, "archive: %s version: %d\n", iap->name, vers);
	if (vers != PM_LOG_VERS02 && vers != PM_LOG_VERS03) {
	    fprintf(stderr,"%s: Error: illegal version number %d in archive (%s)\n",
		    pmGetProgname(), vers, iap->name);
	    abandon_extract();
	    /*NOTREACHED*/
	}

	if (vers > max_inarchvers)
	    max_inarchvers = vers;
	if (outarchvers != -1 && vers > outarchvers) {
	    fprintf(stderr, 
		"%s: Error: input archive version must be no more than %d\n"
		"archive: %s version: %d\n",
		    pmGetProgname(), outarchvers, iap->name, vers);
	    abandon_extract();
	    /*NOTREACHED*/
        }
    }
    if (outarchvers == -1)
	outarchvers = max_inarchvers;
    if (pmDebugOptions.appl3)
	fprintf(stderr, "output archive version: %d\n", outarchvers);

    /*
     * set outarch to inarch[indx] of first non-empty archive to start
     * off with ... there must be at least one of these if we get
     * this far
     */
    for (indx=0; indx<inarchnum; indx++) {
	if (inarch[indx].ctx != PM_ERR_NODATA) {
	    f_iap = &inarch[indx];
	    break;
	}
    }
    if (f_iap == NULL) {
	fprintf(stderr, "Botch: no non-empty archive\n");
	abandon_extract();
	/*NOTREACHED*/
    }

    /* copy magic number, pid, host and timezone */
    lp->magic = PM_LOG_MAGIC | outarchvers;
    lp->pid = (int)getpid();
    free(lp->hostname);
    lp->hostname = strdup(f_iap->label.hostname);
    if (farg) {
	/*
	 * use timezone and zoneinfo from _first_ non-empty archive ...
	 * this is the OLD default
	 */
	free(lp->timezone);
	lp->timezone = strdup(f_iap->label.timezone);
	if (lp->zoneinfo != NULL)
	    free(lp->zoneinfo);
	if (f_iap->label.zoneinfo != NULL)
	    lp->zoneinfo = strdup(f_iap->label.zoneinfo);
	else
	    lp->zoneinfo = NULL;
    }
    else {
	/*
	 * use timezone and zoneinfo from the _last_ non-empty archive ...
	 * this is the NEW default
	 */
	for (indx=inarchnum-1; indx>=0; indx--) {
	    if (inarch[indx].ctx != PM_ERR_NODATA) {
		l_iap = &inarch[indx];
		free(lp->timezone);
		lp->timezone = strdup(l_iap->label.timezone);
		if (lp->zoneinfo != NULL)
		    free(lp->zoneinfo);
		if (l_iap->label.zoneinfo != NULL)
		    lp->zoneinfo = strdup(l_iap->label.zoneinfo);
		else
		    lp->zoneinfo = NULL;
		break;
	    }
	}
    }

    /* reset outarch as appropriate, depending on other input archives */
    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];
	
	if (iap->ctx == PM_ERR_NODATA) {
	    /* empty input archive, nothing to check here ... */
	    continue;
	}

	/* Ensure all archives of the same host */
	if (strcmp(lp->hostname, iap->label.hostname) != 0) {
	    fprintf(stderr,"%s: Error: host name mismatch for input archives\n",
		    pmGetProgname());
	    fprintf(stderr, "archive: %s host: %s\n",
		    f_iap->name, f_iap->label.hostname);
	    fprintf(stderr, "archive: %s host: %s\n",
		    iap->name, iap->label.hostname);
	    abandon_extract();
	    /*NOTREACHED*/
	}

	/* Ensure all archives of the same timezone */
	if (strcmp(lp->timezone, iap->label.timezone) != 0) {
	    const char	*log_used;

	    if (farg)
		log_used = f_iap->name;
	    else if (l_iap)
		log_used = l_iap->name;
	    else
		log_used = "?";
	    fprintf(stderr,
		    "%s: Warning: timezone mismatch for input archives\n",
		    pmGetProgname());
	    fprintf(stderr, "archive: %s timezone: %s [will be used]\n",
		    log_used, lp->timezone);
	    fprintf(stderr, "archive: %s timezone: %s [will be ignored]\n",
		    iap->name, iap->label.timezone);
	}
    } /*for(indx)*/
}


/*
 *  
 */
void
writelabel_metati(int do_rewind)
{
    if (do_rewind) __pmRewind(logctl.tifp);
    logctl.label.vol = PM_LOG_VOL_TI;
    __pmLogWriteLabel(logctl.tifp, &logctl.label);

    if (do_rewind) __pmRewind(logctl.mdfp);
    logctl.label.vol = PM_LOG_VOL_META;
    __pmLogWriteLabel(logctl.mdfp, &logctl.label);
}


/*
 *  
 */
void
writelabel_data(void)
{
    logctl.label.vol = 0;
    __pmLogWriteLabel(archctl.ac_mfp, &logctl.label);
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
	/*NOTREACHED*/
    }
    if (pmDebugOptions.appl1) {
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
	/*NOTREACHED*/
    }
    if (pmDebugOptions.appl1) {
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
	/*NOTREACHED*/
    }
}

/*
 * borrowed from __ntohpmUnits() in endian.c in libpcp (that function is
 * not exported there, so not callable here)
 */
static pmUnits
ntoh_pmUnits(pmUnits units)
{
    unsigned int x;

    x = ntohl(*(unsigned int *)&units);
    units = *(pmUnits *)&x;
    return units;
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
		fprintf(stderr, "%s: %s: metric ",
		    pmGetProgname(), xarg == 0 ? "Error" : "Warning");
		printmetricnames(stderr, curr->pdu);
		fprintf(stderr, ": PMID changed from %s", pmIDStr(curr->desc.pmid));
		fprintf(stderr, " to %s!\n", pmIDStr(pmid));
		if (xarg == 0)
		    abandon_extract();
		    /*NOTREACHED*/
		else
		    skip_metric(curr->desc.pmid);
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
	    /*NOTREACHED*/
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
	    fprintf(stderr, "%s: %s: metric PMID %s",
		pmGetProgname(), xarg == 0 ? "Error" : "Warning",
		pmIDStr(curr->desc.pmid));
	    fprintf(stderr, ": name changed from ");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, " to ");
	    printmetricnames(stderr, iap->pb[META]);
	    fprintf(stderr, "!\n");
	    if (xarg == 0)
		abandon_extract();
		/*NOTREACHED*/
	    else
		skip_metric(curr->desc.pmid);
	}
	if (curr->desc.type != ntohl(iap->pb[META][3])) {
	    fprintf(stderr, "%s: %s: metric ",
		pmGetProgname(), xarg == 0 ? "Error" : "Warning");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": type changed from");
	    fprintf(stderr, " %s", pmTypeStr(curr->desc.type));
	    fprintf(stderr, " to %s!\n", pmTypeStr(ntohl(iap->pb[META][3])));
	    if (xarg == 0)
		abandon_extract();
		/*NOTREACHED*/
	    else
		skip_metric(curr->desc.pmid);
	}
	if (curr->desc.indom != ntoh_pmInDom(iap->pb[META][4])) {
	    fprintf(stderr, "%s: %s: metric ",
		pmGetProgname(), xarg == 0 ? "Error" : "Warning");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": indom changed from");
	    fprintf(stderr, " %s", pmInDomStr(curr->desc.indom));
	    fprintf(stderr, " to %s!\n", pmInDomStr(ntoh_pmInDom(iap->pb[META][4])));
	    if (xarg == 0)
		abandon_extract();
		/*NOTREACHED*/
	    else
		skip_metric(curr->desc.pmid);
	}
	if (curr->desc.sem != ntohl(iap->pb[META][5])) {
	    fprintf(stderr, "%s: %s: metric ",
		pmGetProgname(), xarg == 0 ? "Error" : "Warning");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": semantics changed from");
	    fprintf(stderr, " ");
	    printsem(stderr, curr->desc.sem);
	    fprintf(stderr, " to ");
	    printsem(stderr, (int)ntohl(iap->pb[META][5]));
	    fprintf(stderr, "!\n");
	    if (xarg == 0)
		abandon_extract();
		/*NOTREACHED*/
	    else
		skip_metric(curr->desc.pmid);
	}
	pmup = (pmUnits *)&iap->pb[META][6];
	pmu = ntoh_pmUnits(*pmup);
	if (curr->desc.units.dimSpace != pmu.dimSpace ||
	    curr->desc.units.dimTime != pmu.dimTime ||
	    curr->desc.units.dimCount != pmu.dimCount ||
	    curr->desc.units.scaleSpace != pmu.scaleSpace ||
	    curr->desc.units.scaleTime != pmu.scaleTime ||
	    curr->desc.units.scaleCount != pmu.scaleCount) {
	    fprintf(stderr, "%s: %s: metric ",
		pmGetProgname(), xarg == 0 ? "Error" : "Warning");
	    printmetricnames(stderr, curr->pdu);
	    fprintf(stderr, ": units changed from");
	    fprintf(stderr, " %s", pmUnitsStr(&curr->desc.units));
	    fprintf(stderr, " to %s!\n", pmUnitsStr(&pmu));
	    if (xarg == 0)
		abandon_extract();
		/*NOTREACHED*/
	    else
		skip_metric(curr->desc.pmid);
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
    __int32_t		*pdu;
    int			type;
    int			indom;
    __pmTimestamp	stamp;

    iap = &inarch[indx];
    pdu = iap->pb[META];
    type = ntohl(pdu[1]);

    if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA) {
	__pmLoadTimestamp(&pdu[2], &stamp);
	indom = ntoh_pmInDom(pdu[5]);
    }
    else {
	__pmLoadTimeval(&pdu[2], &stamp);
	indom = ntoh_pmInDom(pdu[4]);
    }

    if ((hp = __pmHashSearch(indom, &rindom)) == NULL) {
	/* append new record */
	curr = mk_reclist_t();
	curr->pdu = pdu;
	curr->stamp = stamp;		/* struct assignment */
	curr->desc.indom = indom;

	if (__pmHashAdd(indom, (void *)curr, &rindom) < 0) {
	    fprintf(stderr, "%s: Error: cannot add to indom hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	    /*NOTREACHED*/
	}
    } else {
	curr = (reclist_t *)hp->data;

	if (curr->pdu == NULL) {
	    /* insert new record */
	    curr->pdu = iap->pb[META];
	    curr->stamp = stamp;		/* struct assignment */
	}
	else {
	    /* do NOT discard old record; append new record */
	    curr->recs = add_reclist_t(curr);
	    rec = &curr->recs[curr->nrecs];
	    rec->pdu = pdu;
	    rec->stamp = stamp;			/* struct assignment */
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
append_labelsetreclist(int i, int type)
{
    inarch_t		*iap;
    __pmHashNode	*hp;
    __pmHashCtl		*hash2;
    __pmTimestamp	stamp;
    reclist_t		*rec;
    int			sts;
    int			ltype;		/* label type */
    int			id;
    int			k;

    iap = &inarch[i];

    /* Initialize the new record. */
    rec = mk_reclist_t();
    rec->pdu = iap->pb[META];
    if (type == TYPE_LABEL) {
	__pmLoadTimestamp(&rec->pdu[2], &stamp);
	k = 5;
    }
    else if (type == TYPE_LABEL_V2) {
	__pmLoadTimeval(&rec->pdu[2], &stamp);
	k = 4;
    }
    else {
	fprintf(stderr, "append_labelsetreclist: Botch type=%s (%d)\n", __pmLogMetaTypeStr(type), type);
	exit(1);
    }
    rec->stamp = stamp;		/* struct assignment */

    /*
     * Label sets are stored in a 2 level hash table. First hashed by ltype.
     */
    ltype = ntoh_pmLabelType(rec->pdu[k++]);
    if ((hp = __pmHashSearch(ltype, &rlabelset)) == NULL) {
	/* This label type was not found. Create a hash table for it. */
	if ((hash2 = (__pmHashCtl *) malloc(sizeof(*hash2))) == NULL) {
	    fprintf(stderr, "%s: Error: cannot malloc space for hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	    /*NOTREACHED*/
	}
	__pmHashInit(hash2);

	sts = __pmHashAdd(ltype, (void *)hash2, &rlabelset);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: cannot add secondary hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	    /*NOTREACHED*/
	}
    }
    else
	hash2 = (__pmHashCtl *)hp->data;

    /*
     * Add the new label set record, even if one with the same type and id
     * already exists.
     */
    id = ntoh_pmID(iap->pb[META][k++]);
    sts = __pmHashAdd(id, (void *)rec, hash2);
    if (sts < 0) {
	fprintf(stderr, "%s: Error: cannot add label set record.\n",
		pmGetProgname());
	abandon_extract();
	/*NOTREACHED*/
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
    int		ident = -1;
    const char *str1, *str2;

    iap = &inarch[i];
    type = ntoh_pmTextType(iap->pb[META][2]);
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID && (type & PM_TEXT_INDOM) == 0)
	ident = ntoh_pmID(iap->pb[META][3]);
    else if ((type & PM_TEXT_PMID) == 0 && (type & PM_TEXT_INDOM) == PM_TEXT_INDOM)
	ident = ntoh_pmInDom(iap->pb[META][3]);
    else {
	fprintf(stderr, "%s: append_textreclist: Botch: bad type %d for ident %d\n",
		pmGetProgname(), type, ntoh_pmID(iap->pb[META][3]));
	abandon_extract();
	/*NOTREACHED*/
    }
    if (((type & PM_TEXT_ONELINE) == PM_TEXT_ONELINE && (type & PM_TEXT_HELP) == 0) ||
	((type & PM_TEXT_ONELINE) == 0 && (type & PM_TEXT_HELP) == PM_TEXT_HELP)) {
	/* good, expect one or the other to be set */
	;
    }
    else {
	if ((type & PM_TEXT_PMID)) {
	    fprintf(stderr, "%s: append_textreclist: Botch: bad type %d for pmid %s\n",
		    pmGetProgname(), type, pmIDStr(ident));
	    abandon_extract();
	    /*NOTREACHED*/
	}
	else /* (type & PM_TEXT_INDOM) */ {
	    fprintf(stderr, "%s: append_textreclist: Botch: bad type %d for indom %s\n",
		    pmGetProgname(), type, pmInDomStr(ident));
	    abandon_extract();
	    /*NOTREACHED*/
	}
    }

    if (pmDebugOptions.appl1) {
	fprintf(stderr, "update_textreclist: looking for ");
	if ((type & PM_TEXT_PMID))
	    fprintf(stderr, "(pmid:%s)", pmIDStr(ident));
	else /* (type & PM_TEXT_INDOM) */
	    fprintf(stderr, "(indom:%s)", pmInDomStr(ident));
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
	    else /* (type & PM_TEXT_INDOM) */ {
		fprintf(stderr, "indom match %s\n", pmInDomStr(curr->desc.indom));
	    }
	}
	str1 = (const char *)&curr->pdu[4];
	str2 = (const char *)&iap->pb[META][4];
	if (strcmp(str1, str2) != 0) {
	    fprintf(stderr, "%s: Warning: ", pmGetProgname());
	    if ((type & PM_TEXT_PMID))
		fprintf(stderr, "metric PMID %s", pmIDStr(curr->desc.pmid));
	    else /* (type & PM_TEXT_INDOM) */
		fprintf(stderr, "instance domain %s",pmInDomStr(curr->desc.indom));
	    if ((type & PM_TEXT_ONELINE)) {
		fprintf(stderr, " one line text changed from\n");
		fprintf(stderr, "  \"%s\" to\n", str1);
		fprintf(stderr, "  \"%s\"!\n", str2);
	    }
	    else /* (type & PM_TEXT_HELP) */ {
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
	    else /* (type & PM_TEXT_HELP) */
		sts = __pmHashAdd(ident, (void *)curr, &rpmidtext);
	} else /* (type & PM_TEXT_INDOM) */ {
	    curr->desc.indom = ident;
	    if ((type & PM_TEXT_ONELINE))
		sts = __pmHashAdd(ident, (void *)curr, &rindomoneline);
	    else /* (type & PM_TEXT_HELP) */
		sts = __pmHashAdd(ident, (void *)curr, &rindomtext);
	}
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: cannot add to help text hash table.\n",
		    pmGetProgname());
	    abandon_extract();
	    /*NOTREACHED*/
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
    int		type;
    __pmLogHdr	*h;
    int		rlen;

    if (rec->written == MARK_FOR_WRITE) {
	if (rec->pdu == NULL) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	    fprintf(stderr,"    record is marked for write, but pdu is NULL\n");
	    dump_state();
	    abandon_extract();
	    /*NOTREACHED*/
	}

	h = (__pmLogHdr *)rec->pdu;
	rlen = ntohl(h->len);
	type = ntohl(h->type);

	if (pmDebugOptions.logmeta) {
	    fprintf(stderr, "write_rec: record len=%d, type=%s (%d) @ offset=%d\n",
	    	rlen, __pmLogMetaTypeStr(type), type, (int)(__pmFtell(logctl.mdfp) - sizeof(__pmLogHdr)));
	    if (type == TYPE_DESC) {
		pmDesc	*dp;
		pmDesc	desc;
		int	len;
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
	    else if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
		__int32_t		*buf;
		__int32_t		*ibuf;
		__pmLogInDom	lid;
		/*
		 * __pmLogLoadInDom() below may re-write (ntohl()) some of
		 * the PDU buffer, so we need to operate on a copy for this
		 * diagnostic code ...
		 */
		if ((buf = (__int32_t *)malloc(rlen)) == NULL) {
		    fprintf(stderr, "malloc for dup indom buf failed\n");
		}
		else {
		    memcpy(buf, rec->pdu, rlen);

		    ibuf = &buf[2];
		    sts = __pmLogLoadInDom(NULL, 0, type, &lid, &ibuf);
		    if (sts < 0) {
			fprintf(stderr, "write_rec: __pmLogLoadInDom(type=%s (%d)): failed: %s\n", __pmLogMetaTypeStr(type), type, pmErrStr(sts));
		    }
		    else {
			fprintf(stderr, "INDOM: %s when: ", pmInDomStr(lid.indom));
			__pmPrintTimestamp(stderr, &lid.stamp);
			fprintf(stderr, " numinst: %d", lid.numinst);
			if (lid.numinst > 0) {
			    int		i;
			    for (i = 0; i < lid.numinst; i++) {
				fprintf(stderr, " [%d] %d", i, lid.instlist[i]);
			    }
			}
			fputc('\n', stderr);
		    }
		    __pmFreeLogInDom(&lid);
		    free(buf);
		}
	    }
	    else if (type == TYPE_LABEL) {
		__pmTimestamp	when;
		int		k = 2;
		int		label_type;
		int		ident;
		char		buf[1024];

		__pmLoadTimestamp(&rec->pdu[k], &when);
		k += 3;
		label_type = ntoh_pmLabelType((unsigned int)rec->pdu[k++]);
		ident = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "LABELSET: %s when: ",
			__pmLabelIdentString(ident, label_type, buf, sizeof(buf)));
		__pmPrintTimestamp(stderr, &when);
		fputc('\n', stderr);
	    }
	    else if (type == TYPE_LABEL_V2) {
		__pmTimestamp	when;
		int		k = 2;
		int		label_type;
		int		ident;
		char		buf[1024];

		__pmLoadTimeval(&rec->pdu[k], &when);
		k += 2;
		label_type = ntoh_pmLabelType((unsigned int)rec->pdu[k++]);
		ident = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "V2 LABELSET: %s when: ",
			__pmLabelIdentString(ident, label_type, buf, sizeof(buf)));
		__pmPrintTimestamp(stderr, &when);
		fputc('\n', stderr);
	    }
	    else if (type == TYPE_TEXT) {
		int		k = 2;
		int		text_type;
		int		ident;

		text_type = ntoh_pmTextType((unsigned int)rec->pdu[k++]);
		fprintf(stderr, "TEXT: type: %s ",
			((text_type & PM_TEXT_ONELINE)) ? "oneline" : "help");
		if ((text_type & PM_TEXT_PMID)) {
		    ident = ntoh_pmID((unsigned int)rec->pdu[k++]);
		    fprintf(stderr, "TEXT: PMID: %s", pmIDStr(ident));
		}
		else { /* (text_type & PM_TEXT_PMINDOM) */
		    ident = ntoh_pmInDom((unsigned int)rec->pdu[k++]);
		    fprintf(stderr, "TEXT: INDOM: %s", pmInDomStr(ident));
		}
		if ((text_type & PM_TEXT_DIRECT))
		    fprintf(stderr, " DIRECT");
		fputc('\n', stderr);
	    }
	    else {
		fprintf(stderr, "Botch: bad type: %d\n", type);
	    }
	}

	if (type == TYPE_INDOM) {
	    sts = pmaTryDeltaInDom(&logctl, &rec->pdu, NULL);
	    if (sts < 0) {
		fprintf(stderr, "Botch: pmaTryDeltaInDom failed: %d\n", sts);
		abandon_extract();
		/*NOTREACHED*/
	    }
	    if (pmDebugOptions.logmeta && sts == 1) {
		h = (__pmLogHdr *)rec->pdu;
		rlen = ntohl(h->len);
		type = ntohl(h->type);
		fprintf(stderr, "write_rec: delta indom rewrite len=%d, type=%s (%d) @ offset=%d\n",
	    	rlen, __pmLogMetaTypeStr(type), type, (int)(__pmFtell(logctl.mdfp) - sizeof(__pmLogHdr)));
	    }
	}

	/* write out the pdu ; exit if write failed */
	if ((sts = pmaPutLog(logctl.mdfp, rec->pdu)) < 0) {
	    fprintf(stderr, "%s: Error: pmaPutLog: meta data : %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	    /*NOTREACHED*/
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
write_priorlabelset(int type, int ident, const __pmTimestamp *now)
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
	if (curr_labelset->stamp.sec < now->sec ||
	    (curr_labelset->stamp.sec == now->sec &&
	     curr_labelset->stamp.nsec <= now->nsec)) {
	    /*
	     * labelset is in list, labelset has pdu
	     * and timestamp in pdu suits us
	     */
	    if (other_labelset == NULL ||
		other_labelset->stamp.sec < curr_labelset->stamp.sec ||
		(other_labelset->stamp.sec == curr_labelset->stamp.sec &&
		 other_labelset->stamp.nsec <= curr_labelset->stamp.nsec)){
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
	if (outarchvers == PM_LOG_VERS03)
	    __pmPutTimestamp(now, &other_labelset->pdu[2]);
	else
	    __pmPutTimeval(now, &other_labelset->pdu[2]);
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

    return __pmTimestampCmp(&ar->stamp, &br->stamp);
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
indom_closest(reclist_t *recs, __pmTimestamp *tsp)
{
    unsigned int	first, last, count, middle, previous;
    reclist_t		*indom, *array = &recs->recs[0];
    int			sts = -1;

    first = 0;
    count = last = recs->nrecs - 1;
    middle = previous = (first + last) / 2;

    while (first <= last) {
	indom = &array[middle];
	sts = __pmTimestampCmp(&indom->stamp, tsp);
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
	sts = __pmTimestampCmp(&indom->stamp, tsp);
    if (sts < 0)
	return indom;
    return NULL;
}

/* borrowed from libpcp ... */
static void
dump_valueset(FILE *f, pmValueSet *vsp)
{
    char	errmsg[PM_MAXERRMSGLEN];
    char	strbuf[20];
    char	*pmid;
    int		j;

    pmid = pmIDStr_r(vsp->pmid, strbuf, sizeof(strbuf));
    fprintf(f, "  %s (%s):", pmid, "<noname>");
    if (vsp->numval == 0) {
	fprintf(f, " No values returned!\n");
	return;
    }
    if (vsp->numval < 0) {
	fprintf(f, " %s\n", pmErrStr_r(vsp->numval, errmsg, sizeof(errmsg)));
	return;
    }
    fprintf(f, " numval: %d", vsp->numval);
    fprintf(f, " valfmt: %d vlist[]:\n", vsp->valfmt);
    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
	    fprintf(f, "    inst [%d", vp->inst);
	    fprintf(f, " or ???]");
	    fputc(' ', f);
	}
	else
	    fprintf(f, "   ");
	fprintf(f, "value ");

	if (vsp->valfmt == PM_VAL_INSITU)
	    pmPrintValue(f, vsp->valfmt, PM_TYPE_UNKNOWN, vp, 1);
	else if (vsp->valfmt == PM_VAL_DPTR || vsp->valfmt == PM_VAL_SPTR)
	    pmPrintValue(f, vsp->valfmt, (int)vp->value.pval->vtype, vp, 1);
	else
	    fprintf(f, "bad valfmt %d", vsp->valfmt);
	fputc('\n', f);
    }
}

void
write_metareclist(inarch_t *iap, __pmResult *result, int *needti)
{
    int			n, count;
    reclist_t		*curr_desc;	/* current desc record */
    reclist_t		*curr_indom;	/* current indom record */
    reclist_t   	*other_indom;	/* other indom record */
    pmID		pmid;
    pmInDom		indom;
    __pmHashNode	*hp;
    __pmTimestamp	stamp;		/* ptr to timestamp in result */

    stamp = result->timestamp;

    /* if pmid in result matches a pmid in desc then write desc */
    for (n = 0; n < result->numpmid; n++) {
	pmid = result->vset[n]->pmid;
	indom = PM_IN_NULL;
	count = 0;
	curr_indom = NULL;

	if ((hp = __pmHashSearch(pmid, &rdesc)) == NULL) {
	    /* descriptor has not been found - this is bad */
	    int		j;
	    fprintf(stderr, "%s: Warning: [log %s] metadata for PMID %s has not been found, culling this metric ...\n",
		    pmGetProgname(), iap->name, pmIDStr(pmid));
	    /* borrowed from __pmPrintResult() ... */
	    fprintf(stderr, "__pmResult dump from " PRINTF_P_PFX "%p timestamp: %" FMT_INT64 ".%09d ",
		    result, result->timestamp.sec, result->timestamp.nsec);
	    __pmPrintTimestamp(stderr, &result->timestamp);
	    fprintf(stderr, " numpmid: %d\n", result->numpmid);
	    fprintf(stderr, "[%d] ", n);
	    dump_valueset(stderr, result->vset[n]);
	    /*
	     * repack pmResult ... small mem leak possible here but this
	     * has very low probability of every happening, and even
	     * then it is likely to be in our QA environment
	     */
	    for (j = n+1; j < result->numpmid; j++)
		result->vset[j-1] = result->vset[j];
	    result->numpmid--;
	    n--;
	    continue;
	}
	else {
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
		/*NOTREACHED*/
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
	write_priorlabelset(PM_LABEL_ITEM, pmid, &stamp);
	write_priorlabelset(PM_LABEL_DOMAIN, pmid, &stamp);
	write_priorlabelset(PM_LABEL_CLUSTER, pmid, &stamp);

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
		other_indom = indom_closest(curr_indom, &stamp);
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
		    if (outarchvers == PM_LOG_VERS03)
			__pmPutTimestamp(&stamp, &other_indom->pdu[2]);
		    else
			__pmPutTimeval(&stamp, &other_indom->pdu[2]);
		    /* make sure to set needti, when writing out the indom */
		    *needti = 1;
		    write_rec(other_indom);
		}

		assert(other_indom->desc.indom == indom);

		/* Write out the label set records associated with this indom */
		write_priorlabelset(PM_LABEL_INDOM, indom, &stamp);
		write_priorlabelset(PM_LABEL_INSTANCES, indom, &stamp);

		/* Write out any help text records associated with this indom */
		write_textreclist(PM_TEXT_INDOM | PM_TEXT_ONELINE, indom);
		write_textreclist(PM_TEXT_INDOM | PM_TEXT_HELP, indom);
	    }
	}
    }
}

/* --- End of reclist functions --- */

void
checklogtime(__pmTimestamp *this, int indx)
{
    if ((curlog.sec == 0 && curlog.nsec == 0) ||
	(curlog.sec > this->sec ||
	(curlog.sec == this->sec && curlog.nsec > this->nsec))) {
	    ilog = indx;
	    curlog = *this;		/* struct assignment */
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
    int		type;			/* record type */
    int		want;
    int		numeof = 0;
    int		sts;
    pmID	pmid;			/* pmid for TYPE_DESC */
    pmInDom	indom;			/* indom for TYPE_INDOM* */
    __pmLogCtl	*lcp;			/* input archive */
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
	    dump_state();
	    abandon_extract();
	    /*NOTREACHED*/
	}
	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), iap->ctx);
	    abandon_extract();
	    /*NOTREACHED*/
	}
	/* Need to hold c_lock for pmaGetLog() */

	lcp = ctxp->c_archctl->ac_log;

	/* Note related to ml_skip[]
	 *   We try do some culling below if ml_skip[] is not empty,
	 *   but the order of metadata across archives means we may
	 *   emit some metadata and LATER ON determine the associated
	 *   pmid needs to be added to ml_skip[].  This is unfortunate
	 *   but not catastrophic, provided the corresponding pmid never
	 *   appears in an output __pmResult.
	 */

againmeta:
	/* get next meta record */

	if ((sts = pmaGetLog(ctxp->c_archctl, PM_LOG_VOL_META, &iap->pb[META])) < 0) {
	    iap->eof[META] = 1;
	    ++numeof;
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: pmaGetLog[meta %s]: %s\n",
			pmGetProgname(), iap->name, pmErrStr(sts));
		_report(lcp->mdfp);
		abandon_extract();
		/*NOTREACHED*/
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
		/* check merics from configfile */
		for (j=0; j<ml_numpmid; j++) {
		    if (pmid == ml[j].desc->pmid) {
			want = 1;
			break;
		    }
		}
	    }
	    if (want && skip_ml != NULL) {
		/* check not on skip list */
		for (j=0; j<skip_ml_numpmid; j++) {
		    if (pmid == skip_ml[j]) {
			want = 0;
			break;
		    }
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
	else if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA || type == TYPE_INDOM_V2) {
	    /*
	     * if ml is defined, then look for instance domain in the list
	     * if indom is not in the list then discard it immediately
	     */
	    if (type == TYPE_INDOM || type == TYPE_INDOM_DELTA)
		indom = ntoh_pmInDom(iap->pb[META][5]);
	    else
		indom = ntoh_pmInDom(iap->pb[META][4]);
	    want = 0;
	    if (ml == NULL)
	        want = 1;
	    else {
	        for (j=0; j<ml_numpmid; j++) {
		    if (indom == ml[j].desc->indom)
		        want = 1;
	        }
	    }

	    if (want) {
		/*
		 * add to indom list 
		 * append_indomreclist() sets pb[META] to NULL
		 * append_indomreclist() may unpin the pdu buffer
		 */
		if (type == TYPE_INDOM_DELTA) {
		    __pmLogInDom	*idp;
		    __pmLogInDom	lid;
		    __int32_t		*new;
		    int			lsts;
		    lid.indom = ntoh_pmInDom(iap->pb[META][5]);
		    idp = pmaUndeltaInDom(lcp, iap->pb[META]);
		    if (idp == NULL) {
			fprintf(stderr, "nextmeta: Botch: undelta indom failed for indom %s\n", pmInDomStr(lid.indom));
			abandon_extract();
			/*NOTREACHED*/
		    }
		    lid.stamp = idp->stamp;
		    lid.numinst = idp->numinst;
		    lid.instlist = idp->instlist;
		    lid.namelist = idp->namelist;
		    lid.alloc = 0;

		    lsts = __pmLogEncodeInDom(lcp, type, &lid, &new);
		    if (lsts < 0) {
			fprintf(stderr, "nextmeta: Botch: delta indom encode failed: %s\n", pmErrStr(lsts));
			abandon_extract();
			/*NOTREACHED*/
		    }
		    free(iap->pb[META]);
		    iap->pb[META] = new;
		    /* and now it is no longer in "delta" indom format */
		    iap->pb[META][1] = htonl(TYPE_INDOM);
		}
		else if (type == TYPE_INDOM_V2 && outarchvers == PM_LOG_VERS03) {
		    int			lsts;

		    lsts = pmaRewriteMeta(lcp, archctl.ac_log, &iap->pb[META]);
		    if (lsts < 0) {
			fprintf(stderr, "nextmeta: Botch: pmaRewriteMeta %s failed: %s\n", __pmLogMetaTypeStr(TYPE_INDOM_V2), pmErrStr(lsts));
			abandon_extract();
			/*NOTREACHED*/
		    }
		}
		append_indomreclist(indx);
	    }
	    else {
	        /* META: don't want this meta */
	        free(iap->pb[META]);
	        iap->pb[META] = NULL;
	        goto againmeta;
	    }
	}
	else if (type == TYPE_LABEL || type == TYPE_LABEL_V2) {
	    /* Decide which label sets we want to keep. */
	    want = 0;
	    if (ml == NULL) {
		/* ml is not defined, then all metrics and indoms are being kept.
		   Keep all label sets as well. */
	        want = 1;
	    }
	    else {
		int	k;
		int	ltype;			/* label type */
		if (type == TYPE_LABEL)
		    k = 5;
		else
		    k = 4;
		ltype = ntoh_pmLabelType(iap->pb[META][k++]);
		switch (ltype) {
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
		    pmid = ntoh_pmID(iap->pb[META][k]);
		    pmid = pmID_domain(pmid); /* Extract the domain */
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == pmID_domain(ml[j].desc->pmid))
			    want = 1;
		    }
		    break;
		case PM_LABEL_CLUSTER:
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][k]);
		    pmid = pmID_build(pmID_domain(pmid), pmID_cluster(pmid), 0);
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == pmID_build(pmID_domain(ml[j].desc->pmid),
					       pmID_cluster(ml[j].desc->pmid), 0))
			    want = 1;
		    }
		    break;
		case PM_LABEL_ITEM:
		    /*
		     * Keep only the label sets whose metrics also being kept.
		     */
		    pmid = ntoh_pmID(iap->pb[META][k]);
		    for (j=0; j<ml_numpmid; j++) {
			if (pmid == ml[j].desc->pmid)
			    want = 1;
		    }
		    break;
		case PM_LABEL_INDOM:
		case PM_LABEL_INSTANCES:
		    /*
		     * Keep only the label sets whose instance domains are also being kept.
		     * These are the domains of the metrics which are being kept.
		     */
		    indom = ntoh_pmInDom(iap->pb[META][k]);
		    for (j=0; j<ml_numpmid; j++) {
			if (indom == ml[j].desc->indom)
			    want = 1;
		    }
		    break;
		default:
		    fprintf(stderr, "%s: Error: invalid label set type: %d\n",
			    pmGetProgname(), ltype);
		    abandon_extract();
		    /*NOTREACHED*/
		}
	    }

	    if (want) {
		/*
		 * Add to label set list.
		 * append_labelsetreclist() sets pb[META] to NULL
		 */
		if (type == TYPE_LABEL_V2 && outarchvers == PM_LOG_VERS03) {
		    int			lsts;

		    lsts = pmaRewriteMeta(lcp, archctl.ac_log, &iap->pb[META]);
		    if (lsts < 0) {
			fprintf(stderr, "nextmeta: Botch: pmaRewriteMeta %s failed: %s\n", __pmLogMetaTypeStr(TYPE_LABEL_V2), pmErrStr(lsts));
			abandon_extract();
			/*NOTREACHED*/
		    }
		    type = TYPE_LABEL;
		}
		append_labelsetreclist(indx, type);
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
			if (pmid == ml[j].desc->pmid)
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
			if (indom == ml[j].desc->indom)
			    want = 1;
		    }
		}
		else {
		    fprintf(stderr, "%s: Error: invalid text type: %d\n",
			    pmGetProgname(), type);
		    abandon_extract();
		    /*NOTREACHED*/
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
	    /*NOTREACHED*/
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
    int			indx;
    int			eoflog = 0;	/* number of log files at eof */
    int			sts;
    __pmTimestamp	curtime;
    __pmArchCtl		*acp;
    __pmContext		*ctxp;
    inarch_t		*iap;


    for (indx=0; indx<inarchnum; indx++) {
	iap = &inarch[indx];

	/* if at the end of log file then skip this archive */
	if (iap->eof[LOG]) {
	    ++eoflog;
	    continue;
	}

	/* at eof, but not yet done <mark> record, nothing to do here */
	if (iap->mark)
	    continue;

	/* if we already have a log record then skip this archive */
	if (iap->_Nresult != NULL) {
	    continue;
	}


	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n", pmGetProgname(), iap->ctx);
	    abandon_extract();
	    /*NOTREACHED*/
	}
	/* Need to hold c_lock for __pmLogRead_ctx() */
	acp = ctxp->c_archctl;

againlog:
	if ((sts =__pmLogRead_ctx(ctxp, PM_MODE_FORW, NULL, &iap->_result, PMLOGREAD_NEXT)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: Error: __pmLogRead[log %s]: %s\n",
			pmGetProgname(), iap->name, pmErrStr(sts));
		_report(acp->ac_mfp);
		if (sts != PM_ERR_LOGREC)
		    abandon_extract();
		    /*NOTREACHED*/
	    }
	    /*
	     * if the first data record has not been written out, then do
	     * not generate a <mark> record, and you may as well ignore
	     * this archive, else get ready for a <mark> records
	     */
	    if (first_datarec) {
		iap->eof[LOG] = 1;
		++eoflog;
	    }
	    else {
		iap->mark = 1;
		iap->pb[LOG] = NULL;
	    }
	    PM_UNLOCK(ctxp->c_lock);
	    continue;
	}
	else
	    iap->laststamp = iap->_result->timestamp;	/* struct assignment */
	iap->recnum++;
	assert(iap->_result != NULL);

	/*
	 * set current log time - this is only done so that we can
	 * determine whether to keep or discard the log
	 */
	curtime = iap->_result->timestamp;

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
			    __pmPrintResult(stderr, iap->_result);
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
			    __pmPrintResult(stderr, iap->_result);
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
	if (__pmTimestampCmp(&curtime, &winstart) < 0) {
	    /*
	     * log is not in time window - discard result and get next record
	     */
	    __pmFreeResult(iap->_result);
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
            else if (ml == NULL && skip_ml == NULL) {
                /*
		 * ml is NOT defined and skip_ml[] is empty so, we want
		 * everything => use the input __pmResult
		 */
                iap->_Nresult = iap->_result;
            }
            else {
                /*
		 * need to search metric list for wanted pmid's and to
		 * omit any skipped pmid's
                 * searchmlist() may pick no metrics, this is OK
                 */
                iap->_Nresult = searchmlist(iap->_result);
            }

            if (iap->_Nresult == NULL) {
                /* dont want any of the metrics in _result, try again */
		__pmFreeResult(iap->_result);
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

	case 'V':	/* output archive version */
	    outarchvers = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || outarchvers < 0) {
		pmprintf("%s: -V requires numeric argument\n", pmGetProgname());
		opts.errors++;
	    }
	    if (outarchvers != PM_LOG_VERS03 && outarchvers != PM_LOG_VERS02) {
		pmprintf("%s: -V value must be 2 or 3\n", pmGetProgname());
		opts.errors++;
	    }
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

	case 'x':	/* ignore metrics with mismatched metadata */
	    xarg++;
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
checkwinend(__pmTimestamp *tsp)
{
    int			indx;
    int			sts;
    inarch_t		*iap;
    int			vers;
    __pmTimestamp	msec = { 0, 1000000 };		/* 1msec */


    if (winend.sec < 0 || __pmTimestampCmp(tsp, &winend) <= 0)
	return(0);

    /*
     * we have reached the end of a window
     *	- if warg is not set, then we have finished (break)
     *	- otherwise, calculate start and end of next window,
     *		     set pre_startwin, discard logs before winstart,
     * 		     and write out mark
     */
    if (!warg) {
	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "checkwinend: done: current ");
	    __pmPrintTimestamp(stderr, tsp);
	    fprintf(stderr, " after winend ");
	    __pmPrintTimestamp(stderr, &winend);
	    fputc('\n', stderr);
	}
	return(-1);
    }

    winstart.sec += NUM_SEC_PER_DAY;
    winend.sec += NUM_SEC_PER_DAY;
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "checkwinend: 24-hr roll window to ");
	__pmPrintTimestamp(stderr, &winstart);
	fprintf(stderr, " ... ");
	__pmPrintTimestamp(stderr, &winend);
	fputc('\n', stderr);
    }
    pre_startwin = 1;

    /*
     * if start of next window is later than max termination
     * then bail out here
     */
    if (__pmTimestampCmp(&winstart, &logend) > 0) {
	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "checkwinend: done: winstart ");
	    __pmPrintTimestamp(stderr, &winstart);
	    fprintf(stderr, " after logend ");
	    __pmPrintTimestamp(stderr, &logend);
	    fputc('\n', stderr);
	}
	return(-1);
    }

    ilog = -1;
    for (indx=0; indx<inarchnum; indx++) {

	iap = &inarch[indx];
	vers = (iap->label.magic & 0xff);
	if (iap->_Nresult != NULL) {
	    if (__pmTimestampCmp(&iap->_Nresult->timestamp, &winstart) < 0) {
		/* free _result and _Nresult */
		if (pmDebugOptions.appl2) {
		    fprintf(stderr, "checkwinend: inarch[%d]: last timestamp ", indx);
		    __pmPrintTimestamp(stderr, &iap->_Nresult->timestamp);
		    fprintf(stderr, " after winstart ");
		    __pmPrintTimestamp(stderr, &winstart);
		    fputc('\n', stderr);
		}
		if (iap->_result != iap->_Nresult) {
		    free(iap->_Nresult);
		}
		if (iap->_result != NULL) {
		    __pmFreeResult(iap->_result);
		    iap->_result = NULL;
		}
		iap->_Nresult = NULL;
		iap->pb[LOG] = NULL;
	    }
	}
	if (iap->pb[LOG] != NULL) {
	    __pmTimestamp	tmptime;
	    if (vers >= PM_LOG_VERS03)
		__pmLoadTimestamp(&iap->pb[LOG][3], &tmptime);
	    else
		__pmLoadTimeval(&iap->pb[LOG][3], &tmptime);
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "checkwinend: inarch[%d]: PDU timestamp ", indx);
		__pmPrintTimestamp(stderr, &tmptime);
		fputc('\n', stderr);
	    }
	    if (__pmTimestampCmp(&tmptime, &winstart) < 0) {
		/*
		 * free PDU buffer ... it is probably a mark
		 * and has not been pinned
		 */
		if (pmDebugOptions.appl2) {
		    fprintf(stderr, "checkwinend: inarch[%d]: PDU timestamp ", indx);
		    __pmPrintTimestamp(stderr, &tmptime);
		    fprintf(stderr, " after winstart ");
		    __pmPrintTimestamp(stderr, &winstart);
		    fputc('\n', stderr);
		}
		free(iap->pb[LOG]);
		iap->pb[LOG] = NULL;
	    }
	}
    }

    /*
     * after 24-hr window roll we must create a <mark> record and
     * write it out
     */
    if ((sts = __pmLogWriteMark(&archctl, &current, &msec)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogWriteMark: log data: %s\n",
		pmGetProgname(), pmErrStr(sts));
	abandon_extract();
	/*NOTREACHED*/
    }
    __pmTimestampInc(&current, &msec);
    written++;
    return(1);
}


void
writerlist(inarch_t *iap, rlist_t **rlready, __pmTimestamp *mintime)
{
    int			sts;
    int			needti = 0;	/* need to flush/update */
    __pmTimestamp	titime  = {0,0};/* time of last temporal index write */
    __pmTimestamp	restime;	/* time of result */
    rlist_t		*elm;		/* element of rlready to be written out */
    __int32_t		*pb;		/* pdu buffer */
    __uint64_t		max_offset;
    unsigned long	peek_offset;

    max_offset = (outarchvers == PM_LOG_VERS02) ? 0x7fffffff : LONGLONG_MAX;

    while (*rlready != NULL) {
	restime = (*rlready)->res->timestamp;
        if (__pmTimestampCmp(&restime, mintime) > 0) {
	    if (pmDebugOptions.appl1) {
		fprintf(stderr, "writerlist: restime %" FMT_INT64 ".%09d mintime %" FMT_INT64 ".%09d ", restime.sec, restime.nsec, mintime->sec, mintime->nsec);
		fprintf(stderr, " break!\n");
	    }
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
	    logctl.label.start = elm->res->timestamp;
            logctl.state = PM_LOG_STATE_INIT;
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

	/* We need to write out the relevant context labels if any. */
	write_priorlabelset(PM_LABEL_CONTEXT, PM_IN_NULL, mintime);

	/* write out the descriptor and instance domain pdu's first */
	write_metareclist(iap, elm->res, &needti);

	/* convert log record to a pdu */
	sts = __pmEncodeResult(&logctl, elm->res, (__pmPDU **)&pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	    /*NOTREACHED*/
	}

        /* switch volumes if required */
        if (varg > 0) {
            if (written > 0 && (written % varg) == 0) {
		__pmTimestamp	stamp;
		if (outarchvers == PM_LOG_VERS03)
		    __pmLoadTimestamp(&pb[3], &stamp);
		else
		    __pmLoadTimeval(&pb[3], &stamp);
                newvolume(outarchname, &stamp);
	    }
        }
	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes (for v2 archives)
	 * or 2^63-1 bytes (for v3 archives and beyond).
	 */
	peek_offset = __pmFtell(archctl.ac_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > max_offset) {
	    __pmTimestamp	stamp;
	    if (outarchvers == PM_LOG_VERS03)
		__pmLoadTimestamp(&pb[3], &stamp);
	    else
		__pmLoadTimeval(&pb[3], &stamp);
	    newvolume(outarchname, &stamp);
	}

	/* write out log record */
	old_log_offset = __pmFtell(archctl.ac_mfp);
	assert(old_log_offset >= 0);
	sts = (outarchvers == PM_LOG_VERS02) ?
		__pmLogPutResult2(&archctl, (__pmPDU *)pb) :
		__pmLogPutResult3(&archctl, (__pmPDU *)pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult: log data: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    abandon_extract();
	    /*NOTREACHED*/
	}
	written++;


	/* check whether we need to write TI (temporal index) */
	if (old_log_offset == 0 ||
	    old_log_offset == __pmLogLabelSize(&logctl) ||
	    __pmFtell(archctl.ac_mfp) > flushsize)
		needti = 1;

	/*
	 * make sure that we do not write out the temporal index more
	 * than once for the same timestamp
	 */
	if (needti && __pmTimestampCmp(&titime, &restime) >= 0)
	    needti = 0;

	/* flush/update */
	if (needti) {
	    titime = restime;

	    __pmFflush(archctl.ac_mfp);
	    __pmFflush(logctl.mdfp);

	    if (old_log_offset == 0)
		old_log_offset = __pmLogLabelSize(&logctl);

            new_log_offset = __pmFtell(archctl.ac_mfp);
	    assert(new_log_offset >= 0);
            new_meta_offset = __pmFtell(logctl.mdfp);
	    assert(new_meta_offset >= 0);

            __pmFseek(archctl.ac_mfp, (long)old_log_offset, SEEK_SET);
            __pmFseek(logctl.mdfp, (long)old_meta_offset, SEEK_SET);

            __pmLogPutIndex(&archctl, &restime);

            __pmFseek(archctl.ac_mfp, (long)new_log_offset, SEEK_SET);
            __pmFseek(logctl.mdfp, (long)new_meta_offset, SEEK_SET);

            old_log_offset = __pmFtell(archctl.ac_mfp);
	    assert(old_log_offset >= 0);
            old_meta_offset = __pmFtell(logctl.mdfp);
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

static int
do_not_need_mark(inarch_t *iap)
{
    int			indx, j;
    __pmTimestamp	tstamp;
    __pmTimestamp	smallest_tstamp;

    if (old_mark_logic || iap->pmcd_pid == -1 || iap->pmcd_seqnum == -1)
	/* no epilogue/prologue for me ... */
	return 0;

    j = -1;
    smallest_tstamp.sec = INT64_MAX;
    smallest_tstamp.nsec = 999999999;
    for (indx=0; indx<inarchnum; indx++) {
	if (&inarch[indx] == iap)
	    continue;
	if (inarch[indx]._result != NULL) {
	    tstamp = inarch[indx]._result->timestamp;
	    if (tstamp.sec < smallest_tstamp.sec || 
		(tstamp.sec == smallest_tstamp.sec && tstamp.nsec < smallest_tstamp.nsec)) {
		j = indx;
		smallest_tstamp = tstamp;
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
    int			indx;
    int			j;
    int			sts;
    int			stslog;		/* sts from nextlog() */
    int			stsmeta;	/* sts from nextmeta() */
    int			nempty = 0;	/* number of empty input archives */

    char	*msg;

    __pmTimestamp	now = {0,0};	/* the current time */
    __pmTimestamp	mintime = {0,0};
    __pmTimestamp	tmptime = {0,0};

    inarch_t		*iap;		/* ptr to archive control */
    rlist_t		*rlready = NULL;	/* results ready for writing */

    struct timeval	unused;
    __pmTimestamp	end;

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
    if (pmDebugOptions.appl1) {
        totalmalloc += (inarchnum * sizeof(inarch_t));
        fprintf(stderr, "main        : allocated %d\n",
			(int)(inarchnum * sizeof(inarch_t)));
    }


    for (indx=0; indx<inarchnum; indx++, opts.optind++) {
	__pmContext	*ctxp;

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
	    if (iap->ctx == PM_ERR_NODATA) {
		fprintf(stderr, "%s: Warning: empty archive \"%s\" will be skipped\n",
			pmGetProgname(), iap->name);
		iap->eof[LOG] = iap->eof[META] = 1;
		nempty++;
		continue;
	    }
	    else {
		fprintf(stderr, "%s: Error: cannot open archive \"%s\": %s\n",
			pmGetProgname(), iap->name, pmErrStr(iap->ctx));
		exit(1);
	    }
	}

	if ((sts = pmUseContext(iap->ctx)) < 0) {
	    fprintf(stderr, "%s: Error: cannot use context (%s): %s\n",
		    pmGetProgname(), iap->name, pmErrStr(sts));
	    exit(1);
	}

	if ((ctxp = __pmHandleToPtr(iap->ctx)) == NULL) {
	    fprintf(stderr, "%s: setup botch: __pmHandleToPtr(%s) returns NULL!\n", pmGetProgname(), iap->name);
	    abandon_extract();
	    /*NOTREACHED*/
	}

	memset((void *)&iap->label, 0, sizeof(iap->label));
	if ((sts = __pmLogLoadLabel(ctxp->c_archctl->ac_log->mdfp, &iap->label)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmGetProgname(), iap->name, pmErrStr(sts));
	    exit(1);
	}

	/*
	 * Note: This application is single threaded, and once we have ctxp
	 *       the associated __pmContext will not move and will only be
	 *       accessed or modified synchronously either here or in libpcp.
	 *       We unlock the context so that it can be locked as required
	 *       within libpcp.
	 */
	PM_UNLOCK(ctxp->c_lock);

	if ((sts = __pmGetArchiveEnd(ctxp->c_archctl, &end)) < 0) {
	    fprintf(stderr, "%s: Error: cannot get end of archive (%s): %s\n",
		pmGetProgname(), iap->name, pmErrStr(sts));
	    if (desperate) {
		end.sec = INT64_MAX;
		end.nsec = 0;
	    }
	    else
		exit(1);
	}

	if (indx == 0) {
	    /* start time */
	    logstart = iap->label.start;	/* struct assignment */
	    logstart_tv.tv_sec = iap->label.start.sec;
	    logstart_tv.tv_usec = iap->label.start.nsec / 1000;
	    /* end time */
	    logend = end;			/* struct assignment */
	    logend_tv.tv_sec = end.sec;
	    logend_tv.tv_usec = end.nsec / 1000;
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "[%d] intial set log* ", indx);
		__pmPrintTimestamp(stderr, &logstart);
		fprintf(stderr, " ... ");
		__pmPrintTimestamp(stderr, &logend);
		fputc('\n', stderr);
	    }

	}
	else {
	    /* get the earlier start time */
	    if (logstart.sec > iap->label.start.sec ||
		(logstart.sec == iap->label.start.sec &&
		logstart.nsec > iap->label.start.nsec)) {
		    logstart = iap->label.start;	/* struct assignment */
		    if (pmDebugOptions.appl2) {
			fprintf(stderr, "[%d] set logstart ", indx);
			__pmPrintTimestamp(stderr, &logstart);
			fputc('\n', stderr);
		    }
	    }

	    /* get the later end time */
	    if (logend.sec < end.sec ||
		(logend.sec == end.sec &&
		logend.nsec < end.nsec)) {
		    logend = end;		/* struct assignment */
		    if (pmDebugOptions.appl2) {
			fprintf(stderr, "[%d] set logend ", indx);
			__pmPrintTimestamp(stderr, &logend);
			fputc('\n', stderr);
		    }
	    }
	}
    } /*for(indx)*/

    if (nempty == inarchnum) {
	fprintf(stderr, "%s: Warning: all input archive(s) are empty, no output archive created\n",
		pmGetProgname());
	exit(1);
    }

    logctl.label.start = logstart;		/* struct assignment */

    /*
     * process config file
     *	- this includes a list of metrics and their instances
     */
    if (configfile && parseconfig() < 0)
	exit(1);

    if (zarg) {
	/* use TZ from metrics source (first non-empty input archive) */
	for (indx=0; indx<inarchnum; indx++) {
	    if (inarch[indx].ctx != PM_ERR_NODATA) {
		if ((sts = pmNewZone(inarch[indx].label.timezone)) < 0) {
		    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
			    pmGetProgname(), pmErrStr(sts));
		    exit_status = 1;
		    goto cleanup;
		}
		printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n", inarch[indx].label.hostname);
		break;
	    }
	}
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
	char	*local_tz;
        local_tz = __pmTimezone();
	/* use TZ from local host */
	if ((sts = pmNewZone(local_tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set local host's timezone: %s\n",
		    pmGetProgname(), pmErrStr(sts));
	    exit_status = 1;
	    goto cleanup;
	}
    }


    /* create output log - must be done before writing label */
    archctl.ac_log = &logctl;
    if ((sts = __pmLogCreate("", outarchname, outarchvers, &archctl, 0)) < 0) {
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
			    &logstart_tv, &logend_tv,
			    &winstart_tv, &winend_tv, &unused, &msg);
    if (sts < 0) {
	fprintf(stderr, "%s: Invalid time window specified: %s\n",
		pmGetProgname(), msg);
	abandon_extract();
	/*NOTREACHED*/
    }
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "pmParseTimeWindow -> %d win*_tv ", sts);
	pmPrintStamp(stderr, &winstart_tv);
	fprintf(stderr, " ... ");
	pmPrintStamp(stderr, &winend_tv);
	fprintf(stderr, " log*_tv ");
	pmPrintStamp(stderr, &logstart_tv);
	fprintf(stderr, " ... ");
	pmPrintStamp(stderr, &logend_tv);
	fputc('\n', stderr);
    }
    if (Sarg != NULL || Aarg != NULL || Oarg != NULL) {
	winstart.sec = winstart_tv.tv_sec;
	winstart.nsec = winstart_tv.tv_usec * 1000;
    }
    if (Targ != NULL || Aarg != NULL) {
	winend.sec = winend_tv.tv_sec;
	winend.nsec = winend_tv.tv_usec * 1000;
	/*
	 * add 1 to winend.sec to dodge truncation in the conversion
	 * from usec to nsec ... without this we risk missing the very
	 * the last input data record and (less likely) the last indom
	 * metadata record
	 */
	winend.sec++;
    }
    if (pmDebugOptions.appl2) {
	fprintf(stderr, "after arg processing: win* ");
	__pmPrintTimestamp(stderr, &winstart);
	fprintf(stderr, " ... ");
	__pmPrintTimestamp(stderr, &winend);
	fputc('\n', stderr);
    }

    if (warg) {
	if (winstart.sec + NUM_SEC_PER_DAY < winend.sec) {
	    fprintf(stderr, "%s: Warning: -S and -T must specify a time window within\nthe same day, for -w to be used.  Ignoring -w flag.\n", pmGetProgname());
	    warg = 0;
	}
    }

    ilog = -1;
    written = 0;
    curlog.sec = 0;
    curlog.nsec = 0;
    current.sec = 0;
    current.nsec = 0;
    first_datarec = 1;
    pre_startwin = 1;

    /*
     * get all meta data first
     * nextmeta() should return 0 (will return -1 when all meta is eof)
     */
    do {
	stsmeta = nextmeta();
    } while (stsmeta >= 0);

    if (skip_ml_numpmid > 0) {
	fprintf(stderr, "Warning: the metrics below will be missing from the output archive\n");
	for (j=0; j<skip_ml_numpmid; j++) {
	    fprintf(stderr, "\tPMID: %s\n", pmIDStr(skip_ml[j]));
	}
    }

    /*
     * get log record - choose one with earliest timestamp
     * write out meta data (required by this log record)
     * write out log
     * do ti update if necessary
     */
    while (sarg == -1 || written < sarg) {
	ilog = -1;
	curlog.sec = 0;
	curlog.nsec = 0;
	old_meta_offset = __pmFtell(logctl.mdfp);
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
	mintime.sec = mintime.nsec = 0;
	for (indx=0; indx<inarchnum; indx++) {
	    if (inarch[indx]._Nresult != NULL) {
		checklogtime(&inarch[indx]._Nresult->timestamp, indx);
		if (pmDebugOptions.appl2) {
		    fprintf(stderr, "result [%d] stamp ", indx);
		    __pmPrintTimestamp(stderr, &inarch[indx]._Nresult->timestamp);
		    fputc('\n', stderr);
		}

		if (ilog == indx) {
		    tmptime = curlog;
		    if (mintime.sec <= 0 || __pmTimestampCmp(&mintime, &tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	    else if (inarch[indx].mark) {
		checklogtime(&inarch[indx].laststamp, indx);
		if (pmDebugOptions.appl2) {
		    fprintf(stderr, "mark [%d] ", indx);
		    __pmPrintTimestamp(stderr, &inarch[indx].laststamp);
		    if (indx == ilog)
			fprintf(stderr, " (ilog)");
		    fputc('\n', stderr);
		}

		if (ilog == indx) {
		    tmptime = curlog;
		    if (mintime.sec <= 0 || __pmTimestampCmp(&mintime, &tmptime) > 0)
		        mintime = tmptime;
		}
	    }
	}

	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "pick [%d] curlog ", ilog);
	    __pmPrintTimestamp(stderr, &curlog);
	    fprintf(stderr, " mintime ");
	    __pmPrintTimestamp(stderr, &mintime);
	    fputc('\n', stderr);

	}

	/*
	 * now     == the earliest timestamp of the archive(s)
	 *		and/or mark records
	 * mintime == now or timestamp of the earliest mark
	 *		(whichever is smaller)
	 */
	now = curlog;

	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "done? now ");
	    __pmPrintTimestamp(stderr, &now);
	    fprintf(stderr, " > logend ");
	    __pmPrintTimestamp(stderr, &logend);
	    fprintf(stderr, "?\n");

	}

	/*
	 * note - mark (after last archive) will be setup, but this
	 * break, will prevent it from being written out
	 */
	if (__pmTimestampCmp(&now, &logend) > 0) {
	    if (pmDebugOptions.appl2) {
		fprintf(stderr, "done now ");
		__pmPrintTimestamp(stderr, &now);
		fprintf(stderr, " > logend ");
		__pmPrintTimestamp(stderr, &logend);
		fprintf(stderr, "?\n");
	    }
	    break;
	}

	sts = checkwinend(&now);
	if (sts < 0)
	    break;
	if (sts > 0)
	    continue;

	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "update current from ");
	    __pmPrintTimestamp(stderr, &current);
	    fprintf(stderr, " to ");
	    __pmPrintTimestamp(stderr, &curlog);
	    fputc('\n', stderr);

	}
	current = curlog;

	/* prepare to write out log record */
	if (ilog < 0 || ilog >= inarchnum) {
	    fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
	    fprintf(stderr, "    log file index = %d\n", ilog);
	    dump_state();
	    abandon_extract();
	    /*NOTREACHED*/
	}


	iap = &inarch[ilog];
	if (iap->mark) {
	    if (do_not_need_mark(iap)) {
		;
	    }
	    else {
		/*
		 * if we are NOT the last input archive, then output
		 * a <mark> record
		 */
		 int	i;
		 int	last = 1;
		 for (i = 0; i < inarchnum; i++) {
		    if (i == ilog)
			continue;
		    if (!inarch[i].eof[LOG]) {
			last = 0;
			break;
		    }
		}
		if (!last) {
		    /*
		     * really write <mark> after end of an input archive
		     */
		    __pmTimestamp	msec = { 0, 1000000 };		/* 1msec */
		    __pmTimestampInc(&iap->laststamp, &msec);
		    if ((sts = __pmLogWriteMark(&archctl, &iap->laststamp, NULL)) < 0) {
			fprintf(stderr, "%s: __pmLogWriteMark failed: %s\n",
				pmGetProgname(), pmErrStr(sts));
			exit_status = 1;
			goto cleanup;
		    }
		    current = iap->laststamp;
		    written++;
		}
	    }

	    /* once mark has been processes, then log is at EOF */
	    iap->mark = 0;
	    iap->eof[LOG] = 1;
	}
	else {
	    /* result is to be written out, but there is no _Nresult */
	    if (iap->_Nresult == NULL) {
		fprintf(stderr, "%s: Fatal Error!\n", pmGetProgname());
		fprintf(stderr, "    pick == LOG and _Nresult = NULL\n");
		dump_state();
		abandon_extract();
		/*NOTREACHED*/
	    }
	    insertresult(&rlready, iap->_Nresult);
	    if (pmDebugOptions.appl1) {
		rlist_t		*rp;
		int		i;

		fprintf(stderr, "rlready");
		for (i = 0, rp = rlready; rp != NULL; i++, rp = rp->next) {
		    fprintf(stderr, " [%d] t=%ld.%09d numpmid=%d", i, (long)rp->res->timestamp.sec, (int)rp->res->timestamp.nsec, rp->res->numpmid);
		}
		fprintf(stderr, " now=%" FMT_INT64 ".%09d\n", now.sec, now.nsec);
	    }

	    writerlist(iap, &rlready, &curlog);

	    /*
	     * writerlist frees elm (elements of rlready) but does not
	     * free _result & _Nresult
	     *
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
		__pmFreeResult(iap->_result);
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
	/*NOTREACHED*/
    }
    else {
	/* write the last time stamp */
	__pmFflush(archctl.ac_mfp);
	__pmFflush(logctl.mdfp);

	if (old_log_offset == 0)
	    old_log_offset = __pmLogLabelSize(&logctl);

	new_log_offset = __pmFtell(archctl.ac_mfp);
	assert(new_log_offset >= 0);
	new_meta_offset = __pmFtell(logctl.mdfp);
	assert(new_meta_offset >= 0);

	if (pmDebugOptions.appl2) {
	    fprintf(stderr, "*** last tstamp: \n\tmintime=%" FMT_INT64 ".%09d \n\ttmptime=%" FMT_INT64 ".%09d \n\tlogend=%" FMT_INT64 ".%09d \n\twinend=%" FMT_INT64 ".%09d \n\tcurrent=%" FMT_INT64 ".%09d\n",
		mintime.sec, mintime.nsec, tmptime.sec, tmptime.nsec, logend.sec, logend.nsec, winend.sec, winend.nsec, current.sec, current.nsec);
	}

	__pmFseek(archctl.ac_mfp, old_log_offset, SEEK_SET);
	__pmLogPutIndex(&archctl, &current);

	/* need to fix up label with new start-time */
	writelabel_metati(1);
    }
    if (pmDebugOptions.appl1) {
        fprintf(stderr, "main        : total allocated %ld\n", totalmalloc);
    }

    exit(exit_status);
}
