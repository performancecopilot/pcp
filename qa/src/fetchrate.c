/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

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

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:h:Ln:i:")) != EOF) {
	switch (c) {
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
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmNewContext(type, host)) < 0) {
	if (type == PM_CONTEXT_HOST)
	    printf("%s: Cannot connect to PMCD on host \"%s\": %s\n", pmProgname, host, pmErrStr(sts));
	else
	    printf("%s: Cannot make standalone local connection: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind >= argc)
	goto USAGE;

    metric = argv[optind];
    if ((sts = pmLookupName(1, &metric, &pmid)) < 0) {
	printf("%s: metric ``%s'' : %s\n", pmProgname, metric, pmErrStr(sts));
	exit(1);
    }

    gettimeofday(&before, (struct timezone *)0);
    for (iter=0; iter < iterations; iter++) {
	sts = pmFetch(1, &pmid, &result);
	if (sts < 0) {
	    printf("%s: iteration %d : %s\n", pmProgname, iter, pmErrStr(sts));
	    exit(1);
	}
	pmFreeResult(result);
    }
    gettimeofday(&after, (struct timezone *)0);

    if ((sts = pmWhichContext()) < 0) {
	printf("%s: pmWhichContext: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }
    pmDestroyContext(sts);

    delta = __pmtimevalSub(&after, &before);
    printf("%s: metric %s %.2lf fetches/second\n",
    pmProgname, metric, (double)iterations / delta);

    exit(0);
}
