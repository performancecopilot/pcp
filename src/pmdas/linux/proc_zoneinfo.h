/*
 * Linux /proc/zoneinfo metrics cluster
 *
 * Copyright (c) 2016-2017 Fujitsu.
 * Copyright (c) 2016-2018 Red Hat.
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

enum {
    ZONE_FREE                          = 0,
    ZONE_MIN                           = 1,
    ZONE_LOW                           = 2,
    ZONE_HIGH                          = 3,
    ZONE_SCANNED                       = 4,
    ZONE_SPANNED                       = 5,
    ZONE_PRESENT                       = 6,
    ZONE_MANAGED                       = 7,
    ZONE_NR_FREE_PAGES                 = 8,
    ZONE_NR_ALLOC_BATCH                = 9,
    ZONE_NR_INACTIVE_ANON              = 10,
    ZONE_NR_ACTIVE_ANON                = 11,
    ZONE_NR_INACTIVE_FILE              = 12,
    ZONE_NR_ACTIVE_FILE                = 13,
    ZONE_NR_UNEVICTABLE                = 14,
    ZONE_NR_MLOCK                      = 15,
    ZONE_NR_ANON_PAGES                 = 16,
    ZONE_NR_MAPPED                     = 17,
    ZONE_NR_FILE_PAGES                 = 18,
    ZONE_NR_DIRTY                      = 19,
    ZONE_NR_WRITEBACK                  = 20,
    ZONE_NR_SLAB_RECLAIMABLE           = 21,
    ZONE_NR_SLAB_UNRECLAIMABLE         = 22,
    ZONE_NR_PAGE_TABLE_PAGES           = 23,
    ZONE_NR_KERNEL_STACK               = 24,
    ZONE_NR_UNSTABLE                   = 25,
    ZONE_NR_BOUNCE                     = 26,
    ZONE_NR_VMSCAN_WRITE               = 27,
    ZONE_NR_VMSCAN_IMMEDIATE_RECLAIM   = 28,
    ZONE_NR_WRITEBACK_TEMP             = 29,
    ZONE_NR_ISOLATED_ANON              = 30,
    ZONE_NR_ISOLATED_FILE              = 31,
    ZONE_NR_SHMEM                      = 32,
    ZONE_NR_DIRTIED                    = 33,
    ZONE_NR_WRITTEN                    = 34,
    ZONE_NUMA_HIT                      = 35,
    ZONE_NUMA_MISS                     = 36,
    ZONE_NUMA_FOREIGN                  = 37,
    ZONE_NUMA_INTERLEAVE               = 38,
    ZONE_NUMA_LOCAL                    = 39,
    ZONE_NUMA_OTHER                    = 40,
    ZONE_WORKINGSET_REFAULT            = 41,
    ZONE_WORKINGSET_ACTIVATE           = 42,
    ZONE_WORKINGSET_NODERECLAIM        = 43,
    ZONE_NR_ANON_TRANSPARENT_HUGEPAGES = 44,
    ZONE_NR_FREE_CMA                   = 45,
    /* enumerate all values here */
    ZONE_VALUES	/* maximum value */
};

#define ZONE_NAMELEN	32

typedef struct {
    __uint32_t	node;
    char	zone[ZONE_NAMELEN];
    __uint64_t	values[ZONE_VALUES];
} zoneinfo_entry_t;

typedef struct {
    __uint32_t	node;
    __uint32_t	lowmem;
    char	zone[ZONE_NAMELEN];
    __uint64_t	value;
} zoneprot_entry_t;

extern int refresh_proc_zoneinfo(pmInDom indom,
                                 pmInDom zoneinfo_protection_indom);
