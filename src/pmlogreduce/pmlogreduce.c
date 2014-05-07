/*
 * pmlogreduce - statistical reduction of a PCP archive log
 *
 * Copyright (c) 2014 Red Hat.
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
 * TODO (global list)
 * 	- check for counter overflow in doscan()
 * 	- optimization (maybe) for discrete and instantaneous metrics
 * 	  to suppress repeated values, provided you get the boundary
 * 	  conditions correct ... beware of mark records and past last
 * 	  value ... it may be difficult to distinguish ... in practice
 * 	  this may not be worth it because discrete is rare and
 * 	  instantaneous is very likely to change over long enough time
 *	  ... counter example is hinv.* that interpolate returns on every
 *	  fetch, even only once in the input archive
 * 	- performance profiling
 * 	- testing with dynamic instance domains
 *	- check comments ahead of call to doscan() and the description
 *	  in the head of scan.c
 *
 * Debug flags
 *   APPL0
 *	initialization
 *	metadata
 *   APPL1
 *	inst-value scanning in doscan()
 *   APPL2
 *	scan summary
 *	details for records read and records written
 */

#include <sys/stat.h>
#include "pmlogreduce.h"

/*
 * globals defined in pmlogreduce.h
 */
__pmTimeval	current;		/* most recent timestamp overall */
char		*iname;			/* name of input archive */
pmLogLabel	ilabel;			/* input archive label */
int		numpmid;		/* all metrics from the input archive */
pmID		*pmidlist;
char		**namelist;
metric_t	*metriclist;
__pmLogCtl	logctl;			/* output archive control */
/* command line args */
double		targ = 600.0;		/* -t arg - interval b/n output samples */
int		sarg = -1;		/* -s arg - finish after X samples */
char		*Sarg;			/* -S arg - window start */
char		*Targ;			/* -T arg - window end */
char		*Aarg;			/* -A arg - output time alignment */
int		varg = -1;		/* -v arg - switch log vol every X */
int		zarg;			/* -z arg - use archive timezone */
char		*tz;			/* -Z arg - use timezone from user */

int	        written;		/* num log writes so far */
int		exit_status;

/* archive control stuff */
int		ictx_a;
char		*oname;			/* name of output archive */
pmLogLabel	olabel;			/* output archive label */
struct timeval	winstart_tval;		/* window start tval*/

/* time window stuff */
static struct timeval logstart_tval;	/* reduced log start */
static struct timeval logend_tval;	/* reduced log end */
static struct timeval winend_tval;	/* window end tval */

/* cmd line args that could exist, but don't (needed for pmParseTimeWin) */
static char	*Oarg;			/* -O arg - non-existent */

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_ALIGN,
    PMOPT_DEBUG,
    PMOPT_START,
    PMOPT_SAMPLES,
    PMOPT_FINISH,
    { "interval", 1, 't', "DELTA", "sample output interval [default 10min]" },
    { "", 1, 'v', "NUM", "switch log volumes after this many samples" },
    PMOPT_TIMEZONE,
    PMOPT_HOSTZONE,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "A:D:S:s:T:t:v:Z:z?",
    .long_options = longopts,
    .short_usage = "[options] input-archive output-archive",
};

static double
tv2double(struct timeval *tv)
{
    return tv->tv_sec + (double)tv->tv_usec / 1000000.0;
}

static int
parseargs(int argc, char *argv[])
{
    int			c;
    int			sts;
    char		*endnum;
    char		*msg;
    struct timeval	interval;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'A':	/* output time alignment */
	    Aarg = opts.optarg;
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

	case 's':	/* number of samples to write out */
	    sarg = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || sarg < 0) {
		pmprintf("%s: -s requires numeric argument\n",
			pmProgname);
		opts.errors++;
	    }
	    break;

	case 'S':	/* start time for reduction */
	    Sarg = opts.optarg;
	    break;

	case 'T':	/* end time for reduction */
	    Targ = opts.optarg;
	    break;

	case 't':	/* output sample interval */
	    if (pmParseInterval(opts.optarg, &interval, &msg) < 0) {
		pmprintf("%s", msg);
		free(msg);
		opts.errors++;
	    }
	    else
		targ = tv2double(&interval);
	    break;

	case 'v':	/* number of samples per volume */
	    varg = (int)strtol(opts.optarg, &endnum, 10);
	    if (*endnum != '\0' || varg < 0) {
		pmprintf("%s: -v requires numeric argument\n",
			pmProgname);
		opts.errors++;
	    }
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

    if (opts.errors == 0 && opts.optind > argc-2) {
	pmprintf("%s: Error: insufficient arguments\n", pmProgname);
	opts.errors++;
    }

    return -opts.errors;
}

