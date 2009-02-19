/*
 * Copyright (C) 2009 Aconex.  All Rights Reserved.
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

enum {
    PCP_SIGHUP  = 1,
    PCP_SIGUSR  = 2,
    PCP_SIGTERM = 3,
    PCP_SIGKILL = 4,
};

int
atosig(const char *sig)
{
    if (strcmp(sig, "HUP") == 0)
	return PCP_SIGHUP;
    if (strcmp(sig, "USR") == 0)
	return PCP_SIGUSR;
    if (strcmp(sig, "TERM") == 0)
	return PCP_SIGTERM;
    if (strcmp(sig, "KILL") == 0)
	return PCP_SIGKILL;
    return 0;
}

int
main(int argc, char **argv)
{
    pid_t	pid;
    char	name[64];
    int		sig, error = 0;

    __pmSetProgname(argv[0]);

    if (argc != 3)
	error++;
    else if ((sig = atosig(argv[1])) < 1)
	error++;
    else if ((pid = atoi(argv[2])) < 1)
	error++;

    if (error) {
	fprintf(stderr, "Usage: %s <HUP|USR|TERM|KILL> <PID>\n", pmProgname);
	return 2;
    }

    if (sig == PCP_SIGKILL) {
	__pmProcessTerminate(pid, 1);
	return 0;
    }

    if (!__pmProcessExists(pid)) {
	fprintf(stderr, "%s: OpenEvent(%s) failed on PID %d (%ld)\n",
			pmProgname, name, pid, GetLastError());
	return 1;
    }

    snprintf(name, sizeof(name), "PCP/%d/SIG%s", pid, argv[1]);
    HANDLE h = OpenEvent(EVENT_MODIFY_STATE, FALSE, TEXT(name));
    if (!h) {
	fprintf(stderr, "%s: OpenEvent(%s) failed on PID %d (%ld)\n",
			pmProgname, name, pid, GetLastError());
	return 1;
    }
    if (!SetEvent(h)) {
	fprintf(stderr, "%s: SetEvent(%s) failed on PID %d (%ld)\n",
			pmProgname, name, pid, GetLastError());
	return 1;
    }

    return 0;
}
