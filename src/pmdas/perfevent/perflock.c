#include "perflock.h"
#include <string.h>

#include <stdlib.h>
#include <string.h>

#define PERF_LOCK_PATH "/perfevent/perflock"

static char *perflock_filename = NULL;

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

    pcppmdasdir = getenv("PCP_PMDAS_DIR");

    if( !pcppmdasdir ) {
        pcppmdasdir = "/var/lib/pcp/pmdas";
    }

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
