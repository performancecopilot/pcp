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
 * $Id: sem_limits.c,v 1.3 2004/06/24 06:15:36 kenmcd Exp $
 */

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#define __USE_GNU 1   /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/sem.h>

#include "sem_limits.h"

static int started = 0;

int
refresh_sem_limits(sem_limits_t *sem_limits)
{

	static struct seminfo seminfo;
	static union semun arg;

	if (!started) {
		started = 1;
		memset(sem_limits, 0, sizeof(sem_limits_t));
		arg.array = (unsigned short *) &seminfo;
	}

	if (semctl(0, 0, IPC_INFO, arg) < 0) {
		return -errno;
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

	/* success */
	return 0;
}

