/*
 * Linux /proc/runq metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmda.h"
#include <ctype.h>
#include <sys/stat.h>
#include "proc_runq.h"
#include "proc_pid.h"
#include "indom.h"

int
proc_runq_append(const char *process, proc_runq_t *proc_runq)
{
    int fd, sname;
    ssize_t sz;
    char *p, buf[4096];
    static int unknown_count;

    snprintf(buf, sizeof(buf), "%s/proc/%s/stat", proc_statspath, process);
    if ((fd = open(buf, O_RDONLY)) < 0)
	return fd;

    sz = read(fd, buf, sizeof(buf));
    close(fd);
    buf[sizeof(buf)-1] = '\0';

    /* defunct (state name is 'Z') */
    if (sz <= 0 || (p = _pm_getfield(buf, PROC_PID_STAT_STATE)) == NULL) {
	proc_runq->unknown++;
	return 0;
    }
    if ((sname = *p) == 'Z') {
	proc_runq->defunct++;
	return 0;
    }

    /* kernel process (not defunct and virtual size is zero) */
    if ((p = _pm_getfield(buf, PROC_PID_STAT_VSIZE)) == NULL) {
	proc_runq->unknown++;
	return 0;
    }
    if (strcmp(p, "0") == 0) {
	proc_runq->kernel++;
	return 0;
    }

    /* swapped (resident set size is zero) */
    if ((p = _pm_getfield(buf, PROC_PID_STAT_RSS)) == NULL) {
	proc_runq->unknown++;
	return 0;
    }
    if (strcmp(p, "0") == 0) {
	proc_runq->swapped++;
	return 0;
    }

    /* All other states */
    switch (sname) {
	case 'R':
	    proc_runq->runnable++;
	    break;
	case 'S':
	    proc_runq->sleeping++;
	    break;
	case 'T':
	    proc_runq->stopped++;
	    break;
	case 'D':
	    proc_runq->blocked++;
	    break;
	/* case 'Z':
	    break; -- already counted above */
	default:
	    if (unknown_count++ < 3)	/* do not spam forever */
		fprintf(stderr, "UNKNOWN %c : %s\n", sname, buf);
	    proc_runq->unknown++;
	    break;
    }
    return 0;
}
