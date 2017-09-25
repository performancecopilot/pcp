/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

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

    if (errflag || optind != argc-1) {
	fprintf(stderr,
"Usage: %s [options] config-file-or-path\n\
\n\
Options:\n\
  -D debugspec     set PCP debugging options (only derive makes sense)\n",
                pmProgname);
        exit(1);
    }

    sts = pmLoadDerivedConfig(argv[optind]);
    printf("pmLoadDerivedConfig(%s) returns: ", argv[optind]);
    if (sts >= 0)
	printf("%d\n", sts);
    else
	printf("%s\n", pmErrStr(sts));

    return 0;
}
