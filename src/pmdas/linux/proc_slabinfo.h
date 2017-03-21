/*
 * Linux /proc/slabinfo metrics cluster
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2016-2017 Red Hat.
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

/*** version 1.1
    "cache-name" num-active-objs total-objs object-size num-active-slabs \
	total-slabs num-pages-per-slab
    + further values (not exported) on SMP and with statistics enabled
 *** version 1.0
    "cache-name" num-active-objs total-objs
 ***/

typedef struct {
    int		id;
    int		seen;	/* have seen this time, and num values seen */
    __uint64_t	num_active_objs;
    __uint64_t	total_objs;
    __uint32_t	object_size;
    __uint64_t	total_size;
    __uint32_t	num_active_slabs;
    __uint32_t	objects_per_slab;
    __uint32_t	total_slabs;
    __uint32_t	pages_per_slab;
} slab_cache_t;

typedef struct {
    int                 permission;
    slab_cache_t	*caches;
    pmdaIndom		*indom;
} proc_slabinfo_t;

extern int refresh_proc_slabinfo(pmInDom, proc_slabinfo_t *);
extern int proc_slabinfo_fetch(pmInDom, int item, unsigned int, pmAtomValue *);

