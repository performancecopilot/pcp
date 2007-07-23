/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: interp2.c,v 1.1 2002/10/22 00:03:20 kenmcd Exp $"

/*
 * interp2 - random offset PM_MODE_INTERP exercises
 */

#include <unistd.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void
mung(struct timeval *start, struct timeval *end,
     int pct, struct timeval *mid)
{
    double	tstart;
    double	tend;
    double	twant;

    tstart = start->tv_sec + (double)start->tv_usec / 1000000;
    tend = end->tv_sec + (double)end->tv_usec / 1000000;
    twant = (pct * tend + (100 - pct) * tstart) / 100;
    mid->tv_sec = (long)twant;
    mid->tv_usec = (long)(1000000 * (twant - mid->tv_sec));
}

static void
printstamp(struct timeval *tp)
{
    static struct tm	*tmp;

    tmp = localtime(&tp->tv_sec);
    printf("%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, tp->tv_usec/1000);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	*p;
    int		errflag = 0;
    int		ahtype = 0;
    char	*host;
    pmLogLabel	label;			/* get hostname for archives */
    char	*namespace = PM_NS_DEFAULT;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;
    pmResult	*result;
    struct timeval tend;
    struct timeval twant;
    int		msec;
    int		forw;
    int		back;
    int		pct;
    int		numpmid = 3;
    pmID	pmid[3];
    char	*name[] = { "sample.seconds", "sample.drift", "sample.milliseconds" };
    extern int	__pmLogReads;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((c = getopt(argc, argv, "a:D:n:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    host = optarg;
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


	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive log\n\
  -D   debug	  standard PCP debug flag\n\
  -n   namespace  use an alternative PMNS\n",
		pmProgname);
	exit(1);
    }

#ifdef REALAPP
    __pmGetLicense(-1, pmProgname, UIMODE);
#endif

    if ((sts = pmLoadNameSpace(namespace)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

#ifdef MALLOC_AUDIT
    _malloc_reset_();
    atexit(_malloc_audit_);
#endif

    if (ahtype == 0) {
	fprintf(stderr, "%s: -a is not optional!\n", pmProgname);
	exit(1);
    }
    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmProgname, pmErrStr(sts));
	exit(1);
    }

    sts = pmGetArchiveEnd(&tend);
    if (sts < 0) {
	printf("pmGetArchiveEnd: %s\n", pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind < argc) {
	numpmid = 0;
	while (optind < argc && numpmid < 3) {
	    name[numpmid] = argv[optind];
	    printf("metric[%d]: %s\n", numpmid, name[numpmid]);
	    optind++;
	    numpmid++;
	}
    }

    sts = pmLookupName(numpmid, name, pmid);
    if (sts < 0) {
	printf("pmLookupName: %s\n", pmErrStr(sts));
	exit(1);
    }

    printf("start: ");
    printstamp(&label.ll_start);
    putchar('\n');
    printf("end: ");
    printstamp(&tend);
    putchar('\n');
    mung(&label.ll_start, &tend, 2, &twant);
#if 0
    msec = 1000 * (twant.tv_sec - label.ll_start.tv_sec) +
		(twant.tv_usec - label.ll_start.tv_usec) / 1000;
#else
    msec = 100;
#endif
    printf("step: %d msec\n", msec);
    for (pct = 0; pct <= 100; pct += 10) {
	__pmLogReads = 0;
	mung(&label.ll_start, &tend, pct, &twant);
	printf("%3d%% ", pct);
	printstamp(&twant);
	forw = back = 0;
	sts = pmSetMode(PM_MODE_INTERP, &twant, msec);
	if (sts < 0) {
	    printf("pmSetMode: %s\n", pmErrStr(sts));
	    exit(1);
	}
	while (pmFetch(numpmid, pmid, &result) >= 0) {
	    forw++;
	    pmFreeResult(result);
	}
	printf("%4d forw + ", forw);
	sts = pmSetMode(PM_MODE_INTERP, &twant, -msec);
	if (sts < 0) {
	    printf("pmSetMode: %s\n", pmErrStr(sts));
	    exit(1);
	}
	while (pmFetch(numpmid, pmid, &result) >= 0) {
	    back++;
	    pmFreeResult(result);
	}
	printf("%4d back = %d, %d log reads\n",
	    back, forw + back, __pmLogReads);
    }

    exit(0);
    /*NOTREACHED*/
}
