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

    if (zfs_stats_file_check(fname, "zil") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strncmp(mname, "zil_itx_" , 8) == 0) {
                if (strncmp(mname, "zil_itx_metaslab_" , 17) == 0) {
                    if (strcmp(mname, "zil_itx_metaslab_normal_count") == 0) zilstats->itx_metaslab_normal_count = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "zil_itx_metaslab_normal_bytes") == 0) zilstats->itx_metaslab_normal_bytes = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "zil_itx_metaslab_slog_count") == 0) zilstats->itx_metaslab_slog_count = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "zil_itx_metaslab_slog_bytes") == 0) zilstats->itx_metaslab_slog_bytes = strtoul(mval, NULL, 0);
                }
                else if (strcmp(mname, "zil_itx_count") == 0) zilstats->itx_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_indirect_count") == 0) zilstats->itx_indirect_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_indirect_bytes") == 0) zilstats->itx_indirect_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_copied_count") == 0) zilstats->itx_copied_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_copied_bytes") == 0) zilstats->itx_copied_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_needcopy_count") == 0) zilstats->itx_needcopy_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "zil_itx_needcopy_bytes") == 0) zilstats->itx_needcopy_bytes = strtoul(mval, NULL, 0);
            }
            else if (strcmp(mname, "zil_commit_count") == 0) zilstats->commit_count = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "zil_commit_writer_count") == 0) zilstats->commit_writer_count = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
