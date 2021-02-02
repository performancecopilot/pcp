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
#include "zfs_dbufstats.h"

void
zfs_dbufstats_refresh(zfs_dbufstats_t *dbufstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "dbufstats") != 0)
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

            if (strncmp(mname, "hash_", 5) == 0) {
                mname += 5;
                if (strcmp(mname, "hits") == 0) dbufstats->hash_hits = value;
                else if (strcmp(mname, "misses") == 0) dbufstats->hash_misses = value;
                else if (strcmp(mname, "collisions") == 0) dbufstats->hash_collisions = value;
                else if (strcmp(mname, "elements") == 0) dbufstats->hash_elements = value;
                else if (strcmp(mname, "elements_max") == 0) dbufstats->hash_elements_max = value;
                else if (strcmp(mname, "chains") == 0) dbufstats->hash_chains = value;
                else if (strcmp(mname, "chain_max") == 0) dbufstats->hash_chain_max = value;
                else if (strcmp(mname, "insert_race") == 0) dbufstats->hash_insert_race = value;
            }
            if (strncmp(mname, "cache_", 6) == 0) {
                mname += 6;
                if (strncmp(mname, "level_", 6) == 0) {
                    mname += 6;
                    if (strcmp(mname, "11") == 0) dbufstats->cache_level_11 = value;
                    else if (strcmp(mname, "11_bytes") == 0) dbufstats->cache_level_11_bytes = value;
                    else if (strcmp(mname, "10") == 0) dbufstats->cache_level_10 = value;
                    else if (strcmp(mname, "10_bytes") == 0) dbufstats->cache_level_10_bytes = value;
                    else if (strcmp(mname, "9") == 0) dbufstats->cache_level_9 = value;
                    else if (strcmp(mname, "9_bytes") == 0) dbufstats->cache_level_9_bytes = value;
                    else if (strcmp(mname, "8") == 0) dbufstats->cache_level_8 = value;
                    else if (strcmp(mname, "8_bytes") == 0) dbufstats->cache_level_8_bytes = value;
                    else if (strcmp(mname, "7") == 0) dbufstats->cache_level_7 = value;
                    else if (strcmp(mname, "7_bytes") == 0) dbufstats->cache_level_7_bytes = value;
                    else if (strcmp(mname, "6") == 0) dbufstats->cache_level_6 = value;
                    else if (strcmp(mname, "6_bytes") == 0) dbufstats->cache_level_6_bytes = value;
                    else if (strcmp(mname, "5") == 0) dbufstats->cache_level_5 = value;
                    else if (strcmp(mname, "5_bytes") == 0) dbufstats->cache_level_5_bytes = value;
                    else if (strcmp(mname, "4") == 0) dbufstats->cache_level_4 = value;
                    else if (strcmp(mname, "4_bytes") == 0) dbufstats->cache_level_4_bytes = value;
                    else if (strcmp(mname, "3") == 0) dbufstats->cache_level_3 = value;
                    else if (strcmp(mname, "3_bytes") == 0) dbufstats->cache_level_3_bytes = value;
                    else if (strcmp(mname, "2") == 0) dbufstats->cache_level_2 = value;
                    else if (strcmp(mname, "2_bytes") == 0) dbufstats->cache_level_2_bytes = value;
                    else if (strcmp(mname, "1") == 0) dbufstats->cache_level_1 = value;
                    else if (strcmp(mname, "1_bytes") == 0) dbufstats->cache_level_1_bytes = value;
                    else if (strcmp(mname, "0") == 0) dbufstats->cache_level_0 = value;
                    else if (strcmp(mname, "0_bytes") == 0) dbufstats->cache_level_0_bytes = value;
                }
                else if (strcmp(mname, "count") == 0) dbufstats->cache_count = value;
                else if (strcmp(mname, "size_bytes") == 0) dbufstats->cache_size_bytes = value;
                else if (strcmp(mname, "size_bytes_max") == 0) dbufstats->cache_size_bytes_max = value;
                else if (strcmp(mname, "target_bytes") == 0) dbufstats->cache_target_bytes = value;
                else if (strcmp(mname, "lowater_bytes") == 0) dbufstats->cache_lowater_bytes = value;
                else if (strcmp(mname, "hiwater_bytes") == 0) dbufstats->cache_hiwater_bytes = value;
                else if (strcmp(mname, "total_evicts") == 0) dbufstats->cache_total_evicts = value;
            }
            else if (strncmp(mname, "metadata_cache_", 15) == 0) {
                mname += 15;
                if (strcmp(mname, "count") == 0) dbufstats->metadata_cache_count = value;
                else if (strcmp(mname, "size_bytes") == 0) dbufstats->metadata_cache_size_bytes = value;
                else if (strcmp(mname, "size_bytes_max") == 0) dbufstats->metadata_cache_size_bytes_max = value;
                else if (strcmp(mname, "overflow") == 0) dbufstats->metadata_cache_overflow = value;
            }
        }
        free(line);
        fclose(fp);
    }
}
