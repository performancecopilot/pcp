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

#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pmapi.h"
#include "pmda.h"

#include "darwin.h"
#include "thermal.h"
#include "smc.h"

/*
 * Module state
 */
static thermalstats_t thermal_stats;
static int thermal_pressure_token = -1;
static bool thermal_initialized = false;

/*
 * Temperature sensor key mappings
 * Keys may vary by Mac model, these are common across Apple Silicon/Intel
 */
static const char *temp_sensor_keys[] = {
    SMC_KEY_CPU_DIE_TEMP,       /* TEMP_CPU_DIE */
    SMC_KEY_CPU_PROXIMITY_TEMP, /* TEMP_CPU_PROXIMITY */
    SMC_KEY_GPU_DIE_TEMP,       /* TEMP_GPU_DIE */
    SMC_KEY_PACKAGE_TEMP,       /* TEMP_PACKAGE */
    SMC_KEY_AMBIENT_TEMP,       /* TEMP_AMBIENT */
};

/*
 * Initialize thermal subsystem
 */
int
init_thermal(pmdaIndom *fan_indom)
{
    if (thermal_initialized)
        return 0;

    pmNotifyErr(LOG_INFO, "init_thermal: initializing thermal monitoring");

    /* Initialize stats structure */
    memset(&thermal_stats, 0, sizeof(thermal_stats));
    thermal_stats.nfan = 0;
    thermal_stats.fans = NULL;

    /* Initialize SMC */
    smc_init();

    /* Register for thermal pressure notifications (always available) */
    int ret = notify_register_check("com.apple.system.thermalpressurelevel",
                                     &thermal_pressure_token);
    if (ret != NOTIFY_STATUS_OK) {
        pmNotifyErr(LOG_WARNING, "init_thermal: failed to register for thermal pressure notifications (ret=%d)", ret);
        thermal_pressure_token = -1;
    } else {
        pmNotifyErr(LOG_DEBUG, "init_thermal: registered for thermal pressure notifications");
    }

    thermal_initialized = true;
    return 0;
}

/*
 * Shutdown thermal subsystem
 */
void
shutdown_thermal(void)
{
    if (!thermal_initialized)
        return;

    pmNotifyErr(LOG_DEBUG, "shutdown_thermal: shutting down thermal monitoring");

    /* Free fan array */
    if (thermal_stats.fans) {
        free(thermal_stats.fans);
        thermal_stats.fans = NULL;
    }
    thermal_stats.nfan = 0;

    /* Unregister thermal pressure notifications */
    if (thermal_pressure_token != -1) {
        notify_cancel(thermal_pressure_token);
        thermal_pressure_token = -1;
    }

    /* Shutdown SMC */
    smc_shutdown();

    thermal_initialized = false;
}

/*
 * Update fan instance domain
 */
static void
update_fan_indom(pmdaIndom *fan_indom, int nfan)
{
    int i;

    if (!fan_indom)
        return;

    /* If fan count changed, reallocate instance array */
    if (nfan > 0 && nfan != fan_indom->it_numinst) {
        i = sizeof(pmdaInstid) * nfan;
        if ((fan_indom->it_set = realloc(fan_indom->it_set, i)) == NULL) {
            fan_indom->it_numinst = 0;
            pmNotifyErr(LOG_ERR, "update_fan_indom: failed to reallocate fan instances");
            return;
        }

        /* Create instances: fan0, fan1, fan2, ... */
        for (i = 0; i < nfan; i++) {
            char *name = malloc(16);
            if (name)
                snprintf(name, 16, "fan%d", i);
            fan_indom->it_set[i].i_name = name;
            fan_indom->it_set[i].i_inst = i;
        }
    } else if (nfan == 0 && fan_indom->it_numinst > 0) {
        /* Free all instances if fan count dropped to 0 */
        for (i = 0; i < fan_indom->it_numinst; i++) {
            if (fan_indom->it_set[i].i_name)
                free(fan_indom->it_set[i].i_name);
        }
        free(fan_indom->it_set);
        fan_indom->it_set = NULL;
    }

    fan_indom->it_numinst = nfan;

    pmNotifyErr(LOG_DEBUG, "update_fan_indom: updated fan indom with %d fans", nfan);
}

