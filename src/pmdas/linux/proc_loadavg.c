/*
 * Linux /proc/loadavg metrics cluster
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
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "proc_loadavg.h"

static int started = 0;

int
refresh_proc_loadavg(proc_loadavg_t *proc_loadavg)
{
    char buf[1024];
    char fmt[64];
    int fd;
    int n;

    if (!started) {
	started = 1;
	memset(proc_loadavg, 0, sizeof(proc_loadavg_t));
    }

    if ((fd = open("/proc/loadavg", O_RDONLY)) < 0)
	return -errno;

    n = read(fd, buf, sizeof(buf));
    close(fd);

    buf[sizeof(buf)-1] = '\0';

    /*
     * 0.00 0.00 0.05 1/67 17563
     * Lastpid added by Mike Mason <mmlnx@us.ibm.com>
     */
    strcpy(fmt, "%f %f %f %u/%u %u");
    sscanf((const char *)buf, fmt,
	   &proc_loadavg->loadavg[0], &proc_loadavg->loadavg[1], 
	   &proc_loadavg->loadavg[2], &proc_loadavg->runnable,
	   &proc_loadavg->nprocs, &proc_loadavg->lastpid);

    /* success */
    return 0;
}
