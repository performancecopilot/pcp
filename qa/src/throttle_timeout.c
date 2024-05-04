/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2024 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

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

/*
 * examples from the __pmNotifyThrottle man page
 */
int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		i;
    int		lineno;
    time_t	first_throttle;
    struct timespec	delay = {0, 250000000};

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

    for (i = 1; i < 15; i++) {
	if ((sts = __pmNotifyThrottle(__FILE__, __LINE__)) < 2) {
	    fprintf(stderr, "Some error message\n");
	    if (sts == 1)
		fprintf(stderr, "[further messages will be suppressed]\n");
	}
    }

    while (1) {
	lineno = __LINE__ + 1;
	if ((sts = __pmNotifyThrottle(__FILE__, lineno)) < 2) {
	    pmNotifyErr(LOG_INFO, "Some error message");
	    if (sts == 1) {
		first_throttle = time(NULL);
		pmNotifyErr(LOG_INFO, "[further messages will be suppressed]");
	    }
	}
	else if (sts == 2) {
	    /* 5 secs, not 10 mins as per the man page */
	    if (time(NULL) - first_throttle >= 5) {
		sts = __pmResetNotifyThrottle(__FILE__, lineno, -1);
		pmNotifyErr(LOG_INFO, "[%d messages were suppressed]", sts);
		break;
	    }
	}
	nanosleep(&delay, NULL);
    }

    return 0;
}

