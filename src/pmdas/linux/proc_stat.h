/*
 * Linux /proc/stat metrics cluster
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
 * Copyright (c) 2015 Red Hat.
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
	unsigned long long	user;
	unsigned long long	sys;
	unsigned long long	nice;
	unsigned long long	idle;
	unsigned long long	wait;
	unsigned long long	irq;
	unsigned long long	sirq;
	unsigned long long	steal;
	unsigned long long	guest;
	unsigned long long	guest_nice;
	unsigned int		ncpu;
	/* per-cpu */
	unsigned long long	*p_user;
	unsigned long long	*p_sys;
	unsigned long long	*p_nice;
	unsigned long long	*p_idle;
	unsigned long long	*p_wait;
	unsigned long long	*p_irq;
	unsigned long long	*p_sirq;
	unsigned long long	*p_steal;
	unsigned long long	*p_guest;
	unsigned long long	*p_guest_nice;
	/* per-node */
	unsigned long long	*n_user;
	unsigned long long	*n_sys;
	unsigned long long	*n_nice;
	unsigned long long	*n_idle;
	unsigned long long	*n_wait;
	unsigned long long	*n_irq;
	unsigned long long	*n_sirq;
	unsigned long long	*n_steal;
	unsigned long long	*n_guest;
	unsigned long long	*n_guest_nice;

	unsigned int		ndisk;
	unsigned int		page[2]; /* unused in 2.6 now in /proc/vmstat */
	unsigned int		swap[2]; /* unused in 2.6 now in /proc/vmstat */
	unsigned long long	intr;
	unsigned long long	ctxt;
	unsigned long		btime;
	unsigned long		processes;
	pmdaIndom		*cpu_indom;
} proc_stat_t;

extern int refresh_proc_stat(proc_cpuinfo_t *, proc_stat_t *);
