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
 */

#include <errno.h>
#include <string.h>
#define __USE_GNU 1    /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/shm.h>

#include "shm_limits.h"

static int started = 0;

int
refresh_shm_limits(shm_limits_t *shm_limits)
{

	static struct shminfo shminfo;

	if (!started) {
		started = 1;
		memset(shm_limits, 0, sizeof(shm_limits_t));
	}

	if (shmctl(0, IPC_INFO, (struct shmid_ds *) &shminfo) < 0) {
		return -errno;
	}

	shm_limits->shmmax = shminfo.shmmax;
	shm_limits->shmmin = shminfo.shmmin;
	shm_limits->shmmni = shminfo.shmmni;
	shm_limits->shmseg = shminfo.shmseg;
	shm_limits->shmall = shminfo.shmall;

	/* success */
	return 0;
}

