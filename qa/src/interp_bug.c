/*
 * interp_bug - demonstrate archive interpolation mode bug
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

#define N_PMID_A sizeof(metrics_a)/sizeof(metrics_a[0])
#define N_PMID_B sizeof(metrics_b)/sizeof(metrics_b[0])

static char *metrics_a[] = {
    "sample.long.one",
    "kernel.all.syscall"
};

static char *metrics_b[] = {
    "sample.long.one",
    "kernel.all.sysexec"
};

static pmID pmid_a[N_PMID_A];
static pmID pmid_b[N_PMID_B];

static void
printstamp(struct timeval *tp)
{
    static struct tm    tmp;

    pmLocaltime(&tp->tv_sec, &tmp);
    printf("%02d:%02d:%02d.%03d", tmp.tm_hour, tmp.tm_min, tmp.tm_sec, (int)(tp->tv_usec/1000));
}


int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		type = 0;
    int		force = 0;
    int 	verbose = 0;
    char	*host = NULL;			/* pander to gcc */
    char 	*configfile = (char *)0;
    char 	*logfile = (char *)0;
    pmLogLabel	label;				/* get hostname for archives */
    int		zflag = 0;			/* for -z */
    char 	*tz = (char *)0;		/* for -Z timezone */
    int		tzh;				/* initial timezone handle */
    char	local[MAXHOSTNAMELEN];
    char	*namespace = PM_NS_DEFAULT;
    int		samples = -1;
    int		sample;
    struct timeval start;
    double	delta = 1.0;
    char	*endnum;
    pmResult	*result;
    int		i;
    int		status = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:c:D:fl:n:s:t:VzZ:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    type = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;

	case 'c':	/* configfile */
	    if (configfile != (char *)0) {
		fprintf(stderr, "%s: at most one -c option allowed\n", pmProgname);
		errflag++;
	    }
	    configfile = optarg;
	    break;	

