/*
 * Call pmLoadDerivedConfig() multiple times ... check locking problem.
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void
dometric(const char *name)
{
    printf("%s\n", name);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "c:D:?")) != EOF) {
	switch (c) {

	case 'c':	/* configfile */
	    sts = pmLoadDerivedConfig(optarg);
	    printf("pmLoadConfig(%s) -> %d\n", optarg, sts);
	    break;	

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

    if (errflag) {
	fprintf(stderr,
"Usage: %s [options] [metrics ...]\n\
\n\
Options:\n\
  -c configfile  file to load derived metric configurations from\n",
                pmProgname);
        exit(1);
    }

    sts = pmNewContext(PM_CONTEXT_HOST, "localhost");
    if (sts < 0) {
	printf("Error: pmNewContext: %s\n", pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    while (optind < argc) {
	sts = pmTraversePMNS(argv[optind], dometric);
	if (sts < 0) {
	    printf("Error: pmTraversePMNS(%s, ...): %s\n", argv[optind], pmErrStr(sts));
    }
	optind++;
    }

    return 0;
}
