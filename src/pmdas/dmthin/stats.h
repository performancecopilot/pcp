/*
 * dm-thin stats derrived from dmsetup status
 *
 * Copyright (c) 2015 Red Hat.
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

#ifndef STATS_H
#define STATS_H

enum {
    POOL_META_USED = 0,
    POOL_META_TOTAL,
    POOL_DATA_USED,
    POOL_DATA_TOTAL,
    POOL_HELD_ROOT,
    POOL_DISCARD_PASSDOWN,
    POOL_READ_MODE,
    POOL_NO_SPACE_MODE,
    NUM_POOL_STATS
};

enum {
    VOL_NUM_MAPPED_SECTORS = 0,
    VOL_HIGHEST_MAPPED_SECTORS,
    NUM_VOL_STATS
};

struct pool_stats {
    __uint64_t meta_used;
    __uint64_t meta_total;
    __uint64_t data_used;
    __uint64_t data_total;
    char held_root[20];
    char read_mode[5];
    char discard_passdown[20];
    char no_space_mode[20];
};

struct vol_stats {
    __uint64_t num_mapped_sectors;
    __uint64_t high_mapped_sector;
};

extern int dmthin_pool_fetch(int, struct pool_stats *, pmAtomValue *);
extern int dmthin_vol_fetch(int, struct vol_stats *, pmAtomValue *);
extern int dmthin_refresh_pool(const int, const char *, const char *, struct pool_stats *);
extern int dmthin_refresh_vol(const int, const char *, const char *, struct vol_stats *);

#endif /* STATS_H */
