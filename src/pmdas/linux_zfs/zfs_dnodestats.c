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

    if (zfs_stats_file_check(fname, sizeof(fname), "dnodestats") != 0)
        return;

    fp = fopen(fname, "r");
    if (fp != NULL) {
        while (getline(&line, &len, fp) != -1) {
            mname = strtok(line, delim);
            mval  = strtok(NULL, delim); // not used
            mval  = strtok(NULL, delim);
            if (strncmp(mname, "dnode_alloc", 11) == 0) {
                if (strcmp(mname, "dnode_allocate") == 0) dnodestats->allocate = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_alloc_next_block") == 0) dnodestats->alloc_next_block = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_alloc_next_chunk") == 0) dnodestats->alloc_next_chunk = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_alloc_race") == 0) dnodestats->alloc_race = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "dnode_hold_", 11) == 0) {
                if (strncmp(mname, "dnode_hold_free_", 16) == 0) {
                    if (strcmp(mname, "dnode_hold_free_hits") == 0) dnodestats->hold_free_hits = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_free_lock_misses") == 0) dnodestats->hold_free_lock_misses = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_free_lock_retry") == 0) dnodestats->hold_free_lock_retry = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_free_misses") == 0) dnodestats->hold_free_misses = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_free_overflow") == 0) dnodestats->hold_free_overflow = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_free_refcount") == 0) dnodestats->hold_free_refcount = strtoul(mval, NULL, 0);
                }
                else if (strncmp(mname, "dnode_hold_alloc_", 17) == 0) {
                    if (strcmp(mname, "dnode_hold_alloc_hits") == 0) dnodestats->hold_alloc_hits = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_alloc_interior") == 0) dnodestats->hold_alloc_interior = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_alloc_lock_misses") == 0) dnodestats->hold_alloc_lock_misses = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_alloc_lock_retry") == 0) dnodestats->hold_alloc_lock_retry = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_alloc_misses") == 0) dnodestats->hold_alloc_misses = strtoul(mval, NULL, 0);
                    else if (strcmp(mname, "dnode_hold_alloc_type_none") == 0) dnodestats->hold_alloc_type_none = strtoul(mval, NULL, 0);
                }
                else if (strcmp(mname, "dnode_hold_dbuf_hold") == 0) dnodestats->hold_dbuf_hold = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_hold_dbuf_read") == 0) dnodestats->hold_dbuf_read = strtoul(mval, NULL, 0);
            }
            else if (strncmp(mname, "dnode_move_", 11) == 0) {
                if (strcmp(mname, "dnode_move_active") == 0) dnodestats->move_active = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_handle") == 0) dnodestats->move_handle = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_invalid") == 0) dnodestats->move_invalid = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_recheck1") == 0) dnodestats->move_recheck1 = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_recheck2") == 0) dnodestats->move_recheck2 = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_rwlock") == 0) dnodestats->move_rwlock = strtoul(mval, NULL, 0);
                else if (strcmp(mname, "dnode_move_special") == 0) dnodestats->move_special = strtoul(mval, NULL, 0);
            }
            else if (strcmp(mname, "dnode_reallocate") == 0) dnodestats->reallocate = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dnode_buf_evict") == 0) dnodestats->buf_evict = strtoul(mval, NULL, 0);
            else if (strcmp(mname, "dnode_free_interior_lock_retry") == 0) dnodestats->free_interior_lock_retry = strtoul(mval, NULL, 0);
        }
        free(line);
    }
    fclose(fp);
}
