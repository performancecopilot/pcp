/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2024 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,	/* -D */
    { "limit", 1, 'l', "N", "throttle limit [default -1]" },
    { "max-iter", 1, 'm', "N", "iteration limit [default 100]" },
    { "reset", 1, 'r', "N", "reset each throttle after N iterations" },
    { "reset-all", 1, 'R', "N", "reset all throttles after N iterations" },
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static int	r_after = -1;
static int	R_after = -1;
static int	limit = -1;
static int	max_iter = 100;

static int
overrides(int opt, pmOptions *optsp)
{
    if (opt == 'l') {
	static char	buf[100];	/* static so putenv() works */
	limit = atoi(optsp->optarg);
	snprintf(buf, sizeof(buf), "PCP_NOTIFY_THROTTLE=%s", optsp->optarg);
	putenv(buf);
	return 1;
    }
    if (opt == 'm') {
	max_iter = atoi(optsp->optarg);
	return 1;
    }
    if (opt == 'r') {
	r_after = atoi(optsp->optarg);
	return 1;
    }
    if (opt == 'R') {
	R_after = atoi(optsp->optarg);
	return 1;
    }
    return 0;
}

static pmOptions opts = {
    .short_options = "D:l:m:r:R:?",
    .long_options = longopts,
    .short_usage = "[options] ...",
    .override = overrides,
};

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		i;
    int		line;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }
    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    for (i = 1; i <= max_iter; i++) {
	printf("[%d]", i);
	/*
	 * not using __LINE__ so the test output does not change
	 * as the source code is edited ...
	 */
	line = 100;
	printf(" @%d", line);
	sts = __pmNotifyThrottle(__FILE__, line);
	printf(" %d", sts);
	if (r_after > 0 && (i % r_after) == 0) {
	    sts = __pmResetNotifyThrottle(__FILE__, line, limit);
	    printf(" reset=>%d", sts);
	}
	if (R_after > 0 && (i % R_after) == 0) {
	    sts = __pmResetNotifyThrottle(NULL, 0, limit);
	    printf(" reset=>%d", sts);
	}
	if ((i % 5) == 0) {
	    line = 200;
	    printf(" @%d[5]", line);
	    sts = __pmNotifyThrottle(__FILE__, line);
	    printf(" %d", sts);
	    if (r_after > 0 && (i % r_after) == 0) {
		sts = __pmResetNotifyThrottle(__FILE__, line, limit);
		printf(" reset=>%d", sts);
	    }
	}
	if ((i % 10) == 0) {
	    line = 300;
	    printf(" @%d[10]", line);
	    sts = __pmNotifyThrottle(__FILE__, line);
	    printf(" %d", sts);
	    if (r_after > 0 && (i % r_after) == 0) {
		sts = __pmResetNotifyThrottle(__FILE__, line, limit);
		printf(" reset=>%d", sts);
	    }
	}
	putchar('\n');
    }

    return 0;
}
