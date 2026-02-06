/*
 * SMC (System Management Controller) access for Darwin PMDA
 *
 * Provides IOKit-based access to AppleSMC for reading temperature and fan data.
 *
 * Copyright (c) 2026 Paul Smith.
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

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <string.h>
#include <sys/sysctl.h>

#include "pmapi.h"
#include "pmda.h"

#include "smc.h"

/*
 * SMC IOKit constants (reverse-engineered, not official Apple APIs)
 */
#define IOKIT_SMC_SERVICE_NAME      "AppleSMC"

/* SMC command selectors */
#define SMC_CMD_READ_KEY            5
#define SMC_CMD_READ_KEYINFO        9

/* SMC kernel index */
#define SMC_KERNEL_INDEX            2

/*
 * SMC IOKit structures
 */
typedef struct {
    uint32_t key;           /* FourCC key code */
    uint32_t vers;          /* Version info */
    uint16_t pLimitData;    /* Platform limit data */
    uint16_t keyInfo;       /* Key info flags */
    uint8_t  padding[16];   /* Padding to 32 bytes */
} smc_key_info_t;

typedef struct {
    uint8_t  command;       /* SMC command */
    uint32_t key;           /* FourCC key code */
    smc_key_info_t key_info;    /* Key information */
    uint8_t  result;        /* Result code */
    uint8_t  status;        /* Status code */
    uint8_t  data8;         /* 8-bit data */
    uint32_t data32;        /* 32-bit data */
    uint8_t  data[SMC_MAX_DATA_SIZE];  /* Data buffer */
} smc_param_t;

/*
 * Module state
 */
static io_connect_t smc_connection = 0;
static bool smc_available = false;
static bool smc_init_attempted = false;

/*
 * Platform detection
 */
static bool
is_apple_silicon(void)
{
    int ret = 0;
    size_t size = sizeof(ret);

    /* Check for ARM64 architecture */
    if (sysctlbyname("hw.optional.arm64", &ret, &size, NULL, 0) == 0) {
        return (ret == 1);
    }

    return false;
}

/*
 * Convert 4-character key string to uint32_t (FourCC)
 */
static uint32_t
key_to_fourcc(const char *key)
{
    if (!key || strlen(key) != 4)
        return 0;

    return ((uint32_t)key[0] << 24) |
           ((uint32_t)key[1] << 16) |
           ((uint32_t)key[2] << 8) |
           ((uint32_t)key[3]);
}

/*
 * Initialize SMC connection
 */
int
smc_init(void)
{
    kern_return_t kr;
    io_service_t service;

    /* Only attempt initialization once */
    if (smc_init_attempted)
        return smc_available ? 0 : PM_ERR_AGAIN;

    smc_init_attempted = true;

    pmNotifyErr(LOG_DEBUG, "smc_init: initializing SMC connection");

    /* Get AppleSMC service */
    service = IOServiceGetMatchingService(
        kIOMainPortDefault,
        IOServiceMatching(IOKIT_SMC_SERVICE_NAME)
    );

    if (!service) {
        pmNotifyErr(LOG_INFO, "smc_init: AppleSMC service not found (may not be available on this system)");
        return PM_ERR_AGAIN;
    }

    /* Open connection to SMC */
    kr = IOServiceOpen(service, mach_task_self(), 0, &smc_connection);
    IOObjectRelease(service);

    if (kr != KERN_SUCCESS) {
        pmNotifyErr(LOG_INFO, "smc_init: failed to open SMC connection (kr=%d, may require entitlements)", kr);
        smc_connection = 0;
        return PM_ERR_AGAIN;
    }

    smc_available = true;
    pmNotifyErr(LOG_INFO, "smc_init: SMC connection established (platform: %s)",
                is_apple_silicon() ? "Apple Silicon" : "Intel");

    return 0;
}

/*
 * Shutdown SMC connection
 */
void
smc_shutdown(void)
{
    if (smc_connection) {
        IOServiceClose(smc_connection);
        smc_connection = 0;
    }
    smc_available = false;
    smc_init_attempted = false;

    pmNotifyErr(LOG_DEBUG, "smc_shutdown: SMC connection closed");
}

/*
 * Check if SMC is available
 */
bool
smc_is_available(void)
{
    return smc_available;
}

/*
 * Low-level SMC read operation
 */
static kern_return_t
smc_call_kernel(int selector, smc_param_t *input, smc_param_t *output)
{
    size_t input_size = sizeof(smc_param_t);
    size_t output_size = sizeof(smc_param_t);

    return IOConnectCallStructMethod(
        smc_connection,
        selector,
        input,
        input_size,
        output,
        &output_size
    );
}

/*
 * Read raw SMC key data
 */
