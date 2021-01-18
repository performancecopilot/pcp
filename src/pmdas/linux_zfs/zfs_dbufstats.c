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

    if (zfs_stats_file_check(fname, "dbufstats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strncmp(mname, "hash_", 5) == 0) {
                if (strcmp(mname, "hash_hits") == 0) dbufstats->hash_hits = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_misses") == 0) dbufstats->hash_misses = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_collisions") == 0) dbufstats->hash_collisions = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_elements") == 0) dbufstats->hash_elements = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_elements_max") == 0) dbufstats->hash_elements_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_chains") == 0) dbufstats->hash_chains = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_chain_max") == 0) dbufstats->hash_chain_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "hash_insert_race") == 0) dbufstats->hash_insert_race = strtoul(mval, NULL, 0);
            }
            if (strncmp(mname, "cache_", 6) == 0) {
                if (strncmp(mname, "cache_level_", 12) == 0) {
                    if (strncmp(mname, "cache_level_11", 14) == 0) {
                        if (strlen(mname) == 14) dbufstats->cache_level_11 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_11_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_10", 14) == 0) {
                        if (strlen(mname) == 14) dbufstats->cache_level_10 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_10_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_9", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_9 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_9_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_8", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_8 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_8_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_7", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_7 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_7_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_6", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_6 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_6_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_5", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_5 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_5_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_4", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_4 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_4_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_3", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_3 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_3_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_2", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_2 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_2_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_1", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_1 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_1_bytes = strtoul(mval, NULL, 0);
                    }
                    else if (strncmp(mname, "cache_level_0", 13) == 0) {
                        if (strlen(mname) == 13) dbufstats->cache_level_0 = strtoul(mval, NULL, 0);
                        else dbufstats->cache_level_0_bytes = strtoul(mval, NULL, 0);
                    }
                }
                else if (strcmp(mname, "cache_count") == 0) dbufstats->cache_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_size_bytes") == 0) dbufstats->cache_size_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_size_bytes_max") == 0) dbufstats->cache_size_bytes_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_target_bytes") == 0) dbufstats->cache_target_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_lowater_bytes") == 0) dbufstats->cache_lowater_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_hiwater_bytes") == 0) dbufstats->cache_hiwater_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "cache_total_evicts") == 0) dbufstats->cache_total_evicts = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "metadata_", 9) == 0) {
                if (strcmp(mname, "metadata_cache_count") == 0) dbufstats->metadata_cache_count = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "metadata_cache_size_bytes") == 0) dbufstats->metadata_cache_size_bytes = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "metadata_cache_size_bytes_max") == 0) dbufstats->metadata_cache_size_bytes_max = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "metadata_cache_overflow") == 0) dbufstats->metadata_cache_overflow = strtoul(mval, NULL, 0);
            }
        }
        free(line);
    }
    fclose(fp);
}
