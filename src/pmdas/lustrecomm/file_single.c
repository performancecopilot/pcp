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

#include "libreadfiles.h"


/* file_single (char *filename, int type, int *base, void **vpp) 
 *   - Get a value from a file containing single value 
 * filename - name of file 
 * type - PCP type described by the PMAPI PM_TYPE_* enum 
 * base  - for integer types, base as stored in the file 
 *       - for string and aggregate types, size of memory pointed to 
 *       ---------------- size may be changed by realloc 
 * vpp   - pointer to pointer to storage allocated to contain value 
 *       - pointed to pointer may change 
 *       ---------------- pointer may be changed by realloc 
 * allocating and deallocating space for value is the responsibility 
 * of the caller
 */
int file_single (char *filename, int type, int *base, void **vpp) 
{
    int fd;
    int n;
    /* overlarge */
    char b[80] = { 0 };

    /* Read in the file into the buffer b */
    if (( fd = open (filename, O_RDONLY)) < 0) {
	perror("file_single: open");
	return -1;
    }
    /* Okay this presents a problem vis a vis some lustre proc files.
     * While the present scheme is adequate for lustre common in most 
     * cases, some lustre proc files have *ridiculously* large amounts 
     * of data stored in them.  Need to come up with some dynamic 
     * memory buffer system for other lustre PMDAs
     */
    switch (type) {
    case PM_TYPE_32:
    case PM_TYPE_U32:
    case PM_TYPE_64:
    case PM_TYPE_U64:
    case PM_TYPE_FLOAT:
    case PM_TYPE_DOUBLE:
	if ((n = read (fd, b, sizeof(b))) < 0 ){
	    perror("file_single: read");
	    close(fd);
	    return -1;
	}
	close(fd);
	break;
    default:
	fprintf(stderr,"file_single: type %s not supported\n", pmTypeStr(type));
	close(fd);
	return -1;
	
    }
    /* One would eventually write a configure script to make
     * sure that the right routines are used below.. defining 
     * a STRTO32 and STRTOU32, etc.
     */
    switch (type) {
    case PM_TYPE_32:
	*((long *)(*vpp)) = strtol(b,0,*base);
	break;
    case PM_TYPE_U32:
	*((unsigned long *)(*vpp)) = strtoul(b,0,*base);
	break;
    case PM_TYPE_64:
	*((long long *)(*vpp)) = strtoll(b,0,*base);
	break;
    case PM_TYPE_U64:
	*((unsigned long long *)(*vpp)) = strtoull(b,0,*base);
	break;
    case PM_TYPE_FLOAT:
	*((float *)(*vpp)) = strtof(b,0);
	break;
    case PM_TYPE_DOUBLE:
	*((double *)(*vpp)) = strtod(b,0);
	break;
    }
    return 0;
}


