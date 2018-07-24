/*
 * mark-bug - tests rewind over a mark (merged log)
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <limits.h>
#include <pcp/pmapi.h>

char *namelist[2] = {"hinv.ncpu", "irix.kernel.all.cpu.idle"};
pmID pmidlist[2];
pmResult *result;
struct timeval curpos;
static void check_result(char *, pmResult *);

static char timebuf[26];

int
main(int argc, char **argv)
{
    int		c;
    int		i;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int 	verbose = 0;
    char	*host = NULL;			/* pander to gcc */
    int		mode = PM_MODE_INTERP;		/* mode for archives */
    char 	*configfile = NULL;
    char	*start = NULL;
    char	*finish = NULL;
    char	*align = NULL;
    char	*offset = NULL;
    char 	*logfile = NULL;
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = NULL;			/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*pmnsfile = PM_NS_DEFAULT;
    int		samples = -1;
    double	delta = 1.0;
    char	*endnum;
    struct timeval startTime;
    struct timeval endTime;
    struct timeval appStart;
    struct timeval appEnd;
    struct timeval appOffset;
    time_t	clock;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:A:c:D:h:l:Ln:O:s:S:t:T:U:VzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'A':	/* time alignment */
	    align = optarg;
	    break;

	case 'c':	/* configfile */
	    if (configfile != NULL) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmGetProgname());
		errflag++;
	    }
	    configfile = optarg;
	    break;	

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'L':	/* LOCAL, no PMCD */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    host = NULL;
	    type = PM_CONTEXT_LOCAL;
	    putenv("PMDA_LOCAL_PROC=");		/* if proc PMDA needed */
	    putenv("PMDA_LOCAL_SAMPLE=");	/* if sampledso PMDA needed */
	    break;

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    pmnsfile = optarg;
	    break;

	case 'O':	/* sample offset time */
	    offset = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'S':	/* start time */
	    start = optarg;
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'T':	/* terminate time */
	    finish = optarg;
	    break;

	case 'U':	/* uninterpolated archive log */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a, -h, -L and -U allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    mode = PM_MODE_FORW;
	    host = optarg;
	    break;

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != NULL) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    zflag++;
	    break;

	case 'Z':	/* $TZ timezone */
	    if (zflag) {
		fprintf(stderr, "%s: at most one of -Z and/or -z allowed\n", pmGetProgname());
		errflag++;
	    }
	    tz = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a, -h or -U option\n", pmGetProgname());
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a archive	metrics source is an archive log\n\
  -A align	time alignment\n\
  -c configfile file to load configuration from\n\
  -D debug	standard PCP debug options\n\
  -h host	metrics source is PMCD on host\n\
  -l logfile	redirect diagnostics and trace output\n\
  -L            use local context instead of PMCD\n\
  -n pmnsfile   use an alternative PMNS\n\
  -O offset	offset sample time\n\
  -s samples	terminate after this many iterations\n\
  -S start	start at this time\n\
  -t delta	sample interval in seconds(float) [default 1.0]\n\
  -T finish	finish at this time\n\
  -U archive	metrics source is an uninterpolated archive log\n\
  -V            verbose/diagnostic output\n\
  -z            set reporting timezone to local time for host from -a, -h or -U\n\
  -Z timezone   set reporting timezone\n",
		pmGetProgname());
	exit(1);
    }

    if (logfile != NULL) {
	pmOpenLog(pmGetProgname(), logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmGetProgname(), logfile);
	}
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadNameSpace(pmnsfile)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), 
	       pmnsfile, pmErrStr(sts));
	exit(1);
    }

    if (type == 0) {
	type = PM_CONTEXT_HOST;
	gethostname(local, sizeof(local));
	host = local;
    }
    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	if (mode != PM_MODE_INTERP) {
	    if ((sts = pmSetMode(mode, &label.ll_start, 0)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
  	startTime = label.ll_start;
	sts = pmGetArchiveEnd(&endTime);
	if (sts < 0) {
	    fprintf(stderr, "%s: pmGetArchiveEnd: %s\n", pmGetProgname(),
		    pmErrStr(sts));
	    exit(1);
	}
    }
    else {
	gettimeofday(&startTime, NULL);
	endTime.tv_sec = INT_MAX;
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	else
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
    }
    else if (tz != NULL) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmGetProgname(), tz, pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to \"TZ=%s\"\n\n", tz);
    }
    else
	tzh = pmNewContextZone();

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	printf("extra argument[%d]: %s\n", optind, argv[optind]);
	optind++;
    }

    if (align != NULL && type != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -A option only supported for PCP archives, alignment request ignored\n",
		pmGetProgname());
	align = NULL;
    }

    sts = pmParseTimeWindow(start, finish, align, offset, &startTime,
			    &endTime, &appStart, &appEnd, &appOffset,
			    &endnum);

    if (sts < 0) {
	fprintf(stderr, "%s: %s\n", pmGetProgname(), endnum);
	exit(1);
    }

    if ((sts = pmLookupName(2, namelist, pmidlist)) < 0) {
	fprintf(stderr, "%s: pmLookupName failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* goto the start */
    if ((sts = pmSetMode(PM_MODE_INTERP, &appStart, (int)(delta * 1000))) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    clock = (time_t)appStart.tv_sec;
    pmCtime(&clock, timebuf);
    printf("archive %s\nstartTime: %19.19s.%06d\n",
	    host, timebuf, (int)appStart.tv_usec);
    clock = (time_t)appEnd.tv_sec;
    pmCtime(&clock, timebuf);
    printf("endTime  : %19.19s.%06d\nsamples=%d delta=%dms\n",
	    timebuf, (int)appEnd.tv_usec, samples, (int)(delta * 1000));

    /* play forwards over the mark */
    for (i=0; i < samples; i++) {
	if ((sts = pmFetch(2, pmidlist, &result)) < 0) {
	    fprintf(stderr, "%s: pmFetch failed: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}
	check_result("forwards ", result);
	curpos = result->timestamp; /* struct cpy */
	pmFreeResult(result);
    }

    /* rewind back over the mark */
    if ((sts = pmSetMode(PM_MODE_INTERP, &curpos, (int)(delta * -1000))) < 0) {
	fprintf(stderr, "%s: pmSetMode failed: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    for (i=0; i < samples; i++) {
	if ((sts = pmFetch(2, pmidlist, &result)) < 0) {
	    fprintf(stderr, "%s: pmFetch failed: %s\n", pmGetProgname(), pmErrStr(sts));
	    exit(1);
	}

	check_result("rewinding", result);
	pmFreeResult(result);
    }


    return 0;
}

static void
check_result(char *title, pmResult *result)
{
    int		err= 0;
    time_t	clock = (time_t)result->timestamp.tv_sec;
    if (result->numpmid != 2) {
	pmCtime(&clock, timebuf);
	printf("%s @ %19.19s.%06d numpmid=%d\n", title,
		timebuf, (int)result->timestamp.tv_usec, result->numpmid);
	err++;
    }
    if (result->numpmid >= 1 && result->vset[0]->numval != 1) {
	pmCtime(&clock, timebuf);
	printf("%s @ %19.19s.%06d vset[0]->numval=%d\n", title,
		timebuf, (int)result->timestamp.tv_usec, result->vset[0]->numval);
	err++;
    }
    if (result->numpmid >= 2 && result->vset[1]->numval != 1) {
	pmCtime(&clock, timebuf);
	printf("%s @ %19.19s.%06d vset[1]->numval=%d\n", title,
		timebuf, (int)result->timestamp.tv_usec, result->vset[1]->numval);
	err++;
    }
    if (err == 0) {
	pmCtime(&clock, timebuf);
	printf("fetch OK %s @ %19.19s.%06d\n", title,
		timebuf, (int)result->timestamp.tv_usec);
    }
}
