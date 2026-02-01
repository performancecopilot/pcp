/*
 * APFS statistics for Darwin PMDA
 *
 * Copyright (c) 2026 Paul Smith.
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

#ifndef APFS_H
#define APFS_H

#include <stdint.h>

#define APFS_NAME_MAX 128

/* Per-APFS container statistics */
struct apfs_container_stat {
    char name[APFS_NAME_MAX];     /* Container identifier */
    uint64_t block_size;          /* Block size in bytes */
    uint64_t bytes_read;          /* Bytes read from block device */
    uint64_t bytes_written;       /* Bytes written to block device */
    uint64_t read_requests;       /* Read operations */
    uint64_t write_requests;      /* Write operations */
    uint64_t transactions;        /* Transactions flushed */
    uint64_t cache_hits;          /* Object cache hits */
    uint64_t cache_evictions;     /* Object cache evictions */
    uint64_t read_errors;         /* Metadata read errors */
    uint64_t write_errors;        /* Metadata write errors */
};

/* Per-APFS volume statistics */
struct apfs_volume_stat {
    char name[APFS_NAME_MAX];     /* Volume name */
    int encrypted;                /* 0 or 1 */
    int locked;                   /* 0 or 1 */
};

/* Collection of all APFS statistics */
struct apfs_stats {
    int container_count;
    int container_highwater;      /* Allocated space */
    struct apfs_container_stat *containers;

    int volume_count;
    int volume_highwater;         /* Allocated space */
    struct apfs_volume_stat *volumes;
};

/* Refresh APFS statistics (called before each fetch) */
struct pmdaIndom;
extern int refresh_apfs(struct apfs_stats *stats,
                        struct pmdaIndom *container_indom,
                        struct pmdaIndom *volume_indom);

/* Fetch APFS metrics */
extern int fetch_apfs(unsigned int item, unsigned int inst, pmAtomValue *atom);

#endif /* APFS_H */
