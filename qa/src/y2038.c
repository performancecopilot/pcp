/*
 * Y2038 time_t overflow issue
 *
 * Try to print the date: Sun Jan  1 12:00:00 2040 (UTC)
 *
 * References:
 * https://www.gnu.org/software/libc/manual/html_node/64_002dbit-time-symbol-handling.html
 * https://sourceware.org/glibc/wiki/Y2038ProofnessDesign
 *
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2024 Ken McDonell.  All Rights Reserved.
 */

#include <time.h>
#include <sys/time.h>
#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char **argv)
{
    int		c;
    			  /*      HH DD MM 2040 */
    struct tm	then_tm = { 0, 0, 12, 1, 0, 140, 0, 0, 0 };
    time_t	then;

    pmSetProgname(argv[0]);
    putenv("TZ=UTC");

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors || opts.optind < argc) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    then = mktime(&then_tm);
    printf("Date: %s", ctime(&then));

    printf("sizeof(time_t): %d\n", (int)sizeof(time_t));
    printf("sizeof(struct timeval): %d\n", (int)sizeof(struct timeval));
    printf("sizeof(struct pmTimeval): %d\n", (int)sizeof(struct pmTimeval));
    printf("sizeof(struct timespec): %d\n", (int)sizeof(struct timespec));
    printf("sizeof(struct pmTimespec): %d\n", (int)sizeof(struct pmTimespec));
    printf("sizeof(__pmTimestamp): %d\n", (int)sizeof(__pmTimestamp));

    return 0;
}
