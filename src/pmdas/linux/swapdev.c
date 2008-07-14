/*
 * Linux Swap Device Cluster
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
#include "swapdev.h"

int
refresh_swapdev(pmInDom swapdev_indom)
{
    char buf[MAXPATHLEN];
    swapdev_t *swap;
    FILE *fp;
    char *path;
    char *type;
    char *size;
    char *used;
    char *priority;
    int sts;

    pmdaCacheOp(swapdev_indom, PMDA_CACHE_INACTIVE);

    if ((fp = fopen("/proc/swaps", "r")) == (FILE *)NULL)
	return -errno;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (buf[0] != '/')
	    continue;
	if ((path = strtok(buf, " \t")) == 0)
	    continue;
	if ((type = strtok(NULL, " \t")) == NULL ||
	    (size = strtok(NULL, " \t")) == NULL ||
	    (used = strtok(NULL, " \t")) == NULL ||
	    (priority = strtok(NULL, " \t")) == NULL)
	    continue;
	sts = pmdaCacheLookupName(swapdev_indom, path, NULL, (void **)&swap);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/swaps? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old swap device */
	    pmdaCacheStore(swapdev_indom, PMDA_CACHE_ADD, path, swap);
	}
	else {	/* new swap device */
	    if ((swap = malloc(sizeof(swapdev_t))) == NULL)
		continue;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA)
		fprintf(stderr, "refresh_swapdev: add \"%s\"\n", path);
#endif
	    pmdaCacheStore(swapdev_indom, PMDA_CACHE_ADD, path, swap);
	}
	sscanf(size, "%u", &swap->size);
	sscanf(used, "%u", &swap->used);
	sscanf(priority, "%d", &swap->priority);
    }

    /*
     * success
     */
    fclose(fp);
    return 0;
}
