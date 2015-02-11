/*
 * Copyright (c) 2014 Red Hat.
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
#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include "localnvml.h"

/*
 * Testing library for exercising the NVIDIA GPU PMDA.  By injecting this
 * library into the PMDA to supply values that the real nvidia-ml usually
 * would, we are able to obtain synthesized statistics and exercise many
 * of the code paths through pmda_nvidia.  No substitute for using actual
 * hardware, of course - but better than a poke in the eye with a sharp
 * stick (or worse, no testing at all).
 */

/*
 * Table of the GPU hardware we'll be faking.
 * Using simple static values here so that tests are deterministic.
 */
#define NUM_GPUS (sizeof(gpu_table)/sizeof(gpu_table[0]))
struct gputab {
    char	name[NVML_DEVICE_NAME_BUFFER_SIZE];
    nvmlPciInfo_t	pciinfo;
    unsigned int	fanspeed;
    unsigned int	temperature;
    nvmlUtilization_t	util;
    nvmlPstates_t	state;
    nvmlMemory_t	mem;
} gpu_table[] = {
    {
	.name = "GeForce 100M Series",
	.pciinfo = {
	    .busId = "0:1:0x2:3:4",
	    .domain = 0,
	    .bus = 1,
	    .device = 0x2,
	    .pciDeviceId = 3,
	    .pciSubSystemId = 4,
	},
	.fanspeed = 5,
	.temperature = 6,
	.util = {
	    .gpu = 7,
	    .memory = 8,
	},
	.state = 9,
	.mem = {
	    .total = 256 * 1024 * 1024,
	    .free = 156 * 1024 * 1024,
	    .used = 100 * 1024 * 1024,
	},
    },
    {
	.name = "Quadro FX 200M Series",
	.pciinfo = {
	    .busId = "20:21:0x2:23:24",
	    .domain = 20,
	    .bus = 21,
	    .device = 0x22,
	    .pciDeviceId = 23,
	    .pciSubSystemId = 24,
	},
	.fanspeed = 25,
	.temperature = 26,
	.util = {
	    .gpu = 27,
	    .memory = 28,
	},
	.state = 29,
	.mem = {
	    .total = 8ULL * 1024 * 1024 * 1024,
	    .free = 2ULL * 1024 * 1024 * 1024,
	    .used = 6ULL * 1024 * 1024 * 1024,
	},
    }
};

static int refcount;

int
nvmlInit(void)
{
    refcount++;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlInit [%d - %d]\n",
		refcount - 1, refcount);
    return 0;
}

int
nvmlShutdown(void)
{
    refcount--;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlShutdown [%d - %d]\n",
		refcount + 1, refcount);
    return NVML_SUCCESS;
}

int
nvmlDeviceGetCount(unsigned int *count)
{
    *count = sizeof(gpu_table) / sizeof(gpu_table[0]);
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetCount [%u]\n", *count);
    return NVML_SUCCESS;
}

#define CHECK_INDEX(index) { \
    if ((index) >= NUM_GPUS) return NVML_ERROR_GPU_IS_LOST; }

#define CHECK_DEVICE(devp) { \
    if ((devp) < gpu_table) return NVML_ERROR_INVALID_ARGUMENT; \
    if ((devp) >= gpu_table + NUM_GPUS) return NVML_ERROR_GPU_IS_LOST; }

int
nvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *dp)
{
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetHandleByIndex %u\n", index);
    CHECK_INDEX(index);
    *dp = &gpu_table[index];
    return NVML_SUCCESS;
}

int
nvmlDeviceGetName(nvmlDevice_t device, char *buffer, unsigned int length)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetName\n");
    CHECK_DEVICE(dev);
    strncpy(buffer, dev->name, length);
    buffer[length-1] = '\0';
    return NVML_SUCCESS;
}

int
nvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t *info)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetPciInfo\n");
    CHECK_DEVICE(dev);
    *info = dev->pciinfo;
    return NVML_SUCCESS;
}

int
nvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *speed)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetFanSpeed\n");
    CHECK_DEVICE(dev);
    *speed = dev->fanspeed;
    return NVML_SUCCESS;
}

int
nvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t sensor, unsigned int *value)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetTemperature\n");
    CHECK_DEVICE(dev);
    if (sensor >= NVML_TEMPERATURE_COUNT)
	return NVML_ERROR_INVALID_ARGUMENT;
    *value = dev->temperature;
    return NVML_SUCCESS;
}

int
nvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t *util)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetUtilizationRates\n");
    CHECK_DEVICE(dev);
    *util = dev->util;
    return NVML_SUCCESS;
}

int
nvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *mem)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetMemoryInfo\n");
    CHECK_DEVICE(dev);
    *mem = dev->mem;
    return NVML_SUCCESS;
}

int
nvmlDeviceGetPerformanceState(nvmlDevice_t device, nvmlPstates_t *state)
{
    struct gputab *dev = (struct gputab *)device;
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "qa-nvidia-ml: nvmlDeviceGetPerformanceState\n");
    CHECK_DEVICE(dev);
    *state = dev->state;
    return NVML_SUCCESS;
}
