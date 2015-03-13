/*
 * Copyright (c) 2015 Red Hat.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"

static int
pmpause(void)
{
    sigset_t sigset;
    sigfillset(&sigset);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    pause();
    return 0;
}

static int
pmsleep(const char *interval)
{
    struct timespec rqt;
    struct timeval delta;
    char *msg;
    
    if (pmParseInterval(interval, &delta, &msg) < 0) {
	fputs(msg, stderr);
	free(msg);
    } else {
	rqt.tv_sec  = delta.tv_sec;
	rqt.tv_nsec = delta.tv_usec * 1000;
	if (0 != nanosleep(&rqt, NULL))
	    return oserror();
    }
    return 0;
}

int
main(int argc, char **argv)
{
    int sts = 1;

    __pmSetProgname(argv[0]);
    if (strcmp(pmProgname, "pmpause") == 0)
	sts = pmpause();
    else if (argc == 2)
	sts = pmsleep(argv[1]);
    else
	fprintf(stderr, "Usage: %s interval\n", pmProgname);
    exit(sts);
}