/*
 * Refresh temperature sensors
 */
static void
refresh_temperatures(thermalstats_t *stats)
{
    int i;
    float *temp_fields[] = {
        &stats->cpu_die,
        &stats->cpu_proximity,
        &stats->gpu_die,
        &stats->package,
        &stats->ambient,
    };

    stats->temp_available = 0;

    /* Try to read each temperature sensor */
    for (i = 0; i < sizeof(temp_fields) / sizeof(temp_fields[0]); i++) {
        if (smc_read_temperature(temp_sensor_keys[i], temp_fields[i]) == 0) {
            stats->temp_available |= (1 << i);
        } else {
            *temp_fields[i] = 0.0f;
        }
    }
}

/*
 * Refresh fan metrics
 */
static void
refresh_fans(thermalstats_t *stats, pmdaIndom *fan_indom)
{
    int nfan = 0;
    int i;
    char key[8];

    /* Get fan count from SMC */
    if (smc_get_fan_count(&nfan) != 0 || nfan <= 0) {
        stats->nfan = 0;
        if (stats->fans) {
            free(stats->fans);
            stats->fans = NULL;
        }
        update_fan_indom(fan_indom, 0);
        return;
    }

    /* Reallocate fan array if size changed */
    if (nfan != stats->nfan) {
        if (stats->fans)
            free(stats->fans);
        stats->fans = calloc(nfan, sizeof(fanstat_t));
        if (!stats->fans) {
            pmNotifyErr(LOG_ERR, "refresh_fans: failed to allocate fan array");
            stats->nfan = 0;
            update_fan_indom(fan_indom, 0);
            return;
        }
        stats->nfan = nfan;
        update_fan_indom(fan_indom, nfan);
    }

    /* Read per-fan metrics */
    for (i = 0; i < nfan; i++) {
        stats->fans[i].instance = i;

        /* Fan speed (F0Ac, F1Ac, etc.) */
        snprintf(key, sizeof(key), SMC_KEY_FAN_SPEED_FMT, i);
        if (smc_read_fan_rpm(key, &stats->fans[i].speed) != 0)
            stats->fans[i].speed = 0.0f;

        /* Fan target (F0Tg, F1Tg, etc.) */
        snprintf(key, sizeof(key), SMC_KEY_FAN_TARGET_FMT, i);
        if (smc_read_fan_rpm(key, &stats->fans[i].target) != 0)
            stats->fans[i].target = 0.0f;

        /* Fan min RPM (F0Mn, F1Mn, etc.) */
        snprintf(key, sizeof(key), SMC_KEY_FAN_MIN_FMT, i);
        if (smc_read_fan_rpm(key, &stats->fans[i].min_rpm) != 0)
            stats->fans[i].min_rpm = 0.0f;

        /* Fan max RPM (F0Mx, F1Mx, etc.) */
        snprintf(key, sizeof(key), SMC_KEY_FAN_MAX_FMT, i);
        if (smc_read_fan_rpm(key, &stats->fans[i].max_rpm) != 0)
            stats->fans[i].max_rpm = 0.0f;

        /* Fan mode - TODO: read F0Md key (for now assume auto) */
        stats->fans[i].mode = 0;  /* 0 = auto */
    }
}

/*
 * Refresh thermal pressure
 */
