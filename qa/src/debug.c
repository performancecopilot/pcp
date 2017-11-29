/*
 * Exercise pmSetDebug() and pmClearDebug(), and the deprecated
 * __pmParseDebug() interface.
 *
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/deprecated.h>

#include "libpcp.h"

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -D debug[,...] set PCP debug options\n",
                pmGetProgname());
        exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "%s: Cannot connect to PMCD on localhost: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    while (optind < argc) {
	printf("arg: %s\n", argv[optind]);

	printf("pmSetDebug(\"%s\") ...\n", argv[optind]);
	if ((sts = pmSetDebug(argv[optind])) != 0) {
	    printf("pmSetDebug -> %s\n", pmErrStr(sts));
	}
	__pmDumpDebug(stdout);

	printf("pmClearDebug(\"%s\") ...\n", argv[optind]);
	if ((sts = pmClearDebug(argv[optind])) != 0) {
	    printf("pmClearDebug -> %s\n", pmErrStr(sts));
	}
	__pmDumpDebug(stdout);

	printf("__pmParseDebug(\"%s\") ...\n", argv[optind]);
	sts = __pmParseDebug(argv[optind]);
	printf("__pmParseDebug -> %d\n", sts);
	if (sts > 0)
	    __pmSetDebugBits(sts);
	__pmDumpDebug(stdout);

	printf("pmClearDebug(\"%s\") ...\n", argv[optind]);
	if ((sts = pmClearDebug(argv[optind])) != 0) {
	    printf("pmClearDebug -> %s\n", pmErrStr(sts));
	}
	__pmDumpDebug(stdout);

	optind++;
    }

    return 0;
}
