/*
 * Copyright (C) 2007,2008 Silicon Graphics, Inc. All Rights Reserved.
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

#include <stdio.h>
#include <limits.h>
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <pcp/pmda.h>

#include "ibpmda.h"


static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
          "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile write log into logfile rather than using default log name\n"
	  "  -c path to configuration file\n"
	  "  -w write the basic configuration file\n",
          stderr);
    exit(1);
}

int
main(int argc, char **argv)
{
    int err = 0;
    int sep = __pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];
    char *confpath = NULL;
    int opt;
    int writeconf = 0;

    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "ib" "%c" "help", 
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, IB, "ib.log", helppath);

    while ((opt = pmdaGetOpt(argc, argv, "D:c:d:l:w?", &dispatch, &err)) != EOF) {
	switch (opt) {
	case 'c':
	    confpath = optarg;
	    break;
	case 'w':
	    writeconf = 1;
	    break;
	default:
	    err++;
	}
    }

    if (err) {
        usage();
    }

    if (!writeconf) {
	/* If writeconf is specified, then errors should go to stdout
	 * since the PMDA daemon will exit immediately after writing
	 * out the default config file
	 */
	pmdaOpenLog(&dispatch);
    }
    ibpmda_init(confpath, writeconf, &dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
    /*NOTREACHED*/
}
