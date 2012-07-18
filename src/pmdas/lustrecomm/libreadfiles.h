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


#include <sys/types.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <stdio.h>
#include <time.h>

struct file_state {
	struct timespec ts;
	char *filename;
	int fd;
	int datas;
	void *datap;
};

#define BUFFERBLOCK 4096

#ifndef FILE_TIME_OFFSET
extern struct timespec file_time_offset;
#endif

/* timespec_routines.c */
extern struct timespec timespec_add (struct timespec *a, struct timespec *b);
extern int timespec_le ( struct timespec *lhs, struct timespec *rhs);
/* refresh_file.c */
extern int refresh_file( struct file_state *f_s );
/* file_indexed.c */
extern int file_indexed (struct file_state *f_s, int type, int *base, void **vpp, int index);
/* file_single.c */
extern int file_single (char *filename, int type, int *base, void **vpp);


