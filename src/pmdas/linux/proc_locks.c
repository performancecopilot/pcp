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
    char		locktype[16], locksec[16], lockacc[16], buf[256];
    lock_stats_t	*lp;
    FILE		*fp;
    int			unused, sts;

    memset(proc_locks, 0, sizeof(*proc_locks));

    if ((fp = linux_statsfile("/proc/locks", buf, sizeof(buf))) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((sts = sscanf(buf, "%d: %15s %15s %15s %d",
			&unused, locktype, locksec, lockacc, &unused)) != 5)
	    continue;
	if (strncmp(locktype, "POSIX", sizeof("POSIX")-1) == 0 ||
	    strncmp(locktype, "ACCESS", sizeof("ACCESS")-1) == 0 ||
	    strncmp(locktype, "OFDLCK", sizeof("OFDLCK")-1) == 0)
	    lp = &proc_locks->posix;
	else if (strncmp(locktype, "FLOCK", sizeof("FLOCK")-1) == 0)
	    lp = &proc_locks->flock;
	else if (strncmp(locktype, "DELEG", sizeof("DELEG")-1) == 0 ||
	    strncmp(locktype, "LEASE", sizeof("LEASE")-1) == 0)
	    lp = &proc_locks->lease;
	else
	    continue;

	lp->count++;

	if (strncmp(lockacc, "READ", sizeof("READ")-1) == 0)
	    lp->read++;
	else if (strncmp(lockacc, "WRITE", sizeof("WRITE")-1) == 0)
	    lp->write++;
	else if (strncmp(lockacc, "RW", sizeof("RW")-1) == 0) {
	    lp->write++;
	    lp->read++;
	}
    }

    fclose(fp);
    return 0;
}
