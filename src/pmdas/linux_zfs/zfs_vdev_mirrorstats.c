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
#include "zfs_vdev_mirrorstats.h"

void
zfs_vdev_mirrorstats_refresh(zfs_vdev_mirrorstats_t *vdev_mirrorstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "vdev_mirror_stats") != 0)
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

            if (strcmp(mname, "rotating_linear") == 0) vdev_mirrorstats->rotating_linear = value;
            else if (strcmp(mname, "rotating_offset") == 0) vdev_mirrorstats->rotating_offset = value;
            else if (strcmp(mname, "rotating_seek") == 0) vdev_mirrorstats->rotating_seek = value;
            else if (strcmp(mname, "non_rotating_linear") == 0) vdev_mirrorstats->non_rotating_linear = value;
            else if (strcmp(mname, "non_rotating_seek") == 0) vdev_mirrorstats->non_rotating_seek = value;
            else if (strcmp(mname, "preferred_found") == 0) vdev_mirrorstats->preferred_found = value;
            else if (strcmp(mname, "preferred_not_found") == 0) vdev_mirrorstats->preferred_not_found = value;
        }
        free(line);
        fclose(fp);
    }
}
