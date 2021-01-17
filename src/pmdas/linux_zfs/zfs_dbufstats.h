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

typedef struct zfs_dbufstats {
    uint64_t cache_count;
    uint64_t cache_size_bytes;
    uint64_t cache_size_bytes_max;
    uint64_t cache_target_bytes;
    uint64_t cache_lowater_bytes;
    uint64_t cache_hiwater_bytes;
    uint64_t cache_total_evicts;
    uint64_t cache_level_0;
    uint64_t cache_level_1;
    uint64_t cache_level_2;
    uint64_t cache_level_3;
    uint64_t cache_level_4;
    uint64_t cache_level_5;
    uint64_t cache_level_6;
    uint64_t cache_level_7;
    uint64_t cache_level_8;
    uint64_t cache_level_9;
    uint64_t cache_level_10;
    uint64_t cache_level_11;
    uint64_t cache_level_0_bytes;
    uint64_t cache_level_1_bytes;
    uint64_t cache_level_2_bytes;
    uint64_t cache_level_3_bytes;
    uint64_t cache_level_4_bytes;
    uint64_t cache_level_5_bytes;
    uint64_t cache_level_6_bytes;
    uint64_t cache_level_7_bytes;
    uint64_t cache_level_8_bytes;
    uint64_t cache_level_9_bytes;
    uint64_t cache_level_10_bytes;
    uint64_t cache_level_11_bytes;
    uint64_t hash_hits;
    uint64_t hash_misses;
    uint64_t hash_collisions;
    uint64_t hash_elements;
    uint64_t hash_elements_max;
    uint64_t hash_chains;
    uint64_t hash_chain_max;
    uint64_t hash_insert_race;
    uint64_t metadata_cache_count;
    uint64_t metadata_cache_size_bytes;
    uint64_t metadata_cache_size_bytes_max;
    uint64_t metadata_cache_overflow;
} zfs_dbufstats_t;

void zfs_dbufstats_refresh(zfs_dbufstats_t *dbufstats);
