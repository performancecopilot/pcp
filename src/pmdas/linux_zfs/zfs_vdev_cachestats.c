#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "zfs_utils.h"
#include "zfs_vdev_cachestats.h"

void
zfs_vdev_cachestats_refresh(zfs_vdev_cachestats_t *vdev_cachestats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "vdev_cache_stats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "delegations ") == 0) vdev_cachestats->delegations  = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "hits ") == 0) vdev_cachestats->hits  = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "misses ") == 0) vdev_cachestats->misses  = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}

