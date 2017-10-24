/*
 * Exercise __pmProcessUnpickArgs() 
 *
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    __pmExecCtl_t	*argp = NULL;

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
"Usage: %s [options]\n\
\n\
Options:\n\
  -D debug[,...] set PCP debugging option(s)\n",
                pmProgname);
        exit(1);
    }

    if ((sts = __pmProcessUnpickArgs(&argp, argv[optind])) < 0) {
	fprintf(stderr, "__pmProcessUnpickArgs: %s\n", pmErrStr(sts));
	exit(1);
    }

    if ((sts = __pmProcessExec(&argp, PM_EXEC_TOSS_STDIN, PM_EXEC_WAIT)) != 0) {
	fprintf(stderr, "__pmProcessExec: %d\n", sts);
	exit(1);
    }

    exit(0);
}
