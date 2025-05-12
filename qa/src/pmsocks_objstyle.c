/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <limits.h>
#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    int		mode = PM_MODE_INTERP;		/* mode for archives */
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
    char	*endnum;
    struct timespec startTime;
    struct timespec endTime;
    struct timespec appStart;
    struct timespec appEnd;
    struct timespec appOffset;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:?")) != EOF) {
	switch (c) {

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
  -D debug	standard PCP debug options\n\
  -h host	metrics source is PMCD on host\n",
		pmGetProgname());
	exit(1);
    }

    if (logfile != NULL) {
	pmOpenLog(pmGetProgname(), logfile, stderr, &sts);
	if (sts != 1) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmGetProgname(), logfile);
	}
    }

    if (pmnsfile != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(pmnsfile, 1)) < 0) {
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
	    if ((sts = pmSetModeHighRes(mode, &label.start, NULL)) < 0) {
		fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
		exit(1);
	    }
	}
  	startTime = label.start;
	sts = pmGetArchiveEnd(&endTime);
	if (sts < 0) {
	    fprintf(stderr, "%s: pmGetArchiveEnd: %s\n", pmGetProgname(),
		    pmErrStr(sts));
	    exit(1);
	}
    }
    else {
	struct timeval	temp_tv;
	gettimeofday(&temp_tv, NULL);
	startTime.tv_sec = temp_tv.tv_sec;
	startTime.tv_nsec = temp_tv.tv_usec * 1000;
	endTime.tv_sec = PM_MAX_TIME_T;
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.hostname);
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

    pmDestroyContext(pmWhichContext());
    return 0;
}
