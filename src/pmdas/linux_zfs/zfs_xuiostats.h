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

typedef struct zfs_xuiostats {
    uint64_t onloan_read_buf;
    uint64_t onloan_write_buf;
    uint64_t read_buf_copied;
    uint64_t read_buf_nocopy;
    uint64_t write_buf_copied;
    uint64_t write_buf_nocopy;
} zfs_xuiostats_t;

void zfs_xuiostats_refresh(zfs_xuiostats_t *xuiostats);
