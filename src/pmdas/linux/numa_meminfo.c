/*
 * Linux NUMA meminfo metrics cluster from sysfs
 *
 * Copyright (c) 2012,2016-2017 Red Hat.
 * Copyright (c) 2009 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <dirent.h>
#include <sys/stat.h>
#include "linux.h"
#include "proc_stat.h"
#include "linux_table.h"
#include "numa_meminfo.h"

/* sysfs file for numa meminfo */
static struct linux_table numa_meminfo_table[] = {
    { field: "MemTotal:",		maxval: 0x0 },
    { field: "MemFree:",		maxval: 0x0 },
    { field: "MemUsed:",		maxval: 0x0 },
    { field: "Active:",			maxval: 0x0 },
    { field: "Inactive:",		maxval: 0x0 },
    { field: "Active(anon):",		maxval: 0x0 },
    { field: "Inactive(anon):",		maxval: 0x0 },
    { field: "Active(file):",		maxval: 0x0 },
    { field: "Inactive(file):",		maxval: 0x0 },
    { field: "HighTotal:",		maxval: 0x0 },
    { field: "HighFree:",		maxval: 0x0 },
    { field: "LowTotal:",		maxval: 0x0 },
    { field: "LowFree:",		maxval: 0x0 },
    { field: "Unevictable:",		maxval: 0x0 },
    { field: "Mlocked:",		maxval: 0x0 },
    { field: "Dirty:",			maxval: 0x0 },
    { field: "Writeback:",		maxval: 0x0 },
    { field: "FilePages:",		maxval: 0x0 },
    { field: "Mapped:",			maxval: 0x0 },
    { field: "AnonPages:",		maxval: 0x0 },
    { field: "Shmem:",			maxval: 0x0 },
    { field: "KernelStack:",		maxval: 0x0 },
    { field: "PageTables:",		maxval: 0x0 },
    { field: "NFS_Unstable:",		maxval: 0x0 },
    { field: "Bounce:",			maxval: 0x0 },
    { field: "WritebackTmp:",		maxval: 0x0 },
    { field: "Slab:",			maxval: 0x0 },
    { field: "SReclaimable:",		maxval: 0x0 },
    { field: "SUnreclaim:",		maxval: 0x0 },
    { field: "HugePages_Total:",	maxval: 0x0 },
    { field: "HugePages_Free:",		maxval: 0x0 },
    { field: "HugePages_Surp:",		maxval: 0x0 },
    { field: NULL }
};

/* sysfs file for numastat */    
static struct linux_table numa_memstat_table[] = {
    { field: "numa_hit",		maxval: ULONGLONG_MAX },
    { field: "numa_miss",		maxval: ULONGLONG_MAX },
    { field: "numa_foreign",		maxval: ULONGLONG_MAX },
    { field: "interleave_hit",		maxval: ULONGLONG_MAX },
    { field: "local_node",		maxval: ULONGLONG_MAX },
    { field: "other_node",		maxval: ULONGLONG_MAX },
    { field: NULL }
};

int
refresh_numa_meminfo(void)
{
    int		i, refresh_bandwidth;
    FILE	*fp;
    char	buf[MAXPATHLEN];
    pmInDom	nodes = INDOM(NODE_INDOM);
    pernode_t	*np;
    static char	bandwidth_conf[PATH_MAX];
    static int	started;

    if (!started) {
	cpu_node_setup();
	for (pmdaCacheOp(nodes, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(nodes, PMDA_CACHE_WALK_NEXT)) < 0)
		break;
	    if (!pmdaCacheLookup(nodes, i, NULL, (void **)&np) || !np)
		continue;
	    if ((np->meminfo = linux_table_clone(numa_meminfo_table)) == NULL) {
		fprintf(stderr, "%s: error allocating meminfo for node%d: %s\n",
		    __FUNCTION__, np->nodeid, osstrerror());
		return -1;
	    }
	    if ((np->memstat = linux_table_clone(numa_memstat_table)) == NULL) {
		fprintf(stderr, "%s: error allocating memstat for node%d: %s\n",
		    __FUNCTION__, np->nodeid, osstrerror());
		return -1;
	    }
	}
	pmsprintf(bandwidth_conf, sizeof(bandwidth_conf),
		 "%s/linux/bandwidth.conf", pmGetConfig("PCP_PMDAS_DIR"));
	started = 1;
    }

    refresh_bandwidth = bandwidth_conf_changed(bandwidth_conf);

    for (pmdaCacheOp(nodes, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(nodes, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(nodes, i, NULL, (void **)&np) || !np)
	    continue;

	pmsprintf(buf, sizeof(buf), "%s/sys/devices/system/node/node%d/meminfo",
		linux_statspath, i);
	if ((fp = fopen(buf, "r")) != NULL) {
	    linux_table_scan(fp, np->meminfo);
	    fclose(fp);
	}

	pmsprintf(buf, sizeof(buf), "%s/sys/devices/system/node/node%d/numastat",
		linux_statspath, i);
	if ((fp = fopen(buf, "r")) != NULL) {
	    linux_table_scan(fp, np->memstat);
	    fclose(fp);
	}

	if (refresh_bandwidth)
	    np->bandwidth = 0.0;	/* reset */
    }

    /* Read NUMA bandwidth info from the bandwidth.conf file (optional) */
    if (refresh_bandwidth)
	get_memory_bandwidth_conf(bandwidth_conf);

    return 0;
}
