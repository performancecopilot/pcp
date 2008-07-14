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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "filesys.h"

int
refresh_filesys(pmInDom filesys_indom)
{
    char buf[MAXPATHLEN];
    char realdevice[MAXPATHLEN];
    filesys_t *fs;
    FILE *fp;
    char *path;
    char *device;
    char *type;
    char *p;
    int sts;

    pmdaCacheOp(filesys_indom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/mounts", "r")) == (FILE *)NULL)
	return -errno;

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
	sts = pmdaCacheLookupName(filesys_indom, device, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(filesys_indom, PMDA_CACHE_ADD, device, fs);
	    if (strcmp(path, fs->path) != 0) {	/* old device, new path */
		free(fs->path);
		fs->path = strdup(path);
	    }
	}
	else {	/* new mount */
	    if ((fs = malloc(sizeof(filesys_t))) == NULL)
		continue;
	    fs->path = strdup(path);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
		    fs->path, device);
	    }
#endif
	    pmdaCacheStore(filesys_indom, PMDA_CACHE_ADD, device, fs);
	}
	fs->fetched = 0;
    }

    /*
     * success
     * Note: we do not call statfs() here since only some instances
     * may be requested (rather, we do it in linux_fetch, see pmda.c).
     */
    fclose(fp);
    return 0;
}
