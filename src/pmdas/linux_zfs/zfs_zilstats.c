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
#include "zfs_zilstats.h"

void
zfs_zilstats_refresh(zfs_zilstats_t *zilstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "zil") != 0)
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

            mname += 4;
            if (strncmp(mname, "itx_" , 4) == 0) {
                mname += 4;
                if (strncmp(mname, "metaslab_" , 9) == 0) {
                    mname += 9;
                    if (strcmp(mname, "normal_count") == 0) zilstats->itx_metaslab_normal_count = value;
                    else if (strcmp(mname, "normal_bytes") == 0) zilstats->itx_metaslab_normal_bytes = value;
                    else if (strcmp(mname, "slog_count") == 0) zilstats->itx_metaslab_slog_count = value;
                    else if (strcmp(mname, "slog_bytes") == 0) zilstats->itx_metaslab_slog_bytes = value;
                }
                else if (strcmp(mname, "count") == 0) zilstats->itx_count = value;
                else if (strcmp(mname, "indirect_count") == 0) zilstats->itx_indirect_count = value;
                else if (strcmp(mname, "indirect_bytes") == 0) zilstats->itx_indirect_bytes = value;
                else if (strcmp(mname, "copied_count") == 0) zilstats->itx_copied_count = value;
                else if (strcmp(mname, "copied_bytes") == 0) zilstats->itx_copied_bytes = value;
                else if (strcmp(mname, "needcopy_count") == 0) zilstats->itx_needcopy_count = value;
                else if (strcmp(mname, "needcopy_bytes") == 0) zilstats->itx_needcopy_bytes = value;
            }
            if (strncmp(mname, "commit_" , 7) == 0) {
                mname += 7;
                if (strcmp(mname, "count") == 0) zilstats->commit_count = value;
                else if (strcmp(mname, "writer_count") == 0) zilstats->commit_writer_count = value;
            }
        }
        free(line);
        fclose(fp);
    }
}
