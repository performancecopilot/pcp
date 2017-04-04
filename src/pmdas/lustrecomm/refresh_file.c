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


#define FILE_TIME_OFFSET 1

#include "libreadfiles.h"

static struct timespec file_time_offset = { 0 , 900000000 };


int refresh_file( struct file_state *f_s ){
	struct timespec now, tmp;
	int i = 0;

	if (f_s->datap) {
		/* get time */
		if ( clock_gettime(CLOCK_MONOTONIC, &now) < 0 ) {
			/* if we don't know what time it is */
			/* there's nothing we can do with this */
			return 0;
		}
		/* if time since last refresh > delta */
		tmp = timespec_add( &f_s->ts, &file_time_offset);
		if ( timespec_le( &now, &tmp) ) {
			/* file is recent */
			return 0;
		}
		f_s->ts = now;
		/* clear old data, make errors obvious, autoterm trailing strings */
		memset ( f_s->datap, 0,  f_s->datas );
	}
	/* if fd is null open file */
	if ( f_s->fd <= 0) { 
		if (( f_s->fd = open (f_s->filename, O_RDONLY)) < 0) {
			perror("refresh_file: open");
			return -1;
		}
	}
	if ( lseek (f_s->fd, 0, SEEK_SET) < 0 ) {
		perror("refresh_file: initial seek");
		return -1;
	}
	/* only grow, never shrink... what would be the point? */
	while (f_s->datap && (i = read (f_s->fd, f_s->datap, f_s->datas)) >= f_s->datas ){
		/* oh heck, what do I do if this fails? */
		f_s->datas += BUFFERBLOCK;
                if ((f_s->datap = realloc(f_s->datap,  f_s->datas)) == NULL){
                       free((char *)f_s->datap);
                       perror("refresh_file: realloc");
                       return -1;
                }
		if ( lseek (f_s->fd, 0, SEEK_SET) < 0 ) {
			perror("refresh_file: subsequent seek");
			return -1;
		}
	}
	if (i < 0 ){
		/* read failed */
		perror("refresh_file: file read failed");
		return -1;
	}
	return 0;
}


