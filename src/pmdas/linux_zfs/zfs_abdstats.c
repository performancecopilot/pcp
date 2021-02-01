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
#include "zfs_abdstats.h"

void
zfs_abdstats_refresh(zfs_abdstats_t *abdstats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "abdstats") != 0)
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

            if (strncmp(mname, "scatter_", 8) == 0) {
                mname += 8;
                if (strncmp(mname, "page_", 5) == 0) {
                    mname += 5;
                    if (strcmp(mname, "multi_chunk") == 0) abdstats->scatter_page_multi_chunk = value;
                    else if (strcmp(mname, "multi_zone") == 0) abdstats->scatter_page_multi_zone = value;
                    else if (strcmp(mname, "alloc_retry") == 0) abdstats->scatter_page_alloc_retry = value;
                }
                else if (strncmp(mname, "order_", 6) == 0) {
                    mname += 6;
                    if (strcmp(mname, "0") == 0) abdstats->scatter_order_0 = value;
                    else if (strcmp(mname, "1") == 0) abdstats->scatter_order_1 = value;
                    else if (strcmp(mname, "2") == 0) abdstats->scatter_order_2 = value;
                    else if (strcmp(mname, "3") == 0) abdstats->scatter_order_3 = value;
                    else if (strcmp(mname, "4") == 0) abdstats->scatter_order_4 = value;
                    else if (strcmp(mname, "5") == 0) abdstats->scatter_order_5 = value;
                    else if (strcmp(mname, "6") == 0) abdstats->scatter_order_6 = value;
                    else if (strcmp(mname, "7") == 0) abdstats->scatter_order_7 = value;
                    else if (strcmp(mname, "8") == 0) abdstats->scatter_order_8 = value;
                    else if (strcmp(mname, "9") == 0) abdstats->scatter_order_9 = value;
                    else if (strcmp(mname, "10") == 0) abdstats->scatter_order_10 = value;
                }
                else if (strcmp(mname, "cnt") == 0) abdstats->scatter_cnt = value;
                else if (strcmp(mname, "data_size") == 0) abdstats->scatter_data_size = value;
                else if (strcmp(mname, "chunk_waste") == 0) abdstats->scatter_chunk_waste = value;
                else if (strcmp(mname, "sg_table_retry") == 0) abdstats->scatter_sg_table_retry = value;
            }
            else if (strcmp(mname, "struct_size") == 0) abdstats->struct_size = value;
            else if (strcmp(mname, "linear_cnt") == 0) abdstats->linear_cnt = value;
            else if (strcmp(mname, "linear_data_size") == 0) abdstats->linear_data_size = value;
        }
        free(line);
        fclose(fp);
    }
}
