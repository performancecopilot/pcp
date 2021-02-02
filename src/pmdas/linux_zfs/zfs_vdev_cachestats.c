/*
 * Copyright (c) 2021 Red Hat.
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
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "vdev_cache_stats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);

            if ((strcmp(mname, "name") == 0) || strtok(NULL, delim) != NULL)
                continue;

            value = strtoull(mval, NULL, 0);

            if (strcmp(mname, "delegations") == 0) vdev_cachestats->delegations = value;
            else if (strcmp(mname, "hits") == 0) vdev_cachestats->hits = value;
            else if (strcmp(mname, "misses") == 0) vdev_cachestats->misses = value;
        }
        free(line);
        fclose(fp);
    }
}

