/*
 * Linux NUMA meminfo metrics cluster from sysfs
 *
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2009 Silicon Graphics Inc.,  All Rights Reserved.
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

/*
 * Information from /sys/devices/node/node[0-9]+/meminfo and numastat
 */
typedef struct {
    struct linux_table	*meminfo;
    struct linux_table	*memstat;
} nodeinfo_t;

typedef struct {
    nodeinfo_t	*node_info;
    pmdaIndom	*node_indom;
} numa_meminfo_t;

extern int refresh_numa_meminfo(numa_meminfo_t *, proc_cpuinfo_t *, proc_stat_t *);

