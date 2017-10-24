/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <unistd.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		sts;
    int		ch;
    int		errflag = 0;
    int		a, b, c;

    __pmSetProgname(argv[0]);

    while ((ch = getopt(argc, argv, "D:?")) != EOF) {
	switch (ch) {


	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind != argc-2) {
	fprintf(stderr, "Usage: %s archive1 archive2\n", pmProgname);
	exit(1);
    }

    a = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind]);
    if (a < 0) {
	fprintf(stderr, "%s: first pmNewContext(..., %s): %s\n", pmProgname, argv[optind], pmErrStr(a));
	exit(1);
    }

    pmDestroyContext(a);

    b = pmNewContext(PM_CONTEXT_HOST, "localhost");
    if (b < 0) {
	fprintf(stderr, "%s: pmNewContext(..., localhost): %s\n", pmProgname, pmErrStr(b));
	exit(1);
    }

    c = pmNewContext(PM_CONTEXT_ARCHIVE, argv[optind+1]);
    if (c < 0) {
	fprintf(stderr, "%s: second pmNewContext(..., %s): %s\n", pmProgname, argv[optind+1], pmErrStr(c));
	exit(1);
    }

    exit(0);
}
