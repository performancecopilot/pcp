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
		char state[3], flags[15];

		if (strncmp(buffer, "G:", 2) == 0) {

			sscanf(buffer, "G: s:%s n:%*u/%*x f:%s t:%*s",
				state, flags
			);

			// Capture glock state info
			if (strncmp(state, "SH", 2) == 0)
				glocks->values[GLOCKS_SHARED]++;
                        else if (strncmp(state, "UN", 2) == 0)
				glocks->values[GLOCKS_UNLOCKED]++;
                        else if (strncmp(state, "DF", 2) == 0)
				glocks->values[GLOCKS_DEFERRED]++;
                        else if (strncmp(state, "EX", 2) == 0)
				glocks->values[GLOCKS_EXCLUSIVE]++;
			glocks->values[GLOCKS_TOTAL]++;

			// Record flags
			if (strpbrk(flags, "l"))
				glocks->values[GLOCKS_FLAGS_LOCKED]++;
			if (strpbrk(flags, "D"))
				glocks->values[GLOCKS_FLAGS_DEMOTE]++;
			if (strpbrk(flags, "d"))
				glocks->values[GLOCKS_FLAGS_DEMOTE_PENDING]++;
			if (strpbrk(flags, "p"))
				glocks->values[GLOCKS_FLAGS_DEMOTE_PROGRESS]++;
			if (strpbrk(flags, "y"))
				glocks->values[GLOCKS_FLAGS_DIRTY]++;
			if (strpbrk(flags, "f"))
				glocks->values[GLOCKS_FLAGS_LOG_FLUSH]++;
			if (strpbrk(flags, "i"))
				glocks->values[GLOCKS_FLAGS_INVALIDATE]++;
			if (strpbrk(flags, "r"))
				glocks->values[GLOCKS_FLAGS_REPLY_PENDING]++;
			if (strpbrk(flags, "I"))
				glocks->values[GLOCKS_FLAGS_INITIAL]++;
			if (strpbrk(flags, "f"))
				glocks->values[GLOCKS_FLAGS_FROZEN]++;
			if (strpbrk(flags, "q"))
				glocks->values[GLOCKS_FLAGS_QUEUED]++;
			if (strpbrk(flags, "o"))
				glocks->values[GLOCKS_FLAGS_OBJECT_ATTACHED]++;
			if (strpbrk(flags, "b"))
				glocks->values[GLOCKS_FLAGS_BLOCKING_REQUEST]++;
			if (strpbrk(flags, "L"))
				glocks->values[GLOCKS_FLAGS_LRU]++;
		}

		if (strncmp(buffer, " H:", 3) == 0) {

			sscanf(buffer, " H: s:%s f:%s e:%*s",
				state, flags
			);

			// Capture holder state info
			if (strncmp(state, "SH", 2) == 0)
				glocks->values[HOLDERS_SHARED]++;
                        else if (strncmp(state, "UN", 2) == 0)
				glocks->values[HOLDERS_UNLOCKED]++;
                        else if (strncmp(state, "DF", 2) == 0)
				glocks->values[HOLDERS_DEFERRED]++;
                        else if (strncmp(state, "EX", 2) == 0)
				glocks->values[HOLDERS_EXCLUSIVE]++;
			glocks->values[HOLDERS_TOTAL]++;

			// Record flags
			if (strpbrk(flags, "a"))
				glocks->values[HOLDERS_FLAGS_ASYNC]++;
			if (strpbrk(flags, "A"))
				glocks->values[HOLDERS_FLAGS_ANY]++;
			if (strpbrk(flags, "c"))
				glocks->values[HOLDERS_FLAGS_NO_CACHE]++;
			if (strpbrk(flags, "e"))
				glocks->values[HOLDERS_FLAGS_NO_EXPIRE]++;
			if (strpbrk(flags, "E"))
				glocks->values[HOLDERS_FLAGS_EXACT]++;
			if (strpbrk(flags, "F"))
				glocks->values[HOLDERS_FLAGS_FIRST]++;
			if (strpbrk(flags, "H"))
				glocks->values[HOLDERS_FLAGS_HOLDER]++;
			if (strpbrk(flags, "p"))
				glocks->values[HOLDERS_FLAGS_PRIORITY]++;
			if (strpbrk(flags, "t"))
				glocks->values[HOLDERS_FLAGS_TRY]++;
			if (strpbrk(flags, "T"))
				glocks->values[HOLDERS_FLAGS_TRY_1CB]++;
			if (strpbrk(flags, "W"))
				glocks->values[HOLDERS_FLAGS_WAIT]++;
		}
    }

    fclose(fp);
    return 0;
}
