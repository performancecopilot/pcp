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

typedef struct zfs_zilstats {
    uint64_t commit_count;
    uint64_t commit_writer_count;
    uint64_t itx_count;
    uint64_t itx_indirect_count;
    uint64_t itx_indirect_bytes;
    uint64_t itx_copied_count;
    uint64_t itx_copied_bytes;
    uint64_t itx_needcopy_count;
    uint64_t itx_needcopy_bytes;
    uint64_t itx_metaslab_normal_count;
    uint64_t itx_metaslab_normal_bytes;
    uint64_t itx_metaslab_slog_count;
    uint64_t itx_metaslab_slog_bytes;
} zfs_zilstats_t;

void zfs_zilstats_refresh(zfs_zilstats_t *zilstats);
