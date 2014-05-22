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

static int override(int, pmOptions *);

static const char *services[] = {
    PM_SERVER_SERVICE_SPEC,
    PM_SERVER_PROXY_SPEC,
    PM_SERVER_WEBD_SPEC,
};

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Discovery options"),
    PMOPT_DEBUG,
    { "mechanism", 1, 'm', "NAME", "set the discovery method to use [avahi|...|all]" },
    { "service", 1, 's', "NAME", "discover services [pmcd|pmproxy|pmwebd|...|all]" },
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "quiet", 0, 'q', 0, "quiet mode, do not write to stdout" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:m:s:q?",
    .long_options = longopts,
    .override = override,
};

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    return (opt == 's');
}

static int
discovery(const char *spec)
{
    int		i, sts;
    char	**urls;

    sts = pmDiscoverServices(spec, mechanism, &urls);
    if (sts < 0) {
	fprintf(stderr, "%s: service %s discovery failure: %s\n",
		pmProgname, spec, pmErrStr(sts));
	return 2;
    }
    if (sts == 0) {
	if (!quiet)
	    printf("No %s servers discovered\n", spec);
	return 1;
    }

    if (!quiet) {
	printf("Discovered %s servers:\n", spec);
	for (i = 0; i < sts; ++i)
	    printf("  %s\n", urls[i]);
    }
    free(urls);
    return 0;
}

int
main(int argc, char **argv)
{
    int		c, total;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 's':	/* local services */
	    if (strcmp(opts.optarg, "all") == 0)
		service = NULL;
	    else
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

    if (service)
	return discovery(service);

    for (c = total = 0; c < sizeof(services)/sizeof(services[0]); c++)
	total += (discovery(services[c]) != 0);

    /*
     * Exit status indicates total failure - success indicates
     * something (any service, any mechanism) was discovered.
     */
    return total == sizeof(services)/sizeof(services[0]);
}
