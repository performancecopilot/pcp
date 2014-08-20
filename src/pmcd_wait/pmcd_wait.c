/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 1998 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <limits.h>
#include "pmapi.h"
#include "impl.h"

/* possible exit states */
#define EXIT_STS_SUCCESS 	0
#define EXIT_STS_USAGE 		1
#define EXIT_STS_TIMEOUT 	2
#define EXIT_STS_UNIXERR 	3
#define EXIT_STS_PCPERR 	4

static char	*hostname;
static long	delta = 60;
static int	verbose;

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("General options"),
    PMOPT_DEBUG,
    { "host", 1, 'h', "HOST", "wait for PMCD on host" },
    { "interval", 1, 't', "TIME", "maximum interval to wait for PMCD [default 60 seconds]" },
    { "verbose", 0, 'v', 0, "turn on output messages" },
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "D:h:t:v?",
    .long_options = longopts,
};

void 
PrintTimeout(void)
{
    if (verbose) {
	fprintf(stderr, "%s: Failed to connect to PMCD on host \"%s\""
		" in %ld seconds\n",
		pmProgname, hostname, delta);
    }
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    char	env[256];
    long	delta_count;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'v':
	    verbose = 1;
	    break;
	}
    }

    if (opts.optind < argc) {
	pmprintf("%s: Too many arguments\n", pmProgname);
	opts.errors++;
    }

    if (opts.interval.tv_sec != 0) {
	delta = opts.interval.tv_sec;
	if (delta <= 0) {
	    pmprintf("%s: -t argument must be at least 1 second\n",
		pmProgname);
	    opts.errors++;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_STS_USAGE);
    }

    if (opts.nhosts == 0)
	hostname = "local:";
    else
	hostname = opts.hosts[0];

    sts = sprintf(env, "PMCD_CONNECT_TIMEOUT=%ld", delta);
    if (sts < 0) {
	if (verbose) {
	    fprintf(stderr, "%s: Failed to create env string: %s\n",
		pmProgname, osstrerror());
        }
	exit(EXIT_STS_UNIXERR);
    }
    sts = putenv(env);
    if (sts != 0) {
	if (verbose) {
	    fprintf(stderr, "%s: Failed to set PMCD_CONNECT_TIMEOUT: %s\n",
		pmProgname, osstrerror());
        }
	exit(EXIT_STS_UNIXERR);
    }

    delta_count = delta;
    for(;;) {
        sts = pmNewContext(PM_CONTEXT_HOST, hostname);

        if (sts >= 0) {
	    (void)pmDestroyContext(sts);
	    exit(EXIT_STS_SUCCESS);
	}
	if (sts == -ECONNREFUSED || sts == -ENOENT || sts == PM_ERR_IPC) {
	    static const struct timeval onesec = { 1, 0 };

	    delta_count--;
	    if (delta_count < 0) {
		PrintTimeout();	
		exit(EXIT_STS_TIMEOUT);
	    }
	    __pmtimevalSleep(onesec); 
        }
	else if (sts == PM_ERR_TIMEOUT) {
	    PrintTimeout();	
	    exit(EXIT_STS_TIMEOUT);
	}
	else {
	    if (verbose) {
		fprintf(stderr, "%s: Cannot connect to PMCD on host \"%s\": %s\n",
			pmProgname, hostname, pmErrStr(sts));
	    }
	    if (sts > PM_ERR_BASE)
		exit(EXIT_STS_UNIXERR);
	    else
		exit(EXIT_STS_PCPERR);

	}
    }
}
