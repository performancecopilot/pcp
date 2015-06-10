/*
 * Device Mapper PMDA - Cache (dm-cache) Stats
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

#ifndef DMCACHE_H
#define DMCACHE_H

enum {
    CACHE_SIZE = 0,
    CACHE_META_BLOCKSIZE,
    CACHE_META_USED,
    CACHE_META_TOTAL,
    CACHE_BLOCKSIZE,
    CACHE_USED,
    CACHE_TOTAL,
    CACHE_READHITS,
    CACHE_READMISSES,
    CACHE_WRITEHITS,
    CACHE_WRITEMISSES,
    CACHE_DEMOTIONS,
    CACHE_PROMOTIONS,
    CACHE_DIRTY,
    CACHE_IOMODE_CODE,
    CACHE_IOMODE,
    NUM_CACHE_STATS
};

struct cache_stats {
    __uint64_t size;
    __uint32_t meta_blocksize;
    __uint64_t meta_used;
    __uint64_t meta_total;
    __uint32_t cache_blocksize;
    __uint64_t cache_used;
    __uint64_t cache_total;
    __uint32_t read_hits;
    __uint32_t read_misses;
    __uint32_t write_hits;
    __uint32_t write_misses;
    __uint32_t demotions;
    __uint32_t promotions;
    __uint64_t dirty;
    __uint32_t io_mode_code;
    char io_mode[13];
};

extern int dm_cache_fetch(int, struct cache_stats *, pmAtomValue *);
extern int dm_refresh_cache(const char *, struct cache_stats *);
extern int dm_cache_instance_refresh(void);
extern void dm_cache_setup(void);

#endif /* DMCACHE_H */
