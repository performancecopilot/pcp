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

static void
usage(void)
{
    fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -m mechanism  set the discovery method to use [avahi|...|all]\n\
  -q            quiet mode, do not write to stdout\n\
  -s service    discover local services [pmcd|...]\n",
	    pmProgname);
}

int
main(int argc, char **argv)
{
    int		c;
    int		sts;
    int		errflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:m:s:q?")) != EOF) {
	switch (c) {

	    case 'D':	/* debug flag */
		if ((sts = __pmParseDebug(optarg)) < 0) {
		    fprintf(stderr,
			"%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		    errflag++;
		} else {
		    pmDebug |= sts;
		}
		break;

	    case 'm':	/* discovery mechanism */
		if (strcmp(optarg, "all") == 0)
		    mechanism = NULL;
		else
		    mechanism = optarg;
		break;

	    case 'q':	/* no stdout messages */
		quiet = 1;
		break;

	    case 's':	/* discover named service */
		service = optarg;
		break;

	    case '?':
		if (errflag == 0) {
		    usage();
		    exit(0);
		}
		break;
	}
    }

    if (optind != argc)
	errflag++;

    if (errflag) {
	usage();
	exit(1);
    }

    return discovery();
}
