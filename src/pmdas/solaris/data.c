/*
 * Data structures that define metrics and control the Solaris PMDA
 *
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Max Matveev.  All Rights Reserved.
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
#include "netmib2.h"
#include <ctype.h>
#include <libzfs.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

method_t methodtab[] = {
    { "sysinfo", sysinfo_init, sysinfo_prefetch, sysinfo_fetch },
    { "disk",  disk_init, disk_prefetch, disk_fetch },
    { "netmib2", netmib2_init, netmib2_refresh, netmib2_fetch },
    { "zpool", zpool_init, zpool_refresh, zpool_fetch },
    { "zfs", zfs_init, zfs_refresh, zfs_fetch },
    { "zpool_vdev", zpool_perdisk_init, zpool_perdisk_refresh, zpool_perdisk_fetch },
    { "netlink", netlink_init, netlink_refresh, netlink_fetch },
    { "kvm", kvm_init, kvm_refresh, kvm_fetch },
    { "zfs_arc", NULL, arcstats_refresh, arcstats_fetch },
    { "filesystem", vnops_init, vnops_refresh, vnops_fetch }
};

const int methodtab_sz = ARRAY_SIZE(methodtab);
static pmdaInstid prefetch_insts[ARRAY_SIZE(methodtab)];

static pmdaInstid loadavg_insts[] = {
	{1, "1 minute"},
	{5, "5 minute"},
	{15, "15 minute"}
};

pmdaMetric *metrictab;

#define SYSINFO_OFF(field) ((ptrdiff_t)&((cpu_stat_t *)0)->cpu_sysinfo.field)
#define KSTAT_IO_OFF(field) ((ptrdiff_t)&((kstat_io_t *)0)->field)
#define VDEV_OFFSET(field) ((ptrdiff_t)&((vdev_stat_t *)0)->field)
#define NM2_UDP_OFFSET(field) ((ptrdiff_t)&(nm2_udp.field))
#define NM2_NETIF_OFFSET(field) ((ptrdiff_t)&((nm2_netif_stats_t *)0)->field)
#define FSF_STAT_OFFSET(field) ((ptrdiff_t)&((fsf_stat_t *)0)->field)

/*
 * all metrics supported in this PMDA - one table entry for each metric
 */
