/*
 * GFS2 glstats file statistics.
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

#include "glstats.h"

/*
 * GFS2 glock type identification; note that the glock type 7 is currently not 
 * used and is reserved to be on the safe side.
 *
 * Type Lock type  Use
 * 1    Trans      Transaction lock
 * 2    Inode      Inode metadata and data
 * 3    Rgrp       Resource group metadata
 * 4    Meta       The superblock
 * 5    Iopen      Inode last closer detection
 * 6    Flock      flock(2) syscall
 * 8    Quota      Quota operations
 * 9    Journal    Journal mutex 
 *
 */

int
gfs2_glstats_fetch(int item, struct glstats *glstats, pmAtomValue *atom)
{
    /* Handle the case for our reserved but not used glock type 7 */
    if ((item < 0 || item >= NUM_GLSTATS_STATS) && item != 7)
	return PM_ERR_PMID;

    /* Check for no values recorded */
    if(glstats->values[item] == UINT64_MAX)
        return 0;

    atom->ull = glstats->values[item];
    return 1;
}

int
gfs2_refresh_glstats(const char *sysfs, const char *name, struct glstats *glstats){
    char buffer[4096];
    FILE *fp;

    /* Reset all counter for this fs */
    memset(glstats, 0, sizeof(*glstats));

    pmsprintf(buffer, sizeof(buffer), "%s/%s/glstats", sysfs, name);
    buffer[sizeof(buffer) - 1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL){
        /*
         * We set the values to UINT64_MAX to signify we have no
         * current values (no metric support or debugfs not mounted)
         *
         */
        memset(glstats, -1, sizeof(*glstats));
	return -oserror();
    }

    /*
     * We read through the glstats file, finding out what glock types we are
     * coming across and tally up the number of each type of glock we find.
     * This file however contains the total number of locks at this time,
     * on a large, heavy utilized filesystem there could be millions of entries
     * so needs to be quick and efficient.
     *
     */
    while(fgets(buffer, sizeof(buffer), fp) != NULL){
        char *p = buffer;
        
        /* We pick out the various glock types by the identifying number */
        if (strncmp(p, "G: n:1", 6) == 0){
            glstats->values[GLSTATS_TRANS]++;
        } else if (strncmp(p, "G: n:2 ", 6) == 0){
            glstats->values[GLSTATS_INODE]++;
        } else if (strncmp(p, "G: n:3 ", 6) == 0){
            glstats->values[GLSTATS_RGRP]++;
        } else if (strncmp(p, "G: n:4 ", 6) == 0){
            glstats->values[GLSTATS_META]++;
        } else if (strncmp(p, "G: n:5 ", 6) == 0){
            glstats->values[GLSTATS_IOPEN]++;
        } else if (strncmp(p, "G: n:6 ", 6) == 0){
            glstats->values[GLSTATS_FLOCK]++;
        } else if (strncmp(p, "G: n:8 ", 6) == 0){
            glstats->values[GLSTATS_QUOTA]++;
        } else if (strncmp(p, "G: n:9 ", 6) == 0){
            glstats->values[GLSTATS_JOURNAL]++;
        }
        glstats->values[GLSTATS_TOTAL]++;

        /*
         * We advance the cursor for after we read what type of lock we have
         * for (p += 6; isspace((int)*p); p++) {;}
         *
         * [ We can extract any other future fields from here on] 
         *
         */
    }

    fclose(fp);
    return 0;
}