static void
refresh_thermal_pressure(thermalstats_t *stats)
{
    uint64_t state = 0;

    if (thermal_pressure_token == -1) {
        stats->pressure_level = 0;
        strcpy(stats->pressure_state, "Unknown");
        return;
    }

    /* Get thermal pressure state */
    int ret = notify_get_state(thermal_pressure_token, &state);
    if (ret != NOTIFY_STATUS_OK) {
        pmNotifyErr(LOG_DEBUG, "refresh_thermal_pressure: failed to get state (ret=%d)", ret);
        stats->pressure_level = 0;
        strcpy(stats->pressure_state, "Unknown");
        return;
    }

    stats->pressure_level = (int)state;

    /* Map level to state string */
    switch (state) {
        case 0:
            strcpy(stats->pressure_state, "Nominal");
            break;
        case 1:
            strcpy(stats->pressure_state, "Fair");
            break;
        case 2:
            strcpy(stats->pressure_state, "Serious");
            break;
        case 3:
            strcpy(stats->pressure_state, "Critical");
            break;
        default:
            strcpy(stats->pressure_state, "Unknown");
            break;
    }

    pmNotifyErr(LOG_DEBUG, "refresh_thermal_pressure: level=%d state=%s",
                stats->pressure_level, stats->pressure_state);
}

/*
 * Refresh thermal statistics
 */
int
refresh_thermal(thermalstats_t *stats, pmdaIndom *fan_indom)
{
    if (!thermal_initialized) {
        pmNotifyErr(LOG_DEBUG, "refresh_thermal: thermal not initialized");
        return PM_ERR_AGAIN;
    }

    if (!stats)
        return PM_ERR_CONV;

    pmNotifyErr(LOG_DEBUG, "refresh_thermal: refreshing thermal stats (SMC available: %s)",
                smc_is_available() ? "yes" : "no");

    /* Refresh temperature sensors (if SMC available) */
    if (smc_is_available()) {
        refresh_temperatures(stats);
        refresh_fans(stats, fan_indom);
    } else {
        stats->temp_available = 0;
        stats->nfan = 0;
    }

    /* Refresh thermal pressure (always available) */
    refresh_thermal_pressure(stats);

    /* Copy to global stats */
    memcpy(&thermal_stats, stats, sizeof(thermalstats_t));

    return 0;
}

/*
 * Fetch thermal metrics
 */
int
fetch_thermal(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    if (!thermal_initialized)
        return PM_ERR_AGAIN;

    /* Temperature metrics (no instance) */
    if (item >= 0 && item <= 4) {
        if ((thermal_stats.temp_available & (1 << item)) == 0)
            return PM_ERR_APPVERSION;  /* Sensor not available */

        switch (item) {
            case 0:  /* thermal.cpu.die */
                atom->f = thermal_stats.cpu_die;
                break;
            case 1:  /* thermal.cpu.proximity */
                atom->f = thermal_stats.cpu_proximity;
                break;
            case 2:  /* thermal.gpu.die */
                atom->f = thermal_stats.gpu_die;
                break;
            case 3:  /* thermal.package */
                atom->f = thermal_stats.package;
                break;
            case 4:  /* thermal.ambient */
                atom->f = thermal_stats.ambient;
                break;
        }
        return PMDA_FETCH_STATIC;
    }

    /* hinv.nfan (item 5) */
    if (item == 5) {
        atom->ul = thermal_stats.nfan;
        return PMDA_FETCH_STATIC;
    }

    /* Fan metrics (items 6-10, per-instance) */
    if (item >= 6 && item <= 10) {
        if (inst >= thermal_stats.nfan)
            return PM_ERR_INST;

        fanstat_t *fan = &thermal_stats.fans[inst];

        switch (item) {
            case 6:  /* thermal.fan.speed */
                atom->f = fan->speed;
                break;
            case 7:  /* thermal.fan.target */
                atom->f = fan->target;
                break;
            case 8:  /* thermal.fan.mode */
                atom->ul = fan->mode;
                break;
            case 9:  /* thermal.fan.min */
                atom->f = fan->min_rpm;
                break;
            case 10: /* thermal.fan.max */
                atom->f = fan->max_rpm;
                break;
        }
        return PMDA_FETCH_STATIC;
    }

    /* Thermal pressure metrics (items 11-12) */
    if (item == 11) {  /* thermal.pressure.level */
        atom->ul = thermal_stats.pressure_level;
        return PMDA_FETCH_STATIC;
    }

    if (item == 12) {  /* thermal.pressure.state */
        atom->cp = thermal_stats.pressure_state;
        return PMDA_FETCH_STATIC;
    }

    return PM_ERR_PMID;
}
