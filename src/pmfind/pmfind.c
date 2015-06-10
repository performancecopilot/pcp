/*
 * Copyright (c) 2013-2015 Red Hat.
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
#include <signal.h>
#include "pmapi.h"
#include "impl.h"

static int	quiet;
static char	*mechanism;
static char	*options;
static volatile sig_atomic_t discoveryFlags;

static int override(int, pmOptions *);

#ifndef IS_MINGW
static void
handleInterrupt(int sig)
{
    discoveryFlags |= PM_SERVICE_DISCOVERY_INTERRUPTED;
}

static void
setupSignals(void)
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = &handleInterrupt;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGHUP);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGTERM);
    sigaddset(&sa.sa_mask, SIGXFSZ);
    sigaddset(&sa.sa_mask, SIGXCPU);
    sa.sa_flags = SA_RESTART;

    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGXFSZ, &sa, NULL);
    sigaction(SIGXCPU, &sa, NULL);
}
#else
#define setupSignals()	do { } while (0)
#endif

static const char *services[] = {
    PM_SERVER_SERVICE_SPEC,
    PM_SERVER_PROXY_SPEC,
    PM_SERVER_WEBD_SPEC,
};

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Discovery options"),
    PMOPT_DEBUG,
    { "mechanism", 1, 'm', "NAME", "set the discovery method to use [avahi|probe=<subnet>|all]" },
    { "resolve", 0, 'r', 0, "resolve addresses" },
    { "service", 1, 's', "NAME", "discover services [pmcd|pmproxy|pmwebd|...|all]" },
    { "timeout", 1, 't', "N.N", "timeout in seconds" },
    PMAPI_OPTIONS_HEADER("Reporting options"),
    { "quiet", 0, 'q', 0, "quiet mode, do not write to stdout" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:m:rs:t:q?",
    .long_options = longopts,
    .override = override,
};

static int
override(int opt, pmOptions *opts)
{
    (void)opts;
    return (opt == 's' || opt == 't');
}

static int
addOption(const char *option, const char *arg)
{
    size_t existingLen, optionLen, argLen;
    size_t commaLen, equalLen;

    /* The existing length and space for a comma. */
    if (options == NULL) {
	existingLen = 0;
	commaLen = 0;
    }
    else {
	existingLen = strlen(options);
	commaLen = 1;
    }

    /*
     * Additional space needed.
     * We need space for the new option name and an optional argument,
     * separated by an '='.
     */
    optionLen = strlen(option);
    if (arg != NULL) {
	equalLen = 1;
	argLen = strlen(arg);
    }
    else {
	equalLen = 0;
	argLen = 0;
    }

    /* Make room for the existing options plus the new option */
    options = realloc(options, existingLen + commaLen + optionLen + equalLen + argLen);
    if (options == NULL)
	return -ENOMEM;

    /* Add the new option. */
    sprintf(options + existingLen, "%s%s%s%s",
	    commaLen != 0 ? "," : "", option,
	    equalLen != 0 ? "=" : "",
	    argLen != 0 ? arg : "");

    return 0;
}

static int
discovery(const char *spec)
{
    int		i, sts;
    char	**urls;

    sts = __pmDiscoverServicesWithOptions(spec, mechanism, options,
					  &discoveryFlags, &urls);
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
    char	*service = NULL;
    int		c, sts, total;

    /*
     * Set up a handler to catch routine signals, to allow for
     * interruption of the discovery process.
     */
    setupSignals();

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'm':	/* discovery mechanism */
	    if (strcmp(opts.optarg, "all") == 0)
		mechanism = NULL;
	    else
		mechanism = opts.optarg;
	    break;
	case 'q':	/* no stdout messages */
	    quiet = 1;
	    break;
	case 'r':	/* resolve addresses */
	    discoveryFlags |= PM_SERVICE_DISCOVERY_RESOLVE;
	    break;
	case 's':	/* local services */
	    if (strcmp(opts.optarg, "all") == 0)
		service = NULL;
	    else
		service = opts.optarg;
	    break;
	case 't':	/* timeout */
	    addOption("timeout", opts.optarg);
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

    for (c = sts = total = 0; c < sizeof(services)/sizeof(services[0]); c++) {
	if ((discoveryFlags & PM_SERVICE_DISCOVERY_INTERRUPTED) != 0)
	    break;
	sts |= discovery(services[c]);
	total += (sts != 0);
    }

    /*
     * Exit status indicates total failure - success indicates
     * something (any service, any mechanism) was discovered.
     */
    return total == sizeof(services)/sizeof(services[0]) ? sts : 0;
}
