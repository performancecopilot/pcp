/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2025 Ken McDonell.  All Rights Reserved.
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <pcp/archive.h>

static pmLongOptions longopts[] = {
    PMOPT_ARCHIVE,	/* -a */
    PMOPT_DEBUG,	/* -D */
    PMOPT_HOST,		/* -h */
    { "", 1, 'f', "FLAG", "pmNewContext() flags: 0 for METADATA_ONLY, 1 for LAST_VOLUME" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "a:D:f:h:",
    .long_options = longopts,
    .short_usage = "[options] ...",
};

int
main(int argc, char **argv)
{
    int		c;
    int		flags = 0;
    int		ctx;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	    case 'f':
		if (strcmp(opts.optarg, "0") == 0)
		    flags |= PM_CTXFLAG_METADATA_ONLY;
		else if (strcmp(opts.optarg, "1") == 0)
		    flags |= PM_CTXFLAG_LAST_VOLUME;
		else {
		    fprintf(stderr, "Error: bad value (%s) for -f\n", opts.optarg);
		    exit(EXIT_FAILURE);
		}
		break;
	}
    }

    if (opts.errors || opts.optind < argc) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (opts.narchives == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE | flags, opts.archives[0])) < 0) {
	    fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		    pmGetProgname(), opts.archives[0],  pmErrStr(ctx));
	    exit(EXIT_FAILURE);
	}
    }
    else if (opts.narchives > 0) {
	fprintf(stderr, "%s: at most one archive allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    if (opts.nhosts == 1) {
	if ((ctx = pmNewContext(PM_CONTEXT_HOST, opts.hosts[0])) < 0) {
	    fprintf(stderr, "%s: Cannot connect to pmcd on host \"%s\": %s\n",
		    pmGetProgname(), opts.hosts[0],  pmErrStr(ctx));
	    exit(EXIT_FAILURE);
	}
    }
    else if (opts.nhosts > 0) {
	fprintf(stderr, "%s: at most one host allowed\n", pmGetProgname());
	exit(EXIT_FAILURE);
    }

    return 0;
}
