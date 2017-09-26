/*
 * Linux /proc/vmstat metrics cluster
 *
 * Copyright (c) 2013-2014,2016-2017 Red Hat.
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
#include <ctype.h>
#include "linux.h"
#include "proc_vmstat.h"

static struct {
    const char	*field;
    __uint64_t	*offset; 
} vmstat_fields[] = {
    /* sorted by name to make maintenance easier */
    { .field = "allocstall",
     .offset = &_pm_proc_vmstat.allocstall },
    { .field = "balloon_deflate",
     .offset = &_pm_proc_vmstat.balloon_deflate },
    { .field = "balloon_inflate",
     .offset = &_pm_proc_vmstat.balloon_inflate },
    { .field = "balloon_migrate",
     .offset = &_pm_proc_vmstat.balloon_migrate },
    { .field = "compact_blocks_moved",
     .offset = &_pm_proc_vmstat.compact_blocks_moved },
    { .field = "compact_daemon_wake",
     .offset = &_pm_proc_vmstat.compact_daemon_wake },
    { .field = "compact_fail",
     .offset = &_pm_proc_vmstat.compact_fail },
    { .field = "compact_free_scanned",
     .offset = &_pm_proc_vmstat.compact_free_scanned },
    { .field = "compact_isolated",
     .offset = &_pm_proc_vmstat.compact_isolated },
    { .field = "compact_migrate_scanned",
     .offset = &_pm_proc_vmstat.compact_migrate_scanned },
    { .field = "compact_pagemigrate_failed",
     .offset = &_pm_proc_vmstat.compact_pagemigrate_failed },
    { .field = "compact_pages_moved",
     .offset = &_pm_proc_vmstat.compact_pages_moved },
    { .field = "compact_stall",
     .offset = &_pm_proc_vmstat.compact_stall },
    { .field = "compact_success",
     .offset = &_pm_proc_vmstat.drop_pagecache },
    { .field = "drop_pagecache",
     .offset = &_pm_proc_vmstat.drop_slab },
    { .field = "drop_slab",
     .offset = &_pm_proc_vmstat.compact_success },
    { .field = "htlb_buddy_alloc_fail",
     .offset = &_pm_proc_vmstat.htlb_buddy_alloc_fail },
    { .field = "htlb_buddy_alloc_success",
     .offset = &_pm_proc_vmstat.htlb_buddy_alloc_success },
    { .field = "kswapd_inodesteal",
     .offset = &_pm_proc_vmstat.kswapd_inodesteal },
    { .field = "kswapd_low_wmark_hit_quickly",
     .offset = &_pm_proc_vmstat.kswapd_low_wmark_hit_quickly },
    { .field = "kswapd_high_wmark_hit_quickly",
     .offset = &_pm_proc_vmstat.kswapd_high_wmark_hit_quickly },
    { .field = "kswapd_skip_congestion_wait",
     .offset = &_pm_proc_vmstat.kswapd_skip_congestion_wait },
    { .field = "kswapd_steal",
     .offset = &_pm_proc_vmstat.kswapd_steal },
    { .field = "nr_active_anon",
     .offset = &_pm_proc_vmstat.nr_active_anon },
    { .field = "nr_active_file",
     .offset = &_pm_proc_vmstat.nr_active_file },
    { .field = "nr_anon_pages",
     .offset = &_pm_proc_vmstat.nr_anon_pages },
    { .field = "nr_anon_transparent_hugepages",
     .offset = &_pm_proc_vmstat.nr_anon_transparent_hugepages },
    { .field = "nr_bounce",
     .offset = &_pm_proc_vmstat.nr_bounce },
    { .field = "nr_dirty",
     .offset = &_pm_proc_vmstat.nr_dirty },
    { .field = "nr_dirtied",
     .offset = &_pm_proc_vmstat.nr_dirtied },
    { .field = "nr_dirty_threshold",
     .offset = &_pm_proc_vmstat.nr_dirty_threshold },
    { .field = "nr_dirty_background_threshold",
     .offset = &_pm_proc_vmstat.nr_dirty_background_threshold },
    { .field = "nr_file_pages",
     .offset = &_pm_proc_vmstat.nr_file_pages },
    { .field = "nr_free_cma",
     .offset = &_pm_proc_vmstat.nr_free_cma },
    { .field = "nr_free_pages",
     .offset = &_pm_proc_vmstat.nr_free_pages },
    { .field = "nr_inactive_anon",
     .offset = &_pm_proc_vmstat.nr_inactive_anon },
    { .field = "nr_inactive_file",
     .offset = &_pm_proc_vmstat.nr_inactive_file },
    { .field = "nr_isolated_anon",
     .offset = &_pm_proc_vmstat.nr_isolated_anon },
    { .field = "nr_isolated_file",
     .offset = &_pm_proc_vmstat.nr_isolated_file },
    { .field = "nr_kernel_stack",
     .offset = &_pm_proc_vmstat.nr_kernel_stack },
    { .field = "nr_mapped",
     .offset = &_pm_proc_vmstat.nr_mapped },
    { .field = "nr_mlock",
     .offset = &_pm_proc_vmstat.nr_mlock },
    { .field = "nr_pages_scanned",
     .offset = &_pm_proc_vmstat.nr_pages_scanned },
    { .field = "nr_page_table_pages",
     .offset = &_pm_proc_vmstat.nr_page_table_pages },
    { .field = "nr_shmem",
     .offset = &_pm_proc_vmstat.nr_shmem },
    { .field = "nr_shmem_hugepages",
     .offset = &_pm_proc_vmstat.nr_shmem_hugepages },
    { .field = "nr_shmem_pmdmapped",
     .offset = &_pm_proc_vmstat.nr_shmem_pmdmapped },
    { .field = "nr_slab_reclaimable",
     .offset = &_pm_proc_vmstat.nr_slab_reclaimable },
    { .field = "nr_slab_unreclaimable",
     .offset = &_pm_proc_vmstat.nr_slab_unreclaimable },
    { .field = "nr_slab",
     .offset = &_pm_proc_vmstat.nr_slab },
    { .field = "nr_unevictable",
     .offset = &_pm_proc_vmstat.nr_unevictable },
    { .field = "nr_unstable",
     .offset = &_pm_proc_vmstat.nr_unstable },
    { .field = "nr_vmscan_immediate_reclaim",
     .offset = &_pm_proc_vmstat.nr_vmscan_immediate_reclaim },
    { .field = "nr_vmscan_write",
     .offset = &_pm_proc_vmstat.nr_vmscan_write },
    { .field = "nr_writeback",
     .offset = &_pm_proc_vmstat.nr_writeback },
    { .field = "nr_writeback_temp",
     .offset = &_pm_proc_vmstat.nr_writeback_temp },
    { .field = "nr_written",
     .offset = &_pm_proc_vmstat.nr_written },
    { .field = "nr_zone_inactive_anon",
     .offset = &_pm_proc_vmstat.nr_zone_inactive_anon },
    { .field = "nr_zone_active_anon",
     .offset = &_pm_proc_vmstat.nr_zone_active_anon },
    { .field = "nr_zone_inactive_file",
     .offset = &_pm_proc_vmstat.nr_zone_inactive_file },
    { .field = "nr_zone_active_file",
     .offset = &_pm_proc_vmstat.nr_zone_active_file },
    { .field = "nr_zone_unevictable",
     .offset = &_pm_proc_vmstat.nr_zone_unevictable },
    { .field = "nr_zone_write_pending",
     .offset = &_pm_proc_vmstat.nr_zone_write_pending },
    { .field = "nr_zspages",
     .offset = &_pm_proc_vmstat.nr_zspages },
    { .field = "numa_foreign",
     .offset = &_pm_proc_vmstat.numa_foreign },
    { .field = "numa_hint_faults",
     .offset = &_pm_proc_vmstat.numa_hint_faults },
    { .field = "numa_hint_faults_local",
     .offset = &_pm_proc_vmstat.numa_hint_faults_local },
    { .field = "numa_hit",
     .offset = &_pm_proc_vmstat.numa_hit },
    { .field = "numa_huge_pte_updates",
     .offset = &_pm_proc_vmstat.numa_huge_pte_updates },
    { .field = "numa_miss",
     .offset = &_pm_proc_vmstat.numa_miss },
    { .field = "numa_interleave",
     .offset = &_pm_proc_vmstat.numa_interleave },
    { .field = "numa_local",
     .offset = &_pm_proc_vmstat.numa_local },
    { .field = "numa_other",
     .offset = &_pm_proc_vmstat.numa_other },
    { .field = "numa_pages_migrated",
     .offset = &_pm_proc_vmstat.numa_pages_migrated },
    { .field = "numa_pte_updates",
     .offset = &_pm_proc_vmstat.numa_pte_updates },
    { .field = "pageoutrun",
     .offset = &_pm_proc_vmstat.pageoutrun },
    { .field = "pgactivate",
     .offset = &_pm_proc_vmstat.pgactivate },
    { .field = "pgalloc_dma",
     .offset = &_pm_proc_vmstat.pgalloc_dma },
    { .field = "pgalloc_dma32",
     .offset = &_pm_proc_vmstat.pgalloc_dma32 },
    { .field = "pgalloc_high",
     .offset = &_pm_proc_vmstat.pgalloc_high },
    { .field = "pgalloc_movable",
     .offset = &_pm_proc_vmstat.pgalloc_movable },
    { .field = "pgalloc_normal",
     .offset = &_pm_proc_vmstat.pgalloc_normal },
    { .field = "pgdeactivate",
     .offset = &_pm_proc_vmstat.pgdeactivate },
    { .field = "pgfault",
     .offset = &_pm_proc_vmstat.pgfault },
    { .field = "pgfree",
     .offset = &_pm_proc_vmstat.pgfree },
    { .field = "pginodesteal",
     .offset = &_pm_proc_vmstat.pginodesteal },
    { .field = "pglazyfreed",
     .offset = &_pm_proc_vmstat.pglazyfreed },
    { .field = "pgmajfault",
     .offset = &_pm_proc_vmstat.pgmajfault },
    { .field = "pgmigrate_fail",
     .offset = &_pm_proc_vmstat.pgmigrate_fail },
    { .field = "pgmigrate_success",
     .offset = &_pm_proc_vmstat.pgmigrate_success },
    { .field = "pgpgin",
     .offset = &_pm_proc_vmstat.pgpgin },
    { .field = "pgpgout",
     .offset = &_pm_proc_vmstat.pgpgout },
    { .field = "pgrefill_dma",
     .offset = &_pm_proc_vmstat.pgrefill_dma },
    { .field = "pgrefill_dma32",
     .offset = &_pm_proc_vmstat.pgrefill_dma32 },
    { .field = "pgrefill_high",
     .offset = &_pm_proc_vmstat.pgrefill_high },
    { .field = "pgrefill_movable",
     .offset = &_pm_proc_vmstat.pgrefill_movable },
    { .field = "pgrefill_normal",
     .offset = &_pm_proc_vmstat.pgrefill_normal },
    { .field = "pgrotated",
     .offset = &_pm_proc_vmstat.pgrotated },
    { .field = "pgscan_direct_dma",
     .offset = &_pm_proc_vmstat.pgscan_direct_dma },
    { .field = "pgscan_direct_dma32",
     .offset = &_pm_proc_vmstat.pgscan_direct_dma32 },
    { .field = "pgscan_direct_high",
     .offset = &_pm_proc_vmstat.pgscan_direct_high },
    { .field = "pgscan_direct_movable",
     .offset = &_pm_proc_vmstat.pgscan_direct_movable },
    { .field = "pgscan_direct_normal",
     .offset = &_pm_proc_vmstat.pgscan_direct_normal },
    { .field = "pgscan_direct_throttle",
     .offset = &_pm_proc_vmstat.pgscan_direct_throttle },
    { .field = "pgscan_kswapd_dma",
     .offset = &_pm_proc_vmstat.pgscan_kswapd_dma },
    { .field = "pgscan_kswapd_dma32",
     .offset = &_pm_proc_vmstat.pgscan_kswapd_dma32 },
    { .field = "pgscan_kswapd_high",
     .offset = &_pm_proc_vmstat.pgscan_kswapd_high },
    { .field = "pgscan_kswapd_movable",
     .offset = &_pm_proc_vmstat.pgscan_kswapd_movable },
    { .field = "pgscan_kswapd_normal",
     .offset = &_pm_proc_vmstat.pgscan_kswapd_normal },
    { .field = "pgsteal_dma",
     .offset = &_pm_proc_vmstat.pgsteal_dma },
    { .field = "pgsteal_dma32",
     .offset = &_pm_proc_vmstat.pgsteal_dma32 },
    { .field = "pgsteal_high",
     .offset = &_pm_proc_vmstat.pgsteal_high },
    { .field = "pgsteal_movable",
     .offset = &_pm_proc_vmstat.pgsteal_movable },
    { .field = "pgsteal_normal",
     .offset = &_pm_proc_vmstat.pgsteal_normal },
    { .field = "pgsteal_kswapd_dma",
     .offset = &_pm_proc_vmstat.pgsteal_kswapd_dma },
    { .field = "pgsteal_kswapd_dma32",
     .offset = &_pm_proc_vmstat.pgsteal_kswapd_dma32 },
    { .field = "pgsteal_kswapd_normal",
     .offset = &_pm_proc_vmstat.pgsteal_kswapd_normal },
    { .field = "pgsteal_kswapd_movable",
     .offset = &_pm_proc_vmstat.pgsteal_kswapd_movable },
    { .field = "pgsteal_direct_dma",
     .offset = &_pm_proc_vmstat.pgsteal_direct_dma },
    { .field = "pgsteal_direct_dma32",
     .offset = &_pm_proc_vmstat.pgsteal_direct_dma32 },
    { .field = "pgsteal_direct_normal",
     .offset = &_pm_proc_vmstat.pgsteal_direct_normal },
    { .field = "pgsteal_direct_movable",
     .offset = &_pm_proc_vmstat.pgsteal_direct_movable },
    { .field = "pswpin",
     .offset = &_pm_proc_vmstat.pswpin },
    { .field = "pswpout",
     .offset = &_pm_proc_vmstat.pswpout },
    { .field = "slabs_scanned",
     .offset = &_pm_proc_vmstat.slabs_scanned },
    { .field = "thp_collapse_alloc",
     .offset = &_pm_proc_vmstat.thp_collapse_alloc },
    { .field = "thp_collapse_alloc_failed",
     .offset = &_pm_proc_vmstat.thp_collapse_alloc_failed },
    { .field = "thp_deferred_split_page",
     .offset = &_pm_proc_vmstat.thp_deferred_split_page },
    { .field = "thp_fault_alloc",
     .offset = &_pm_proc_vmstat.thp_fault_alloc },
    { .field = "thp_fault_fallback",
     .offset = &_pm_proc_vmstat.thp_fault_fallback },
    { .field = "thp_file_alloc",
     .offset = &_pm_proc_vmstat.thp_file_alloc },
    { .field = "thp_file_mapped",
     .offset = &_pm_proc_vmstat.thp_file_mapped },
    { .field = "thp_split",
     .offset = &_pm_proc_vmstat.thp_split },
    { .field = "thp_split_page",
     .offset = &_pm_proc_vmstat.thp_split_page },
    { .field = "thp_split_page_failed",
     .offset = &_pm_proc_vmstat.thp_split_page_failed },
    { .field = "thp_split_pmd",
     .offset = &_pm_proc_vmstat.thp_split_pmd },
    { .field = "thp_zero_page_alloc",
     .offset = &_pm_proc_vmstat.thp_zero_page_alloc },
    { .field = "thp_zero_page_alloc_failed",
     .offset = &_pm_proc_vmstat.thp_zero_page_alloc_failed },
    { .field = "unevictable_pgs_cleared",
     .offset = &_pm_proc_vmstat.unevictable_pgs_cleared },
    { .field = "unevictable_pgs_culled",
     .offset = &_pm_proc_vmstat.unevictable_pgs_culled },
    { .field = "unevictable_pgs_mlocked",
     .offset = &_pm_proc_vmstat.unevictable_pgs_mlocked },
    { .field = "unevictable_pgs_mlockfreed",
     .offset = &_pm_proc_vmstat.unevictable_pgs_mlockfreed },
    { .field = "unevictable_pgs_munlocked",
     .offset = &_pm_proc_vmstat.unevictable_pgs_munlocked },
    { .field = "unevictable_pgs_rescued",
     .offset = &_pm_proc_vmstat.unevictable_pgs_rescued },
    { .field = "unevictable_pgs_scanned",
     .offset = &_pm_proc_vmstat.unevictable_pgs_scanned },
    { .field = "unevictable_pgs_stranded",
     .offset = &_pm_proc_vmstat.unevictable_pgs_stranded },
    { .field = "workingset_activate",
     .offset = &_pm_proc_vmstat.workingset_activate },
    { .field = "workingset_nodereclaim",
     .offset = &_pm_proc_vmstat.workingset_nodereclaim },
    { .field = "workingset_refault",
     .offset = &_pm_proc_vmstat.workingset_refault },
    { .field = "zone_reclaim_failed",
     .offset = &_pm_proc_vmstat.zone_reclaim_failed },

    { .field = NULL, .offset = NULL }
};

