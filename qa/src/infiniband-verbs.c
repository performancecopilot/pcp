/*
 * Copyright (c) 2025 Red Hat.
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

/*
 * Mock InfiniBand verbs library for testing.
 * Provides ibv_* functions used by the IB PMDA.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <infiniband/verbs.h>

/* Mock device table matching the umad mock */
typedef struct mock_ibv_device_s {
    struct ibv_device device;
    char name[64];
    uint64_t node_guid;
    uint32_t vendor_id;
    uint32_t vendor_part_id;
    uint32_t hw_ver;
} mock_ibv_device_t;

static mock_ibv_device_t mock_devices[] = {
    {
        .name = "mlx5_0",
        .node_guid = 0x506b4b03005c7a20ULL,
        .vendor_id = 0x02c9,  /* Mellanox */
        .vendor_part_id = 4099,
        .hw_ver = 0,
    },
    {
        .name = "mlx5_1",
        .node_guid = 0x248a0703009c7b30ULL,
        .vendor_id = 0x02c9,  /* Mellanox */
        .vendor_part_id = 4119,
        .hw_ver = 0,
    },
};

#define NUM_MOCK_DEVICES (sizeof(mock_devices)/sizeof(mock_devices[0]))

struct ibv_device **
ibv_get_device_list(int *num_devices)
{
    struct ibv_device **list;
    int i;

    list = calloc(NUM_MOCK_DEVICES + 1, sizeof(struct ibv_device *));
    if (!list) {
        if (num_devices)
            *num_devices = 0;
        return NULL;
    }

    for (i = 0; i < NUM_MOCK_DEVICES; i++) {
        list[i] = &mock_devices[i].device;
    }
    list[NUM_MOCK_DEVICES] = NULL;

    if (num_devices)
        *num_devices = NUM_MOCK_DEVICES;

    return list;
}

void
ibv_free_device_list(struct ibv_device **list)
{
    if (list)
        free(list);
}

const char *
ibv_get_device_name(struct ibv_device *device)
{
    int i;

    for (i = 0; i < NUM_MOCK_DEVICES; i++) {
        if (device == &mock_devices[i].device) {
            return mock_devices[i].name;
        }
    }

    return "unknown";
}

struct ibv_context *
ibv_open_device(struct ibv_device *device)
{
    struct ibv_context *ctx;

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx)
        return NULL;

    ctx->device = device;
    return ctx;
}

int
ibv_close_device(struct ibv_context *context)
{
    if (context)
        free(context);
    return 0;
}

int
ibv_query_device(struct ibv_context *context, struct ibv_device_attr *device_attr)
{
    int i;

    if (!context || !device_attr)
        return -1;

    memset(device_attr, 0, sizeof(*device_attr));

    /* Find which device this context belongs to */
    for (i = 0; i < NUM_MOCK_DEVICES; i++) {
        if (context->device == &mock_devices[i].device) {
            device_attr->vendor_id = mock_devices[i].vendor_id;
            device_attr->vendor_part_id = mock_devices[i].vendor_part_id;
            device_attr->hw_ver = mock_devices[i].hw_ver;
            device_attr->node_guid = mock_devices[i].node_guid;
            device_attr->sys_image_guid = mock_devices[i].node_guid;

            /* Fill in some reasonable default values */
            device_attr->max_mr_size = 0xffffffffffffffffULL;
            device_attr->page_size_cap = 0xfffff000;
            device_attr->max_qp = 262144;
            device_attr->max_qp_wr = 32768;
            device_attr->max_sge = 30;
            device_attr->max_cq = 16777216;
            device_attr->max_cqe = 4194303;
            device_attr->max_mr = 16777216;
            device_attr->max_pd = 16777216;
            device_attr->phys_port_cnt = (i == 0) ? 1 : 2;

            return 0;
        }
    }

    return -1;
}
