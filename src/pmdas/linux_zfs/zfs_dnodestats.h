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

typedef struct zfs_dnodestats {
    uint64_t hold_dbuf_hold;
    uint64_t hold_dbuf_read;
    uint64_t hold_alloc_hits;
    uint64_t hold_alloc_misses;
    uint64_t hold_alloc_interior;
    uint64_t hold_alloc_lock_retry;
    uint64_t hold_alloc_lock_misses;
    uint64_t hold_alloc_type_none;
    uint64_t hold_free_hits;
    uint64_t hold_free_misses;
    uint64_t hold_free_lock_misses;
    uint64_t hold_free_lock_retry;
    uint64_t hold_free_overflow;
    uint64_t hold_free_refcount;
    uint64_t free_interior_lock_retry;
    uint64_t allocate;
    uint64_t reallocate;
    uint64_t buf_evict;
    uint64_t alloc_next_chunk;
    uint64_t alloc_race;
    uint64_t alloc_next_block;
    uint64_t move_invalid;
    uint64_t move_recheck1;
    uint64_t move_recheck2;
    uint64_t move_special;
    uint64_t move_handle;
    uint64_t move_rwlock;
    uint64_t move_active;
} zfs_dnodestats_t;

void zfs_dnodestats_refresh(zfs_dnodestats_t *dnodestats);
