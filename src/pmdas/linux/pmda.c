/*
 * Linux PMDA
 *
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Portions Copyright (c) International Business Machines Corp., 2002
 * Portions Copyright (c) 2007 Aconex.  All Rights Reserved.
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
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 *
 */

#ident "$Id: pmda.c,v 1.73 2007/02/20 00:08:32 kimbrr Exp $"

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/vfs.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "domain.h"

#include "clusters.h"
#include "indom.h"

#include "proc_cpuinfo.h"
#include "proc_stat.h"
#include "proc_meminfo.h"
#include "proc_loadavg.h"
#include "proc_net_dev.h"
#include "infiniband.h"
#include "proc_interrupts.h"
#include "filesys.h"
#include "swapdev.h"
#include "proc_net_rpc.h"
#include "proc_net_sockstat.h"
#include "proc_net_tcp.h"
#include "proc_pid.h"
#include "proc_partitions.h"
#include "proc_runq.h"
#include "proc_net_snmp.h"
#include "proc_scsi.h"
#include "proc_fs_xfs.h"
#include "proc_slabinfo.h"
#include "proc_uptime.h"
#include "sem_limits.h"
#include "msg_limits.h"
#include "shm_limits.h"
#include "ksym.h"
#include "proc_sys_fs.h"
#include "proc_vmstat.h"

/*
 * Some metrics are exported by the kernel as "unsigned long".
 * On most 64bit platforms this is not the same size as an
 * "unsigned int". 
 */
#if defined(HAVE_64BIT_LONG)
#define KERNEL_ULONG PM_TYPE_U64
#define _pm_assign_ulong(atomp, val) do { (atomp)->ull = (val); } while (0)
#else
#define KERNEL_ULONG PM_TYPE_U32
#define _pm_assign_ulong(atomp, val) do { (atomp)->ul = (val); } while (0)
#endif

/*
 * Some metrics need to have their type set at runtime, based on the
 * running kernel version (not simply a 64 vs 32 bit machine issue).
 */
#define KERNEL_UTYPE PM_TYPE_NOSUPPORT	/* set to real type at runtime */
#define _pm_metric_type(type, size) \
    do { \
	(type) = ((size)==8 ? PM_TYPE_U64 : PM_TYPE_U32); \
    } while (0)
#define _pm_assign_utype(size, atomp, val) \
    do { \
	if ((size)==8) { (atomp)->ull = (val); } else { (atomp)->ul = (val); } \
    } while (0)

static proc_stat_t		proc_stat;
static proc_meminfo_t		proc_meminfo;
static proc_loadavg_t		proc_loadavg;
static proc_interrupts_t	proc_interrupts;
static filesys_t		filesys;
static swapdev_t		swapdev;
static proc_net_rpc_t		proc_net_rpc;
static proc_net_tcp_t		proc_net_tcp;
static proc_net_sockstat_t	proc_net_sockstat;
static proc_pid_t		proc_pid;
static struct utsname		kernel_uname;
static char 			uname_string[sizeof(kernel_uname)];
static char			*distro_name = NULL;
static proc_runq_t		proc_runq;
static proc_net_snmp_t		proc_net_snmp;
static proc_scsi_t		proc_scsi;
static proc_fs_xfs_t		proc_fs_xfs;
static proc_cpuinfo_t		proc_cpuinfo;
static proc_slabinfo_t		proc_slabinfo;
static sem_limits_t		sem_limits;
static msg_limits_t		msg_limits;
static shm_limits_t		shm_limits;
static proc_uptime_t		proc_uptime;
static proc_sys_fs_t		proc_sys_fs;
static proc_vmstat_t		proc_vmstat;

static int		_isDSO = 1;	/* =0 I am a daemon */

/* globals */
size_t _pm_system_pagesize; /* for hinv.pagesize and used elsewhere */
int _pm_have_proc_vmstat; /* if /proc/vmstat is available */
int _pm_intr_size; /* size in bytes of interrupt sum count metric */
int _pm_ctxt_size; /* size in bytes of context switch count metric */
int _pm_cputime_size; /* size in bytes of most of the cputime metrics */
int _pm_idletime_size; /* size in bytes of the idle cputime metric */

/*
 * Metric Instance Domains (statically initialized ones only)
 */
static pmdaInstid loadavg_indom_id[] = {
    { 1, "1 minute" }, { 5, "5 minute" }, { 15, "15 minute" }
};

static pmdaInstid nfs_indom_id[] = {
	{ 0, "null" },
	{ 1, "getattr" },
	{ 2, "setattr" },
	{ 3, "root" },
	{ 4, "lookup" },
	{ 5, "readlink" },
	{ 6, "read" },
	{ 7, "wrcache" },
	{ 8, "write" },
	{ 9, "create" },
	{ 10, "remove" },
	{ 11, "rename" },
	{ 12, "link" },
	{ 13, "symlink" },
	{ 14, "mkdir" },
	{ 15, "rmdir" },
	{ 16, "readdir" },
	{ 17, "statfs" }
};

static pmdaInstid nfs3_indom_id[] = {
	{ 0, "null" },
	{ 1, "getattr" },
	{ 2, "setattr" },
	{ 3, "lookup" },
	{ 4, "access" },
	{ 5, "readlink" },
	{ 6, "read" },
	{ 7, "write" },
	{ 8, "create" },
	{ 9, "mkdir" },
	{ 10, "symlink" },
	{ 11, "mknod" },
	{ 12, "remove" },
	{ 13, "rmdir" },
	{ 14, "rename" },
	{ 15, "link" },
	{ 16, "readdir" },
	{ 17, "readdir+" },
	{ 18, "statfs" },
	{ 19, "fsinfo" },
	{ 20, "pathconf" },
	{ 21, "commit" }
};

pmdaIndom indomtab[] = {
    { CPU_INDOM, 0, NULL },
    { DISK_INDOM, 0, NULL }, /* cached */
    { LOADAVG_INDOM, 3, loadavg_indom_id },
    { NET_DEV_INDOM, 0, NULL },
    { PROC_INTERRUPTS_INDOM, 0, NULL },
    { FILESYS_INDOM, 0, NULL },
    { SWAPDEV_INDOM, 0, NULL },
    { NFS_INDOM, NR_RPC_COUNTERS,  nfs_indom_id},
    { NFS3_INDOM, NR_RPC3_COUNTERS,  nfs3_indom_id},
    { PROC_INDOM, 0,  NULL},
    { PARTITIONS_INDOM, 0,  NULL}, /* cached */
    { SCSI_INDOM, 0,  NULL},
    { SLAB_INDOM, 0,  NULL},
    { IB_INDOM, 0, NULL },
    { NET_INET_INDOM, 0, NULL },
};



/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric metrictab[] = {

/*
 * /proc/stat cluster
 */

/* kernel.percpu.cpu.user */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,0), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.nice */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,1), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.sys */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,2), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.idle */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,3), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,4), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,5), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,6), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,7), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.avactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,46), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.aveq */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,47), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.read_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,49), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.write_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,50), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.avactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.aveq */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.read_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.write_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* swap.pagesin */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* swap.pagesout */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* swap.in */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* swap.out */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.all.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,12), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.all.pswitch */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,13), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.all.sysfork */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,14), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },


/* kernel.all.cpu.user */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,20), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.nice */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,21), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.sys */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,22), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.idle */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,23), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,28), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,36), PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.percpu.cpu.wait.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,30), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,31), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* hinv.ncpu */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.ndisk */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* kernel.all.cpu.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,34), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.wait.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,35), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.hz */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,48), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,-1,1,0,PM_TIME_SEC,PM_COUNT_ONE) }, },

/*
 * /proc/uptime cluster
 * Uptime modified and idletime added by Mike Mason <mmlnx@us.ibm.com>
 */

/* kernel.all.uptime */
    { NULL,
      { PMDA_PMID(CLUSTER_UPTIME,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,1,1,0,PM_TIME_SEC,PM_COUNT_ONE) }, },

/* kernel.all.idletime */
    { NULL,
      { PMDA_PMID(CLUSTER_UPTIME,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,1,1,0,PM_TIME_SEC,PM_COUNT_ONE) }, },

/*
 * /proc/meminfo cluster
 */

/* mem.physmem */
    { NULL, 
      { PMDA_PMID(CLUSTER_MEMINFO,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.used */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.free */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.shared */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.bufmem */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.cached */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.active */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.inactive */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.swapCached */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.highTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.highFree */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.lowTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.lowFree */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.swapTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.swapFree */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.dirty */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.writeback */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.mapped */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.slab */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.committed_AS */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.pageTables */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.reverseMaps */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.cache_clean */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* swap.length */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* swap.used */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* swap.free */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* hinv.physmem */
    { NULL, 
      { PMDA_PMID(CLUSTER_MEMINFO,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0) }, },

/* mem.freemem */
    { NULL, 
      { PMDA_PMID(CLUSTER_MEMINFO,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* hinv.pagesize */
    { NULL, 
      { PMDA_PMID(CLUSTER_MEMINFO,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.util.other */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.active */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.inactive */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/*
 * /proc/slabinfo cluster
 */

    /* mem.slabinfo.objects.active */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,0), PM_TYPE_U64, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.objects.total */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,1), PM_TYPE_U64, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.objects.size */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,2), PM_TYPE_U32, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* mem.slabinfo.slabs.active */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,3), PM_TYPE_U32, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.slabs.total */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,4), PM_TYPE_U32, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.slabs.pages_per_slab */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,5), PM_TYPE_U32, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.slabs.objects_per_slab */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,6), PM_TYPE_U32, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* mem.slabinfo.slabs.total_size */
    { NULL,
      { PMDA_PMID(CLUSTER_SLAB,7), PM_TYPE_U64, SLAB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/*
 * /proc/loadavg cluster
 */

    /* kernel.all.load */
    { NULL,
      { PMDA_PMID(CLUSTER_LOADAVG,0), PM_TYPE_FLOAT, LOADAVG_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* kernel.all.lastpid -- added by Mike Mason <mmlnx@us.ibm.com> */
    { NULL,
      { PMDA_PMID(CLUSTER_LOADAVG, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * /proc/net/dev cluster
 */

/* network.interface.in.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,0), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* network.interface.in.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,1), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,2), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.drops */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,3), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.fifo */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,4), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.frame */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,5), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.compressed */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,6), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.in.mcasts */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,7), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,8), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* network.interface.out.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,9), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,10), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.drops */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,11), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.fifo */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,12), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.collisions */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,13), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.carrier */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,14), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.out.compressed */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,15), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.total.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,16), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.interface.total.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,17), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.total.errors */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,18), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.total.drops */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,19), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.total.mcasts */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,20), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.interface.mtu */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,21), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.interface.speed */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,22), PM_TYPE_FLOAT, NET_DEV_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_MBYTE,0,0) }, },

/* network.interface.baudrate */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,23), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.interface.duplex */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,24), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.up */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,25), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.ipaddr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_INET,0), PM_TYPE_STRING, NET_INET_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/interrupts cluster
 */

/* kernel.percpu.interrupts */
    { NULL, 
      { PMDA_PMID(CLUSTER_INTERRUPTS, 0), PM_TYPE_U32, PROC_INTERRUPTS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.all.syscall */
    { NULL, 
      { PMDA_PMID(CLUSTER_INTERRUPTS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* kernel.percpu.syscall */
    { NULL, 
      { PMDA_PMID(CLUSTER_INTERRUPTS,2), PM_TYPE_U32, CPU_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * filesys cluster
 */

/* hinv.nmounts */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* filesys.capacity */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,1), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* filesys.used */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,2), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* filesys.free */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,3), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* filesys.maxfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,4), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_DISCRETE,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* filesys.usedfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,5), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* filesys.freefiles */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,6), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* filesys.mountdir */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,7), PM_TYPE_STRING, FILESYS_INDOM, PM_SEM_DISCRETE,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* filesys.full */
  { NULL,
     { PMDA_PMID(CLUSTER_FILESYS,8), PM_TYPE_DOUBLE, FILESYS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * Blocksize and avail added by Mike Mason <mmlnx@us.ibm.com>
 */

/* filesys.blocksize */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,9), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* filesys.avail -- space available to non-superusers */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,10), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/*
 * swapdev cluster
 */

