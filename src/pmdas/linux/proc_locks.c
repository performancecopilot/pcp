/*
 * Linux /proc/locks metrics cluster
 *
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
 */
#include "linux.h"
#include "proc_locks.h"

int
refresh_proc_locks(proc_locks_t *proc_locks)
{
    char		type[16], access[16], buf[256];
    lock_stats_t	*lp;
    FILE		*fp;
    int			sts;

    memset(proc_locks, 0, sizeof(*proc_locks));

    if ((fp = linux_statsfile("/proc/locks", buf, sizeof(buf))) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((sts = sscanf(buf, "%*d: %15s %*s %15s %*d", type, access)) != 2)
	    continue;
	if (strncmp(type, "POSIX", sizeof("POSIX")-1) == 0 ||
	    strncmp(type, "ACCESS", sizeof("ACCESS")-1) == 0 ||
	    strncmp(type, "OFDLCK", sizeof("OFDLCK")-1) == 0)
	    lp = &proc_locks->posix;
	else if (strncmp(type, "FLOCK", sizeof("FLOCK")-1) == 0)
	    lp = &proc_locks->flock;
	else if (strncmp(type, "DELEG", sizeof("DELEG")-1) == 0 ||
	    strncmp(type, "LEASE", sizeof("LEASE")-1) == 0)
	    lp = &proc_locks->lease;
	else
	    continue;

	lp->count++;

	if (strncmp(access, "READ", sizeof("READ")-1) == 0)
	    lp->read++;
	else if (strncmp(access, "WRITE", sizeof("WRITE")-1) == 0)
	    lp->write++;
	else if (strncmp(access, "RW", sizeof("RW")-1) == 0) {
	    lp->write++;
	    lp->read++;
	}
    }

    fclose(fp);
    return 0;
}
