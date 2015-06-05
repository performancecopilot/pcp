/*
 * Linux PMDA
 *
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 2007-2011 Aconex.  All Rights Reserved.
 * Copyright (c) 2002 International Business Machines Corp.
 * Copyright (c) 2000,2004,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "impl.h"
#include "pmda.h"
#undef LINUX /* defined in NSS/NSPR headers as something different, which we do not need. */
#include "domain.h"

#include <ctype.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <utmp.h>
#include <pwd.h>
#include <grp.h>

#include "convert.h"
#include "clusters.h"
#include "indom.h"

#include "proc_cpuinfo.h"
#include "proc_stat.h"
#include "proc_meminfo.h"
#include "proc_loadavg.h"
#include "proc_net_dev.h"
#include "filesys.h"
#include "swapdev.h"
#include "getinfo.h"
#include "proc_net_rpc.h"
#include "proc_net_sockstat.h"
#include "proc_net_tcp.h"
#include "proc_partitions.h"
#include "proc_net_netstat.h"
#include "proc_net_snmp.h"
#include "proc_scsi.h"
#include "proc_slabinfo.h"
#include "proc_uptime.h"
#include "proc_sys_fs.h"
#include "proc_vmstat.h"
#include "sysfs_kernel.h"
#include "linux_table.h"
#include "numa_meminfo.h"
#include "namespaces.h"
#include "interrupts.h"
#include "ipc.h"

static proc_stat_t		proc_stat;
static proc_meminfo_t		proc_meminfo;
static proc_loadavg_t		proc_loadavg;
static proc_net_rpc_t		proc_net_rpc;
static proc_net_tcp_t		proc_net_tcp;
static proc_net_sockstat_t	proc_net_sockstat;
static struct utsname		kernel_uname;
static char 			uname_string[sizeof(kernel_uname)];
static proc_cpuinfo_t		proc_cpuinfo;
static proc_slabinfo_t		proc_slabinfo;
static sem_limits_t		sem_limits;
static msg_limits_t		msg_limits;
static shm_limits_t		shm_limits;
static proc_uptime_t		proc_uptime;
static proc_sys_fs_t		proc_sys_fs;
static sysfs_kernel_t		sysfs_kernel;
static numa_meminfo_t		numa_meminfo;

static int		_isDSO = 1;	/* =0 I am a daemon */
static int		rootfd = -1;	/* af_unix pmdaroot */
static char		*username;

/* globals */
size_t _pm_system_pagesize; /* for hinv.pagesize and used elsewhere */
int _pm_have_proc_vmstat; /* if /proc/vmstat is available */
int _pm_intr_size; /* size in bytes of interrupt sum count metric */
int _pm_ctxt_size; /* size in bytes of context switch count metric */
int _pm_cputime_size; /* size in bytes of most of the cputime metrics */
int _pm_idletime_size; /* size in bytes of the idle cputime metric */
proc_vmstat_t _pm_proc_vmstat;
proc_net_snmp_t _pm_proc_net_snmp;
pmdaInstid _pm_proc_net_snmp_indom_id[NR_ICMPMSG_COUNTERS];
proc_net_netstat_t _pm_proc_net_netstat;

/*
 * Metric Instance Domains (statically initialized ones only)
 */
static pmdaInstid loadavg_indom_id[] = {
    { 1, "1 minute" }, { 5, "5 minute" }, { 15, "15 minute" }
};

static pmdaInstid nfs_indom_id[NR_RPC_COUNTERS] = {
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

static pmdaInstid nfs3_indom_id[NR_RPC3_COUNTERS] = {
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

static pmdaInstid nfs4_cli_indom_id[NR_RPC4_CLI_COUNTERS] = {
	{ 0,  "null" },
	{ 1,  "read" },
	{ 2,  "write" },
	{ 3,  "commit" },
	{ 4,  "open" },
	{ 5,  "open_conf" },
	{ 6,  "open_noat" },
	{ 7,  "open_dgrd" },
	{ 8,  "close" },
	{ 9,  "setattr" },
	{ 10, "fsinfo" },
	{ 11, "renew" },
	{ 12, "setclntid" },
	{ 13, "confirm" },
	{ 14, "lock" },
	{ 15, "lockt" },
	{ 16, "locku" },
	{ 17, "access" },
	{ 18, "getattr" },
	{ 19, "lookup" },
	{ 20, "lookup_root" },
	{ 21, "remove" },
	{ 22, "rename" },
	{ 23, "link" },
	{ 24, "symlink" },
	{ 25, "create" },
	{ 26, "pathconf" },
	{ 27, "statfs" },
	{ 28, "readlink" },
	{ 29, "readdir" },
	{ 30, "server_caps" },
	{ 31, "delegreturn" },
	{ 32, "getacl" },
	{ 33, "setacl" },
	{ 34, "fs_locations" },
	{ 35, "rel_lkowner" },
	{ 36, "secinfo" },
	{ 37, "fsid_present" },
	/* nfsv4.1 client ops */
	{ 38, "exchange_id" },
	{ 39, "create_ses" },
	{ 40, "destroy_ses" },
	{ 41, "sequence" },
	{ 42, "get_lease_t" },
	{ 43, "reclaim_comp" },
	{ 44, "layoutget" },
	{ 45, "getdevinfo" },
	{ 46, "layoutcommit" },
	{ 47, "secinfononam" },
	{ 48, "test_stateid" },
	{ 49, "free_stateid" },
	{ 50, "getdevlist" },
	{ 51, "bind_ses" },
	{ 52, "destroy_clntid" },
};

static pmdaInstid nfs4_svr_indom_id[NR_RPC4_SVR_COUNTERS] = {
	/*
	 * { 0,  "null" } - the kernel actually only exports from "access"
	 * The first three values in the net/rpc/nfsd file are always zero
	 * In particular, see the nfs_opnum4 enum in <linux/nfs4.h>, these
	 * values are used as array indices in the kernel.
	 */
	{ 1,  "op0-unused" },
	{ 2,  "op1-unused"},
	{ 3,  "op2-future"}, /* name matching the nfsstat convention */
	{ 4,  "access" },
	{ 5,  "close" },
	{ 6,  "commit" },
	{ 7,  "create" },
	{ 8,  "delegpurge" },
	{ 9,  "delegreturn" },
	{ 10, "getattr" },
	{ 11, "getfh" },
	{ 12, "link" },
	{ 13, "lock" },
	{ 14, "lockt" },
	{ 15, "locku" },
	{ 16, "lookup" },
	{ 17, "lookup_root" },
	{ 18, "nverify" },
	{ 19, "open" },
	{ 20, "openattr" },
	{ 21, "open_conf" },
	{ 22, "open_dgrd" },
	{ 23, "putfh" },
	{ 24, "putpubfh" },
	{ 25, "putrootfh" },
	{ 26, "read" },
	{ 27, "readdir" },
	{ 28, "readlink" },
	{ 29, "remove" },
	{ 30, "rename" },
	{ 31, "renew" },
	{ 32, "restorefh" },
	{ 33, "savefh" },
	{ 34, "secinfo" },
	{ 35, "setattr" },
	{ 36, "setcltid" },
	{ 37, "setcltidconf" },
	{ 38, "verify" },
	{ 39, "write" },
	{ 40, "rellockowner" },
	/* nfsv4.1 server ops */
	{ 41, "bc_ctl" },
	{ 42, "bind_conn" },
	{ 43, "exchange_id" },
	{ 44, "create_ses" },
	{ 45, "destroy_ses" },
	{ 46, "free_stateid" },
	{ 47, "getdirdeleg" },
	{ 48, "getdevinfo" },
	{ 49, "getdevlist" },
	{ 50, "layoutcommit" },
	{ 51, "layoutget" },
	{ 52, "layoutreturn" },
	{ 53, "secinfononam" },
	{ 54, "sequence" },
	{ 55, "set_ssv" },
	{ 56, "test_stateid" },
	{ 57, "want_deleg" },
	{ 58, "destroy_clid" },
	{ 59, "reclaim_comp" },
};

static pmdaIndom indomtab[] = {
    { CPU_INDOM, 0, NULL },
    { DISK_INDOM, 0, NULL }, /* cached */
    { LOADAVG_INDOM, 3, loadavg_indom_id },
    { NET_DEV_INDOM, 0, NULL },
    { PROC_INTERRUPTS_INDOM, 0, NULL },	/* deprecated */
    { FILESYS_INDOM, 0, NULL },
    { SWAPDEV_INDOM, 0, NULL },
    { NFS_INDOM, NR_RPC_COUNTERS, nfs_indom_id },
    { NFS3_INDOM, NR_RPC3_COUNTERS, nfs3_indom_id },
    { PROC_PROC_INDOM, 0, NULL },	/* migrated to the proc PMDA */
    { PARTITIONS_INDOM, 0, NULL }, /* cached */
    { SCSI_INDOM, 0, NULL },
    { SLAB_INDOM, 0, NULL },
    { STRINGS_INDOM, 0, NULL },
    { NFS4_CLI_INDOM, NR_RPC4_CLI_COUNTERS, nfs4_cli_indom_id },
    { NFS4_SVR_INDOM, NR_RPC4_SVR_COUNTERS, nfs4_svr_indom_id },
    { QUOTA_PRJ_INDOM, 0, NULL },	/* migrated to the xfs PMDA */
    { NET_ADDR_INDOM, 0, NULL },
    { TMPFS_INDOM, 0, NULL },
    { NODE_INDOM, 0, NULL },
    { PROC_CGROUP_SUBSYS_INDOM, 0, NULL },
    { PROC_CGROUP_MOUNTS_INDOM, 0, NULL },
    { 0 }, /* deprecated LV_INDOM */
    { ICMPMSG_INDOM, NR_ICMPMSG_COUNTERS, _pm_proc_net_snmp_indom_id },
    { DM_INDOM, 0, NULL }, /* cached */
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

/* kernel.percpu.cpu.wait.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,30), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,31), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.irq.soft */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,56), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.irq.hard */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,57), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.steal */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,58), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.guest */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,61), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.vuser */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,76), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },


/* kernel.pernode.cpu.user */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,62), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.nice */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,63), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.sys */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,64), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.idle */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,65), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.wait.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,69), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,66), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.irq.soft */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,70), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.irq.hard */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,71), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.steal */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,67), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.guest */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,68), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.vuser */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,77), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,4), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,5), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,6), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,7), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
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
      { PMDA_PMID(CLUSTER_STAT,49), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.write_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,50), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.scheduler */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,59), PM_TYPE_STRING, DISK_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* disk.dev.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,72), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.write_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,73), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

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

/* disk.all.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

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

/* kernel.all.cpu.intr */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,34), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.wait.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,35), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.irq.soft */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,53), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.irq.hard */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,54), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.steal */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,55), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.guest */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,60), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.vuser */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,78), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
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

