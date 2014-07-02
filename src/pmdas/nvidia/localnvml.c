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
#include "pmapi.h"
#include "impl.h"
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif
#include "localnvml.h"

/*
 * Implements NVML interfaces based on:
 * http://docs.nvidia.com/deploy/nvml-api/index.html
 * ... using either a dlopen'd 3rd party or "no values available".
 */

struct {
    const char	*symbol;
    void	*handle;
} nvml_symtab[] = {
    { .symbol = "nvmlInit" },
    { .symbol = "nvmlShutdown" },
    { .symbol = "nvmlDeviceGetCount" },
    { .symbol = "nvmlDeviceGetHandleByIndex" },
    { .symbol = "nvmlDeviceGetName" },
    { .symbol = "nvmlDeviceGetPciInfo" },
    { .symbol = "nvmlDeviceGetFanSpeed" },
    { .symbol = "nvmlDeviceGetTemperature" },
    { .symbol = "nvmlDeviceGetUtilizationRates" },
    { .symbol = "nvmlDeviceGetMemoryInfo" },
    { .symbol = "nvmlDeviceGetPerformanceState" },
};
enum {
    NVML_INIT,
    NVML_SHUTDOWN,
    NVML_DEVICE_GET_COUNT,
    NVML_DEVICE_GET_HANDLEBYINDEX,
    NVML_DEVICE_GET_NAME,
    NVML_DEVICE_GET_PCIINFO,
    NVML_DEVICE_GET_FANSPEED,
    NVML_DEVICE_GET_TEMPERATURE,
    NVML_DEVICE_GET_UTILIZATIONRATES,
    NVML_DEVICE_GET_MEMORYINFO,
    NVML_DEVICE_GET_PERFORMANCESTATE,
    NVML_SYMBOL_COUNT
};
typedef int (*local_init_t)(void);
typedef int (*local_shutdown_t)(void);
typedef int (*local_dev_get_count_t)(unsigned int *);
typedef int (*local_dev_get_handlebyindex_t)(unsigned int, nvmlDevice_t *);
typedef int (*local_dev_get_name_t)(nvmlDevice_t, char *, unsigned int);
typedef int (*local_dev_get_pciinfo_t)(nvmlDevice_t, nvmlPciInfo_t *);
typedef int (*local_dev_get_fanspeed_t)(nvmlDevice_t, unsigned int *);
typedef int (*local_dev_get_temperature_t)(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
typedef int (*local_dev_get_utilizationrates_t)(nvmlDevice_t, nvmlUtilization_t *);
typedef int (*local_dev_get_memoryinfo_t)(nvmlDevice_t, nvmlMemory_t *);
typedef int (*local_dev_get_performancestate_t)(nvmlDevice_t, nvmlPstates_t *);

static int
resolve_symbols(void)
{
    static void *nvml_dso;
    int i;

    if (nvml_dso != NULL)
	return 0;
    if ((nvml_dso = dlopen("libnvidia-ml." DSOSUFFIX, RTLD_NOW)) == NULL)
	return NVML_ERROR_LIBRARY_NOT_FOUND;
    __pmNotifyErr(LOG_INFO, "Successfully loaded NVIDIA NVML library");
    for (i = 0; i < NVML_SYMBOL_COUNT; i++)
	nvml_symtab[i].handle = dlsym(nvml_dso, nvml_symtab[i].symbol);
    return 0;
}

int
localNvmlInit(void)
{
    local_init_t init;
    void *func;
    int sts = resolve_symbols();

    if (sts != 0)
	return sts;
    if ((func = nvml_symtab[NVML_INIT].handle) == NULL)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    init = (local_init_t)func;
    return init();
}

int
localNvmlShutdown(void)
{
    local_shutdown_t shutdown;
    void *func = nvml_symtab[NVML_SHUTDOWN].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    shutdown = (local_shutdown_t)func;
    return shutdown();
}

int
localNvmlDeviceGetCount(unsigned int *count)
{
    local_dev_get_count_t dev_get_count;
    void *func = nvml_symtab[NVML_DEVICE_GET_COUNT].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_count = (local_dev_get_count_t)func;
    return dev_get_count(count);
}

int
localNvmlDeviceGetHandleByIndex(unsigned int index, nvmlDevice_t *device)
{
    local_dev_get_handlebyindex_t dev_get_handlebyindex;
    void *func = nvml_symtab[NVML_DEVICE_GET_HANDLEBYINDEX].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_handlebyindex = (local_dev_get_handlebyindex_t)func;
    return dev_get_handlebyindex(index, device);
}

int
localNvmlDeviceGetName(nvmlDevice_t device, char *name, unsigned int size)
{
    local_dev_get_name_t dev_get_name;
    void *func = nvml_symtab[NVML_DEVICE_GET_NAME].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_name = (local_dev_get_name_t)func;
    return dev_get_name(device, name, size);
}

int
localNvmlDeviceGetPciInfo(nvmlDevice_t device, nvmlPciInfo_t *info)
{
    local_dev_get_pciinfo_t dev_get_pciinfo;
    void *func = nvml_symtab[NVML_DEVICE_GET_PCIINFO].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_pciinfo = (local_dev_get_pciinfo_t)func;
    return dev_get_pciinfo(device, info);
}

int
localNvmlDeviceGetFanSpeed(nvmlDevice_t device, unsigned int *speed)
{
    local_dev_get_fanspeed_t dev_get_fanspeed;
    void *func = nvml_symtab[NVML_DEVICE_GET_FANSPEED].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_fanspeed = (local_dev_get_fanspeed_t)func;
    return dev_get_fanspeed(device, speed);
}

int
localNvmlDeviceGetTemperature(nvmlDevice_t device, nvmlTemperatureSensors_t code, unsigned int *temp)
{
    local_dev_get_temperature_t dev_get_temperature;
    void *func = nvml_symtab[NVML_DEVICE_GET_TEMPERATURE].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_temperature = (local_dev_get_temperature_t)func;
    return dev_get_temperature(device, code, temp);
}

int
localNvmlDeviceGetUtilizationRates(nvmlDevice_t device, nvmlUtilization_t *util)
{
    local_dev_get_utilizationrates_t dev_get_utilizationrates;
    void *func = nvml_symtab[NVML_DEVICE_GET_UTILIZATIONRATES].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_utilizationrates = (local_dev_get_utilizationrates_t)func;
    return dev_get_utilizationrates(device, util);
}

int
localNvmlDeviceGetMemoryInfo(nvmlDevice_t device, nvmlMemory_t *memory)
{
    local_dev_get_memoryinfo_t dev_get_memoryinfo;
    void *func = nvml_symtab[NVML_DEVICE_GET_MEMORYINFO].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_memoryinfo = (local_dev_get_memoryinfo_t)func;
    return dev_get_memoryinfo(device, memory);
}

int
localNvmlDeviceGetPerformanceState(nvmlDevice_t device, nvmlPstates_t *state)
{
    local_dev_get_performancestate_t dev_get_performancestate;
    void *func = nvml_symtab[NVML_DEVICE_GET_PERFORMANCESTATE].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_performancestate = (local_dev_get_performancestate_t)func;
    return dev_get_performancestate(device, state);
}

const char *
localNvmlErrStr(nvmlReturn_t sts)
{
    int i;
    static const char *unknown = "No such error code";
    static struct {
	int		code;
	const char	*msg;
    } table[] = { {
	NVML_SUCCESS,
"The operation was successful" }, {
	NVML_ERROR_UNINITIALIZED,
"NVML was not first initialized with nvmlInit()" }, {
	NVML_ERROR_INVALID_ARGUMENT,
"A supplied argument is invalid" }, {
	NVML_ERROR_NOT_SUPPORTED,
"The requested operation is not available on target device" }, {
	NVML_ERROR_NO_PERMISSION,
"The current user does not have permission for operation" }, {
	NVML_ERROR_ALREADY_INITIALIZED,
"Deprecated error code (5)" }, {
	NVML_ERROR_NOT_FOUND,
"A query to find an object was unsuccessful" }, {
	NVML_ERROR_INSUFFICIENT_SIZE,
"An input argument is not large enough" }, {
	NVML_ERROR_INSUFFICIENT_POWER,
"A device's external power cables are not properly attached" }, {
	NVML_ERROR_DRIVER_NOT_LOADED,
"NVIDIA driver is not loaded" }, {
	NVML_ERROR_TIMEOUT,
"User provided timeout passed" }, {
	NVML_ERROR_IRQ_ISSUE,
"NVIDIA Kernel detected an interrupt issue with a GPU" }, {
	NVML_ERROR_LIBRARY_NOT_FOUND,
"NVML Shared Library couldn't be found or loaded" }, {
	NVML_ERROR_FUNCTION_NOT_FOUND,
"Local version of NVML doesn't implement this function" }, {
	NVML_ERROR_CORRUPTED_INFOROM,
"infoROM is corrupted" }, {
	NVML_ERROR_GPU_IS_LOST,
"The GPU has fallen off the bus or has otherwise become inaccessible" }, {
	NVML_ERROR_UNKNOWN,
"An internal driver error occurred"
    } };

    for (i = 0; i < (sizeof(table)/sizeof(table[0])); i++) {
	if (table[i].code == sts)
	    return table[i].msg;
    }
    return unknown;
}
