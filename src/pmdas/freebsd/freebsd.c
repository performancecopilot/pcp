/*
 * FreeBSD Kernel PMDA
 *
 * Copyright (c) 2012 Red Hat.
 * Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
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

#include "pmapi.h"
#include "pmda.h"

#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <vm/vm_param.h>

#include "domain.h"
#include "freebsd.h"

/* static instances */
static pmdaInstid loadav_indom[] = {
    { 1, "1 minute" }, { 5, "5 minute" }, { 15, "15 minute" }
};

/* instance domains */
pmdaIndom indomtab[] = {
    { LOADAV_INDOM, sizeof(loadav_indom)/sizeof(loadav_indom[0]), loadav_indom },
    { CPU_INDOM, 0, NULL },
    { DISK_INDOM, 0, NULL },
    { NETIF_INDOM, 0, NULL },
};
static int indomtablen = sizeof(indomtab) / sizeof(indomtab[0]);

#define CL_SYSCTL	0
#define CL_SPECIAL	1
#define CL_DISK		2
#define CL_NETIF	3

/*
 * All the PCP metrics.
 *
 * For sysctl metrics, m_user (the first field) is set the the PCP
 * name of the metric, and during initialization this is replaced by
 * a pointer to the corresponding entry in mib[] (based on matching,
 * or prefix matching (see matchname()) the PCP name here with
 * m_pcpname[] in the mib[] entries.
 *
 * cluster map
 * CL_SYSCTL	simple sysctl() metrics, either one metric per mib, or
 *		one struct per mib
 * CL_SPECIAL	trickier sysctl() metrics involving synthesis or arithmetic
 *		or other methods
 * CL_DISK	disk metrics
 * CL_NETIF	network interface metrics
 */

