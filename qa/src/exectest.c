/*
 * Exercise pmSetDebug() and pmClearDebug(), and the deprecated
 * __pmParseDebug() interface.
 *
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <sys/types.h>
#include <sys/wait.h>

static void
report_status(int status)
{
    if (WIFEXITED(status)) printf(" exit=%d", WEXITSTATUS(status));
    if (WIFSIGNALED(status)) printf(" signal=%d", WTERMSIG(status));
    if (WIFSTOPPED(status)) printf(" stop signal=%d", WSTOPSIG(status));
    if (WIFCONTINUED(status)) printf(" continued");
    if (WCOREDUMP(status)) printf(" core dumped");
}

int
main(int argc, char **argv)
{
    __pmExecCtl_t	*h;
    int		sts;
    int		status;
    int		c;
    int		errflag = 0;
    int		pipein = 0;
    int		pipeout = 0;
    FILE	*f;
    FILE	*fdata;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:pP:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmProgname, optarg);
		errflag++;
	    }
	    break;

	case 'p':	/* __pmProcessPipe(), reading */
	    if (pipein || pipeout) {
		fprintf(stderr, "%s: at most one of -p or -P allowed\n", pmProgname);
		errflag++;
	    }
	    pipein++;
	    break;

	case 'P':	/* __pmProcessPipe(), writing */
	    if (pipein || pipeout) {
		fprintf(stderr, "%s: at most one of -p or -P allowed\n", pmProgname);
		errflag++;
	    }
	    pipeout++;
	    if ((fdata = fopen(optarg, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open \"%s\" for reading: \"%s\"\n",
		    pmProgname, optarg, pmErrStr(-errno));
		exit(1);
	    }
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag || optind == argc) {
	fprintf(stderr,
"Usage: %s [options] execarg ...\n\
\n\
Options:\n\
  -D debug[,...] set PCP debugging option(s)\n\
  -p             read to EOF from __pmProcessPipe\n\
  -P data        read data file and write to __pmProcessPipe\n",
                pmProgname);
        exit(1);
    }

    h = NULL;
    while (optind < argc) {
	sts = __pmProcessAddArg(&h, argv[optind]);
	if (pmDebugOptions.desperate) printf("sts=%d h=%p\n", sts, h);
	if (h == NULL) {
	    printf("__pmProcessAddArg: failed (handle is NULL) at argv[%d]: \"%s\"\n", optind-1, argv[optind]);
	    exit(1);
	}
	optind++;
    }

    if (pipein) {
	f = __pmProcessPipe(&h, "r", &status);
	printf("__pmProcessPipe(..., \"r\", ...) -> %s", f == NULL ? "FAIL" : "OK");
	if (f == NULL)
	    sts = 1;
	else {
	    /* TODO */
	    sts = 0;
	}
    }
    else if (pipeout) {
	f = __pmProcessPipe(&h, "w", &status);
	printf("__pmProcessPipe(..., \"w\", ...) -> %s", f == NULL ? "FAIL" : "OK");
	if (f == NULL)
	    sts = 1;
	else {
	    /* TODO */
	    sts = 0;
	}
    }
    else {
	sts = __pmProcessExec(&h, PM_EXEC_TOSS_NONE, PM_EXEC_WAIT, &status);
	printf("__pmProcessExec -> %d", sts);
    }
    if (sts > 0)
	report_status(status);
    putchar('\n');

    return(0);
}
