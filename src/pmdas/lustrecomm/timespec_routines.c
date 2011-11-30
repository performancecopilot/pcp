/*
 * Lustre common /proc PMDA
 *
 * Copyright (c) 2008 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * Author: Scott Emery <emery@sgi.com> 
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

#include <time.h>


struct timespec timespec_add (struct timespec *a, struct timespec *b){
	struct timespec tret;

	tret.tv_nsec = (a->tv_nsec + b->tv_nsec) % 1000000000;
	tret.tv_sec =  a->tv_sec + b->tv_sec + 
			((a->tv_nsec + b->tv_nsec)/1000000000);
	return tret;
}

int timespec_le ( struct timespec *lhs, struct timespec *rhs) {
	if (rhs->tv_sec < lhs->tv_sec) {
		/* false */
		return 0;
	}
	if (lhs->tv_sec == rhs->tv_sec) {
		if (rhs->tv_nsec < lhs->tv_nsec) {
			/* false */
			return 0;
		}
	}
	return 1;
}

