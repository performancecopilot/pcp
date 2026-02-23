/*
 * Copyright (c) 2025 Performance Co-Pilot contributors
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

#ifndef POWER_H
#define POWER_H

typedef struct {
    int battery_present;      /* Battery exists (1/0) */
    int charging;             /* Currently charging (1/0) */
    int charge_percent;       /* Current charge 0-100 */
    int time_remaining;       /* Minutes to empty/full, -1 if unknown */
    int health_percent;       /* Battery health 0-100 */
    int cycle_count;          /* Charge cycle count */
    int temperature;          /* Temperature in °C × 100 for precision */
    int voltage_mv;           /* Current voltage in millivolts */
    int amperage_ma;          /* Discharge rate in milliamps (negative when discharging) */
    int design_capacity_mah;  /* Design capacity in mAh */
    int max_capacity_mah;     /* Current maximum capacity in mAh */
    int ac_connected;         /* AC adapter connected (1/0) */
    char power_source[32];    /* "AC Power" or "Battery Power" */
} powerstats_t;

extern int refresh_power(powerstats_t *stats);
extern int fetch_power(unsigned int item, pmAtomValue *atom);

#endif /* POWER_H */
