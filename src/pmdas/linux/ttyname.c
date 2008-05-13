/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include "pmapi.h"

char *
get_ttyname_info(int pid, dev_t dev, char *ttyname)
{
    DIR *dir;
    struct dirent *dp;
    struct stat sbuf;
    int found=0;
    char procpath[MAXPATHLEN];
    char ttypath[MAXPATHLEN];

    sprintf(procpath, "/proc/%d/fd", pid);
    if ((dir = opendir(procpath)) != NULL) {
	while ((dp = readdir(dir)) != NULL) {
	    if (!isdigit(dp->d_name[0]))
	    	continue;
	    sprintf(procpath, "/proc/%d/fd/%s", pid, dp->d_name);
	    if (realpath(procpath, ttypath) == NULL || stat(ttypath, &sbuf) < 0)
	    	continue;
	    if (S_ISCHR(sbuf.st_mode) && dev == sbuf.st_rdev) {
		found=1;
		break;
	    }
	}
	closedir(dir);
    }

    if (!found)
    	strcpy(ttyname, "?");
    else
	/* skip the "/dev/" prefix */
    	strcpy(ttyname, &ttypath[5]);

    return ttyname;
}
