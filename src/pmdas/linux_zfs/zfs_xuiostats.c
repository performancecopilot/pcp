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
#include "zfs_xuiostats.h"

void
zfs_xuiostats_refresh(zfs_xuiostats_t *xuiostats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;

    if (zfs_stats_file_check(fname, "xuio_stats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strcmp(mname, "onloan_read_buf") == 0) xuiostats->onloan_read_buf = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "onloan_write_buf") == 0) xuiostats->onloan_write_buf = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "read_buf_copied") == 0) xuiostats->read_buf_copied = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "read_buf_nocopy") == 0) xuiostats->read_buf_nocopy = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "write_buf_copied") == 0) xuiostats->write_buf_copied = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "write_buf_nocopy") == 0) xuiostats->write_buf_nocopy = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
