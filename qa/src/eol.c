/*
 * Copyright (c) 1997-2001 Silicon Graphics, Inc.  All Rights Reserved.
 */

/*
 * eol - exercise _pmLogFindEOF
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>

static void
printstamp(struct timeval *tp)
{
    static struct tm	*tmp;

    tmp = localtime(&tp->tv_sec);
    printf("%02d:%02d:%02d.%03d", tmp->tm_hour, tmp->tm_min, tmp->tm_sec, (int)(tp->tv_usec/1000));
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;
    int		ahtype = 0;
    char	*host = NULL;			/* pander to gcc */
    pmLogLabel	label;				/* get hostname for archives */
    char	*namespace = PM_NS_DEFAULT;
    pmResult	*result;
    pmResult	*prev;
    struct timeval	end;
    int		numpmid = 3;
    char	*name[] = { "sample.seconds", "sample.drift", "sample.milliseconds" };

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "a:D:n:s:?")) != EOF) {
	switch (c) {

	case 'a':	/* archive name */
	    if (ahtype != 0) {
		fprintf(stderr, "%s: at most one of -a and/or -h allowed\n", pmProgname);
		errflag++;
	    }
	    ahtype = PM_CONTEXT_ARCHIVE;
	    host = optarg;
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
  -a   archive	  metrics source is an archive log\n\
  -n   namespace  use an alternative PMNS\n",
		pmProgname);
	exit(1);
    }

    if ((sts = pmLoadASCIINameSpace(namespace, 1)) < 0) {
	printf("%s: Cannot load namespace from \"%s\": %s\n", pmProgname, namespace, pmErrStr(sts));
	exit(1);
    }

    if (ahtype != PM_CONTEXT_ARCHIVE) {
	fprintf(stderr, "%s: -a is not optional!\n", pmProgname);
	exit(1);
    }
    if ((sts = pmNewContext(ahtype, host)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
	    pmProgname, host, pmErrStr(sts));
	exit(1);
    }

    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
	    pmProgname, pmErrStr(sts));
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

    sts = pmGetArchiveEnd(&end);
    if (sts < 0) {
	printf("pmGetArchiveEnd: %s\n", pmErrStr(sts));
    }
    else {
	printf("pmGetArchiveEnd time: ");
	printstamp(&end);
	printf("\n");
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    sts = pmSetMode(PM_MODE_BACK, &end, 0);
    if (sts < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive: %s\n", pmErrStr(sts));
    }
    else {
	printf("last result time (direct): ");
	printstamp(&result->timestamp);
	printf("\n");
	pmFreeResult(result);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    end.tv_sec = 0x7fffffff;
    end.tv_usec = 0;
    sts = pmSetMode(PM_MODE_BACK, &end, 0);
    if (sts < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    sts = pmFetchArchive(&result);
    if (sts < 0) {
	printf("pmFetchArchive: %s\n", pmErrStr(sts));
    }
    else {
	printf("last result time (indirect): ");
	printstamp(&result->timestamp);
	printf("\n");
	pmFreeResult(result);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    putchar('\n');
    prev = (pmResult *)0;
    end.tv_sec = 0;
    sts = pmSetMode(PM_MODE_FORW, &end, 0);
    if (sts < 0) {
	printf("pmSetMode: %s\n", pmErrStr(sts));
	exit(1);
    }
    while ((sts = pmFetchArchive(&result)) >= 0) {
	if (prev != (pmResult *)0)
	    pmFreeResult(prev);
	prev = result;
    }
    printf("pmFetchArchive: %s\n", pmErrStr(sts));
    if (prev == (pmResult *)0) {
	printf("no results!\n");
    }
    else {
	printf("last result time (serial): ");
	printstamp(&prev->timestamp);
	printf("\n");
	pmFreeResult(prev);
    }
    printf("required %d log reads\n", __pmLogReads);
    __pmLogReads = 0;

    exit(0);
}
