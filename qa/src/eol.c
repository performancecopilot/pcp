/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * eol - exercise _pmLogFindEOF
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static void
printstamp(struct timespec *ts)
{
    static struct tm	*tmp;
    time_t		clock = (time_t)ts->tv_sec;

    tmp = localtime(&clock);
    printf("%02d:%02d:%02d.%09d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)(ts->tv_nsec));
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		ahtype = 0;
    char	*host = NULL;			/* pander to gcc */
    pmHighResLogLabel	label;			/* get hostname for archives */
    char	*namespace = PM_NS_DEFAULT;
    pmHighResResult	*result;
    pmHighResResult	*prev;
    struct timespec	end;
    int		numpmid = 3;
    char	*name[] = { "sample.seconds", "sample.drift", "sample.milliseconds" };

    pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:s:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmGetProgname());
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    host = optarg;
	    break;


	case 'D':	/* debug options */
	    sts = pmSetDebug(optarg);
	    if (sts < 0) {
		fprintf(stderr, "%s: unrecognized debug options specification (%s)\n",
		    pmGetProgname(), optarg);
		errflag++;
	    }
	    break;

	case 'n':	/* alternative name space file */
	    namespace = optarg;
	    break;

	case '?':
	default:
	    errflag++;
	    break;
	}
    }

    if (errflag) {
	fprintf(stderr,
"Usage: %s options ...\n\
\n\
Options\n\
  -a   archive	  metrics source is an archive\n\
  -n   namespace  use an alternative PMNS\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmGetProgname(), namespace, pmErrStr(sts));
	exit(1);
    }

    if (ahtype != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -a is not optional!\n", pmGetProgname());
	exit(1);
    }
    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmGetProgname(), host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetHighResArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    /* non-flag args are argv[optind] ... argv[argc-1] */
    if (optind < argc) {
	numpmid = 0;
	while (optind < argc && numpmid < 3) {
	    name[numpmid] = argv[optind];
	    printf("metric[%d]: %s\n", numpmid, name[numpmid]);
	    optind++;
	    numpmid++;
	}
    }

    sts = pmGetHighResArchiveEnd(&end);
    if (sts < 0) {
	printf("pmGetHighResArchiveEnd: %s\n", pmErrStr(sts));
    }
    else {
	printf("pmGetHighResArchiveEnd time: ");
	printstamp(&end);
	printf("\n");
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    sts = pmSetModeHighRes(PM_MODE_BACK, &end, 0);
    if (sts < 0) {
	printf("pmSetModeHighRes: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmFetchHighResArchive(&result);
    if (sts < 0) {
	printf("pmFetchHighResArchive: %s\n", pmErrStr(sts));
    }
    else {
	printf("last result time (direct): ");
	printstamp(&result->timestamp);
	printf("\n");
	pmFreeHighResResult(result);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    end.tv_sec = PM_MAX_TIME_T;
    end.tv_nsec = 0;
    sts = pmSetModeHighRes(PM_MODE_BACK, &end, 0);
    if (sts < 0) {
	printf("pmSetModeHighRes: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmFetchHighResArchive(&result);
    if (sts < 0) {
	printf("pmFetchHighResArchive: %s\n", pmErrStr(sts));
    }
    else {
	printf("last result time (indirect): ");
	printstamp(&result->timestamp);
	printf("\n");
	pmFreeHighResResult(result);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    prev = (pmHighResResult *)0;
    end.tv_sec = 0;
    sts = pmSetModeHighRes(PM_MODE_FORW, &end, 0);
    if (sts < 0) {
	printf("pmSetModeHighRes: %s\n", pmErrStr(sts));
	exit(1);
    }
    while ((sts = pmFetchHighResArchive(&result)) >= 0) {
	if (prev != (pmHighResResult *)0)
	    pmFreeHighResResult(prev);
	prev = result;
    }
    printf("pmFetchHighResArchive: %s\n", pmErrStr(sts));
    if (prev == (pmHighResResult *)0) {
	printf("no results!\n");
    }
    else {
	printf("last result time (serial): ");
	printstamp(&prev->timestamp);
	printf("\n");
	pmFreeHighResResult(prev);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    exit(0);
}
