/*
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
#include <stdio.h>
#include <string.h>
#include "pmapi.h"
#include "impl.h"

/* possible exit states */
#define EXIT_STS_SUCCESS 	0
#define EXIT_STS_USAGE 		1
#define EXIT_STS_TIMEOUT 	2
#define EXIT_STS_UNIXERR 	3
#define EXIT_STS_PCPERR 	4

static char     localhost[MAXHOSTNAMELEN];
static char	*hostname = NULL;
static long	delta = 60;
static int	verbose = 0;


static void
PrintUsage(void)
{
    fprintf(stderr,
"Usage: %s [options] \n\
\n\
Options:\n\
  -h host	wait for PMCD on host\n\
  -t interval   maximum interval to wait for PMCD [default 60 seconds]\n\
  -v		turn on output messages\n",
		pmProgname);
}

static void
ParseOptions(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		errflag = 0;
    char	*msg;
    struct 	timeval delta_time;

    while ((c = getopt(argc, argv, "D:h:t:v?")) != EOF) {
	switch (c) {

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: Unrecognized debug flag specification (%s)\n",
			    pmProgname, optarg);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;


	    case 'h':	/* contact PMCD on this hostname */
		hostname = optarg;
		break;


	    case 't':   /* delta to wait */
		if (pmParseInterval(optarg, &delta_time, &msg) < 0) {
		    fprintf(stderr, "%s: Illegal -t argument (%s)\n", 
			pmProgname, optarg);
		    fputs(msg, stderr);
		    free(msg);
		    errflag++;
		}
		delta = delta_time.tv_sec;
		if (delta <= 0) {
		    fprintf(stderr, "%s: -t argument must be at least 1 second\n",
			pmProgname);
		    errflag++;
		}
		break;

	    case 'v':
		verbose = 1;
		break;

	    default:
	    case '?':
		PrintUsage();
		exit(EXIT_STS_SUCCESS);
	}
    }

    if (optind < argc) {
	fprintf(stderr, "%s: Too many arguments\n", pmProgname);
	errflag++;
    }

    if (errflag) {
	PrintUsage();
	exit(EXIT_STS_USAGE);
    }

}

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
    int		sts;
    char	env[256];
    long	delta_count;

    __pmSetProgname(argv[0]);

    ParseOptions(argc, argv);

    if (hostname == NULL) {
	(void)gethostname(localhost, MAXHOSTNAMELEN);
	localhost[MAXHOSTNAMELEN-1] = '\0';
	hostname = localhost;
    }

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
	if (sts == -ECONNREFUSED || sts == PM_ERR_IPC) {
	    static const struct timeval onesec = { 1, 0};

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
