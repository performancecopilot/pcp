/*
 * Copyright (c) International Business Machines Corp., 2002
 * This code contributed by Mike Mason <mmlnx@us.ibm.com>
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

#define __USE_GNU 1    /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/shm.h>

#include "pmapi.h"
#include "shm_limits.h"

int
refresh_shm_limits(shm_limits_t *shm_limits)
{
	static int started;
	static struct shminfo shminfo;

	if (!started) {
		started = 1;
		memset(shm_limits, 0, sizeof(shm_limits_t));
	}

	if (shmctl(0, IPC_INFO, (struct shmid_ds *) &shminfo) < 0)
		return -oserror();

	shm_limits->shmmax = shminfo.shmmax;
	shm_limits->shmmin = shminfo.shmmin;
	shm_limits->shmmni = shminfo.shmmni;
	shm_limits->shmseg = shminfo.shmseg;
	shm_limits->shmall = shminfo.shmall;
	return 0;
}
