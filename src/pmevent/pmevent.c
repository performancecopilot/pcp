/*
 * pmevent - event record dumper
 * (based on pmval)
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
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
 * TODO
 *   +	-g and -p - nothing has been checked
 *   +	>1 metricname in command line and associated semantic checks, esp
 *   +	over same indom on PM_INDOM_NULL
 *   +	add -L for explicit local context
 *   +	semantic checks between -a, -h, -L and pmParseMetricSpec results
 *   +	semantic checks between -i and pmParseMetricSpec results
 *   +	split into multiple source files, add pmevent.h, ...
 *   +	drop -x (this is the default and only beahviour)
 *   +	check type is PM_TYPE_EVENT for all cmd line args
 *   +	conditional pmGetInDomArchive() in lookup()
 *   +	add proper cache for metrics found _within_ event records, to
 *   	avoid calls to pmNameID and pmLookupDesc in mydump()
 */

#include <math.h>
#include "pmapi.h"
#include "impl.h"
#include "pmtime.h"

/***************************************************************************
 * constants
 ***************************************************************************/

#define ALL_SAMPLES	-1

static char *options = "A:a:D:gh:i:K:n:O:p:s:S:t:T:U:w:xzZ:?";
static char usage[] =
    "Usage: %s [options] metricname ...\n\n"
    "Options:\n"
    "  -A align      align sample times on natural boundaries\n"
    "  -a archive    metrics source is a PCP archive\n"
    "  -g            start in GUI mode with new time control\n"
    "  -h host       metrics source is PMCD on host\n"
    "  -i instance   metric instance or list of instances - elements in a\n"
    "                list are separated by commas or whitespace\n"
    "  -K spec       optional additional PMDA spec for local connection\n"
    "                spec is of the form op,domain,dso-path,init-routine\n"
    "  -O offset     initial offset into the reporting time window\n"
    "  -p port       port number for connection to existing time control\n"
    "  -S starttime  start of the reporting time window\n"
    "  -s samples    terminate after this many samples\n"
    "  -T endtime    end of the reporting time window\n"
    "  -t interval   sample interval [default 1 second]\n"
    "  -x            expand event records\n"
    "  -Z timezone   set reporting timezone\n"
    "  -z            set reporting timezone to local timezone of metrics source\n";

/* performance metric control */
typedef struct {
    char	*pmname;	/* name of metric */
    pmID	pmid;		/* metric identifier */
    int		iall;		/* all instances */
    int		inum;		/* number of instances */
    char	**inames;	/* list of instance names */
    int		*iids;		/* list of instance ids */
    pmDesc	desc;		/* metric description */
} Control;


/***************************************************************************
 * Globals
 ***************************************************************************/

static char		*host;				/* original host */
static char		*archive = NULL;		/* archive source */
static pmLogLabel	label;
static char		*rpt_tz;
static int		ahtype = PM_CONTEXT_HOST;	/* archive or host? */
static int		amode = PM_MODE_FORW;		/* archive scan mode */
static char		local[] = "localhost";
static int		gui;
static int		port = -1;
static long		samples;			/* number of samples */
static pmTime		*pmtime;
static pmTimeControls	controls;
static struct timeval	last = {INT_MAX, 999999};	/* end time for log */
static int		xflag;				/* for -x */

