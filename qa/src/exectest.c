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
    if (status == 0)
	return;
    else if (status < 0)
	printf(" %s", pmErrStr(status));
    else if (status >= 2000)
	printf(" unknown cause");
    else if (status >= 1000)
	printf(" signal=%d", status-1000);
    else
	printf(" exit=%d", status);
}

int
main(int argc, char **argv)
{
    __pmExecCtl_t	*h;
    int		sts;
    int		c;
    int		errflag = 0;
    int		pipein = 0;
    int		pipeout = 0;
    FILE	*fin;
    FILE	*fout;

    /* trim cmd name of leading directory components */
    __pmSetProgname(argv[0]);

    setlinebuf(stdout);
    setlinebuf(stderr);

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
	    if ((fin = fopen(optarg, "r")) == NULL) {
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
	sts = __pmProcessPipe(&h, "r", PM_EXEC_TOSS_NONE, &fin);
	printf("__pmProcessPipe(..., \"r\", ...) -> %d", sts);
	if (sts < 0) {
	    printf(": %s\n", pmErrStr(sts));
	}
	else {
	    putchar('\n');
	    while ((c = fgetc(fin)) != EOF) {
		putchar(c);
	    }
	    sts = __pmProcessPipeClose(fin);
	    printf("__pmProcessPipeClose() -> %d", sts);
	    report_status(sts);
	    putchar('\n');
	}
    }
    else if (pipeout) {
	sts = __pmProcessPipe(&h, "w", PM_EXEC_TOSS_NONE, &fout);
	printf("__pmProcessPipe(..., \"w\", ...) -> %d", sts);
	if (sts < 0) {
	    printf(": %s\n", pmErrStr(sts));
	}
	else {
	    putchar('\n');
	    while ((c = fgetc(fin)) != EOF) {
		fputc(c, fout);
	    }
	    sts = __pmProcessPipeClose(fout);
	    printf("__pmProcessPipeClose() -> %d", sts);
	    report_status(sts);
	    putchar('\n');
	}
    }
    else {
	sts = __pmProcessExec(&h, PM_EXEC_TOSS_NONE, PM_EXEC_WAIT);
	printf("__pmProcessExec -> %d", sts);
	report_status(sts);
	putchar('\n');
    }

    return(0);
}
