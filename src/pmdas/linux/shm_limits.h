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
	unsigned int shmmax; /* maximum shared segment size (bytes) */
	unsigned int shmmin; /* minimum shared segment size (bytes) */
	unsigned int shmmni; /* maximum number of segments system wide */
	unsigned int shmseg; /* maximum shared segments per process */
	unsigned int shmall; /* maximum shared memory system wide (pages) */
} shm_limits_t;

extern int refresh_shm_limits(shm_limits_t *);

