/*
 * Copyright (c) 2017 Red Hat.
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

#include "series.h"
#include "load.h"

static int overrides(int, pmOptions *);

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "desc", 0, 'd', 0, "metric descriptor for time series" },
    { "instance", 0, 'i', 0, "instance identifiers for time series" },
    { "labels", 0, 'l', 0, "list all labels for time series" },
    { "load", 0, 'L', 0, "load time series values and metadata" },
    { "loadmeta", 0, 'M', 0, "load time series metadata only" },
    { "metrics", 0, 'm', 0, "metric names for time series" },
    { "query", 0, 'q', 0, "perform a time series query" },
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .flags = PM_OPTFLAG_BOUNDARIES,
    .short_options = "dD:ilLmMqV?",
    .long_options = longopts,
    .short_usage = "[query ... | series ...]",
    .override = overrides,
};

static int
overrides(int opt, pmOptions *opts)
{
    return (opt == 'L');
}

static void
set_command(char **cmdp, const char *set, pmOptions *opts)
{
    if (*cmdp == NULL)
	*cmdp = (char *)set;
    else {
	pmprintf("%s: command is set to %s, cannot reset to %s\n",
		pmGetProgname(), *cmdp, set);
	opts->errors++;
    }
}

int
main(int argc, char *argv[])
{
    int		c, sts, len;
    char	*p, query[BUFSIZ] = {0};
    char	*command = NULL;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {

	case 'd':	/* command line contains series identifiers */
	    set_command(&command, "series/desc", &opts);
	    break;

	case 'i':	/* command line contains series identifiers */
	    set_command(&command, "series/instance", &opts);
	    break;

	case 'l':	/* command line contains series identifiers */
	    set_command(&command, "series/labels", &opts);
	    break;

	case 'L':	/* command line contains load string */
	    set_command(&command, "series/load", &opts);
	    break;

	case 'm':	/* command line contains series identifiers */
	    set_command(&command, "series/metrics", &opts);
	    break;

	case 'M':	/* command line contains load string */
	    set_command(&command, "series/loadmeta", &opts);
	    break;

	case 'q':	/* command line contains query string */
	    set_command(&command, "series/query", &opts);
	    break;

	default:
	    opts.errors++;
	    break;
	}
    }

    if (!command)
	opts.errors++;

    if (opts.errors || (opts.flags & PM_OPTFLAG_EXIT)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    for (c = opts.optind, len = 0; c < argc; c++) {
	p = query + len;
	len += pmsprintf(p, sizeof(query) - len - 2, "%s ", argv[c]);
    }
    if (len)	/* cull the final space */
	query[len - 1] = '\0';

    /* TODO: add code to handle reporting here in pmseries, */
    /* via callback funcs, not embedded printfs in modules. */

    if ((sts = pmSeries(command, query)) < 0) {
	fprintf(stderr, "%s: pmSeries: %s - %s\n",
		    pmGetProgname(), command, pmErrStr(sts));
	exit(1);
    }
    exit(0);
}
