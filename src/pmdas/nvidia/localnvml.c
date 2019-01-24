/*
 * Copyright (c) 2014,2019 Red Hat.
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
    { .symbol = "nvmlDeviceSetAccountingMode" },
    { .symbol = "nvmlDeviceSetPersistenceMode" },
    { .symbol = "nvmlDeviceGetComputeRunningProcesses" },
    { .symbol = "nvmlDeviceGetAccountingPids" },
    { .symbol = "nvmlDeviceGetAccountingStats" },
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
    NVML_DEVICE_SET_ACCOUNTINGMODE,
    NVML_DEVICE_SET_PERSISTENCEMODE,
    NVML_DEVICE_GET_COMPUTERUNNINGPROCESSES,
    NVML_DEVICE_GET_ACCOUNTINGPIDS,
    NVML_DEVICE_GET_ACCOUNTINGSTATS,
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
typedef int (*local_dev_set_accountingmode_t)(nvmlDevice_t, nvmlEnableState_t);
typedef int (*local_dev_set_persistencemode_t)(nvmlDevice_t, nvmlEnableState_t);
typedef int (*local_dev_get_computerunningprocesses_t)(nvmlDevice_t, unsigned int *, nvmlProcessInfo_t *);
typedef int (*local_dev_get_accountingpids_t)(nvmlDevice_t, unsigned int *, unsigned int *);
typedef int (*local_dev_get_accountingstats_t)(nvmlDevice_t, unsigned int, nvmlAccountingStats_t *);

static int
resolve_symbols(void)
{
    static void *nvml_dso;
    int i;

    if (nvml_dso != NULL)
	return 0;
    if ((nvml_dso = dlopen("libnvidia-ml." DSOSUFFIX, RTLD_NOW)) == NULL)
	return NVML_ERROR_LIBRARY_NOT_FOUND;
    pmNotifyErr(LOG_INFO, "Successfully loaded NVIDIA NVML library");
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

int
localNvmlDeviceSetAccountingMode(nvmlDevice_t device, nvmlEnableState_t state)
{
    local_dev_set_accountingmode_t dev_set_accountingmode;
    void *func = nvml_symtab[NVML_DEVICE_SET_ACCOUNTINGMODE].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_set_accountingmode = (local_dev_set_accountingmode_t)func;
    return dev_set_accountingmode(device, state);
}

int
localNvmlDeviceSetPersistenceMode(nvmlDevice_t device, nvmlEnableState_t state)
{
    local_dev_set_persistencemode_t dev_set_persistencemode;
    void *func = nvml_symtab[NVML_DEVICE_SET_PERSISTENCEMODE].handle;

    if (!func)
        return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_set_persistencemode = (local_dev_set_persistencemode_t)func;
    return dev_set_persistencemode(device, state);
}

int
localNvmlDeviceGetComputeRunningProcesses(nvmlDevice_t device, unsigned int *count, nvmlProcessInfo_t *infos)
{
    local_dev_get_computerunningprocesses_t dev_get_computerunningprocesses;
    void *func = nvml_symtab[NVML_DEVICE_GET_COMPUTERUNNINGPROCESSES].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_computerunningprocesses = (local_dev_get_computerunningprocesses_t)func;
    return dev_get_computerunningprocesses(device, count, infos);
}

int
localNvmlDeviceGetAccountingPids(nvmlDevice_t device, unsigned int *count, unsigned int *pids)
{
    local_dev_get_accountingpids_t dev_get_accountingpids;
    void *func = nvml_symtab[NVML_DEVICE_GET_ACCOUNTINGPIDS].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_accountingpids = (local_dev_get_accountingpids_t)func;
    return dev_get_accountingpids(device, count, pids);
}

int
localNvmlDeviceGetAccountingStats(nvmlDevice_t device, unsigned int pid, nvmlAccountingStats_t *stats)
{
    local_dev_get_accountingstats_t dev_get_accountingstats;
    void *func = nvml_symtab[NVML_DEVICE_GET_ACCOUNTINGSTATS].handle;

    if (!func)
	return NVML_ERROR_FUNCTION_NOT_FOUND;
    dev_get_accountingstats = (local_dev_get_accountingstats_t)func;
    return dev_get_accountingstats(device, pid, stats);
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
	NVML_ERROR_RESET_REQUIRED,
"The GPU requires a reset before it can be used again." }, {
	NVML_ERROR_OPERATING_SYSTEM,
"The GPU control device has been blocked by the operating system/cgroups." }, {
	NVML_ERROR_LIB_RM_VERSION_MISMATCH,
"RM detects a driver/library version mismatch." }, {
	NVML_ERROR_UNKNOWN,
"An internal driver error occurred"
    } };

    for (i = 0; i < (sizeof(table)/sizeof(table[0])); i++) {
	if (table[i].code == sts)
	    return table[i].msg;
    }
    return unknown;
}
