/*
 * Linux /sys/{kernel/mm,devices/system/node/nodeN}/hugepages clusters
 *
 * Copyright (c) 2024-2025, Red Hat.
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
#ifndef SYSFS_HUGEPAGES_H
#define SYSFS_HUGEPAGES_H

enum {
    /* direct indexed pagesize metric */
    PAGESIZE_HUGEPAGES = 0,

    /* direct indexed counter metrics */
    FREE_HUGEPAGES,
    RESV_HUGEPAGES,
    SURPLUS_HUGEPAGES,
    TOTALSIZE_HUGEPAGES,
    OVERCOMMIT_HUGEPAGES,

    /* number of direct indexed counters */
    HUGEPAGES_METRIC_COUNT
};

enum {
    /* direct indexed NUMA pagesize metric */
    PAGESIZE_NUMA_HUGEPAGES = 0,

    /* direct indexed NUMA counter metrics */
    FREE_NUMA_HUGEPAGES,
    SURPLUS_NUMA_HUGEPAGES,
    TOTALSIZE_NUMA_HUGEPAGES,

    /* number of direct indexed counters */
    NUMA_HUGEPAGES_METRIC_COUNT
};

extern int refresh_sysfs_hugepages(pmInDom);
extern int refresh_sysfs_numa_hugepages(pmInDom);

#endif /* SYSFS_HUGEPAGES_H */
