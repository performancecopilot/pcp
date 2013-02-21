/*
 * Linux /proc/vmstat metrics cluster
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Portions Copyright (c) 2007,2011 Aconex.  All Rights Reserved.
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
#include "pmapi.h"
#include "proc_vmstat.h"

static proc_vmstat_t vmstat;

static struct {
    char	*field;
    __uint64_t	*offset; 
} vmstat_fields[] = {
    /* sorted by name to make maintenance easier */
    { "allocstall",		&vmstat.allocstall },
    { "compact_blocks_moved",	&vmstat.compact_blocks_moved },
    { "compact_fail",		&vmstat.compact_fail },
    { "compact_pagemigrate_failed", &vmstat.compact_pagemigrate_failed },
    { "compact_pages_moved",	&vmstat.compact_pages_moved },
    { "compact_stall",		&vmstat.compact_stall },
    { "compact_success",	&vmstat.compact_success },
    { "htlb_buddy_alloc_fail",	&vmstat.htlb_buddy_alloc_fail },
    { "htlb_buddy_alloc_success", &vmstat.htlb_buddy_alloc_success },
    { "kswapd_inodesteal",	&vmstat.kswapd_inodesteal },
    { "kswapd_low_wmark_hit_quickly", &vmstat.kswapd_low_wmark_hit_quickly },
    { "kswapd_high_wmark_hit_quickly", &vmstat.kswapd_high_wmark_hit_quickly },
    { "kswapd_skip_congestion_wait", &vmstat.kswapd_skip_congestion_wait },
    { "kswapd_steal",		&vmstat.kswapd_steal },
    { "nr_active_anon",		&vmstat.nr_active_anon },
    { "nr_active_file",		&vmstat.nr_active_file },
    { "nr_anon_pages",		&vmstat.nr_anon_pages },
    { "nr_anon_transparent_hugepages", &vmstat.nr_anon_transparent_hugepages },
    { "nr_bounce",		&vmstat.nr_bounce },
    { "nr_dirty",		&vmstat.nr_dirty },
    { "nr_dirtied",		&vmstat.nr_dirtied },
    { "nr_dirty_threshold",	&vmstat.nr_dirty_threshold },
    { "nr_dirty_background_threshold", &vmstat.nr_dirty_background_threshold },
    { "nr_file_pages",		&vmstat.nr_file_pages },
    { "nr_free_pages",		&vmstat.nr_free_pages },
    { "nr_inactive_anon",	&vmstat.nr_inactive_anon },
    { "nr_inactive_file",	&vmstat.nr_inactive_file },
    { "nr_isolated_anon",	&vmstat.nr_isolated_anon },
    { "nr_isolated_file",	&vmstat.nr_isolated_file },
    { "nr_kernel_stack",	&vmstat.nr_kernel_stack },
    { "nr_mapped",		&vmstat.nr_mapped },
    { "nr_mlock",		&vmstat.nr_mlock },
    { "nr_page_table_pages",	&vmstat.nr_page_table_pages },
    { "nr_shmem",		&vmstat.nr_shmem },
    { "nr_slab_reclaimable",	&vmstat.nr_slab_reclaimable },
    { "nr_slab_unreclaimable",	&vmstat.nr_slab_unreclaimable },
    { "nr_slab",		&vmstat.nr_slab }, /* not in later kernels */
    { "nr_unevictable",		&vmstat.nr_unevictable },
    { "nr_unstable",		&vmstat.nr_unstable },
    { "nr_vmscan_write",	&vmstat.nr_vmscan_write },
    { "nr_writeback",		&vmstat.nr_writeback },
    { "nr_writeback_temp",	&vmstat.nr_writeback_temp },
    { "nr_written",		&vmstat.nr_written },
    { "numa_hit",		&vmstat.numa_hit },
    { "numa_miss",		&vmstat.numa_miss },
    { "numa_foreign",		&vmstat.numa_foreign },
    { "numa_interleave",	&vmstat.numa_interleave },
    { "numa_local",		&vmstat.numa_local },
    { "numa_other",		&vmstat.numa_other },
    { "pageoutrun",		&vmstat.pageoutrun },
    { "pgactivate",		&vmstat.pgactivate },
    { "pgalloc_dma",		&vmstat.pgalloc_dma },
    { "pgalloc_dma32",		&vmstat.pgalloc_dma32 },
    { "pgalloc_high",		&vmstat.pgalloc_high },
    { "pgalloc_movable",	&vmstat.pgalloc_movable },
    { "pgalloc_normal",		&vmstat.pgalloc_normal },
    { "pgdeactivate",		&vmstat.pgdeactivate },
    { "pgfault",		&vmstat.pgfault },
    { "pgfree",			&vmstat.pgfree },
    { "pginodesteal",		&vmstat.pginodesteal },
    { "pgmajfault",		&vmstat.pgmajfault },
    { "pgpgin",			&vmstat.pgpgin },
    { "pgpgout",		&vmstat.pgpgout },
    { "pgrefill_dma",		&vmstat.pgrefill_dma },
    { "pgrefill_dma32",		&vmstat.pgrefill_dma32 },
    { "pgrefill_high",		&vmstat.pgrefill_high },
    { "pgrefill_movable",	&vmstat.pgrefill_movable },
    { "pgrefill_normal",	&vmstat.pgrefill_normal },
    { "pgrotated",		&vmstat.pgrotated },
    { "pgscan_direct_dma",	&vmstat.pgscan_direct_dma },
    { "pgscan_direct_dma32",	&vmstat.pgscan_direct_dma32 },
    { "pgscan_direct_high",	&vmstat.pgscan_direct_high },
    { "pgscan_direct_movable",	&vmstat.pgscan_direct_movable },
    { "pgscan_direct_normal",	&vmstat.pgscan_direct_normal },
    { "pgscan_kswapd_dma",	&vmstat.pgscan_kswapd_dma },
    { "pgscan_kswapd_dma32",	&vmstat.pgscan_kswapd_dma32 },
    { "pgscan_kswapd_high",	&vmstat.pgscan_kswapd_high },
    { "pgscan_kswapd_movable",	&vmstat.pgscan_kswapd_movable },
    { "pgscan_kswapd_normal",	&vmstat.pgscan_kswapd_normal },
    { "pgsteal_dma",		&vmstat.pgsteal_dma },
    { "pgsteal_dma32",		&vmstat.pgsteal_dma32 },
    { "pgsteal_high",		&vmstat.pgsteal_high },
    { "pgsteal_movable",	&vmstat.pgsteal_movable },
    { "pgsteal_normal",		&vmstat.pgsteal_normal },
    { "pswpin",			&vmstat.pswpin },
    { "pswpout",		&vmstat.pswpout },
    { "slabs_scanned",		&vmstat.slabs_scanned },
    { "thp_collapse_alloc",	&vmstat.thp_collapse_alloc },
    { "thp_collapse_alloc_failed", &vmstat.thp_collapse_alloc_failed },
    { "thp_fault_alloc",	&vmstat.thp_fault_alloc },
    { "thp_fault_fallback",	&vmstat.thp_fault_fallback },
    { "thp_split",		&vmstat.thp_split },
    { "unevictable_pgs_cleared",&vmstat.unevictable_pgs_cleared },
    { "unevictable_pgs_culled",	&vmstat.unevictable_pgs_culled },
    { "unevictable_pgs_mlocked",&vmstat.unevictable_pgs_mlocked },
    { "unevictable_pgs_mlockfreed", &vmstat.unevictable_pgs_mlockfreed },
    { "unevictable_pgs_munlocked", &vmstat.unevictable_pgs_munlocked },
    { "unevictable_pgs_rescued", &vmstat.unevictable_pgs_rescued },
    { "unevictable_pgs_scanned", &vmstat.unevictable_pgs_scanned },
    { "unevictable_pgs_stranded", &vmstat.unevictable_pgs_stranded },
    { "zone_reclaim_failed",	&vmstat.zone_reclaim_failed },
    { NULL, NULL }
};

#define VMSTAT_OFFSET(ii, pp) (int64_t *)((char *)pp + \
    (__psint_t)vmstat_fields[ii].offset - (__psint_t)&vmstat)

int
refresh_proc_vmstat(proc_vmstat_t *proc_vmstat)
{
    static int	started;
    char	buf[1024];
    char	*bufp;
    int64_t	*p;
    int		i;
    FILE	*fp;

    if (!started) {
	started = 1;
	memset(proc_vmstat, 0, sizeof(*proc_vmstat));
    }

    for (i = 0; vmstat_fields[i].field != NULL; i++) {
	p = VMSTAT_OFFSET(i, proc_vmstat);
	*p = -1; /* marked as "no value available" */
    }

    if ((fp = fopen("/proc/vmstat", "r")) == NULL)
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

    /* success */
    return 0;
}
