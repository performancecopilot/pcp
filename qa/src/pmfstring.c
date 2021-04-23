/*
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Ken McDonell.  All Rights Reserved.
 *
 * parse stdin using pmfstring(3)
 */

#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,	/* -D */
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D?",
    .long_options = longopts,
    .short_usage = "[options]",
};

int
main(int argc, char **argv)
{
    int		c;
    char	*buf;
    int		sts;

    pmSetProgname(argv[0]);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	;
    }

    if (opts.flags & PM_OPTFLAG_EXIT) {
	pmflush();
	pmUsageMessage(&opts);
	exit(0);
    }

    /* non-flag args are argv[opts.optind] ... argv[argc-1] */
    while (opts.optind < argc) {
	printf("extra argument[%d]: %s\n", opts.optind, argv[opts.optind]);
	opts.optind++;
    }

    while ((sts = pmfstring(stdin, &buf)) != -1) {
	if (sts < 1) {
	    printf("pmfstring -> %d\n", sts);
	    break;
	}
	printf("pmfstring -> %d \"%s\"\n", sts, buf);
	free(buf);
    }

    return 0;
}
