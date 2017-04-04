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


/* int file_indexed (struct file_state *f_s,
 *                   int type, int base, void *vpp, int index) 
 *  - Get single value from file containing multiple values 
 * struct file_state *f_s - struct for access to single file 
 * type - PCP type described by the PMAPI PM_TYPE_* enum 
 * base  - for integer types, base as stored in the file 
 *       - for string and aggregate types, size of memory pointed to 
 *       ---------------- size may be changed by realloc 
 * vpp   - pointer to pointer to storage allocated to contain value 
 *       - pointed to pointer may change
 *       ---------------- pointer may be changed by realloc 
 * index - value is the n'th strtok token in the file 
 * consider specifying token seperator, default whitespace is fine for now
 */
int file_indexed (struct file_state *f_s, int type, int *base, void **vpp, int index) 
{
    int i;
    char *cp, *sp;
	
    /* reload file if it hasn't been reloaded in x */
    if (refresh_file( f_s ) <0 ) {
	__pmNotifyErr(LOG_ERR, "file_indexed: refresh_file error");
	return -1;
    }
    /* file_indexed assumes that the file contains one line 
     * if the file may ever contain more than one line, consider file_arrayed 
     * (if that ever gets written) 
     * strtok to the appropriate spot 
     * strtok screws up the target string, so make a copy of the file data
     */
    cp = malloc ( f_s->datas );
    strcpy ( cp, f_s->datap );
    sp = strtok( cp, " " );
    for (i = 0; i < index; i++){
	sp = strtok( NULL, " ");
    }
    /* otherwise, a lot like file_single (see below)
     * one would eventually write a configure script to make
     * sure that the right routines are used below.. defining
     * a STRTO32 and STRTOU32, etc.
     */
    switch (type) {
    case PM_TYPE_32:
	*((long *)(*vpp)) = strtol(sp,0,*base);
	break;
    case PM_TYPE_U32:
	*((unsigned long *)(*vpp)) = strtoul(sp,0,*base);
	break;
    case PM_TYPE_64:
	*((long long *)(*vpp)) = strtoll(sp,0,*base);
	break;
    case PM_TYPE_U64:
	*((unsigned long long *)(*vpp)) = strtoull(sp,0,*base);
	break;
    case PM_TYPE_FLOAT:
	*((float *)(*vpp)) = strtof(sp,0);
	break;
    case PM_TYPE_DOUBLE:
	*((double *)(*vpp)) = strtod(sp,0);
	break;
    default:
	fprintf(stderr,"file_indexed: type %s not supported\n", pmTypeStr(type));
	break;
    }
    /* so, write it up and slice and dice */
    free (cp);

    return 0;
}


