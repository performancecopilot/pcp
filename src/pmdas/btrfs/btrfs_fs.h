/*
 * Copyright (c) 2026 Red Hat.
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

#ifndef _BTRFS_FS_H
#define _BTRFS_FS_H

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

enum { BTRFS_FS_INDOM = 0 };

enum {
    BTRFS_INFO_LABEL = 0,
    BTRFS_INFO_NODESIZE,
    BTRFS_INFO_SECTORSIZE,
    BTRFS_INFO_GENERATION,
    BTRFS_INFO_CHECKSUM,
    BTRFS_INFO_METADATA_UUID,
};

enum {
    BTRFS_COMMIT_COUNT = 0,
    BTRFS_COMMIT_LAST_TIME,
    BTRFS_COMMIT_MAX_TIME,
    BTRFS_COMMIT_TOTAL_TIME,
};

enum {
    BTRFS_ALLOC_TOTAL_BYTES = 0,
    BTRFS_ALLOC_BYTES_USED,
    BTRFS_ALLOC_BYTES_PINNED,
    BTRFS_ALLOC_BYTES_RESERVED,
    BTRFS_ALLOC_BYTES_READONLY,
    BTRFS_ALLOC_DISK_USED,
    BTRFS_ALLOC_DISK_TOTAL,
};

enum {
    BTRFS_GLOBAL_RSV_SIZE = 0,
    BTRFS_GLOBAL_RSV_RESERVED,
};

enum {
    BTRFS_DISCARD_DISCARDABLE_BYTES = 0,
    BTRFS_DISCARD_DISCARDABLE_EXTENTS,
    BTRFS_DISCARD_BITMAP_BYTES,
    BTRFS_DISCARD_EXTENT_BYTES,
    BTRFS_DISCARD_BYTES_SAVED,
    BTRFS_DISCARD_IOPS_LIMIT,
    BTRFS_DISCARD_KBPS_LIMIT,
    BTRFS_DISCARD_MAX_SIZE,
};

typedef struct btrfs_info {
    char	label[256];
    uint64_t	nodesize;
    uint64_t	sectorsize;
    uint64_t	generation;
    char	checksum[64];
    char	metadata_uuid[64];
} btrfs_info_t;

typedef struct btrfs_commit {
    uint64_t	commits;
    uint64_t	last_commit_ms;
    uint64_t	max_commit_ms;
    uint64_t	total_commit_ms;
} btrfs_commit_t;

typedef struct btrfs_alloc {
    uint64_t	total_bytes;
    uint64_t	bytes_used;
    uint64_t	bytes_pinned;
    uint64_t	bytes_reserved;
    uint64_t	bytes_readonly;
    uint64_t	disk_used;
    uint64_t	disk_total;
} btrfs_alloc_t;

typedef struct btrfs_global_rsv {
    uint64_t	size;
    uint64_t	reserved;
} btrfs_global_rsv_t;

typedef struct btrfs_discard {
    uint64_t	discardable_bytes;
    uint64_t	discardable_extents;
    uint64_t	discard_bitmap_bytes;
    uint64_t	discard_extent_bytes;
    uint64_t	discard_bytes_saved;
    uint32_t	iops_limit;
    uint32_t	kbps_limit;
    uint64_t	max_discard_size;
} btrfs_discard_t;

typedef struct btrfs_fs {
    char		uuid[64];
    btrfs_info_t	info;
    btrfs_commit_t	commit;
    btrfs_alloc_t	alloc_data;
    btrfs_alloc_t	alloc_metadata;
    btrfs_alloc_t	alloc_system;
    btrfs_global_rsv_t	global_rsv;
    btrfs_discard_t	discard;
} btrfs_fs_t;

extern void btrfs_fs_refresh(pmInDom indom);

#endif /* _BTRFS_FS_H */
