/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 */

/*
 * Check pmGetConfig(3) and pmGetOptionalConfig(3)
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		mode = PM_FATAL_ERR;
    int		errflag = 0;
    char	*value;
    char	*usage = "[-D debug] [-o] configvar ...";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:o")) != EOF) {
	switch (c) {

	case 'o':	/* optional mode flag */
	    mode = PM_RECOV_ERR;
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
	if (mode == PM_FATAL_ERR) {
	    printf("%s\n", pmGetConfig(argv[optind]));
	} else if ((value = pmGetOptionalConfig(argv[optind])) == NULL) {
	    printf("NULL\n");
	    exit(1);
	} else {
	    printf("%s\n", value);
	}
	optind++;
    }

    exit(0);
}
