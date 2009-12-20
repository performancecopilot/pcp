/*
 * Data structures that define metrics and control the Solaris PMDA
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "common.h"
#include "netmib2.h"
#include <ctype.h>
#include <libzfs.h>

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
    { ZPOOL_PERDISK_INDOM, 0, NULL }
};
int indomtab_sz = sizeof(indomtab) / sizeof(indomtab[0]);

pmdaMetric *metrictab;

method_t methodtab[] = {
    { sysinfo_init, sysinfo_prefetch, sysinfo_fetch },	// M_SYSINFO
    { disk_init, disk_prefetch, disk_fetch },		// M_DISK
    { netmib2_init, netmib2_refresh, netmib2_fetch },
    { zpool_init, zpool_refresh, zpool_fetch },
    { zfs_init, zfs_refresh, zfs_fetch },
    { zpool_perdisk_init, zpool_perdisk_refresh, zpool_perdisk_fetch }
};
int methodtab_sz = sizeof(methodtab) / sizeof(methodtab[0]);

#define SYSINFO_OFF(field) ((ptrdiff_t)&((cpu_stat_t *)0)->cpu_sysinfo.field)
#define KSTAT_IO_OFF(field) ((ptrdiff_t)&((kstat_io_t *)0)->field)
#define VDEV_OFFSET(field) ((ptrdiff_t)&((vdev_stat_t *)0)->field)
#define VDEV_STATE_COMBINED ((ptrdiff_t)(sizeof(vdev_stat_t)))
#define NM2_UDP_OFFSET(field) ((ptrdiff_t)&(nm2_udp.field))
#define NM2_NETIF_OFFSET(field) ((ptrdiff_t)&((nm2_netif_stats_t *)0)->field)

/*
 * all metrics supported in this PMDA - one table entry for each metric
 */
metricdesc_t metricdesc[] = {

/* kernel.all.cpu.idle */
    { { PMDA_PMID(0,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_IDLE]) },

/* kernel.all.cpu.user */
    { { PMDA_PMID(0,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_USER]) },

/* kernel.all.cpu.sys */
    { { PMDA_PMID(0,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_KERNEL]) },

/* kernel.all.cpu.wait.total */
    { { PMDA_PMID(0,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_WAIT]) },

/* kernel.percpu.cpu.idle */
    { { PMDA_PMID(0,4), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_IDLE]) },

/* kernel.percpu.cpu.user */
    { { PMDA_PMID(0,5), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_USER]) },

/* kernel.percpu.cpu.sys */
    { { PMDA_PMID(0,6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_KERNEL]) },

/* kernel.percpu.cpu.wait.total */
    { { PMDA_PMID(0,7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(cpu[CPU_WAIT]) },

/* kernel.all.cpu.wait.io */
    { { PMDA_PMID(0,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_IO]) },

/* kernel.all.cpu.wait.pio */
    { { PMDA_PMID(0,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_PIO]) },

/* kernel.all.cpu.wait.swap */
    { { PMDA_PMID(0,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_SWAP]) },

/* kernel.percpu.cpu.wait.io */
    { { PMDA_PMID(0,11), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_IO]) },

/* kernel.percpu.cpu.wait.pio */
    { { PMDA_PMID(0,12), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_PIO]) },

/* kernel.percpu.cpu.wait.swap */
    { { PMDA_PMID(0,13), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0)
      }, M_SYSINFO, SYSINFO_OFF(wait[W_SWAP]) },

