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

#define DSOSUFFIX "so"

/* Looks like AMD kept PCI_VENDOR_ID_ATI for its GPU IDs */
#define PCI_VENDOR_ID_ATI 0x1002

int DRMShutdown(drmDevicePtr devs[], uint32_t count)
{
  drmFreeDevices(devs, count);

  free(devs);

  return DRM_SUCCESS;
}

int DRMDeviceGetDevices(drmDevicePtr *devs[],
			uint32_t *max,
			uint32_t *count)
{
  uint32_t amdgpu_count = 0;
  drmDevicePtr *temp = NULL; /* Will contain all devices, including non-amd ones*/
  drmDevicePtr *p = NULL; /* Helper to copy AMD device data */

  /* First get the total count of devices */
  int dev_count = drmGetDevices(NULL, 0);

  if (dev_count <= 0) {
      printf("No devices\n");
      return DRM_ERROR_NOT_FOUND;
  }

  /* Allocate space to store device data */
  temp = (drmDevicePtr *)calloc(dev_count, sizeof(drmDevicePtr));
  if (!temp) {
      printf("No memory\n");
      return DRM_ERROR_MEMORY;
  }

  /* Allocate space for the devices given back to the user */
  p = *devs = (drmDevicePtr *)calloc(dev_count, sizeof(drmDevicePtr));
  if (!*devs) {
      printf("No memory\n");
      free(temp);
      return DRM_ERROR_MEMORY;
  }

  dev_count = drmGetDevices(temp, dev_count);

  if (dev_count <= 0) {
      printf("Failed to retrieve devices\n");
      free(temp);
      free(*devs);
      *devs = NULL;
      return DRM_ERROR_NOT_FOUND;
  }

  /* Walk through the devices, and keep the AMD GPU ones */
  for (uint32_t i = 0; i < dev_count; i++) {
      if (temp[i]->bustype != DRM_BUS_PCI ||
	  temp[i]->deviceinfo.pci->vendor_id != PCI_VENDOR_ID_ATI)
	continue;

      int fd = -1;

      // Try render node first
      if (1 << DRM_NODE_RENDER & temp[i]->available_nodes) {
	  fd = open(temp[i]->nodes[DRM_NODE_RENDER], O_RDWR);
      }

      if (fd < 0) {
	  // Fallback to primary node
	  if (1 << DRM_NODE_PRIMARY & temp[i]->available_nodes) {
	      fd = open(temp[i]->nodes[DRM_NODE_PRIMARY], O_RDWR);
	  }
      }

      if (fd < 0)
	continue;

      /* Check the version, as it contains the driver name */
      drmVersionPtr ver = drmGetVersion(fd);
      close(fd);

      if (!ver)
	continue;

      if (strcmp(ver->name, "amdgpu")) {
	  drmFreeVersion(ver);
	  continue;
      }

      /* Copy the AMD GPU data */
      memcpy(&p[amdgpu_count++], &temp[i], sizeof(drmDevicePtr));

      /* Done with version */
      drmFreeVersion(ver);
  }

  *max = dev_count;
  *count = amdgpu_count;

  /* Done with all devices (we copied the ones needed */
  free(temp);

  return DRM_SUCCESS;
}

int getAMDDevice(drmDevicePtr dev, amdgpu_device_handle *amd_dev, int *fd)
{
  uint32_t drm_major, drm_minor;

  // Try render node first
  if (1 << DRM_NODE_RENDER & dev->available_nodes) {
      *fd = open(dev->nodes[DRM_NODE_RENDER], O_RDWR);
  }
  if (*fd < 0) {
      // Fallback to primary node
      if (1 << DRM_NODE_PRIMARY & dev->available_nodes) {
	  *fd = open(dev->nodes[DRM_NODE_PRIMARY], O_RDWR);
      }
  }

  if (*fd < 0)
    return DRM_ERROR_INSUFFICIENT_RESOURCES;

  /* Initialize AMD GPU */
  amdgpu_device_initialize(*fd, &drm_major, &drm_minor, amd_dev);

  return DRM_SUCCESS;
}

void releaseAMDDevice(amdgpu_device_handle amd_dev, int fd)
{
  amdgpu_device_deinitialize(amd_dev);

  close(fd);
}

int DRMDeviceGetName(amdgpu_device_handle device, void *out)
{
  char *name = (char *)out;
  pmstrncpy(name, 63, amdgpu_get_marketing_name(device));

  return DRM_SUCCESS;
}

int DRMDeviceGetGPUInfo(amdgpu_device_handle device, void *out)
{
  struct amdgpu_gpu_info *info = (struct amdgpu_gpu_info *)out;
  if (amdgpu_query_gpu_info(device, info) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetMemoryClock(amdgpu_device_handle device, void *out)
{
  uint32_t *value = (uint32_t *) out;
  /* The GPU speed is the memory clock (GFX_MCLK)
   * The GDDRx memory speed is the shader clock (GFX_SCLK)
   */ 
  if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_SCLK, sizeof(*value), value) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetGPUClock(amdgpu_device_handle device, void *out)
{
  uint32_t *value = (uint32_t *) out;
  /* The GPU speed is the memory clock (GFX_MCLK)
   * The GDDRx memory speed is the shader clock (GFX_SCLK)
   */
  if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GFX_MCLK, sizeof(*value), value) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetTemperature(amdgpu_device_handle device, void *out)
{
  uint32_t *value = (uint32_t *) out;
  if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_TEMP, sizeof(*value), value) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetGPULoad(amdgpu_device_handle device, void *out)
{
  uint32_t *value = (uint32_t *) out;
  if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_LOAD, sizeof(*value), value) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetGPUAveragePower(amdgpu_device_handle device, void *out)
{
  uint32_t *value = (uint32_t *) out;
  if (amdgpu_query_sensor_info(device, AMDGPU_INFO_SENSOR_GPU_AVG_POWER, sizeof(*value), value) < 0)
    return DRM_ERROR_NO_DATA;

  return DRM_SUCCESS;
}

int DRMDeviceGetMemoryInfo(amdgpu_device_handle device, void *out)
{
  drmMemory_t *memory = (drmMemory_t *) out;
  struct drm_amdgpu_memory_info mem;

  if (amdgpu_query_info(device, AMDGPU_INFO_MEMORY, sizeof(mem), &mem) < 0)
    return DRM_ERROR_NO_DATA;

  memory->total = mem.vram.total_heap_size;
  memory->usable = mem.vram.usable_heap_size;
  memory->used = mem.vram.heap_usage;

  return DRM_SUCCESS;
}

const char *DRMErrStr(drmReturn_t sts)
{
  int i;
  static const char *unknown = "No such error code";
  static struct {
      int code;
      const char *msg;
  } table[] = {
	{DRM_SUCCESS, "The operation was successful"},
	{DRM_ERROR_NOT_FOUND, "A query to find an object was unsuccessful"},
	{DRM_ERROR_MEMORY, "Not enough memory available"},
	{DRM_ERROR_NO_DATA, "No data available for this request"},
	{DRM_ERROR_INSUFFICIENT_RESOURCES, "Unable to open file, not enough resources"},
	{DRM_ERROR_UNKNOWN, "An internal driver error occurred"}};

  for (i = 0; i < (sizeof(table) / sizeof(table[0])); i++) {
      if (table[i].code == sts)
	return table[i].msg;
  }

  return unknown;
}
