/*
 * interp_bug - https://bugzilla.redhat.com/show_bug.cgi?id=958745
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

#define N_PMID (int)(sizeof(metrics)/sizeof(metrics[0]))

static char *metrics[] = {
    "gfs.inodes",
    "gfs.fmb"
};

static pmID pmid[N_PMID];

static void
printstamp(struct timespec *tp)
{
    static struct tm    tmp;

    pmLocaltime(&tp->tv_sec, &tmp);
    printf("%02d:%02d:%02d.%09d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)tp->tv_nsec);
}


int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx;
    int		errflag = 0;
    int		type = 0;
    char	*host = NULL;			/* pander to gcc */
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = (char *)0;		/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    int		samples = 10;
    int		sample;
    struct timespec start;
    struct timespec now;
    struct timespec	delta = { 1, 0 };
    char	*endnum;
    pmResult	**result;
    int		i;
    int		status = 0;

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:s:t:VzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one -a allowed\n", pmGetProgname());
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    pmtimespecFromReal(strtod(optarg, &endnum), &delta);
	    if (*endnum != '\0' || pmtimespecToReal(&delta) <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmGetProgname());
		errflag++;
	    }
	    break;

	case 'z':	/* timezone from host */
	    if (tz != (char *)0) {
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

    if (type == 0) {
	fprintf(stderr, "%s: -a is not optional!\n", pmGetProgname());
	errflag++;
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmGetProgname());
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive\n\
  -D   debugspec  standard PCP debugging options\n\
  -s   samples	  terminate after this many iterations [default 10]\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n\
  -z              set reporting timezone to local time for host from -a or -h\n\
  -Z   timezone   set reporting timezone\n",
		pmGetProgname());
	exit(1);
    }

    if ((ctx = pmNewContext(type, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), host, pmErrStr(ctx));
	exit(1);
    }

    if ((sts = pmGetHighResArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmGetProgname(), pmErrStr(tzh));
	    exit(1);
	}
	printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
	    label.hostname);
    }
    else if (tz != (char *)0) {
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

    sts = pmLookupName(N_PMID, (const char **)metrics, pmid);
    for (i = 0; i < N_PMID; i++) {
	printf("metrics[%d]: %s %s\n", i, metrics[i], pmIDStr(pmid[i])); 
    }
    if (sts != N_PMID) {
	if (sts < 0)
	    fprintf(stderr, "%s: pmLookupName: %s\n", pmGetProgname(), pmErrStr(sts));
	else
	    fprintf(stderr, "%s: pmLookupName: expecting %d, got %d\n", pmGetProgname(), N_PMID, sts);
	exit(1);
    }

    start = label.start;

    printf("Start at: ");
    printstamp(&start);
    printf("\n\n");

    if ((sts = pmSetMode(PM_MODE_INTERP, &start, &delta)) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    result = (pmResult **)malloc(2*samples*sizeof(result[0]));
    if (result == NULL) {
	fprintf(stderr, "%s: arrgh, malloc failed for %d bytes\n", pmGetProgname(), (int)(2*samples*sizeof(result[0])));
	exit(1);
    }

    for (sample=0; sample < samples; sample++) {
	if ((sts = pmFetch(N_PMID, pmid, &result[sample])) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		status = 1;
	    }
	    break;
	}

	printf("sample %3d time=", sample);
	printstamp(&result[sample]->timestamp);
	putchar('\n');
	now = result[sample]->timestamp;
    }

    /* simulate pmchart's [Stop] */

    if ((sts = pmSetMode(PM_MODE_FORW, &now, NULL)) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    for ( ; sample < 2*samples; sample++) {
	if ((sts = pmFetch(N_PMID, pmid, &result[sample])) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: pmFetch: %s\n", pmGetProgname(), pmErrStr(sts));
		status = 1;
	    }
	    break;
	}

	printf("sample %3d time=", sample);
	printstamp(&result[sample]->timestamp);
	putchar('\n');
    }

    for (i = 0; i < sample; i++) {
	pmFreeResult(result[i]);
    }

    exit(status);
}
