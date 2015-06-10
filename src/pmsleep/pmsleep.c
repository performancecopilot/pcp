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
#include <signal.h>
#include <sys/wait.h>

static volatile sig_atomic_t finished;

static void
sigterm(int sig)
{
    (void)sig;
    finished = 1;
}

static void
sigchld(int sig)
{
    int sts;

    (void)sig;
    do {
	sts = waitpid(-1, NULL, WNOHANG);
	if (sts < 0 && errno == ECHILD)
	    finished = 1;
    } while (sts > 0);
}

static int
pmpause(void)
{
    sigset_t sigset;
    struct sigaction sigact;
    int i, finish[] = { SIGINT, SIGQUIT, SIGTERM };

    sigfillset(&sigset);
    sigdelset(&sigset, SIGCHLD);

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = &sigchld;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sigact, 0) == -1) {
	perror(pmProgname);
	return 1;
    }

    for (i = 0; i < sizeof(finish) / sizeof(int); i++) {
	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = &sigterm;
	sigemptyset(&sigact.sa_mask);
	sigact.sa_flags = SA_RESTART;
	if (sigaction(finish[i], &sigact, 0) == -1) {
	    perror(pmProgname);
	    return 1;
	}
	sigdelset(&sigset, finish[i]);
    }

    sigprocmask(SIG_BLOCK, &sigset, NULL);
    while (!finished)
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
