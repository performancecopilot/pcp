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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "proc_pid.h"
#include "proc_runq.h"

int
refresh_proc_runq(proc_runq_t *proc_runq)
{
    int sz;
    int fd;
    char *p;
    int sname;
    DIR *dir;
    struct dirent *d;
    char fullpath[MAXPATHLEN];
    char buf[4096];

    memset(proc_runq, 0, sizeof(proc_runq_t));
    if ((dir=opendir("/proc")) == NULL)
    	return -errno;

    while((d = readdir(dir)) != NULL) {
	if (!isdigit(d->d_name[0]))
	    continue;
	sprintf(fullpath, "/proc/%s/stat", d->d_name);
	if ((fd = open(fullpath, O_RDONLY)) < 0)
	    continue;
	sz = read(fd, buf, sizeof(buf));
	close(fd);
	buf[sizeof(buf)-1] = '\0';

	/*
	 * defunct (state name is 'Z')
	 */
	if (sz <= 0 || (p = _pm_getfield(buf, PROC_PID_STAT_STATE)) == NULL) {
	    proc_runq->unknown++;
	    continue;
	}
	if ((sname = *p) == 'Z') {
	    proc_runq->defunct++;
	    continue;
	}

	/*
	 * kernel process (not defunct and virtual size is zero)
	 */
	if ((p = _pm_getfield(buf, PROC_PID_STAT_VSIZE)) == NULL) {
	    proc_runq->unknown++;
	    continue;
	}
	if (strcmp(p, "0") == 0) {
	    proc_runq->kernel++;
	    continue;
	}

	/*
	 * swapped (resident set size is zero)
	 */
	if ((p = _pm_getfield(buf, PROC_PID_STAT_RSS)) == NULL) {
	    proc_runq->unknown++;
	    continue;
	}
	if (strcmp(p, "0") == 0) {
	    proc_runq->swapped++;
	    continue;
	}

	/*
	 * All other states
	 */
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
	case 'Z':
	    break; /* already counted above */
	default:
	    fprintf(stderr, "UNKNOWN %c : %s\n", sname, buf);
	    proc_runq->unknown++;
	    break;
	}
    }
    closedir(dir);

#if PCP_DEBUG
    if (pmDebug & DBG_TRACE_LIBPMDA) {
	fprintf(stderr, "refresh_runq: runnable=%d sleeping=%d stopped=%d blocked=%d unknown=%d\n",
	    proc_runq->runnable, proc_runq->sleeping, proc_runq->stopped,
	    proc_runq->blocked, proc_runq->unknown);
    }
#endif

    return 0;
}
