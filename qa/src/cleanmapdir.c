/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017,2023 Ken McDonell.  All Rights Reserved.
 *
 * Exerciser for __pmCleanMapDir()
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    { "check-special", 1, 'C', "SPECIALNAME", "special filename" },
    PMOPT_DEBUG,	/* -D */
    PMOPT_HELP,		/* -? */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "C:D:?",
    .long_options = longopts,
    .short_usage = "[options] dirname",
};

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char 	*special = NULL;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'C':	/* special filename */
	    if (special != NULL) {
		fprintf(stderr, "%s: at most one -C option allowed\n", pmGetProgname());
		exit(EXIT_FAILURE);
	    }
	    special = opts.optarg;
	    break;	

	}
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    if (opts.errors || opts.optind != argc-1) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    sts = __pmCleanMapDir(argv[opts.optind], special);
    printf("__pmCleanMapDir(%s, %s) => %d\n", argv[opts.optind], special, sts);
 
    return 0;
}