/* hinv.ncpu */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.ndisk */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.nnode */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

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
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },

/* kernel.all.idletime */
    { NULL,
      { PMDA_PMID(CLUSTER_UPTIME,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },

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

/* mem.util.anonpages */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.commitLimit */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.bounce */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.NFS_Unstable */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.slabReclaimable */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.slabUnreclaimable */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.active_anon */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.inactive_anon */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.active_file */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.inactive_file */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.unevictable */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.mlocked */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.shmem */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.kernelStack */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.hugepagesTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.util.hugepagesFree */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.util.hugepagesRsvd */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.util.hugepagesSurp */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.util.directMap4k */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.directMap2M */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.vmallocTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.vmallocUsed */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.vmallocChunk */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.mmap_copy */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.quicklists */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.corrupthardware */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.mmap_copy */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.directMap1G */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.util.available */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* hinv.hugepagesize */
    { NULL, 
      { PMDA_PMID(CLUSTER_MEMINFO,59), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.numa.util.total */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,0), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.free */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,1), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.used */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,2), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.active */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,3), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.inactive */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,4), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.active_anon */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,5), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.inactive_anon */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,6), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.active_file */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,7), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.inactive_file */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,8), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.highTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,9), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.highFree */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,10), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.lowTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,11), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.lowFree */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,12), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.unevictable */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,13), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.mlocked */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,14), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.dirty */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,15), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.writeback */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,16), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.filePages */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,17), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.mapped */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,18), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.anonpages */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,19), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.shmem */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,20), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.kernelStack */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,21), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.pageTables */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,22), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.NFS_Unstable */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,23), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.bounce */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,24), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.writebackTmp */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,25), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.slab */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,26), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.slabReclaimable */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,27), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.slabUnreclaimable */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,28), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.numa.util.hugepagesTotal */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,29), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.util.hugepagesFree */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,30), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.util.hugepagesSurp */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,31), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.hit */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,32), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.miss */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,33), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.foreign */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,34), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.interleave_hit */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,35), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.local_node */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,36), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* mem.numa.alloc.other_node */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,37), PM_TYPE_U64, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },


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

    /* kernel.all.runnable */
    { NULL,
      { PMDA_PMID(CLUSTER_LOADAVG, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) } },

    /* kernel.all.nprocs */
    { NULL,
      { PMDA_PMID(CLUSTER_LOADAVG, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
      PMDA_PMUNITS(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0) }, },

/* network.interface.baudrate */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,23), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,-1,0,PM_SPACE_BYTE,PM_TIME_SEC,0) }, },

/* network.interface.duplex */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,24), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.up */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,25), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.running */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,26), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.ninterface */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,27), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.inet_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,0), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.ipv6_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,1), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.ipv6_scope */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,2), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.hw_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,3), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

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

