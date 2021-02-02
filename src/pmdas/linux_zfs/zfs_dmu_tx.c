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
#include "zfs_dmu_tx.h"

void
zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "dmu_tx") != 0)
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

            mname += 7;
            if (strncmp(mname, "dirty_", 6) == 0) {
                mname += 6;
                if (strcmp(mname, "throttle") == 0) dmu_tx->dirty_throttle = value;
                else if (strcmp(mname, "delay") == 0) dmu_tx->dirty_delay = value;
                else if (strcmp(mname, "over_max") == 0) dmu_tx->dirty_over_max = value;
                else if (strcmp(mname, "frees_delay") == 0) dmu_tx->dirty_frees_delay = value;
            }
            else if (strcmp(mname, "assigned") == 0) dmu_tx->assigned = value;
            else if (strcmp(mname, "delay") == 0) dmu_tx->delay = value;
            else if (strcmp(mname, "error") == 0) dmu_tx->error = value;
            else if (strcmp(mname, "suspended") == 0) dmu_tx->suspended = value;
            else if (strcmp(mname, "group") == 0) dmu_tx->group = value;
            else if (strcmp(mname, "memory_reserve") == 0) dmu_tx->memory_reserve = value;
            else if (strcmp(mname, "memory_reclaim") == 0) dmu_tx->memory_reclaim = value;
            else if (strcmp(mname, "quota") == 0) dmu_tx->quota = value;
        }
        free(line);
        fclose(fp);
    }
}
