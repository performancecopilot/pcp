/*
 * Linux /proc/stat metrics cluster
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: proc_stat.h,v 1.14 2007/02/20 00:08:32 kimbrr Exp $"

typedef struct {
	unsigned long long user, sys, nice, idle, wait, irq, sirq;
	unsigned int ncpu;
	unsigned long long *p_user, *p_sys, *p_nice, *p_idle, *p_wait, *p_irq, *p_sirq;
	unsigned int ndisk;
	unsigned int page[2];	/* unused in 2.6, switched to /proc/vmstat */
	unsigned int swap[2];	/* unused in 2.6, switched to /proc/vmstat */
	unsigned long long intr;
	unsigned long long ctxt;
	unsigned long btime;
	unsigned long processes;
	pmdaIndom    *cpu_indom;
	unsigned int hz;
} proc_stat_t;

extern int refresh_proc_stat(proc_cpuinfo_t *, proc_stat_t *);
