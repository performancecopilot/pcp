/*
 * Copyright (c) 2018 Red Hat.
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
 *
 * Simple program for checking the return value from pmTraversePMNS(3).
 * For details see https://github.com/performancecopilot/pcp/issues/441
 */
#include <stdlib.h>
#include <pcp/pmapi.h>

int verbose = 0;

void
dometric(const char *name, void *arg)
{
    int *counter = (int *)arg;

    if (verbose)
	printf("  %s\n", name);
    *counter += 1;
}

int
main(int argc, char *argv[])
{
    int sts;
    int n = 0;
    // pmDebugOptions.pmns = pmDebugOptions.derive = 1;

    if (argc != 3) {
    	fprintf(stderr, "Usage %s hostname pmnsname\n", argv[0]);
    	fprintf(stderr, "pmnsname may be a non-leaf, leaf or derived metric\n");
	exit(1);
    }

    if ((sts = pmNewContext(PM_CONTEXT_HOST, argv[1])) >= 0) {
	if ((sts = pmTraversePMNS_r(argv[2], dometric, (void *)&n)) >= 0) {
	    printf("Traversing '%s' visited %d leaf nodes, returned %d\n", argv[2], n, sts);
	    if (sts != n) {
	    	printf("FAILED: pmTraversePMNS return code should be number of leaf nodes visited\n");
		exit(1);
	    }
	}
	else
	    fprintf(stderr, "Error: pmTraversePMNS: %d %s\n", sts, pmErrStr(sts));
    }

    if (sts < 0)
	fprintf(stderr, "Error %d: %s\n", sts, pmErrStr(sts));

    exit(sts == n ? 0 : 1);
}
