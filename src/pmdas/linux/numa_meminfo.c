/*
 * Linux NUMA meminfo metrics cluster from sysfs
 *
 * Copyright (c) 2012 Red Hat.
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
#include <string.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "linux_table.h"
#include "proc_cpuinfo.h"
#include "proc_stat.h"
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

int refresh_numa_meminfo(numa_meminfo_t *numa_meminfo, proc_cpuinfo_t *proc_cpuinfo, proc_stat_t *proc_stat)
{
    int i;
    FILE *fp;
    pmdaIndom *idp = PMDAINDOM(NODE_INDOM);
    static int started;

    /* First time only */
    if (!started) {
	refresh_proc_stat(proc_cpuinfo, proc_stat);

	if (!numa_meminfo->node_info)	/* may have allocated this, but failed below */
	    numa_meminfo->node_info = (nodeinfo_t *)calloc(idp->it_numinst, sizeof(nodeinfo_t));
	if (!numa_meminfo->node_info) {
	    fprintf(stderr, "%s: error allocating numa node_info: %s\n",
		__FUNCTION__, osstrerror());
	    return -1;
	}

	for (i = 0; i < idp->it_numinst; i++) {
	    numa_meminfo->node_info[i].meminfo = linux_table_clone(numa_meminfo_table);
	    if (!numa_meminfo->node_info[i].meminfo) {
		fprintf(stderr, "%s: error allocating meminfo: %s\n",
		    __FUNCTION__, osstrerror());
		return -1;
	    }
	    numa_meminfo->node_info[i].memstat = linux_table_clone(numa_memstat_table);
	    if (!numa_meminfo->node_info[i].memstat) {
		fprintf(stderr, "%s: error allocating memstat: %s\n",
		    __FUNCTION__, osstrerror());
		return -1;
	    }
	}

	numa_meminfo->node_indom = idp;
	started = 1;
    }

    /* Refresh */
    for (i = 0; i < idp->it_numinst; i++) {
	char buf[1024];

	sprintf(buf, "/sys/devices/system/node/node%d/meminfo", i);
	if ((fp = fopen(buf, "r")) != NULL) {
	    linux_table_scan(fp, numa_meminfo->node_info[i].meminfo);
	    fclose(fp);
	}

	sprintf(buf, "/sys/devices/system/node/node%d/numastat", i);
	if ((fp = fopen(buf, "r")) != NULL) {
	    linux_table_scan(fp, numa_meminfo->node_info[i].memstat);
	    fclose(fp);
	}
    }

    return 0;
}
