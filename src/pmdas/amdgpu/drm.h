/*
 * Copyright (c) 2024 Red Hat.
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
#ifndef _DRM_H
#define _DRM_H

#include <libdrm/amdgpu.h>
#include <libdrm/amdgpu_drm.h>
#include <xf86drm.h>

/* Error codes */
typedef enum {
    DRM_SUCCESS = 0,
    DRM_ERROR_NOT_FOUND,
    DRM_ERROR_MEMORY,
    DRM_ERROR_NO_DATA,
    DRM_ERROR_INSUFFICIENT_RESOURCES,
    DRM_ERROR_UNKNOWN = 99
} drmReturn_t;

typedef struct {
    uint64_t	total;
    uint64_t	usable;
    uint64_t	used;
} drmMemory_t;

extern int DRMShutdown(drmDevicePtr [], uint32_t);
extern const char *DRMErrStr(drmReturn_t);

extern int DRMDeviceGetDevices(drmDevicePtr *[], uint32_t *, uint32_t *);
extern int getAMDDevice(drmDevicePtr, amdgpu_device_handle *, int *);
extern void releaseAMDDevice(amdgpu_device_handle, int);

extern int DRMDeviceGetName(amdgpu_device_handle, void *);
extern int DRMDeviceGetGPUInfo(amdgpu_device_handle, void *);
extern int DRMDeviceGetMemoryClock(amdgpu_device_handle, void *);
extern int DRMDeviceGetGPUClock(amdgpu_device_handle, void *);
extern int DRMDeviceGetTemperature(amdgpu_device_handle, void *);
extern int DRMDeviceGetGPULoad(amdgpu_device_handle, void *);
extern int DRMDeviceGetGPUAveragePower(amdgpu_device_handle, void *);
extern int DRMDeviceGetMemoryInfo(amdgpu_device_handle, void *);
#endif /* _DRM_H */
