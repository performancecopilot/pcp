/*
 * Wrapper to execute the version of python(1) defined in the
 * pcp.conf file.
 *
 * Copyright (c) 2016 Ken McDonell.  All Rights Reserved.
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "pmapi.h"

int
main(int argc, char **argv)
{
    char	*python;
    char	*myname;

    python = pmGetConfig("PCP_PYTHON_PROG");
    if (python[0] == '\0') {
	char *altconf = getenv("PCP_CONF");
	char *altdir = getenv("PCP_DIR");
	if (altconf) {
	    if (altdir)
		fprintf(stderr, "%s: PCP_PYTHON_PROG not set in %s/%s\n", argv[0], altdir, altconf);
	    else
		fprintf(stderr, "%s: PCP_PYTHON_PROG not set in %s\n", argv[0], altconf);
	}
	else {
	    if (altdir)
		fprintf(stderr, "%s: PCP_PYTHON_PROG not set in %s/etc/pcp.conf\n", argv[0], altdir);
	    else
		fprintf(stderr, "%s: PCP_PYTHON_PROG not set in /etc/pcp.conf\n", argv[0]);
	}
	return 1;
    }
    myname = argv[0];
    argv[0] = python;
    execvp(argv[0], argv);

    /* should not get here */
    fprintf(stderr, "%s: execvp(%s, ...) failed: %s\n", myname, python, strerror(errno));

    return 1;
}
