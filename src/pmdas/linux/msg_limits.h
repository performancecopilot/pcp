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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * This code contributed by Mike Mason (mmlnx@us.ibm.com)
 */

typedef struct {
	unsigned int msgpool; /* size of message pool (kbytes) */
	unsigned int msgmap;  /* # of entries in message map */
	unsigned int msgmax;  /* maximum size of a message */
	unsigned int msgmnb;  /* default maximum size of message queue */
	unsigned int msgmni;  /* maximum # of message queue identifiers */
	unsigned int msgssz;  /* message segment size */
	unsigned int msgtql;  /* # of system message headers */
	unsigned int msgseg;  /* maximum # of message segments */
} msg_limits_t;

extern int refresh_msg_limits(msg_limits_t*);

