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
    char	*host = "localhost";
    int		type = PM_CONTEXT_HOST;
    char	*namespace = PM_NS_DEFAULT;
    int		iterations = 2000;
    int		iter;
    char	*metric;
    pmID	pmid;
    pmResult	*result;
    struct timeval      before, after;
    double	delta;
    static char	*usage = "[-h hostname] [-L] [-n namespace] [-i iterations] metric";

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:Ln:i:")) != EOF) {
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
	    host = optarg;
	    break;

	case 'L':	/* local, no PMCD */
	    putenv("PMDA_LOCAL_SAMPLE=");
	    type = PM_CONTEXT_LOCAL;
	    host = NULL;
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

    if (errflag) {
USAGE:
	fprintf(stderr, "Usage: %s %s\n", pmGetProgname(), usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmGetProgname(), host, pmErrStr(sts));
	else
	    printf("%s: Cannot make standalone local connection: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind >= argc)
	goto USAGE;

    metric = argv[optind];
    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	printf("%s: metric ``%s'' : %s\n", pmGetProgname(), metric, pmErrStr(sts));
	exit(1);
    }

    gettimeofday(&before, (struct timezone *)0);
    for (iter=0; iter < iterations; iter++) {
	sts = pmFetch(1, &pmid, &result);
	if (sts < 0) {
	    printf("%s: iteration %d : %s\n", pmGetProgname(), iter, pmErrStr(sts));
	    exit(1);
	}
	pmFreeResult(result);
    }
    gettimeofday(&after, (struct timezone *)0);

    if ((sts = pmWhichContext()) < 0) {
	printf("%s: pmWhichContext: %s\n", pmGetProgname(), pmErrStr(sts));
	exit(1);
    }
    pmDestroyContext(sts);

    delta = pmtimevalSub(&after, &before);
    printf("%s: metric %s %.2lf fetches/second\n",
    pmGetProgname(), metric, (double)iterations / delta);

    exit(0);
}