static pmdaMetric metrictab[] = {
    { (void *)"hinv.ncpu",
      { PMDA_PMID(CL_SYSCTL,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { (void *)"hinv.physmem",
      { PMDA_PMID(CL_SYSCTL,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0) } },
    { (void *)"kernel.all.load",
      { PMDA_PMID(CL_SYSCTL,2), PM_TYPE_FLOAT, LOADAV_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { (void *)"kernel.all.cpu.user",
      { PMDA_PMID(CL_SYSCTL,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.all.cpu.nice",
      { PMDA_PMID(CL_SYSCTL,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.all.cpu.sys",
      { PMDA_PMID(CL_SYSCTL,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.all.cpu.intr",
      { PMDA_PMID(CL_SYSCTL,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.all.cpu.idle",
      { PMDA_PMID(CL_SYSCTL,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.percpu.cpu.user",
      { PMDA_PMID(CL_SYSCTL,8), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.percpu.cpu.nice",
      { PMDA_PMID(CL_SYSCTL,9), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.percpu.cpu.sys",
      { PMDA_PMID(CL_SYSCTL,10), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.percpu.cpu.intr",
      { PMDA_PMID(CL_SYSCTL,11), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.percpu.cpu.idle",
      { PMDA_PMID(CL_SYSCTL,12), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER, 
	PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },
    { (void *)"kernel.all.hz",
      { PMDA_PMID(CL_SYSCTL,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) } },
    { (void *)"hinv.cpu.vendor",
      { PMDA_PMID(CL_SYSCTL,15), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { (void *)"hinv.cpu.model",
      { PMDA_PMID(CL_SYSCTL,16), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { (void *)"hinv.cpu.arch",
      { PMDA_PMID(CL_SYSCTL,17), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { (void *)"swap.pagesin",
      { PMDA_PMID(CL_SYSCTL,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"swap.pagesout",
      { PMDA_PMID(CL_SYSCTL,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"swap.in",
      { PMDA_PMID(CL_SYSCTL,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"swap.in",
      { PMDA_PMID(CL_SYSCTL,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"kernel.all.pswitch",
      { PMDA_PMID(CL_SYSCTL,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"kernel.all.syscall",
      { PMDA_PMID(CL_SYSCTL,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"kernel.all.intr",
      { PMDA_PMID(CL_SYSCTL,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { (void *)"swap.length",
      { PMDA_PMID(CL_SYSCTL,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { (void *)"swap.used",
      { PMDA_PMID(CL_SYSCTL,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

    { NULL,	/* hinv.ndisk */
      { PMDA_PMID(CL_SPECIAL,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    /*
     * swap.free is the difference between sysctl variables vm.swap_total
     * and vm.swap_reserved, so it is important they are kept together
     * in mib[] below
     */
    { (void *)"swap.length",	/* swap.free */
      { PMDA_PMID(CL_SPECIAL,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* hinv.pagesize */
      { PMDA_PMID(CL_SPECIAL,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { (void *)"mem.util.all",
      { PMDA_PMID(CL_SPECIAL,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    /*
     * mem.util.used is computed from several of the vm.stats.vm.v_*_count
     * sysctl metrics, so it is important they are kept together in mib[]
     * below
     */
    { (void *)"mem.util.all",	/* mem.util.used */
      { PMDA_PMID(CL_SPECIAL,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.free",
      { PMDA_PMID(CL_SPECIAL,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.bufmem",
      { PMDA_PMID(CL_SPECIAL,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.cached",
      { PMDA_PMID(CL_SPECIAL,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.wired",
      { PMDA_PMID(CL_SPECIAL,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.active",
      { PMDA_PMID(CL_SPECIAL,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    { (void *)"mem.util.inactive",
      { PMDA_PMID(CL_SPECIAL,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },
    /*
     * mem.util.avail is computed from several of the vm.stats.vm.v_*_count
     * sysctl metrics, so it is important they are kept together in mib[]
     * below
     */
    { (void *)"mem.util.all",	/* mem.util.avail */
      { PMDA_PMID(CL_SPECIAL,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

    { NULL,	/* disk.dev.read */
      { PMDA_PMID(CL_DISK,0), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.dev.write */
      { PMDA_PMID(CL_DISK,1), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.dev.total */
      { PMDA_PMID(CL_DISK,2), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.dev.read_bytes */
      { PMDA_PMID(CL_DISK,3), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.dev.write_bytes */
      { PMDA_PMID(CL_DISK,4), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.dev.total_bytes */
      { PMDA_PMID(CL_DISK,5), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.all.read */
      { PMDA_PMID(CL_DISK,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.write */
      { PMDA_PMID(CL_DISK,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.total */
      { PMDA_PMID(CL_DISK,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.read_bytes */
      { PMDA_PMID(CL_DISK,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.all.write_bytes */
      { PMDA_PMID(CL_DISK,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.all.total_bytes */
      { PMDA_PMID(CL_DISK,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* disk.dev.blkread */
      { PMDA_PMID(CL_DISK,12), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.dev.blkwrite */
      { PMDA_PMID(CL_DISK,13), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.dev.blktotal */
      { PMDA_PMID(CL_DISK,14), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.blkread */
      { PMDA_PMID(CL_DISK,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.blkwrite */
      { PMDA_PMID(CL_DISK,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* disk.all.blktotal */
      { PMDA_PMID(CL_DISK,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

    { NULL,	/* network.interface.mtu */
      { PMDA_PMID(CL_NETIF,0), PM_TYPE_U32, NETIF_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* network.interface.up */
      { PMDA_PMID(CL_NETIF,1), PM_TYPE_U32, NETIF_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* network.interface.baudrate */
      { PMDA_PMID(CL_NETIF,2), PM_TYPE_U64, NETIF_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* network.interface.in.bytes */
      { PMDA_PMID(CL_NETIF,3), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* network.interface.in.packets */
      { PMDA_PMID(CL_NETIF,4), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.in.mcasts */
      { PMDA_PMID(CL_NETIF,5), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.in.errors */
      { PMDA_PMID(CL_NETIF,6), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.in.drops */
      { PMDA_PMID(CL_NETIF,7), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.out.bytes */
      { PMDA_PMID(CL_NETIF,8), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* network.interface.out.packets */
      { PMDA_PMID(CL_NETIF,9), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.out.mcasts */
      { PMDA_PMID(CL_NETIF,10), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.out.errors */
      { PMDA_PMID(CL_NETIF,11), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.out.collisions */
      { PMDA_PMID(CL_NETIF,12), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.total.bytes */
      { PMDA_PMID(CL_NETIF,13), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },
    { NULL,	/* network.interface.total.packets */
      { PMDA_PMID(CL_NETIF,14), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.total.mcasts */
      { PMDA_PMID(CL_NETIF,15), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { NULL,	/* network.interface.total.errors */
      { PMDA_PMID(CL_NETIF,16), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

    { NULL,	/* kernel.uname.release */
      { PMDA_PMID(CL_SPECIAL,14), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* kernel.uname.version */
      { PMDA_PMID(CL_SPECIAL,15), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* kernel.uname.sysname */
      { PMDA_PMID(CL_SPECIAL,16), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* kernel.uname.machine */
      { PMDA_PMID(CL_SPECIAL,17), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* kernel.uname.nodename */
      { PMDA_PMID(CL_SPECIAL,18), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* pmda.uname */
      { PMDA_PMID(CL_SPECIAL,20), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },
    { NULL,	/* pmda.version */
      { PMDA_PMID(CL_SPECIAL,21), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) } },

};
static int metrictablen = sizeof(metrictab) / sizeof(metrictab[0]);

/*
 * mapping between PCP metrics and sysctl metrics
 *
 * initialization note ... all elments after m_pcpname and m_name are OK
 * to be 0 or NULL
 */
typedef struct {
    const char	*m_pcpname;		/* PCP metric name or prefix (see matchname() */
    const char	*m_name;		/* sysctl metric name */
    size_t	m_miblen;		/* number of elements in m_mib[] */
    int		*m_mib;
    int		m_fetched;		/* 1 if m_data is current and valid */
    size_t	m_datalen;		/* number of bytes in m_data[] */
    void	*m_data;		/* value from sysctl */
} mib_t;
static mib_t map[] = {
    { "hinv.ncpu",		"hw.ncpu" },
    { "hinv.physmem",		"hw.physmem" },
    { "hinv.cpu.vendor",	"hw.machine" },
    { "hinv.cpu.model",		"hw.model" },
    { "hinv.cpu.arch",		"hw.machine_arch" },
    { "kernel.all.load",	"vm.loadavg" },
    { "kernel.all.hz",		"kern.clockrate" },
    { "kernel.all.cpu.*",	"kern.cp_time" },
    { "kernel.percpu.cpu.*",	"kern.cp_times" },
    { "swap.pagesin",		"vm.stats.vm.v_swappgsin" },
    { "swap.pagesout",		"vm.stats.vm.v_swappgsout" },
    { "swap.in",		"vm.stats.vm.v_swapin" },
    { "swap.out",		"vm.stats.vm.v_swapout" },
    { "kernel.all.pswitch",	"vm.stats.sys.v_swtch" },
    { "kernel.all.syscall",	"vm.stats.sys.v_syscall" },
    { "kernel.all.intr",	"vm.stats.sys.v_intr" },
    { "mem.util.bufmem",	"vfs.bufspace" },
/*
 * DO NOT MOVE next 2 entries ... see note above for swap.free
 */
    { "swap.length",		"vm.swap_total" },
    { "swap.used",		"vm.swap_reserved" },
/*
 * DO NOT MOVE next 6 entries ... see note above for mem.util.avail
 * and mem.util.used
 */
    { "mem.util.all",		"vm.stats.vm.v_page_count" },
    { "mem.util.free",		"vm.stats.vm.v_free_count" },
    { "mem.util.cached",	"vm.stats.vm.v_cache_count" },
    { "mem.util.wired",		"vm.stats.vm.v_wire_count" },
    { "mem.util.active",	"vm.stats.vm.v_active_count" },
    { "mem.util.inactive",	"vm.stats.vm.v_inactive_count" },

};
static int maplen = sizeof(map) / sizeof(map[0]);
static mib_t bad_mib = { "bad.mib", "bad.mib", 0, NULL, 0, 0, NULL };

static char	*username;
static int	isDSO = 1;	/* =0 I am a daemon */
static int	cpuhz;		/* frequency for CPU time metrics */
static int	ncpu;		/* number of cpus in kern.cp_times data */
static int	pagesize;	/* vm page size */
static struct utsname		kernel_uname;

/*
 * Fetch values from sysctl()
 *
 * Expect the result to be xpect bytes to match the PCP data size or
 * anticipated structure size, unless xpect is ==0 in which case the
 * size test is skipped.
 */
static int
do_sysctl(mib_t *mp, size_t xpect)
{
    /*
     * Note zero trip if mp->m_data and mp->datalen are already valid
     * and current
     */
    for ( ; mp->m_fetched == 0; ) {
	int	sts;
	sts = sysctl(mp->m_mib, (u_int)mp->m_miblen, mp->m_data, &mp->m_datalen, NULL, 0);
	if (pmDebugOptions.appl0) {
	    fprintf(stderr, "sysctl(%s%s) -> %d (datalen=%d)\n", mp->m_name, mp->m_data == NULL ? " firstcall" : "", sts, (int)mp->m_datalen);
	}
	if (sts == 0 && mp->m_data != NULL) {
	    mp->m_fetched = 1;
	    break;
	}
	if ((sts == -1 && errno == ENOMEM) || (sts == 0 && mp->m_data == NULL)) {
	    /* first call for this one, or data changed size */
	    mp->m_data = realloc(mp->m_data, mp->m_datalen);
	    if (mp->m_data == NULL) {
		fprintf(stderr, "Error: %s: buffer alloc failed for sysctl metric \"%s\"\n", mp->m_pcpname, mp->m_name);
		pmNoMem("do_sysctl", mp->m_datalen, PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	}
	else
	    return -errno;
    }
    if (xpect > 0 && mp->m_datalen != xpect) {
	fprintf(stderr, "Error: %s: sysctl(%s) datalen=%d not %d!\n", mp->m_pcpname, mp->m_name, (int)mp->m_datalen, (int)xpect);
	return 0;
    }
    return mp->m_datalen;
}


/*
 * Callback provided to pmdaFetch ... come here once per metric-instance
 * pair in each pmFetch().
 */
static int
freebsd_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int			sts = PM_ERR_PMID;
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    mib_t		*mp;

    mp = (mib_t *)mdesc->m_user;
    if (cluster == CL_SYSCTL) {
	/* sysctl() simple cases */
	switch (item) {
	    /* 32-bit integer values */
	    case 0:		/* hinv.ncpu */
	    case 18:		/* swap.pagesin */
	    case 19:		/* swap.pagesout */
	    case 20:		/* swap.in */
	    case 21:		/* swap.out */
	    case 22:		/* kernel.all.pswitch */
	    case 23:		/* kernel.all.syscall */
	    case 24:		/* kernel.all.intr */
		sts = do_sysctl(mp, sizeof(atom->ul));
		if (sts > 0) {
		    atom->ul = *((__uint32_t *)mp->m_data);
		    sts = 1;
		}
		break;

	    /* 64-bit integer values */
	    case 25:		/* swap.length */
	    case 26:		/* swap.used */
		sts = do_sysctl(mp, sizeof(atom->ull));
		if (sts > 0) {
		    atom->ull = *((__uint64_t *)mp->m_data);
		    sts = 1;
		}
		break;

	    /* long integer value */
	    case 1:		/* hinv.physmem */
		sts = do_sysctl(mp, sizeof(long));
		if (sts > 0) {
		    /* stsctl() returns bytes, convert to MBYTES */
		    atom->ull = (*((long *)mp->m_data))/(1024*1024);
		    sts = 1;
		}
		break;

	    /* string values */
	    case 15:		/* hinv.cpu.vendor */
	    case 16:		/* hinv.cpu.model */
	    case 17:		/* hinv.cpu.arch */
		sts = do_sysctl(mp, (size_t)0);
		if (sts > 0) {
		    atom->cp = (char *)mp->m_data;
		    sts = 1;
		}
		break;

	    /* structs and aggregates */
	    case 2:		/* kernel.all.load */
		sts = do_sysctl(mp, sizeof(struct loadavg));
		if (sts > 0) {
		    int			i;
		    struct loadavg	*lp = (struct loadavg *)mp->m_data;
		    if (inst == 1)
			i = 0;
		    else if (inst == 5)
			i = 1;
		    else if (inst == 15)
			i = 2;
		    else
			return PM_ERR_INST;
		    atom->f = (float)((double)lp->ldavg[i] / lp->fscale);
		    sts = 1;
		}
		break;

	    case 3:		/* kernel.all.cpu.user */
	    case 4:		/* kernel.all.cpu.nice */
	    case 5:		/* kernel.all.cpu.sys */
	    case 6:		/* kernel.all.cpu.intr */
	    case 7:		/* kernel.all.cpu.idle */
		/*
		 * assume this declaration is correct ...
		 * long pc_cp_time[CPUSTATES];	...
		 * from /usr/include/sys/pcpu.h
		 */
		sts = do_sysctl(mp, CPUSTATES*sizeof(long));
		if (sts > 0) {
		    /*
		     * PMID assignment is important in the "-3" below so
		     * that metrics map to consecutive elements of the
		     * returned value in the order defined for CPUSTATES,
		     * i.e. CP_USER, CP_NICE, CP_SYS, CP_INTR and
		     * CP_IDLE
		     */
		    atom->ull = 1000*((__uint64_t)((long *)mp->m_data)[item-3])/cpuhz;
		    sts = 1;
		}
		break;

	    case 8:		/* kernel.percpu.cpu.user */
	    case 9:		/* kernel.percpu.cpu.nice */
	    case 10:		/* kernel.percpu.cpu.sys */
	    case 11:		/* kernel.percpu.cpu.intr */
	    case 12:		/* kernel.percpu.cpu.idle */
		sts = do_sysctl(mp, ncpu*CPUSTATES*sizeof(atom->ull));
		if (sts > 0) {
		    /*
		     * PMID assignment is important in the "-8" below so
		     * that metrics map to consecutive elements of the
		     * returned value in the order defined for CPUSTATES,
		     * i.e. CP_USER, CP_NICE, CP_SYS, CP_INTR and
		     * CP_IDLE, and then there is one such set for each
		     * CPU up to the maximum number of CPUs installed in
		     * the system.
		     */
		    atom->ull = 1000*((__uint64_t *)mp->m_data)[inst * CPUSTATES + item-8]/cpuhz;
		    sts = 1;
		}
		break;

	    case 13:		/* kernel.all.hz */
		sts = do_sysctl(mp, sizeof(struct clockinfo));
		if (sts > 0) {
		    struct clockinfo	*cp = (struct clockinfo *)mp->m_data;
		    atom->ul = cp->hz;
		    sts = 1;
		}
		break;

	}
    }
    else if (cluster == CL_SPECIAL) {
	/* special cases */
	static char 	uname_string[sizeof(kernel_uname)+5];

	switch (item) {
	    case 0:	/* hinv.ndisk */
		refresh_disk_metrics();
		atom->ul = pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_SIZE_ACTIVE);
		sts = 1;
		break;

	    case 1:	/* swap.free */
		/* first vm.swap_total */
		sts = do_sysctl(mp, sizeof(atom->ull));
		if (sts > 0) {
		    atom->ull = *((__uint64_t *)mp->m_data);
		    /*
		     * now subtract vm.swap_reserved ... assumes consecutive
		     * mib[] entries
		     */
		    mp++;
		    sts = do_sysctl(mp, sizeof(atom->ull));
		    if (sts > 0) {
			atom->ull -= *((__uint64_t *)mp->m_data);
			sts = 1;
		    }
		}
		break;

	    case 3:	/* hinv.pagesize */
		atom->ul = pagesize;
		sts = 1;
		break;

	    case 4:	/* mem.util.all */
	    case 6:	/* mem.util.free */
	    case 8:	/* mem.util.cached */
	    case 9:	/* mem.util.wired */
	    case 10:	/* mem.util.active */
	    case 11:	/* mem.util.inactive */
		sts = do_sysctl(mp, sizeof(atom->ul));
		if (sts > 0) {
		    atom->ul = *((__uint32_t *)mp->m_data) * (pagesize / 1024);
		    sts = 1;
		}
		break;

	    case 7:	/* mem.util.bufmem */
		sts = do_sysctl(mp, sizeof(atom->ul));
		if (sts > 0) {
		    atom->ul = *((__uint32_t *)mp->m_data) / 1024;
		    sts = 1;
		}
		break;

	    case 5:	/* mem.util.used */
		/*
		 * mp-> v_page_count entry in mib[]
		 * assuming consecutive mib[] entries, we want
		 * v_page_count mp[0] - v_free_count mp[1] -
		 * v_cache_count mp[2] - v_inactive_count mp[5]
		 */
		sts = do_sysctl(mp, sizeof(atom->ul));
		if (sts > 0) {
		    atom->ul = *((__uint32_t *)mp->m_data);
		    sts = do_sysctl(&mp[1], sizeof(atom->ul));
		    if (sts > 0) {
			atom->ul -= *((__uint32_t *)mp[1].m_data);
			sts = do_sysctl(&mp[2], sizeof(atom->ul));
			if (sts > 0) {
			    atom->ul -= *((__uint32_t *)mp[2].m_data);
			    sts = do_sysctl(&mp[5], sizeof(atom->ul));
			    if (sts > 0) {
				atom->ul -= *((__uint32_t *)mp[5].m_data);
				atom->ul *= (pagesize / 1024);
				sts = 1;
			    }
			}
		    }
		}
		break;

	    case 12:	/* mem.util.avail */
		/*
		 * mp-> v_page_count entry in mib[]
		 * assuming consecutive mib[] entries, we want
		 * v_free_count mp[1] + v_cache_count mp[2] +
		 * v_inactive_count mp[5]
		 */
		sts = do_sysctl(&mp[1], sizeof(atom->ul));
		if (sts > 0) {
		    atom->ul = *((__uint32_t *)mp[1].m_data);
		    sts = do_sysctl(&mp[2], sizeof(atom->ul));
		    if (sts > 0) {
			atom->ul += *((__uint32_t *)mp[2].m_data);
			sts = do_sysctl(&mp[5], sizeof(atom->ul));
			if (sts > 0) {
			    atom->ul += *((__uint32_t *)mp[5].m_data);
			    atom->ul *= (pagesize / 1024);
			    sts = 1;
			}
		    }
		}
		break;

	    case 14:	/* kernel.uname.release */
		atom->cp = kernel_uname.release;
		sts = 1;
		break;

	    case 15:	/* kernel.uname.version */
		atom->cp = kernel_uname.version;
		sts = 1;
		break;

	    case 16:	/* kernel.uname.sysname */
		atom->cp = kernel_uname.sysname;
		sts = 1;
		break;

	    case 17:	/* kernel.uname.machine */
		atom->cp = kernel_uname.machine;
		sts = 1;
		break;

	    case 18:	/* kernel.uname.nodename */
		atom->cp = kernel_uname.nodename;
		sts = 1;
		break;

	    case 20: /* pmda.uname */
		pmsprintf(uname_string, sizeof(uname_string), "%s %s %s %s %s",
		    kernel_uname.sysname, 
		    kernel_uname.nodename,
		    kernel_uname.release,
		    kernel_uname.version,
		    kernel_uname.machine);
		atom->cp = uname_string;
		sts = 1;
		break;

	    case 21: /* pmda.version */
		atom->cp = pmGetConfig("PCP_VERSION");
		sts = 1;
		break;

	}
    }
    else if (cluster == CL_DISK) {
	/* disk metrics */
	sts = do_disk_metrics(mdesc, inst, atom);
    }
    else if (cluster == CL_NETIF) {
	/* network interface metrics */
	sts = do_netif_metrics(mdesc, inst, atom);
    }

    return sts;
}

/*
 * wrapper for pmdaFetch ... force value caches to be reloaded if needed,
 * then do the fetch
 */
static int
freebsd_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i;
    int		done_disk = 0;
    int		done_netif = 0;

    for (i = 0; i < maplen; i++) {
	map[i].m_fetched = 0;
    }

    /*
     * pre-fetch all metrics if needed, and update instance domains if
     * they have changed
     */
    for (i = 0; !done_disk && !done_netif && i < numpmid; i++) {
	if (pmID_cluster(pmidlist[i]) == CL_DISK) {
	    refresh_disk_metrics();
	    done_disk = 1;
	}
	else if (pmID_cluster(pmidlist[i]) == CL_NETIF) {
	    refresh_netif_metrics();
	    done_netif = 1;
	}
    }

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * wrapper for pmdaInstance ... refresh required instance domain first
 */
static int
freebsd_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    /*
     * indomtab[] instance names and ids are not used for some indoms,
     * ensure pmdaCache is current
     */
    if (indom == indomtab[DISK_INDOM].it_indom)
	refresh_disk_metrics();
    if (indom == indomtab[NETIF_INDOM].it_indom)
	refresh_netif_metrics();

    return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * PCP metric name matching for linking metrictab[] entries to mib[]
 * entries.
 *
 * Return 1 if prefix[] is equal to, or a prefix of name[]
 *
 * prefix[] of the form "a.bc" or "a.bc*" matches a name[] like "a.bc"
 * or "a.bcanything", to improve readability of the initializers in
 * mib[], and asterisk is a "match all" special case, so "a.b.*" matches
 * "a.b.anything"
 */
static int
matchname(const char *prefix, const char *name)
{
    while (*prefix != '\0' && *name != '\0' && *prefix == *name) {
	prefix++;
	name++;
    }
    if (*prefix == '\0' || *prefix == '*')
	return 1;
    else
	return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 * 
 * Do mapping from sysclt(3) names to mibs.
 * Collect some global constants.
 * Build the system-specific, but not dynamic, instance domains,
 * e.g. CPU_INDOM.
 * Initialize the kernel memory reader.
 */
void 
freebsd_init(pmdaInterface *dp)
{
    int			i;
    int			m;
    int			sts;
    struct clockinfo	clockrates;
    size_t		sz;
    int			mib[CTL_MAXNAME];	/* enough for longest mib key */
    char		iname[16];		/* enough for cpuNN.. */

    if (isDSO) {
	char	mypath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(mypath, sizeof(mypath), "%s%c" "freebsd" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_5, "freebsd DSO", mypath);
    } else {
	pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.four.fetch = freebsd_fetch;
    dp->version.four.instance = freebsd_instance;

    pmdaSetFetchCallBack(dp, freebsd_fetchCallBack);

    pmdaInit(dp, indomtab, indomtablen, metrictab, metrictablen);

    /*
     * Link metrictab[] entries via m_user to map[] entries based on
     * matching sysctl(3) name
     *
     * also translate the sysctl(3) name to a mib
     */
    for (m = 0; m < metrictablen; m++) {
	if (metrictab[m].m_user == NULL) {
	    /* not using sysctl(3) */
	    continue;
	}
	for (i = 0; i < maplen; i++) {
	    if (matchname(map[i].m_pcpname, (char *)metrictab[m].m_user)) {
		if (map[i].m_mib == NULL) {
		    /*
		     * multiple metrictab[] entries may point to the same
		     * mib[] entry, but this is the first time for this
		     * mib[] entry ...
		     */
		    map[i].m_miblen = sizeof(mib);
		    sts = sysctlnametomib(map[i].m_name, mib, &map[i].m_miblen);
		    if (sts == 0) {
			map[i].m_mib = (int *)malloc(map[i].m_miblen*sizeof(map[i].m_mib[0]));
			if (map[i].m_mib == NULL) {
			    fprintf(stderr, "Error: %s (%s): failed mib alloc for sysctl metric \"%s\"\n", map[i].m_pcpname, pmIDStr(metrictab[m].m_desc.pmid), map[i].m_name);
			    pmNoMem("freebsd_init: mib", map[i].m_miblen*sizeof(map[i].m_mib[0]), PM_FATAL_ERR);
			    /*NOTREACHED*/
			}
			memcpy(map[i].m_mib, mib, map[i].m_miblen*sizeof(map[i].m_mib[0]));
		    }
		    else {
			fprintf(stderr, "Error: %s (%s): failed sysctlnametomib(\"%s\", ...): %s\n", map[i].m_pcpname, pmIDStr(metrictab[m].m_desc.pmid), map[i].m_name, pmErrStr(-errno));
			metrictab[m].m_user = (void *)&bad_mib;
		    }
		}
		if (pmDebugOptions.appl0) {
		    int	p;
		    fprintf(stderr, "Info: %s (%s): sysctl metric \"%s\" -> ", (char *)metrictab[m].m_user, pmIDStr(metrictab[m].m_desc.pmid), map[i].m_name);
		    for (p = 0; p < map[i].m_miblen; p++) {
			if (p > 0) fputc('.', stderr);
			fprintf(stderr, "%d", map[i].m_mib[p]);
		    }
		    fputc('\n', stderr);
		}
		metrictab[m].m_user = (void *)&map[i];
		break;
	    }
	}
	if (i == maplen) {
	    fprintf(stderr, "Error: %s (%s): cannot match name in sysctl map[]\n", (char *)metrictab[m].m_user, pmIDStr(metrictab[m].m_desc.pmid));
	    metrictab[m].m_user = (void *)&bad_mib;
	}
    }

    /*
     * Collect some global constants needed later ...
     */
    sz = sizeof(clockrates);
    sts = sysctlbyname("kern.clockrate", &clockrates, &sz, NULL, 0);
    if (sts < 0) {
	fprintf(stderr, "Fatal Error: sysctlbyname(\"kern.clockrate\", ...) failed: %s\n", pmErrStr(-errno));
	exit(1);
    }
    cpuhz = clockrates.stathz;
    if (pmDebugOptions.appl0)
	fprintf(stderr, "Info: CPU time \"hz\" = %d\n", cpuhz);

    sts = sysctlbyname("kern.cp_times", NULL, &sz, NULL, 0);
    if (sts < 0) {
	fprintf(stderr, "Fatal Error: sysctlbyname(\"kern.cp_times\", ...) failed: %s\n", pmErrStr(-errno));
	exit(1);
    }
    /*
     * see note below when fetching kernel.percpu.cpu.* metrics to
     * explain this
     */
    ncpu = sz / (CPUSTATES * sizeof(__uint64_t));
    if (pmDebugOptions.appl0)
	fprintf(stderr, "Info: ncpu = %d\n", ncpu);

    sz = sizeof(pagesize);
    sts = sysctlbyname("hw.pagesize", &pagesize, &sz, NULL, 0);
    if (sts < 0) {
	fprintf(stderr, "Fatal Error: sysctlbyname(\"hw.pagesize\", ...) failed: %s\n", pmErrStr(-errno));
	exit(1);
    }
    if (pmDebugOptions.appl0)
	fprintf(stderr, "Info: VM pagesize = %d\n", pagesize);

    uname(&kernel_uname);

    /*
     * Build some instance domains ...
     */
    indomtab[CPU_INDOM].it_numinst = ncpu;
    indomtab[CPU_INDOM].it_set = (pmdaInstid *)malloc(ncpu * sizeof(pmdaInstid));
    if (indomtab[CPU_INDOM].it_set == NULL) {
	pmNoMem("freebsd_init: CPU_INDOM it_set", ncpu * sizeof(pmdaInstid), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    for (i = 0; i < ncpu; i++) {
	indomtab[CPU_INDOM].it_set[i].i_inst = i;
	pmsprintf(iname, sizeof(iname), "cpu%d", i);
	indomtab[CPU_INDOM].it_set[i].i_name = strdup(iname);
	if (indomtab[CPU_INDOM].it_set[i].i_name == NULL) {
	    pmNoMem("freebsd_init: CPU_INDOM strdup iname", strlen(iname), PM_FATAL_ERR);
	    /*NOTREACHED*/
	}
    }
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmGetProgname());
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -U username  user account to run under (default \"pcp\")\n"
	  "\nExactly one of the following options may appear:\n"
	  "  -i port      expect PMCD to connect on given inet port (number or name)\n"
	  "  -p           expect PMCD to supply stdin/stdout (pipe)\n"
	  "  -u socket    expect PMCD to connect on given unix domain socket\n"
	  "  -6 port      expect PMCD to connect on given ipv6 port (number or name)\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			c, err = 0;
    int			sep = pmPathSeparator();
    pmdaInterface	dispatch;
    char		mypath[MAXPATHLEN];

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "freebsd" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_5, pmGetProgname(), FREEBSD,
		"freebsd.log", mypath);

    while ((c = pmdaGetOpt(argc, argv, "D:d:i:l:pu:U:6:?", &dispatch, &err)) != EOF) {
	switch(c) {
	case 'U':
	    username = optarg;
	    break;
	default:
	    err++;
	}
    }
    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    freebsd_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
