/*
 * Exercise __pmProcessAddArg(), __pmProcessExec(), __pmProcessPipe(),
 * and __pmProcessPipeClose().
 *
 * Copyright (c) 2018 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <sys/types.h>

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
    int		nargc = 0;
    char	**nargv;
    FILE	*fin = NULL;
    FILE	*pin;
    FILE	*pout;

    /* trim cmd name of leading directory components */
    pmSetProgname(argv[0]);

    setlinebuf(stdout);
    setlinebuf(stderr);

    while ((c = getopt(argc, argv, "D:pP:?")) != EOF) {
	switch (c) {

	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'p':	/* __pmProcessPipe() reading or __pmProcessCreate() */
	    pipein++;
	    break;

	case 'P':	/* __pmProcessPipe(), writing or __pmProcessCreate() */
	    pipeout++;
	    if ((fin = fopen(optarg, "r")) == NULL) {
		fprintf(stderr, "%s: cannot open \"%s\" for reading: \"%s\"\n",
		    pmGetProgname(), optarg, pmErrStr(-errno));
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
                pmGetProgname());
        exit(1);
    }

    nargv = (char **)malloc((argc+1)*sizeof(char *));
    nargv[0] = argv[0];
    nargc = 1;
    h = NULL;
    while (optind < argc) {
	sts = __pmProcessAddArg(&h, argv[optind]);
	if (pmDebugOptions.desperate) printf("sts=%d h=%p\n", sts, h);
	if (h == NULL) {
	    printf("__pmProcessAddArg: failed (handle is NULL) at argv[%d]: \"%s\"\n", optind-1, argv[optind]);
	    exit(1);
	}
	nargv[nargc++] = argv[optind];
	optind++;
    }
    nargv[nargc] = NULL;

    fflush(stdout);
    fflush(stderr);

    if (pipein && !pipeout) {
	sts = __pmProcessPipe(&h, "r", PM_EXEC_TOSS_NONE, &pin);
	printf("__pmProcessPipe(..., \"r\", ...) -> %d", sts);
	if (sts < 0) {
	    printf(": %s\n", pmErrStr(sts));
	}
	else {
	    if (pmDebugOptions.desperate) printf(" fileno(pin)=%d", fileno(pin));
	    putchar('\n');
	    printf("--- start pipe output ---\n");
	    while ((c = fgetc(pin)) != EOF) {
		putchar(c);
	    }
	    printf("--- end ---\n");
	    sts = __pmProcessPipeClose(pin);
	    printf("__pmProcessPipeClose() -> %d", sts);
	    report_status(sts);
	    putchar('\n');
	}
    }
    else if (pipeout && !pipein) {
	sts = __pmProcessPipe(&h, "w", PM_EXEC_TOSS_NONE, &pout);
	printf("__pmProcessPipe(..., \"w\", ...) -> %d", sts);
	if (sts < 0) {
	    printf(": %s\n", pmErrStr(sts));
	}
	else {
	    if (pmDebugOptions.desperate) printf(" fileno(pout)=%d", fileno(pout));
	    putchar('\n');
	    printf("--- start pipe input ---\n");
	    while ((c = fgetc(fin)) != EOF) {
		putchar(c);
		if ((sts = fputc(c, pout)) != (int)c) {
		    fprintf(stderr, "\nfputc('%c', [%d]) failed: %d %s\n", c & 0xff, fileno(pout), ferror(pout), osstrerror());
		    break;
		}
	    }
	    fclose(fin);
	    printf("--- end ---\n");
	    sts = __pmProcessPipeClose(pout);
	    printf("__pmProcessPipeClose() -> %d", sts);
	    report_status(sts);
	    putchar('\n');
	}
    }
    else if (pipein && pipeout) {
	int	fromChild, toChild;
	sts = __pmProcessCreate(nargv, &fromChild, &toChild);
	printf("__pmProcessCreate(...) -> %d fromChild=%d toChild=%d", sts, fromChild, toChild);
	if (sts < 0) {
	    printf(": %s\n", pmErrStr(sts));
	}
	else {
	    pin = fdopen(fromChild, "r");
	    pout = fdopen(toChild, "w");
	    putchar('\n');
	    printf("--- start pipe input ---\n");
	    while ((c = fgetc(fin)) != EOF) {
		putchar(c);
		if ((sts = fputc(c, pout)) != (int)c) {
		    fprintf(stderr, "\nfputc('%c', [%d]) failed: %d %s\n", c & 0xff, fileno(pout), ferror(pout), osstrerror());
		    break;
		}
	    }
	    printf("--- end ---\n");
	    fclose(fin);
	    fclose(pout);
	    printf("--- start pipe output ---\n");
	    while ((c = fgetc(pin)) != EOF) {
		putchar(c);
	    }
	    printf("--- end ---\n");
	    fclose(pin);
	}
    }
    else {
	sts = __pmProcessExec(&h, PM_EXEC_TOSS_NONE, PM_EXEC_WAIT);
	printf("__pmProcessExec -> %d", sts);
	report_status(sts);
	putchar('\n');
    }

    free(nargv);

    return(0);
}
