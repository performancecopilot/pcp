/*
 * Thermal monitoring (temperature, fans, thermal pressure) for Darwin PMDA
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

#ifndef THERMAL_H
#define THERMAL_H

/*
 * Per-fan statistics
 */
typedef struct {
    int   instance;      /* Fan index (0-based) */
    float speed;         /* Current RPM */
    float target;        /* Target RPM */
    int   mode;          /* 0=auto, 1=manual */
    float min_rpm;       /* Minimum RPM */
    float max_rpm;       /* Maximum RPM */
} fanstat_t;

/*
 * Thermal statistics structure
 */
typedef struct {
    /* Temperature sensors (°C) */
    float cpu_die;          /* CPU die temperature */
    float cpu_proximity;    /* CPU proximity sensor */
    float gpu_die;          /* GPU die temperature */
    float package;          /* SoC package temperature */
    float ambient;          /* Ambient air temperature */

    /* Bitmap of available temperature sensors */
    int temp_available;     /* Bit flags: 1<<0=cpu_die, 1<<1=cpu_prox, etc */

    /* Fan metrics */
    int nfan;               /* Number of fans */
    fanstat_t *fans;        /* Per-fan statistics (dynamic array) */

    /* Thermal pressure (always available via notify API) */
    int pressure_level;     /* 0-3: Nominal, Fair, Serious, Critical */
    char pressure_state[16]; /* String: "Nominal", "Fair", etc */
} thermalstats_t;

/* Temperature sensor availability bits */
#define TEMP_CPU_DIE        (1 << 0)
#define TEMP_CPU_PROXIMITY  (1 << 1)
#define TEMP_GPU_DIE        (1 << 2)
#define TEMP_PACKAGE        (1 << 3)
#define TEMP_AMBIENT        (1 << 4)

/* Initialize thermal subsystem */
extern int init_thermal(pmdaIndom *fan_indom);

/* Shutdown thermal subsystem */
extern void shutdown_thermal(void);

/* Refresh thermal statistics */
extern int refresh_thermal(thermalstats_t *stats, pmdaIndom *fan_indom);

/* Fetch thermal metrics */
extern int fetch_thermal(unsigned int item, unsigned int inst, pmAtomValue *atom);

#endif /* THERMAL_H */
