/*
 * Linux /proc/vmstat metrics cluster
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2007,2011 Aconex.  All Rights Reserved.
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
 */

/*
 * All fields (sorted!) in /proc/vmstat for 2.6.x
 */
typedef struct {
	/* sorted by name to make maintenance easier */
	__uint64_t allocstall;
	__uint64_t compact_blocks_moved;
	__uint64_t compact_fail;
	__uint64_t compact_pagemigrate_failed;
	__uint64_t compact_pages_moved;
	__uint64_t compact_stall;
	__uint64_t compact_success;
	__uint64_t htlb_buddy_alloc_fail;
	__uint64_t htlb_buddy_alloc_success;
	__uint64_t kswapd_high_wmark_hit_quickly;
	__uint64_t kswapd_inodesteal;
	__uint64_t kswapd_low_wmark_hit_quickly;
	__uint64_t kswapd_skip_congestion_wait;
	__uint64_t kswapd_steal;
	__uint64_t nr_active_anon;
	__uint64_t nr_active_file;
	__uint64_t nr_anon_pages;
	__uint64_t nr_anon_transparent_hugepages;
	__uint64_t nr_bounce;
	__uint64_t nr_dirtied;
	__uint64_t nr_dirty;
	__uint64_t nr_dirty_threshold;
	__uint64_t nr_dirty_background_threshold;
	__uint64_t nr_file_pages;
	__uint64_t nr_free_pages;
	__uint64_t nr_inactive_anon;
	__uint64_t nr_inactive_file;
	__uint64_t nr_isolated_anon;
	__uint64_t nr_isolated_file;
	__uint64_t nr_kernel_stack;
	__uint64_t nr_mapped;
	__uint64_t nr_mlock;
	__uint64_t nr_page_table_pages;
	__uint64_t nr_shmem;
	__uint64_t nr_slab;		/* not in later kernels */
	__uint64_t nr_slab_reclaimable;
	__uint64_t nr_slab_unreclaimable;
	__uint64_t nr_unevictable;
	__uint64_t nr_unstable;
	__uint64_t nr_vmscan_write;
	__uint64_t nr_writeback;
	__uint64_t nr_writeback_temp;
	__uint64_t nr_written;
	__uint64_t numa_foreign;
	__uint64_t numa_hit;
	__uint64_t numa_interleave;
	__uint64_t numa_local;
	__uint64_t numa_miss;
	__uint64_t numa_other;
	__uint64_t pageoutrun;
	__uint64_t pgactivate;
	__uint64_t pgalloc_dma;
	__uint64_t pgalloc_dma32;
	__uint64_t pgalloc_movable;
	__uint64_t pgalloc_high;
	__uint64_t pgalloc_normal;
	__uint64_t pgdeactivate;
	__uint64_t pgfault;
	__uint64_t pgfree;
	__uint64_t pginodesteal;
	__uint64_t pgmajfault;
	__uint64_t pgpgin;
	__uint64_t pgpgout;
	__uint64_t pgrefill_dma;
	__uint64_t pgrefill_dma32;
	__uint64_t pgrefill_high;
	__uint64_t pgrefill_movable;
	__uint64_t pgrefill_normal;
	__uint64_t pgrotated;
	__uint64_t pgscan_direct_dma;
	__uint64_t pgscan_direct_dma32;
	__uint64_t pgscan_direct_high;
	__uint64_t pgscan_direct_movable;
	__uint64_t pgscan_direct_normal;
	__uint64_t pgscan_kswapd_dma;
	__uint64_t pgscan_kswapd_dma32;
	__uint64_t pgscan_kswapd_high;
	__uint64_t pgscan_kswapd_movable;
	__uint64_t pgscan_kswapd_normal;
	__uint64_t pgsteal_dma;
	__uint64_t pgsteal_dma32;
	__uint64_t pgsteal_high;
	__uint64_t pgsteal_movable;
	__uint64_t pgsteal_normal;
	__uint64_t pswpin;
	__uint64_t pswpout;
	__uint64_t slabs_scanned;
	__uint64_t thp_fault_alloc;
	__uint64_t thp_fault_fallback;
	__uint64_t thp_collapse_alloc;
	__uint64_t thp_collapse_alloc_failed;
	__uint64_t thp_split;
	__uint64_t unevictable_pgs_cleared;
	__uint64_t unevictable_pgs_culled;
	__uint64_t unevictable_pgs_mlocked;
	__uint64_t unevictable_pgs_mlockfreed;
	__uint64_t unevictable_pgs_munlocked;
	__uint64_t unevictable_pgs_rescued;
	__uint64_t unevictable_pgs_scanned;
	__uint64_t unevictable_pgs_stranded;
	__uint64_t zone_reclaim_failed;
} proc_vmstat_t;

extern void proc_vmstat_init(void);
extern int refresh_proc_vmstat(proc_vmstat_t *);
extern int _pm_have_proc_vmstat;
extern proc_vmstat_t _pm_proc_vmstat;

