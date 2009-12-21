/*
 * Linux NUMA meminfo metrics cluster from sysfs
 *
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
 *
 */

#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "linux_table.h"
#include "numa_meminfo.h"

/* sysfs file for numa meminfo */
static struct linux_table numa_meminfo_table[] = {
    { "MemTotal:",	    0x0     },
    { "MemFree:",	    0x0     },
    { "MemUsed:",	    0x0     },
    { "Active:",	    0x0	    },
    { "Inactive:",	    0x0	    },
    { "Active(anon):",	    0x0	    },
    { "Inactive(anon):",    0x0	    },
    { "Active(file):",	    0x0	    },
    { "Inactive(file):",    0x0	    },
    { "HighTotal:",	    0x0     },
    { "HighFree:",	    0x0     },
    { "LowTotal:",	    0x0     },
    { "LowFree:",	    0x0     },
    { "Unevictable:",	    0x0	    },
    { "Mlocked:",	    0x0	    },
    { "Dirty:",		    0x0	    },
    { "Writeback:",	    0x0	    },
    { "FilePages:",	    0x0	    },
    { "Mapped:",	    0x0	    },
    { "AnonPages:",	    0x0	    },
    { "Shmem:",		    0x0	    },
    { "KernelStack:",	    0x0	    },
    { "PageTables:",	    0x0	    },
    { "NFS_Unstable:",	    0x0	    },
    { "Bounce:",	    0x0	    },
    { "WritebackTmp:",	    0x0	    },
    { "Slab:",		    0x0	    },
    { "SReclaimable:",	    0x0	    },
    { "SUnreclaim:",	    0x0	    },
    { "HugePages_Total:",   0x0     },
    { "HugePages_Free:",    0x0     },
    { "HugePages_Surp:",    0x0	    },
    { NULL }
};

/* sysfs file for numastat */    
static struct linux_table numa_memstat_table[] = {
    { "numa_hit",           0xffffffffffffffff },
    { "numa_miss",          0xffffffffffffffff },
    { "numa_foreign",       0xffffffffffffffff },
    { "interleave_hit",     0xffffffffffffffff },
    { "local_node",         0xffffffffffffffff },
    { "other_node",         0xffffffffffffffff },
    { NULL }
};

int refresh_numa_meminfo(numa_meminfo_t *numa_meminfo)
{
    int i;
    FILE *fp;
    pmdaIndom	*idp = &indomtab[NODE_INDOM];
    static int started = 0;

    /* First time only */
    if (!started) {
	DIR *ndir;
	struct dirent *dep;
	int max_node = -1;

	/* count number of nodes */
	if ((ndir = opendir("/sys/devices/system/node/")) == NULL) {
	    fprintf(stderr, "%s: unable to initialize: %s\n",
		    __FUNCTION__, strerror(errno));
	    return -1;
	}

	while ((dep = readdir(ndir))) {
	    int node_num;

	    if (sscanf(dep->d_name, "node%d", &node_num) == 1) {
		if (node_num >max_node)
		    max_node = node_num;
	    }
	}
	closedir(ndir);

	numa_meminfo->node_indom->it_numinst = max_node + 1;
	numa_meminfo->node_indom->it_set =
	    (pmdaInstid *)malloc(max_node * sizeof(pmdaInstid));
	if (!numa_meminfo->node_indom->it_set) {
	    fprintf(stderr, "%s: error allocating numa_indom: %s\n",
		__FUNCTION__, strerror(errno));
	    return -1;
	}

	numa_meminfo->node_info = (nodeinfo_t *)malloc(max_node * sizeof(nodeinfo_t));
	if (!numa_meminfo->node_info) {
	    fprintf(stderr, "%s: error allocating numa node_info: %s\n",
		__FUNCTION__, strerror(errno));
	    return -1;
	}
	memset(numa_meminfo->node_info, 0, max_node * sizeof(nodeinfo_t));
   
	/* Nodes are always zero-indexed and contiguous */
	for (i = 0; i <= max_node; i++) {
	    char node_name[256];

	    sprintf(node_name, "node%d", i);
	    numa_meminfo->node_indom->it_set[i].i_inst = i;
	    numa_meminfo->node_indom->it_set[i].i_name = strdup(node_name);
	    if (pmDebug & DBG_TRACE_APPL2) {
		fprintf(stderr, "%s: inst=%d, name=%s\n", __FUNCTION__, i,
				node_name);
	    }

	    numa_meminfo->node_info[i].meminfo = linux_table_clone(numa_meminfo_table);
	    if (!numa_meminfo->node_info[i].meminfo) {
		fprintf(stderr, "%s: error allocating meminfo: %s\n",
		    __FUNCTION__, strerror(errno));
		return -1;
	    }
	    numa_meminfo->node_info[i].memstat = linux_table_clone(numa_memstat_table);
	    if (!numa_meminfo->node_info[i].memstat) {
		fprintf(stderr, "%s: error allocating memstat: %s\n",
		    __FUNCTION__, strerror(errno));
		return -1;
	    }
	}

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
