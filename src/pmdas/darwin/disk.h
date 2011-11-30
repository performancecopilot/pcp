/*
 * Disk statistics types
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#define DEVNAMEMAX	255	/* largest device name we allow */

/*
 * Per-device statistics
 */
typedef struct diskstat {
    __uint64_t	read;
    __uint64_t	write;
    __uint64_t	read_bytes;
    __uint64_t	write_bytes;
    __uint64_t	read_time;
    __uint64_t	write_time;
    __uint64_t	blocksize;
    char	name[DEVNAMEMAX + 1];
} diskstat_t;

/*
 * Global statistics.
 * 
 * We avoid continually realloc'ing memory by keeping track
 * of the maximum number of devices we've allocated space for
 * so far, and only realloc new space if we go beyond that.
 */
typedef struct diskstats {
    __uint64_t	read;
    __uint64_t	write;
    __uint64_t	read_bytes;
    __uint64_t	write_bytes;
    __uint64_t	blkread;
    __uint64_t	blkwrite;
    __uint64_t	read_time;
    __uint64_t	write_time;
    int		highwater;	/* largest number of devices seen so far */
    diskstat_t	*disks;		/* space for highwater number of devices */
} diskstats_t;

