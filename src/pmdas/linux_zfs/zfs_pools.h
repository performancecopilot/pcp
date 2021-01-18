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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

enum { ZFS_POOL_INDOM = 0, };

enum { /* metric item identifiers */
    ZFS_POOL_STATE = 0,
    ZFS_POOL_NREAD,
    ZFS_POOL_NWRITTEN,
    ZFS_POOL_READS,
    ZFS_POOL_WRITES,
    ZFS_POOL_WTIME,
    ZFS_POOL_WLENTIME,
    ZFS_POOL_WUPDATE,
    ZFS_POOL_RTIME,
    ZFS_POOL_RLENTIME,
    ZFS_POOL_RUPDATE,
    ZFS_POOL_WCNT,
    ZFS_POOL_RCNT,
};

typedef struct zfs_poolstats {
    uint32_t state;
    uint64_t nread;
    uint64_t nwritten;
    uint64_t reads;
    uint64_t writes;
    uint64_t wtime;
    uint64_t wlentime;
    uint64_t wupdate;
    uint64_t rtime;
    uint64_t rlentime;
    uint64_t rupdate;
    uint64_t wcnt;
    uint64_t rcnt;
} zfs_poolstats_t;

void zfs_pools_init(zfs_poolstats_t **, pmdaInstid **, pmdaIndom *);
void zfs_pools_clear(zfs_poolstats_t **, pmdaInstid **, pmdaIndom *);
void zfs_poolstats_refresh(zfs_poolstats_t **, pmdaInstid **, pmdaIndom *);
