/*
 * Linux /proc/zoneinfo metrics cluster
 *
 * Copyright (c) 2016 Fujitsu.
 * Copyright (c) 2016 Red Hat.
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
    ZONE_FREE          = 0,
    ZONE_MIN           = 1,
    ZONE_LOW           = 2,
    ZONE_HIGH          = 3,
    ZONE_SCANNED       = 4,
    ZONE_SPANNED       = 5,
    ZONE_PRESENT       = 6,
    ZONE_MANAGED       = 7,
    /* enumerate all values here */
    ZONE_VALUES	/* maximum value */
};

typedef struct {
    __uint64_t	values[ZONE_VALUES];
} zoneinfo_entry_t;

extern int refresh_proc_zoneinfo(pmInDom indom);
