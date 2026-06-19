/*
 * Copyright (c) 2026 Ken McDonell.  All Rights Reserved.
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
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pcp/pmapi.h>

#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif
#include "drm.h"

#include <libdrm/amdgpu.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>

/* Looks like AMD kept PCI_VENDOR_ID_ATI for its GPU IDs */
#define PCI_VENDOR_ID_ATI 0x1002

/*
 * fake implementation of drm*() and amd*() library routines for
 * testing if you don't have AMD GPU hardware ...
 *
 * #define FAKE_DRM to the number of required cards ...
 */
#define FAKE_DRM 2

int
drmGetDevices(drmDevicePtr devices[], int max_devices)
{
    int i;
    fprintf(stderr, "fake drmGetDevices(%p,%d) called\n", devices, max_devices);
    if (devices == NULL)
	return FAKE_DRM;
    for (i = 0; i < max_devices ; i++) {
	devices[i] = (drmDevice *)calloc(1, sizeof(drmDevice));
	devices[i]->bustype = DRM_BUS_PCI;
	devices[i]->deviceinfo.pci = (drmPciDeviceInfo *)calloc(1, sizeof(drmPciDeviceInfo));
	devices[i]->deviceinfo.pci->vendor_id = PCI_VENDOR_ID_ATI;
	devices[i]->nodes = (char **)calloc(DRM_NODE_MAX, sizeof(char *));
	if (i == 0) {
	    devices[i]->available_nodes = 1 << DRM_NODE_RENDER;
	    devices[i]->nodes[DRM_NODE_RENDER] = "/dev/null";
	}
	else {
	    devices[i]->available_nodes = 1 << DRM_NODE_RENDER;
	    devices[i]->nodes[DRM_NODE_RENDER] = "/no/such/device";
	    devices[i]->available_nodes |= 1 << DRM_NODE_PRIMARY;
	    devices[i]->nodes[DRM_NODE_PRIMARY] = "/dev/null";
	}
    }
    return FAKE_DRM;
}

void
drmFreeDevices(drmDevicePtr devices[], int count)
{
    fprintf(stderr, "fake drmFreeDevices(%p,%d) called\n", devices, count);
    /*
     * this is a fake implementation, don't worry about memory leaks
     */
    return;
}


drmVersionPtr
drmGetVersion(int fd)
{
    fprintf(stderr, "fake drmGetVersion(%d) called\n", fd);
    static drmVersion res = { 1, 2, 3, 6, "amdgpu", 0, NULL, 0, NULL };
    return &res;
}

void
drmFreeVersion(drmVersionPtr x)
{
    fprintf(stderr, "fake drmFreeVersion(%p) called\n", x);
    return;
}

int
amdgpu_device_initialize(int fd, uint32_t *major_version, uint32_t *minor_version, amdgpu_device_handle *device_handle)
{
    fprintf(stderr, "fake amdgpu_device_initialize(%d,...,%p) called\n", fd, device_handle);
    *major_version = 42;	/* answer to everything */
    *minor_version = 13;	/* lucky */
    return 0;
}

int
amdgpu_device_deinitialize(amdgpu_device_handle dev)
{
    fprintf(stderr, "fake amdgpu_device_deinitialize(%p) called\n", dev);
    return 0;
}

const char *
amdgpu_get_marketing_name(amdgpu_device_handle dev)
{
    static int toggle = 1;
    fprintf(stderr, "fake amdgpu_get_marketing_name(%p) called\n", dev);
    toggle = 1 - toggle;
    if (toggle == 0)
	return "AMD Radeon 780M Graphics";
    else
	return "AMD Radeon RX 7700S";
}

int
amdgpu_query_gpu_info(amdgpu_device_handle dev, struct amdgpu_gpu_info *info)
{
    fprintf(stderr, "fake amdgpu_query_gpu_info(%p,%p) called\n", dev, info);
    info->max_memory_clk = 3210;
    return 0;
}

int
amdgpu_query_sensor_info(amdgpu_device_handle dev, unsigned sensor_type, unsigned size, void *value)
{
    fprintf(stderr, "fake amdgpu_query_sensor_info(%p,%u,%u,%p) called\n", dev, sensor_type, size, value);
    if (size != sizeof(uint32_t)) {
	fprintf(stderr, "Botch: no support for size=%u\n", size);
	return -EINVAL;
    }
    switch (sensor_type) {
	case AMDGPU_INFO_SENSOR_GFX_SCLK:
	    *((uint32_t *)value) = 4321;
	    break;
	case AMDGPU_INFO_SENSOR_GFX_MCLK:
	    *((uint32_t *)value) = 4322;
	    break;
	case AMDGPU_INFO_SENSOR_GPU_TEMP:
	    *((uint32_t *)value) = 38000;
	    break;
	case AMDGPU_INFO_SENSOR_GPU_LOAD:
	    *((uint32_t *)value) = 13;
	    break;
	case AMDGPU_INFO_SENSOR_GPU_AVG_POWER:
	    *((uint32_t *)value) = 42;
	    break;
	default:
	    fprintf(stderr, "Botch: no support for sensor_type=%u\n", sensor_type);
	    return -EINVAL;
    }
    return 0;
}

int
amdgpu_query_info(amdgpu_device_handle dev, unsigned info_id, unsigned size, void *value)
{
    struct drm_amdgpu_memory_info mem;
    fprintf(stderr, "fake amdgpu_query_info(%p,%u,%u,%p) called\n", dev, info_id, size, value);

    switch (info_id) {
        case AMDGPU_INFO_MEMORY:
	    if (size != sizeof(mem)) {
		fprintf(stderr, "Botch: no support for size=%u (not %u)\n", size, (unsigned int)sizeof(mem));
		return -EINVAL;
	    }
	    mem.vram.total_heap_size = 8*1024*1024;
	    mem.vram.usable_heap_size = 7*1024*1024;
	    mem.vram.heap_usage = 6*1024*1024;
	    mem.vram.max_allocation = 5*1024*1024;
	    memcpy((void *)value, (void *)&mem, sizeof(mem));
	    break;
	default:
	    fprintf(stderr, "Botch: no support for info_id=%u\n", info_id);
	    return -EINVAL;
    }
    return 0;
}
