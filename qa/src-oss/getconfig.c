/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 */

/*
 * Check pmConfig()
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*usage = "[-D debug] configvar ...";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
	switch (c) {

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

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind >= argc) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    while (optind < argc) {
	printf("%s -> ", argv[optind]);
	printf("%s\n", pmGetConfig(argv[optind]));
	optind++;
    }

    exit(0);
}
