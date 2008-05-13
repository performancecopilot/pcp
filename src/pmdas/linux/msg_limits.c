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
#define __USE_GNU 1  /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/msg.h>

#include "msg_limits.h"

static int started = 0;

int
refresh_msg_limits(msg_limits_t *msg_limits)
{
	static struct msginfo msginfo;

	if (!started) {
		started = 1;
		memset(msg_limits, 0, sizeof(msg_limits_t));
	}

	if (msgctl(0, IPC_INFO, (struct msqid_ds *) &msginfo) < 0) {
		return -errno;
	}

	msg_limits->msgpool = msginfo.msgpool;
	msg_limits->msgmap = msginfo.msgmap;
	msg_limits->msgmax = msginfo.msgmax;
	msg_limits->msgmnb = msginfo.msgmnb;
	msg_limits->msgmni = msginfo.msgmni;
	msg_limits->msgssz = msginfo.msgssz;
	msg_limits->msgtql = msginfo.msgtql;
	msg_limits->msgseg = msginfo.msgseg;

	/* success */
	return 0;
}

