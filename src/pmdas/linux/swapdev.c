/*
 * Linux SwapDev Cluster
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
refresh_swapdev(swapdev_t *swapdev) {
    char buf[1024];
    FILE *fp;
    char *path;
    char *type;
    char *size;
    char *used;
    char *priority;
    int unused_entry;
    int i;
    int n;
    pmdaIndom *indomp = swapdev->indom;
    static int next_id = -1;

    if (next_id < 0) {
	next_id = 0;
	swapdev->nswaps = 0;
    	swapdev->swaps = (swapdev_entry_t *)malloc(sizeof(swapdev_entry_t));
	swapdev->indom->it_numinst = 0;
	swapdev->indom->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid));
    }

    if ((fp = fopen("/proc/swaps", "r")) == (FILE *)NULL)
    	return -errno;

    for (i=0; i < swapdev->nswaps; i++) {
    	swapdev->swaps[i].seen = 0;
    }

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
	unused_entry = -1;
	for (i=0; i < swapdev->nswaps; i++) {
	    if (swapdev->swaps[i].valid == 0) {
	    	unused_entry = i;
		continue;
	    }
	    if (strcmp(swapdev->swaps[i].path, path) == 0) {
		break;
	    }
	}
	if (i == swapdev->nswaps) {
	    /* new mount */
	    if (unused_entry >= 0)
	    	i = unused_entry;
	    else {
		swapdev->nswaps++;
	    	swapdev->swaps = (swapdev_entry_t *)realloc(swapdev->swaps,
		    swapdev->nswaps * sizeof(swapdev_entry_t));
	    }
	    swapdev->swaps[i].valid = 1;
	    swapdev->swaps[i].id = next_id++;
	    swapdev->swaps[i].path = strdup(path);
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_swapdev: add \"%s\"\n", swapdev->swaps[i].path);
	    }
#endif
	}

	sscanf(size, "%u", &swapdev->swaps[i].size);
	sscanf(used, "%u", &swapdev->swaps[i].used);
	sscanf(priority, "%d", &swapdev->swaps[i].priority);

	swapdev->swaps[i].seen = 1;
    }

    /* check for swaps that have been unmounted */
    for (n=0, i=0; i < swapdev->nswaps; i++) {
	if (swapdev->swaps[i].valid) {
	    if (swapdev->swaps[i].seen == 0) {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "refresh_swapdev: drop \"%s\"\n", swapdev->swaps[i].path);
		}
#endif
		free(swapdev->swaps[i].path);
		swapdev->swaps[i].path = NULL;
		swapdev->swaps[i].valid = 0;
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
    for (n=0, i=0; i < swapdev->nswaps; i++) {
        if (swapdev->swaps[i].valid) {
            if (swapdev->swaps[i].id != indomp->it_set[n].i_inst || indomp->it_set[n].i_name == NULL) {
                indomp->it_set[n].i_inst = swapdev->swaps[i].id;
                indomp->it_set[n].i_name = swapdev->swaps[i].path;
            }
            n++;
        }
    }

    /*
     * success
     */
    fclose(fp);
    return 0;
}