/* kernel.all.io.bread */
    { { PMDA_PMID(0,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(bread) },

/* kernel.all.io.bwrite */
    { { PMDA_PMID(0,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(bwrite) },

/* kernel.all.io.lread */
    { { PMDA_PMID(0,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(lread) },

/* kernel.all.io.lwrite */
    { { PMDA_PMID(0,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(lwrite) },

/* kernel.percpu.io.bread */
    { { PMDA_PMID(0,18), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(bread) },

/* kernel.percpu.io.bwrite */
    { { PMDA_PMID(0,19), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(bwrite) },

/* kernel.percpu.io.lread */
    { { PMDA_PMID(0,20), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(lread) },

/* kernel.percpu.io.lwrite */
    { { PMDA_PMID(0,21), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(lwrite) },

/* kernel.all.syscall */
    { { PMDA_PMID(0,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(syscall) },

/* kernel.all.pswitch */
    { { PMDA_PMID(0,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(pswitch) },

/* kernel.percpu.syscall */
    { { PMDA_PMID(0,24), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(syscall) },

/* kernel.percpu.pswitch */
    { { PMDA_PMID(0,25), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(pswitch) },

/* kernel.all.io.phread */
    { { PMDA_PMID(0,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(phread) },

/* kernel.all.io.phwrite */
    { { PMDA_PMID(0,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(phwrite) },

/* kernel.all.io.intr */
    { { PMDA_PMID(0,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(intr) },

/* kernel.percpu.io.phread */
    { { PMDA_PMID(0,29), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(phread) },

/* kernel.percpu.io.phwrite */
    { { PMDA_PMID(0,30), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(phwrite) },

/* kernel.percpu.io.intr */
    { { PMDA_PMID(0,31), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(intr) },

/* kernel.all.trap */
    { { PMDA_PMID(0,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(trap) },

/* kernel.all.sysexec */
    { { PMDA_PMID(0,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysexec) },

/* kernel.all.sysfork */
    { { PMDA_PMID(0,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysfork) },

/* kernel.all.sysvfork */
    { { PMDA_PMID(0,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysvfork) },

/* kernel.all.sysread */
    { { PMDA_PMID(0,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysread) },

/* kernel.all.syswrite */
    { { PMDA_PMID(0,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(syswrite) },

/* kernel.percpu.trap */
    { { PMDA_PMID(0,38), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(trap) },

/* kernel.percpu.sysexec */
    { { PMDA_PMID(0,39), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysexec) },

/* kernel.percpu.sysfork */
    { { PMDA_PMID(0,40), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysfork) },

/* kernel.percpu.sysvfork */
    { { PMDA_PMID(0,41), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysvfork) },

/* kernel.percpu.sysread */
    { { PMDA_PMID(0,42), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(sysread) },

/* kernel.percpu.syswrite */
    { { PMDA_PMID(0,43), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, SYSINFO_OFF(syswrite) },

/* disk.all.read */
    { { PMDA_PMID(0,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, KSTAT_IO_OFF(reads) },

/* disk.all.write */
    { { PMDA_PMID(0,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, KSTAT_IO_OFF(writes) },

/* disk.all.total */
    { { PMDA_PMID(0,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, -1 /* derived */ },

/* disk.all.read_bytes */
    { { PMDA_PMID(0,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, KSTAT_IO_OFF(nread) },

/* disk.all.write_bytes */
    { { PMDA_PMID(0,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, KSTAT_IO_OFF(nwritten) },

/* disk.all.total_bytes */
    { { PMDA_PMID(0,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, -1 /* derived */ },

/* disk.dev.read */
    { { PMDA_PMID(0,50), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, KSTAT_IO_OFF(reads) },

/* disk.dev.write */
    { { PMDA_PMID(0,51), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, KSTAT_IO_OFF(writes) },

/* disk.dev.total */
    { { PMDA_PMID(0,52), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, -1 /* derived */ },

/* disk.dev.read_bytes */
    { { PMDA_PMID(0,53), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, KSTAT_IO_OFF(nread) },

/* disk.dev.write_bytes */
    { { PMDA_PMID(0,54), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, KSTAT_IO_OFF(nwritten) },

/* disk.dev.total_bytes */
    { { PMDA_PMID(0,55), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_DISK, -1 /* derived */ },

/* hinv.ncpu */
    { { PMDA_PMID(0,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_SYSINFO, -1 /* derived */ },

/* hinv.ndisk */
    { { PMDA_PMID(0,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_DISK, -1 /* derived */ },
/* zpool.capacity */
    { { PMDA_PMID(0,58), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_space) },
/* zpool.used */
    { { PMDA_PMID(0,59), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_alloc) },
/* zpool.in.bytes */
    { { PMDA_PMID(0,60), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_bytes[ZIO_TYPE_READ]) },
/* zpool.out.bytes */
    { { PMDA_PMID(0,61), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_bytes[ZIO_TYPE_WRITE]) },
/* zpool.in.ops */
    { { PMDA_PMID(0,62), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL, VDEV_OFFSET(vs_ops[ZIO_TYPE_READ]) },
/* zpool.out.ops */
    { { PMDA_PMID(0,63), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL, VDEV_OFFSET(vs_bytes[ZIO_TYPE_WRITE]) },
/* zpool.in.errors */
    { { PMDA_PMID(0,64), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL, VDEV_OFFSET(vs_read_errors) },
/* zpool.out.errors */
    { { PMDA_PMID(0,65), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL, VDEV_OFFSET(vs_write_errors) },
/* zpool.checksum_errors */
    { { PMDA_PMID(0,66), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL, VDEV_OFFSET(vs_checksum_errors) },
/* zpool.self_healed */
    { { PMDA_PMID(0,67), PM_TYPE_U64, ZPOOL_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_self_healed) },
/* zpool continued at 97 */

/* zfs.used.total */
    { { PMDA_PMID(0,68), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_USED },
/* zfs.available */
    { { PMDA_PMID(0,69), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_AVAILABLE },
/* zfs.quota */
    { { PMDA_PMID(0,70), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_QUOTA },
/* zfs.reservation */
    { { PMDA_PMID(0,71), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_RESERVATION },
/* zfs.compression */
    { { PMDA_PMID(0,72), PM_TYPE_DOUBLE, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZFS, ZFS_PROP_COMPRESSRATIO },
/* zfs.copies */
    { { PMDA_PMID(0,73), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZFS, ZFS_PROP_COPIES },
/* zfs.used.byme */
    { { PMDA_PMID(0,74), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_USEDDS },
/* zfs.used.bysnapshots */
    { { PMDA_PMID(0,75), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_USEDSNAP },
/* zfs.used.bychildren */
    { { PMDA_PMID(0,76), PM_TYPE_U64, ZFS_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZFS, ZFS_PROP_USEDCHILD },

/* network.udp.ipackets */
    { { PMDA_PMID(0,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(ipackets) },
/* network.udp.opackets */
    { { PMDA_PMID(0,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(opackets) },
/* network.udp.ierrors */
    { { PMDA_PMID(0,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(ierrors) },
/* network.udp.oerrors */
    { { PMDA_PMID(0,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(oerrors) },

/* network.interface.mtu */
    { { PMDA_PMID(0,81), PM_TYPE_U32, NETIF_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, NM2_NETIF_OFFSET(mtu) },
/* network.interface.in.bytes */
    { { PMDA_PMID(0,82), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, NM2_NETIF_OFFSET(ibytes) },
/* network.interface.out.bytes */
    { { PMDA_PMID(0,83), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_NETIF, NM2_NETIF_OFFSET(obytes) },
/* network.interface.in.packets */
    { { PMDA_PMID(0,84), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(ipackets) },
/* network.interface.out.packets */
    { { PMDA_PMID(0,85), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(opackets) },
/* network.interface.in.bcasts */
    { { PMDA_PMID(0,86), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(ibcast) },
/* network.interface.out.bcasts */
    { { PMDA_PMID(0,87), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(obcast) },
/* network.interface.in.mcasts */
    { { PMDA_PMID(0,88), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(imcast) },
/* network.interface.out.mcasts */
    { { PMDA_PMID(0,89), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(omcast) },
/* network.interface.in.errors */
    { { PMDA_PMID(0,90), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(ierrors) },
/* network.interface.out.errors */
    { { PMDA_PMID(0,91), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(oerrors) },
/* network.interface.in.drops */
    { { PMDA_PMID(0,92), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(idrops) },
/* network.interface.out.drops */
    { { PMDA_PMID(0,93), PM_TYPE_U32, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(odrops) },
/* network.interface.in.delivers */
    { { PMDA_PMID(0,94), PM_TYPE_U64, NETIF_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_NETIF_OFFSET(delivered) },
/* network.udp.noport */
    { { PMDA_PMID(0,95), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(noports) },
/* network.udp.overflows */
    { { PMDA_PMID(0,96), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_NETIF, NM2_UDP_OFFSET(overflows) },

/* zpool.state */
    { { PMDA_PMID(0,97), PM_TYPE_U32, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_state) },
/* zpool.state_aux */
    { { PMDA_PMID(0,98), PM_TYPE_U32, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL, VDEV_OFFSET(vs_aux) },
/* zpool.state_combined */
    { { PMDA_PMID(0,99), PM_TYPE_U32, ZPOOL_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL, VDEV_STATE_COMBINED },

/* zpool.perdisk.state */
    { { PMDA_PMID(0,100), PM_TYPE_U32, ZPOOL_PERDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_state) },
/* zpool.perdisk.state_aux */
    { { PMDA_PMID(0,101), PM_TYPE_U32, ZPOOL_PERDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_aux) },
/* zpool.perdisk.state_combined */
    { { PMDA_PMID(0,102), PM_TYPE_U32, ZPOOL_PERDISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0, 0, 0, 0, 0, 0)
      }, M_ZPOOL_PERDISK, VDEV_STATE_COMBINED },
/* zpool.perdisk.checksum_errors */
    { { PMDA_PMID(0,103), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_checksum_errors) },
/* zpool.perdisk.self_healed */
    { { PMDA_PMID(0,104), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_self_healed) },
/* zpool.perdisk.in.errors */
    { { PMDA_PMID(0,105), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_read_errors) },
/* zpool.perdisk.out.errors */
    { { PMDA_PMID(0,106), PM_TYPE_U64, ZPOOL_PERDISK_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE)
      }, M_ZPOOL_PERDISK, VDEV_OFFSET(vs_write_errors) }

/* remember to add trailing comma before adding more entries ... */
};
int metrictab_sz = sizeof(metricdesc) / sizeof(metricdesc[0]);

kstat_ctl_t 		*kc;

void
init_data(int domain)
{
    int			i;
    int			serial;
    __pmID_int		*ip;
    __pmInDom_int	*iip;

    /*
     * set up kstat() handle ... failure is fatal
     */
    if ((kc = kstat_open()) == NULL) {
	fprintf(stderr, "init_data: kstat_open failed: %s\n", strerror(errno));
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
	    metrictab_sz * sizeof(pmdaMetric), strerror(errno));
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
    }
}
