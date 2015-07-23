/*
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
 */

#include "pmevent.h"

static int setupEventTrace(int, char **, int, char *);
static char *options = "a:D:gh:K:LO:p:S:s:T:t:vx:zZ:?";
static char usage[] =
    "Usage: %s [options] metricname ...\n\n"
    "Options:\n"
    "  -a archive    metrics source is a PCP archive\n"
    "  -g            start in GUI mode with new time control\n"
    "  -h host       metrics source is PMCD on host\n"
    "  -K spec       optional additional PMDA spec for local connection\n"
    "                spec is of the form op,domain,dso-path,init-routine\n"
    "  -L            metrics source is a local context\n"
    "  -O offset     initial offset into the reporting time window\n"
    "  -p port       port number for connection to existing time control\n"
    "  -S starttime  start of the reporting time window\n"
    "  -s samples    terminate after this many samples\n"
    "  -T endtime    end of the reporting time window\n"
    "  -t interval   sample interval [default 1 second]\n"
    "  -v            increase diagnostic output\n"
    "  -x filter     optionally enable and filter the event stream\n"
    "  -Z timezone   set reporting timezone\n"
    "  -z            set reporting timezone to local timezone of metrics source\n";

/* process command line options and flags - exits on error */
void
doargs(int argc, char **argv)
{
    int			c;
    long		d;
    int			errflag = 0;
    int			m;
    int			src = 0;
    int			have_context = 0;
    int			sts;
    char		*endnum;
    char		*errmsg;
    char		*Sflag = NULL;		/* argument of -S flag */
    char		*Tflag = NULL;		/* argument of -T flag */
    char		*Oflag = NULL;		/* argument of -O flag */
    char		*xflag = NULL;		/* argument of -x flag */
    int			zflag = 0;		/* for -z */
    char 		*tz = NULL;		/* for -Z timezone */
    int			tzh;			/* initial timezone handle */
    struct timeval	logStart;
    metric_t		*mp;
    pmMetricSpec	*msp;
    char		*msg;
    static pmLogLabel	label;
    static char		*default_host_conn = "local:"; 
    char		*host_conn = default_host_conn;	/* argument of -h */
    const char		*tmp;

    delta.tv_sec = 1;
    delta.tv_usec = 0;
    samples = ALL_SAMPLES;

    /* extract command-line arguments */
    while ((c = getopt(argc, argv, options)) != EOF) {
	switch (c) {

	case 'a':		/* archive */
	    if (++src > 1) {
	    	fprintf(stderr, "%s: at most one of -a/-h/-L allowed\n", pmProgname);
	    	errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    archive = optarg;
	    break;

	case 'D':		/* debug flag */
	    sts = __pmParseDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    else
		pmDebug |= sts;
	    break;

	case 'g':		/* use "gui" mode */
	    if (port != -1) {
		fprintf(stderr, "%s: at most one of -g and -p allowed\n", pmProgname);
		errflag++;
	    }
	    else
		gui = 1;
	    break;

	case 'h':		/* host name */
	    if (++src > 1) {
		fprintf(stderr, "%s: at most one of -a/-h/-L allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_HOST;
	    host_conn = optarg;
	    break;

	case 'K':		/* update local PMDA table */
	    if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
		fprintf(stderr, "%s: illegal -K argument\n", pmProgname);
		fputs(errmsg, stderr);
		fputc('\n', stderr);
		errflag++;
	    }
	    break;

	case 'L':		/* use local context */
	    if (++src > 1) {
		fprintf(stderr, "%s: at most one of -a/-h/-L allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_LOCAL;
	    break;

	case 'O':		/* start report time offset */
	    Oflag = optarg;
	    break;

	case 'p':		/* port for slave of existing time control */
	    if (gui == 1) {
		fprintf(stderr, "%s: at most one of -g and -p allowed\n", pmProgname);
		errflag++;
	    }
	    else {
		port = (int)strtol(optarg, &endnum, 10);
		if (*endnum != '\0' || port < 0) {
		    fprintf(stderr, "%s: -p requires a positive numeric argument\n", pmProgname);
		    port = -1;
		    errflag++;
		}
	    }
	    break;

	case 's':		/* sample count */
	    d = (int)strtol(optarg, &endnum, 10);
	    if (Tflag) {
		fprintf(stderr, "%s: at most one of -s and -T allowed\n", pmProgname);
		errflag++;
	    }
	    else if (*endnum != '\0' || d < 0) {
		fprintf(stderr, "%s: -s requires a positive numeric argument\n", pmProgname);
		errflag++;
	   }
	   else
	       samples = d;
	   break;

	case 'S':		/* start report time */
	    Sflag = optarg;
	    break;

	case 't':		/* sampling interval */
	    if (pmParseInterval(optarg, &delta, &msg) < 0) {
		fprintf(stderr, "%s: illegal -t argument\n", pmProgname);
		fputs(msg, stderr);
		free(msg);
		errflag++;
	    }
	    break;

	case 'T':		/* end reporting time */
	    if (samples != ALL_SAMPLES) {
		fprintf(stderr, "%s: at most one of -s and -T allowed\n", pmProgname);
		errflag++;
	    }
	    Tflag = optarg;
	    break;

	case 'v':
	    verbose++;
	    break;

	case 'x':
	    xflag = optarg;
	    break;

	case 'z':		/* timezone from metrics source */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmProgname);
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':		/* $TZ timezone */
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

    if (errflag) {
	fprintf(stderr, usage, pmProgname);
	exit(EXIT_FAILURE);
    }

    if (optind >= argc) {
	fprintf(stderr, "Error: no metricname specified\n");
	exit(EXIT_FAILURE);
    }

    /* parse uniform metric spec */
    for ( ; optind < argc; optind++) {
	if (ahtype == PM_CONTEXT_ARCHIVE)
	    sts = pmParseMetricSpec(argv[optind], 1, archive, &msp, &msg);
	else
	    sts = pmParseMetricSpec(argv[optind], 0, host_conn, &msp, &msg);
	if (sts < 0) {
	    fprintf(stderr, "%s: bad metric specification\n", pmProgname);
	    fputs(msg, stderr);
	    free(msg);
	    exit(EXIT_FAILURE);
	}

	if (msp->isarch == 0) {
	    if (ahtype == -1) {
		ahtype = PM_CONTEXT_HOST;
		host_conn = msp->source;
	    }
	    else if ((ahtype == PM_CONTEXT_ARCHIVE) ||
		     (ahtype == PM_CONTEXT_LOCAL &&
		     (strcmp(msp->source, default_host_conn)))) {
		fprintf(stderr, "%s: %s: only one type of metric source allowed\n", pmProgname, argv[optind]);
		exit(EXIT_FAILURE);
	    }
	    else if (strcmp(host_conn, msp->source) != 0) {
		fprintf(stderr, "%s: %s: only one metric source allowed, found hosts %s and %s\n", pmProgname, argv[optind], host_conn, msp->source);
		exit(EXIT_FAILURE);
	    }
	}
	else if (msp->isarch == 1) {
	    if (ahtype == -1) {
		ahtype = PM_CONTEXT_ARCHIVE;
		archive = msp->source;
	    }
	    else if (ahtype != PM_CONTEXT_ARCHIVE) {
		fprintf(stderr, "%s: %s: only one type of metric source allowed\n", pmProgname, argv[optind]);
		exit(EXIT_FAILURE);
	    }
	    else if (strcmp(archive, msp->source) != 0) {
		fprintf(stderr, "%s: %s: only one metric source allowed, found archives %s and %s\n", pmProgname, argv[optind], archive, msp->source);
		exit(EXIT_FAILURE);
	    }
	}
	else if (msp->isarch == 2) {
	    if (ahtype == -1) {
		ahtype = PM_CONTEXT_LOCAL;
	    }
	    else if (ahtype != PM_CONTEXT_LOCAL) {
		fprintf(stderr, "%s: %s: only one type of metric source allowed\n", pmProgname, argv[optind]);
		exit(EXIT_FAILURE);
	    }
	}

	if (!have_context) {
	    if (ahtype == PM_CONTEXT_ARCHIVE) {
		/* open connection to archive */
		if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
		    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
			pmProgname, msp->source, pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
		ctxhandle = sts;
		if ((sts = pmGetArchiveLabel(&label)) < 0) {
		    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
			pmProgname, pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
		have_context = 1;
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
		if ((sts = pmNewContext(ahtype, host_conn)) < 0) {
		    if (ahtype == PM_CONTEXT_HOST)
			fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			    pmProgname, msp->source, pmErrStr(sts));
		    else
			fprintf(stderr, "%s: Cannot establish local context: %s\n",
			    pmProgname, pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
		ctxhandle = sts;
		have_context = 1;
		__pmtimevalNow(&logStart);
	    }
	}

        /* Look up the host name according to the pmcd or the archive. */
        tmp = pmGetContextHostName(ctxhandle);
	if (strlen(tmp) == 0) {
	    fprintf(stderr, "%s: pmGetContextHostName(%d) failed\n",
		pmProgname, ctxhandle);
	    exit(EXIT_FAILURE);
	}
        if ((host = strdup(tmp)) == NULL)
            __pmNoMem("host name copy", strlen(tmp)+1, PM_FATAL_ERR);

	for (m = 0; m < nmetric; m++) {
	    if (strcmp(msp->metric, metrictab[m].name) == 0)
		break;
	}
	if (m < nmetric)
	    mp = &metrictab[m];
	else {
	    nmetric++;
	    metrictab = (metric_t *)realloc(metrictab, nmetric*sizeof(metrictab[0]));
	    if (metrictab == NULL) {
		__pmNoMem("metrictab", nmetric*sizeof(metrictab[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    mp = &metrictab[nmetric-1];
	    mp->name = msp->metric;
	    if ((sts = pmLookupName(1, &mp->name, &mp->pmid)) < 0) {
		fprintf(stderr, "%s: pmLookupName: %s: %s\n", pmProgname, mp->name, pmErrStr(sts));
		exit(EXIT_FAILURE);
	    }
	    if ((sts = pmLookupDesc(mp->pmid, &mp->desc)) < 0) {
		fprintf(stderr, "%s: pmLookupDesc: %s: %s\n", pmProgname, mp->name, pmErrStr(sts));
		exit(EXIT_FAILURE);
	    }
	    if (mp->desc.type != PM_TYPE_EVENT &&
		mp->desc.type != PM_TYPE_HIGHRES_EVENT) {
		fprintf(stderr, "%s: %s: metrics must be of event type\n", pmProgname, mp->name);
		exit(EXIT_FAILURE);
	    }
	    mp->ninst = 0;
	    mp->iname = NULL;
	    mp->inst = NULL;
	    mp->ihash.nodes = 0;
	    mp->ihash.hsize = 0;
	    mp->ihash.hash = NULL;
	}

	if (msp->ninst > 0) {
	    int		i;
	    int		j;
	    if (mp->desc.indom == PM_INDOM_NULL) {
		fprintf(stderr, "%s: %s: singular metrics do not have instances\n", pmProgname, argv[optind]);
		exit(EXIT_FAILURE);
	    }
	    i = mp->ninst;
	    mp->ninst += msp->ninst;
	    mp->iname = (char **)realloc(mp->iname, mp->ninst*sizeof(mp->iname[0]));
	    if (mp->iname == NULL) {
		__pmNoMem("iname[]", mp->ninst*sizeof(mp->iname[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    mp->inst = (int *)realloc(mp->inst, mp->ninst*sizeof(mp->inst[0]));
	    if (mp->inst == NULL) {
		__pmNoMem("inst[]", mp->ninst*sizeof(mp->inst[0]), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    for (j = 0; j < msp->ninst; j++, i++) {
		mp->iname[i] = msp->inst[j];
		if (ahtype == PM_CONTEXT_ARCHIVE)
		    sts = pmLookupInDomArchive(mp->desc.indom, mp->iname[i]);
		else
		    sts = pmLookupInDom(mp->desc.indom, mp->iname[i]);
		if (sts < 0) {
		    fprintf(stderr, "%s: pmLookupInDom: %s[%s]: %s\n", pmProgname, mp->name, mp->iname[i], pmErrStr(sts));
		    exit(EXIT_FAILURE);
		}
		mp->inst[i] = sts;
	    }
	}

	/*
	 * don't call pmFreeMetricSpec(msp) because we retain pointers into
	 * the structure
	 */
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

    if (pmParseTimeWindow(Sflag, Tflag, NULL, Oflag,
			  &logStart, &last, &first, &last, &now, &msg) < 0) {
	fprintf(stderr, "%s", msg);
	exit(EXIT_FAILURE);
    }

    if (setupEventTrace(argc, argv, ahtype, xflag) < 0)
	exit(EXIT_FAILURE);

    if (pmDebug & DBG_TRACE_APPL0) {
	char		timebuf[26];
	char		*tp;
	int		i;
	fprintf(stderr, "first=%.6f", __pmtimevalToReal(&first));
	tp = pmCtime((const time_t *)&first.tv_sec, timebuf);
	/*
	 * tp -> Ddd Mmm DD HH:MM:SS YYYY\n
	 *       0   4   8  1      1 2  2 2
	 *                  1      8 0  3 4
	 */
	fprintf(stderr, " [%24.24s]\n", tp);
	fprintf(stderr, "now=%.6f", __pmtimevalToReal(&now));
	tp = pmCtime((const time_t *)&now.tv_sec, timebuf);
	fprintf(stderr, " [%24.24s]\n", tp);
	fprintf(stderr, "last=%.6f", __pmtimevalToReal(&last));
	tp = pmCtime((const time_t *)&last.tv_sec, timebuf);
	fprintf(stderr, " [%24.24s]\n", tp);
	fprintf(stderr, "delta=%.6f\n", __pmtimevalToReal(&delta));
	if (samples != ALL_SAMPLES)
	    fprintf(stderr, "samples=%ld\n", samples);
	for (m = 0; m < nmetric; m++) {
	    fprintf(stderr, "[%d] metric: %s", m, metrictab[m].name);
	    if (metrictab[m].ninst > 0) {
		fprintf(stderr, " instance:");
		for (i = 0; i < metrictab[m].ninst; i++) {
		    if (i == 0)
			fputc(' ', stderr);
		    else
			fprintf(stderr, ", ");
		    fprintf(stderr, "%s (%d)", metrictab[m].iname[i], metrictab[m].inst[i]);
		}
		fputc('\n', stderr);
	    }
	}
    }
}

int
setupEventTrace(int argc, char **argv, int ahtype, char *xflag)
{
    pmValueSet		pmvs;
    pmValueBlock	*pmvbp;
    pmResult		store = { .numpmid = 1 };
    int			m, vlen;

    if (ahtype == PM_CONTEXT_ARCHIVE)
	return 0;	/* nothing to do at this stage */

    if (ahtype == PM_CONTEXT_HOST)
	__pmSetClientIdArgv(argc, argv);

    /* build pmResult for pmStore call if we're explicitly enabling events */
    if (xflag != NULL) {
	vlen = PM_VAL_HDR_SIZE + strlen(xflag) + 1;
	pmvbp = (pmValueBlock *)malloc(vlen);
	if (!pmvbp)
	    __pmNoMem("store", vlen, PM_FATAL_ERR);
	pmvbp->vtype = PM_TYPE_STRING;
	pmvbp->vlen = vlen;
	strcpy(pmvbp->vbuf, xflag);

	store.vset[0] = &pmvs;
	for (m = 0; m < nmetric; m++) {
	    pmvs.pmid = metrictab[m].pmid;
	    pmvs.numval = 1;
	    pmvs.valfmt = PM_VAL_SPTR;
	    pmvs.vlist[0].inst = PM_IN_NULL;
	    pmvs.vlist[0].value.pval = pmvbp;

	    pmStore(&store);
	}

	free(pmvbp);
    }
    return 0;
}