#ifdef PCP_DEBUG
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
#endif

	case 'f':	/* force */
	    force++; 
	    break;	

	case 'h':	/* contact PMCD on this hostname */
	    if (type != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    host = optarg;
	    type = PM_CONTEXT_HOST;
	    break;

	case 'l':	/* logfile */
	    logfile = optarg;
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 's':	/* sample count */
	    samples = (int)strtol(optarg, &endnum, 10);
	    if (*endnum != '\0' || samples < 0) {
		fprintf(stderr, "%s: -s requires numeric argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 't':	/* delta seconds (double) */
	    delta = strtod(optarg, &endnum);
	    if (*endnum != '\0' || delta <= 0.0) {
		fprintf(stderr, "%s: -t requires floating point argument\n", pmProgname);
		errflag++;
	    }
	    break;

	case 'V':	/* verbose */
	    verbose++;
	    break;

	case 'z':	/* timezone from host */
	    if (tz != (char *)0) {
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
	default:
	    errflag++;
	    break;
	}
    }

    if (zflag && type == 0) {
	fprintf(stderr, "%s: -z requires an explicit -a or -h option\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive log\n\
  -c   configfile file to load configuration from\n\
  -D   debug	  standard PCP debug flag\n\
  -f		  force .. \n\
  -h   host	  metrics source is PMCD on host\n\
  -l   logfile	  redirect diagnostics and trace output\n\
  -n   namespace  use an alternative PMNS\n\
  -s   samples	  terminate after this many iterations\n\
  -t   delta	  sample interval in seconds(float) [default 1.0]\n\
  -V 	          verbose/diagnostic output\n\
  -z              set reporting timezone to local time for host from -a or -h\n\
  -Z   timezone   set reporting timezone\n",
		pmProgname);
	exit(1);
    }

    if (logfile != (char *)0) {
	__pmOpenLog(pmProgname, logfile, stderr, &sts);
	if (sts < 0) {
	    fprintf(stderr, "%s: Could not open logfile \"%s\"\n", pmProgname, logfile);
	}
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
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
		pmProgname, host, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if (type == PM_CONTEXT_ARCHIVE) {
	if ((sts = pmGetArchiveLabel(&label)) < 0) {
	    fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmProgname, pmErrStr(sts));
	    exit(1);
	}
    }

    if (zflag) {
	if ((tzh = pmNewContextZone()) < 0) {
	    fprintf(stderr, "%s: Cannot set context timezone: %s\n",
		pmProgname, pmErrStr(tzh));
	    exit(1);
	}
	if (type == PM_CONTEXT_ARCHIVE)
	    printf("Note: timezone set to local timezone of host \"%s\" from archive\n\n",
		label.ll_hostname);
	else
	    printf("Note: timezone set to local timezone of host \"%s\"\n\n", host);
    }
    else if (tz != (char *)0) {
	if ((tzh = pmNewZone(tz)) < 0) {
	    fprintf(stderr, "%s: Cannot set timezone to \"%s\": %s\n",
		pmProgname, tz, pmErrStr(tzh));
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

    sts = pmLookupName(N_PMID_A, metrics_a, pmid_a);
    for (i = 0; i < N_PMID_A; i++) {
	printf("metrics_a[%d]: %s %s\n", i, metrics_a[i], pmIDStr(pmid_a[i])); 
    }
    if (sts != N_PMID_A) {
	if (sts < 0)
	    fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: pmLookupName: expecting %d, got %d\n", pmProgname, (int)(N_PMID_A), sts);
	exit(1);
    }

    sts = pmLookupName(N_PMID_B, metrics_b, pmid_b);
    for (i = 0; i < N_PMID_B; i++) {
	printf("metrics_b[%d]: %s %s\n", i, metrics_b[i], pmIDStr(pmid_b[i])); 
    }
    if (sts != N_PMID_B) {
	if (sts < 0)
	    fprintf(stderr, "%s: pmLookupName: %s\n", pmProgname, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: pmLookupName: expecting %d, got %d\n", pmProgname, (int)(N_PMID_B), sts);
	exit(1);
    }

    /* skip the first two seconds, due to staggered start in log */
    start = label.ll_start;
    start.tv_sec += 2;

    printf("Start at: ");
    printstamp(&start);
    printf("\n\n");

    printf("Pass One: rewind and fetch metrics_a until end of log\n");
    if ((sts = pmSetMode(PM_MODE_INTERP, &start, (int)(delta * 1000))) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    for (sample=0; ; sample++) {
	if ((sts = pmFetch(N_PMID_A, pmid_a, &result)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
		status = 1;
	    }
	    break;
	}

	printf("sample %3d time=", sample);
	printstamp(&result->timestamp);
	putchar(' ');
	if (result->numpmid != N_PMID_A || result->vset[0]->numval != 1) {
	    printf("expected %d (got %d) value sets, with one value in the first.\n",
	    (int)(N_PMID_A), result->numpmid);
	    status = 1;
	}
	else {
	    if (result->vset[0]->vlist[0].value.lval != 1) {
		printf("expected value=1, got value=%d\n", result->vset[0]->vlist[0].value.lval);
		__pmDumpResult(stdout, result);
		status = 1;
	    }
	    else
		printf("correct result\n");
	}
	pmFreeResult(result);
    }

    printf("Pass Two: rewind and fetch metrics_b until end of log\n");
    if ((sts = pmSetMode(PM_MODE_INTERP, &start, (int)(delta * 1000))) < 0) {
	fprintf(stderr, "%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    for (sample=0; ; sample++) {
	if ((sts = pmFetch(N_PMID_B, pmid_b, &result)) < 0) {
	    if (sts != PM_ERR_EOL) {
		fprintf(stderr, "%s: pmFetch: %s\n", pmProgname, pmErrStr(sts));
		status = 1;
	    }
	    break;
	}

	printf("sample %3d time=", sample);
	printstamp(&result->timestamp);
	putchar(' ');
	if (result->numpmid != N_PMID_B || result->vset[0]->numval != 1) {
	    printf("expected %d (got %d) value sets, with 1 (got %d) value in the first.\n",
	    (int)(N_PMID_B), result->numpmid, result->vset[0]->numval);
	    status = 1;
	}
	else {
	    if (result->vset[0]->vlist[0].value.lval != 1) {
		printf("expected value=1, got value=%d\n", result->vset[0]->vlist[0].value.lval);
		status = 1;
		__pmDumpResult(stdout, result);
	    }
	    else
		printf("correct result\n");
	}
	pmFreeResult(result);
    }

    exit(status);
}
