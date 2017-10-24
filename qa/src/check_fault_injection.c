/*
 * Copyright (c) 2011 Ken McDonell.  All Rights Reserved.
 *
 * Check if libpcp built with -DPM_FAULT_INJECTION ... answer
 * is in the exit status.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/fault.h>
#include <string.h>
#include <errno.h>

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*usage = "[-D debug]";

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:")) != EOF) {
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

    if (errflag || optind != argc) {
	fprintf(stderr, "Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    __pmFaultInject("test", 0);
    /*
     * will only return in the OK case
     */

    return 0;
}
