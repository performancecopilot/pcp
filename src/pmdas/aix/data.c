/*
 * Data structures that define metrics and control the AIX PMDA
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
 */

#include "common.h"
#include <ctype.h>

/*
 * List of instance domains ... we expect the *_INDOM macros
 * to index into this table.
 */
pmdaIndom indomtab[] = {
    { DISK_INDOM, 0, NULL },
    { CPU_INDOM, 0, NULL },
    { NETIF_INDOM, 0, NULL }
};
int indomtab_sz = sizeof(indomtab) / sizeof(indomtab[0]);

pmdaMetric *metrictab;

method_t methodtab[] = {
    { cpu_total_init, cpu_total_prefetch, cpu_total_fetch },	// M_CPU_TOTAL
    { cpu_init, cpu_prefetch, cpu_fetch },			// M_CPU
    { disk_total_init, disk_total_prefetch, disk_total_fetch },	// M_DISK_TOTAL
    { disk_init, disk_prefetch, disk_fetch },			// M_DISK
    { netif_init, netif_prefetch, netif_fetch }			// M_NETIF
    // M_NETBUF - TODO
    // M_PROTO - TODO
    // M_MEM_TOTAL - TODO
};
int methodtab_sz = sizeof(methodtab) / sizeof(methodtab[0]);

#define CPU_OFF(field) ((int)&((perfstat_cpu_t *)0)->field)
#define CPU_TOTAL_OFF(field) ((int)&((perfstat_cpu_total_t *)0)->field)
#define DISK_OFF(field) ((int)&((perfstat_disk_t *)0)->field)
#define DISK_TOTAL_OFF(field) ((int)&((perfstat_disk_total_t *)0)->field)
#define NETIF_OFF(field) ((int)&((perfstat_netinterface_t *)0)->field)

/*
 * all metrics supported in this PMDA - one table entry for each metric
 */
