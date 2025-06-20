/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		highres = 0;
    char	*hostspec = "local:";
    int		type = PM_CONTEXT_HOST;
    char	*namespace = PM_NS_DEFAULT;
    int		iterations = 2000;
    int		iter;
    const char	*metric;
    pmID	pmid;
    pmResult_v2	*result;
    pmResult	*hresult;
    struct timespec before, after;
    double	delta;
    static char	*usage = "[-h hostspec] [-H] [-L] [-n namespace] [-i iterations] metric";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:HLn:i:")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'h':	/* hostname for PMCD to contact */
	    type = PM_CONTEXT_HOST;
	    hostspec = optarg;
	    break;

	case 'H':	/* high resolution fetching */
	    highres = 1;
	    break;

	case 'L':	/* local, no PMCD */
	    putenv("PMDA_LOCAL_SAMPLE=");
	    type = PM_CONTEXT_LOCAL;
	    hostspec = NULL;
	    break;

	case 'i':	/* iterations */
	    iterations = atoi(optarg);
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

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (errflag || optind >= argc) {
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if ((namespace != PM_NS_DEFAULT) &&
	(sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	fprintf(stderr, "%s: Cannot load namespace from \"%s\": %s\n",
			pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(type, hostspec)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmGetProgname(), hostspec, pmErrStr(sts));
	else
	    fprintf(stderr, "%s: Cannot make local context connection: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    metric = argv[optind];
    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	fprintf(stderr, "%s: metric ``%s'' : %s\n",
			pmGetProgname(), metric, pmErrStr(sts));
	exit(1);
    }

    pmtimespecNow(&before);
    for (iter=0; iter < iterations; iter++) {
	sts = highres ? pmFetch(1, &pmid, &hresult) :
			pmFetch_v2(1, &pmid, &result);
	if (sts < 0) {
	    fprintf(stderr, "%s: iteration %d : %s\n",
			    pmGetProgname(), iter, pmErrStr(sts));
	    exit(1);
	}
	highres ? pmFreeResult(hresult) : pmFreeResult_v2(result);
    }
    pmtimespecNow(&after);

    if ((sts = pmWhichContext()) < 0) {
	fprintf(stderr, "%s: pmWhichContext: %s\n",
			pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    pmDestroyContext(sts);

    delta = pmtimespecSub(&after, &before);
    printf("%s: metric %s %.2lf fetches/second\n",
	    pmGetProgname(), metric, (double)iterations / delta);

    exit(0);
}
