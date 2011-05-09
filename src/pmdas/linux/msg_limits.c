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

#define __USE_GNU 1  /* required for IPC_INFO define */
#include <sys/ipc.h>
#include <sys/msg.h>

#include "pmapi.h"
#include "msg_limits.h"

int
refresh_msg_limits(msg_limits_t *msg_limits)
{
	static struct msginfo msginfo;
	static int started;

	if (!started) {
		started = 1;
		memset(msg_limits, 0, sizeof(msg_limits_t));
	}

	if (msgctl(0, IPC_INFO, (struct msqid_ds *) &msginfo) < 0) {
		return -oserror();
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