/* filesys.blocksize */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,9), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* filesys.avail */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,10), PM_TYPE_U64, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* filesys.readonly */
  { NULL,
    { PMDA_PMID(CLUSTER_FILESYS,11), PM_TYPE_U32, FILESYS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * tmpfs filesystem cluster
 */

/* tmpfs.capacity */
  { NULL,
    { PMDA_PMID(CLUSTER_TMPFS,1), PM_TYPE_U64, TMPFS_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* tmpfs.used */
  { NULL,
    { PMDA_PMID(CLUSTER_TMPFS,2), PM_TYPE_U64, TMPFS_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* tmpfs.free */
  { NULL,
     { PMDA_PMID(CLUSTER_TMPFS,3), PM_TYPE_U64, TMPFS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

/* tmpfs.maxfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_TMPFS,4), PM_TYPE_U32, TMPFS_INDOM, PM_SEM_DISCRETE,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* tmpfs.usedfiles */
  { NULL,
     { PMDA_PMID(CLUSTER_TMPFS,5), PM_TYPE_U32, TMPFS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* tmpfs.freefiles */
  { NULL,
     { PMDA_PMID(CLUSTER_TMPFS,6), PM_TYPE_U32, TMPFS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

/* tmpfs.full */
  { NULL,
     { PMDA_PMID(CLUSTER_TMPFS,7), PM_TYPE_DOUBLE, TMPFS_INDOM, PM_SEM_INSTANT,
     PMDA_PMUNITS(0,0,0,0,0,0) } },

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

/* nfs4.client.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,64), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs4.client.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,65), PM_TYPE_U32, NFS4_CLI_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs4.server.calls */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs4.server.reqs */
  { NULL,
    { PMDA_PMID(CLUSTER_NET_NFS,67), PM_TYPE_U32, NFS4_SVR_INDOM, PM_SEM_COUNTER,
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

/* rpc.server.fh_anon */
  { &proc_net_rpc.server.fh_anon,
    { PMDA_PMID(CLUSTER_NET_NFS,51), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_nocache_dir */
  { &proc_net_rpc.server.fh_nocache_dir,
    { PMDA_PMID(CLUSTER_NET_NFS,52), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.fh_nocache_nondir */
  { &proc_net_rpc.server.fh_nocache_nondir,
    { PMDA_PMID(CLUSTER_NET_NFS,53), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.io_read */
  { &proc_net_rpc.server.io_read,
    { PMDA_PMID(CLUSTER_NET_NFS,54), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* rpc.server.io_write */
  { &proc_net_rpc.server.io_write,
    { PMDA_PMID(CLUSTER_NET_NFS,55), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) } },

/* rpc.server.th_cnt */
  { &proc_net_rpc.server.th_cnt,
    { PMDA_PMID(CLUSTER_NET_NFS,56), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.th_fullcnt */
  { &proc_net_rpc.server.th_fullcnt,
    { PMDA_PMID(CLUSTER_NET_NFS,57), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.ra_size */
  { &proc_net_rpc.server.ra_size,
    { PMDA_PMID(CLUSTER_NET_NFS,68), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.ra_hits */
  { &proc_net_rpc.server.ra_hits,
    { PMDA_PMID(CLUSTER_NET_NFS,69), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* rpc.server.ra_misses */
  { &proc_net_rpc.server.ra_misses,
    { PMDA_PMID(CLUSTER_NET_NFS,70), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * /proc/partitions cluster
 */

/* disk.partitions.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,0), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,1), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,2), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,3), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,4), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
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
 * network snmp cluster
 */

/* network.ip.forwarding */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FORWARDING], 
    { PMDA_PMID(CLUSTER_NET_SNMP,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.defaultttl */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_DEFAULTTTL], 
    { PMDA_PMID(CLUSTER_NET_SNMP,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inreceives */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INRECEIVES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inhdrerrors */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INHDRERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inaddrerrors */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INADDRERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.forwdatagrams */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FORWDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inunknownprotos */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INUNKNOWNPROTOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.indiscards */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INDISCARDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.indelivers */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_INDELIVERS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outrequests */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTREQUESTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outdiscards */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTDISCARDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outnoroutes */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_OUTNOROUTES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmtimeout */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMTIMEOUT], 
    { PMDA_PMID(CLUSTER_NET_SNMP,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmreqds */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMREQDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmoks */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMOKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.reasmfails */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_REASMFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragoks */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGOKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragfails */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.fragcreates */
  { &_pm_proc_net_snmp.ip[_PM_SNMP_IP_FRAGCREATES], 
    { PMDA_PMID(CLUSTER_NET_SNMP,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },


/* network.icmp.inmsgs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INMSGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inerrors */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.indestunreachs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INDESTUNREACHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimeexcds */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMEEXCDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inparmprobs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INPARMPROBS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.insrcquenchs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INSRCQUENCHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inredirects */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INREDIRECTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inechos */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inechoreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INECHOREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimestamps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.intimestampreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INTIMESTAMPREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inaddrmasks */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.inaddrmaskreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INADDRMASKREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outmsgs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTMSGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outerrors */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outdestunreachs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTDESTUNREACHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimeexcds */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMEEXCDS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outparmprobs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTPARMPROBS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outsrcquenchs */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTSRCQUENCHS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outredirects */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTREDIRECTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outechos */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outechoreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTECHOREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimestamps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outtimestampreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTTIMESTAMPREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outaddrmasks */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.outaddrmaskreps */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_OUTADDRMASKREPS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp.incsumerrors */
  { &_pm_proc_net_snmp.icmp[_PM_SNMP_ICMP_INCSUMERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },


/* network.tcp.rtoalgorithm */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOALGORITHM], 
    { PMDA_PMID(CLUSTER_NET_SNMP,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rtomin */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMIN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rtomax */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMAX], 
    { PMDA_PMID(CLUSTER_NET_SNMP,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.maxconn */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_MAXCONN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.established */
  { &proc_net_tcp.stat[_PM_TCP_ESTABLISHED],
    { PMDA_PMID(CLUSTER_NET_TCP, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.syn_sent */
  { &proc_net_tcp.stat[_PM_TCP_SYN_SENT],
    { PMDA_PMID(CLUSTER_NET_TCP, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.syn_recv */
  { &proc_net_tcp.stat[_PM_TCP_SYN_RECV],
    { PMDA_PMID(CLUSTER_NET_TCP, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.fin_wait1 */
  { &proc_net_tcp.stat[_PM_TCP_FIN_WAIT1],
    { PMDA_PMID(CLUSTER_NET_TCP, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.fin_wait2 */
  { &proc_net_tcp.stat[_PM_TCP_FIN_WAIT2],
    { PMDA_PMID(CLUSTER_NET_TCP, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.time_wait */
  { &proc_net_tcp.stat[_PM_TCP_TIME_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP, 6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.close */
  { &proc_net_tcp.stat[_PM_TCP_CLOSE],
    { PMDA_PMID(CLUSTER_NET_TCP, 7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.close_wait */
  { &proc_net_tcp.stat[_PM_TCP_CLOSE_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP, 8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.last_ack */
  { &proc_net_tcp.stat[_PM_TCP_LAST_ACK],
    { PMDA_PMID(CLUSTER_NET_TCP, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.listen */
  { &proc_net_tcp.stat[_PM_TCP_LISTEN],
    { PMDA_PMID(CLUSTER_NET_TCP, 10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn.closing */
  { &proc_net_tcp.stat[_PM_TCP_CLOSING],
    { PMDA_PMID(CLUSTER_NET_TCP, 11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.activeopens */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ACTIVEOPENS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.passiveopens */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_PASSIVEOPENS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.attemptfails */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ATTEMPTFAILS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.estabresets */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_ESTABRESETS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.currestab */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_CURRESTAB], 
    { PMDA_PMID(CLUSTER_NET_SNMP,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.insegs */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.outsegs */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_OUTSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.retranssegs */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RETRANSSEGS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.inerrs */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INERRS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.outrsts */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_OUTRSTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.incsumerrors */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_INCSUMERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.indatagrams */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.noports */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_NOPORTS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.inerrors */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INERRORS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.outdatagrams */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_OUTDATAGRAMS], 
    { PMDA_PMID(CLUSTER_NET_SNMP,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.recvbuferrors */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_RECVBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.sndbuferrors */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_SNDBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.incsumerrors */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_INCSUMERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.indatagrams */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INDATAGRAMS],
    { PMDA_PMID(CLUSTER_NET_SNMP,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.noports */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_NOPORTS],
    { PMDA_PMID(CLUSTER_NET_SNMP,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.inerrors */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.outdatagrams */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_OUTDATAGRAMS],
    { PMDA_PMID(CLUSTER_NET_SNMP,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.recvbuferrors */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_RECVBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.sndbuferrors */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_SNDBUFERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.incsumerrors */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_INCSUMERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmpmsg.intype */
  { &_pm_proc_net_snmp.icmpmsg[_PM_SNMP_ICMPMSG_INTYPE],
    { PMDA_PMID(CLUSTER_NET_SNMP,88), PM_TYPE_U64, ICMPMSG_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmpmsg.outtype */
  { &_pm_proc_net_snmp.icmpmsg[_PM_SNMP_ICMPMSG_OUTTYPE],
    { PMDA_PMID(CLUSTER_NET_SNMP,89), PM_TYPE_U64, ICMPMSG_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/*
 * network netstat cluster
 */

/* network.ip.innoroutes */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INNOROUTES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.intruncatedpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INTRUNCATEDPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inmcastpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INMCASTPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outmcastpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTMCASTPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inbcastpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INBCASTPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outbcastpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTBCASTPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inmcastoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INMCASTOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outmcastoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTMCASTOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.inbcastoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_INBCASTOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.outbcastoctets */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_OUTBCASTOCTETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.csumerrors */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_CSUMERRORS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.noectpkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_NOECTPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.ect1pkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_ECT1PKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.ect0pkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_ECT0PKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip.cepkts */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_CEPKTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.syncookiessent */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESSENT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.syncookiesrecv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESRECV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.syncookiesfailed */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SYNCOOKIESFAILED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.embryonicrsts */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_EMBRYONICRSTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.prunecalled */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PRUNECALLED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rcvpruned */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_RCVPRUNED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.ofopruned */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_OFOPRUNED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.outofwindowicmps */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_OUTOFWINDOWICMPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lockdroppedicmps */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LOCKDROPPEDICMPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.arpfilter */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_ARPFILTER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.timewaited */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.timewaitrecycled */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITRECYCLED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.timewaitkilled */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TIMEWAITKILLED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.pawspassiverejected */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSPASSIVEREJECTED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.pawsactiverejected */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSACTIVEREJECTED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.pawsestabrejected */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PAWSESTABREJECTED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.delayedacks */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.delayedacklocked */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKLOCKED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.delayedacklost */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_DELAYEDACKLOST],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.listenoverflows */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LISTENOVERFLOWS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.listendrops */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_LISTENDROPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.prequeued */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPREQUEUED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.directcopyfrombacklog */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDIRECTCOPYFROMBACKLOG],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.directcopyfromprequeue */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDIRECTCOPYFROMPREQUEUE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.prequeuedropped */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPREQUEUEDROPPED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.hphits*/
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPHITS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.hphitstouser */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPHITSTOUSER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.pureacks */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPUREACKS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.hpacks */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHPACKS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.renorecovery */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENORECOVERY],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackrecovery */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRECOVERY],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackreneging */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRENEGING],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fackreorder */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFACKREORDER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackreorder */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKREORDER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.renoreorder */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENOREORDER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tsreorder */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTSREORDER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fullundo */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFULLUNDO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.partialundo */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPPARTIALUNDO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackundo */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKUNDO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lossundo */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSUNDO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lostretransmit */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSTRETRANSMIT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.renofailures */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENOFAILURES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackfailures */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKFAILURES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lossfailures */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSFAILURES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastretrans */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTRETRANS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.forwardretrans */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFORWARDRETRANS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.slowstartretrans */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSLOWSTARTRETRANS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.timeouts */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEOUTS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lossprobes */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSPROBES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.lossproberecovery */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSSPROBERECOVERY],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.renorecoveryfail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRENORECOVERYFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackrecoveryfail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKRECOVERYFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.schedulerfailed */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSCHEDULERFAILED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rcvcollapsed */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVCOLLAPSED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackoldsent */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOLDSENT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackofosent */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOFOSENT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackrecv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKRECV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackoforecv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKOFORECV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortondata */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONDATA],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortonclose */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONCLOSE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortonmemory */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONMEMORY],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortontimeout */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONTIMEOUT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortonlinger */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTONLINGER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.abortfailed */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPABORTFAILED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.memorypressures */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMEMORYPRESSURES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackdiscard */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSACKDISCARD],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackignoredold */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDOLD],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.dsackignorednoundo */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDNOUNDO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.spuriousrtos */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSPURIOUSRTOS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.md5notfound */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5NOTFOUND],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.md5unexpected */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5UNEXPECTED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackshifted */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKSHIFTED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackmerged */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKMERGED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.sackshiftfallback */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_SACKSHIFTFALLBACK],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.backlogdrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPBACKLOGDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.minttldrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMINTTLDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.deferacceptdrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDEFERACCEPTDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.iprpfilter */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_IPRPFILTER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.timewaitoverflow */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEWAITOVERFLOW],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.reqqfulldocookies */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPREQQFULLDOCOOKIES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.reqqfulldrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPREQQFULLDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.retransfail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRETRANSFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.rcvcoalesce */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVCOALESCE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.ofoqueue */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFOQUEUE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.ofodrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFODROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.ofomerge */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPOFOMERGE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.challengeack */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPCHALLENGEACK],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,103), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.synchallenge */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSYNCHALLENGE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,104), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopenactive */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENACTIVE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,105), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopenactivefail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENACTIVEFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,106), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopenpassive */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,107), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopenpassivefail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVEFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,108), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopenlistenoverflow */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENLISTENOVERFLOW],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,109), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fastopencookiereqd */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENCOOKIEREQD],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,110), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.spuriousrtxhostqueues */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSPURIOUS_RTX_HOSTQUEUES],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,111), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.busypollrxpackets */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_BUSYPOLLRXPACKETS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,112), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.autocorking */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPAUTOCORKING],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,113), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.fromzerowindowadv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFROMZEROWINDOWADV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,114), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tozerowindowadv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTOZEROWINDOWADV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,115), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.wantzerowindowadv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWANTZEROWINDOWADV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,116), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.synretrans */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPSYNRETRANS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,117), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.origdatasent */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPORIGDATASENT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,118), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* hinv.map.scsi */
    { NULL, 
      { PMDA_PMID(CLUSTER_SCSI,0), PM_TYPE_STRING, SCSI_INDOM, PM_SEM_DISCRETE, 
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

/* hinv.map.cpu_num */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 6), PM_TYPE_U32, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.machine */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.map.cpu_node */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 8), PM_TYPE_U32, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.model_name */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 9), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.flags */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 10), PM_TYPE_STRING, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.cpu.cache_alignment */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 11), PM_TYPE_U32, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,PM_SPACE_BYTE,0,0) } },

/*
 * sysfs device state cluster
 */

/* hinv.cpu.online */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 0), PM_TYPE_U32, CPU_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* hinv.node.online */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 1), PM_TYPE_U32, NODE_INDOM, PM_SEM_INSTANT,
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
    { &_pm_proc_vmstat.nr_dirty,
    {PMDA_PMID(28,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_writeback */
    { &_pm_proc_vmstat.nr_writeback,
    {PMDA_PMID(28,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_unstable */
    { &_pm_proc_vmstat.nr_unstable,
    {PMDA_PMID(28,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_page_table_pages */
    { &_pm_proc_vmstat.nr_page_table_pages,
    {PMDA_PMID(28,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_mapped */
    { &_pm_proc_vmstat.nr_mapped,
    {PMDA_PMID(28,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_slab */
    { &_pm_proc_vmstat.nr_slab,
    {PMDA_PMID(28,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.pgpgin */
    { &_pm_proc_vmstat.pgpgin,
    {PMDA_PMID(28,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgpgout */
    { &_pm_proc_vmstat.pgpgout,
    {PMDA_PMID(28,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpin */
    { &_pm_proc_vmstat.pswpin,
    {PMDA_PMID(28,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpout */
    { &_pm_proc_vmstat.pswpout,
    {PMDA_PMID(28,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_high */
    { &_pm_proc_vmstat.pgalloc_high,
    {PMDA_PMID(28,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_normal */
    { &_pm_proc_vmstat.pgalloc_normal,
    {PMDA_PMID(28,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_dma */
    { &_pm_proc_vmstat.pgalloc_dma,
    {PMDA_PMID(28,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfree */
    { &_pm_proc_vmstat.pgfree,
    {PMDA_PMID(28,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgactivate */
    { &_pm_proc_vmstat.pgactivate,
    {PMDA_PMID(28,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgdeactivate */
    { &_pm_proc_vmstat.pgdeactivate,
    {PMDA_PMID(28,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfault */
    { &_pm_proc_vmstat.pgfault,
    {PMDA_PMID(28,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgmajfault */
    { &_pm_proc_vmstat.pgmajfault,
    {PMDA_PMID(28,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_high */
    { &_pm_proc_vmstat.pgrefill_high,
    {PMDA_PMID(28,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_normal */
    { &_pm_proc_vmstat.pgrefill_normal,
    {PMDA_PMID(28,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_dma */
    { &_pm_proc_vmstat.pgrefill_dma,
    {PMDA_PMID(28,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_high */
    { &_pm_proc_vmstat.pgsteal_high,
    {PMDA_PMID(28,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_normal */
    { &_pm_proc_vmstat.pgsteal_normal,
    {PMDA_PMID(28,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_dma */
    { &_pm_proc_vmstat.pgsteal_dma,
    {PMDA_PMID(28,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_high */
    { &_pm_proc_vmstat.pgscan_kswapd_high,
    {PMDA_PMID(28,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_normal */
    { &_pm_proc_vmstat.pgscan_kswapd_normal,
    {PMDA_PMID(28,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_dma */
    { &_pm_proc_vmstat.pgscan_kswapd_dma,
    {PMDA_PMID(28,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_high */
    { &_pm_proc_vmstat.pgscan_direct_high,
    {PMDA_PMID(28,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_normal */
    { &_pm_proc_vmstat.pgscan_direct_normal,
    {PMDA_PMID(28,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_dma */
    { &_pm_proc_vmstat.pgscan_direct_dma,
    {PMDA_PMID(28,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pginodesteal */
    { &_pm_proc_vmstat.pginodesteal,
    {PMDA_PMID(28,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.slabs_scanned */
    { &_pm_proc_vmstat.slabs_scanned,
    {PMDA_PMID(28,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_steal */
    { &_pm_proc_vmstat.kswapd_steal,
    {PMDA_PMID(28,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_inodesteal */
    { &_pm_proc_vmstat.kswapd_inodesteal,
    {PMDA_PMID(28,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pageoutrun */
    { &_pm_proc_vmstat.pageoutrun,
    {PMDA_PMID(28,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall */
    { &_pm_proc_vmstat.allocstall,
    {PMDA_PMID(28,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrotated */
    { &_pm_proc_vmstat.pgrotated,
    {PMDA_PMID(28,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_slab_reclaimable */
    { &_pm_proc_vmstat.nr_slab_reclaimable,
    {PMDA_PMID(28,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_slab_unreclaimable */
    { &_pm_proc_vmstat.nr_slab_unreclaimable,
    {PMDA_PMID(28,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_anon_pages */
    { &_pm_proc_vmstat.nr_anon_pages,
    {PMDA_PMID(28,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_bounce */
    { &_pm_proc_vmstat.nr_bounce,
    {PMDA_PMID(28,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_file_pages */
    { &_pm_proc_vmstat.nr_file_pages,
    {PMDA_PMID(28,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_vmscan_write */
    { &_pm_proc_vmstat.nr_vmscan_write,
    {PMDA_PMID(28,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.htlb_buddy_alloc_fail */
    { &_pm_proc_vmstat.htlb_buddy_alloc_fail,
    {PMDA_PMID(28,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.htlb_buddy_alloc_success */
    { &_pm_proc_vmstat.htlb_buddy_alloc_success,
    {PMDA_PMID(28,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_active_anon */
    { &_pm_proc_vmstat.nr_active_anon,
    {PMDA_PMID(28,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_active_file */
    { &_pm_proc_vmstat.nr_active_file,
    {PMDA_PMID(28,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_free_pages */
    { &_pm_proc_vmstat.nr_free_pages,
    {PMDA_PMID(28,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_inactive_anon */
    { &_pm_proc_vmstat.nr_inactive_anon,
    {PMDA_PMID(28,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_inactive_file */
    { &_pm_proc_vmstat.nr_inactive_file,
    {PMDA_PMID(28,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_isolated_anon */
    { &_pm_proc_vmstat.nr_isolated_anon,
    {PMDA_PMID(28,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_isolated_file */
    { &_pm_proc_vmstat.nr_isolated_file,
    {PMDA_PMID(28,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_kernel_stack */
    { &_pm_proc_vmstat.nr_kernel_stack,
    {PMDA_PMID(28,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_mlock */
    { &_pm_proc_vmstat.nr_mlock,
    {PMDA_PMID(28,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_shmem */
    { &_pm_proc_vmstat.nr_shmem,
    {PMDA_PMID(28,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_unevictable */
    { &_pm_proc_vmstat.nr_unevictable,
    {PMDA_PMID(28,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_writeback_temp */
    { &_pm_proc_vmstat.nr_writeback_temp,
    {PMDA_PMID(28,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_blocks_moved */
    { &_pm_proc_vmstat.compact_blocks_moved,
    {PMDA_PMID(28,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_fail */
    { &_pm_proc_vmstat.compact_fail,
    {PMDA_PMID(28,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_pagemigrate_failed */
    { &_pm_proc_vmstat.compact_pagemigrate_failed,
    {PMDA_PMID(28,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_pages_moved */
    { &_pm_proc_vmstat.compact_pages_moved,
    {PMDA_PMID(28,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_stall */
    { &_pm_proc_vmstat.compact_stall,
    {PMDA_PMID(28,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_success */
    { &_pm_proc_vmstat.compact_success,
    {PMDA_PMID(28,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_dma32 */
    { &_pm_proc_vmstat.pgalloc_dma32,
    {PMDA_PMID(28,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_movable */
    { &_pm_proc_vmstat.pgalloc_movable,
    {PMDA_PMID(28,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_dma32 */
    { &_pm_proc_vmstat.pgrefill_dma32,
    {PMDA_PMID(28,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_movable */
    { &_pm_proc_vmstat.pgrefill_movable,
    {PMDA_PMID(28,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_dma32 */
    { &_pm_proc_vmstat.pgscan_direct_dma32,
    {PMDA_PMID(28,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_movable */
    { &_pm_proc_vmstat.pgscan_direct_movable,
    {PMDA_PMID(28,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_dma32 */
    { &_pm_proc_vmstat.pgscan_kswapd_dma32,
    {PMDA_PMID(28,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_movable */
    { &_pm_proc_vmstat.pgscan_kswapd_movable,
    {PMDA_PMID(28,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_dma32 */
    { &_pm_proc_vmstat.pgsteal_dma32,
    {PMDA_PMID(28,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_movable */
    { &_pm_proc_vmstat.pgsteal_movable,
    {PMDA_PMID(28,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_fault_alloc */
    { &_pm_proc_vmstat.thp_fault_alloc,
    {PMDA_PMID(28,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_fault_fallback */
    { &_pm_proc_vmstat.thp_fault_fallback,
    {PMDA_PMID(28,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_collapse_alloc */
    { &_pm_proc_vmstat.thp_collapse_alloc,
    {PMDA_PMID(28,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_collapse_alloc_failed */
    { &_pm_proc_vmstat.thp_collapse_alloc_failed,
    {PMDA_PMID(28,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split */
    { &_pm_proc_vmstat.thp_split,
    {PMDA_PMID(28,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_cleared */
    { &_pm_proc_vmstat.unevictable_pgs_cleared,
    {PMDA_PMID(28,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_culled */
    { &_pm_proc_vmstat.unevictable_pgs_culled,
    {PMDA_PMID(28,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_mlocked */
    { &_pm_proc_vmstat.unevictable_pgs_mlocked,
    {PMDA_PMID(28,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_mlockfreed */
    { &_pm_proc_vmstat.unevictable_pgs_mlockfreed,
    {PMDA_PMID(28,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_munlocked */
    { &_pm_proc_vmstat.unevictable_pgs_munlocked,
    {PMDA_PMID(28,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_rescued */
    { &_pm_proc_vmstat.unevictable_pgs_rescued,
    {PMDA_PMID(28,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_scanned */
    { &_pm_proc_vmstat.unevictable_pgs_scanned,
    {PMDA_PMID(28,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_stranded */
    { &_pm_proc_vmstat.unevictable_pgs_stranded,
    {PMDA_PMID(28,85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.zone_reclaim_failed */
    { &_pm_proc_vmstat.zone_reclaim_failed,
    {PMDA_PMID(28,86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_low_wmark_hit_quickly */
    { &_pm_proc_vmstat.kswapd_low_wmark_hit_quickly,
    {PMDA_PMID(28,87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_high_wmark_hit_quickly */
    { &_pm_proc_vmstat.kswapd_high_wmark_hit_quickly,
    {PMDA_PMID(28,88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_skip_congestion_wait */
    { &_pm_proc_vmstat.kswapd_skip_congestion_wait,
    {PMDA_PMID(28,89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_anon_transparent_hugepages */
    { &_pm_proc_vmstat.nr_anon_transparent_hugepages,
    {PMDA_PMID(28,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirtied */
    { &_pm_proc_vmstat.nr_dirtied,
    {PMDA_PMID(28,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirty_background_threshold */
    { &_pm_proc_vmstat.nr_dirty_background_threshold,
    {PMDA_PMID(28,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirty_threshold */
    { &_pm_proc_vmstat.nr_dirty_threshold,
    {PMDA_PMID(28,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_written */
    { &_pm_proc_vmstat.nr_written,
    {PMDA_PMID(28,94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_foreign */
    { &_pm_proc_vmstat.numa_foreign,
    {PMDA_PMID(28,95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_hit */
    { &_pm_proc_vmstat.numa_hit,
    {PMDA_PMID(28,96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_interleave */
    { &_pm_proc_vmstat.numa_interleave,
    {PMDA_PMID(28,97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_local */
    { &_pm_proc_vmstat.numa_local,
    {PMDA_PMID(28,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_miss */
    { &_pm_proc_vmstat.numa_miss,
    {PMDA_PMID(28,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_other */
    { &_pm_proc_vmstat.numa_other,
    {PMDA_PMID(28,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * sysfs_kernel cluster
 */
    /* sysfs.kernel.uevent_seqnum */
    { &sysfs_kernel.uevent_seqnum,
      { PMDA_PMID(CLUSTER_SYSFS_KERNEL,0), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * /proc/interrupts cluster
 */
    /* kernel.all.interrupts.errors */
    { &irq_err_count,
      { PMDA_PMID(CLUSTER_INTERRUPTS, 3), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.interrupts.line[<N>] */
    { NULL, { PMDA_PMID(CLUSTER_INTERRUPT_LINES, 0), PM_TYPE_U32,
    CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.interrupts.[<other>] */
    { NULL, { PMDA_PMID(CLUSTER_INTERRUPT_OTHER, 0), PM_TYPE_U32,
    CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * disk.dm cluster
 */
    /* disk.dm.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,0), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,1), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,2), PM_TYPE_U64, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,3), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,4), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,5), PM_TYPE_U64, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,6), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,7), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,8), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.read_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,9), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.write_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,10), KERNEL_ULONG, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.avactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,11), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.aveq */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,12), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* hinv.map.dmname */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,13), PM_TYPE_STRING, DM_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* disk.dm.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,14), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.write_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_DM,15), PM_TYPE_U32, DM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },
};

typedef struct {
    linux_container_t	container;
} perctx_t;

static perctx_t *ctxtab;
static int      num_ctx;

char *linux_statspath = "";	/* optional path prefix for all stats files */

FILE *
linux_statsfile(const char *path, char *buffer, int size)
{
    snprintf(buffer, size, "%s%s", linux_statspath, path);
    buffer[size-1] = '\0';
    return fopen(buffer, "r");
}

static void
linux_refresh(pmdaExt *pmda, int *need_refresh, linux_container_t *cp)
{
    int need_refresh_mtab = 0;

    if (need_refresh[CLUSTER_PARTITIONS])
    	refresh_proc_partitions(INDOM(DISK_INDOM),
				INDOM(PARTITIONS_INDOM),
				INDOM(DM_INDOM));

    if (need_refresh[CLUSTER_STAT])
	refresh_proc_stat(&proc_cpuinfo, &proc_stat);

    if (need_refresh[CLUSTER_CPUINFO])
	refresh_proc_cpuinfo(&proc_cpuinfo);

    if (need_refresh[CLUSTER_MEMINFO])
	refresh_proc_meminfo(&proc_meminfo);

    if (need_refresh[CLUSTER_NUMA_MEMINFO])
	refresh_numa_meminfo(&numa_meminfo, &proc_cpuinfo, &proc_stat);

    if (need_refresh[CLUSTER_LOADAVG])
	refresh_proc_loadavg(&proc_loadavg);

    if (need_refresh[CLUSTER_NET_DEV])
	refresh_proc_net_dev(INDOM(NET_DEV_INDOM), cp);

    if (need_refresh[CLUSTER_NET_ADDR])
	refresh_net_dev_addr(INDOM(NET_ADDR_INDOM), cp);

    if (need_refresh[CLUSTER_FILESYS] || need_refresh[CLUSTER_TMPFS])
	refresh_filesys(INDOM(FILESYS_INDOM), INDOM(TMPFS_INDOM), cp);

    if (need_refresh[CLUSTER_INTERRUPTS] ||
	need_refresh[CLUSTER_INTERRUPT_LINES] ||
	need_refresh[CLUSTER_INTERRUPT_OTHER])
	need_refresh_mtab |= refresh_interrupt_values();

    if (need_refresh[CLUSTER_SWAPDEV])
	refresh_swapdev(INDOM(SWAPDEV_INDOM));

    if (need_refresh[CLUSTER_NET_NFS])
	refresh_proc_net_rpc(&proc_net_rpc);

    if (need_refresh[CLUSTER_NET_SOCKSTAT])
	refresh_proc_net_sockstat(&proc_net_sockstat);

    if (need_refresh[CLUSTER_KERNEL_UNAME])
	uname(&kernel_uname);

    if (need_refresh[CLUSTER_NET_SNMP])
	refresh_proc_net_snmp(&_pm_proc_net_snmp);

    if (need_refresh[CLUSTER_SCSI])
	refresh_proc_scsi(INDOM(SCSI_INDOM));

    if (need_refresh[CLUSTER_NET_TCP])
	refresh_proc_net_tcp(&proc_net_tcp);

    if (need_refresh[CLUSTER_NET_NETSTAT])
	refresh_proc_net_netstat(&_pm_proc_net_netstat);

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
    	refresh_proc_vmstat(&_pm_proc_vmstat);

    if (need_refresh[CLUSTER_SYSFS_KERNEL])
    	refresh_sysfs_kernel(&sysfs_kernel);

    if (need_refresh_mtab)
	pmdaDynamicMetricTable(pmda);
}

static linux_container_t *
linux_ctx_container(int ctx)
{
    if (ctx < num_ctx && ctx >= 0 && ctxtab[ctx].container.name)
	return &ctxtab[ctx].container;
    return NULL;
}

static int
linux_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    linux_container_t	*container = NULL;
    __pmInDom_int	*indomp = (__pmInDom_int *)&indom;
    int			need_refresh[NUM_CLUSTERS] = {0};
    int			sts, ns_flags = 0;

    switch (indomp->serial) {
    case DISK_INDOM:
    case PARTITIONS_INDOM:
    case DM_INDOM:
	need_refresh[CLUSTER_PARTITIONS]++;
	break;
    case CPU_INDOM:
	need_refresh[CLUSTER_STAT]++;
	break;
    case NODE_INDOM:
	need_refresh[CLUSTER_NUMA_MEMINFO]++;
	break;
    case LOADAVG_INDOM:
	need_refresh[CLUSTER_LOADAVG]++;
	break;
    case NET_DEV_INDOM:
	need_refresh[CLUSTER_NET_DEV]++;
	ns_flags |= LINUX_NAMESPACE_NET;
	break;
    case NET_ADDR_INDOM:
	need_refresh[CLUSTER_NET_ADDR]++;
	ns_flags |= LINUX_NAMESPACE_NET;
	break;
    case FILESYS_INDOM:
	need_refresh[CLUSTER_FILESYS]++;
	ns_flags |= LINUX_NAMESPACE_MNT;
	break;
    case TMPFS_INDOM:
	need_refresh[CLUSTER_TMPFS]++;
	ns_flags |= LINUX_NAMESPACE_MNT;
	break;
    case SWAPDEV_INDOM:
	need_refresh[CLUSTER_SWAPDEV]++;
	break;
    case NFS_INDOM:
    case NFS3_INDOM:
    case NFS4_CLI_INDOM:
    case NFS4_SVR_INDOM:
	need_refresh[CLUSTER_NET_NFS]++;
	ns_flags |= LINUX_NAMESPACE_MNT;
	break;
    case SCSI_INDOM:
	need_refresh[CLUSTER_SCSI]++;
	break;
    case SLAB_INDOM:
	need_refresh[CLUSTER_SLAB]++;
	break;
    case ICMPMSG_INDOM:
	need_refresh[CLUSTER_NET_SNMP]++;
	break;
    /* no default label : pmdaInstance will pick up errors */
    }

    if ((container = linux_ctx_container(pmda->e_context)) != NULL) {
	sts = container_enter_namespaces(rootfd, container, ns_flags);
	if (sts < 0)
	    return sts;
    }
    linux_refresh(pmda, need_refresh, container);
    sts = pmdaInstance(indom, inst, name, result, pmda);
    if (container) {
	container_close_network(container);
	container_leave_namespaces(rootfd, ns_flags);
    }
    return sts;
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
    long		sl;
    struct filesys	*fs;
    net_addr_t		*addrp;
    net_interface_t	*netip;
    scsi_entry_t	*scsi_entry;

    if (mdesc->m_user != NULL) {
	/* 
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not all metrics support this - those that
	 * don't have NULL for the m_user field in their respective
         * metrictab slot.
	 */
	if (idp->cluster == CLUSTER_VMSTAT) {
	    if (!(_pm_have_proc_vmstat) ||
		*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
	    	return 0; /* no value available on this kernel */
	}
	else
	if (idp->cluster == CLUSTER_NET_SNMP) {
	    __uint64_t value;

	    /* network.icmpmsg has an indom - deal with it now */
	    if (idp->item == 88 || idp->item == 89) {
		if (inst > NR_ICMPMSG_COUNTERS)
		    return PM_ERR_INST;
		value = *((__uint64_t *)mdesc->m_user + inst);
		if (value == (__uint64_t)-1)
		    return 0;	/* no value for this instance */
		atom->ull = value;
		return 1;
	    }
	    if (*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
		if (idp->item != 53)	/* tcp.maxconn is special */
		    return 0; /* no value available on this kernel */
	}
	else
	if (idp->cluster == CLUSTER_NET_NETSTAT) {
	    if (*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
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
		/* no values available - expected without /proc/net/rpc/nfsd */
	    	return 0; /* no values available */
	    if (idp->item >= 51 && idp->item <= 57 && proc_net_rpc.server.errcode != 0)
		/* no values available - expected without /proc/net/rpc/nfsd */
	    	return 0; /* no values available */
	}
	if (idp->cluster == CLUSTER_SYSFS_KERNEL) {
	    /* no values available for udev metrics */
	    if (idp->item == 0 && !sysfs_kernel.valid_uevent_seqnum) {
		return 0;
	    }
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
	default:
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
	case 0: /* kernel.percpu.cpu.user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_user[inst] / proc_stat.hz);
	    break;
	case 1: /* kernel.percpu.cpu.nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_nice[inst] / proc_stat.hz);
	    break;
	case 2: /* kernel.percpu.cpu.sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_sys[inst] / proc_stat.hz);
	    break;
	case 3: /* kernel.percpu.cpu.idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.p_idle[inst] / proc_stat.hz);
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
	case 56: /* kernel.percpu.cpu.irq.soft */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_sirq[inst] / proc_stat.hz);
	    break;
	case 57: /* kernel.percpu.cpu.irq.hard */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_irq[inst] / proc_stat.hz);
	    break;
	case 58: /* kernel.percpu.cpu.steal */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_steal[inst] / proc_stat.hz);
	    break;
	case 61: /* kernel.percpu.cpu.guest */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.p_guest[inst] / proc_stat.hz);
	    break;
	case 76: /* kernel.percpu.cpu.vuser */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.p_user[inst] - (double)proc_stat.p_guest[inst])
			/ proc_stat.hz);
	    break;
	case 62: /* kernel.pernode.cpu.user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_user[inst] / proc_stat.hz);
	    break;
	case 63: /* kernel.pernode.cpu.nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_nice[inst] / proc_stat.hz);
	    break;
	case 64: /* kernel.pernode.cpu.sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_sys[inst] / proc_stat.hz);
	    break;
	case 65: /* kernel.pernode.cpu.idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.n_idle[inst] / proc_stat.hz);
	    break;
	case 69: /* kernel.pernode.cpu.wait.total */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_wait[inst] / proc_stat.hz);
	    break;
	case 66: /* kernel.pernode.cpu.intr */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.n_irq[inst] +
				(double)proc_stat.n_sirq[inst]) / proc_stat.hz);
	    break;
	case 70: /* kernel.pernode.cpu.irq.soft */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_sirq[inst] / proc_stat.hz);
	    break;
	case 71: /* kernel.pernode.cpu.irq.hard */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_irq[inst] / proc_stat.hz);
	    break;
	case 67: /* kernel.pernode.cpu.steal */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_steal[inst] / proc_stat.hz);
	    break;
	case 68: /* kernel.pernode.cpu.guest */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.n_guest[inst] / proc_stat.hz);
	    break;
	case 77: /* kernel.pernode.cpu.vuser */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.n_user[inst] - (double)proc_stat.n_guest[inst])
			/ proc_stat.hz);
	    break;

	case 8: /* swap.pagesin */
	    if (_pm_have_proc_vmstat)
		atom->ul = _pm_proc_vmstat.pswpin;
	    else
		atom->ul = proc_stat.swap[0];
	    break;
	case 9: /* swap.pagesout */
	    if (_pm_have_proc_vmstat)
		atom->ul = _pm_proc_vmstat.pswpout;
	    else
		atom->ul = proc_stat.swap[1];
	    break;
	case 10: /* swap.in */
	    if (_pm_have_proc_vmstat)
		return PM_ERR_APPVERSION; /* no swap operation counts in 2.6 */
	    else
		atom->ul = proc_stat.page[0];
	    break;
	case 11: /* swap.out */
	    if (_pm_have_proc_vmstat)
		return PM_ERR_APPVERSION; /* no swap operation counts in 2.6 */
	    else
		atom->ul = proc_stat.page[1];
	    break;
	case 12: /* kernel.all.intr */
	    _pm_assign_utype(_pm_intr_size, atom, proc_stat.intr);
	    break;
	case 13: /* kernel.all.pswitch */
	    _pm_assign_utype(_pm_ctxt_size, atom, proc_stat.ctxt);
	    break;
	case 14: /* kernel.all.sysfork */
	    _pm_assign_ulong(atom, proc_stat.processes);
	    break;

	case 20: /* kernel.all.cpu.user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.user / proc_stat.hz);
	    break;
	case 21: /* kernel.all.cpu.nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.nice / proc_stat.hz);
	    break;
	case 22: /* kernel.all.cpu.sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.sys / proc_stat.hz);
	    break;
	case 23: /* kernel.all.cpu.idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.idle / proc_stat.hz);
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
	case 53: /* kernel.all.cpu.irq.soft */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.sirq / proc_stat.hz);
	    break;
	case 54: /* kernel.all.cpu.irq.hard */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.irq / proc_stat.hz);
	    break;
	case 55: /* kernel.all.cpu.steal */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.steal / proc_stat.hz);
	    break;
	case 60: /* kernel.all.cpu.guest */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.guest / proc_stat.hz);
	    break;
	case 78: /* kernel.all.cpu.vuser */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.user - (double)proc_stat.guest)
				/ proc_stat.hz);
	    break;

	case 19: /* hinv.nnode */
	    atom->ul = indomtab[NODE_INDOM].it_numinst;
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
    	switch (idp->item) {
	case 0: /* mem.physmem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemTotal >> 10;
	    break;
	case 1: /* mem.util.used (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
		return 0; /* no values available */
	    atom->ull = (proc_meminfo.MemTotal - proc_meminfo.MemFree) >> 10;
	    break;
	case 2: /* mem.util.free (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree >> 10;
	    break;
	case 3: /* mem.util.shared (in kbytes) */
	    /*
	     * If this metric is exported by the running kernel, it is always
	     * zero (deprecated). PCP exports it for compatibility with older
	     * PCP monitoring tools, e.g. pmgsys running on IRIX(TM).
	     */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemShared))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemShared >> 10;
	    break;
	case 4: /* mem.util.bufmem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Buffers))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Buffers >> 10;
	    break;
	case 5: /* mem.util.cached (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Cached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Cached >> 10;
	    break;
	case 6: /* swap.length (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal;
	    break;
	case 7: /* swap.used (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
		return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal - proc_meminfo.SwapFree;
	    break;
	case 8: /* swap.free (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree;
	    break;
	case 9: /* hinv.physmem (in mbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ul = proc_meminfo.MemTotal >> 20;
	    break;
	case 10: /* mem.freemem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree >> 10;
	    break;
	case 11: /* hinv.pagesize (in bytes) */
	    atom->ul = _pm_system_pagesize;
	    break;
	case 12: /* mem.util.other (in kbytes) */
	    /* other = used - (cached+buffers) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.MemFree) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Cached) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Buffers))
		return 0; /* no values available */
	    sl = (proc_meminfo.MemTotal -
		 proc_meminfo.MemFree -
		 proc_meminfo.Cached -
		 proc_meminfo.Buffers) >> 10;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	case 13: /* mem.util.swapCached (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapCached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapCached >> 10;
	    break;
	case 14: /* mem.util.active (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Active))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Active >> 10;
	    break;
	case 15: /* mem.util.inactive (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Inactive >> 10;
	    break;
	case 16: /* mem.util.highTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.HighTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighTotal >> 10;
	    break;
	case 17: /* mem.util.highFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.HighFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighFree >> 10;
	    break;
	case 18: /* mem.util.lowTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.LowTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowTotal >> 10;
	    break;
	case 19: /* mem.util.lowFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.LowFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowFree >> 10;
	    break;
	case 20: /* mem.util.swapTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal >> 10;
	    break;
	case 21: /* mem.util.swapFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree >> 10;
	    break;
	case 22: /* mem.util.dirty (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Dirty))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Dirty >> 10;
	    break;
	case 23: /* mem.util.writeback (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Writeback >> 10;
	    break;
	case 24: /* mem.util.mapped (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Mapped))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Mapped >> 10;
	    break;
	case 25: /* mem.util.slab (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Slab))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Slab >> 10;
	    break;
	case 26: /* mem.util.committed_AS (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Committed_AS))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Committed_AS >> 10;
	    break;
	case 27: /* mem.util.pageTables (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.PageTables))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.PageTables >> 10;
	    break;
	case 28: /* mem.util.reverseMaps (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.ReverseMaps))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.ReverseMaps >> 10;
	    break;
	case 29: /* mem.util.clean_cache (in kbytes) */
	    /* clean=cached-(dirty+writeback) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Cached) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Dirty) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    sl = (proc_meminfo.Cached -
	    	 proc_meminfo.Dirty -
	    	 proc_meminfo.Writeback) >> 10;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	case 30: /* mem.util.anonpages */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.AnonPages))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.AnonPages >> 10;
	   break;
	case 31: /* mem.util.commitLimit (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.CommitLimit))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.CommitLimit >> 10;
	    break;
	case 32: /* mem.util.bounce */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Bounce))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Bounce >> 10;
	   break;
	case 33: /* mem.util.NFS_Unstable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.NFS_Unstable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.NFS_Unstable >> 10;
	   break;
	case 34: /* mem.util.slabReclaimable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.SlabReclaimable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.SlabReclaimable >> 10;
	   break;
	case 35: /* mem.util.slabUnreclaimable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.SlabUnreclaimable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.SlabUnreclaimable >> 10;
	   break;
	case 36: /* mem.util.active_anon */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Active_anon))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Active_anon >> 10;
	   break;
	case 37: /* mem.util.inactive_anon */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive_anon))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Inactive_anon >> 10;
	   break;
	case 38: /* mem.util.active_file */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Active_file))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Active_file >> 10;
	   break;
	case 39: /* mem.util.inactive_file */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive_file))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Inactive_file >> 10;
	   break;
	case 40: /* mem.util.unevictable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Unevictable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Unevictable >> 10;
	   break;
	case 41: /* mem.util.mlocked */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Mlocked))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Mlocked >> 10;
	   break;
	case 42: /* mem.util.shmem */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Shmem))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Shmem >> 10;
	   break;
	case 43: /* mem.util.kernelStack */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.KernelStack))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.KernelStack >> 10;
	   break;
	case 44: /* mem.util.hugepagesTotal */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesTotal))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesTotal;
	   break;
	case 45: /* mem.util.hugepagesFree */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesFree))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesFree;
	   break;
	case 46: /* mem.util.hugepagesRsvd */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesRsvd))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesRsvd;
	   break;
	case 47: /* mem.util.hugepagesSurp */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesSurp))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesSurp;
	   break;
	case 48: /* mem.util.directMap4k */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap4k))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap4k >> 10;
	   break;
	case 49: /* mem.util.directMap2M */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap2M))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap2M >> 10;
	   break;
	case 50: /* mem.util.vmallocTotal */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocTotal))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocTotal >> 10;
	   break;
	case 51: /* mem.util.vmallocUsed */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocUsed))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocUsed >> 10;
	   break;
	case 52: /* mem.util.vmallocChunk */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocChunk))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocChunk >> 10;
	   break;
	case 53: /* mem.util.mmap_copy */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.MmapCopy))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.MmapCopy >> 10;
	   break;
	case 54: /* mem.util.quicklists */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Quicklists))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Quicklists >> 10;
	   break;
	case 55: /* mem.util.corrupthardware */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HardwareCorrupted))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HardwareCorrupted >> 10;
	   break;
	case 56: /* mem.util.anonhugepages */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.AnonHugePages))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.AnonHugePages >> 10;
	   break;
	case 57: /* mem.util.directMap1G */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap1G))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap1G >> 10;
	   break;
	case 58: /* mem.util.available */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.MemAvailable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.MemAvailable >> 10;
	   break;
	case 59: /* hinv.hugepagesize (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Hugepagesize))
	    	return 0; /* no values available */
	    atom->ul = proc_meminfo.Hugepagesize;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_LOADAVG: 
	switch(idp->item) {
	case 0:  /* kernel.all.load */
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
	case 2: /* kernel.all.runnable */
		atom->ul = proc_loadavg.runnable;
		break;
	case 3: /* kernel.all.nprocs */
		atom->ul = proc_loadavg.nprocs;
		break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_NET_DEV: /* network.interface */
	if (idp->item == 27) {	/* hinv.ninterface */
	    atom->ul = pmdaCacheOp(INDOM(NET_DEV_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;
	}
	sts = pmdaCacheLookup(INDOM(NET_DEV_INDOM), inst, NULL, (void **)&netip);
	if (sts < 0)
	    return sts;
	if (idp->item <= 15) {
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
	    atom->f = ((float)netip->ioc.speed * 1000000) / 8 / 1024 / 1024;
	    break;
	case 23: /* network.interface.baudrate */
	    if (!netip->ioc.speed)
		return 0;
	    atom->ul = ((long long)netip->ioc.speed * 1000000 / 8);
	    break;
	case 24: /* network.interface.duplex */
	    if (!netip->ioc.duplex)
		return 0;
	    atom->ul = netip->ioc.duplex;
	    break;
	case 25: /* network.interface.up */
	    atom->ul = netip->ioc.linkup;
	    break;
	case 26: /* network.interface.running */
	    atom->ul = netip->ioc.running;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_NET_ADDR:
	sts = pmdaCacheLookup(INDOM(NET_ADDR_INDOM), inst, NULL, (void **)&addrp);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	switch (idp->item) {
	case 0: /* network.interface.inet_addr */
	    if (addrp->has_inet == 0)
		return 0;
	    atom->cp = addrp->inet;
	    break;
	case 1: /* network.interface.ipv6_addr */
	    if (addrp->has_ipv6 == 0)
		return 0;
	    atom->cp = addrp->ipv6;
	    break;
	case 2: /* network.interface.ipv6_scope */
	    if (addrp->has_ipv6 == 0)
		return 0;
	    atom->cp = lookup_ipv6_scope(addrp->ipv6scope);
	    break;
	case 3: /* network.interface.hw_addr */
	    if (addrp->has_hw == 0)
		return 0;
	    atom->cp = addrp->hw_addr;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_FILESYS:
	if (idp->item == 0)
	    atom->ul = pmdaCacheOp(INDOM(FILESYS_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	else {
	    struct statfs *sbuf;
	    __uint64_t ull, used;

	    sts = pmdaCacheLookup(INDOM(FILESYS_INDOM), inst, NULL, (void **)&fs);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;

	    sbuf = &fs->stats;
	    if (!(fs->flags & FSF_FETCHED)) {
		if (statfs(fs->path, sbuf) < 0)
		    return PM_ERR_INST;
		fs->flags |= FSF_FETCHED;
	    }

	    switch (idp->item) {
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
	    	atom->cp = fs->path;
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
	    case 11: /* filesys.readonly */
	    	atom->ul = (scan_filesys_options(fs->options, "ro") != NULL);
		break;
	    default:
		return PM_ERR_PMID;
	    }
	}
	break;

    case CLUSTER_TMPFS: {
	    struct statfs *sbuf;
	    __uint64_t ull, used;

	    sts = pmdaCacheLookup(INDOM(TMPFS_INDOM), inst, NULL, (void **)&fs);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;

	    sbuf = &fs->stats;
	    if (!(fs->flags & FSF_FETCHED)) {
		if (statfs(fs->path, sbuf) < 0)
		    return PM_ERR_INST;
		fs->flags |= FSF_FETCHED;
	    }

	    switch (idp->item) {
	    case 1: /* tmpfs.capacity */
	    	ull = (__uint64_t)sbuf->f_blocks;
	    	atom->ull = ull * sbuf->f_bsize / 1024;
		break;
	    case 2: /* tmpfs.used */
	    	used = (__uint64_t)(sbuf->f_blocks - sbuf->f_bfree);
	    	atom->ull = used * sbuf->f_bsize / 1024;
		break;
	    case 3: /* tmpfs.free */
	    	ull = (__uint64_t)sbuf->f_bfree;
	    	atom->ull = ull * sbuf->f_bsize / 1024;
		break;
	    case 4: /* tmpfs.maxfiles */
	    	atom->ul = sbuf->f_files;
		break;
	    case 5: /* tmpfs.usedfiles */
	    	atom->ul = sbuf->f_files - sbuf->f_ffree;
		break;
	    case 6: /* tmpfs.freefiles */
	    	atom->ul = sbuf->f_ffree;
		break;
	    case 7: /* tmpfs.full */
		used = (__uint64_t)(sbuf->f_blocks - sbuf->f_bfree);
		ull = used + (__uint64_t)sbuf->f_bavail;
		atom->d = (100.0 * (double)used) / (double)ull;
		break;
	    default:
		return PM_ERR_PMID;
	    }
	}
	break;

    case CLUSTER_SWAPDEV: {
	struct swapdev *swap;

	sts = pmdaCacheLookup(INDOM(SWAPDEV_INDOM), inst, NULL, (void **)&swap);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;

	switch (idp->item) {
	case 0: /* swapdev.free (kbytes) */
	    atom->ul = swap->size - swap->used;
	    break;
	case 1: /* swapdev.length (kbytes) */
	case 2: /* swapdev.maxswap (kbytes) */
	    atom->ul = swap->size;
	    break;
	case 3: /* swapdev.vlength (kbytes) */
	    atom->ul = 0;
	    break;
	case 4: /* swapdev.priority */
	    atom->l = swap->priority;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }

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
	    if (inst < NR_RPC_COUNTERS)
		atom->ul = proc_net_rpc.client.reqcounts[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 12: /* nfs.server.reqs */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    if (inst < NR_RPC_COUNTERS)
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
	    if (inst < NR_RPC3_COUNTERS)
		atom->ul = proc_net_rpc.client.reqcounts3[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 63: /* nfs3.server.reqs */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    if (inst < NR_RPC3_COUNTERS)
		atom->ul = proc_net_rpc.server.reqcounts3[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 64: /* nfs4.client.calls */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC4_CLI_COUNTERS; i++) {
		atom->ul += proc_net_rpc.client.reqcounts4[i];
	    }
	    break;

	case 66: /* nfs4.server.calls */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    for (atom->ul=0, i=0; i < NR_RPC4_SVR_COUNTERS; i++) {
		atom->ul += proc_net_rpc.server.reqcounts4[i+1];
	    }
	    break;

	case 65: /* nfs4.client.reqs */
	    if (proc_net_rpc.client.errcode != 0)
	    	return 0; /* no values available */
	    if (inst < NR_RPC4_CLI_COUNTERS)
		atom->ul = proc_net_rpc.client.reqcounts4[inst];
	    else
	    	return PM_ERR_INST;
	    break;

	case 67: /* nfs4.server.reqs */
	    if (proc_net_rpc.server.errcode != 0)
	    	return 0; /* no values available */
	    if (inst && inst <= NR_RPC4_SVR_COUNTERS)
		atom->ul = proc_net_rpc.server.reqcounts4[inst];
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

    case CLUSTER_SLAB:
	if (proc_slabinfo.ncaches == 0)
	    return 0; /* no values available */

	if (inst >= proc_slabinfo.ncaches)
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
	scsi_entry = NULL;
	sts = pmdaCacheLookup(INDOM(SCSI_INDOM), inst, NULL, (void **)&scsi_entry);
	if (sts < 0)
	    return sts;
	if (sts == PMDA_CACHE_INACTIVE)
	    return PM_ERR_INST;
	switch (idp->item) {
	case 0: /* hinv.map.scsi */
	    atom->cp = (scsi_entry && scsi_entry->dev_name) ? scsi_entry->dev_name : "";
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
	    atom->cp = get_distro_info();
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_CPUINFO:
	if (idp->item != 7 && /* hinv.machine is singular */
	    (inst >= proc_cpuinfo.cpuindom->it_numinst))
	    return PM_ERR_INST;
	switch(idp->item) {
	case 0: /* hinv.cpu.clock */
	    if (proc_cpuinfo.cpuinfo[inst].clock == 0.0)
		return 0;
	    atom->f = proc_cpuinfo.cpuinfo[inst].clock;
	    break;
	case 1: /* hinv.cpu.vendor */
	    i = proc_cpuinfo.cpuinfo[inst].vendor;
	    atom->cp = linux_strings_lookup(i);
	    if (atom->cp == NULL)
		atom->cp = "unknown";
	    break;
	case 2: /* hinv.cpu.model */
	    if ((i = proc_cpuinfo.cpuinfo[inst].model) < 0)
		i = proc_cpuinfo.cpuinfo[inst].model_name;
	    atom->cp = linux_strings_lookup(i);
	    if (atom->cp == NULL)
		atom->cp = "unknown";
	    break;
	case 3: /* hinv.cpu.stepping */
	    i = proc_cpuinfo.cpuinfo[inst].stepping;
	    atom->cp = linux_strings_lookup(i);
	    if (atom->cp == NULL)
		atom->cp = "unknown";
	    break;
	case 4: /* hinv.cpu.cache */
	    if (!proc_cpuinfo.cpuinfo[inst].cache)
		return 0;
	    atom->ul = proc_cpuinfo.cpuinfo[inst].cache;
	    break;
	case 5: /* hinv.cpu.bogomips */
	    if (proc_cpuinfo.cpuinfo[inst].bogomips == 0.0)
		return 0;
	    atom->f = proc_cpuinfo.cpuinfo[inst].bogomips;
	    break;
	case 6: /* hinv.map.cpu_num */
	    atom->ul = proc_cpuinfo.cpuinfo[inst].cpu_num;
	    break;
	case 7: /* hinv.machine */
	    atom->cp = proc_cpuinfo.machine;
	    break;
	case 8: /* hinv.map.cpu_node */
	    atom->ul = proc_cpuinfo.cpuinfo[inst].node;
	    break;
	case 9: /* hinv.cpu.model_name */
	    if ((i = proc_cpuinfo.cpuinfo[inst].model_name) < 0)
		i = proc_cpuinfo.cpuinfo[inst].model;
	    atom->cp = linux_strings_lookup(i);
	    if (atom->cp == NULL)
		atom->cp = "unknown";
	    break;
	case 10: /* hinv.cpu.flags */
	    i = proc_cpuinfo.cpuinfo[inst].flags;
	    atom->cp = linux_strings_lookup(i);
	    if (atom->cp == NULL)
		atom->cp = "unknown";
	    break;
	case 11: /* hinv.cpu.cache_alignment */
	    if (!proc_cpuinfo.cpuinfo[inst].cache_align)
		return 0;
	    atom->ul = proc_cpuinfo.cpuinfo[inst].cache_align;
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_SYSFS_DEVICES:
	switch (idp->item) {
	case 0: /* hinv.cpu.online */
	    if (inst >= proc_cpuinfo.cpuindom->it_numinst)
		return PM_ERR_INST;
	    atom->ul = refresh_sysfs_online(inst, "cpu");
	    break;
	case 1: /* hinv.node.online */
	    if (inst >= proc_cpuinfo.node_indom->it_numinst)
		return PM_ERR_INST;
	    atom->ul = refresh_sysfs_online(inst, "node");
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

    case CLUSTER_IB: /* deprecated: network.ib, use infiniband PMDA */
        return PM_ERR_APPVERSION;

    case CLUSTER_NUMA_MEMINFO:
	/* NUMA memory metrics from /sys/devices/system/node/nodeX */
	if (inst >= numa_meminfo.node_indom->it_numinst)
	    return PM_ERR_INST;

	switch(idp->item) {
	case 0: /* mem.numa.util.total */
	    sts = linux_table_lookup("MemTotal:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;
	case 1: /* mem.numa.util.free */
	    sts = linux_table_lookup("MemFree:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 2: /* mem.numa.util.used */
	    sts = linux_table_lookup("MemUsed:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 3: /* mem.numa.util.active */
	    sts = linux_table_lookup("Active:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 4: /* mem.numa.util.inactive */
	    sts = linux_table_lookup("Inactive:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 5: /* mem.numa.util.active_anon */
	    sts = linux_table_lookup("Active(anon):", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 6: /* mem.numa.util.inactive_anon */
	    sts = linux_table_lookup("Inactive(anon):", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 7: /* mem.numa.util.active_file */
	    sts = linux_table_lookup("Active(file):", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 8: /* mem.numa.util.inactive_file */
	    sts = linux_table_lookup("Inactive(file):", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 9: /* mem.numa.util.highTotal */
	    sts = linux_table_lookup("HighTotal:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 10: /* mem.numa.util.highFree */
	    sts = linux_table_lookup("HighFree:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 11: /* mem.numa.util.lowTotal */
	    sts = linux_table_lookup("LowTotal:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 12: /* mem.numa.util.lowFree */
	    sts = linux_table_lookup("LowFree:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 13: /* mem.numa.util.unevictable */
	    sts = linux_table_lookup("Unevictable:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 14: /* mem.numa.util.mlocked */
	    sts = linux_table_lookup("Mlocked:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 15: /* mem.numa.util.dirty */
	    sts = linux_table_lookup("Dirty:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 16: /* mem.numa.util.writeback */
	    sts = linux_table_lookup("Writeback:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 17: /* mem.numa.util.filePages */
	    sts = linux_table_lookup("FilePages:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 18: /* mem.numa.util.mapped */
	    sts = linux_table_lookup("Mapped:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 19: /* mem.numa.util.anonPages */
	    sts = linux_table_lookup("AnonPages:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 20: /* mem.numa.util.shmem */
	    sts = linux_table_lookup("Shmem:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 21: /* mem.numa.util.kernelStack */
	    sts = linux_table_lookup("KernelStack:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 22: /* mem.numa.util.pageTables */
	    sts = linux_table_lookup("PageTables:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 23: /* mem.numa.util.NFS_Unstable */
	    sts = linux_table_lookup("NFS_Unstable:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 24: /* mem.numa.util.bounce */
	    sts = linux_table_lookup("Bounce:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 25: /* mem.numa.util.writebackTmp */
	    sts = linux_table_lookup("WritebackTmp:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 26: /* mem.numa.util.slab */
	    sts = linux_table_lookup("Slab:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 27: /* mem.numa.util.slabReclaimable */
	    sts = linux_table_lookup("SReclaimable:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 28: /* mem.numa.util.slabUnreclaimable */
	    sts = linux_table_lookup("SUnreclaim:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 29: /* mem.numa.util.hugepagesTotal */
	    sts = linux_table_lookup("HugePages_Total:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 30: /* mem.numa.util.hugepagesFree */
	    sts = linux_table_lookup("HugePages_Free:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 31: /* mem.numa.util.hugepagesSurp */
	    sts = linux_table_lookup("HugePages_Surp:", numa_meminfo.node_info[inst].meminfo,
		    &atom->ull);
	    break;

	case 32: /* mem.numa.alloc.hit */
	    sts = linux_table_lookup("numa_hit", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	case 33: /* mem.numa.alloc.miss */
	    sts = linux_table_lookup("numa_miss", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	case 34: /* mem.numa.alloc.foreign */
	    sts = linux_table_lookup("numa_foreign", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	case 35: /* mem.numa.alloc.interleave_hit */
	    sts = linux_table_lookup("interleave_hit", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	case 36: /* mem.numa.alloc.local_node */
	    sts = linux_table_lookup("local_node", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	case 37: /* mem.numa.alloc.other_node */
	    sts = linux_table_lookup("other_node", numa_meminfo.node_info[inst].memstat,
		    &atom->ull);
	    break;

	default:
	    return PM_ERR_PMID;
	}
	return sts;

    case CLUSTER_INTERRUPTS:
	switch (idp->item) {
	case 3:	/* kernel.all.interrupts.error */
	    atom->ul = irq_err_count;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_INTERRUPT_LINES:
    case CLUSTER_INTERRUPT_OTHER:
	if (inst >= indomtab[CPU_INDOM].it_numinst)
	    return PM_ERR_INST;
	return interrupts_fetch(idp->cluster, idp->item, inst, atom);

    case CLUSTER_DM:
	return proc_partitions_fetch(mdesc, inst, atom);

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}


static int
linux_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    linux_container_t	*container = NULL;
    int			need_refresh[NUM_CLUSTERS] = {0};
    int			i, sts, ns_flags = 0;

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);

	if (idp->cluster >= NUM_CLUSTERS)
	    continue;
	need_refresh[idp->cluster]++;

	switch (idp->cluster) {
	case CLUSTER_STAT:
	case CLUSTER_DM:
	    if (need_refresh[CLUSTER_PARTITIONS] == 0 &&
		is_partitions_metric(pmidlist[i]))
		need_refresh[CLUSTER_PARTITIONS]++;
	    /* In 2.6 kernels, swap.{pagesin,pagesout} are in /proc/vmstat */
	    if (_pm_have_proc_vmstat && idp->cluster == CLUSTER_STAT) {
		if (idp->item >= 8 && idp->item <= 11)
		    need_refresh[CLUSTER_VMSTAT]++;
	    }
	    break;

	case CLUSTER_CPUINFO:
	case CLUSTER_SYSFS_DEVICES:
	case CLUSTER_INTERRUPT_LINES:
	case CLUSTER_INTERRUPT_OTHER:
	case CLUSTER_INTERRUPTS:
	    need_refresh[CLUSTER_STAT]++;
	    break;

	case CLUSTER_KERNEL_UNAME:
	    ns_flags |= LINUX_NAMESPACE_UTS;
	    break;

	case CLUSTER_NET_DEV:
	case CLUSTER_NET_ADDR:
	    ns_flags |= LINUX_NAMESPACE_NET;
	    ns_flags |= LINUX_NAMESPACE_MNT;	/* for /sys access */
	    break;

	case CLUSTER_NET_NFS:
	case CLUSTER_FILESYS:
	case CLUSTER_TMPFS:
	    ns_flags |= LINUX_NAMESPACE_MNT;
	    break;

	case CLUSTER_SEM_LIMITS:
	case CLUSTER_MSG_LIMITS:
	case CLUSTER_SHM_LIMITS:
	    ns_flags |= LINUX_NAMESPACE_IPC;
	    break;
	}
    }

    if ((container = linux_ctx_container(pmda->e_context)) != NULL) {
	sts = container_enter_namespaces(rootfd, container, ns_flags);
	if (sts < 0)
	    return sts;
    }
    linux_refresh(pmda, need_refresh, container);
    sts = pmdaFetch(numpmid, pmidlist, resp, pmda);
    if (container) {
	container_close_network(container);
	container_leave_namespaces(rootfd, ns_flags);
    }
    return sts;
}

static int
linux_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
linux_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
linux_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
linux_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

static void
linux_grow_ctxtab(int ctx)
{
    /* expand and initialize the per client context table */
    ctxtab = (perctx_t *)realloc(ctxtab, (ctx+1)*sizeof(ctxtab[0]));
    if (ctxtab == NULL) {
	__pmNoMem("grow_ctxtab", (ctx+1)*sizeof(ctxtab[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    while (num_ctx <= ctx) {
	memset(&ctxtab[num_ctx], 0, sizeof(perctx_t));
	num_ctx++;
    }
    memset(&ctxtab[ctx], 0, sizeof(perctx_t));
}

static void
linux_end_context(int ctx)
{
    if (ctx >= 0 && ctx < num_ctx) {
	if (ctxtab[ctx].container.name)
	    free(ctxtab[ctx].container.name);
	if (ctxtab[ctx].container.netfd)
	    close(ctxtab[ctx].container.netfd);
	memset(&ctxtab[ctx], 0, sizeof(perctx_t));
    }
}

static int
linux_attribute(int ctx, int attr, const char *value, int len, pmdaExt *pmda)
{
    if (attr == PCP_ATTR_CONTAINER) {
	if (ctx >= num_ctx)
	    linux_grow_ctxtab(ctx);
	if (ctxtab[ctx].container.name)
	    free(ctxtab[ctx].container.name);
	if ((ctxtab[ctx].container.name = strdup(value)) == NULL)
	    return -ENOMEM;
	ctxtab[ctx].container.length = len;
	ctxtab[ctx].container.netfd = -1;
	ctxtab[ctx].container.pid = 0;
    }
    return pmdaAttribute(ctx, attr, value, len, pmda);
}

pmInDom
linux_indom(int serial)
{
    return indomtab[serial].it_indom;
}

pmdaIndom *
linux_pmda_indom(int serial)
{
    return &indomtab[serial];
}

/*
 * Helper routines for accessing a generic static string dictionary
 */

char *
linux_strings_lookup(int index)
{
    char *value;
    pmInDom dict = INDOM(STRINGS_INDOM);

    if (pmdaCacheLookup(dict, index, &value, NULL) == PMDA_CACHE_ACTIVE)
	return value;
    return NULL;
}

int
linux_strings_insert(const char *buf)
{
    pmInDom dict = INDOM(STRINGS_INDOM);
    return pmdaCacheStore(dict, PMDA_CACHE_ADD, buf, NULL);
}

/*
 * Initialise the agent (both daemon and DSO).
 */

void
__PMDA_INIT_CALL
linux_init(pmdaInterface *dp)
{
    int		i, major, minor, point;
    size_t	nmetrics, nindoms;
    char	*envpath;
    __pmID_int	*idp;

    _pm_system_pagesize = getpagesize();
    if ((envpath = getenv("LINUX_STATSPATH")) != NULL)
	linux_statspath = envpath;

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "linux DSO", helppath);
    } else {
	if (username)
	    __pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;
    dp->comm.flags |= PDU_FLAG_CONTAINER;

    dp->version.six.instance = linux_instance;
    dp->version.six.fetch = linux_fetch;
    dp->version.six.text = linux_text;
    dp->version.six.pmid = linux_pmid;
    dp->version.six.name = linux_name;
    dp->version.six.children = linux_children;
    dp->version.six.attribute = linux_attribute;
    dp->version.six.ext->e_endCallBack = linux_end_context;
    pmdaSetFetchCallBack(dp, linux_fetchCallBack);

    proc_stat.cpu_indom = proc_cpuinfo.cpuindom = &indomtab[CPU_INDOM];
    numa_meminfo.node_indom = proc_cpuinfo.node_indom = &indomtab[NODE_INDOM];
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
    if (sscanf(kernel_uname.release, "%d.%d.%d", &major, &minor, &point) == 3) {
	if (major < 2 || (major == 2 && minor <= 4)) {	/* 2.4 and earlier */
	    _pm_ctxt_size = 4;
	    _pm_intr_size = 4;
	    _pm_cputime_size = 4;
	    _pm_idletime_size = sizeof(unsigned long);
	}
	else if (major == 2 && minor == 6 &&
		 point >= 0 && point <= 4) {  /* 2.6.0->.4 */
	    _pm_cputime_size = 4;
	    _pm_idletime_size = 4;
	}
    }
    for (i = 0; i < sizeof(metrictab)/sizeof(pmdaMetric); i++) {
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
	    case 53:	/* kernel.all.cpu.irq.soft */
	    case 54:	/* kernel.all.cpu.irq.hard */
	    case 55:	/* kernel.all.cpu.steal */
	    case 56:	/* kernel.percpu.cpu.irq.soft */
	    case 57:	/* kernel.percpu.cpu.irq.hard */
	    case 58:	/* kernel.percpu.cpu.steal */
	    case 60:	/* kernel.all.cpu.guest */
	    case 78:	/* kernel.all.cpu.vuser */
	    case 61:	/* kernel.percpu.cpu.guest */
	    case 76:	/* kernel.percpu.cpu.vuser */
	    case 62:	/* kernel.pernode.cpu.user */
	    case 63:	/* kernel.pernode.cpu.nice */
	    case 64:	/* kernel.pernode.cpu.sys */
	    case 69:	/* kernel.pernode.cpu.wait.total */
	    case 66:	/* kernel.pernode.cpu.intr */
	    case 70:	/* kernel.pernode.cpu.irq.soft */
	    case 71:	/* kernel.pernode.cpu.irq.hard */
	    case 67:	/* kernel.pernode.cpu.steal */
	    case 68:	/* kernel.pernode.cpu.guest */
	    case 77:	/* kernel.pernode.cpu.vuser */
		_pm_metric_type(metrictab[i].m_desc.type, _pm_cputime_size);
		break;
	    case 3:	/* kernel.percpu.cpu.idle */
	    case 23:	/* kernel.all.cpu.idle */
	    case 65:	/* kernel.pernode.cpu.idle */
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

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    proc_vmstat_init();
    interrupts_init(metrictab, nmetrics);

    rootfd = pmdaRootConnect(NULL);
    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    _isDSO = 0;
    __pmSetProgname(argv[0]);

    snprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, LINUX, "linux.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    linux_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
