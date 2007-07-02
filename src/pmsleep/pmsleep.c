/*
 * Copyright (c) 2007 Silicon Graphics, Inc.  All Rights Reserved.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
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
		r = errno;

	    exit(r);
	}
    }
    fprintf(stderr, "Usage: pmsleep [-v] interval\n");
    exit(1);
    /*NOTREACHED*/
}
