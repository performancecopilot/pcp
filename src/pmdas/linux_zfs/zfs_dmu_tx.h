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

typedef struct zfs_dmu_tx {
    uint64_t assigned;
    uint64_t delay;
    uint64_t error;
    uint64_t suspended;
    uint64_t group;
    uint64_t memory_reserve;
    uint64_t memory_reclaim;
    uint64_t dirty_throttle;
    uint64_t dirty_delay;
    uint64_t dirty_over_max;
    uint64_t dirty_frees_delay;
    uint64_t quota;
} zfs_dmu_tx_t;

void zfs_dmu_tx_refresh(zfs_dmu_tx_t *dmu_tx);