#define VMSTAT_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)vmstat_fields[ii].offset - (__psint_t)&_pm_proc_vmstat)

void
proc_vmstat_init(void)
{
    char	buf[1024];

    /*
     * The swap metrics moved from /proc/stat to /proc/vmstat early in 2.6.
     * In addition, the swap operation count was removed; the fetch routine
     * needs to deal with these quirks and return something sensible based
     * (initially) on whether the vmstat file exists.
     *
     * We'll re-evaluate this on each fetch of the mem.vmstat metrics, but
     * that is not a problem.  This routine makes sure any swap.xxx metric
     * fetch without a preceding mem.vmstat fetch has the correct state.
     */
    pmsprintf(buf, sizeof(buf), "%s/proc/vmstat", linux_statspath);
    _pm_have_proc_vmstat = (access(buf, R_OK) == 0);
}

int
refresh_proc_vmstat(proc_vmstat_t *proc_vmstat)
{
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    for (i = 0; vmstat_fields[i].field != NULL; i++) {
	p = VMSTAT_OFFSET(i, proc_vmstat);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = linux_statsfile("/proc/vmstat", buf, sizeof(buf))) == NULL)
    	return -oserror();

    _pm_have_proc_vmstat = 1;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((bufp = strchr(buf, ' ')) == NULL)
	    continue;
	*bufp = '\0';
	for (i = 0; vmstat_fields[i].field != NULL; i++) {
	    if (strcmp(buf, vmstat_fields[i].field) != 0)
		continue;
	    p = VMSTAT_OFFSET(i, proc_vmstat);
	    for (bufp++; *bufp; bufp++) {
	    	if (isdigit((int)*bufp)) {
		    sscanf(bufp, "%llu", (unsigned long long *)p);
		    break;
		}
	    }
	}
    }
    fclose(fp);

    if (proc_vmstat->nr_slab == -1)	/* split apart in 2.6.18 */
	proc_vmstat->nr_slab = proc_vmstat->nr_slab_reclaimable +
				proc_vmstat->nr_slab_unreclaimable;
    if (proc_vmstat->thp_split == -1)
	proc_vmstat->thp_split = proc_vmstat->thp_split_page +
				proc_vmstat->thp_split_page_failed;
    return 0;
}
