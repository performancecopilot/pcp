/*
 * Copyright (c) 2013-2014 Red Hat.
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

static int	quiet;
static char	*service;
static char	*mechanism;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Discovery options"),
    PMOPT_DEBUG,
    { "mechanism", 1, 'm', "NAME", "set the discovery method to use [avahi|...|all]" },
    { "service", 1, 's', "NAME", "discover local services [pmcd|...]" },
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "quiet", 0, 'q', 0, "quiet mode, do not write to stdout" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:m:s:q?",
    .long_options = longopts,
};

static int
discovery(void)
{
    int		i, sts;
    char	**urls;

    if (!service)
	service = PM_SERVER_SERVICE_SPEC;	/* pmcd */

    sts = pmDiscoverServices(service, mechanism, &urls);
    if (sts < 0) {
	fprintf(stderr, "%s: service %s discovery failure: %s\n",
		pmProgname, service, pmErrStr(sts));
	return 2;
    }
    if (sts == 0) {
	if (!quiet)
	    printf("No %s servers discovered\n", service);
	return 1;
    }

    if (!quiet) {
	printf("Discovered %s servers:\n", service);
	for (i = 0; i < sts; ++i)
	    printf("  %s\n", urls[i]);
    }
    free(urls);
    return 0;
}

int
main(int argc, char **argv)
{
    int		c;

    while ((c = pmgetopt_r(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'D':
	    if ((c = __pmParseDebug(opts.optarg)) < 0) {
		pmprintf("%s: unrecognized debug flag specification (%s)\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    } else {
		pmDebug |= c;
	    }
	    break;
	case 's':
	    service = opts.optarg;
	    break;
	case 'm':	/* discovery mechanism */
	    if (strcmp(opts.optarg, "all") == 0)
		mechanism = NULL;
	    else
		mechanism = opts.optarg;
	    break;
	case 'q':	/* no stdout messages */
	    quiet = 1;
	    break;
	default:
	    opts.errors++;
	    break;
	}
    }

    if (opts.optind != argc)
	opts.errors++;

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(1);
    }

    return discovery();
}
