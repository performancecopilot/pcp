/*
 * Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <stdlib.h>
#include <time.h>
#include "pmapi.h"

int
main(int argc, char **argv)
{
    struct timespec rqt;
    struct timeval delta;
    int r = 0;
    char *msg;

    if (argc == 2) {
	if (pmParseInterval(argv[1], &delta, &msg) < 0) {
	    fputs(msg, stderr);
	    free(msg);
	} else {
	    rqt.tv_sec  = delta.tv_sec;
	    rqt.tv_nsec = delta.tv_usec * 1000;
	    if (0 != nanosleep(&rqt, NULL))
		r = oserror();

	    exit(r);
	}
    }
    fprintf(stderr, "Usage: pmsleep interval\n");
    exit(1);
}
