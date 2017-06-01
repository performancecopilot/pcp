/*
 * Linux /proc/stat metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2015-2017 Red Hat.
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
    cpuacct_t		all;		/* aggregate CPU utilisation */
    unsigned int	page[2]; /* unused in 2.6 now in /proc/vmstat */
    unsigned int	swap[2]; /* unused in 2.6 now in /proc/vmstat */
    unsigned long long	intr;
    unsigned long long	ctxt;
    unsigned long	btime;
    unsigned long	processes;
    unsigned long	procs_running;
    unsigned long	procs_blocked;
} proc_stat_t;

extern int refresh_proc_stat(proc_stat_t *);
extern void setup_cpu_info(cpuinfo_t *);
extern void cpu_node_setup(void);
