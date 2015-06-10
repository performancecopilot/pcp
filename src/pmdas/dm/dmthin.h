/*
 * Device Mapper PMDA - Thin Provisioning (dm-thin) Stats
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

#ifndef DMTHIN_H
#define DMTHIN_H

enum {
    POOL_SIZE = 0,
    POOL_TRANS_ID,
    POOL_META_USED,
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
    VOL_SIZE = 0,
    VOL_NUM_MAPPED_SECTORS,
    VOL_HIGHEST_MAPPED_SECTORS,
    NUM_VOL_STATS
};

struct pool_stats {
    __uint64_t size;
    __uint64_t trans_id;
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
    __uint64_t size;
    __uint64_t num_mapped_sectors;
    __uint64_t high_mapped_sector;
};

extern int dm_thin_pool_fetch(int, struct pool_stats *, pmAtomValue *);
extern int dm_thin_vol_fetch(int, struct vol_stats *, pmAtomValue *);
extern int dm_refresh_thin_pool(const char *, struct pool_stats *);
extern int dm_refresh_thin_vol(const char *, struct vol_stats *);
extern int dm_thin_pool_instance_refresh(void);
extern int dm_thin_vol_instance_refresh(void);
extern void dm_thin_setup(void);

#endif /* DMTHIN_H */
