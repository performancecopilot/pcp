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
#include "zfs_dnodestats.h"

void
zfs_dnodestats_refresh(zfs_dnodestats_t *dnodestats)
{
    char *line = NULL, *mname, *mval;
    char delim[] = " ";
    char fname[MAXPATHLEN];
    FILE *fp;
    size_t len = 0;
    uint64_t value;

    if (zfs_stats_file_check(fname, sizeof(fname), "dnodestats") != 0)
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

            mname += 6;
            if (strcmp(mname, "allocate") == 0) dnodestats->allocate = value;
            else if (strncmp(mname, "alloc_", 6) == 0) {
                mname += 6;
                if (strcmp(mname, "next_block") == 0) dnodestats->alloc_next_block = value;
                else if (strcmp(mname, "next_chunk") == 0) dnodestats->alloc_next_chunk = value;
                else if (strcmp(mname, "race") == 0) dnodestats->alloc_race = value;
            }
            else if (strncmp(mname, "hold_", 5) == 0) {
                mname += 5;
                if (strncmp(mname, "free_", 5) == 0) {
                    mname += 5;
                    if (strcmp(mname, "hits") == 0) dnodestats->hold_free_hits = value;
                    else if (strcmp(mname, "lock_misses") == 0) dnodestats->hold_free_lock_misses = value;
                    else if (strcmp(mname, "lock_retry") == 0) dnodestats->hold_free_lock_retry = value;
                    else if (strcmp(mname, "misses") == 0) dnodestats->hold_free_misses = value;
                    else if (strcmp(mname, "overflow") == 0) dnodestats->hold_free_overflow = value;
                    else if (strcmp(mname, "refcount") == 0) dnodestats->hold_free_refcount = value;
                }
                else if (strncmp(mname, "alloc_", 6) == 0) {
                    mname += 6;
                    if (strcmp(mname, "hits") == 0) dnodestats->hold_alloc_hits = value;
                    else if (strcmp(mname, "interior") == 0) dnodestats->hold_alloc_interior = value;
                    else if (strcmp(mname, "lock_misses") == 0) dnodestats->hold_alloc_lock_misses = value;
                    else if (strcmp(mname, "lock_retry") == 0) dnodestats->hold_alloc_lock_retry = value;
                    else if (strcmp(mname, "misses") == 0) dnodestats->hold_alloc_misses = value;
                    else if (strcmp(mname, "type_none") == 0) dnodestats->hold_alloc_type_none = value;
                }
                else if (strcmp(mname, "hold_dbuf_hold") == 0) dnodestats->hold_dbuf_hold = value;
                else if (strcmp(mname, "hold_dbuf_read") == 0) dnodestats->hold_dbuf_read = value;
            }
            else if (strncmp(mname, "move_", 5) == 0) {
                mname += 5;
                if (strcmp(mname, "active") == 0) dnodestats->move_active = value;
                else if (strcmp(mname, "handle") == 0) dnodestats->move_handle = value;
                else if (strcmp(mname, "invalid") == 0) dnodestats->move_invalid = value;
                else if (strcmp(mname, "recheck1") == 0) dnodestats->move_recheck1 = value;
                else if (strcmp(mname, "recheck2") == 0) dnodestats->move_recheck2 = value;
                else if (strcmp(mname, "rwlock") == 0) dnodestats->move_rwlock = value;
                else if (strcmp(mname, "special") == 0) dnodestats->move_special = value;
            }
            else if (strcmp(mname, "reallocate") == 0) dnodestats->reallocate = value;
            else if (strcmp(mname, "buf_evict") == 0) dnodestats->buf_evict = value;
            else if (strcmp(mname, "free_interior_lock_retry") == 0) dnodestats->free_interior_lock_retry = value;
        }
        free(line);
        fclose(fp);
    }
}
