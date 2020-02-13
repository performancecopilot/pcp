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
#ifndef _LOCAL_NVML_H
#define _LOCAL_NVML_H

/*
 * NVML interfaces and data structures, based on:
 * http://docs.nvidia.com/deploy/nvml-api/index.html
 */

#define NVML_DEVICE_NAME_BUFFER_SIZE		64
#define NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE	16

typedef void *nvmlDevice_t;	/* used as an opaque handle */
typedef int nvmlPstates_t;	/* performance state (0-15) */

/* Error codes */
typedef enum {
    NVML_SUCCESS			= 0,
    NVML_ERROR_UNINITIALIZED		= 1,
    NVML_ERROR_INVALID_ARGUMENT		= 2,
    NVML_ERROR_NOT_SUPPORTED		= 3,
    NVML_ERROR_NO_PERMISSION		= 4,
    NVML_ERROR_ALREADY_INITIALIZED	= 5,
    NVML_ERROR_NOT_FOUND		= 6,
    NVML_ERROR_INSUFFICIENT_SIZE	= 7,
    NVML_ERROR_INSUFFICIENT_POWER	= 8,
    NVML_ERROR_DRIVER_NOT_LOADED	= 9,
    NVML_ERROR_TIMEOUT			= 10,
    NVML_ERROR_IRQ_ISSUE		= 11,
    NVML_ERROR_LIBRARY_NOT_FOUND	= 12,
    NVML_ERROR_FUNCTION_NOT_FOUND	= 13,
    NVML_ERROR_CORRUPTED_INFOROM	= 14,
    NVML_ERROR_GPU_IS_LOST		= 15,
    NVML_ERROR_RESET_REQUIRED		= 16,
    NVML_ERROR_OPERATING_SYSTEM		= 17,
    NVML_ERROR_LIB_RM_VERSION_MISMATCH	= 18,
    NVML_ERROR_UNKNOWN			= 999
} nvmlReturn_t;

typedef enum {
    NVML_TEMPERATURE_GPU		= 0,
    NVML_TEMPERATURE_MEM,
    NVML_TEMPERATURE_BOARD,
    NVML_TEMPERATURE_VR1,
    NVML_TEMPERATURE_VR2,
    NVML_TEMPERATURE_VR3,
    NVML_TEMPERATURE_VR4,
    NVML_TEMPERATURE_COUNT
} nvmlTemperatureSensors_t;

typedef enum {
    NVML_FEATURE_DISABLED		= 0,
    NVML_FEATURE_ENABLED
} nvmlEnableState_t;

typedef struct {
    char		busId[NVML_DEVICE_PCI_BUS_ID_BUFFER_SIZE];
    unsigned int	domain;
    unsigned int	bus;
    unsigned int	device;
    unsigned int	pciDeviceId;
    unsigned int	pciSubSystemId;
    unsigned int	reserved[4];
} nvmlPciInfo_t;

typedef struct {
    unsigned int	gpu;
    unsigned int	memory;
} nvmlUtilization_t;

typedef struct {
    unsigned long long	total;
    unsigned long long	free;
    unsigned long long	used;
} nvmlMemory_t;

typedef struct {
    unsigned int	pid;
    unsigned long long	usedGpuMemory;
} nvmlProcessInfo_t;

typedef struct {
    unsigned int	gpuUtilization;
    unsigned int	isRunning;
    unsigned long long	maxMemoryUse;
    unsigned int	memoryUtilization;
    unsigned int	reserved[5];
    unsigned long long	startTime;
    unsigned long long	time;
} nvmlAccountingStats_t;

extern int localNvmlInit(void);
extern int localNvmlShutdown(void);
extern const char *localNvmlErrStr(nvmlReturn_t);

extern int localNvmlDeviceGetCount(unsigned int *);
extern int localNvmlDeviceGetHandleByIndex(unsigned int, nvmlDevice_t *);
extern int localNvmlDeviceGetName(nvmlDevice_t, char *, unsigned int);
extern int localNvmlDeviceGetPciInfo(nvmlDevice_t, nvmlPciInfo_t *);
extern int localNvmlDeviceGetFanSpeed(nvmlDevice_t, unsigned int *);
extern int localNvmlDeviceGetTemperature(nvmlDevice_t, nvmlTemperatureSensors_t, unsigned int *);
extern int localNvmlDeviceGetUtilizationRates(nvmlDevice_t, nvmlUtilization_t *);
extern int localNvmlDeviceGetMemoryInfo(nvmlDevice_t, nvmlMemory_t *);
extern int localNvmlDeviceGetPerformanceState(nvmlDevice_t, nvmlPstates_t *);
extern int localNvmlDeviceSetAccountingMode(nvmlDevice_t, nvmlEnableState_t);
extern int localNvmlDeviceSetPersistenceMode(nvmlDevice_t, nvmlEnableState_t);
extern int localNvmlDeviceGetComputeRunningProcesses(nvmlDevice_t, unsigned int *, nvmlProcessInfo_t *);
extern int localNvmlDeviceGetAccountingPids(nvmlDevice_t, unsigned int *, unsigned int *);
extern int localNvmlDeviceGetAccountingStats(nvmlDevice_t, unsigned int, nvmlAccountingStats_t *);

#endif /* _LOCAL_NVML_H */
