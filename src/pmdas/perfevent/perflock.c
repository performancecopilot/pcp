/*
 * Copyright (C) 2013  Joe White
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
#include "perflock.h"
#include <pcp/pmapi.h>

#define PERF_LOCK_PATH "/perfevent/perflock"

static char *perflock_filename;

/* Utility function to get the filename of the lock file
 * This function is in its own translation unit so that it can
 * be linked against the perfalloc binary and the perfevent pmda
 */
const char *get_perf_alloc_lockfile()
{
    const char *pcppmdasdir;

    if(perflock_filename) {
        return perflock_filename;
    }

    pcppmdasdir = pmGetConfig("PCP_PMDAS_DIR");
    perflock_filename = malloc( strlen(pcppmdasdir) + strlen( PERF_LOCK_PATH ) + 1);

    memcpy(perflock_filename, pcppmdasdir, strlen(pcppmdasdir));
    memcpy(perflock_filename + strlen(pcppmdasdir), PERF_LOCK_PATH, strlen( PERF_LOCK_PATH ));
    perflock_filename[ strlen(pcppmdasdir) + strlen( PERF_LOCK_PATH ) ] = '\0';

    return perflock_filename;
}

void free_perf_alloc_lockfile()
{
    if(perflock_filename) {
        free(perflock_filename);
        perflock_filename = NULL;
    }
}
