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

typedef struct zfs_abdstats {
    uint64_t struct_size;
    uint64_t linear_cnt;
    uint64_t linear_data_size;
    uint64_t scatter_cnt;
    uint64_t scatter_data_size;
    uint64_t scatter_chunk_waste;
    uint64_t scatter_order_0;
    uint64_t scatter_order_1;
    uint64_t scatter_order_2;
    uint64_t scatter_order_3;
    uint64_t scatter_order_4;
    uint64_t scatter_order_5;
    uint64_t scatter_order_6;
    uint64_t scatter_order_7;
    uint64_t scatter_order_8;
    uint64_t scatter_order_9;
    uint64_t scatter_order_10;
    uint64_t scatter_page_multi_chunk;
    uint64_t scatter_page_multi_zone;
    uint64_t scatter_page_alloc_retry;
    uint64_t scatter_sg_table_retry;
} zfs_abdstats_t;

void zfs_abdstats_refresh(zfs_abdstats_t *abdstats);