int
smc_read_key(const char *key, smc_key_data_t *data)
{
    smc_param_t input;
    smc_param_t output;
    kern_return_t kr;
    uint32_t key_code;

    if (!smc_available)
        return PM_ERR_AGAIN;

    if (!key || !data || strlen(key) != 4)
        return PM_ERR_CONV;

    key_code = key_to_fourcc(key);
    if (key_code == 0)
        return PM_ERR_CONV;

    /* First, get key info */
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    input.command = SMC_CMD_READ_KEYINFO;
    input.key = key_code;
    input.key_info.key = key_code;

    kr = smc_call_kernel(SMC_KERNEL_INDEX, &input, &output);
    if (kr != KERN_SUCCESS) {
        pmNotifyErr(LOG_DEBUG, "smc_read_key: failed to get key info for '%s' (kr=%d)", key, kr);
        return PM_ERR_AGAIN;
    }

    /* Now read the actual data */
    memset(&input, 0, sizeof(input));
    memset(&output, 0, sizeof(output));

    input.command = SMC_CMD_READ_KEY;
    input.key = key_code;
    input.key_info.key = key_code;
    input.key_info.keyInfo = output.key_info.keyInfo;

    kr = smc_call_kernel(SMC_KERNEL_INDEX, &input, &output);
    if (kr != KERN_SUCCESS) {
        pmNotifyErr(LOG_DEBUG, "smc_read_key: failed to read key '%s' (kr=%d)", key, kr);
        return PM_ERR_AGAIN;
    }

    /* Copy results to output structure */
    strncpy(data->key, key, 4);
    data->key[4] = '\0';
    data->data_type = output.key_info.keyInfo;
    data->data_size = (output.key_info.keyInfo >> 16) & 0xFF;

    if (data->data_size > SMC_MAX_DATA_SIZE)
        data->data_size = SMC_MAX_DATA_SIZE;

    memcpy(data->data, output.data, data->data_size);

    return 0;
}

/*
 * Read temperature sensor in Celsius
 */
int
smc_read_temperature(const char *key, float *celsius)
{
    smc_key_data_t data;
    int ret;
    uint16_t raw_value;

    if (!celsius)
        return PM_ERR_CONV;

    ret = smc_read_key(key, &data);
    if (ret != 0)
        return ret;

    /* Temperature is typically 2 bytes in sp78 format */
    if (data.data_size < 2) {
        pmNotifyErr(LOG_DEBUG, "smc_read_temperature: unexpected data size %d for key '%s'",
                    data.data_size, key);
        return PM_ERR_CONV;
    }

    /* Big-endian to host byte order */
    raw_value = ((uint16_t)data.data[0] << 8) | data.data[1];

    /* Convert sp78 to Celsius */
    *celsius = smc_sp78_to_celsius(raw_value);

    pmNotifyErr(LOG_DEBUG, "smc_read_temperature: key='%s' raw=0x%04x temp=%.2f°C",
                key, raw_value, *celsius);

    return 0;
}

/*
 * Read fan RPM
 */
int
smc_read_fan_rpm(const char *key, float *rpm)
{
    smc_key_data_t data;
    int ret;
    uint16_t raw_value;

    if (!rpm)
        return PM_ERR_CONV;

    ret = smc_read_key(key, &data);
    if (ret != 0)
        return ret;

    /* Fan speed is typically 2 bytes in fpe2 format */
    if (data.data_size < 2) {
        pmNotifyErr(LOG_DEBUG, "smc_read_fan_rpm: unexpected data size %d for key '%s'",
                    data.data_size, key);
        return PM_ERR_CONV;
    }

    /* Big-endian to host byte order */
    raw_value = ((uint16_t)data.data[0] << 8) | data.data[1];

    /* Convert fpe2 to RPM */
    *rpm = smc_fpe2_to_rpm(raw_value);

    pmNotifyErr(LOG_DEBUG, "smc_read_fan_rpm: key='%s' raw=0x%04x rpm=%.2f",
                key, raw_value, *rpm);

    return 0;
}

/*
 * Get fan count
 */
int
smc_get_fan_count(int *count)
{
    smc_key_data_t data;
    int ret;

    if (!count)
        return PM_ERR_CONV;

    ret = smc_read_key(SMC_KEY_FAN_COUNT, &data);
    if (ret != 0) {
        *count = 0;
        return ret;
    }

    /* Fan count is a single uint8 */
    if (data.data_size < 1) {
        pmNotifyErr(LOG_DEBUG, "smc_get_fan_count: unexpected data size %d", data.data_size);
        *count = 0;
        return PM_ERR_CONV;
    }

    *count = data.data[0];

    pmNotifyErr(LOG_DEBUG, "smc_get_fan_count: count=%d", *count);

    return 0;
}
