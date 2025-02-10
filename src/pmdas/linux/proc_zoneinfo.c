/*
 * Linux zoneinfo Cluster
 *
 * Copyright (c) 2016-2017,2019 Fujitsu.
 * Copyright (c) 2017-2018,2021 Red Hat.
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
#include "linux.h"
#include "proc_zoneinfo.h"

static void
extract_zone_protection(const char *bp, int node, const char *zonetype,
			const char *instname, pmInDom protected)
{
    zoneprot_entry_t *prot;
    char *endp, prot_name[64];
    unsigned long long value;
    unsigned int lowmem;
    int sts;

    for (lowmem = 0;; lowmem++) {
	value = strtoul(bp, &endp, 10);
	pmsprintf(prot_name, sizeof(prot_name),
		 "%s::lowmem_reserved%u", instname, lowmem);
	/* replace existing value if one exists, else need space for new one */
	prot = NULL;
	sts = pmdaCacheLookupName(protected, prot_name, NULL, (void **)&prot);
	if ((sts < 0 && prot == NULL) &&
	    (prot = (zoneprot_entry_t *)calloc(1, sizeof(*prot))) == NULL)
	    continue;
	prot->node = node;
	prot->value = value;
	prot->lowmem = lowmem;
	pmsprintf(prot->zone, ZONE_NAMELEN, "%s", zonetype);
	pmdaCacheStore(protected, PMDA_CACHE_ADD, prot_name, (void *)prot);
	if (*endp != ',')
	    break;
	bp = endp + 2;   /* skip comma and space, then continue */
    }
}

