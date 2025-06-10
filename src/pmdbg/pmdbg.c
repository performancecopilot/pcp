/*
 * Copyright (c) 2013-2014,2016-2017 Red Hat.
 * Copyright (c) 1995,2002-2003 Silicon Graphics, Inc.  All Rights Reserved.
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

/*
 * pmdbg - help for PCP debug fields
 */

#include <ctype.h>
#include "pmapi.h"
#include "../libpcp/src/pmdbg.h"

static char	*fmt = "%-14.14s  %s";

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    { "list", 0, 'l', 0, "display names and text for all PCP debug options [default action]" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "l?",
    .long_options = longopts,
    .short_usage = "[-l]",
};

int
main(int argc, char **argv)
{
    int		i;
    int		c;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'l':	/* list all debug options [same as default] */
	    break;

	case '?':
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    printf("Performance Co-Pilot Debug Options\n");
    printf(fmt, "Option", "Meaning");
    putchar('\n');
    for (i = 0; i < num_debug; i++) {
	printf(fmt, debug_map[i].name, debug_map[i].text);
	putchar('\n');
    }

    return 0;
}
