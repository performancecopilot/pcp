/*
 * TOOLNAME - one line summary
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

#ident "$Id: badloglabel.c,v 1.1 2002/10/22 07:02:33 kenmcd Exp $"

#include <unistd.h>
#include <stdlib.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		sts;
    int		ch;
    char	*p;
    int		errflag = 0;
    int		a, b, c;
    extern char	*optarg;
    extern int	optind;
    extern int	pmDebug;

    /* trim command name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    while ((ch = getopt(argc, argv, "D:?")) != EOF) {
	switch (ch) {

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
    /*NOTREACHED*/
}
