/*
 * Linux /proc/vmstat metrics cluster
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#ident "$Id: proc_vmstat.h,v 1.1 2004/12/15 06:50:50 markgw Exp $"

/*
 * All fields in /proc/vmstat for 2.6.x
 */
typedef struct {
	__uint64_t nr_dirty;
	__uint64_t nr_writeback;
	__uint64_t nr_unstable;
	__uint64_t nr_page_table_pages;
	__uint64_t nr_mapped;
	__uint64_t nr_slab;
	__uint64_t pgpgin;
	__uint64_t pgpgout;
	__uint64_t pswpin;
	__uint64_t pswpout;
	__uint64_t pgalloc_high;
	__uint64_t pgalloc_normal;
	__uint64_t pgalloc_dma;
	__uint64_t pgfree;
	__uint64_t pgactivate;
	__uint64_t pgdeactivate;
	__uint64_t pgfault;
	__uint64_t pgmajfault;
	__uint64_t pgrefill_high;
	__uint64_t pgrefill_normal;
	__uint64_t pgrefill_dma;
	__uint64_t pgsteal_high;
	__uint64_t pgsteal_normal;
	__uint64_t pgsteal_dma;
	__uint64_t pgscan_kswapd_high;
	__uint64_t pgscan_kswapd_normal;
	__uint64_t pgscan_kswapd_dma;
	__uint64_t pgscan_direct_high;
	__uint64_t pgscan_direct_normal;
	__uint64_t pgscan_direct_dma;
	__uint64_t pginodesteal;
	__uint64_t slabs_scanned;
	__uint64_t kswapd_steal;
	__uint64_t kswapd_inodesteal;
	__uint64_t pageoutrun;
	__uint64_t allocstall;
	__uint64_t pgrotated;
} proc_vmstat_t;

extern int refresh_proc_vmstat(proc_vmstat_t *);
extern int _pm_have_proc_vmstat;