metricdesc_t metricdesc[] = {

/* kernel.all.cpu.idle */
    { { PMDA_PMID(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(idle) },

/* kernel.all.cpu.user */
    { { PMDA_PMID(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(user) },

/* kernel.all.cpu.sys */
    { { PMDA_PMID(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(sys) },

/* kernel.all.cpu.wait.total */
    { { PMDA_PMID(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(wait) },

/* kernel.percpu.cpu.idle */
    { { PMDA_PMID(0,4), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(idle) },

/* kernel.percpu.cpu.user */
    { { PMDA_PMID(0,5), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(user) },

/* kernel.percpu.cpu.sys */
    { { PMDA_PMID(0,6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(sys) },

/* kernel.percpu.cpu.wait.total */
    { { PMDA_PMID(0,7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(wait) },

/* kernel.all.readch */
    { { PMDA_PMID(0,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(readch) },

/* kernel.all.writech */
    { { PMDA_PMID(0,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(readch) },

/* kernel.all.io.softintrs */
    { { PMDA_PMID(0,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(softintrs) },

/* kernel.percpu.readch */
    { { PMDA_PMID(0,11), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(readch) },

/* kernel.percpu.writech */
    { { PMDA_PMID(0,12), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, CPU_OFF(writech) },

/* kernel.percpu.cpu.intr */
    { { PMDA_PMID(0,13), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU, OFF_NOVALUES },

/* kernel.all.io.bread */
    { { PMDA_PMID(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(bread) },

/* kernel.all.io.bwrite */
    { { PMDA_PMID(0,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(bwrite) },

/* kernel.all.io.lread */
    { { PMDA_PMID(0,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(lread) },

/* kernel.all.io.lwrite */
    { { PMDA_PMID(0,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(lwrite) },

/* kernel.percpu.io.bread */
    { { PMDA_PMID(0,18), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(bread) },

/* kernel.percpu.io.bwrite */
    { { PMDA_PMID(0,19), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(bwrite) },

/* kernel.percpu.io.lread */
    { { PMDA_PMID(0,20), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(lread) },

/* kernel.percpu.io.lwrite */
    { { PMDA_PMID(0,21), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(lwrite) },

/* kernel.all.syscall */
    { { PMDA_PMID(0,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(syscall) },

/* kernel.all.pswitch */
    { { PMDA_PMID(0,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(pswitch) },

/* kernel.percpu.syscall */
    { { PMDA_PMID(0,24), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(syscall) },

/* kernel.percpu.pswitch */
    { { PMDA_PMID(0,25), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(pswitch) },

/* kernel.all.io.phread */
    { { PMDA_PMID(0,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(phread) },

/* kernel.all.io.phwrite */
    { { PMDA_PMID(0,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(phwrite) },

/* kernel.all.io.devintrs */
    { { PMDA_PMID(0,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(devintrs) },

/* kernel.percpu.io.phread */
    { { PMDA_PMID(0,29), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(phread) },

/* kernel.percpu.io.phwrite */
    { { PMDA_PMID(0,30), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(phwrite) },

/* kernel.all.cpu.intr */
    { { PMDA_PMID(0,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_CPU_TOTAL, OFF_NOVALUES },

/* kernel.all.trap */
    { { PMDA_PMID(0,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(traps) },

/* kernel.all.sysexec */
    { { PMDA_PMID(0,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(sysexec) },

/* kernel.all.sysfork */
    { { PMDA_PMID(0,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(sysfork) },

/* kernel.all.io.namei */
    { { PMDA_PMID(0,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(namei) },

/* kernel.all.sysread */
    { { PMDA_PMID(0,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(sysread) },

/* kernel.all.syswrite */
    { { PMDA_PMID(0,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(syswrite) },

/* hinv.ncpu_cfg */
    { { PMDA_PMID(0,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(ncpus_cfg) },

/* kernel.percpu.sysexec */
    { { PMDA_PMID(0,39), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(sysexec) },

/* kernel.percpu.sysfork */
    { { PMDA_PMID(0,40), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(sysfork) },

/* kernel.percpu.io.namei */
    { { PMDA_PMID(0,41), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(namei) },

/* kernel.percpu.sysread */
    { { PMDA_PMID(0,42), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(sysread) },

/* kernel.percpu.syswrite */
    { { PMDA_PMID(0,43), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU, CPU_OFF(syswrite) },

/* disk.all.read -- not available */
    { { PMDA_PMID(0,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK_TOTAL, OFF_NOVALUES },

/* disk.all.write -- not available */
    { { PMDA_PMID(0,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK_TOTAL, OFF_NOVALUES },

/* disk.all.total */
    { { PMDA_PMID(0,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK_TOTAL, DISK_TOTAL_OFF(xfers) },

/* disk.all.read_bytes */
    { { PMDA_PMID(0,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK_TOTAL, DISK_TOTAL_OFF(rblks) },

/* disk.all.write_bytes */
    { { PMDA_PMID(0,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK_TOTAL, DISK_TOTAL_OFF(wblks) },

/* disk.all.total_bytes */
    { { PMDA_PMID(0,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK_TOTAL, OFF_DERIVED },

/* disk.dev.read -- not available */
    { { PMDA_PMID(0,50), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, OFF_NOVALUES },

/* disk.dev.write -- not available */
    { { PMDA_PMID(0,51), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, OFF_NOVALUES },

/* disk.dev.total */
    { { PMDA_PMID(0,52), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, DISK_OFF(xfers) },

/* disk.dev.read_bytes */
    { { PMDA_PMID(0,53), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK, DISK_OFF(rblks) },

/* disk.dev.write_bytes */
    { { PMDA_PMID(0,54), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK, DISK_OFF(wblks) },

/* disk.dev.total_bytes */
    { { PMDA_PMID(0,55), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, M_DISK, OFF_DERIVED },

/* hinv.ncpu */
    { { PMDA_PMID(0,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_CPU_TOTAL, CPU_TOTAL_OFF(ncpus) },

/* hinv.ndisk */
    { { PMDA_PMID(0,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK_TOTAL, DISK_TOTAL_OFF(number) },

/* hinv.nnetif */
    { { PMDA_PMID(0,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, OFF_DERIVED },

/* network.interface.in.packets */
    { { PMDA_PMID(0,59), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NETIF_OFF(ipackets) },

/* network.interface.in.bytes */
    { { PMDA_PMID(0,60), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, NETIF_OFF(ibytes) },

/* network.interface.in.errors */
    { { PMDA_PMID(0,61), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NETIF_OFF(ierrors) },

/* network.interface.out.packets */
    { { PMDA_PMID(0,62), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NETIF_OFF(opackets) },

/* network.interface.out.bytes */
    { { PMDA_PMID(0,63), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, NETIF_OFF(obytes) },

/* network.interface.out.errors */
    { { PMDA_PMID(0,64), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NETIF_OFF(oerrors) },

/* network.interface.total.packets */
    { { PMDA_PMID(0,65), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, OFF_DERIVED },

/* network.interface.total.bytes */
    { { PMDA_PMID(0,66), PM_TYPE_U64, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, OFF_DERIVED }

/* remember to add trailing comma before adding more entries ... */
};
int metrictab_sz = sizeof(metricdesc) / sizeof(metricdesc[0]);

void
init_data(int domain)
{
    int			i;
    pmID		pmid;

    /*
     * Create the PMDA's metrictab[] version of the per-metric table.
     *
     * Also do domain initialization for each pmid and indom element of
     * the metricdesc[] table ... the PMDA table is fixed up in
     * libpcp_pmda
     */
    if ((metrictab = (pmdaMetric *)malloc(metrictab_sz * sizeof(pmdaMetric))) == NULL) {
	fprintf(stderr, "init_data: Error: malloc metrictab [%d] failed: %s\n",
	    metrictab_sz * sizeof(pmdaMetric), osstrerror());
	exit(1);
    }
    for (i = 0; i < metrictab_sz; i++) {
	metrictab[i].m_user = &metricdesc[i];
	metrictab[i].m_desc = metricdesc[i].md_desc;
	pmid = metricdesc[i].md_desc.pmid;
	metricdesc[i].md_desc.pmid = pmID_build(domain, pmID_cluster(pmid), pmID_item(pmid));
	if (metricdesc[i].md_desc.indom != PM_INDOM_NULL) {
	    metricdesc[i].md_desc.indom = pmInDom_build(domain, metricdesc[i].md_desc.indom);
	}
    }

    /*
     * initialize each of the methods
     */
    for (i = 0; i < methodtab_sz; i++) {
	methodtab[i].m_init(1);
    }
}
