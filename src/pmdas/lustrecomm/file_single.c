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
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
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
    char b[80];
    /* this is bad - FIX */
    struct stat sb = {
    	.st_size = 0
    };

    memset (b, 0, 80);

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
	    return -1;
	}
	close(fd);
	break;
    case PM_TYPE_STRING:
    case PM_TYPE_AGGREGATE:
	/* Here we take whatever's in the file and hand it out */
	if (*base < sb.st_size) {
	    /* Check! vp is no longer valid */
	    if ((*vpp = realloc(*vpp, sb.st_size)) == NULL){
		free(*vpp);
		perror("file_single: realloc");
		return -1;
	    }
	    /* this doesn't get used... for now */
	    /* FIXME to prevent future accidents */
	}
	/* Absolutely not necessary!:
	 * I'm just going to overwrite it with the read
	 * but it makes disasters easy to identify
	 */
	memset ( *vpp, 0, *base);
	if (( n= read( fd, *vpp, *base)) < 0){
	    perror("file_single: read");
	    return -1;
	}
	break;
    case PM_TYPE_NOSUPPORT:
    default:
	break;
	
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
    case PM_TYPE_STRING:
    case PM_TYPE_AGGREGATE:
	/* the work is already done */
	break;
    case PM_TYPE_NOSUPPORT:
    default:
	fprintf(stderr,"file_single: type %d not supported\n", type);
	break;
    }
    return 0;
}


