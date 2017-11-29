/*
 * Copyright (c) 2016 Red Hat.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <pcp/pmapi.h>

static pmLongOptions longopts[] = {
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};
static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[options] name=expr ...",
};

int
main(int argc, char **argv)
{
    char *name, *expr;
    char *errmsg;

    pmGetOptions(argc, argv, &opts);
    if (opts.errors) {
	pmUsageMessage(&opts);
	return 1;
    }

    while (opts.optind < argc) {
	/* non-flag args are argv[optind] ... argv[argc-1] */
	name = expr = argv[opts.optind];
	if ((name = strsep(&expr, "=")) == NULL) {
	    fprintf(stderr, "%s: invalid name=expr \"%s\"\n", pmGetProgname(), name);
	    return 1;
	}

	if (pmRegisterDerivedMetric(name, expr, &errmsg) < 0) {
	    fprintf(stderr, "%s: %s", pmGetProgname(), errmsg);
	} else {
	    printf("%s: registered \"%s\" as: \"%s\"\n", pmGetProgname(), name, expr);
	}
	opts.optind++;
    }

    return 0;
}
