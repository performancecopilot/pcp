/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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

static void
usage()
{
    fprintf(stderr, "Usage: pmhostname [hostname]\n");
}

int
main(int argc, char **argv)
{
    char	*name, *hename;
    char       	host[MAXHOSTNAMELEN];
    __pmHostEnt	*hep;
    int		c;
    int		sts;
    int		errflag = 0;

    __pmSetProgname(argv[0]);

    while ((c = getopt(argc, argv, "D:?")) != EOF) {
	switch (c) {

	    case 'D':	/* debug flag */
		sts = __pmParseDebug(optarg);
		if (sts < 0) {
		    fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
			pmProgname, optarg);
		    errflag++;
		}
		else
		    pmDebug |= sts;
		break;

	    case '?':
		if (errflag == 0) {
		    usage();
		    exit(0);
		}
	}
    }
    if (errflag || argc > optind+1) {
	usage();
	exit(1);
    }

    if (argc == optind) {
	if (gethostname(host, MAXHOSTNAMELEN) < 0) {
	    __pmNotifyErr(LOG_ERR, "%s: gethostname failure\n", pmProgname);
	    exit(1);
	}
	name = host;
    }
    else
	name = argv[optind];

    hep = __pmGetAddrInfo(name);
    if (hep == NULL) {
        printf("%s\n", name);
    }
    else {
	hename = __pmHostEntGetName(hep);
        printf("%s\n", hename ? hename : name);
    }

    exit(0);
}
