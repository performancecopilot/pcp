/*
 * Linux Filesystem Cluster
 *
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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "filesys.h"

int
refresh_filesys(filesys_t *filesys) {
    char buf[MAXPATHLEN];
    char realdevice[MAXPATHLEN];
    FILE *fp;
    char *path;
    char *device;
    char *type;
    int i;
    int n;
    int old_device;
    char *p;
    pmdaIndom *indomp = filesys->indom;
    static int next_id = -1;

    if (next_id < 0) {
	next_id = 0;
	filesys->nmounts = 0;
    	filesys->mounts = (filesys_entry_t *)malloc(sizeof(filesys_entry_t));
	indomp->it_numinst = 0;
	indomp->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid));
    }

    if ((fp = fopen("/proc/mounts", "r")) == (FILE *)NULL)
    	return -errno;

    for (i=0; i < filesys->nmounts; i++) {
    	filesys->mounts[i].seen = 0;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((device = strtok(buf, " ")) == 0)
	    continue;
	if (strncmp(device, "/dev", 4) != 0)
	    continue;
	if ((p = realpath(device, realdevice)) != NULL)
	    device = p;
	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	if (strcmp(type, "proc") == 0 ||
	    strcmp(type, "nfs") == 0 ||
	    strcmp(type, "devfs") == 0 ||
	    strcmp(type, "devpts") == 0 ||
	    strncmp(type, "auto", 4) == 0)
	    continue;
	old_device = -1;
	for (i=0; i < filesys->nmounts; i++) {
	    if (filesys->mounts[i].device != NULL && strcmp(filesys->mounts[i].device, device) == 0) {
		if (filesys->mounts[i].valid)
		    break;
		else
		    old_device = i;
	    }
	}
	if (i == filesys->nmounts) {
	    /* new mount */
	    if (old_device >= 0) {
		/* same device as last time mounted: reuse the id and device name */ 
	    	i = old_device;
	    }
	    else {
		filesys->nmounts++;
	    	filesys->mounts = (filesys_entry_t *)realloc(filesys->mounts,
		    filesys->nmounts * sizeof(filesys_entry_t));
		filesys->mounts[i].device = strdup(device);
		filesys->mounts[i].id = next_id++;
	    }
	    filesys->mounts[i].path = strdup(path);
	    filesys->mounts[i].valid = 1;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    filesys->mounts[i].path, filesys->mounts[i].device);
	    }
#endif
	}

	filesys->mounts[i].seen = 1;
    }

    /* check for filesystems that have been unmounted */
    for (n=0, i=0; i < filesys->nmounts; i++) {
	if (filesys->mounts[i].valid) {
	    if (filesys->mounts[i].seen == 0) {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "refresh_filesys: drop \"%s\" \"%s\"\n",
			filesys->mounts[i].path, filesys->mounts[i].device);
		}
#endif
		free(filesys->mounts[i].path);
		filesys->mounts[i].path = NULL;
		filesys->mounts[i].valid = 0;
	    }
	    else
		n++;
    	}
    }

    /* refresh indom */
    if (indomp->it_numinst != n) {
        indomp->it_numinst = n;
        indomp->it_set = (pmdaInstid *)realloc(indomp->it_set, n * sizeof(pmdaInstid));
        memset(indomp->it_set, 0, n * sizeof(pmdaInstid));
    }
    for (n=0, i=0; i < filesys->nmounts; i++) {
        if (filesys->mounts[i].valid) {
            if (filesys->mounts[i].id != indomp->it_set[n].i_inst || indomp->it_set[n].i_name == NULL) {
                indomp->it_set[n].i_inst = filesys->mounts[i].id;
                indomp->it_set[n].i_name = filesys->mounts[i].device;
            }
	    filesys->mounts[i].fetched = 0; /* avoid multiple calls to statfs */
            n++;
        }
    }


    /*
     * success
     * Note: we do not call statfs() here since only some instances
     * may be requested (rather, we do it in linux_fetch, see pmda.c).
     */
    fclose(fp);
    return 0;
}