/* swapdev.free */
  { NULL,
    { PMDA_PMID(CLUSTER_SWAPDEV,0), PM_TYPE_U32, SWAPDEV_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* swapdev.length */
  { NULL,
    { PMDA_PMID(CLUSTER_SWAPDEV,1), PM_TYPE_U32, SWAPDEV_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* swapdev.maxswap */
  { NULL,
    { PMDA_PMID(CLUSTER_SWAPDEV,2), PM_TYPE_U32, SWAPDEV_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* swapdev.vlength */
  { NULL,
    { PMDA_PMID(CLUSTER_SWAPDEV,3), PM_TYPE_U32, SWAPDEV_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* swapdev.priority */
  { NULL,
    { PMDA_PMID(CLUSTER_SWAPDEV,4), PM_TYPE_32, SWAPDEV_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * socket stat cluster
 */

/* network.sockstat.tcp.inuse */
  { &proc_net_sockstat.tcp[_PM_SOCKSTAT_INUSE],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.highest */
  { &proc_net_sockstat.tcp[_PM_SOCKSTAT_HIGHEST],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.util */
  { &proc_net_sockstat.tcp[_PM_SOCKSTAT_UTIL],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* network.sockstat.udp.inuse */
  { &proc_net_sockstat.udp[_PM_SOCKSTAT_INUSE],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udp.highest */
  { &proc_net_sockstat.udp[_PM_SOCKSTAT_HIGHEST],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udp.util */
  { &proc_net_sockstat.udp[_PM_SOCKSTAT_UTIL],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* network.sockstat.raw.inuse */
  { &proc_net_sockstat.raw[_PM_SOCKSTAT_INUSE],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.raw.highest */
  { &proc_net_sockstat.raw[_PM_SOCKSTAT_HIGHEST],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.raw.util */
  { &proc_net_sockstat.raw[_PM_SOCKSTAT_UTIL],
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * nfs cluster
 */

/* nfs.client.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.client.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,4), PM_TYPE_U32, NFS_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,12), PM_TYPE_U32, NFS_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs3.client.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs3.client.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,61), PM_TYPE_U32, NFS3_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs3.server.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs3.server.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,63), PM_TYPE_U32, NFS3_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.rpccnt */
  { &proc_net_rpc.client.rpccnt,
    { PMDA_PMID(CLUSTER_NET_NFS,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.rpcretrans */
  { &proc_net_rpc.client.rpcretrans,
    { PMDA_PMID(CLUSTER_NET_NFS,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.rpcauthrefresh */
  { &proc_net_rpc.client.rpcauthrefresh,
    { PMDA_PMID(CLUSTER_NET_NFS,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.netcnt */
  { &proc_net_rpc.client.netcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.netudpcnt */
  { &proc_net_rpc.client.netudpcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.nettcpcnt */
  { &proc_net_rpc.client.nettcpcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.client.nettcpconn */
  { &proc_net_rpc.client.nettcpconn,
    { PMDA_PMID(CLUSTER_NET_NFS,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rpccnt */
  { &proc_net_rpc.server.rpccnt,
    { PMDA_PMID(CLUSTER_NET_NFS,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rpcerr */
  { &proc_net_rpc.server.rpcerr,
    { PMDA_PMID(CLUSTER_NET_NFS,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rpcbadfmt */
  { &proc_net_rpc.server.rpcbadfmt,
    { PMDA_PMID(CLUSTER_NET_NFS,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rpcbadauth */
  { &proc_net_rpc.server.rpcbadauth,
    { PMDA_PMID(CLUSTER_NET_NFS,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rpcbadclnt */
  { &proc_net_rpc.server.rpcbadclnt,
    { PMDA_PMID(CLUSTER_NET_NFS,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rchits */
  { &proc_net_rpc.server.rchits,
    { PMDA_PMID(CLUSTER_NET_NFS,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rcmisses */
  { &proc_net_rpc.server.rcmisses,
    { PMDA_PMID(CLUSTER_NET_NFS,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.rcnocache */
  { &proc_net_rpc.server.rcnocache,
    { PMDA_PMID(CLUSTER_NET_NFS,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_cached */
  { &proc_net_rpc.server.fh_cached,
    { PMDA_PMID(CLUSTER_NET_NFS,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_valid */
  { &proc_net_rpc.server.fh_valid,
    { PMDA_PMID(CLUSTER_NET_NFS,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_fixup */
  { &proc_net_rpc.server.fh_fixup,
    { PMDA_PMID(CLUSTER_NET_NFS,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_lookup */
  { &proc_net_rpc.server.fh_lookup,
    { PMDA_PMID(CLUSTER_NET_NFS,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_stale */
  { &proc_net_rpc.server.fh_stale,
    { PMDA_PMID(CLUSTER_NET_NFS,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_concurrent */
  { &proc_net_rpc.server.fh_concurrent,
    { PMDA_PMID(CLUSTER_NET_NFS,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.netcnt */
  { &proc_net_rpc.server.netcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.netudpcnt */
  { &proc_net_rpc.server.netudpcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.nettcpcnt */
  { &proc_net_rpc.server.nettcpcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.nettcpconn */
  { &proc_net_rpc.server.nettcpcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * proc/<pid>/stat cluster
 */

/* proc.nprocs */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,99), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.pid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,0), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.cmd */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,1), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sname */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,2), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.ppid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,3), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.pgrp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,4), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.session */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,5), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.tty */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,6), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.tty_pgrp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,7), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.flags */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,8), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.minflt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,9), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cmin_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,10), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.maj_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,11), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cmaj_flt */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,12), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.utime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,13), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.stime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,14), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.cutime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,15), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.cstime */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,16), KERNEL_ULONG, PROC_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* proc.psinfo.priority */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,17), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.nice */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,18), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

#if 0
/* invalid field */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,19), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },
#endif

/* proc.psinfo.it_real_value */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,20), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.start_time */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,21), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) } },

/* proc.psinfo.vsize */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,22), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,23), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.rss_rlim */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,24), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.psinfo.start_code */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,25), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.end_code */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,26), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.start_stack */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,27), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.esp */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,28), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.eip */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,29), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.signal */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,30), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.blocked */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,31), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sigignore */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,32), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.sigcatch */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,33), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.wchan */
#if defined(HAVE_64BIT_PTR)
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,34), PM_TYPE_U64, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },
#elif defined(HAVE_32BIT_PTR)
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,34), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },
#else
    error! unsupported pointer size
#endif

/* proc.psinfo.nswap */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,35), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.cnswap */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,36), PM_TYPE_U32, PROC_INDOM, PM_SEM_COUNTER, 
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.psinfo.exit_signal */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,37), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.processor -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,38), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.ttyname */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,39), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* proc.psinfo.wchan_s -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,40), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.psargs -- modified by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STAT,41), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * proc/<pid>/status cluster
 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
 */

/* proc.id.uid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,0), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.euid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,1), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.suid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,2), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsuid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,3), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.gid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,4), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.egid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,5), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.sgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,6), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsgid */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,7), PM_TYPE_U32, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.uid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,8), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.euid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,9), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.suid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,10), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsuid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,11), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.gid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,12), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.egid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,13), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.sgid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,14), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.id.fsgid_nm */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,15), PM_TYPE_STRING, PROC_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.signal_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,16), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.blocked_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,17), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.sigignore_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,18), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.psinfo.sigcatch_s */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,19), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* proc.memory.vmsize */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,20), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmlock */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,21), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,22), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmdata */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,23), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmstack */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,24), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmexe */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,25), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/* proc.memory.vmlib */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATUS,26), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0)}},

/*
 * proc/<pid>/statm cluster
 */

/* proc.memory.size */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,0), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,1), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.share */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,2), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.textrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,3), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.librss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,4), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.datrss */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,5), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.dirty */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,6), PM_TYPE_U32, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* proc.memory.maps -- added by Mike Mason <mmlnx@us.ibm.com> */
  { NULL,
    { PMDA_PMID(CLUSTER_PID_STATM,7), PM_TYPE_STRING, PROC_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * /proc/partitions cluster
 */

/* disk.partitions.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,0), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,1), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,2), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,3), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,4), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,5), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,6), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,7), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,8), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,38), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,39), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,40), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/*
 * kernel_uname cluster
 */

/* kernel.uname.release */
  { kernel_uname.release,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.version */
  { kernel_uname.version,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.sysname */
  { kernel_uname.sysname,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.machine */
  { kernel_uname.machine,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.nodename */
  { kernel_uname.nodename,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 4), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* pmda.uname */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 5), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* pmda.version */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 6), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.distro */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * proc.runq cluster
 */

/* proc.runq.runnable */
  { &proc_runq.runnable,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.blocked */
  { &proc_runq.blocked,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.sleeping */
  { &proc_runq.sleeping,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.stopped */
  { &proc_runq.stopped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.swapped */
  { &proc_runq.swapped,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.defunct */
  { &proc_runq.defunct,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.unknown */
  { &proc_runq.unknown,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.runq.kernel */
  { &proc_runq.kernel,
    { PMDA_PMID(CLUSTER_PROC_RUNQ, 7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },


/*
 * network snmp cluster
 */

/* network.ip.forwarding */
  { &proc_net_snmp.ip[_PM_SNMP_IP_FORWARDING], 
    { PMDA_PMID(CLUSTER_NET_SNMP,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.defaultttl */
  { &proc_net_snmp.ip[_PM_SNMP_IP_DEFAULTTTL], 
    { PMDA_PMID(CLUSTER_NET_SNMP,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inreceives */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INRECEIVES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inhdrerrors */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INHDRERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inaddrerrors */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INADDRERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.forwdatagrams */
  { &proc_net_snmp.ip[_PM_SNMP_IP_FORWDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inunknownprotos */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INUNKNOWNPROTOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.indiscards */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INDISCARDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.indelivers */
  { &proc_net_snmp.ip[_PM_SNMP_IP_INDELIVERS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outrequests */
  { &proc_net_snmp.ip[_PM_SNMP_IP_OUTREQUESTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outdiscards */
  { &proc_net_snmp.ip[_PM_SNMP_IP_OUTDISCARDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outnoroutes */
  { &proc_net_snmp.ip[_PM_SNMP_IP_OUTNOROUTES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmtimeout */
  { &proc_net_snmp.ip[_PM_SNMP_IP_REASMTIMEOUT], 
    { PMDA_PMID(CLUSTER_NET_SNMP,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmreqds */
  { &proc_net_snmp.ip[_PM_SNMP_IP_REASMREQDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmoks */
  { &proc_net_snmp.ip[_PM_SNMP_IP_REASMOKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmfails */
  { &proc_net_snmp.ip[_PM_SNMP_IP_REASMFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragoks */
  { &proc_net_snmp.ip[_PM_SNMP_IP_FRAGOKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragfails */
  { &proc_net_snmp.ip[_PM_SNMP_IP_FRAGFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragcreates */
  { &proc_net_snmp.ip[_PM_SNMP_IP_FRAGCREATES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },


/* network.icmp.inmsgs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INMSGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inerrors */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.indestunreachs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INDESTUNREACHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimeexcds */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMEEXCDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inparmprobs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INPARMPROBS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.insrcquenchs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INSRCQUENCHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inredirects */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INREDIRECTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inechos */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inechoreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimestamps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimestampreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inaddrmasks */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inaddrmaskreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outmsgs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTMSGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outerrors */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outdestunreachs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTDESTUNREACHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimeexcds */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMEEXCDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outparmprobs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTPARMPROBS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outsrcquenchs */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTSRCQUENCHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outredirects */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTREDIRECTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outechos */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outechoreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimestamps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimestampreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outaddrmasks */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outaddrmaskreps */
  { &proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },


/* network.tcp.rtoalgorithm */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_RTOALGORITHM], 
    { PMDA_PMID(CLUSTER_NET_SNMP,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rtomin */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMIN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rtomax */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMAX], 
    { PMDA_PMID(CLUSTER_NET_SNMP,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.maxconn */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_MAXCONN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
			
/* proc.tcp.established */
  { &proc_net_tcp.stat[_PM_TCP_ESTABLISHED],
    { PMDA_PMID(CLUSTER_NET_TCP, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.syn_sent */
  { &proc_net_tcp.stat[_PM_TCP_SYN_SENT],
    { PMDA_PMID(CLUSTER_NET_TCP, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.syn_recv */
  { &proc_net_tcp.stat[_PM_TCP_SYN_RECV],
    { PMDA_PMID(CLUSTER_NET_TCP, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.fin_wait1 */
  { &proc_net_tcp.stat[_PM_TCP_FIN_WAIT1],
    { PMDA_PMID(CLUSTER_NET_TCP, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.fin_wait2 */
  { &proc_net_tcp.stat[_PM_TCP_FIN_WAIT2],
    { PMDA_PMID(CLUSTER_NET_TCP, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.time_wait */
  { &proc_net_tcp.stat[_PM_TCP_TIME_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP, 6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.close */
  { &proc_net_tcp.stat[_PM_TCP_CLOSE],
    { PMDA_PMID(CLUSTER_NET_TCP, 7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.close_wait */
  { &proc_net_tcp.stat[_PM_TCP_CLOSE_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP, 8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.last_ack */
  { &proc_net_tcp.stat[_PM_TCP_LAST_ACK],
    { PMDA_PMID(CLUSTER_NET_TCP, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.listen */
  { &proc_net_tcp.stat[_PM_TCP_LISTEN],
    { PMDA_PMID(CLUSTER_NET_TCP, 10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* proc.tcp.closing */
  { &proc_net_tcp.stat[_PM_TCP_CLOSING],
    { PMDA_PMID(CLUSTER_NET_TCP, 11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.activeopens */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_ACTIVEOPENS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.passiveopens */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_PASSIVEOPENS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.attemptfails */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_ATTEMPTFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.estabresets */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_ESTABRESETS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.currestab */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_CURRESTAB], 
    { PMDA_PMID(CLUSTER_NET_SNMP,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.insegs */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_INSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.outsegs */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_OUTSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.retranssegs */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_RETRANSSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.inerrs */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_INERRS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.outrsts */
  { &proc_net_snmp.tcp[_PM_SNMP_TCP_OUTRSTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.indatagrams */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_INDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.noports */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_NOPORTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.inerrors */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_INERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.outdatagrams */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_OUTDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,74), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.recvbuferrors */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_RECVBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,75), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.sndbuferrors */
  { &proc_net_snmp.udp[_PM_SNMP_UDP_SNDBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,76), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.indatagrams */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INDATAGRAMS],
    { PMDA_PMID(CLUSTER_NET_SNMP,77), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.noports */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_NOPORTS],
    { PMDA_PMID(CLUSTER_NET_SNMP,78), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.inerrors */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.outdatagrams */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_OUTDATAGRAMS],
    { PMDA_PMID(CLUSTER_NET_SNMP,80), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.recvbuferrors */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_RECVBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,81), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.sndbuferrors */
  { &proc_net_snmp.udplite[_PM_SNMP_UDPLITE_SNDBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,82), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* hinv.map.scsi */
    { NULL, 
      { PMDA_PMID(CLUSTER_SCSI,0), PM_TYPE_STRING, SCSI_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/fs/xfs/stat cluster
 */

/* xfs.allocs.alloc_extent */
    { &proc_fs_xfs.xs_allocx,
      { PMDA_PMID(CLUSTER_XFS,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.alloc_block */
    { &proc_fs_xfs.xs_allocb,
      { PMDA_PMID(CLUSTER_XFS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_extent*/
    { &proc_fs_xfs.xs_freex,
      { PMDA_PMID(CLUSTER_XFS,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.allocs.free_block */
    { &proc_fs_xfs.xs_freeb,
      { PMDA_PMID(CLUSTER_XFS,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.alloc_btree.lookup */
    { &proc_fs_xfs.xs_abt_lookup,
      { PMDA_PMID(CLUSTER_XFS,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.compare */
    { &proc_fs_xfs.xs_abt_compare,
      { PMDA_PMID(CLUSTER_XFS,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.insrec */
    { &proc_fs_xfs.xs_abt_insrec,
      { PMDA_PMID(CLUSTER_XFS,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.alloc_btree.delrec */
    { &proc_fs_xfs.xs_abt_delrec,
      { PMDA_PMID(CLUSTER_XFS,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.block_map.read_ops */
    { &proc_fs_xfs.xs_blk_mapr,
      { PMDA_PMID(CLUSTER_XFS,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.write_ops */
    { &proc_fs_xfs.xs_blk_mapw,
      { PMDA_PMID(CLUSTER_XFS,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.unmap */
    { &proc_fs_xfs.xs_blk_unmap,
      { PMDA_PMID(CLUSTER_XFS,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.add_exlist */
    { &proc_fs_xfs.xs_add_exlist,
      { PMDA_PMID(CLUSTER_XFS,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.del_exlist */
    { &proc_fs_xfs.xs_del_exlist,
      { PMDA_PMID(CLUSTER_XFS,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.look_exlist */
    { &proc_fs_xfs.xs_look_exlist,
      { PMDA_PMID(CLUSTER_XFS,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.block_map.cmp_exlist */
    { &proc_fs_xfs.xs_cmp_exlist,
      { PMDA_PMID(CLUSTER_XFS,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.bmap_btree.lookup */
    { &proc_fs_xfs.xs_bmbt_lookup,
      { PMDA_PMID(CLUSTER_XFS,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.compare */
    { &proc_fs_xfs.xs_bmbt_compare,
      { PMDA_PMID(CLUSTER_XFS,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.insrec */
    { &proc_fs_xfs.xs_bmbt_insrec,
      { PMDA_PMID(CLUSTER_XFS,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.bmap_btree.delrec */
    { &proc_fs_xfs.xs_bmbt_delrec,
      { PMDA_PMID(CLUSTER_XFS,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.dir_ops.lookup */
    { &proc_fs_xfs.xs_dir_lookup,
      { PMDA_PMID(CLUSTER_XFS,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.create */
    { &proc_fs_xfs.xs_dir_create,
      { PMDA_PMID(CLUSTER_XFS,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.remove */
    { &proc_fs_xfs.xs_dir_remove,
      { PMDA_PMID(CLUSTER_XFS,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.dir_ops.getdents */
    { &proc_fs_xfs.xs_dir_getdents,
      { PMDA_PMID(CLUSTER_XFS,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.transactions.sync */
    { &proc_fs_xfs.xs_trans_sync,
      { PMDA_PMID(CLUSTER_XFS,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.async */
    { &proc_fs_xfs.xs_trans_async,
      { PMDA_PMID(CLUSTER_XFS,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.transactions.empty */
    { &proc_fs_xfs.xs_trans_empty,
      { PMDA_PMID(CLUSTER_XFS,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.inode_ops.ig_attempts */
    { &proc_fs_xfs.xs_ig_attempts,
      { PMDA_PMID(CLUSTER_XFS,26), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_found */
    { &proc_fs_xfs.xs_ig_found,
      { PMDA_PMID(CLUSTER_XFS,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_frecycle */
    { &proc_fs_xfs.xs_ig_frecycle,
      { PMDA_PMID(CLUSTER_XFS,28), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_missed */
    { &proc_fs_xfs.xs_ig_missed,
      { PMDA_PMID(CLUSTER_XFS,29), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_dup */
    { &proc_fs_xfs.xs_ig_dup,
      { PMDA_PMID(CLUSTER_XFS,30), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_reclaims */
    { &proc_fs_xfs.xs_ig_reclaims,
      { PMDA_PMID(CLUSTER_XFS,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.inode_ops.ig_attrchg */
    { &proc_fs_xfs.xs_ig_attrchg,
      { PMDA_PMID(CLUSTER_XFS,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.writes */
    { &proc_fs_xfs.xs_log_writes,
      { PMDA_PMID(CLUSTER_XFS,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.blocks */
    { &proc_fs_xfs.xs_log_blocks,
      { PMDA_PMID(CLUSTER_XFS,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
/* xfs.log.noiclogs */
    { &proc_fs_xfs.xs_log_noiclogs,
      { PMDA_PMID(CLUSTER_XFS,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force */
    { &proc_fs_xfs.xs_log_force,
      { PMDA_PMID(CLUSTER_XFS,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log.force_sleep */
    { &proc_fs_xfs.xs_log_force_sleep,
      { PMDA_PMID(CLUSTER_XFS,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log_tail.try_logspace */
    { &proc_fs_xfs.xs_try_logspace,
      { PMDA_PMID(CLUSTER_XFS,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.sleep_logspace */
    { &proc_fs_xfs.xs_sleep_logspace,
      { PMDA_PMID(CLUSTER_XFS,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushes */
    { &proc_fs_xfs.xs_push_ail,
      { PMDA_PMID(CLUSTER_XFS,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.success */
    { &proc_fs_xfs.xs_push_ail_success,
      { PMDA_PMID(CLUSTER_XFS,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pushbuf */
    { &proc_fs_xfs.xs_push_ail_pushbuf,
      { PMDA_PMID(CLUSTER_XFS,42), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.pinned */
    { &proc_fs_xfs.xs_push_ail_pinned,
      { PMDA_PMID(CLUSTER_XFS,43), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.locked */
    { &proc_fs_xfs.xs_push_ail_locked,
      { PMDA_PMID(CLUSTER_XFS,44), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flushing */
    { &proc_fs_xfs.xs_push_ail_flushing,
      { PMDA_PMID(CLUSTER_XFS,45), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.restarts */
    { &proc_fs_xfs.xs_push_ail_restarts,
      { PMDA_PMID(CLUSTER_XFS,46), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.log_tail.push_ail.flush */
    { &proc_fs_xfs.xs_push_ail_flush,
      { PMDA_PMID(CLUSTER_XFS,47), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.xstrat.bytes */
    { &proc_fs_xfs.xpc.xs_xstrat_bytes,
      { PMDA_PMID(CLUSTER_XFS,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.xstrat.quick */
    { &proc_fs_xfs.xs_xstrat_quick,
      { PMDA_PMID(CLUSTER_XFS,49), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.xstrat.split */
    { &proc_fs_xfs.xs_xstrat_split,
      { PMDA_PMID(CLUSTER_XFS,50), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.write */
    { &proc_fs_xfs.xs_write_calls,
      { PMDA_PMID(CLUSTER_XFS,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.write_bytes */
    { &proc_fs_xfs.xpc.xs_write_bytes,
      { PMDA_PMID(CLUSTER_XFS,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
/* xfs.read */
    { &proc_fs_xfs.xs_read_calls,
      { PMDA_PMID(CLUSTER_XFS,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.read_bytes */
    { &proc_fs_xfs.xpc.xs_read_bytes,
      { PMDA_PMID(CLUSTER_XFS,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* xfs.attr.get */
    { &proc_fs_xfs.xs_attr_get,
      { PMDA_PMID(CLUSTER_XFS,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.set */
    { &proc_fs_xfs.xs_attr_set,
      { PMDA_PMID(CLUSTER_XFS,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.remove */
    { &proc_fs_xfs.xs_attr_remove,
      { PMDA_PMID(CLUSTER_XFS,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.attr.list */
    { &proc_fs_xfs.xs_attr_list,
      { PMDA_PMID(CLUSTER_XFS,58), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.quota.reclaims */
    { &proc_fs_xfs.xs_qm_dqreclaims,
      { PMDA_PMID(CLUSTER_XFS,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.reclaim_misses */
    { &proc_fs_xfs.xs_qm_dqreclaim_misses,
      { PMDA_PMID(CLUSTER_XFS,60), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.dquot_dups */
    { &proc_fs_xfs.xs_qm_dquot_dups,
      { PMDA_PMID(CLUSTER_XFS,61), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachemisses */
    { &proc_fs_xfs.xs_qm_dqcachemisses,
      { PMDA_PMID(CLUSTER_XFS,62), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.cachehits */
    { &proc_fs_xfs.xs_qm_dqcachehits,
      { PMDA_PMID(CLUSTER_XFS,63), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.wants */
    { &proc_fs_xfs.xs_qm_dqwants,
      { PMDA_PMID(CLUSTER_XFS,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.shake_reclaims */
    { &proc_fs_xfs.xs_qm_dqshake_reclaims,
      { PMDA_PMID(CLUSTER_XFS,65), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.quota.inact_reclaims */
    { &proc_fs_xfs.xs_qm_dqinact_reclaims,
      { PMDA_PMID(CLUSTER_XFS,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.iflush_count */
    { &proc_fs_xfs.xs_iflush_count,
      { PMDA_PMID(CLUSTER_XFS,67), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushcnt */
    { &proc_fs_xfs.xs_icluster_flushcnt,
      { PMDA_PMID(CLUSTER_XFS,68), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.icluster_flushinode */
    { &proc_fs_xfs.xs_icluster_flushinode,
      { PMDA_PMID(CLUSTER_XFS,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.buffer.get */
    { &proc_fs_xfs.xs_buf_get,
      { PMDA_PMID(CLUSTER_XFSBUF,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.create */
    { &proc_fs_xfs.xs_buf_create,
      { PMDA_PMID(CLUSTER_XFSBUF,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked */
    { &proc_fs_xfs.xs_buf_get_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_locked_waited */
    { &proc_fs_xfs.xs_buf_get_locked_waited,
      { PMDA_PMID(CLUSTER_XFSBUF,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.busy_locked */
    { &proc_fs_xfs.xs_buf_busy_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.miss_locked */
    { &proc_fs_xfs.xs_buf_miss_locked,
      { PMDA_PMID(CLUSTER_XFSBUF,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_retries */
    { &proc_fs_xfs.xs_buf_page_retries,
      { PMDA_PMID(CLUSTER_XFSBUF,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.page_found */         
    { &proc_fs_xfs.xs_buf_page_found,
      { PMDA_PMID(CLUSTER_XFSBUF,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.buffer.get_read */         
    { &proc_fs_xfs.xs_buf_get_read,
      { PMDA_PMID(CLUSTER_XFSBUF,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.vnodes.active */
    { &proc_fs_xfs.vnodes.vn_active,
      { PMDA_PMID(CLUSTER_XFS,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.vnodes.alloc */
    { &proc_fs_xfs.vnodes.vn_alloc,
      { PMDA_PMID(CLUSTER_XFS,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.get */
    { &proc_fs_xfs.vnodes.vn_get,
      { PMDA_PMID(CLUSTER_XFS,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.hold */
    { &proc_fs_xfs.vnodes.vn_hold,
      { PMDA_PMID(CLUSTER_XFS,73), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.rele */
    { &proc_fs_xfs.vnodes.vn_rele,
      { PMDA_PMID(CLUSTER_XFS,74), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.reclaim */
    { &proc_fs_xfs.vnodes.vn_reclaim,
      { PMDA_PMID(CLUSTER_XFS,75), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.remove */
    { &proc_fs_xfs.vnodes.vn_remove,
      { PMDA_PMID(CLUSTER_XFS,76), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
/* xfs.vnodes.free */
    { &proc_fs_xfs.vnodes.vn_free,
      { PMDA_PMID(CLUSTER_XFS,77), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* xfs.log.write_ratio */
    { &proc_fs_xfs.xs_log_write_ratio,
      { PMDA_PMID(CLUSTER_XFS,78), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
/* xfs.control.reset */
    { NULL,
      { PMDA_PMID(CLUSTER_XFS,79), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/cpuinfo cluster (cpu indom)
 */

/* hinv.cpu.clock */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 0), PM_TYPE_FLOAT, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,1,0,-6,0) } },

/* hinv.cpu.vendor */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 1), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.model */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 2), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.stepping */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 3), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.cache */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 4), PM_TYPE_U32, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,PM_SPACE_KBYTE,0,0) } },

/* hinv.cpu.bogomips */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 5), PM_TYPE_FLOAT, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.map.cpu */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 6), PM_TYPE_U32, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.machine */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * semaphore limits cluster
 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
 */

/* ipc.sem.max_semmap */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_semid */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_sem */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.num_undo */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_perid */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_ops */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_undoent */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.sz_semundo */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_semval */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.max_exit */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_LIMITS, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * message limits cluster
 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
 */

/* ipc.msg.sz_pool */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0, PM_SPACE_KBYTE,0,0)}},

/* ipc.msg.mapent */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.max_msgsz */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.max_defmsgq */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.max_msgqid */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.max_msgseg */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0, 0,0,0)}},

/* ipc.msg.max_smsghdr */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.max_seg */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_LIMITS, 7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * shared memory limits cluster
 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
 */

/* ipc.shm.max_segsz */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_LIMITS, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.min_segsz */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_LIMITS, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.max_seg */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_LIMITS, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.shm.max_segproc */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_LIMITS, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.shm.max_shmsys */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_LIMITS, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * number of users cluster
 */

/* kernel.all.nusers */
  { NULL,
    { PMDA_PMID(CLUSTER_NUSERS, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/*
 * /proc/sys/fs vfs cluster
 */

/* vfs.files */
    { &proc_sys_fs.fs_files_count,
      { PMDA_PMID(CLUSTER_VFS,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_files_free,
      { PMDA_PMID(CLUSTER_VFS,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_files_max,
      { PMDA_PMID(CLUSTER_VFS,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_inodes_count,
      { PMDA_PMID(CLUSTER_VFS,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_inodes_free,
      { PMDA_PMID(CLUSTER_VFS,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_dentry_count,
      { PMDA_PMID(CLUSTER_VFS,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_dentry_free,
      { PMDA_PMID(CLUSTER_VFS,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /*
     * mem.vmstat cluster
     */

    /* mem.vmstat.nr_dirty */
    { &proc_vmstat.nr_dirty,
    {PMDA_PMID(28,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_writeback */
    { &proc_vmstat.nr_writeback,
    {PMDA_PMID(28,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_unstable */
    { &proc_vmstat.nr_unstable,
    {PMDA_PMID(28,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_page_table_pages */
    { &proc_vmstat.nr_page_table_pages,
    {PMDA_PMID(28,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_mapped */
    { &proc_vmstat.nr_mapped,
    {PMDA_PMID(28,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /*
     * mem.vmstat.nr_slab - replaced by nr_slab_reclaimable and
     * nr_slab_unreclaimable in 2.6.18 kernels
     */
    { &proc_vmstat.nr_slab,
    {PMDA_PMID(28,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.pgpgin */
    { &proc_vmstat.pgpgin,
    {PMDA_PMID(28,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgpgout */
    { &proc_vmstat.pgpgout,
    {PMDA_PMID(28,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpin */
    { &proc_vmstat.pswpin,
    {PMDA_PMID(28,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpout */
    { &proc_vmstat.pswpout,
    {PMDA_PMID(28,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_high */
    { &proc_vmstat.pgalloc_high,
    {PMDA_PMID(28,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_normal */
    { &proc_vmstat.pgalloc_normal,
    {PMDA_PMID(28,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_dma */
    { &proc_vmstat.pgalloc_dma,
    {PMDA_PMID(28,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfree */
    { &proc_vmstat.pgfree,
    {PMDA_PMID(28,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgactivate */
    { &proc_vmstat.pgactivate,
    {PMDA_PMID(28,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgdeactivate */
    { &proc_vmstat.pgdeactivate,
    {PMDA_PMID(28,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfault */
    { &proc_vmstat.pgfault,
    {PMDA_PMID(28,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgmajfault */
    { &proc_vmstat.pgmajfault,
    {PMDA_PMID(28,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_high */
    { &proc_vmstat.pgrefill_high,
    {PMDA_PMID(28,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_normal */
    { &proc_vmstat.pgrefill_normal,
    {PMDA_PMID(28,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_dma */
    { &proc_vmstat.pgrefill_dma,
    {PMDA_PMID(28,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_high */
    { &proc_vmstat.pgsteal_high,
    {PMDA_PMID(28,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_normal */
    { &proc_vmstat.pgsteal_normal,
    {PMDA_PMID(28,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_dma */
    { &proc_vmstat.pgsteal_dma,
    {PMDA_PMID(28,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_high */
    { &proc_vmstat.pgscan_kswapd_high,
    {PMDA_PMID(28,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_normal */
    { &proc_vmstat.pgscan_kswapd_normal,
    {PMDA_PMID(28,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_dma */
    { &proc_vmstat.pgscan_kswapd_dma,
    {PMDA_PMID(28,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_high */
    { &proc_vmstat.pgscan_direct_high,
    {PMDA_PMID(28,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_normal */
    { &proc_vmstat.pgscan_direct_normal,
    {PMDA_PMID(28,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_dma */
    { &proc_vmstat.pgscan_direct_dma,
    {PMDA_PMID(28,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pginodesteal */
    { &proc_vmstat.pginodesteal,
    {PMDA_PMID(28,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.slabs_scanned */
    { &proc_vmstat.slabs_scanned,
    {PMDA_PMID(28,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_steal */
    { &proc_vmstat.kswapd_steal,
    {PMDA_PMID(28,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_inodesteal */
    { &proc_vmstat.kswapd_inodesteal,
    {PMDA_PMID(28,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pageoutrun */
    { &proc_vmstat.pageoutrun,
    {PMDA_PMID(28,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall */
    { &proc_vmstat.allocstall,
    {PMDA_PMID(28,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrotated */
    { &proc_vmstat.pgrotated,
    {PMDA_PMID(28,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_slab_reclaimable */
    { &proc_vmstat.nr_slab_reclaimable,
    {PMDA_PMID(28,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_slab_unreclaimable */
    { &proc_vmstat.nr_slab_unreclaimable,
    {PMDA_PMID(28,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_anon_pages */
    { &proc_vmstat.nr_anon_pages,
    {PMDA_PMID(28,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_bounce */
    { &proc_vmstat.nr_bounce,
    {PMDA_PMID(28,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_file_pages */
    { &proc_vmstat.nr_file_pages,
    {PMDA_PMID(28,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_vmscan_write */
    { &proc_vmstat.nr_vmscan_write,
    {PMDA_PMID(28,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * infiniband cluster
 */

/* network.ib.in.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,0), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* network.ib.in.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,1), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.in.errors.drop */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,2), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.in.errors.filter */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,3), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.in.errors.local */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,4), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.in.errors.remote */    { NULL,
      { PMDA_PMID(CLUSTER_IB,5), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.out.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,6), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* network.ib.out.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,7), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.out.errors.drop */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,8), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.out.errors.filter */    { NULL,
      { PMDA_PMID(CLUSTER_IB,9), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,16), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.ib.total.packets */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,17), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.drop */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,18), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.filter */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,19), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.link */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,10), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.recover */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,11), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.integrity */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,12), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.vl15 */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,13), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.overrun */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,14), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.total.errors.symbol */
    { NULL,
      { PMDA_PMID(CLUSTER_IB,15), PM_TYPE_U64, IB_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.ib.status */
    { NULL, 
      { PMDA_PMID(CLUSTER_IB,20), PM_TYPE_STRING, IB_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

#define IB_COUNTERS_IN   6
#define IB_COUNTERS_ALL  20  /* includes synthetic counters */

};

static void
linux_refresh(int *need_refresh)
{
    if (need_refresh[CLUSTER_PARTITIONS])
    	refresh_proc_partitions(INDOM(DISK_INDOM), INDOM(PARTITIONS_INDOM));

    if (need_refresh[CLUSTER_STAT])
    	refresh_proc_stat(&proc_cpuinfo, &proc_stat);

    if (need_refresh[CLUSTER_CPUINFO])
    	refresh_proc_cpuinfo(&proc_cpuinfo);

    if (need_refresh[CLUSTER_MEMINFO])
	refresh_proc_meminfo(&proc_meminfo);

    if (need_refresh[CLUSTER_LOADAVG])
	refresh_proc_loadavg(&proc_loadavg);

    if (need_refresh[CLUSTER_INTERRUPTS])
	refresh_proc_interrupts(&proc_interrupts);

    if (need_refresh[CLUSTER_NET_DEV])
	refresh_proc_net_dev(INDOM(NET_DEV_INDOM));

    if (need_refresh[CLUSTER_NET_INET])
	refresh_net_dev_inet(INDOM(NET_INET_INDOM));

    if (need_refresh[CLUSTER_FILESYS])
	refresh_filesys(&filesys);

    if (need_refresh[CLUSTER_SWAPDEV])
	refresh_swapdev(&swapdev);

    if (need_refresh[CLUSTER_NET_NFS])
	refresh_proc_net_rpc(&proc_net_rpc);

    if (need_refresh[CLUSTER_NET_SOCKSTAT])
    	refresh_proc_net_sockstat(&proc_net_sockstat);

    if (need_refresh[CLUSTER_PID_STAT] || need_refresh[CLUSTER_PID_STATM] || 
        need_refresh[CLUSTER_PID_STATUS])
	refresh_proc_pid(&proc_pid);

    if (need_refresh[CLUSTER_KERNEL_UNAME])
    	uname(&kernel_uname);

    if (need_refresh[CLUSTER_PROC_RUNQ])
	refresh_proc_runq(&proc_runq);

    if (need_refresh[CLUSTER_NET_SNMP])
	refresh_proc_net_snmp(&proc_net_snmp);

    if (need_refresh[CLUSTER_SCSI])
	refresh_proc_scsi(&proc_scsi);

    if (need_refresh[CLUSTER_XFS])
    	refresh_proc_fs_xfs(&proc_fs_xfs);

    if (need_refresh[CLUSTER_NET_TCP])
	refresh_proc_net_tcp(&proc_net_tcp);

    if (need_refresh[CLUSTER_SLAB])
	refresh_proc_slabinfo(&proc_slabinfo);

    if (need_refresh[CLUSTER_SEM_LIMITS])
        refresh_sem_limits(&sem_limits);

    if (need_refresh[CLUSTER_MSG_LIMITS])
        refresh_msg_limits(&msg_limits);

    if (need_refresh[CLUSTER_SHM_LIMITS])
        refresh_shm_limits(&shm_limits);

    if (need_refresh[CLUSTER_UPTIME])
        refresh_proc_uptime(&proc_uptime);

    if (need_refresh[CLUSTER_VFS])
    	refresh_proc_sys_fs(&proc_sys_fs);

    if (need_refresh[CLUSTER_VMSTAT])
    	refresh_proc_vmstat(&proc_vmstat);

    if (need_refresh[CLUSTER_IB])
	refresh_ib(INDOM(IB_INDOM));
}

static int
linux_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS];
    char		newname[8];

    memset(need_refresh, 0, sizeof(need_refresh));
    switch (indomp->serial) {
    case DISK_INDOM:
    case PARTITIONS_INDOM:
	need_refresh[CLUSTER_PARTITIONS]++;
	break;
    case CPU_INDOM:
    	need_refresh[CLUSTER_STAT]++;
	break;
    case LOADAVG_INDOM:
    	need_refresh[CLUSTER_LOADAVG]++;
	break;
    case NET_DEV_INDOM:
    	need_refresh[CLUSTER_NET_DEV]++;
	break;
    case PROC_INTERRUPTS_INDOM:
    	need_refresh[CLUSTER_INTERRUPTS]++;
	break;
    case FILESYS_INDOM:
    	need_refresh[CLUSTER_FILESYS]++;
	break;
    case SWAPDEV_INDOM:
    	need_refresh[CLUSTER_SWAPDEV]++;
	break;
    case NFS_INDOM:
    case NFS3_INDOM:
    	need_refresh[CLUSTER_NET_NFS]++;
	break;
    case PROC_INDOM:
    	need_refresh[CLUSTER_PID_STAT]++;
    	need_refresh[CLUSTER_PID_STATM]++;
        need_refresh[CLUSTER_PID_STATUS]++;
        break;
    case SCSI_INDOM:
    	need_refresh[CLUSTER_SCSI]++;
	break;
    case SLAB_INDOM:
    	need_refresh[CLUSTER_SLAB]++;
	break;
    case IB_INDOM:
    	need_refresh[CLUSTER_IB]++;
	break;
    /* no default label : pmdaInstance will pick up errors */
    }

    if (indomp->serial == PROC_INDOM && inst == PM_IN_NULL && name != NULL) {
    	/*
	 * For the proc indom, if the name is a pid (as a string), and it
	 * contains only digits (i.e. it's not a full instance name) then
	 * reformat it to be exactly six digits, with leading zeros.
	 */
	char *p;
	for (p = name; *p != '\0'; p++) {
	    if (!isdigit(*p))
	    	break;
	}
	if (*p == '\0') {
	    sprintf(newname, "%06d", atoi(name));
	    name = newname;
	}
    }

    linux_refresh(need_refresh);
    return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * callback provided to pmdaFetch
 */

static int
linux_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int			i;
    int			sts;
    char		*f;
    long		sl;
    unsigned long	ul;
    proc_pid_entry_t	*entry;
    net_inet_t		*inetp;
    net_interface_t	*netip;
    ib_port_t		*ibportp;

    if (mdesc->m_user != NULL) {
	/* 
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not all metrics support this - those that
	 * don't have NULL for the m_user field in their respective
         * metrictab slot.
	 */
	if (idp->cluster == CLUSTER_VMSTAT) {
	    if (!_pm_have_proc_vmstat || *(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
	    	return 0; /* no value available on this kernel */
	}
	else
	if (idp->cluster == CLUSTER_NET_NFS) {
	    /*
	     * check if rpc stats are available
	     */
	    if (idp->item >= 20 && idp->item <= 27 && proc_net_rpc.client.errcode != 0)
		/* no values available for client rpc/nfs - this is expected <= 2.0.36 */
	    	return 0;
	    else
	    if (idp->item >= 30 && idp->item <= 47 && proc_net_rpc.server.errcode != 0)
		/* no values available - this is expected < 2.2.8 without /proc/net/rpc/nfsd */
	    	return 0; /* no values available */
	}
	else
	if (idp->cluster == CLUSTER_XFS && proc_fs_xfs.errcode != 0) {
	    /* no values available for XFS metrics */
	    return 0;
	}

	switch (mdesc->m_desc.type) {
	case PM_TYPE_32:
	    atom->l = *(__int32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)mdesc->m_user;
	    break;
	case PM_TYPE_64:
	    atom->ll = *(__int64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)mdesc->m_user;
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)mdesc->m_user;
	    break;
	case PM_TYPE_DOUBLE:
	    atom->d = *(double *)mdesc->m_user;
	    break;
	case PM_TYPE_STRING:
	    atom->cp = (char *)mdesc->m_user;
	    break;
	case PM_TYPE_AGGREGATE:
	case PM_TYPE_AGGREGATE_STATIC:
	    atom->vp = (void *)mdesc->m_user;
	    break;

	case PM_TYPE_NOSUPPORT:
	    return 0;

	case PM_TYPE_UNKNOWN:
	default:
	    fprintf(stderr, "error in linux_fetchCallBack : unknown metric type %d\n", mdesc->m_desc.type);
	    return 0;
	}
    }
    else
    switch (idp->cluster) {
    case CLUSTER_STAT:
	/*
	 * All metrics from /proc/stat
	 */
	switch (idp->item) {
	case 0: /* user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_user[inst] / proc_stat.hz);
	    break;
	case 1: /* nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_nice[inst] / proc_stat.hz);
	    break;
	case 2: /* sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_sys[inst] / proc_stat.hz);
	    break;
	case 3: /* idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.p_idle[inst] / proc_stat.hz);
	    break;

	case 8: /* pagesin */
	    if (_pm_have_proc_vmstat)
		atom->ul = proc_vmstat.pswpin;
	    else
		atom->ul = proc_stat.swap[0];
	    break;
	case 9: /* pagesout */
	    if (_pm_have_proc_vmstat)
		atom->ul = proc_vmstat.pswpout;
	    else
		atom->ul = proc_stat.swap[1];
	    break;
	case 10: /* in */
	    if (_pm_have_proc_vmstat)
		atom->ul = proc_vmstat.pgpgin;
	    else
		atom->ul = proc_stat.page[0];
	    break;
	case 11: /* out */
	    if (_pm_have_proc_vmstat)
		atom->ul = proc_vmstat.pgpgout;
	    else
		atom->ul = proc_stat.page[1];
	    break;
	case 12: /* intr */
	    _pm_assign_utype(_pm_intr_size, atom, proc_stat.intr);
	    break;
	case 13: /* ctxt */
	    _pm_assign_utype(_pm_ctxt_size, atom, proc_stat.ctxt);
	    break;
	case 14: /* processes */
	    _pm_assign_ulong(atom, proc_stat.processes);
	    break;

	/* gilly - change the calculation to prevent a bug */
	case 20: /* all.user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.user / proc_stat.hz);
	    break;
	case 21: /* all.nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.nice / proc_stat.hz);
	    break;
	case 22: /* all.sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.sys / proc_stat.hz);
	    break;
	case 23: /* all.idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.idle / proc_stat.hz);
	    break;

	case 30: /* kernel.percpu.cpu.wait.total */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_wait[inst] / proc_stat.hz);
	    break;
	case 31: /* kernel.percpu.cpu.intr */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.p_irq[inst] +
				(double)proc_stat.p_sirq[inst]) / proc_stat.hz);
	    break;
	case 34: /* kernel.all.cpu.intr */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.irq +
			      	(double)proc_stat.sirq) / proc_stat.hz);
	    break;
	case 35: /* kernel.all.cpu.wait.total */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.wait / proc_stat.hz);
	    break;
	case 32: /* hinv.ncpu */
	    atom->ul = indomtab[CPU_INDOM].it_numinst;
	    break;
	case 33: /* hinv.ndisk */
	    atom->ul = pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;

	case 48: /* kernel.all.hz */
	    atom->ul = proc_stat.hz;
	    break;

	default:
	    /*
	     * Disk metrics used to be fetched from /proc/stat (2.2 kernels)
	     * but have since moved to /proc/partitions (2.4 kernels) and
	     * /proc/diskstats (2.6 kernels). We preserve the cluster number
	     * (middle bits of a PMID) for backward compatibility.
	     *
	     * Note that proc_partitions_fetch() will return PM_ERR_PMID
	     * if we have tried to fetch an unknown metric.
	     */
	    return proc_partitions_fetch(mdesc, inst, atom);
	}
	break;

    case CLUSTER_UPTIME:  /* uptime */
	switch (idp->item) {
	case 0:
	    /*
	     * kernel.all.uptime (in seconds)
	     * contributed by "gilly" <gilly@exanet.com>
	     * modified by Mike Mason" <mmlnx@us.ibm.com>
	     */
	    atom->ul = proc_uptime.uptime;
	    break;
	case 1:
	    /*
	     * kernel.all.idletime (in seconds)
	     * contributed by "Mike Mason" <mmlnx@us.ibm.com>
	     */
	    atom->ul = proc_uptime.idletime;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_MEMINFO: /* mem */

#define VALID_VALUE(x)		((x) != (uint64_t)-1)
#define VALUE_OR_ZERO(x)	(((x) == (uint64_t)-1) ? 0 : (x))

    	switch (idp->item) {
	case 0: /* mem.physmem (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemTotal >> 10;
	    break;
	case 1: /* mem.util.used (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.MemTotal) ||
	        !VALID_VALUE(proc_meminfo.MemFree))
		return 0; /* no values available */
	    atom->ull = (proc_meminfo.MemTotal - proc_meminfo.MemFree) >> 10;
	    break;
	case 2: /* mem.util.free (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree >> 10;
	    break;
	case 3: /* mem.util.shared (in kbytes) */
	    /*
	     * If this metric is exported by the running kernel, it is always
	     * zero (deprecated). PCP exports it for compatibility with older
	     * PCP monitoring tools, e.g. pmgsys running on IRIX(TM).
	     */
	    if (!VALID_VALUE(proc_meminfo.MemShared))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemShared >> 10;
	    break;
	case 4: /* mem.util.bufmem (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Buffers))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Buffers >> 10;
	    break;
	case 5: /* mem.util.cached (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Cached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Cached >> 10;
	    break;
	case 6: /* swap.length (in bytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal;
	    break;
	case 7: /* swap.used (in bytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapTotal) ||
	        !VALID_VALUE(proc_meminfo.SwapFree))
		return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal - proc_meminfo.SwapFree;
	    break;
	case 8: /* swap.free (in bytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree;
	    break;
	case 9: /* hinv.physmem (in mbytes) */
	    if (!VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ul = proc_meminfo.MemTotal >> 20;
	    break;
	case 10: /* mem.freemem (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree >> 10;
	    break;
	case 11: /* hinv.pagesize (in bytes) */
	    atom->ul = _pm_system_pagesize;
	    break;
	case 12: /* mem.util.other (in kbytes) */
	    /* other = used - (cached+buffers) */
	    if (!VALID_VALUE(proc_meminfo.MemTotal) ||
	        !VALID_VALUE(proc_meminfo.MemFree) ||
	        !VALID_VALUE(proc_meminfo.Cached) ||
	        !VALID_VALUE(proc_meminfo.Buffers))
		return 0; /* no values available */
	    sl = (proc_meminfo.MemTotal -
		 proc_meminfo.MemFree -
		 proc_meminfo.Cached -
		 proc_meminfo.Buffers) >> 10;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	case 13: /* mem.util.swapCached (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapCached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapCached >> 10;
	    break;
	case 14: /* mem.active (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Active))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Active >> 10;
	    break;
	case 15: /* mem.inactive (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Inactive))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Inactive >> 10;
	    break;
	case 16: /* mem.util.highTotal (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.HighTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighTotal >> 10;
	    break;
	case 17: /* mem.util.highFree (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.HighFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighFree >> 10;
	    break;
	case 18: /* mem.util.lowTotal (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.LowTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowTotal >> 10;
	    break;
	case 19: /* mem.util.lowFree (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.LowFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowFree >> 10;
	    break;
	case 20: /* mem.util.swapTotal (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal >> 10;
	    break;
	case 21: /* mem.util.swapFree (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree >> 10;
	    break;
	case 22: /* mem.util.dirty (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Dirty))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Dirty >> 10;
	    break;
	case 23: /* mem.util.writeback (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Writeback >> 10;
	    break;
	case 24: /* mem.util.mapped (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Mapped))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Mapped >> 10;
	    break;
	case 25: /* mem.util.slab (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Slab))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Slab >> 10;
	    break;
	case 26: /* mem.util.committed_AS (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.Committed_AS))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Committed_AS >> 10;
	    break;
	case 27: /* mem.util.pageTables (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.PageTables))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.PageTables >> 10;
	    break;
	case 28: /* mem.util.reverseMaps (in kbytes) */
	    if (!VALID_VALUE(proc_meminfo.ReverseMaps))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.ReverseMaps >> 10;
	    break;
	case 29: /* mem.util.clean_cache (in kbytes) */
	    /* clean=cached-(dirty+writeback) */
	    if (!VALID_VALUE(proc_meminfo.Cached) ||
	        !VALID_VALUE(proc_meminfo.Dirty) ||
	        !VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    sl = (proc_meminfo.Cached -
	    	 proc_meminfo.Dirty -
	    	 proc_meminfo.Writeback) >> 10;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_LOADAVG: 
	switch(idp->item) {
	case 0:  /* kernel.all.loadavg */
	    if (inst == 1)
	    	atom->f = proc_loadavg.loadavg[0];
	    else
	    if (inst == 5)
	    	atom->f = proc_loadavg.loadavg[1];
	    else
	    if (inst == 15)
	    	atom->f = proc_loadavg.loadavg[2];
	    else
	    	return PM_ERR_INST;
	    break;
	case 1: /* kernel.all.lastpid -- added by "Mike Mason" <mmlnx@us.ibm.com> */
		atom->ul = proc_loadavg.lastpid;
		break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_NET_DEV: /* network.interface */
	sts = pmdaCacheLookup(indomtab[NET_DEV_INDOM].it_indom, inst, NULL, (void **)&netip);
	if (sts < 0)
	    return sts;
	if (idp->item >= 0 && idp->item <= 15) {
	    /* network.interface.{in,out} */
	    atom->ull = netip->counters[idp->item];
	}
	else
	switch (idp->item) {
	case 16: /* network.interface.total.bytes */
	    atom->ull = netip->counters[0] + netip->counters[8];
	    break;
	case 17: /* network.interface.total.packets */
	    atom->ull = netip->counters[1] + netip->counters[9];
	    break;
	case 18: /* network.interface.total.errors */
	    atom->ull = netip->counters[2] + netip->counters[10];
	    break;
	case 19: /* network.interface.total.drops */
	    atom->ull = netip->counters[3] + netip->counters[11];
	    break;
	case 20: /* network.interface.total.mcasts */
	    /*
	     * NOTE: there is no network.interface.out.mcasts metric
	     * so this total only includes network.interface.in.mcasts
	     */
	    atom->ull = netip->counters[7];
	    break;
	case 21: /* network.interface.mtu */
	    if (!netip->ioc.mtu)
		return 0;
	    atom->ul = netip->ioc.mtu;
	    break;
	case 22: /* network.interface.speed */
	    if (!netip->ioc.speed)
		return 0;
	    atom->f = ((float)netip->ioc.speed * 1000000) / 1024 / 1024;
	    break;
	case 23: /* network.interface.baudrate */
	    if (!netip->ioc.speed)
		return 0;
	    atom->ul = ((long long)netip->ioc.speed * 1024 * 1024 / 10);
	    break;
	case 24: /* network.interface.duplex */
	    if (!netip->ioc.duplex)
		return 0;
	    atom->ul = netip->ioc.duplex;
	    break;
	case 25: /* network.interface.up */
	    atom->ul = netip->ioc.linkup;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_NET_INET:
	sts = pmdaCacheLookup(indomtab[NET_INET_INDOM].it_indom, inst, NULL, (void **)&inetp);
	if (sts < 0)
	    return sts;
	switch (idp->item) {
	case 0: /* network.interface.inet_addr */
	    if (!inetp->hasip)
		return 0;
	    if ((atom->cp = inet_ntoa(inetp->addr)) == NULL)
		return 0;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_INTERRUPTS:
	switch (idp->item) {
	case 0: /* kernel.percpu.interrupts */
	    if (proc_interrupts.nstats == 0)
		return 0; /* no values available */
	    for (i=0; i < proc_interrupts.nstats; i++) {
		if (proc_interrupts.stats[i].valid && proc_interrupts.stats[i].id == inst) {
		    atom->ul = proc_interrupts.stats[i].count;
		    break;
		}
	    }
	    if (i == proc_interrupts.nstats)
		return PM_ERR_INST;
	    break;

	case 1: /* kernel.all.syscall */
	    if (proc_interrupts.ncpus == 0)
		return 0; /* need syscall-acct patch, so no values available */
	    for (atom->ul=0, i=0; i < proc_interrupts.ncpus; i++) {
		atom->ul += proc_interrupts.syscall[i];
	    }
	    break;

	case 2: /* kernel.percpu.syscall */
	    if (proc_interrupts.ncpus == 0)
		return 0; /* need syscall-acct patch, so no values available */
	    if (inst < 0 || inst >= proc_interrupts.ncpus)
		return PM_ERR_INST;
	    atom->ul = proc_interrupts.syscall[inst];
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_XFS:
	switch (idp->item) {
	case 79: /* xfs.control.reset */
	    atom->ul = 0;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_FILESYS:
	if (idp->item == 0) {
	    atom->ul = indomtab[FILESYS_INDOM].it_numinst;
	}
	else {
	    struct statfs *sbuf;

	    if (filesys.nmounts == 0)
	    	return 0; /* no values available */
	    for (i=0; i < filesys.nmounts; i++) {
	    	if (filesys.mounts[i].valid && filesys.mounts[i].id == inst)
		    break;
	    }
	    if (i == filesys.nmounts)
	    	return PM_ERR_INST;

	    sbuf = &filesys.mounts[i].stats;
	    if (filesys.mounts[i].fetched == 0) {
		if (statfs(filesys.mounts[i].path, sbuf) < 0)
		    return -errno;
		filesys.mounts[i].fetched = 1;
	    }

	    switch (idp->item) {
	    __uint64_t	ull, used;

	    case 1: /* filesys.capacity */
	    	ull = (__uint64_t)sbuf->f_blocks;
	    	atom->ull = ull * sbuf->f_bsize / 1024;
		break;
	    case 2: /* filesys.used */
	    	used = (__uint64_t)(sbuf->f_blocks - sbuf->f_bfree);
	    	atom->ull = used * sbuf->f_bsize / 1024;
		break;
	    case 3: /* filesys.free */
	    	ull = (__uint64_t)sbuf->f_bfree;
	    	atom->ull = ull * sbuf->f_bsize / 1024;
		break;
	    case 4: /* filesys.maxfiles */
	    	atom->ul = sbuf->f_files;
		break;
	    case 5: /* filesys.usedfiles */
	    	atom->ul = sbuf->f_files - sbuf->f_ffree;
		break;
	    case 6: /* filesys.freefiles */
	    	atom->ul = sbuf->f_ffree;
		break;
	    case 7: /* filesys.mountdir */
	    	atom->cp = filesys.mounts[i].path;
		break;
	    case 8: /* filesys.full */
		used = (__uint64_t)(sbuf->f_blocks - sbuf->f_bfree);
		ull = used + (__uint64_t)sbuf->f_bavail;
		atom->d = (100.0 * (double)used) / (double)ull;
		break;
	    case 9: /* filesys.blocksize -- added by Mike Mason <mmlnx@us.ibm.com> */
		atom->ul = sbuf->f_bsize;
		break;
	    case 10: /* filesys.avail -- added by Mike Mason <mmlnx@us.ibm.com> */
		ull = (__uint64_t)sbuf->f_bavail;
		atom->ull = ull * sbuf->f_bsize / 1024;
		break;
	    default:
		return PM_ERR_PMID;
	    }
	}
	break;

    case CLUSTER_SWAPDEV:
	if (swapdev.nswaps == 0)
	    return 0; /* no values available */
	for (i=0; i < swapdev.nswaps; i++) {
	    if (swapdev.swaps[i].valid && swapdev.swaps[i].id == inst)
		break;
	}

	if (i == swapdev.nswaps)
	    return PM_ERR_INST;

	switch (idp->item) {
	case 0: /* swapdev.free (kbytes) */
	    atom->ul = swapdev.swaps[i].size - swapdev.swaps[i].used;
	    break;
	case 1: /* swapdev.length (kbytes) */
	case 2: /* swapdev.maxswap (kbytes) */
	    atom->ul = swapdev.swaps[i].size;
	    break;
	case 3: /* swapdev.vlength (kbytes) */
	    atom->ul = 0;
	    break;
	case 4: /* swapdev.priority */
	    atom->l = swapdev.swaps[i].priority;
	    break;
	default:
	    return PM_ERR_PMID;
	}

	break;

    case CLUSTER_NET_NFS:
	switch (idp->item) {
	case 1: /* nfs.client.calls */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC_COUNTERS; i++) {
		atom->ul += proc_net_rpc.client.reqcounts[i];
	    }
	    break;
	case 50: /* nfs.server.calls */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC_COUNTERS; i++) {
		atom->ul += proc_net_rpc.server.reqcounts[i];
	    }
	    break;
	case 4: /* nfs.client.reqs */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    if (inst >= 0 && inst < NR_RPC_COUNTERS)
		atom->ul = proc_net_rpc.client.reqcounts[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 12: /* nfs.server.reqs */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    if (inst >= 0 && inst < NR_RPC_COUNTERS)
		atom->ul = proc_net_rpc.server.reqcounts[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 60: /* nfs3.client.calls */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC3_COUNTERS; i++) {
		atom->ul += proc_net_rpc.client.reqcounts3[i];
	    }
	    break;

	case 62: /* nfs3.server.calls */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC3_COUNTERS; i++) {
		atom->ul += proc_net_rpc.server.reqcounts3[i];
	    }
	    break;

	case 61: /* nfs3.client.reqs */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    if (inst >= 0 && inst < NR_RPC3_COUNTERS)
		atom->ul = proc_net_rpc.client.reqcounts3[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 63: /* nfs3.server.reqs */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    if (inst >= 0 && inst < NR_RPC3_COUNTERS)
		atom->ul = proc_net_rpc.server.reqcounts3[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	/*
	 * Note: all other rpc metric values are extracted directly via the
	 * address specified in the metrictab (see above)
	 */
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PID_STAT:
	if (idp->item == 99) /* proc.nprocs */
	    atom->ul = proc_pid.indom->it_numinst;
	else {
	    static char ttyname[MAXPATHLEN];
	    extern char *get_ttyname_info(int, dev_t, char *);

	    if ((entry = fetch_proc_pid_stat(inst, &proc_pid)) == NULL)
	    	return PM_ERR_INST;

	    switch (idp->item) {


	    case PROC_PID_STAT_PID:
	    	atom->ul = entry->id;
		break;

	    case PROC_PID_STAT_TTYNAME:
		if ((f = _pm_getfield(entry->stat_buf, PROC_PID_STAT_TTY)) == NULL)
		    atom->cp = "?";
		else {
		    dev_t dev = (dev_t)atoi(f);
		    atom->cp = get_ttyname_info(inst, dev, ttyname);
		}
		break;

		case PROC_PID_STAT_CMD:
			if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
				return PM_ERR_INST;
			atom->cp = f + 1;
			atom->cp[strlen(atom->cp)-1] = '\0';
			break;

		case PROC_PID_STAT_PSARGS:
			atom->cp = entry->name + 7;
			break;

		case PROC_PID_STAT_STATE:
		/*
		 * string
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return PM_ERR_INST;
	    	atom->cp = f;
		break;

	    case PROC_PID_STAT_VSIZE:
	    case PROC_PID_STAT_RSS_RLIM:
		/*
		 * bytes converted to kbytes
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return PM_ERR_INST;
		sscanf(f, "%u", &atom->ul);
		atom->ul /= 1024;
		break;

		case PROC_PID_STAT_RSS:
		/*
		 * pages converted to kbytes
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
			return PM_ERR_INST;
		sscanf(f, "%u", &atom->ul);
		atom->ul *= _pm_system_pagesize / 1024;
		break;

	    case PROC_PID_STAT_UTIME:
	    case PROC_PID_STAT_STIME:
	    case PROC_PID_STAT_CUTIME:
	    case PROC_PID_STAT_CSTIME:
		/*
		 * unsigned jiffies converted to unsigned milliseconds
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return PM_ERR_INST;

		sscanf(f, "%lu", &ul);
		_pm_assign_ulong(atom, 1000 * (double)ul / proc_stat.hz);
		break;
	    
	    case PROC_PID_STAT_PRIORITY:
	    case PROC_PID_STAT_NICE:
		/*
		 * signed decimal int
		 */
		if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    return PM_ERR_INST;
		sscanf(f, "%d", &atom->l);
		break;

		case PROC_PID_STAT_WCHAN:
		case PROC_PID_STAT_WCHAN_SYMBOL: 
		{
		    char *wc;

		    if ((f = _pm_getfield(entry->stat_buf, PROC_PID_STAT_WCHAN)) == NULL)
			return PM_ERR_INST;
#if defined(HAVE_64BIT_PTR)
		    sscanf(f, "%lu", &atom->ull); /* 64bit address */
#else
		    sscanf(f, "%u", &atom->ul);    /* 32bit address */
#endif

		    /*
		     * Convert address to symbol name if requested
		     * Added by Mike Mason <mmlnx@us.ibm.com>
		     */
		    if (idp->item == PROC_PID_STAT_WCHAN_SYMBOL) {
#if defined(HAVE_64BIT_PTR)
			/* 64 bit address */
			if ((wc = wchan(atom->ull)))
			    atom->cp = strdup(wc);
			else
			    atom->cp = strdup(atom->ull ? f : "");
#else
			/* 32 bit address */
			if ((wc = wchan((__psint_t)atom->ul)))
			    atom->cp = strdup(wc);
			else
			    atom->cp = strdup(atom->ul ? f : "");
#endif
		    }
		}
		break;

		default:
		/*
		 * unsigned decimal int
		 */
		if (idp->item >= 0 && idp->item < NR_PROC_PID_STAT) {
		    if ((f = _pm_getfield(entry->stat_buf, idp->item)) == NULL)
		    	return PM_ERR_INST;
		    sscanf(f, "%u", &atom->ul);
		}
		else
		    return PM_ERR_PMID;
		break;
	    }
	}
	break;

    case CLUSTER_PID_STATM:

	if (idp->item == PROC_PID_STATM_MAPS) {	/* proc.memory.maps */
	    if ((entry = fetch_proc_pid_maps(inst, &proc_pid)) == NULL)
		    return PM_ERR_INST;
	    atom->cp = entry->maps_buf;
	} else {
	    if ((entry = fetch_proc_pid_statm(inst, &proc_pid)) == NULL)
		return PM_ERR_INST;

	    if (idp->item >= 0 && idp->item <= PROC_PID_STATM_DIRTY) {
		/* unsigned int */
		if ((f = _pm_getfield(entry->statm_buf, idp->item)) == NULL)
		    return PM_ERR_INST;
		sscanf(f, "%u", &atom->ul);
		atom->ul *= _pm_system_pagesize / 1024;
	    }
	    else
		return PM_ERR_PMID;
	}
    	break;

    case CLUSTER_SLAB:
	if (proc_slabinfo.ncaches == 0)
	    return 0; /* no values available */

	if (inst < 0 || inst >= proc_slabinfo.ncaches)
	    return PM_ERR_INST;

	switch(idp->item) {
	case 0:	/* mem.slabinfo.objects.active */
	    atom->ull = proc_slabinfo.caches[inst].num_active_objs;
	    break;
	case 1:	/* mem.slabinfo.objects.total */
	    atom->ull = proc_slabinfo.caches[inst].total_objs;
	    break;
	case 2:	/* mem.slabinfo.objects.size */
	    if (proc_slabinfo.caches[inst].seen < 11)	/* version 1.1 or later only */
		return 0;
	    atom->ul = proc_slabinfo.caches[inst].object_size;
	    break;
	case 3:	/* mem.slabinfo.slabs.active */
	    if (proc_slabinfo.caches[inst].seen < 11)	/* version 1.1 or later only */
		return 0;
	    atom->ul = proc_slabinfo.caches[inst].num_active_slabs;
	    break;
	case 4:	/* mem.slabinfo.slabs.total */
	    if (proc_slabinfo.caches[inst].seen == 11)	/* version 1.1 only */
		return 0;
	    atom->ul = proc_slabinfo.caches[inst].total_slabs;
	    break;
	case 5:	/* mem.slabinfo.slabs.pages_per_slab */
	    if (proc_slabinfo.caches[inst].seen < 11)	/* version 1.1 or later only */
		return 0;
	    atom->ul = proc_slabinfo.caches[inst].pages_per_slab;
	    break;
	case 6:	/* mem.slabinfo.slabs.objects_per_slab */
	    if (proc_slabinfo.caches[inst].seen != 20)	/* version 2.0 only */
		return 0;
	    atom->ul = proc_slabinfo.caches[inst].objects_per_slab;
	    break;
	case 7:	/* mem.slabinfo.slabs.total_size */
	    if (proc_slabinfo.caches[inst].seen < 11)	/* version 1.1 or later only */
		return 0;
	    atom->ull = proc_slabinfo.caches[inst].total_size;
	    break;
	default:
	    return PM_ERR_PMID;
	}
    	break;

    case CLUSTER_PARTITIONS:
	return proc_partitions_fetch(mdesc, inst, atom);

    case CLUSTER_SCSI:
	if (proc_scsi.nscsi == 0)
	    return 0; /* no values available */
	switch(idp->item) {
	case 0: /* hinv.map.scsi */
	    atom->cp = (char *)NULL;
	    for (i=0; i < proc_scsi.nscsi; i++) {
		if (proc_scsi.scsi[i].id == inst) {
		    atom->cp = proc_scsi.scsi[i].dev_name;
		    break;
		}
	    }
	    if (i == proc_scsi.nscsi)
	    	return PM_ERR_INST;
	    break;
	default:
	    return PM_ERR_PMID;
	}
    	break;

    case CLUSTER_KERNEL_UNAME:
	switch(idp->item) {
	case 5: /* pmda.uname */
	    sprintf(uname_string, "%s %s %s %s %s",
	    	kernel_uname.sysname, 
		kernel_uname.nodename,
		kernel_uname.release,
		kernel_uname.version,
		kernel_uname.machine);
	    atom->cp = uname_string;
	    break;

	case 6: /* pmda.version */
	    atom->cp = pmGetConfig("PCP_VERSION");
	    break;

	case 7:	/* kernel.uname.distro ... not from uname(2) */
	    if (distro_name == NULL) {
		/*
		 * Heuristic guesswork ... add stuff here as we learn
		 * more
		 */
		struct stat	sbuf;
		int		r, fd = -1, len = 0;
		char		prefix[16];
		char *rfiles[] = {
			"/etc/debian_version",
			"/etc/fedora-release",
			"/etc/redhat-release",
			"/etc/SuSE-release",
			NULL
		};
		for (r=0; rfiles[r] != NULL; r++) {
		    if (stat(rfiles[r], &sbuf) == 0 && S_ISREG(sbuf.st_mode)) {
			fd = open(rfiles[r], O_RDONLY);
		    }
		}
		if (fd != -1) {
		    if (r == 0) {	/* Debian, needs prefix */
			strncpy(prefix, "Debian ", sizeof(prefix));
			len = 7;
		    }
		    /*
		     * at this point, assume sbuf is good and file contains
		     * the string we want, probably with a \n terminator
		     */
		    distro_name = (char *)malloc(len + (int)sbuf.st_size + 1);
		    if (distro_name != NULL) {
			if (len)
			    strncpy(distro_name, prefix, len);
			r = read(fd, distro_name + len, (int)sbuf.st_size);
			close(fd);
			if (r <= 0) {
			    free (distro_name);
			    distro_name = NULL;
			} else {
			    char *nl;
			    distro_name[r] = '\0';
			    if ((nl = strchr (distro_name, '\n')) != NULL) {
				*nl = '\0';
			    }
			}
		    }
		}
		if (distro_name == NULL) 
		    distro_name = "?";
	    }
	    atom->cp = distro_name;
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_CPUINFO:
	if (idp->item != 7 && /* hinv.machine is singular */
	    (inst < 0 || inst >= proc_cpuinfo.cpuindom->it_numinst))
	    return PM_ERR_INST;
	switch(idp->item) {
	case 0: /* hinv.cpu.clock */
	    atom->f = proc_cpuinfo.cpuinfo[inst].clock;
	    break;
	case 1: /* hinv.cpu.vendor */
	    if ((atom->cp = proc_cpuinfo.cpuinfo[inst].vendor) == (char *)NULL)
	    	atom->cp = "unknown";
	    break;
	case 2: /* hinv.cpu.model */
	    if ((atom->cp = proc_cpuinfo.cpuinfo[inst].model) == (char *)NULL)
	    	atom->cp = "unknown";
	    break;
	case 3: /* hinv.cpu.stepping */
	    if ((atom->cp = proc_cpuinfo.cpuinfo[inst].stepping) == (char *)NULL)
	    	atom->cp = "unknown";
	    break;
	case 4: /* hinv.cpu.cache */
	    atom->ul = proc_cpuinfo.cpuinfo[inst].cache;
	    break;
	case 5: /* hinv.cpu.bogomips */
	    atom->f = proc_cpuinfo.cpuinfo[inst].bogomips;
	    break;
	case 6: /* hinv.map.cpu_num */
	    atom->ul = proc_cpuinfo.cpuinfo[inst].cpu_num;
	    break;
	case 7: /* hinv.machine */
	    atom->cp = proc_cpuinfo.machine;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

	/*
	 * Cluster added by Mike Mason <mmlnx@us.ibm.com>
	 */
    case CLUSTER_PID_STATUS:
	if ((entry = fetch_proc_pid_status(inst, &proc_pid)) == NULL)
		return PM_ERR_INST;

	switch (idp->item) {

	case PROC_PID_STATUS_UID:
	case PROC_PID_STATUS_EUID:
	case PROC_PID_STATUS_SUID:
	case PROC_PID_STATUS_FSUID:
	case PROC_PID_STATUS_UID_NM:
	case PROC_PID_STATUS_EUID_NM:
	case PROC_PID_STATUS_SUID_NM:
	case PROC_PID_STATUS_FSUID_NM:
	{
	    struct passwd *pwe;

	    if ((f = _pm_getfield(entry->status_lines.uid, (idp->item % 4) + 1)) == NULL)
		return PM_ERR_INST;
	    sscanf(f, "%u", &atom->ul);
	    if (idp->item > PROC_PID_STATUS_FSUID) {
		if ((pwe = getpwuid((uid_t)atom->ul)) != NULL)
		    atom->cp = strdup(pwe->pw_name);
		else
		    atom->cp = "UNKNOWN";
	    }
	}
	break;

	case PROC_PID_STATUS_GID:
	case PROC_PID_STATUS_EGID:
	case PROC_PID_STATUS_SGID:
	case PROC_PID_STATUS_FSGID:
	case PROC_PID_STATUS_GID_NM:
	case PROC_PID_STATUS_EGID_NM:
	case PROC_PID_STATUS_SGID_NM:
	case PROC_PID_STATUS_FSGID_NM:
	{
	    struct group *gre;

	    if ((f = _pm_getfield(entry->status_lines.gid, (idp->item % 4) + 1)) == NULL)
		return PM_ERR_INST;
	    sscanf(f, "%u", &atom->ul);
	    if (idp->item > PROC_PID_STATUS_FSGID) {
		if ((gre = getgrgid((gid_t)atom->ul)) != NULL) {
		    atom->cp = strdup(gre->gr_name);
		} else {
		    atom->cp = "UNKNOWN";
		}
	    }
	}
	break;

	case PROC_PID_STATUS_SIGNAL:
	if ((atom->cp = _pm_getfield(entry->status_lines.sigpnd, 1)) == NULL)
	    return PM_ERR_INST;
	break;

	case PROC_PID_STATUS_BLOCKED:
	if ((atom->cp = _pm_getfield(entry->status_lines.sigblk, 1)) == NULL)
	    return PM_ERR_INST;
	break;

	case PROC_PID_STATUS_SIGCATCH:
	if ((atom->cp = _pm_getfield(entry->status_lines.sigcgt, 1)) == NULL)
	    return PM_ERR_INST;
	break;

	case PROC_PID_STATUS_SIGIGNORE:
	if ((atom->cp = _pm_getfield(entry->status_lines.sigign, 1)) == NULL)
	    return PM_ERR_INST;
	break;

	case PROC_PID_STATUS_VMSIZE:
	if ((f = _pm_getfield(entry->status_lines.vmsize, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMLOCK:
	if ((f = _pm_getfield(entry->status_lines.vmlck, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMRSS:
	if ((f = _pm_getfield(entry->status_lines.vmrss, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMDATA:
	if ((f = _pm_getfield(entry->status_lines.vmdata, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMSTACK:
	if ((f = _pm_getfield(entry->status_lines.vmstk, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMEXE:
	if ((f = _pm_getfield(entry->status_lines.vmexe, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	case PROC_PID_STATUS_VMLIB:
	if ((f = _pm_getfield(entry->status_lines.vmlib, 1)) == NULL)
	    atom->ul = 0;
	else
	    sscanf(f, "%u", &atom->ul);
	break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_SEM_LIMITS:
	switch (idp->item) {
	case 0:	/* ipc.sem.max_semmap */
	    atom->ul = sem_limits.semmap;
	    break;
	case 1:	/* ipc.sem.max_semid */
	    atom->ul = sem_limits.semmni;
	    break;
	case 2:	/* ipc.sem.max_sem */
	    atom->ul = sem_limits.semmns;
	    break;
	case 3:	/* ipc.sem.num_undo */
	    atom->ul = sem_limits.semmnu;
	    break;
	case 4:	/* ipc.sem.max_perid */
	    atom->ul = sem_limits.semmsl;
	    break;
	case 5:	/* ipc.sem.max_ops */
	    atom->ul = sem_limits.semopm;
	    break;
	case 6:	/* ipc.sem.max_undoent */
	    atom->ul = sem_limits.semume;
	    break;
	case 7:	/* ipc.sem.sz_semundo */
	    atom->ul = sem_limits.semusz;
	    break;
	case 8:	/* ipc.sem.max_semval */
	    atom->ul = sem_limits.semvmx;
	    break;
	case 9:	/* ipc.sem.max_exit */
	    atom->ul = sem_limits.semaem;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_MSG_LIMITS:
	switch (idp->item) {
	case 0:	/* ipc.msg.sz_pool */
	    atom->ul = msg_limits.msgpool;
	    break;
	case 1:	/* ipc.msg.mapent */
	    atom->ul = msg_limits.msgmap;
	    break;
	case 2:	/* ipc.msg.max_msgsz */
	    atom->ul = msg_limits.msgmax;
	    break;
	case 3:	/* ipc.msg.max_defmsgq */
	    atom->ul = msg_limits.msgmnb;
	    break;
	case 4:	/* ipc.msg.max_msgqid */
	    atom->ul = msg_limits.msgmni;
	    break;
	case 5:	/* ipc.msg.sz_msgseg */
	    atom->ul = msg_limits.msgssz;
	    break;
	case 6:	/* ipc.msg.num_smsghdr */
	    atom->ul = msg_limits.msgtql;
	    break;
	case 7:	/* ipc.msg.max_seg */
	    atom->ul = (unsigned long) msg_limits.msgseg;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_SHM_LIMITS:
	switch (idp->item) {
	case 0:	/* ipc.shm.max_segsz */
	    atom->ul = shm_limits.shmmax;
	    break;
	case 1:	/* ipc.shm.min_segsz */
	    atom->ul = shm_limits.shmmin;
	    break;
	case 2:	/* ipc.shm.max_seg */
	    atom->ul = shm_limits.shmmni;
	    break;
	case 3:	/* ipc.shm.max_segproc */
	    atom->ul = shm_limits.shmseg;
	    break;
	case 4:	/* ipc.shm.max_shmsys */
	    atom->ul = shm_limits.shmall;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_NUSERS:
	{
	    /* count the number of users */
	    struct utmp *ut;
	    atom->ul = 0;
	    setutent();
	    while ((ut = getutent())) {
		    if ((ut->ut_type == USER_PROCESS) && (ut->ut_name[0] != '\0'))
			    atom->ul++;
	    }
	    endutent();
	}
	break;


    case CLUSTER_IB: /* network.ib */
	sts = pmdaCacheLookup(indomtab[IB_INDOM].it_indom, inst, NULL, (void **)&ibportp);
	if (sts < 0) return sts;

	/* network.ib.{in,out}.bytes: convert to bytes */
	if (idp->item == 0 || idp->item == IB_COUNTERS_IN) {
	    atom->ull = ibportp->counters[idp->item] << 2;
	} else if (idp->item < IB_COUNTERS) {
	    /* other non-synthetic in/out counter */
	    atom->ull = ibportp->counters[idp->item];
	} else if (idp->item == IB_COUNTERS) {
	    /* network.ib.total.bytes (synthetic) */
	    atom->ull = (ibportp->counters[0] + ibportp->counters[IB_COUNTERS_IN]) << 2;
	} else if (idp->item < IB_COUNTERS_ALL) { /* other total counter */
	    pmID base = idp->item - IB_COUNTERS;
	    atom->ull = ibportp->counters[base] + ibportp->counters[IB_COUNTERS_IN + base];
	} else if (idp->item == IB_COUNTERS_ALL) { /* status */
	    sts = status_ib(ibportp);
	    if (sts != 0) return sts;
	    atom->cp = ibportp->status;
	} else { /* idp->item >  IB_COUNTERS_ALL */
            return PM_ERR_PMID;
	}
	break;
    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}


static int
linux_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i;
    int		need_refresh[NUM_CLUSTERS];

    memset(need_refresh, 0, sizeof(need_refresh));
    for (i=0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster >= 0 && idp->cluster < NUM_CLUSTERS) {
	    need_refresh[idp->cluster]++;
	    if (idp->cluster == CLUSTER_STAT && 
		need_refresh[CLUSTER_PARTITIONS] == 0 &&
	    	is_partitions_metric(pmidlist[i])) {
		need_refresh[CLUSTER_PARTITIONS]++;
	    }
	}

	/* In 2.6 kernels, swap.{pagesin,pagesout,in,out} are in /proc/vmstat */
	if (_pm_have_proc_vmstat && idp->cluster == CLUSTER_STAT) {
	    if (idp->item >= 8 && idp->item <= 11)
	    	need_refresh[CLUSTER_VMSTAT]++;
	}
    }

    linux_refresh(need_refresh);
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
procfs_zero(const char *filename, pmValueSet *vsp)
{
    FILE	*fp;
    int		value;
    int		sts = 0;

    value = vsp->vlist[0].value.lval;
    if (value < 0)
	return PM_ERR_SIGN;

    fp = fopen(filename, "w");
    if (!fp) {
	sts = -errno;
    } else {
	fprintf(fp, "%d\n", value);
	fclose(fp);
    }
    return sts;
}

static int
linux_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		sts = 0;
    pmValueSet	*vsp;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid && !sts; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster == CLUSTER_XFS && pmidp->item == 79)
	    sts = procfs_zero("/proc/sys/fs/xfs/stats_clear", vsp);
	else
	    sts = -EACCES;
    }
    return sts;
}


/*
 * Initialise the agent (both daemon and DSO).
 */

void 
linux_init(pmdaInterface *dp)
{
    int		need_refresh[NUM_CLUSTERS];
    int		i, major, minor;
    __pmID_int	*idp;

    _pm_system_pagesize = getpagesize();
    if (_isDSO) {
	char helppath[MAXPATHLEN];
	snprintf(helppath, sizeof(helppath), "%s/pmdas/linux/help", pmGetConfig("PCP_VAR_DIR"));
    	pmdaDSO(dp, PMDA_INTERFACE_3, "linux DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->version.two.instance = linux_instance;
    dp->version.two.store = linux_store;
    dp->version.two.fetch = linux_fetch;
    pmdaSetFetchCallBack(dp, linux_fetchCallBack);

    filesys.indom = &indomtab[FILESYS_INDOM];
    swapdev.indom = &indomtab[SWAPDEV_INDOM];
    proc_interrupts.indom = &indomtab[PROC_INTERRUPTS_INDOM];
    proc_pid.indom = &indomtab[PROC_INDOM];
    proc_stat.cpu_indom = proc_cpuinfo.cpuindom = &indomtab[CPU_INDOM];
    proc_scsi.scsi_indom = &indomtab[SCSI_INDOM];
    proc_slabinfo.indom = &indomtab[SLAB_INDOM];

    /*
     * Figure out kernel version.  The precision of certain metrics
     * (e.g. percpu time counters) has changed over kernel versions.
     * See include/linux/kernel_stat.h for all the various flavours.
     */
    uname(&kernel_uname);
    _pm_ctxt_size = 8;
    _pm_intr_size = 8;
    _pm_cputime_size = 8;
    _pm_idletime_size = 8;
    if (sscanf(kernel_uname.release, "%d.%d", &major, &minor) == 2) {
	if (major < 2 || (major == 2 && minor <= 4)) {	/* 2.4 and earlier */
	     fprintf(stderr, "NOTICE: using kernel 2.4 or earlier CPU types\n");
	    _pm_ctxt_size = 4;
	    _pm_intr_size = 4;
	    _pm_cputime_size = 4;
	    _pm_idletime_size = sizeof(unsigned long);
	}
	else if (major == 2 && minor >= 0 && minor <= 4) {  /* 2.6.0->.4 */
	     fprintf(stderr, "NOTICE: using kernel 2.6.0 to 2.6.4 CPU types\n");
	    _pm_cputime_size = 4;
	    _pm_idletime_size = 4;
	}
	else
	    fprintf(stderr, "NOTICE: using 64 bit CPU time types\n");
    }
    for (i = 0; i < sizeof(metrictab)/sizeof(metrictab[0]); i++) {
	idp = (__pmID_int *)&(metrictab[i].m_desc.pmid);
	if (idp->cluster == CLUSTER_STAT) {
	    switch (idp->item) {
	    case 0:	/* kernel.percpu.cpu.user */
	    case 1:	/* kernel.percpu.cpu.nice */
	    case 2:	/* kernel.percpu.cpu.sys */
	    case 20:	/* kernel.all.cpu.user */
	    case 21:	/* kernel.all.cpu.nice */
	    case 22:	/* kernel.all.cpu.sys */
	    case 30:	/* kernel.percpu.cpu.wait.total */
	    case 31:	/* kernel.percpu.cpu.intr */
	    case 34:	/* kernel.all.cpu.intr */
	    case 35:	/* kernel.all.cpu.wait.total */
		_pm_metric_type(metrictab[i].m_desc.type, _pm_cputime_size);
		break;
	    case 3:	/* kernel.percpu.cpu.idle */
	    case 23:	/* kernel.all.cpu.idle */
		_pm_metric_type(metrictab[i].m_desc.type, _pm_idletime_size);
		break;
	    case 12:	/* kernel.all.intr */
		_pm_metric_type(metrictab[i].m_desc.type, _pm_intr_size);
		break;
	    case 13:	/* kernel.all.pswitch */
		_pm_metric_type(metrictab[i].m_desc.type, _pm_ctxt_size);
		break;
	    }
	}
	if (metrictab[i].m_desc.type == PM_TYPE_NOSUPPORT)
	    fprintf(stderr, "Bad kernel metric descriptor type (%u.%u)\n",
			    idp->cluster, idp->item);
    }

    /* 
     * Read System.map and /proc/ksyms. Used to translate wait channel
     * addresses to symbol names. 
     * Added by Mike Mason <mmlnx@us.ibm.com>
     */
    read_ksym_sources();

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
             sizeof(metrictab)/sizeof(metrictab[0]));

    /* initially refresh all clusters */
    memset(need_refresh, 1, sizeof(need_refresh));
    linux_refresh(need_refresh);
}


static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain  use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile write log into logfile rather than using default log name\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */

int
main(int argc, char **argv)
{
    int			err = 0;
    int			c = 0;
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];
    char		*p;

    /* trim cmd name of leading directory components */
    pmProgname = argv[0];
    for (p = pmProgname; *p; p++) {
	if (*p == '/')
	    pmProgname = p+1;
    }

    _isDSO = 0;

    snprintf(helppath, sizeof(helppath), "%s/pmdas/linux/help", pmGetConfig("PCP_VAR_DIR"));
    pmdaDaemon(&dispatch, PMDA_INTERFACE_3, pmProgname, LINUX, "linux.log", helppath);

    if ((c = pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err)) != EOF)
    	err++;

    if (err)
    	usage();

    pmdaOpenLog(&dispatch);
    linux_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
    /*NOTREACHED*/
}