/* Initialize API and fill in internal description for given Control. */
static void
initapi(Control *cp)
{
    int e;

    if ((e = pmLookupName(1, &(cp->pmname), &(cp->pmid))) < 0) {
        fprintf(stderr, "%s: pmLookupName(%s): %s\n", pmProgname, cp->pmname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if ((e = pmLookupDesc(cp->pmid, &(cp->desc))) < 0) {
        fprintf(stderr, "%s: pmLookupDesc: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }
}

/* Fetch metric values. */
static int
getvals(Control *cp,		/* in - full pm description */
        pmResult **vs)		/* alloc - pm values */
{
    pmResult	*r;
    int		e;
    int		i;

    if (archive != NULL) {
	/*
	 * for archives read until we find a pmResult with a
	 * pmid we are after
	 */
	for ( ; ; ) {
	    e = pmFetchArchive(&r);
	    if (e < 0)
		break;

	    if (r->numpmid == 0)
		/* skip mark records */
		continue;

	    for (i = 0; i < r->numpmid; i++) {
		if (r->vset[i]->pmid == cp->pmid)
		    break;
	    }
	    if (i != r->numpmid)
		break;
	    pmFreeResult(r);
	}
    }
    else {
	e = pmFetch(1, &(cp->pmid), &r);
	i = 0;
    }

    if (e < 0) {
	if (e == PM_ERR_EOL && gui) {
	    pmTimeStateBounds(&controls, pmtime);
	    return -1;
	}
	if (archive == NULL)
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(e));
	else
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(e));
        exit(EXIT_FAILURE);
    }

    if (gui)
	pmTimeStateAck(&controls, pmtime);

    if ((double)r->timestamp.tv_sec + (double)r->timestamp.tv_usec/1000000 >
	(double)last.tv_sec + (double)last.tv_usec/1000000) {
	return -2;
    }

    if (r->vset[i]->numval == 0) {
	if (gui || archive != NULL) {
	    __pmPrintStamp(stdout, &r->timestamp);
	    printf("  ");
	}
	printf("No values available\n");
	return -1;
    }
    else if (r->vset[i]->numval < 0) {
	if (archive == NULL)
	    fprintf(stderr, "\n%s: pmFetch: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	else
	    fprintf(stderr, "\n%s: pmFetchArchive: %s\n", pmProgname, pmErrStr(r->vset[i]->numval));
	return -1;
    }

    *vs = r;

    return i;
}

static void
timestep(struct timeval delta)
{
    /* time moved, may need to wait for previous value again */
}


/***************************************************************************
 * output
 ***************************************************************************/

/* Print parameter values as output header. */
static void
printhdr(Control *cp, struct timeval delta, struct timeval first)
{
    char		timebuf[26];

    if (archive == NULL)
	printf("host:      %s\n", host);
    else {
	printf("archive:   %s\n", archive);
	printf("host:      %s\n", label.ll_hostname);
	printf("start:     %s", pmCtime(&first.tv_sec, timebuf));
	if (last.tv_sec != INT_MAX)
	    printf("end:       %s", pmCtime(&last.tv_sec, timebuf));
    }

    /* sample count and interval */
    if (samples == ALL_SAMPLES) printf("samples:   all\n");
    else printf("samples:   %ld\n", samples);
    if (samples != ALL_SAMPLES && samples > 1 &&
	(ahtype != PM_CONTEXT_ARCHIVE || amode == PM_MODE_INTERP))
	printf("interval:  %1.2f sec\n", __pmtimevalToReal(&delta));
}

/*
 * cache all of the most recently requested
 * pmInDom ...
 */
static char *
lookup(pmInDom indom, int inst)
{
    static pmInDom	last = PM_INDOM_NULL;
    static int		numinst = -1;
    static int		*instlist;
    static char		**namelist;
    int			i;

    if (indom != last) {
	if (numinst > 0) {
	    free(instlist);
	    free(namelist);
	}
	numinst = pmGetInDom(indom, &instlist, &namelist);
	last = indom;
    }

    for (i = 0; i < numinst; i++) {
	if (instlist[i] == inst)
	    return namelist[i];
    }

    return NULL;
}

static void myeventdump(pmValueSet *vsp);

static void
mydump(pmDesc *dp, pmValueSet *vsp, char *indent)
{
    int		j;
    char	*p;

    if (indent != NULL)
	printf("%s", indent);
    if (vsp->numval == 0) {
	printf("No value(s) available!\n");
	return;
    }
    else if (vsp->numval < 0) {
	printf("Error: %s\n", pmErrStr(vsp->numval));
	return;
    }

    for (j = 0; j < vsp->numval; j++) {
	pmValue	*vp = &vsp->vlist[j];
	if (dp->indom != PM_INDOM_NULL) {
	    if ((p = lookup(dp->indom, vp->inst)) == NULL)
		printf("    inst [%d]", vp->inst);
	    else
		printf("    inst [%d or \"%s\"]", vp->inst, p);
	}
	else
	    printf("   ");
	printf(" value ");
	pmPrintValue(stdout, vsp->valfmt, dp->type, vp, 1);
	putchar('\n');
	if (dp->type == PM_TYPE_EVENT && xflag)
	    myeventdump(vsp);
    }
}

static void
myeventdump(pmValueSet *vsp)
{
    int		r;		/* event records */
    int		p;		/* event parameters */
    int		nrecords;
    int		flags;
    pmResult	**res;
    static pmID	pmid_flags;
    static pmID	pmid_missed;

    nrecords = pmUnpackEventRecords(vsp, &res);
    if (nrecords < 0) {
	fprintf(stderr, "pmUnpackEventRecords: %s\n", pmErrStr(nrecords));
	return;
    }
    printf(" %d event records\n", nrecords);

    if (pmid_flags == 0) {
	/*
	 * get PMID for event.flags and event.missed
	 * note that pmUnpackEventRecords() will have called
	 * __pmRegisterAnon(), so the anonymous metrics
	 * should now be in the PMNS
	 */
	char	*name_flags = "event.flags";
	char	*name_missed = "event.missed";
	int	sts;

	sts = pmLookupName(1, &name_flags, &pmid_flags);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_flags, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_flags)->item = 1;
	}
	sts = pmLookupName(1, &name_missed, &pmid_missed);
	if (sts < 0) {
	    /* should not happen! */
	    fprintf(stderr, "Warning: cannot get PMID for %s: %s\n",
			name_missed, pmErrStr(sts));
	    /* avoid subsequent warnings ... */
	    __pmid_int(&pmid_missed)->item = 1;
	}
    }

    for (r = 0; r < nrecords; r++) {
	__pmPrintStamp(stdout, &res[r]->timestamp);
	printf(" --- event record [%d] ", r);
	if (res[r]->numpmid == 0) {
	    printf(" ---\n");
	    printf("	No parameters\n");
	    continue;
	}
	if (res[r]->numpmid < 0) {
	    printf(" ---\n");
	    printf("	Error: illegal number of parameters (%d)\n",
			res[r]->numpmid);
	    continue;
	}
	flags = 0;
	for (p = 0; p < res[r]->numpmid; p++) {
	    pmValueSet	*xvsp = res[r]->vset[p];
	    int		sts;
	    pmDesc	desc;
	    char	*name;

	    if (pmNameID(xvsp->pmid, &name) >= 0) {
		if (p == 0) {
		    if (xvsp->pmid == pmid_flags) {
			flags = xvsp->vlist[0].value.lval;
			printf(" flags 0x%x", flags);
			printf(" (%s) ---\n", pmEventFlagsStr(flags));
			free(name);
			continue;
		    }
		    else
			printf(" ---\n");
		}
		if ((flags & PM_EVENT_FLAG_MISSED) &&
		    (p == 1) &&
		    (xvsp->pmid == pmid_missed)) {
		    printf("    ==> %d missed event records\n",
				xvsp->vlist[0].value.lval);
		    free(name);
		    continue;
		}
		printf("    %s\n", name);
		free(name);
	    }
	    else
		printf("	PMID: %s\n", pmIDStr(xvsp->pmid));
	    if ((sts = pmLookupDesc(xvsp->pmid, &desc)) < 0) {
		printf("	pmLookupDesc: %s\n", pmErrStr(sts));
		continue;
	    }
	    mydump(&desc, xvsp, "    ");
	}
    }
    if (nrecords >= 0)
	pmFreeEventResult(res);
}


/***************************************************************************
 * command line processing
 ***************************************************************************/

#define WHITESPACE ", \t\n"

static int
isany(char *p, char *set)
{
    if (p != NULL && *p) {
	while (*set) {
	    if (*p == *set)
		return 1;
	    set++;
	}
    }
    return 0;
}

/*
 * like strtok, but smarter
 */
static char *
getinstance(char *p)
{
    static char	*save;
    char	quot;
    char	*q;
    char	*start;

    if (p == NULL)
	q = save;
    else
	q = p;
    
    while (isany(q, WHITESPACE))
	q++;

    if (*q == '\0')
	return NULL;
    else if (*q == '"' || *q == '\'') {
	quot = *q;
	start = ++q;

	while (*q && *q != quot)
	    q++;
	if (*q == quot)
	    *q++ = '\0';
    }
    else {
	start = q;
	while (*q && !isany(q, WHITESPACE))
	    q++;
    }
    if (*q)
	*q++ = '\0';
    save = q;

    return start;
}

/* extract command line arguments - exits on error */
static void
getargs(int		argc,		/* in - command line argument count */
        char		*argv[],	/* in - argument strings */
        Control		*cp,		/* out - full pm description */
        struct timeval	*posn,		/* out - first sample time */
        struct timeval	*delta)
{
    int			c;
    char		*subopt;
    long		d;
    int			errflag = 0;
    int			i;
    int			src = 0;
    int			sts;
    char		*endnum;
    char		*errmsg;
    char		*Sflag = NULL;		/* argument of -S flag */
    char		*Tflag = NULL;		/* argument of -T flag */
    char		*Aflag = NULL;		/* argument of -A flag */
    char		*Oflag = NULL;		/* argument of -O flag */
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */
    int			tzh;			/* initial timezone handle */
    struct timeval	logStart;
    struct timeval	first;
    pmMetricSpec	*msp;
    char		*msg;

    /* fill in default values */
    cp->iall = 1;
    cp->inum = 0;
    cp->inames = NULL;
    delta->tv_sec = 1;
    delta->tv_usec = 0;
    samples = ALL_SAMPLES;
    host = local;

    /* extract command-line arguments */
    while ((c = getopt(argc, argv, options)) != EOF) {
	switch (c) {

	case 'A':		/* sample alignment */
	    Aflag = optarg;
	    break;

	case 'a':		/* interpolate archive */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    archive = optarg;
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

	case 'g':
	    gui = 1;
	    break;

	case 'h':		/* host name */
	    if (++src > 1) {
		fprintf(stderr, "%s: at most one of -a and -h allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    break;

	case 'i':		/* instance names */
	    cp->iall = 0;
	    i = cp->inum;
	    subopt = getinstance(optarg);
	    while (subopt != NULL) {
		i++;
		cp->inames =
		    (char **)realloc(cp->inames, i * (sizeof (char *)));
		if (cp->inames == NULL) {
		    __pmNoMem("pmval.ip", i * sizeof(char *), PM_FATAL_ERR);
		}
		*(cp->inames + i - 1) = subopt;
		subopt = getinstance(NULL);
	    }
	    cp->inum = i;
	    break;

	case 'K':	/* update local PMDA table */
	    if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: __pmSpecLocalPMDA failed\n%s\n", pmProgname, errmsg);
		errflag++;
	    }
	    break;

	case 'O':		/* sample offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* port for slave of existing time control */
	    port = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || port < 0) {
		fprintf(stderr, "%s: Error: invalid pmtime port \"%s\": %s\n",
			pmProgname, optarg, pmErrStr(-oserror()));
		errflag++;
	    }
	    break;

	case 's':		/* sample count */
	    d = (int)strtol(optarg, &endnum, 10);
	    if (Tflag) {
		fprintf(stderr, "%s: at most one of -E and -T allowed\n", pmProgname);
		errflag++;
	    }
	    else if (*endnum != '\0' || d < 0) {
		fprintf(stderr, "%s: -s requires +ve numeric argument\n", pmProgname);
		errflag++;
	   }
	   else samples = d;
	   break;

	case 'S':		/* start run time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, delta, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	    break;

	case 'T':		/* run time */
	    if (samples != ALL_SAMPLES) {
		fprintf(stderr, "%s: at most one of -T and -s allowed\n", pmProgname);
		errflag++;
	    }
	    Tflag = optarg;
	    break;

	case 'x':
	    xflag = 1;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	    fprintf(stderr, usage, pmProgname);
	    exit(EXIT_FAILURE);

	default:
	    errflag++;
	}
    }

    /* parse uniform metric spec */
    if (optind >= argc) {
	fprintf(stderr, "Error: no metricname specified\n\n");
	errflag++;
    }
    else if (optind < argc-1) {
	fprintf(stderr, "Error: pmval can only process one metricname at a time\n\n");
	errflag++;
    }
    else {
	if (ahtype == PM_CONTEXT_HOST) {
	    if (pmParseMetricSpec(argv[optind], 0, host, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
	else {		/* must be archive */
	    if (pmParseMetricSpec(argv[optind], 1, archive, &msp, &msg) < 0) {
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	}
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    if (msp->isarch == 1) {
	archive = msp->source;
	ahtype = PM_CONTEXT_ARCHIVE;
    }
    else if (msp->isarch == 2) {
	ahtype = PM_CONTEXT_LOCAL;
    }

    if (gui == 1 && port != -1) {
	fprintf(stderr, "%s: -g cannot be used with -p\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    cp->pmname = msp->metric;
    if (msp->ninst > 0) {
	cp->inum = msp->ninst;
	cp->iall = (cp->inum == 0);
	cp->inames = &msp->inst[0];
    }

    if (msp->isarch == 1) {
	/* open connection to archive */
	if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, msp->source)) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, msp->source, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	logStart = label.ll_start;
	host = label.ll_hostname;
	if ((sts = pmGetArchiveEnd(&last)) < 0) {
	    fprintf(stderr, "%s: Cannot determine end of archive: %s",
		pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
    }
    else {
	/* open connection to host or local context */
	if ((sts = pmNewContext(ahtype, msp->source)) < 0) {
	    if (ahtype == PM_CONTEXT_HOST)
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		    pmProgname, msp->source, pmErrStr(sts));
	    else
		fprintf(stderr, "%s: Cannot establish local context: %s\n",
		    pmProgname, pmErrStr(sts));
	    exit(EXIT_FAILURE);
	}
	host = msp->source;
	__pmtimevalNow(&logStart);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	if (ahtype == PM_CONTEXT_ARCHIVE) {
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		host);
	}
	else {
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
	}
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
	    exit(EXIT_FAILURE);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else printf("\n");

    pmWhichZone(&rpt_tz);

    if (pmParseTimeWindow(Sflag, Tflag, Aflag, Oflag,
			   &logStart, &last,
			   &first, &last, posn, &msg) < 0) {
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
    }

    initapi(cp);

    if (!(gui || port != -1) &&
	samples == ALL_SAMPLES &&
	last.tv_sec != INT_MAX &&
	amode != PM_MODE_FORW) {

	samples = (long)((__pmtimevalToReal(&last) - __pmtimevalToReal(posn)) /
		__pmtimevalToReal(delta));
	if (samples < 0) 
	    /* if end is before start, no samples thanks */
	    samples = 0;
	else {
	    /*
	     * p stands for posn
	     * + p         + p+delta   + p+2*delta + p+3*delta        + last
	     * |           |           |           |              |   |
	     * +-----------+-----------+-----------+-- ...... ----+---+---> time
	     *             1           2           3              samples
	     *
	     * So we will perform samples+1 fetches ... the number of reported
	     * values cannot be determined as it is usually (but not always
	     * thanks to interpolation mode in archives) one less for
	     * PM_SEM_COUNTER metrics.
	     *
	     * samples: as reported in the header output is the number
	     * of fetches to be attempted.
	     */
	    samples++;
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    char		timebuf[26];
	    char		*tp;
	    fprintf(stderr, "getargs: first=%.6f", __pmtimevalToReal(&first));
	    tp = pmCtime(&first.tv_sec, timebuf);
	    /*
	     * tp -> Ddd Mmm DD HH:MM:SS YYYY\n
	     *       0   4   8  1      1 2  2 2
	     *                  1      8 0  3 4
	     */
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: posn=%.6f", __pmtimevalToReal(posn));
	    tp = pmCtime(&posn->tv_sec, timebuf);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: last=%.6f", __pmtimevalToReal(&last));
	    tp = pmCtime(&last.tv_sec, timebuf);
	    fprintf(stderr, "[%8.8s]\n", &tp[11]);
	    fprintf(stderr, "getargs: delta=%.6f samples=%ld\n",
	    __pmtimevalToReal(delta), samples);
	}
#endif
    }

    if (gui || port != -1) {
	/* set up pmtime control */
	pmtime = pmTimeStateSetup(&controls, ahtype, port, *delta, *posn,
				    first, last, rpt_tz, host);
	controls.stepped = timestep;
	gui = 1;	/* we're using pmtime control from here on */
    }
    else if (ahtype == PM_CONTEXT_ARCHIVE) /* no time control, go it alone */
	pmTimeStateMode(amode, *delta, posn);
}

/***************************************************************************
 * main
 ***************************************************************************/
int
main(int argc, char *argv[])
{
    struct timeval  delta;		/* sample interval */
    struct timeval  now;		/* current task start time */
    Control	    cntrl;		/* global control structure */
    pmResult	    *rslt1;		/* current values */
    int		    forever;
    int		    idx1;

    __pmSetProgname(argv[0]);
    setlinebuf(stdout);

    getargs(argc, argv, &cntrl, &now, &delta);

    if (cntrl.desc.type == PM_TYPE_EVENT && xflag == 0) {
	fprintf(stderr, "%s: Cannot display values for PM_TYPE_EVENT metrics without -x\n",
		pmProgname);
	exit(EXIT_FAILURE);
    }

    forever = (samples == ALL_SAMPLES || gui);

    printhdr(&cntrl, delta, now);

    /* wait till time for first sample */
    if (archive == NULL)
	__pmtimevalPause(now);

    /* main loop fetching and printing sample values */
    while (forever || (samples-- > 0)) {
	if (gui)
	    pmTimeStateVector(&controls, pmtime);

	/* wait till time for sample */
	if (!gui && archive == NULL)
	    __pmtimevalSleep(delta);

	/* next sample */
	if ((idx1 = getvals(&cntrl, &rslt1)) == -2)
	    /* out the end of the window */
	    break;
	else if (idx1 < 0) {
	    /* nothing to report this time */
	    continue;
	}

	if (gui || archive != NULL) {
	    __pmPrintStamp(stdout, &rslt1->timestamp);
	    printf("  ");
	}
	printf("%s: ", cntrl.pmname);

	myeventdump(rslt1->vset[idx1]);

	pmFreeResult(rslt1);
    }

    /*
     * All serious error conditions have explicit exit() calls, so
     * if we get this far, all has gone well.
     */
    return 0;
}
