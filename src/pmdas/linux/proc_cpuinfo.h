/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2013-2015,2017,2020-2021,2024 Red Hat.
 * Copyright (c) 2001 Gilly Ran (gilly@exanet.com) for the
 * portions of the code supporting the Alpha platform.
 * All rights reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

enum {
    CPUFREQ_SAMPLED	= 1<<0,
    CPUFREQ_SCALE	= 1<<5,
    CPUFREQ_COUNT	= 1<<6,
    CPUFREQ_TIME	= 1<<7,
    CPUFREQ_MAX		= 1<<8,
    CPUFREQ_MIN		= 1<<9,
};

extern int refresh_proc_cpuinfo(void);
extern int refresh_sysfs_online(char *, const char *);
extern int refresh_sysfs_frequency_scaling(char *, int, percpu_t *);
extern int refresh_sysfs_frequency_scaling_cur_freq(char *, int, percpu_t *);
extern unsigned long refresh_sysfs_thermal_throttle(char *, const char *, const char *);
