/*
 * Copyright (c) International Business Machines Corp., 2002
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

/*
 * This code contributed by Mike Mason <mmlnx@us.ibm.com>
 * $Id: proc_uptime.c,v 1.3 2004/06/24 06:15:36 kenmcd Exp $
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "proc_uptime.h"

static int started = 0;

int
refresh_proc_uptime(proc_uptime_t *proc_uptime)
{
	char buf[80];
	int fd, n;
	float uptime, idletime;

	if (!started) {
		started = 1;
		memset(proc_uptime, 0, sizeof(proc_uptime_t));
	}

	if ((fd = open("/proc/uptime", O_RDONLY)) < 0) {
		return -errno;
	}

	if ((n = read(fd, buf, sizeof(buf))) < 0) {
		return -errno;
	}
	close(fd);

	buf[n] = '\0';

	/*
	 * 0.00 0.00
	 */
	sscanf((const char *)buf, "%f %f", &uptime, &idletime);
	proc_uptime->uptime = (unsigned long) uptime;
	proc_uptime->idletime = (unsigned long) idletime;

	/* success */
	return 0;
}