int
refresh_proc_zoneinfo(pmInDom indom, pmInDom protection_indom)
{
    int node;
    zoneinfo_entry_t *info;
    zoneinfo_entry_t *perzone;
    zoneinfo_entry_t *pernode;
    unsigned long long value;
    char zonetype[ZONE_NAMELEN];
    char instname[64];
    char nodename[64];
    char buf[BUFSIZ];
    static int setup;
    int changed = 0;
    FILE *fp;

    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    if ((fp = linux_statsfile("/proc/zoneinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    while ((!feof(fp)) && fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "Node", 4) != 0)
	    continue;
	if (sscanf(buf, "Node %d, zone   %s", &node, zonetype) != 2)
	    continue;

	info = pernode = perzone = NULL;
	pmsprintf(instname, sizeof(instname), "%s::node%u", zonetype, node);
	if (pmdaCacheLookupName(indom, instname, NULL, (void **)&info) < 0 ||
	    info == NULL) {
	    /* not found: allocate and add a new entry */
	    info = (zoneinfo_entry_t *)calloc(1, sizeof(zoneinfo_entry_t));
	    changed = 1;
	}
	info->node = node;
	pmsprintf(info->zone, ZONE_NAMELEN, "%s", zonetype);
	info->flags = 0;
	perzone = info;

	/* inner loop to extract all values for this node */
	while ((!feof(fp)) && fgets(buf, sizeof(buf), fp) != NULL) {

	    /* bail out of inner loop when we reach the next node */
	    if (strncmp(buf, "Node", 4) == 0) {
		(void)fseek(fp, -(long)(strlen(buf)), 1); /* back-track */
		break;
	    }

	    /* switch to/from kernel section with per-node metrics */
	    if (strncmp(buf, "  per-node stats", 16) == 0) {
		info = NULL;
		pmsprintf(nodename, sizeof(nodename), "node%u", node);
		if (pmdaCacheLookupName(indom, nodename, NULL, (void **)&info) < 0 ||
		    info == NULL) {
		    /* not found: allocate and add a new entry */
		    info = (zoneinfo_entry_t *)calloc(1, sizeof(zoneinfo_entry_t));
		    changed = 1;
		}
		info->node = node;
		info->flags = 0;
		pernode = info;
	    } else if (strncmp(buf, "  pages ", 8) == 0) {
		info = perzone;
	    }

	    if ((sscanf(buf, " pages free %llu", &value)) == 1) {
		info->values[ZONE_FREE] = value;
		info->flags |= (1ULL << ZONE_FREE);
	    }
	    else if ((sscanf(buf, " min %llu", &value)) == 1) {
		info->values[ZONE_MIN] = value;
		info->flags |= (1ULL << ZONE_MIN);
	    }
	    else if ((sscanf(buf, " low %llu", &value)) == 1) {
		info->values[ZONE_LOW] = value;
		info->flags |= (1ULL << ZONE_LOW);
	    }
	    else if ((sscanf(buf, " high %llu", &value)) == 1) {
		info->values[ZONE_HIGH] = value;
		info->flags |= (1ULL << ZONE_HIGH);
	    }
	    else if ((sscanf(buf, " scanned %llu", &value)) == 1 ||
	             (sscanf(buf, " node_scanned %llu", &value)) == 1) {
		info->values[ZONE_SCANNED] = value;
		info->flags |= (1ULL << ZONE_SCANNED);
	    }
	    else if ((sscanf(buf, " spanned %llu", &value)) == 1) {
		info->values[ZONE_SPANNED] = value;
		info->flags |= (1ULL << ZONE_SPANNED);
	    }
	    else if ((sscanf(buf, " present %llu", &value)) == 1) {
		info->values[ZONE_PRESENT] = value;
		info->flags |= (1ULL << ZONE_PRESENT);
	    }
	    else if ((sscanf(buf, " managed %llu", &value)) == 1) {
		info->values[ZONE_MANAGED] = value;
		info->flags |= (1ULL << ZONE_MANAGED);
	    }
	    else if ((sscanf(buf, " cma %llu", &value)) == 1) {
		info->values[ZONE_CMA] = value;
		info->flags |= (1ULL << ZONE_CMA);
	    }
	    else if ((sscanf(buf, " nr_alloc_batch %llu", &value)) == 1) {
		info->values[ZONE_NR_ALLOC_BATCH] = value;
		info->flags |= (1ULL << ZONE_NR_ALLOC_BATCH);
	    }
	    else if ((sscanf(buf, " nr_inactive_anon %llu", &value)) == 1) {
		info->values[ZONE_NR_INACTIVE_ANON] = value;
		info->flags |= (1ULL << ZONE_NR_INACTIVE_ANON);
	    }
	    else if ((sscanf(buf, " nr_active_anon %llu", &value)) == 1) {
		info->values[ZONE_NR_ACTIVE_ANON] = value;
		info->flags |= (1ULL << ZONE_NR_ACTIVE_ANON);
	    }
	    else if ((sscanf(buf, " nr_inactive_file %llu", &value)) == 1) {
		info->values[ZONE_NR_INACTIVE_FILE] = value;
		info->flags |= (1ULL << ZONE_NR_INACTIVE_FILE);
	    }
	    else if ((sscanf(buf, " nr_active_file %llu", &value)) == 1) {
		info->values[ZONE_NR_ACTIVE_FILE] = value;
		info->flags |= (1ULL << ZONE_NR_ACTIVE_FILE);
	    }
	    else if ((sscanf(buf, " nr_unevictable %llu", &value)) == 1) {
		info->values[ZONE_NR_UNEVICTABLE] = value;
		info->flags |= (1ULL << ZONE_NR_UNEVICTABLE);
	    }
	    else if ((sscanf(buf, " nr_mlock %llu", &value)) == 1) {
		info->values[ZONE_NR_MLOCK] = value;
		info->flags |= (1ULL << ZONE_NR_MLOCK);
	    }
	    else if ((sscanf(buf, " nr_anon_pages %llu", &value)) == 1) {
		info->values[ZONE_NR_ANON_PAGES] = value;
		info->flags |= (1ULL << ZONE_NR_ANON_PAGES);
	    }
	    else if ((sscanf(buf, " nr_mapped %llu", &value)) == 1) {
		info->values[ZONE_NR_MAPPED] = value;
		info->flags |= (1ULL << ZONE_NR_MAPPED);
	    }
	    else if ((sscanf(buf, " nr_file_pages %llu", &value)) == 1) {
		info->values[ZONE_NR_FILE_PAGES] = value;
		info->flags |= (1ULL << ZONE_NR_FILE_PAGES);
	    }
	    else if ((sscanf(buf, " nr_dirty %llu", &value)) == 1) {
		info->values[ZONE_NR_DIRTY] = value;
		info->flags |= (1ULL << ZONE_NR_DIRTY);
	    }
	    else if ((sscanf(buf, " nr_writeback %llu", &value)) == 1) {
		info->values[ZONE_NR_WRITEBACK] = value;
		info->flags |= (1ULL << ZONE_NR_WRITEBACK);
	    }
	    else if ((sscanf(buf, " nr_slab_reclaimable %llu", &value)) == 1) {
		info->values[ZONE_NR_SLAB_RECLAIMABLE] = value;
		info->flags |= (1ULL << ZONE_NR_SLAB_RECLAIMABLE);
	    }
	    else if ((sscanf(buf, " nr_slab_unreclaimable %llu", &value)) == 1) {
		info->values[ZONE_NR_SLAB_UNRECLAIMABLE] = value;
		info->flags |= (1ULL << ZONE_NR_SLAB_UNRECLAIMABLE);
	    }
	    else if ((sscanf(buf, " nr_page_table_pages %llu", &value)) == 1) {
		info->values[ZONE_NR_PAGE_TABLE_PAGES] = value;
		info->flags |= (1ULL << ZONE_NR_PAGE_TABLE_PAGES);
	    }
	    else if ((sscanf(buf, " nr_kernel_stack %llu", &value)) == 1) {
		info->values[ZONE_NR_KERNEL_STACK] = value;
		info->flags |= (1ULL << ZONE_NR_KERNEL_STACK);
	    }
	    else if ((sscanf(buf, " nr_unstable %llu", &value)) == 1) {
		info->values[ZONE_NR_UNSTABLE] = value;
		info->flags |= (1ULL << ZONE_NR_UNSTABLE);
	    }
	    else if ((sscanf(buf, " nr_bounce %llu", &value)) == 1) {
		info->values[ZONE_NR_BOUNCE] = value;
		info->flags |= (1ULL << ZONE_NR_BOUNCE);
	    }
	    else if ((sscanf(buf, " nr_vmscan_write %llu", &value)) == 1) {
		info->values[ZONE_NR_VMSCAN_WRITE] = value;
		info->flags |= (1ULL << ZONE_NR_VMSCAN_WRITE);
	    }
	    else if ((sscanf(buf, " nr_vmscan_immediate_reclaim %llu", &value)) == 1) {
		info->values[ZONE_NR_VMSCAN_IMMEDIATE_RECLAIM] = value;
		info->flags |= (1ULL << ZONE_NR_VMSCAN_IMMEDIATE_RECLAIM);
	    }
	    else if ((sscanf(buf, " nr_writeback_temp %llu", &value)) == 1) {
		info->values[ZONE_NR_WRITEBACK_TEMP] = value;
		info->flags |= (1ULL << ZONE_NR_WRITEBACK_TEMP);
	    }
	    else if ((sscanf(buf, " nr_isolated_anon %llu", &value)) == 1) {
		info->values[ZONE_NR_ISOLATED_ANON] = value;
		info->flags |= (1ULL << ZONE_NR_ISOLATED_ANON);
	    }
	    else if ((sscanf(buf, " nr_isolated_file %llu", &value)) == 1) {
		info->values[ZONE_NR_ISOLATED_FILE] = value;
		info->flags |= (1ULL << ZONE_NR_ISOLATED_FILE);
	    }
	    else if ((sscanf(buf, " nr_shmem %llu", &value)) == 1) {
		info->values[ZONE_NR_SHMEM] = value;
		info->flags |= (1ULL << ZONE_NR_SHMEM);
	    }
	    else if ((sscanf(buf, " nr_shmem_hugepages %llu", &value)) == 1) {
		info->values[ZONE_NR_SHMEM_HUGEPAGES] = value;
		info->flags |= (1ULL << ZONE_NR_SHMEM_HUGEPAGES);
	    }
	    else if ((sscanf(buf, " nr_shmem_pmdmapped %llu", &value)) == 1) {
		info->values[ZONE_NR_SHMEM_PMDMAPPED] = value;
		info->flags |= (1ULL << ZONE_NR_SHMEM_PMDMAPPED);
	    }
	    else if ((sscanf(buf, " nr_file_hugepages %llu", &value)) == 1) {
		info->values[ZONE_NR_FILE_HUGEPAGES] = value;
		info->flags |= (1ULL << ZONE_NR_FILE_HUGEPAGES);
	    }
	    else if ((sscanf(buf, " nr_file_pmdmapped %llu", &value)) == 1) {
		info->values[ZONE_NR_FILE_PMDMAPPED] = value;
		info->flags |= (1ULL << ZONE_NR_FILE_PMDMAPPED);
	    }
	    else if ((sscanf(buf, " nr_dirtied %llu", &value)) == 1) {
		info->values[ZONE_NR_DIRTIED] = value;
		info->flags |= (1ULL << ZONE_NR_DIRTIED);
	    }
	    else if ((sscanf(buf, " nr_written %llu", &value)) == 1) {
		info->values[ZONE_NR_WRITTEN] = value;
		info->flags |= (1ULL << ZONE_NR_WRITTEN);
	    }
	    else if ((sscanf(buf, " nr_kernel_misc_reclaimable %llu", &value)) == 1) {
		info->values[ZONE_NR_KERNEL_MISC_RECLAIMABLE] = value;
		info->flags |= (1ULL << ZONE_NR_KERNEL_MISC_RECLAIMABLE);
	    }
	    else if ((sscanf(buf, " nr_foll_pin_acquired %llu", &value)) == 1) {
		info->values[ZONE_NR_FOLL_PIN_ACQUIRED] = value;
		info->flags |= (1ULL << ZONE_NR_FOLL_PIN_ACQUIRED);
	    }
	    else if ((sscanf(buf, " nr_foll_pin_released %llu", &value)) == 1) {
		info->values[ZONE_NR_FOLL_PIN_RELEASED] = value;
		info->flags |= (1ULL << ZONE_NR_FOLL_PIN_RELEASED);
	    }
	    else if ((sscanf(buf, " numa_hit %llu", &value)) == 1) {
		info->values[ZONE_NUMA_HIT] = value;
		info->flags |= (1ULL << ZONE_NUMA_HIT);
	    }
	    else if ((sscanf(buf, " numa_miss %llu", &value)) == 1) {
		info->values[ZONE_NUMA_MISS] = value;
		info->flags |= (1ULL << ZONE_NUMA_MISS);
	    }
	    else if ((sscanf(buf, " numa_foreign %llu", &value)) == 1) {
		info->values[ZONE_NUMA_FOREIGN] = value;
		info->flags |= (1ULL << ZONE_NUMA_FOREIGN);
	    }
	    else if ((sscanf(buf, " numa_interleave %llu", &value)) == 1) {
		info->values[ZONE_NUMA_INTERLEAVE] = value;
		info->flags |= (1ULL << ZONE_NUMA_INTERLEAVE);
	    }
	    else if ((sscanf(buf, " numa_local %llu", &value)) == 1) {
		info->values[ZONE_NUMA_LOCAL] = value;
		info->flags |= (1ULL << ZONE_NUMA_LOCAL);
	    }
	    else if ((sscanf(buf, " numa_other %llu", &value)) == 1) {
		info->values[ZONE_NUMA_OTHER] = value;
		info->flags |= (1ULL << ZONE_NUMA_OTHER);
	    }
	    else if ((sscanf(buf, " workingset_refault_anon %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_REFAULT_ANON] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_REFAULT_ANON);
	    }
	    else if ((sscanf(buf, " workingset_refault_file %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_REFAULT_FILE] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_REFAULT_FILE);
	    }
	    else if ((sscanf(buf, " workingset_refault %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_REFAULT] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_REFAULT);
	    }
	    else if ((sscanf(buf, " workingset_activate_anon %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_ACTIVATE_ANON] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_ACTIVATE_ANON);
	    }
	    else if ((sscanf(buf, " workingset_activate_file %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_ACTIVATE_FILE] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_ACTIVATE_FILE);
	    }
	    else if ((sscanf(buf, " workingset_activate %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_ACTIVATE] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_ACTIVATE);
	    }
	    else if ((sscanf(buf, " workingset_restore_anon %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_RESTORE_ANON] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_RESTORE_ANON);
	    }
	    else if ((sscanf(buf, " workingset_restore_file %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_RESTORE_FILE] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_RESTORE_FILE);
	    }
	    else if ((sscanf(buf, " workingset_nodereclaim %llu", &value)) == 1) {
		info->values[ZONE_WORKINGSET_NODERECLAIM] = value;
		info->flags |= (1ULL << ZONE_WORKINGSET_NODERECLAIM);
	    }
	    else if ((sscanf(buf, " nr_anon_transparent_hugepages %llu", &value)) == 1) {
		info->values[ZONE_NR_ANON_TRANSPARENT_HUGEPAGES] = value;
		info->flags |= (1ULL << ZONE_NR_ANON_TRANSPARENT_HUGEPAGES);
	    }
	    else if ((sscanf(buf, " nr_zspages %llu", &value)) == 1) {
		info->values[ZONE_NR_ZSPAGES] = value;
		info->flags |= (1ULL << ZONE_NR_ZSPAGES);
	    }
	    else if ((sscanf(buf, " nr_free_cma %llu", &value)) == 1) {
		info->values[ZONE_NR_FREE_CMA] = value;
		info->flags |= (1ULL << ZONE_NR_FREE_CMA);
	    }
	    else if ((sscanf(buf, " nr_swapcached %llu", &value)) == 1) {
		info->values[ZONE_NR_SWAPCACHED] = value;
		info->flags |= (1ULL << ZONE_NR_SWAPCACHED);
	    }
	    else if ((sscanf(buf, " nr_zone_inactive_file %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_INACTIVE_FILE] = value;
		info->flags |= (1ULL << ZONE_NR_ZONE_INACTIVE_FILE);
	    }
	    else if ((sscanf(buf, " nr_zone_active_file %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_ACTIVE_FILE] = value;
		info->flags |= (1ULL << ZONE_NR_ZONE_ACTIVE_FILE);
	    }
	    else if ((sscanf(buf, " nr_zone_inactive_anon %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_INACTIVE_ANON] = value;
		info->flags1 |= (1ULL << (ZONE_NR_ZONE_INACTIVE_ANON - ZONE_VALUES0));
	    }
	    else if ((sscanf(buf, " nr_zone_active_anon %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_ACTIVE_ANON] = value;
		info->flags1 |= (1ULL << (ZONE_NR_ZONE_ACTIVE_ANON - ZONE_VALUES0));
	    }
	    else if ((sscanf(buf, " nr_zone_unevictable %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_UNEVICTABLE] = value;
		info->flags1 |= (1ULL << (ZONE_NR_ZONE_UNEVICTABLE - ZONE_VALUES0));
	    }
	    else if ((sscanf(buf, " nr_zone_write_pending %llu", &value)) == 1) {
		info->values[ZONE_NR_ZONE_WRITE_PENDING] = value;
		info->flags1 |= (1ULL << (ZONE_NR_ZONE_WRITE_PENDING - ZONE_VALUES0));
	    }
	    else if (strncmp(buf, "        protection: (", 20) == 0) {
		extract_zone_protection(buf+20+1, node, zonetype, instname,
					protection_indom);
	    }
	}

	if (pernode) {
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, nodename, (void *)pernode);
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "%s: instance %s\n", __FUNCTION__, nodename);
	}

	pmdaCacheStore(indom, PMDA_CACHE_ADD, instname, (void *)perzone);
	if (info->values[ZONE_PRESENT] == 0)
	    pmdaCacheStore(indom, PMDA_CACHE_HIDE, instname, (void *)perzone);

	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "%s: instance %s\n", __FUNCTION__, instname);
    }
    fclose(fp);

    if (changed)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}
