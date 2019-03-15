/*
 * Linux zoneinfo Cluster
 *
 * Copyright (c) 2016-2017,2019 Fujitsu.
 * Copyright (c) 2017-2018 Red Hat.
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
	value = (strtoul(bp, &endp, 10) << _pm_pageshift) / 1024;
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
    int node, values;
    zoneinfo_entry_t *info;
    unsigned long long value;
    char zonetype[ZONE_NAMELEN];
    char instname[64];
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
	pmsprintf(instname, sizeof(instname), "%s::node%u", zonetype, node);
	values = 0;
	info = NULL;
	if (pmdaCacheLookupName(indom, instname, NULL, (void **)&info) < 0 ||
	    info == NULL) {
	    /* not found: allocate and add a new entry */
	    info = (zoneinfo_entry_t *)calloc(1, sizeof(zoneinfo_entry_t));
	    changed = 1;
	}
	info->node = node;
	pmsprintf(info->zone, ZONE_NAMELEN, "%s", zonetype);
	/* inner loop to extract all values for this node */
        while ((!feof(fp)) && values < ZONE_VALUES + 1 && fgets(buf, sizeof(buf), fp) != NULL) {
            if (strncmp(buf, "Node", 4) == 0){
                fseek(fp, -(long)(strlen(buf)), 1);
                break;
            }            
            if ((sscanf(buf, "  pages free %llu", &value)) == 1) {
		info->values[ZONE_FREE] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        min %llu", &value)) == 1) {
		info->values[ZONE_MIN] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        low %llu", &value)) == 1) {
		info->values[ZONE_LOW] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        high %llu", &value)) == 1) {
		info->values[ZONE_HIGH] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        scanned %llu", &value)) == 1 ||
	             (sscanf(buf, "   node_scanned %llu", &value)) == 1) {
		info->values[ZONE_SCANNED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        spanned %llu", &value)) == 1) {
		info->values[ZONE_SPANNED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        present %llu", &value)) == 1) {
		info->values[ZONE_PRESENT] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        managed %llu", &value)) == 1) {
		info->values[ZONE_MANAGED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
            else if ((sscanf(buf, "    nr_free_pages %llu", &value)) == 1) {
                info->values[ZONE_NR_FREE_PAGES] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_alloc_batch %llu", &value)) == 1) {
                info->values[ZONE_NR_ALLOC_BATCH] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_inactive_anon %llu", &value)) == 1) {
                info->values[ZONE_NR_INACTIVE_ANON] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_active_anon %llu", &value)) == 1) {
                info->values[ZONE_NR_ACTIVE_ANON] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_inactive_file %llu", &value)) == 1) {
                info->values[ZONE_NR_INACTIVE_FILE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_active_file %llu", &value)) == 1) {
                info->values[ZONE_NR_ACTIVE_FILE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_unevictable %llu", &value)) == 1) {
                info->values[ZONE_NR_UNEVICTABLE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_mlock %llu", &value)) == 1) {
                info->values[ZONE_NR_MLOCK] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_anon_pages %llu", &value)) == 1) {
                info->values[ZONE_NR_ANON_PAGES] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_mapped %llu", &value)) == 1) {
                info->values[ZONE_NR_MAPPED] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_file_pages %llu", &value)) == 1) {
                info->values[ZONE_NR_FILE_PAGES] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_dirty %llu", &value)) == 1) {
                info->values[ZONE_NR_DIRTY] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_writeback %llu", &value)) == 1) {
                info->values[ZONE_NR_WRITEBACK] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_slab_reclaimable %llu", &value)) == 1) {
                info->values[ZONE_NR_SLAB_RECLAIMABLE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_slab_unreclaimable %llu", &value)) == 1) {
                info->values[ZONE_NR_SLAB_UNRECLAIMABLE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_page_table_pages %llu", &value)) == 1) {
                info->values[ZONE_NR_PAGE_TABLE_PAGES] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_kernel_stack %llu", &value)) == 1) {
                info->values[ZONE_NR_KERNEL_STACK] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_unstable %llu", &value)) == 1) {
                info->values[ZONE_NR_UNSTABLE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_bounce %llu", &value)) == 1) {
                info->values[ZONE_NR_BOUNCE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_vmscan_write %llu", &value)) == 1) {
                info->values[ZONE_NR_VMSCAN_WRITE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_vmscan_immediate_reclaim %llu", &value)) == 1) {
                info->values[ZONE_NR_VMSCAN_IMMEDIATE_RECLAIM] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_writeback_temp %llu", &value)) == 1) {
                info->values[ZONE_NR_WRITEBACK_TEMP] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_isolated_anon %llu", &value)) == 1) {
                info->values[ZONE_NR_ISOLATED_ANON] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_isolated_file %llu", &value)) == 1) {
                info->values[ZONE_NR_ISOLATED_FILE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_shmem %llu", &value)) == 1) {
                info->values[ZONE_NR_SHMEM] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_dirtied %llu", &value)) == 1) {
                info->values[ZONE_NR_DIRTIED] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_written %llu", &value)) == 1) {
                info->values[ZONE_NR_WRITTEN] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_hit %llu", &value)) == 1) {
                info->values[ZONE_NUMA_HIT] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_miss %llu", &value)) == 1) {
                info->values[ZONE_NUMA_MISS] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_foreign %llu", &value)) == 1) {
                info->values[ZONE_NUMA_FOREIGN] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_interleave %llu", &value)) == 1) {
                info->values[ZONE_NUMA_INTERLEAVE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_local %llu", &value)) == 1) {
                info->values[ZONE_NUMA_LOCAL] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    numa_other %llu", &value)) == 1) {
                info->values[ZONE_NUMA_OTHER] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    workingset_refault %llu", &value)) == 1) {
                info->values[ZONE_WORKINGSET_REFAULT] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    workingset_activate %llu", &value)) == 1) {
                info->values[ZONE_WORKINGSET_ACTIVATE] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    workingset_nodereclaim %llu", &value)) == 1) {
                info->values[ZONE_WORKINGSET_NODERECLAIM] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_anon_transparent_hugepages %llu", &value)) == 1) {
                info->values[ZONE_NR_ANON_TRANSPARENT_HUGEPAGES] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if ((sscanf(buf, "    nr_free_cma %llu", &value)) == 1) {
                info->values[ZONE_NR_FREE_CMA] = (value << _pm_pageshift) / 1024;
                values++;
                continue;
            }
            else if (strncmp(buf, "        protection: (", 20) == 0) {
		extract_zone_protection(buf+20+1, node, zonetype,
				instname, protection_indom);
		values++;
		continue;
            }
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, instname, (void *)info);

	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "refresh_proc_zoneinfo: instance %s\n", instname);
    }
    fclose(fp);

    if (changed)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}
