/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * exercise outside feasible log range operations ... before start
 * and after end
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static int	vflag;

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		ctx;
    int		errflag = 0;
    char	*archive = "foo";
    char	*namespace = PM_NS_DEFAULT;
    static char	*usage = "[-a archive] [-n namespace] [-v]";
    pmResult		*resp;
    int			resnum = 0;
    pmID		pmid = 0;
    struct timeval	when;
    struct timeval	first;
    struct timeval	last;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:v")) != EOF) {
	switch (c) {

	case 'a':	/* archive */
	    archive = optarg;
	    break;

#ifdef PCP_DEBUG
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
#endif

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case 'v':	/* verbose output */
	    vflag++;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	printf("Usage: %s %s\n", pmProgname, usage);
	exit(1);
    }

    if (namespace != PM_NS_DEFAULT && (sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, archive)) < 0) {
	printf("%s: Cannot connect to archive \"%s\": %s\n", pmProgname, archive, pmErrStr(ctx));
	exit(1);
    }

    when.tv_sec = 0;
    when.tv_usec = 0;
    if ((sts = pmSetMode(PM_MODE_FORW, &when, 0)) < 0) {
	printf("%s: pmSetMode: %s\n", pmProgname, pmErrStr(sts));
	exit(1);
    }

    printf("\nPass 1: forward scan\n");
    for (;;) {
	if ((sts = pmFetchArchive(&resp)) < 0) {
	    if (sts != PM_ERR_EOL)
		printf("pmFetchArchive: %s\n", pmErrStr(sts));
	    break;
	}
	if (resnum == 0) {
	    first = resp->timestamp;
	    pmid = resp->vset[0]->pmid;
	}
	else
	    last = resp->timestamp;
	resnum++;
	if (vflag)
	    __pmDumpResult(stdout, resp);
	pmFreeResult(resp);
    }
    printf("Found %d samples, with %d log reads\n", resnum, __pmLogReads);

    printf("\nPass 2: before start of log scan\n");
    first.tv_sec--;
    printf("  2.1: FORWARD, expect success\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_FORW, &first, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchArchive(&resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }
    printf("  2.2: BACKWARD, expect EOL\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_BACK, &first, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchArchive(&resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }
    printf("  2.3: INTERP, expect EOL\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_INTERP, &first, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }

    printf("\nPass 3: after end of log scan\n");
    last.tv_sec++;
    printf("  3.1: FORWARD, expect EOL\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_FORW, &last, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchArchive(&resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }
    printf("  3.2: BACKWARD, expect success\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_BACK, &last, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetchArchive(&resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }
    printf("  3.3: INTERP, expect EOL\n");
    __pmLogReads = 0;
    if ((sts = pmSetMode(PM_MODE_INTERP, &last, 0)) < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmFetch(1, &pmid, &resp)) < 0) {
	printf("    %s with %d log_reads\n", pmErrStr(sts), __pmLogReads);
    }
    else {
	if (vflag)
	    __pmDumpResult(stdout, resp);
	printf("    found 1 with %d log reads\n", __pmLogReads);
	pmFreeResult(resp);
    }

    exit(0);
}
