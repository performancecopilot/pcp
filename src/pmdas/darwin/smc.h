/*
 * SMC (System Management Controller) access for Darwin PMDA
 *
 * Provides low-level access to AppleSMC for reading temperature sensors
 * and fan metrics on Apple Silicon and Intel Macs.
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
 *
 * REFERENCES:
 * - iSMC (https://github.com/dkorunic/iSMC) - CLI tool with Apple Silicon support
 * - SMCKit (https://github.com/beltex/SMCKit) - Comprehensive key reference
 * - Community reverse-engineering docs (not Apple-supported APIs)
 *
 * NOTE: SMC access is reverse-engineered and may require entitlements on
 * newer macOS versions. The code degrades gracefully if access is denied.
 */

#ifndef DARWIN_PMDA_SMC_H
#define DARWIN_PMDA_SMC_H

#include <stdbool.h>
#include <stdint.h>

/*
 * SMC Key definitions
 * Keys are 4-character codes that identify specific sensors
 */

/* Temperature keys (°C) */
#define SMC_KEY_CPU_DIE_TEMP        "Tp01"  /* CPU die temp (Apple Silicon) */
#define SMC_KEY_CPU_PROXIMITY_TEMP  "TA0P"  /* CPU proximity sensor */
#define SMC_KEY_GPU_DIE_TEMP        "Tg01"  /* GPU die temp (Apple Silicon) */
#define SMC_KEY_PACKAGE_TEMP        "TCXC"  /* SoC package temperature */
#define SMC_KEY_AMBIENT_TEMP        "TA0P"  /* Ambient air temperature */

/* Fan keys */
#define SMC_KEY_FAN_COUNT           "FNum"  /* Number of fans (uint8) */
#define SMC_KEY_FAN_SPEED_FMT       "F%dAc" /* Current RPM (fpe2 format) */
#define SMC_KEY_FAN_TARGET_FMT      "F%dTg" /* Target RPM (fpe2 format) */
#define SMC_KEY_FAN_MODE_FMT        "F%dMd" /* Fan mode: 0=auto, 1=manual */
#define SMC_KEY_FAN_MIN_FMT         "F%dMn" /* Minimum RPM (fpe2 format) */
#define SMC_KEY_FAN_MAX_FMT         "F%dMx" /* Maximum RPM (fpe2 format) */

/*
 * SMC data types (FourCC format)
 */
#define SMC_TYPE_SP78   0x73703738  /* 'sp78' - signed fixed-point (÷256) */
#define SMC_TYPE_FPE2   0x66706532  /* 'fpe2' - unsigned fixed-point (÷4) */
#define SMC_TYPE_UI8    0x75693820  /* 'ui8 ' - unsigned 8-bit integer */
#define SMC_TYPE_FLAG   0x666c6167  /* 'flag' - boolean flag */

/* Maximum data size returned by SMC */
#define SMC_MAX_DATA_SIZE 32

/*
 * SMC data structures
 */
typedef struct {
    char key[5];              /* 4-char key + null terminator */
    uint32_t data_type;       /* FourCC data type */
    uint8_t data_size;        /* Size of data in bytes */
    uint8_t data[SMC_MAX_DATA_SIZE];  /* Raw data bytes */
} smc_key_data_t;

/*
 * Public API
 */

/* Initialize SMC connection (IOServiceOpen to AppleSMC) */
int smc_init(void);

/* Shutdown SMC connection (IOServiceClose) */
void smc_shutdown(void);

/* Check if SMC access is available (connection succeeded) */
bool smc_is_available(void);

/* Read temperature sensor in Celsius (handles sp78 format conversion) */
int smc_read_temperature(const char *key, float *celsius);

/* Read fan RPM (handles fpe2 format conversion) */
int smc_read_fan_rpm(const char *key, float *rpm);

/* Get fan count from SMC */
int smc_get_fan_count(int *count);

/* Low-level: read raw SMC key data */
int smc_read_key(const char *key, smc_key_data_t *data);

/*
 * Format conversion utilities (exposed for testing)
 */

/* Convert SMC sp78 format to Celsius (signed fixed-point, ÷256) */
static inline float
smc_sp78_to_celsius(uint16_t raw)
{
    int16_t signed_raw = (int16_t)raw;
    return (float)signed_raw / 256.0f;
}

/* Convert SMC fpe2 format to RPM (unsigned fixed-point, ÷4) */
static inline float
smc_fpe2_to_rpm(uint16_t raw)
{
    return (float)raw / 4.0f;
}

#endif /* DARWIN_PMDA_SMC_H */