metricdesc_t metricdesc[] = {

    { "kernel.all.cpu.idle",
      { PMDA_PMID(SCLR_SYSINFO,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_IDLE]) },

    { "kernel.all.cpu.user",
      { PMDA_PMID(SCLR_SYSINFO,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_USER]) },

    { "kernel.all.cpu.sys",
      { PMDA_PMID(SCLR_SYSINFO,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_KERNEL]) },

    { "kernel.all.cpu.wait.total",
      { PMDA_PMID(SCLR_SYSINFO,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_WAIT]) },

    { "kernel.percpu.cpu.idle",
      { PMDA_PMID(SCLR_SYSINFO,4), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_IDLE]) },

    { "kernel.percpu.cpu.user",
      { PMDA_PMID(SCLR_SYSINFO,5), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_USER]) },

    { "kernel.percpu.cpu.sys",
      { PMDA_PMID(SCLR_SYSINFO,6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_KERNEL]) },

    { "kernel.percpu.cpu.wait.total",
      { PMDA_PMID(SCLR_SYSINFO,7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(cpu[CPU_WAIT]) },

    { "kernel.all.cpu.wait.io",
      { PMDA_PMID(SCLR_SYSINFO,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_IO]) },

    { "kernel.all.cpu.wait.pio",
      { PMDA_PMID(SCLR_SYSINFO,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_PIO]) },

    { "kernel.all.cpu.wait.swap",
      { PMDA_PMID(SCLR_SYSINFO,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_SWAP]) },

    { "kernel.percpu.cpu.wait.io",
      { PMDA_PMID(SCLR_SYSINFO,11), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_IO]) },

    { "kernel.percpu.cpu.wait.pio",
      { PMDA_PMID(SCLR_SYSINFO,12), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_PIO]) },

    { "kernel.percpu.cpu.wait.swap",
      { PMDA_PMID(SCLR_SYSINFO,13), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, SYSINFO_OFF(wait[W_SWAP]) },

    { "kernel.all.io.bread",
      { PMDA_PMID(SCLR_SYSINFO,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(bread) },

    { "kernel.all.io.bwrite",
      { PMDA_PMID(SCLR_SYSINFO,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(bwrite) },

    { "kernel.all.io.lread",
      { PMDA_PMID(SCLR_SYSINFO,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(lread) },

    { "kernel.all.io.lwrite",
      { PMDA_PMID(SCLR_SYSINFO,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(lwrite) },

    { "kernel.percpu.io.bread",
      { PMDA_PMID(SCLR_SYSINFO,18), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(bread) },

    { "kernel.percpu.io.bwrite",
      { PMDA_PMID(SCLR_SYSINFO,19), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(bwrite) },

    { "kernel.percpu.io.lread",
      { PMDA_PMID(SCLR_SYSINFO,20), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(lread) },

    { "kernel.percpu.io.lwrite",
      { PMDA_PMID(SCLR_SYSINFO,21), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(lwrite) },

    { "kernel.all.syscall",
      { PMDA_PMID(SCLR_SYSINFO,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(syscall) },

    { "kernel.all.pswitch",
      { PMDA_PMID(SCLR_SYSINFO,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(pswitch) },

    { "kernel.percpu.syscall",
      { PMDA_PMID(SCLR_SYSINFO,24), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(syscall) },

    { "kernel.percpu.pswitch",
      { PMDA_PMID(SCLR_SYSINFO,25), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(pswitch) },

    { "kernel.all.io.phread",
      { PMDA_PMID(SCLR_SYSINFO,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(phread) },

    { "kernel.all.io.phwrite",
      { PMDA_PMID(SCLR_SYSINFO,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(phwrite) },

    { "kernel.all.io.intr",
      { PMDA_PMID(SCLR_SYSINFO,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(intr) },

    { "kernel.percpu.io.phread",
      { PMDA_PMID(SCLR_SYSINFO,29), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(phread) },

    { "kernel.percpu.io.phwrite",
      { PMDA_PMID(SCLR_SYSINFO,30), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(phwrite) },

    { "kernel.percpu.io.intr",
      { PMDA_PMID(SCLR_SYSINFO,31), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(intr) },

    { "kernel.all.trap",
      { PMDA_PMID(SCLR_SYSINFO,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(trap) },

    { "kernel.all.sysexec",
      { PMDA_PMID(SCLR_SYSINFO,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysexec) },

    { "kernel.all.sysfork",
      { PMDA_PMID(SCLR_SYSINFO,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysfork) },

    { "kernel.all.sysvfork",
      { PMDA_PMID(SCLR_SYSINFO,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysvfork) },

    { "kernel.all.sysread",
      { PMDA_PMID(SCLR_SYSINFO,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysread) },

    { "kernel.all.syswrite",
      { PMDA_PMID(SCLR_SYSINFO,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(syswrite) },

    { "kernel.percpu.trap",
      { PMDA_PMID(SCLR_SYSINFO,38), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(trap) },

    { "kernel.percpu.sysexec",
      { PMDA_PMID(SCLR_SYSINFO,39), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysexec) },

    { "kernel.percpu.sysfork",
      { PMDA_PMID(SCLR_SYSINFO,40), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysfork) },

    { "kernel.percpu.sysvfork",
      { PMDA_PMID(SCLR_SYSINFO,41), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysvfork) },

    { "kernel.percpu.sysread",
      { PMDA_PMID(SCLR_SYSINFO,42), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(sysread) },

    { "kernel.percpu.syswrite",
      { PMDA_PMID(SCLR_SYSINFO,43), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, SYSINFO_OFF(syswrite) },

    { "disk.all.read",
      { PMDA_PMID(SCLR_DISK,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(reads) },

    { "disk.all.write",
      { PMDA_PMID(SCLR_DISK,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(writes) },

    { "disk.all.total",
      { PMDA_PMID(SCLR_DISK,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, -1},

    { "disk.all.read_bytes",
      { PMDA_PMID(SCLR_DISK,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, KSTAT_IO_OFF(nread) },

    { "disk.all.write_bytes",
      { PMDA_PMID(SCLR_DISK,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, KSTAT_IO_OFF(nwritten) },

    { "disk.all.total_bytes",
      { PMDA_PMID(SCLR_DISK,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, -1},

    { "disk.dev.read",
      { PMDA_PMID(SCLR_DISK,10), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(reads) },


    { "disk.dev.write",
      { PMDA_PMID(SCLR_DISK,11), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(writes) },

    { "disk.dev.total",
      { PMDA_PMID(SCLR_DISK,12), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, -1},

    { "disk.dev.read_bytes",
      { PMDA_PMID(SCLR_DISK,13), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, KSTAT_IO_OFF(nread) },

    { "disk.dev.write_bytes",
      { PMDA_PMID(SCLR_DISK,14), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, KSTAT_IO_OFF(nwritten) },

    { "disk.dev.total_bytes",
      { PMDA_PMID(SCLR_DISK,15), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, -1},

    { "hinv.ncpu",
      { PMDA_PMID(SCLR_SYSINFO,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1},

    { "hinv.ndisk",
      { PMDA_PMID(SCLR_DISK,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1},

    { "hinv.nfilesys",
      { PMDA_PMID(SCLR_FILESYS,1023), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1},

    { "pmda.uname",
      { PMDA_PMID(SCLR_SYSINFO,107), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1 },

    { "hinv.pagesize",
      { PMDA_PMID(SCLR_SYSINFO,108), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, -1 },

    { "hinv.physmem",
      { PMDA_PMID(SCLR_SYSINFO,109), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_MBYTE, 0, 0)
      }, -1 },

    { "zpool.capacity",
      { PMDA_PMID(SCLR_ZPOOL,2), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_space) },
    { "zpool.used",
      { PMDA_PMID(SCLR_ZPOOL,3), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_alloc) },
    { "zpool.checksum_errors",
      { PMDA_PMID(SCLR_ZPOOL,4), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_checksum_errors) },
    { "zpool.self_healed",
      { PMDA_PMID(SCLR_ZPOOL,5), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_self_healed) },
    { "zpool.in.bytes",
      { PMDA_PMID(SCLR_ZPOOL,6), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_bytes[ZIO_TYPE_READ]) },
    { "zpool.in.ops",
      { PMDA_PMID(SCLR_ZPOOL,7), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_READ]) },
    { "zpool.in.errors",
      { PMDA_PMID(SCLR_ZPOOL,8), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_read_errors) },
    { "zpool.out.bytes",
      { PMDA_PMID(SCLR_ZPOOL,9), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_bytes[ZIO_TYPE_WRITE]) },
    { "zpool.out.ops",
      { PMDA_PMID(SCLR_ZPOOL,10), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_WRITE]) },
    { "zpool.out.errors",
      { PMDA_PMID(SCLR_ZPOOL,11), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_write_errors) },
    { "zpool.ops.noops",
      { PMDA_PMID(SCLR_ZPOOL,12), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_NULL]) },
    { "zpool.ops.ioctls",
      { PMDA_PMID(SCLR_ZPOOL,13), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_WRITE]) },
    { "zpool.ops.claims",
      { PMDA_PMID(SCLR_ZPOOL,14), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_WRITE]) },
    { "zpool.ops.frees",
      { PMDA_PMID(SCLR_ZPOOL,15), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_ops[ZIO_TYPE_WRITE]) },
    { "zfs.used.total",
      { PMDA_PMID(SCLR_ZFS,10), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USED },
    { "zfs.available",
      { PMDA_PMID(SCLR_ZFS,0), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_AVAILABLE },
    { "zfs.quota",
      { PMDA_PMID(SCLR_ZFS,1), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_QUOTA },
    { "zfs.reservation",
      { PMDA_PMID(SCLR_ZFS,2), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_RESERVATION },
    { "zfs.compression",
      { PMDA_PMID(SCLR_ZFS,3), PM_TYPE_DOUBLE, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, ZFS_PROP_COMPRESSRATIO },
    { "zfs.copies",
      { PMDA_PMID(SCLR_ZFS,4), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, ZFS_PROP_COPIES },
    { "zfs.used.byme",
      { PMDA_PMID(SCLR_ZFS,11), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USEDDS },
    { "zfs.used.bysnapshots",
      { PMDA_PMID(SCLR_ZFS,12), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USEDSNAP },
    { "zfs.used.bychildren",
      { PMDA_PMID(SCLR_ZFS,13), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USEDCHILD },

    { "network.udp.ipackets",
      { PMDA_PMID(SCLR_NETIF,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(ipackets) },
    { "network.udp.opackets",
      { PMDA_PMID(SCLR_NETIF,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(opackets) },
    { "network.udp.ierrors",
      { PMDA_PMID(SCLR_NETIF,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(ierrors) },
    { "network.udp.oerrors",
      { PMDA_PMID(SCLR_NETIF,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(oerrors) },

    { "network.interface.mtu",
      { PMDA_PMID(SCLR_NETIF,0), PM_TYPE_U32, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, NM2_NETIF_OFFSET(mtu) },
    { "network.interface.in.packets",
      { PMDA_PMID(SCLR_NETIF,2), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(ipackets) },
    { "network.interface.in.bytes",
      { PMDA_PMID(SCLR_NETIF,3), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, NM2_NETIF_OFFSET(ibytes) },
    { "network.interface.in.bcasts",
      { PMDA_PMID(SCLR_NETIF,4), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(ibcast) },
    { "network.interface.in.mcasts",
      { PMDA_PMID(SCLR_NETIF,5), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(imcast) },
    { "network.interface.out.packets",
      { PMDA_PMID(SCLR_NETIF,9), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(opackets) },
    { "network.interface.out.bytes",
      { PMDA_PMID(SCLR_NETIF,10), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, NM2_NETIF_OFFSET(obytes) },
    { "network.interface.out.bcasts",
      { PMDA_PMID(SCLR_NETIF,11), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(obcast) },
    { "network.interface.out.mcasts",
      { PMDA_PMID(SCLR_NETIF,12), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(omcast) },
    { "network.interface.in.errors",
      { PMDA_PMID(SCLR_NETIF,1), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(ierrors) },
    { "network.interface.out.errors",
      { PMDA_PMID(SCLR_NETIF,8), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(oerrors) },
    { "network.interface.in.drops",
      { PMDA_PMID(SCLR_NETIF,6), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(idrops) },
    { "network.interface.out.drops",
      { PMDA_PMID(SCLR_NETIF,13), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(odrops) },
    { "network.interface.in.delivers",
      { PMDA_PMID(SCLR_NETIF,7), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_NETIF_OFFSET(delivered) },
    { "network.udp.noports",
      { PMDA_PMID(SCLR_NETIF,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(noports) },
    { "network.udp.overflows",
      { PMDA_PMID(SCLR_NETIF,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, NM2_UDP_OFFSET(overflows) },

    { "zpool.state",
      { PMDA_PMID(SCLR_ZPOOL,0), PM_TYPE_STRING, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, 0 },

    { "zpool.state_int",
      { PMDA_PMID(SCLR_ZPOOL,1), PM_TYPE_U32, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, 0 },
    { "zpool.perdisk.state",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,0), PM_TYPE_STRING, ZPOOL_PERDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, VDEV_OFFSET(vs_state) },
    { "zpool.perdisk.state_int",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,1), PM_TYPE_U32, ZPOOL_PERDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, 0 },
    { "zpool.perdisk.checksum_errors",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,2), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_checksum_errors) },
    { "zpool.perdisk.self_healed",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,3), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, VDEV_OFFSET(vs_self_healed) },
    { "zpool.perdisk.in.errors",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,4), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_read_errors) },
    { "zpool.perdisk.out.errors",
      { PMDA_PMID(SCLR_ZPOOL_PERDISK,5), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, VDEV_OFFSET(vs_write_errors) },

    { "network.link.in.errors",
      { PMDA_PMID(SCLR_NETLINK,4), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ierrors" },
    { "network.link.in.packets",
      { PMDA_PMID(SCLR_NETLINK,5), PM_TYPE_U64, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ipackets64" },
    { "network.link.in.bytes",
      { PMDA_PMID(SCLR_NETLINK,6), PM_TYPE_U64, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"rbytes64" },
    { "network.link.in.bcasts",
      { PMDA_PMID(SCLR_NETLINK,7), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"brdcstrcv" },
    { "network.link.in.mcasts",
      { PMDA_PMID(SCLR_NETLINK,8), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"multircv" },
    { "network.link.in.nobufs",
      { PMDA_PMID(SCLR_NETLINK,9), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"norcvbuf" },
    { "network.link.out.errors",
      { PMDA_PMID(SCLR_NETLINK,10), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"oerrors" },
    { "network.link.out.packets",
      { PMDA_PMID(SCLR_NETLINK,11), PM_TYPE_U64, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"opackets64" },
    { "network.link.out.bytes",
      { PMDA_PMID(SCLR_NETLINK,12), PM_TYPE_U64, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"obytes64" },
    { "network.link.out.bcasts",
      { PMDA_PMID(SCLR_NETLINK,13), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"brdcstxmt" },
    { "network.link.out.mcasts",
      { PMDA_PMID(SCLR_NETLINK,14), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"multixmt" },
    { "network.link.out.nobufs",
      { PMDA_PMID(SCLR_NETLINK,15), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"noxmtbuf" },
    { "network.link.collisions",
      { PMDA_PMID(SCLR_NETLINK,0), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"collisions" },
    { "network.link.state",
      { PMDA_PMID(SCLR_NETLINK,1), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"link_state" },
    { "network.link.duplex",
      { PMDA_PMID(SCLR_NETLINK,2), PM_TYPE_U32, NETLINK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"link_duplex" },
    { "network.link.speed",
      { PMDA_PMID(SCLR_NETLINK,3), PM_TYPE_U64, NETLINK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ifspeed" },

    { "zfs.recordsize",
      { PMDA_PMID(SCLR_ZFS,5), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_RECORDSIZE },
    { "zfs.refquota",
      { PMDA_PMID(SCLR_ZFS,6), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_REFQUOTA },
    { "zfs.refreservation",
      { PMDA_PMID(SCLR_ZFS,7), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_REFRESERVATION },
    { "zfs.used.byrefreservation",
      { PMDA_PMID(SCLR_ZFS,14), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USEDREFRESERV },
    { "zfs.referenced",
      { PMDA_PMID(SCLR_ZFS,8), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_REFERENCED },
    { "zfs.nsnapshots",
      { PMDA_PMID(SCLR_ZFS,9), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, -1 },
    { "zfs.snapshot.used",
      { PMDA_PMID(SCLR_ZFS,15), PM_TYPE_U64, ZFS_SNAP_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_USED },
    { "zfs.snapshot.referenced",
      { PMDA_PMID(SCLR_ZFS,16), PM_TYPE_U64, ZFS_SNAP_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, ZFS_PROP_REFERENCED },
    { "zfs.snapshot.compression",
      { PMDA_PMID(SCLR_ZFS,17), PM_TYPE_DOUBLE, ZFS_SNAP_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, ZFS_PROP_COMPRESSRATIO },
    { "kernel.all.load",
      { PMDA_PMID(SCLR_SYSINFO,135), PM_TYPE_FLOAT, LOADAVG_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, 0 },

    { "kernel.fsflush.scanned",
      { PMDA_PMID(SCLR_FSFLUSH,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_scan) },
    { "kernel.fsflush.examined",
      { PMDA_PMID(SCLR_FSFLUSH,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_examined) },
    { "kernel.fsflush.locked",
      { PMDA_PMID(SCLR_FSFLUSH,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_locked) },
    { "kernel.fsflush.modified",
      { PMDA_PMID(SCLR_FSFLUSH,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_modified) },
    { "kernel.fsflush.coalesced",
      { PMDA_PMID(SCLR_FSFLUSH,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_coalesce) },
    { "kernel.fsflush.released",
      { PMDA_PMID(SCLR_FSFLUSH,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, FSF_STAT_OFFSET(fsf_releases) },
    { "kernel.fsflush.time",
      { PMDA_PMID(SCLR_FSFLUSH,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, FSF_STAT_OFFSET(fsf_time) },

    { "mem.physmem",
      { PMDA_PMID(SCLR_SYSINFO,136), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, -1},
    { "mem.freemem",
      { PMDA_PMID(SCLR_SYSINFO,137), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, -1},
    { "mem.lotsfree",
      { PMDA_PMID(SCLR_SYSINFO,138), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, -1},
    { "mem.availrmem",
      { PMDA_PMID(SCLR_SYSINFO,139), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_KBYTE, 0, 0)
      }, -1},

    { "zfs.arc.size",
      { PMDA_PMID(SCLR_ARCSTATS,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"size"},
    { "zfs.arc.min_size",
      { PMDA_PMID(SCLR_ARCSTATS,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"c_min"},
    { "zfs.arc.max_size",
      { PMDA_PMID(SCLR_ARCSTATS,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"c_max"},
    { "zfs.arc.mru_size",
      { PMDA_PMID(SCLR_ARCSTATS,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"p"},
    { "zfs.arc.target_size",
      { PMDA_PMID(SCLR_ARCSTATS,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"c"},
    { "zfs.arc.misses.total",
      { PMDA_PMID(SCLR_ARCSTATS,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"misses"},
    { "zfs.arc.misses.demand_data",
      { PMDA_PMID(SCLR_ARCSTATS,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"demand_data_misses"},
    { "zfs.arc.misses.demand_metadata",
      { PMDA_PMID(SCLR_ARCSTATS,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"demand_metadata_misses"},
    { "zfs.arc.misses.prefetch_data",
      { PMDA_PMID(SCLR_ARCSTATS,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"prefetch_data_misses"},
    { "zfs.arc.misses.prefetch_metadata",
      { PMDA_PMID(SCLR_ARCSTATS,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"prefetch_metadata_misses"},
    { "zfs.arc.hits.total",
      { PMDA_PMID(SCLR_ARCSTATS,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"hits"},
    { "zfs.arc.hits.mfu",
      { PMDA_PMID(SCLR_ARCSTATS,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"mfu_hits"},
    { "zfs.arc.hits.mru",
      { PMDA_PMID(SCLR_ARCSTATS,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"mru_hits"},
    { "zfs.arc.hits.mfu_ghost",
      { PMDA_PMID(SCLR_ARCSTATS,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"mfu_ghost_hits"},
    { "zfs.arc.hits.mru_ghost",
      { PMDA_PMID(SCLR_ARCSTATS,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"mru_ghost_hits"},
    { "zfs.arc.hits.demand_data",
      { PMDA_PMID(SCLR_ARCSTATS,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"demand_data_hits"},
    { "zfs.arc.hits.demand_metadata",
      { PMDA_PMID(SCLR_ARCSTATS,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"demand_metadata_hits"},
    { "zfs.arc.hits.prefetch_data",
      { PMDA_PMID(SCLR_ARCSTATS,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"prefetch_data_hits"},
    { "zfs.arc.hits.prefetch_metadata",
      { PMDA_PMID(SCLR_ARCSTATS,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"prefetch_metadata_hits"},
    { "pmda.prefetch.time",
      { PMDA_PMID(4095,0), PM_TYPE_U64, PREFETCH_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, -1 },
    { "pmda.prefetch.count",
      { PMDA_PMID(4095,1), PM_TYPE_U64, PREFETCH_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1 },
    { "pmda.metric.time",
      { PMDA_PMID(4095,2), PM_TYPE_U64, METRIC_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, -1 },
    { "pmda.metric.count",
      { PMDA_PMID(4095,3), PM_TYPE_U64, METRIC_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1 },
    { "disk.dev.wait.time",
      { PMDA_PMID(SCLR_DISK,16), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, KSTAT_IO_OFF(wtime)},
    { "disk.dev.wait.count",
      { PMDA_PMID(SCLR_DISK,17), PM_TYPE_U32, DISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(wcnt)},
    { "disk.dev.run.time",
      { PMDA_PMID(SCLR_DISK,18), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, KSTAT_IO_OFF(rtime)},
    { "disk.dev.run.count",
      { PMDA_PMID(SCLR_DISK,19), PM_TYPE_U32, DISK_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, KSTAT_IO_OFF(rcnt)},

    { "disk.all.wait.time",
      { PMDA_PMID(SCLR_DISK,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, KSTAT_IO_OFF(wtime)},
    { "disk.all.wait.count",
      { PMDA_PMID(SCLR_DISK,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, -1},
    { "disk.all.run.time",
      { PMDA_PMID(SCLR_DISK,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_NSEC, 0)
      }, KSTAT_IO_OFF(rtime)},
    { "disk.all.run.count",
      { PMDA_PMID(SCLR_DISK,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, -1},

    { "kernel.fs.read_bytes",
      { PMDA_PMID(SCLR_FILESYS,0), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"read_bytes"},
    { "kernel.fs.readdir_bytes",
      { PMDA_PMID(SCLR_FILESYS,1), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"readdir_bytes"},
    { "kernel.fs.write_bytes",
      { PMDA_PMID(SCLR_FILESYS,2), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"write_bytes"},
    { "kernel.fs.vnops.access",
      { PMDA_PMID(SCLR_FILESYS,3), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"naccess"},
    { "kernel.fs.vnops.addmap",
      {PMDA_PMID(SCLR_FILESYS,4), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"naddmap"},
    { "kernel.fs.vnops.close",
      {PMDA_PMID(SCLR_FILESYS,5), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nclose"},
    { "kernel.fs.vnops.cmp",
      {PMDA_PMID(SCLR_FILESYS,6), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ncmp"},
    { "kernel.fs.vnops.create",
      {PMDA_PMID(SCLR_FILESYS,7), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ncreate"},
    { "kernel.fs.vnops.delmap",
      {PMDA_PMID(SCLR_FILESYS,8), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndelmap"},
    { "kernel.fs.vnops.dispose",
      {PMDA_PMID(SCLR_FILESYS,9), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndispose"},
    { "kernel.fs.vnops.dump",
      {PMDA_PMID(SCLR_FILESYS,10), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndump"},
    { "kernel.fs.vnops.dumpctl",
      {PMDA_PMID(SCLR_FILESYS,11), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndumpctl"},
    { "kernel.fs.vnops.fid",
      {PMDA_PMID(SCLR_FILESYS,12), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfid"},
    { "kernel.fs.vnops.frlock",
      {PMDA_PMID(SCLR_FILESYS,13), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfrlock"},
    { "kernel.fs.vnops.fsync",
      {PMDA_PMID(SCLR_FILESYS,14), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfsync"},
    { "kernel.fs.vnops.getattr",
      {PMDA_PMID(SCLR_FILESYS,15), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetattr"},
    { "kernel.fs.vnops.getpage",
      {PMDA_PMID(SCLR_FILESYS,16), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetpage"},
    { "kernel.fs.vnops.getsecattr",
      {PMDA_PMID(SCLR_FILESYS,17), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetsecattr"},
    { "kernel.fs.vnops.inactive",
      {PMDA_PMID(SCLR_FILESYS,18), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ninactive"},
    { "kernel.fs.vnops.ioctl",
      {PMDA_PMID(SCLR_FILESYS,19), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nioctl"},
    { "kernel.fs.vnops.link",
      {PMDA_PMID(SCLR_FILESYS,20), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nlink"},
    { "kernel.fs.vnops.lookup",
      {PMDA_PMID(SCLR_FILESYS,21), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nlookup"},
    { "kernel.fs.vnops.map",
      {PMDA_PMID(SCLR_FILESYS,22), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nmap"},
    { "kernel.fs.vnops.mkdir",
      {PMDA_PMID(SCLR_FILESYS,23), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nmkdir"},
    { "kernel.fs.vnops.open",
      {PMDA_PMID(SCLR_FILESYS,24), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nopen"},
    { "kernel.fs.vnops.pageio",
      {PMDA_PMID(SCLR_FILESYS,25), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npageio"},
    { "kernel.fs.vnops.pathconf",
      {PMDA_PMID(SCLR_FILESYS,26), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npathconf"},
    { "kernel.fs.vnops.poll",
      {PMDA_PMID(SCLR_FILESYS,27), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npoll"},
    { "kernel.fs.vnops.putpage",
      {PMDA_PMID(SCLR_FILESYS,28), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nputpage"},
    { "kernel.fs.vnops.read",
      {PMDA_PMID(SCLR_FILESYS,29), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nread"},
    { "kernel.fs.vnops.readdir",
      {PMDA_PMID(SCLR_FILESYS,30), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nreaddir"},
    { "kernel.fs.vnops.readlink",
      {PMDA_PMID(SCLR_FILESYS,31), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nreadlink"},
    { "kernel.fs.vnops.realvp",
      {PMDA_PMID(SCLR_FILESYS,32), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrealvp"},
    { "kernel.fs.vnops.remove",
      {PMDA_PMID(SCLR_FILESYS,33), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nremove"},
    { "kernel.fs.vnops.rename",
      {PMDA_PMID(SCLR_FILESYS,34), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrename"},
    { "kernel.fs.vnops.rmdir",
      {PMDA_PMID(SCLR_FILESYS,35), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrmdir"},
    { "kernel.fs.vnops.rwlock",
      {PMDA_PMID(SCLR_FILESYS,36), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrwlock"},
    { "kernel.fs.vnops.rwunlock",
      {PMDA_PMID(SCLR_FILESYS,37), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrwunlock"},
    { "kernel.fs.vnops.seek",
      {PMDA_PMID(SCLR_FILESYS,38), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nseek"},
    { "kernel.fs.vnops.setattr",
      {PMDA_PMID(SCLR_FILESYS,39), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetattr"},
    { "kernel.fs.vnops.setfl",
      {PMDA_PMID(SCLR_FILESYS,40), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetfl"},
    { "kernel.fs.vnops.setsecattr",
      {PMDA_PMID(SCLR_FILESYS,41), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetsecattr"},
    { "kernel.fs.vnops.shrlock",
      {PMDA_PMID(SCLR_FILESYS,42), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nshrlock"},
    { "kernel.fs.vnops.space",
      {PMDA_PMID(SCLR_FILESYS,43), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nspace"},
    { "kernel.fs.vnops.symlink",
      {PMDA_PMID(SCLR_FILESYS,44), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsymlink"},
    { "kernel.fs.vnops.vnevent",
      {PMDA_PMID(SCLR_FILESYS,45), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nvnevent"},
    { "kernel.fs.vnops.write",
      {PMDA_PMID(SCLR_FILESYS,46), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nwrite"},

    { "kernel.fstype.read_bytes",
      { PMDA_PMID(SCLR_FILESYS,47), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"read_bytes"},
    { "kernel.fstype.readdir_bytes",
      { PMDA_PMID(SCLR_FILESYS,48), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"readdir_bytes"},
    { "kernel.fstype.write_bytes",
      { PMDA_PMID(SCLR_FILESYS,49), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"write_bytes"},
    { "kernel.fstype.vnops.access",
      { PMDA_PMID(SCLR_FILESYS,50), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"naccess"},
    { "kernel.fstype.vnops.addmap",
      {PMDA_PMID(SCLR_FILESYS,51), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"naddmap"},
    { "kernel.fstype.vnops.close",
      {PMDA_PMID(SCLR_FILESYS,52), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nclose"},
    { "kernel.fstype.vnops.cmp",
      {PMDA_PMID(SCLR_FILESYS,53), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ncmp"},
    { "kernel.fstype.vnops.create",
      {PMDA_PMID(SCLR_FILESYS,54), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ncreate"},
    { "kernel.fstype.vnops.delmap",
      {PMDA_PMID(SCLR_FILESYS,55), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndelmap"},
    { "kernel.fstype.vnops.dispose",
      {PMDA_PMID(SCLR_FILESYS,56), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndispose"},
    { "kernel.fstype.vnops.dump",
      {PMDA_PMID(SCLR_FILESYS,57), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndump"},
    { "kernel.fstype.vnops.dumpctl",
      {PMDA_PMID(SCLR_FILESYS,58), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ndumpctl"},
    { "kernel.fstype.vnops.fid",
      {PMDA_PMID(SCLR_FILESYS,59), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfid"},
    { "kernel.fstype.vnops.frlock",
      {PMDA_PMID(SCLR_FILESYS,60), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfrlock"},
    { "kernel.fstype.vnops.fsync",
      {PMDA_PMID(SCLR_FILESYS,61), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nfsync"},
    { "kernel.fstype.vnops.getattr",
      {PMDA_PMID(SCLR_FILESYS,62), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetattr"},
    { "kernel.fstype.vnops.getpage",
      {PMDA_PMID(SCLR_FILESYS,63), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetpage"},
    { "kernel.fstype.vnops.getsecattr",
      {PMDA_PMID(SCLR_FILESYS,64), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ngetsecattr"},
    { "kernel.fstype.vnops.inactive",
      {PMDA_PMID(SCLR_FILESYS,65), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"ninactive"},
    { "kernel.fstype.vnops.ioctl",
      {PMDA_PMID(SCLR_FILESYS,66), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nioctl"},
    { "kernel.fstype.vnops.link",
      {PMDA_PMID(SCLR_FILESYS,67), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nlink"},
    { "kernel.fstype.vnops.lookup",
      {PMDA_PMID(SCLR_FILESYS,68), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nlookup"},
    { "kernel.fstype.vnops.map",
      {PMDA_PMID(SCLR_FILESYS,69), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nmap"},
    { "kernel.fstype.vnops.mkdir",
      {PMDA_PMID(SCLR_FILESYS,70), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nmkdir"},
    { "kernel.fstype.vnops.open",
      {PMDA_PMID(SCLR_FILESYS,71), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nopen"},
    { "kernel.fstype.vnops.pageio",
      {PMDA_PMID(SCLR_FILESYS,72), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npageio"},
    { "kernel.fstype.vnops.pathconf",
      {PMDA_PMID(SCLR_FILESYS,73), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npathconf"},
    { "kernel.fstype.vnops.poll",
      {PMDA_PMID(SCLR_FILESYS,74), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"npoll"},
    { "kernel.fstype.vnops.putpage",
      {PMDA_PMID(SCLR_FILESYS,75), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nputpage"},
    { "kernel.fstype.vnops.read",
      {PMDA_PMID(SCLR_FILESYS,76), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nread"},
    { "kernel.fstype.vnops.readdir",
      {PMDA_PMID(SCLR_FILESYS,77), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nreaddir"},
    { "kernel.fstype.vnops.readlink",
      {PMDA_PMID(SCLR_FILESYS,78), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nreadlink"},
    { "kernel.fstype.vnops.realvp",
      {PMDA_PMID(SCLR_FILESYS,79), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrealvp"},
    { "kernel.fstype.vnops.remove",
      {PMDA_PMID(SCLR_FILESYS,80), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nremove"},
    { "kernel.fstype.vnops.rename",
      {PMDA_PMID(SCLR_FILESYS,81), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrename"},
    { "kernel.fstype.vnops.rmdir",
      {PMDA_PMID(SCLR_FILESYS,82), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrmdir"},
    { "kernel.fstype.vnops.rwlock",
      {PMDA_PMID(SCLR_FILESYS,83), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrwlock"},
    { "kernel.fstype.vnops.rwunlock",
      {PMDA_PMID(SCLR_FILESYS,84), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nrwunlock"},
    { "kernel.fstype.vnops.seek",
      {PMDA_PMID(SCLR_FILESYS,85), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nseek"},
    { "kernel.fstype.vnops.setattr",
      {PMDA_PMID(SCLR_FILESYS,86), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetattr"},
    { "kernel.fstype.vnops.setfl",
      {PMDA_PMID(SCLR_FILESYS,87), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetfl"},
    { "kernel.fstype.vnops.setsecattr",
      {PMDA_PMID(SCLR_FILESYS,88), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsetsecattr"},
    { "kernel.fstype.vnops.shrlock",
      {PMDA_PMID(SCLR_FILESYS,89), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nshrlock"},
    { "kernel.fstype.vnops.space",
      {PMDA_PMID(SCLR_FILESYS,90), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nspace"},
    { "kernel.fstype.vnops.symlink",
      {PMDA_PMID(SCLR_FILESYS,91), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nsymlink"},
    { "kernel.fstype.vnops.vnevent",
      {PMDA_PMID(SCLR_FILESYS,92), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nvnevent"},
    { "kernel.fstype.vnops.write",
      {PMDA_PMID(SCLR_FILESYS,93), PM_TYPE_U64, FSTYPE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, (ptrdiff_t)"nwrite"},

    { "hinv.cpu.maxclock",
      {PMDA_PMID(SCLR_SYSINFO,147), PM_TYPE_64, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, -1, 1, 0, PM_TIME_SEC, 6)
      }, (ptrdiff_t)"clock_MHz"},
    { "hinv.cpu.clock",
      {PMDA_PMID(SCLR_SYSINFO,148), PM_TYPE_U64, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, -1, 1, 0, PM_TIME_SEC, 0)
      }, (ptrdiff_t)"current_clock_Hz"},
    { "hinv.cpu.brand",
      {PMDA_PMID(SCLR_SYSINFO, 149), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"brand"},
    { "hinv.cpu.frequencies",
      {PMDA_PMID(SCLR_SYSINFO, 150), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"supported_frequencies_Hz"},
    { "hinv.cpu.implementation",
      {PMDA_PMID(SCLR_SYSINFO, 151), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"implementation"},
    { "hinv.cpu.chip_id",
      {PMDA_PMID(SCLR_SYSINFO, 152), PM_TYPE_64, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"chip_id"},
    { "hinv.cpu.clog_id",
      {PMDA_PMID(SCLR_SYSINFO, 153), PM_TYPE_32, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"clog_id"},
    { "hinv.cpu.core_id",
      {PMDA_PMID(SCLR_SYSINFO, 154), PM_TYPE_64, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"core_id"},
    { "hinv.cpu.pkg_core_id",
      {PMDA_PMID(SCLR_SYSINFO, 155), PM_TYPE_64, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"pkg_core_id"},
    { "hinv.cpu.cstate",
      {PMDA_PMID(SCLR_SYSINFO, 156), PM_TYPE_32, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"current_cstate"},
    { "hinv.cpu.maxcstates",
      {PMDA_PMID(SCLR_SYSINFO, 157), PM_TYPE_32, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"supported_max_cstates"},
    { "hinv.cpu.ncores",
      {PMDA_PMID(SCLR_SYSINFO, 158), PM_TYPE_32, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"ncore_per_chip"},
    { "hinv.cpu.ncpus",
      {PMDA_PMID(SCLR_SYSINFO, 159), PM_TYPE_32, CPU_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"ncpu_per_chip"},

    { "disk.dev.errors.soft",
      {PMDA_PMID(SCLR_DISK, 21), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Soft Errors"},
    { "disk.dev.errors.hard",
      {PMDA_PMID(SCLR_DISK, 22), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Hard Errors"},
    { "disk.dev.errors.transport",
      {PMDA_PMID(SCLR_DISK, 23), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Transport Errors"},
    { "disk.dev.errors.media",
      {PMDA_PMID(SCLR_DISK, 24), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Media Error"},
    { "disk.dev.errors.recoverable",
      {PMDA_PMID(SCLR_DISK, 25), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Recoverable"},
    { "disk.dev.errors.notready",
      {PMDA_PMID(SCLR_DISK, 26), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Device Not Ready"},
    { "disk.dev.errors.nodevice",
      {PMDA_PMID(SCLR_DISK, 27), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"No Device"},
    { "disk.dev.errors.badrequest",
      {PMDA_PMID(SCLR_DISK, 28), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Illegal Request"},
    { "disk.dev.errors.pfa",
      {PMDA_PMID(SCLR_DISK, 29), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE),
      }, (ptrdiff_t)"Predictive Failure Analysis"},
    { "hinv.disk.vendor",
      {PMDA_PMID(SCLR_DISK, 30), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"Vendor"},
    { "hinv.disk.product",
      {PMDA_PMID(SCLR_DISK, 31), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"Product"},
    { "hinv.disk.revision",
      {PMDA_PMID(SCLR_DISK, 32), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"Revision"},
    { "hinv.disk.serial",
      {PMDA_PMID(SCLR_DISK, 33), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, (ptrdiff_t)"Serial No"},
    { "hinv.disk.capacity",
      { PMDA_PMID(SCLR_DISK,34), PM_TYPE_U64, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, (ptrdiff_t)"Size" },
    { "hinv.disk.devlink",
      {PMDA_PMID(SCLR_DISK, 35), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, -1}

    /* remember to add trailing comma before adding more entries ... */
};
int metrictab_sz = ARRAY_SIZE(metricdesc);

pmdaInstid metric_insts[ARRAY_SIZE(metricdesc)];

/*
 * List of instance domains ... we expect the *_INDOM macros
 * to index into this table.
 */
pmdaIndom indomtab[] = {
    { DISK_INDOM, 0, NULL },
    { CPU_INDOM, 0, NULL },
    { NETIF_INDOM, 0, NULL },
    { ZPOOL_INDOM, 0, NULL },
    { ZFS_INDOM, 0, NULL },
    { ZPOOL_PERDISK_INDOM, 0, NULL },
    { NETLINK_INDOM},
    { ZFS_SNAP_INDOM },
    { LOADAVG_INDOM, ARRAY_SIZE(loadavg_insts), loadavg_insts},
    { PREFETCH_INDOM, ARRAY_SIZE(prefetch_insts), prefetch_insts},
    { METRIC_INDOM, ARRAY_SIZE(metric_insts), metric_insts},
    { FILESYS_INDOM },
    { FSTYPE_INDOM }
};

int indomtab_sz = sizeof(indomtab) / sizeof(indomtab[0]);

static kstat_ctl_t *kc;
static int kstat_chains_updated;

kstat_ctl_t *
kstat_ctl_update(void)
{
    if (!kstat_chains_updated) {
	if (kstat_chain_update(kc) == -1)  {
	    kstat_chains_updated = 0;
	    return NULL;
	}
	kstat_chains_updated = 1;
    }
    return kc;
}

void
kstat_ctl_needs_update(void)
{
	kstat_chains_updated = 0;
}

void
init_data(int domain)
{
    int			i;
    int			serial;
    __pmID_int		*ip;

    /*
     * set up kstat() handle ... failure is fatal
     */
    if ((kc = kstat_open()) == NULL) {
	fprintf(stderr, "init_data: kstat_open failed: %s\n", osstrerror());
	exit(1);
    }

    /*
     * Create the PMDA's metrictab[] version of the per-metric table.
     *
     * Also do domain initialization for each pmid and indom element of
     * the metricdesc[] table ... the PMDA table is fixed up in
     * libpcp_pmda
     */
    if ((metrictab = (pmdaMetric *)malloc(metrictab_sz * sizeof(pmdaMetric))) == NULL) {
	fprintf(stderr, "init_data: Error: malloc metrictab [%d] failed: %s\n",
	    (int)(metrictab_sz * sizeof(pmdaMetric)), osstrerror());
	exit(1);
    }
    for (i = 0; i < metrictab_sz; i++) {
	metrictab[i].m_user = &metricdesc[i];
	metrictab[i].m_desc = metricdesc[i].md_desc;
	ip = (__pmID_int *)&metricdesc[i].md_desc.pmid;
	ip->domain = domain;

	if (metricdesc[i].md_desc.indom != PM_INDOM_NULL) {
	    serial = metricdesc[i].md_desc.indom;
	    metricdesc[i].md_desc.indom = pmInDom_build(domain, serial);
	}
	metric_insts[i].i_inst = i+1;
	metric_insts[i].i_name = (char *)metricdesc[i].md_name;
    }

    /* Bless indoms with our own domain - usually pmdaInit will do it for
     * us but we need properly setup indoms for pmdaCache which means that
     * we have to do it ourselves */
    for (i = 0; i < indomtab_sz; i++) {
	__pmindom_int(&indomtab[i].it_indom)->domain = domain;
    }

    /*
     * initialize each of the methods
     */
    for (i = 0; i < methodtab_sz; i++) {
	if (methodtab[i].m_init) {
	    methodtab[i].m_init(1);
	}

	prefetch_insts[i].i_inst = i + 1;
	prefetch_insts[i].i_name = (char *)methodtab[i].m_name;
    }
}
