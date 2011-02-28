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

#define __USE_GNU 1   /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/sem.h>

#include "pmapi.h"
#include "sem_limits.h"

int
refresh_sem_limits(sem_limits_t *sem_limits)
{
	static int started;
	static struct seminfo seminfo;
	static union semun arg;

	if (!started) {
		started = 1;
		memset(sem_limits, 0, sizeof(sem_limits_t));
		arg.array = (unsigned short *) &seminfo;
	}

	if (semctl(0, 0, IPC_INFO, arg) < 0) {
		return -oserror();
	}

	sem_limits->semmap = seminfo.semmap;
	sem_limits->semmni = seminfo.semmni;
	sem_limits->semmns = seminfo.semmns;
	sem_limits->semmnu = seminfo.semmnu;
	sem_limits->semmsl = seminfo.semmsl;
	sem_limits->semopm = seminfo.semopm;
	sem_limits->semume = seminfo.semume;
	sem_limits->semusz = seminfo.semusz;
	sem_limits->semvmx = seminfo.semvmx;
	sem_limits->semaem = seminfo.semaem;
	return 0;
}
