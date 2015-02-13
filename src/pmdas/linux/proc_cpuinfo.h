/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2013-2015 Red Hat.
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

typedef struct {
    int			cpu_num;
    int			node;
    char		*name;
    float		clock;
    float		bogomips;
    int			sapic;		/* strings dictionary hash key */
    int			vendor;		/* strings dictionary hash key */
    int			model;		/* strings dictionary hash key */
    int			model_name;	/* strings dictionary hash key */
    int			stepping;	/* strings dictionary hash key */
    int			flags;		/* strings dictionary hash key */
    unsigned int	cache;
    unsigned int	cache_align;
} cpuinfo_t;

typedef struct {
    char		*machine;
    cpuinfo_t		*cpuinfo;
    pmdaIndom		*cpuindom;
    pmdaIndom		*node_indom;
} proc_cpuinfo_t;

extern int refresh_proc_cpuinfo(proc_cpuinfo_t *);
extern char *cpu_name(proc_cpuinfo_t *, unsigned int);
extern int refresh_sysfs_online(unsigned int, const char *);
