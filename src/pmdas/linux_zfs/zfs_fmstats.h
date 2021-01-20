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

typedef struct zfs_fmstats {
    uint64_t erpt_dropped;
    uint64_t erpt_set_failed;
    uint64_t fmri_set_failed;
    uint64_t payload_set_failed;
} zfs_fmstats_t;

void zfs_fmstats_refresh(zfs_fmstats_t *fmstats);