int
main(int argc, char **argv)
{
    int		sts;
    char	*msg;
    pmResult	*irp;		/* input pmResult */
    pmResult	*orp;		/* output pmResult */
    __pmPDU	*pb;		/* pdu buffer */
    struct timeval	unused;
    unsigned long	peek_offset;

    /* process cmd line args */
    if (parseargs(argc, argv) < 0) {
	pmUsageMessage(&opts);
	exit(1);
    }

    /* input  archive name is argv[opts.optind] */
    /* output archive name is argv[argc-1]) */

    /* output archive */
    oname = argv[argc-1];

    /* input archive */
    iname = argv[opts.optind];

    /*
     * This is the interp mode context
     */
    if ((ictx_a = pmNewContext(PM_CONTEXT_ARCHIVE, iname)) < 0) {
	fprintf(stderr, "%s: Error: cannot open archive \"%s\" (ctx_a): %s\n",
		pmProgname, iname, pmErrStr(ictx_a));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&ilabel)) < 0) {
	fprintf(stderr, "%s: Error: cannot get archive label record (%s): %s\n", pmProgname, iname, pmErrStr(sts));
	exit(1);
    }

    /* start time */
    logstart_tval.tv_sec = ilabel.ll_start.tv_sec;
    logstart_tval.tv_usec = ilabel.ll_start.tv_usec;

    /* end time */
    if ((sts = pmGetArchiveEnd(&logend_tval)) < 0) {
	fprintf(stderr, "%s: Error: cannot get end of archive (%s): %s\n",
		pmProgname, iname, pmErrStr(sts));
	exit(1);
    }

    if (zarg) {
	/* use TZ from metrics source (input-archive) */
	if ((sts = pmNewZone(ilabel.ll_tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		    pmProgname, pmErrStr(sts));
            exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n", ilabel.ll_hostname);
    }
    else if (tz != NULL) {
	/* use TZ as specified by user */
	if ((sts = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		    pmProgname, tz, pmErrStr(sts));
	    exit(1);
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
	    exit(1);
	}
    }

    /* set winstart and winend timevals */
    sts = pmParseTimeWindow(Sarg, Targ, Aarg, Oarg,
			    &logstart_tval, &logend_tval,
			    &winstart_tval, &winend_tval, &unused, &msg);
    if (sts < 0) {
	fprintf(stderr, "%s: Invalid time window specified: %s\n",
		pmProgname, msg);
	exit(1);
    }
#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0) {
	char	buf[26];
	pmCtime((const time_t *)&winstart_tval.tv_sec, buf);
	fprintf(stderr, "Start time: %s", buf);
	pmCtime((const time_t *)&winend_tval.tv_sec, buf);
	fprintf(stderr, "End time: %s", buf);
    }
#endif

    if ((sts = pmSetMode(PM_MODE_INTERP | PM_XTB_SET(PM_TIME_SEC),
                         &winstart_tval, (int)targ)) < 0) {
	fprintf(stderr, "%s: pmSetMode(PM_MODE_INTERP ...) failed: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* create output log - must be done before writing label */
    if ((sts = __pmLogCreate("", oname, PM_LOG_VERS02, &logctl)) < 0) {
	fprintf(stderr, "%s: Error: __pmLogCreate: %s\n",
		pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* This must be done after log is created:
     *		- checks that archive version, host, and timezone are ok
     *		- set archive version, host, and timezone of output archive
     *		- set start time
     *		- write labels
     */
    newlabel();
    current.tv_sec = logctl.l_label.ill_start.tv_sec = winstart_tval.tv_sec;
    current.tv_usec = logctl.l_label.ill_start.tv_usec = winstart_tval.tv_usec;
    /* write label record */
    writelabel();
    /*
     * Supress any automatic label creation in libpcp at the first
     * pmResult write.
     */
    logctl.l_state = PM_LOG_STATE_INIT;

    /*
     * Traverse the PMNS to get all the metrics and their metadata
     */
    if ((sts = pmTraversePMNS ("", dometric)) < 0) {
	fprintf(stderr, "%s: Error traversing namespace ... %s\n",
		pmProgname, pmErrStr(sts));
	goto cleanup;
    }

    /*
     * All the initial metadata has been generated, add timestamp
     */
    fflush(logctl.l_mdfp);
    __pmLogPutIndex(&logctl, &current);

    written = 0;

    /*
     * main loop
     */
    while (sarg == -1 || written < sarg) {
	/*
	 * do stuff
	 */
	if ((sts = pmUseContext(ictx_a)) < 0) {
	    fprintf(stderr, "%s: Error: cannot use context (%s): %s\n",
		    pmProgname, iname, pmErrStr(sts));
	    goto cleanup;
	}
	if ((sts = pmFetch(numpmid, pmidlist, &irp)) < 0) {
	    if (sts == PM_ERR_EOL)
		break;
	    fprintf(stderr,
		"%s: Error: pmFetch failed: %s\n", pmProgname, pmErrStr(sts));
	    exit(1);
	}
	if (irp->timestamp.tv_sec > winend_tval.tv_sec ||
	    (irp->timestamp.tv_sec == winend_tval.tv_sec &&
	     irp->timestamp.tv_usec > winend_tval.tv_usec)) {
	    /* past end time as per -T */
	    break;
	}
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    fprintf(stderr, "input record ...\n");
	    __pmDumpResult(stderr, irp);
	}
#endif

	/*
	 * traverse the interval, looking at every archive record ...
	 * we are particularly interested in:
	 * 	- metric-values that are interpolated but not present in
	 * 	  this interval
	 * 	- counter wraps
	 *	- mark records
	 */
	doscan(&irp->timestamp);

	if ((sts = pmUseContext(ictx_a)) < 0) {
	    fprintf(stderr, "%s: Error: cannot use context (%s): %s\n",
		    pmProgname, iname, pmErrStr(sts));
	    goto cleanup;
	}

	orp = rewrite(irp);
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2) {
	    if (orp == NULL)
		fprintf(stderr, "output record ... none!\n");
	    else {
		fprintf(stderr, "output record ...\n");
		__pmDumpResult(stderr, orp);
	    }
	}
#endif
	if (orp == NULL)
	    goto next;

	/*
	 * convert log record to a PDU, and enforce V2 encoding semantics,
	 * then write it out
	 */
	sts = __pmEncodeResult(PDU_OVERRIDE2, orp, &pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmEncodeResult: %s\n",
		    pmProgname, pmErrStr(sts));
	    goto cleanup;
	}

	/* switch volumes if required */
	if (varg > 0) {
	    if (written > 0 && (written % varg) == 0) {
		__pmTimeval	next_stamp;
		next_stamp.tv_sec = irp->timestamp.tv_sec;
		next_stamp.tv_usec = irp->timestamp.tv_usec;
		newvolume(oname, &next_stamp);
	    }
	}
	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes
	 */
	peek_offset = ftell(logctl.l_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
	    __pmTimeval	next_stamp;
	    next_stamp.tv_sec = irp->timestamp.tv_sec;
	    next_stamp.tv_usec = irp->timestamp.tv_usec;
	    newvolume(oname, &next_stamp);
	}

	current.tv_sec = orp->timestamp.tv_sec;
	current.tv_usec = orp->timestamp.tv_usec;

	doindom(orp);

	/* write out log record */
	sts = __pmLogPutResult2(&logctl, pb);
	__pmUnpinPDUBuf(pb);
	if (sts < 0) {
	    fprintf(stderr, "%s: Error: __pmLogPutResult2: log data: %s\n",
		    pmProgname, pmErrStr(sts));
	    goto cleanup;
	}
	written++;

	rewrite_free();

next:
	pmFreeResult(irp);
    }

    /* write the last time stamp */
    fflush(logctl.l_mfp);
    fflush(logctl.l_mdfp);
    __pmLogPutIndex(&logctl, &current);

    exit(exit_status);

cleanup:
    {
	char    fname[MAXNAMELEN];
	fprintf(stderr, "Archive \"%s\" not created.\n", oname);
	snprintf(fname, sizeof(fname), "%s.0", oname);
	unlink(fname);
	snprintf(fname, sizeof(fname), "%s.meta", oname);
	unlink(fname);
	snprintf(fname, sizeof(fname), "%s.index", oname);
	unlink(fname);
	exit(1);
    }
}
