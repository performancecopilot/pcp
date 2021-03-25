/*
 * Linux sysfs_fchost (Fibre Channel) cluster
 *
 * Copyright (c) 2021, Red Hat.
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
#ifndef SYSFS_FCHOST_H
#define SYSFS_FCHOST_H

enum {
    /* direct indexed counter metrics */
    FCHOST_IN_FRAMES = 0,
    FCHOST_OUT_FRAMES,
    FCHOST_IN_BYTES,
    FCHOST_OUT_BYTES,
    FCHOST_LIP_COUNT,
    FCHOST_NOS_COUNT,
    FCHOST_ERROR_FRAMES,
    FCHOST_DUMPED_FRAMES,

    /* number of direct indexed counters */
    FCHOST_COUNT, 

    /* singular metrics */
    FCHOST_HINV_NFCHOST = 16 
};

typedef struct fchost {
    uint64_t	counts[FCHOST_COUNT];
} fchost_t;

extern int refresh_sysfs_fchosts(pmInDom);

#endif /* SYSFS_FCHOST_H */
