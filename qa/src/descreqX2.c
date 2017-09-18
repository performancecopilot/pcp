/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * 2 DESC_REQs back-to-back ... trying to understand www.sgi.com PMDA deaths
 */

#include <stdio.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>

int
main(int argc, char **argv)
{
    int		c;
    int		fd;
    int		ctx;
    int		errflag = 0;
    int		e;
    int		sts;
    __pmContext	*ctxp;
    __pmPDU	*pb;
    pmID	pmid;
    char	*name = "sample.seconds";

    __pmSetProgname(argv[0]);

    if (argc > 1) {
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

	if (errflag || optind > argc) {
	    fprintf(stderr, "Usage: %s [-D]\n", pmProgname);
	    exit(1);
	}
    }

    if ((ctx = pmNewContext(PM_CONTEXT_HOST, "localhost")) < 0) {
	fprintf(stderr, "pmNewContext: %s\n", pmErrStr(ctx));
	exit(1);
    }

    if ((ctxp = __pmHandleToPtr(ctx)) == NULL) {
	fprintf(stderr, "__pmHandleToPtr failed: eh?\n");
	exit(1);
    }

    fd = ctxp->c_pmcd->pc_fd;
    PM_UNLOCK(ctxp->c_lock);

    if ((e = pmLoadNameSpace(PM_NS_DEFAULT)) < 0) {
	fprintf(stderr, "pmLoadNameSpace: %s\n", pmErrStr(e));
	exit(1);
    }

    if ((e = pmLookupName(1, &name, &pmid)) < 0) {
	printf("pmLookupName: Unexpected error: %s\n", pmErrStr(e));
	exit(1);
    }

    if ((e = __pmSendDescReq(fd, FROM_ANON, pmid)) < 0) {
	fprintf(stderr, "Error: SendDescReqX1: %s\n", pmErrStr(e));
	exit(1);
    }

    if ((e = __pmSendDescReq(fd, FROM_ANON, pmid)) < 0) {
	fprintf(stderr, "Error: SendDescReqX2: %s\n", pmErrStr(e));
	exit(1);
    }

    if ((e = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb)) < 0)
	fprintf(stderr, "Error: __pmGetPDUX1: %s\n", pmErrStr(e));
    else
	fprintf(stderr, "__pmGetPDUX1 -> 0x%x\n", e);

    if ((e = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb)) < 0)
	fprintf(stderr, "Error: __pmGetPDUX2: %s\n", pmErrStr(e));
    else
	fprintf(stderr, "__pmGetPDUX2 -> 0x%x\n", e);

    exit(0);
}

