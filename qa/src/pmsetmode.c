/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
 *
 * Examples from pmSetMode(3) man page.
 *
 * NOTE: quick and dirty, no error checking
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,	/* -a */
    PMOPT_DEBUG,	/* -D */
    PMOPT_INTERVAL,	/* -t */
    PMOPT_TIMEZONE,	/* -Z */
    PMOPT_HOSTZONE,	/* -z */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:t:Z:z?",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char **argv)
{
    int		c;
    char	**namelist = NULL;
    pmID	*pmidlist = NULL;
    int		numpmid = 0;
    int		sts;

    struct timespec	mytime;
    struct timespec	mydelta = { 10, 0 };
    pmLogLabel		label;
    pmHighResResult	*result;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.narchives != 1) {
	fprintf(stderr, "%s: need one -a option\n", pmGetProgname());
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.optind == argc) {
	fprintf(stderr, "%s: need at least one metric\n", pmGetProgname());
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.interval.tv_sec != 0 || opts.interval.tv_nsec != 0)
	mydelta = opts.interval;

    /* metrics are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	namelist = (char **)realloc(namelist, (numpmid+1)*sizeof(namelist[0]));
	namelist[numpmid++] = argv[opts.optind];
	opts.optind++;
    }
    pmidlist = (pmID *)malloc(numpmid*sizeof(pmidlist[0]));

    if ((sts = pmNewContext(PM_CONTEXT_ARCHIVE, opts.archives[0])) < 0) {
	fprintf(stderr, "pmNewContext: %s: %s\n", opts.archives[0], pmErrStr(sts));
	exit(1);
    }
    if ((sts = pmLookupName(numpmid, (const char **)namelist, pmidlist)) < 0) {
	fprintf(stderr, "pmLookupName: %s ...: %s\n", namelist[0], pmErrStr(sts));
	exit(1);
    }
    free(namelist);

    /* EXAMPLE #1 */
    printf("Forwards ...\n");
    if ((sts = pmGetArchiveLabel(&label)) < 0) {
	fprintf(stderr, "pmGetArchiveLabel: %s\n", pmErrStr(sts));
	exit(1);
    }
    mytime = label.start;
    if ((pmSetMode(PM_MODE_INTERP, &mytime, &mydelta)) < 0) {
	fprintf(stderr, "pmSetMode: start: %s\n", pmErrStr(sts));
	exit(1);
    }
    while ((sts = pmFetchHighRes(numpmid, pmidlist, &result)) >= 0) {
	__pmDumpHighResResult(stdout, result);
	pmFreeHighResResult(result);
    }
    if (sts != PM_ERR_EOL)
	fprintf(stderr, "pmFetch: forward: %s\n", pmErrStr(sts));

    /* EXAMPLE #2 */
    printf("\nBackwards ...\n");
    if ((pmGetArchiveEnd(&mytime)) < 0) {
	fprintf(stderr, "pmGetArchiveEnd: %s\n", pmErrStr(sts));
	exit(1);
    }
    if ((pmSetMode(PM_MODE_BACK, &mytime, NULL)) < 0) {
	fprintf(stderr, "pmSetMode: end: %s\n", pmErrStr(sts));
	exit(1);
    }
    while ((sts = pmFetchHighRes(numpmid, pmidlist, &result)) >= 0) {
	__pmDumpHighResResult(stdout, result);
	pmFreeHighResResult(result);
    }
    if (sts != PM_ERR_EOL)
	fprintf(stderr, "pmFetch: backward: %s\n", pmErrStr(sts));

    free(pmidlist);

    return 0;
}

