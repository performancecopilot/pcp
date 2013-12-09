/*
 * Copyright (c) 2013 Red Hat.
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

static int	discover_pmcd = 0;
static char	*discovery_domain = NULL;

static void
PrintUsage(void)
{
    fprintf(stderr,
"Usage: %s [options]\n\
\n\
Options:\n\
  -p	           discover local pmcd servers\n\
  -d [avahi|all]   set the discovery domain\n",
	    pmProgname);
}

static void
ParseOptions(int argc, char *argv[])
{
    int		c;
    int		errflag = 0;
    char	*opts = "d:p?";

    /* If no options specified, then use these defaults. */
    if (argc == 1) {
	discover_pmcd = 1;
	return;
    }

    while ((c = getopt(argc, argv, opts)) != EOF) {
	switch (c) {

	    case 'd':	/* domain flag */
		if (strcmp(optarg, "all") == 0)
		    discovery_domain = NULL;
		else if (strcmp(optarg, "avahi") == 0)
		    discovery_domain = optarg;
		else {
		    fprintf(stderr, "%s: Unsupported discovery domain '%s'\n",
			    pmProgname, optarg);
		    errflag++;
		}
		break;

	    case 'p':
		discover_pmcd = 1;
		break;

	    case '?':
	    default:
		if (errflag == 0) {
		    PrintUsage();
		    exit(0);
		}
	}
    }

    if (errflag) {
	PrintUsage();
	exit(1);
    }
}

/*****************************************************************************/

int
main(int argc, char **argv)
{
    int exitsts = 0;

    __pmSetProgname(argv[0]);

    ParseOptions(argc, argv);

    if (discover_pmcd) {
	char **urls = NULL;
	int numUrls = 0;
	if ((numUrls = pmDiscoverServices(PM_SERVER_SERVICE_SPEC, NULL, numUrls, &urls)) > 0) {
	    int i;
	    printf("Discovered PMCD servers:\n");
	    for (i = 0; i < numUrls; ++i)
		printf("  %s\n", urls[i]);
	    free(urls);
	}
	else
	    printf("No PMCD servers discovered\n");
    }

    exit(exitsts);
}
