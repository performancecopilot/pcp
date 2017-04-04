/*
 * Linux sysfs_tapestats cluster
 *
 * Copyright (c) 2017, Red Hat.
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
#ifndef SYSFS_TAPESTATS_H
#define SYSFS_TAPESTATS_H

enum {
    /* direct indexed counter metrics */
    TAPESTATS_IN_FLIGHT = 0,
    TAPESTATS_IO_NS,
    TAPESTATS_OTHER_CNT,
    TAPESTATS_READ_BYTE_CNT,
    TAPESTATS_READ_CNT,
    TAPESTATS_READ_NS,
    TAPESTATS_RESID_CNT,
    TAPESTATS_WRITE_BYTE_CNT,
    TAPESTATS_WRITE_CNT,
    TAPESTATS_WRITE_NS,    

    /* number of direct indexed counters */
    TAPESTATS_COUNT, 

    /* singular metrics */
    TAPESTATS_HINV_NTAPE = 16 
};

typedef struct {
    int		devnum;		/* inst number */
    char	devname[16];	/* inst name */
    uint64_t	counts[TAPESTATS_COUNT];
} tapedev_t;

typedef struct {
    pmdaIndom           indom;
    tapedev_t		**tapes; /* indom.it_numinst ptrs to tape devs */
} sysfs_tapestats_t;

extern int refresh_sysfs_tapestats(pmInDom);

#endif /* SYSFS_TAPESTATS_H */
