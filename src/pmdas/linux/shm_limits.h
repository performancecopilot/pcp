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
 * This code contributed by Mike Mason (mmlnx@us.ibm.com)
 * $Id: shm_limits.h,v 1.3 2004/06/24 06:15:36 kenmcd Exp $
 */

typedef struct {
	unsigned int shmmax; /* maximum shared segment size (bytes) */
	unsigned int shmmin; /* minimum shared segment size (bytes) */
	unsigned int shmmni; /* maximum number of segments system wide */
	unsigned int shmseg; /* maximum shared segments per process */
	unsigned int shmall; /* maximum shared memory system wide (pages) */
} shm_limits_t;

extern int refresh_shm_limits(shm_limits_t *);

