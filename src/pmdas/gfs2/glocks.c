/*
 * GFS2 glocks sysfs file statistics.
 *
 * Copyright (c) 2013 Red Hat.
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
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "glocks.h"

#include <ctype.h>

int
gfs2_glocks_fetch(int item, struct glocks *glocks, pmAtomValue *atom)
{
    /* Check for valid metric count */
    if (item < 0 || item >= NUM_GLOCKS_STATS)
	return PM_ERR_PMID;

    /* Check for no values recorded */
    if(glocks->values[item] == UINT64_MAX)
        return 0;

    atom->ull = glocks->values[item];
    return 1;
}

int
gfs2_refresh_glocks(const char *sysfs, const char *name, struct glocks *glocks)
{
    char buffer[4096];
    FILE *fp;

    /* Reset all counter for this fs */
    memset(glocks, 0, sizeof(*glocks));

    pmsprintf(buffer, sizeof(buffer), "%s/%s/glocks", sysfs, name);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL) {
        /*
         * We set the values to UINT64_MAX to signify we have no
         * current values (no metric support or debugfs not mounted)
         *
         */
        memset(glocks, -1, sizeof(*glocks));
	return -oserror();
    }

    /*
     * Read through glocks file accumulating statistics as we go;
     * as an early starting point, we're simply binning aggregate
     * glock state counts.
     *
     */
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	char *p = buffer;

	/* interested in glock lines only for now */
	if (strncmp(p, "G:", 2) != 0)
	    continue;
	for (p += 2; isspace((int)*p); p++) {;}

	/* pick out the various state fields next */
	if (strncmp(p, "s:SH", 4) == 0)
	    glocks->values[GLOCKS_SHARED]++;
	else if (strncmp(p, "s:UN ", 4) == 0)
	    glocks->values[GLOCKS_UNLOCKED]++;
	else if (strncmp(p, "s:DF ", 4) == 0)
	    glocks->values[GLOCKS_DEFERRED]++;
	else if (strncmp(p, "s:EX", 4) == 0)
	    glocks->values[GLOCKS_EXCLUSIVE]++;
	glocks->values[GLOCKS_TOTAL]++;
	for (p += 4; isspace((int)*p); p++) {;}

	/* [ extract any other field stats here ] */
    }

    fclose(fp);
    return 0;
}
