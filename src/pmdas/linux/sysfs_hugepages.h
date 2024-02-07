/*
 * Linux /sys/kernel/mm/hugepages cluster
 *
 * Copyright (c) 2024, Red Hat.
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

typedef struct hugepages {
    uint64_t	values[HUGEPAGES_METRIC_COUNT];
} hugepages_t;

extern int refresh_sysfs_hugepages(pmInDom);

#endif /* SYSFS_HUGEPAGES_H */
