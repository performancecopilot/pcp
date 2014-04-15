/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"
#include "impl.h"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:?",
    .long_options = longopts,
    .short_usage = "[hostname]",
};

int
main(int argc, char **argv)
{
    char	*name, *hename;
    char       	host[MAXHOSTNAMELEN];
    __pmHostEnt	*hep;

    while (pmGetOptions(argc, argv, &opts) != EOF)
	opts.errors++;

    if (opts.errors || argc > opts.optind + 1) {
	pmUsageMessage(&opts);
	exit(1);
    }

    if (argc == opts.optind) {
	if (gethostname(host, MAXHOSTNAMELEN) < 0) {
	    fprintf(stderr, "%s: gethostname failure\n", pmProgname);
	    exit(1);
	}
	name = host;
    }
    else
	name = argv[opts.optind];

    hep = __pmGetAddrInfo(name);
    if (hep == NULL) {
        printf("%s\n", name);
    }
    else {
	hename = __pmHostEntGetName(hep);
        printf("%s\n", hename ? hename : name);
    }

    exit(0);
}
