/*
 * Linux PMDA
 *
 * Copyright (c) 2012-2021 Red Hat.
 * Copyright (c) 2016-2017 Fujitsu.
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
#include "linux.h"
#undef LINUX /* defined in NSS/NSPR headers as something different, which we do not need. */
#include "domain.h"

#include <ctype.h>
#include <sys/vfs.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/utsname.h>
#include <pwd.h>
#include <grp.h>

#include "ipc.h"
#include "login.h"
#include "filesys.h"
#include "getinfo.h"
#include "swapdev.h"
#include "linux_table.h"
#include "namespaces.h"
#include "sysfs_kernel.h"
#include "proc_cpuinfo.h"
#include "proc_interrupts.h"
#include "proc_stat.h"
#include "proc_locks.h"
#include "proc_loadavg.h"
#include "proc_meminfo.h"
#include "proc_net_dev.h"
#include "proc_net_rpc.h"
#include "proc_net_sockstat.h"
#include "proc_net_sockstat6.h"
#include "proc_net_raw.h"
#include "proc_net_tcp.h"
#include "proc_net_udp.h"
#include "proc_net_unix.h"
#include "proc_partitions.h"
#include "proc_net_netstat.h"
#include "proc_net_snmp6.h"
#include "proc_net_snmp.h"
#include "proc_scsi.h"
#include "proc_slabinfo.h"
#include "proc_uptime.h"
#include "proc_sys_fs.h"
#include "proc_sys_kernel.h"
#include "proc_vmstat.h"
#include "proc_net_softnet.h"
#include "proc_buddyinfo.h"
#include "proc_zoneinfo.h"
#include "proc_fs_nfsd.h"
#include "numa_meminfo.h"
#include "ksm.h"
#include "sysfs_fchost.h"
#include "sysfs_tapestats.h"
#include "proc_tty.h"
#include "proc_pressure.h"

static proc_stat_t		proc_stat;
static proc_meminfo_t		proc_meminfo;
static proc_loadavg_t		proc_loadavg;
static proc_net_all_t		proc_net_all;
static proc_net_rpc_t		proc_net_rpc;
static proc_net_tcp_t		proc_net_tcp;
static proc_net_tcp6_t		proc_net_tcp6;
static proc_net_udp_t		proc_net_udp;
static proc_net_udp6_t		proc_net_udp6;
static proc_net_raw_t		proc_net_raw;
static proc_net_raw6_t		proc_net_raw6;
static proc_net_sockstat_t	proc_net_sockstat;
static proc_net_sockstat6_t	proc_net_sockstat6;
static proc_net_unix_t		proc_net_unix;
static proc_net_udp6_t		proc_net_udp6;
static struct utsname		kernel_uname;
static char 			uname_string[sizeof(kernel_uname)];
static proc_slabinfo_t		proc_slabinfo;
static sem_limits_t		sem_limits;
static msg_limits_t		msg_limits;
static shm_limits_t		shm_limits;
static proc_uptime_t		proc_uptime;
static proc_sys_fs_t		proc_sys_fs;
static proc_sys_kernel_t	proc_sys_kernel;
static sysfs_kernel_t		sysfs_kernel;
static shm_info_t		shm_info;
static sem_info_t		sem_info;
static msg_info_t		msg_info;
static login_info_t		login_info;
static proc_net_softnet_t	proc_net_softnet;
static proc_buddyinfo_t		proc_buddyinfo;
static proc_pressure_t		proc_pressure;
static ksm_info_t               ksm_info;
static proc_fs_nfsd_t 		proc_fs_nfsd;
static proc_locks_t 		proc_locks;
static int                      proc_tty_permission;

static int		_isDSO = 1;	/* =0 I am a daemon */
static int		rootfd = -1;	/* af_unix pmdaroot */
static int		all_access;	/* =1 no access checks */
static int		hz;

/* globals */
int _pm_pageshift; /* for hinv.pagesize and for pages -> bytes */
int _pm_ncpus; /* maximum number of processors configurable */
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
	/* nfsv4.2 client ops */
	{ 53, "seek" },
	{ 54, "allocate" },
	{ 55, "deallocate" },
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
	/* nfsv4.2 server ops */
	{ 60, "allocate" },
	{ 61, "copy" },
	{ 62, "copy_notify" },
	{ 63, "deallocate" },
	{ 64, "io_advise" },
	{ 65, "layouterror" },
	{ 66, "layoutstats" },
	{ 67, "offload_cancel" },
	{ 68, "offload_status" },
	{ 69, "read_plus" },
	{ 70, "seek" },
	{ 71, "write_same" },
};

static pmdaInstid pressureavg_indom_id[] = {
    { 10, "10 second" }, { 60, "1 minute" }, { 300, "5 minute" }
};

static pmdaIndom indomtab[] = {
    { CPU_INDOM, 0, NULL }, /* cached */
    { DISK_INDOM, 0, NULL }, /* cached */
    { LOADAVG_INDOM, 3, loadavg_indom_id },
    { NET_DEV_INDOM, 0, NULL },
    { INTERRUPT_INDOM, 0, NULL },
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
    { NODE_INDOM, 0, NULL }, /* cached */
    { PROC_CGROUP_SUBSYS_INDOM, 0, NULL },
    { PROC_CGROUP_MOUNTS_INDOM, 0, NULL },
    { 0 }, /* deprecated LV_INDOM */
    { ICMPMSG_INDOM, NR_ICMPMSG_COUNTERS, _pm_proc_net_snmp_indom_id },
    { DM_INDOM, 0, NULL }, /* cached */
    { MD_INDOM, 0, NULL }, /* cached */
    { 0 }, /* deprecated INTERRUPT_NAMES_INDOM */
    { 0 }, /* deprecated SOFTIRQ_NAMES_INDOM */
    { IPC_STAT_INDOM, 0, NULL },
    { IPC_MSG_INDOM, 0, NULL },
    { IPC_SEM_INDOM, 0, NULL },
    { BUDDYINFO_INDOM, 0, NULL },
    { ZONEINFO_INDOM, 0, NULL },
    { ZONEINFO_PROTECTION_INDOM, 0, NULL },
    { TAPEDEV_INDOM, 0, NULL },
    { TTY_INDOM, 0, NULL },
    { SOFTIRQ_INDOM, 0, NULL },
    { PRESSUREAVG_INDOM, 3, pressureavg_indom_id },
    { ZRAM_INDOM, 0, NULL },
    { FCHOST_INDOM, 0, NULL },
    { INTERRUPT_CPU_INDOM, 0, NULL },
    { SOFTIRQ_CPU_INDOM, 0, NULL },
    { WWID_INDOM, 0, NULL },
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

/* kernel.percpu.cpu.guest_nice */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,83), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.percpu.cpu.vnice */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,84), KERNEL_UTYPE, CPU_INDOM, PM_SEM_COUNTER,
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

/* kernel.pernode.cpu.guest_nice */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,85), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.pernode.cpu.vnice */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,86), KERNEL_UTYPE, NODE_INDOM, PM_SEM_COUNTER,
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
      { PMDA_PMID(CLUSTER_STAT,59), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.map.scsi_id */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,103), PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },


/* disk.dev.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,72), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.write_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,73), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.total_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,79), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.capacity */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,87), PM_TYPE_U64, DISK_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.discard */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,88), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.blkdiscard */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,89), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.discard_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,90), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.discard_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,91), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.discard_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,92), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.flush */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,93), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.dev.flush_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,94), PM_TYPE_U32, DISK_INDOM, PM_SEM_COUNTER, 
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

/* disk.all.write_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.total_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
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

/* kernel.all.running */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,15), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* kernel.all.blocked */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,16), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* kernel.all.boottime */
    { NULL,
      { PMDA_PMID(CLUSTER_STAT,17), PM_TYPE_64, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },

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

/* kernel.all.cpu.guest_nice */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,81), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* kernel.all.cpu.vnice */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,82), KERNEL_UTYPE, PM_INDOM_NULL, PM_SEM_COUNTER,
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
 */

/* kernel.all.uptime */
    { NULL,
      { PMDA_PMID(CLUSTER_UPTIME,0), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },

/* kernel.all.idletime */
    { NULL,
      { PMDA_PMID(CLUSTER_UPTIME,1), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_INSTANT, 
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

/* mem.util.hugepagesTotalBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.util.hugepagesFreeBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.util.hugepagesRsvdBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.util.hugepagesSurpBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_MEMINFO,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
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

/* mem.numa.max_bandwidth */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,38), PM_TYPE_DOUBLE, NODE_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(1,-1,0,PM_SPACE_MBYTE,PM_TIME_SEC,0) }, },

/* mem.numa.util.hugepagesTotalBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,39), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.numa.util.hugepagesFreeBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,40), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* mem.numa.util.hugepagesSurpBytes */
    { NULL,
      { PMDA_PMID(CLUSTER_NUMA_MEMINFO,41), PM_TYPE_U64, NODE_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

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
 * /proc/buddyinfo cluster
 */

    /* mem.buddyinfo.pages */
    { NULL,
      { PMDA_PMID(CLUSTER_BUDDYINFO,0), KERNEL_ULONG, BUDDYINFO_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, }, 
    /* mem.buddyinfo.bytes */
    { NULL,
      { PMDA_PMID(CLUSTER_BUDDYINFO,1), PM_TYPE_U64, BUDDYINFO_INDOM,
	PM_SEM_INSTANT, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, }, 

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
      { PMDA_PMID(CLUSTER_NET_DEV,23), PM_TYPE_U64, NET_DEV_INDOM, PM_SEM_DISCRETE, 
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

/* network.interface.wireless */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,28), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.type */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_DEV,29), PM_TYPE_U32, NET_DEV_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.inet_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,0), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.ipv6_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,1), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.ipv6_scope */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,2), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* network.interface.hw_addr */
    { NULL, 
      { PMDA_PMID(CLUSTER_NET_ADDR,3), PM_TYPE_STRING, NET_ADDR_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * aggregate network interface metrics cluster
 */

/* network.all.in.bytes */
    { &proc_net_all.in.bytes,
      { PMDA_PMID(CLUSTER_NET_ALL,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.all.in.packets */
    { &proc_net_all.in.packets,
      { PMDA_PMID(CLUSTER_NET_ALL,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.in.errors */
    { &proc_net_all.in.errors,
      { PMDA_PMID(CLUSTER_NET_ALL,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.in.drops */
    { &proc_net_all.in.drops,
      { PMDA_PMID(CLUSTER_NET_ALL,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.out.bytes */
    { &proc_net_all.out.bytes,
      { PMDA_PMID(CLUSTER_NET_ALL,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.all.out.packets */
    { &proc_net_all.out.packets,
      { PMDA_PMID(CLUSTER_NET_ALL,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.out.errors */
    { &proc_net_all.out.errors,
      { PMDA_PMID(CLUSTER_NET_ALL,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.out.drops */
    { &proc_net_all.out.drops,
      { PMDA_PMID(CLUSTER_NET_ALL,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.total.bytes */
    { &proc_net_all.total.bytes,
      { PMDA_PMID(CLUSTER_NET_ALL,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,0,PM_SPACE_BYTE,0) }, },

/* network.all.total.packets */
    { &proc_net_all.total.packets,
      { PMDA_PMID(CLUSTER_NET_ALL,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.total.errors */
    { &proc_net_all.total.errors,
      { PMDA_PMID(CLUSTER_NET_ALL,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* network.all.total.drops */
    { &proc_net_all.total.drops,
      { PMDA_PMID(CLUSTER_NET_ALL,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
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
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/*
 * socket stat cluster
 */

/* network.sockstat.total */
  { &proc_net_sockstat.total,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,9), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.inuse */
  { &proc_net_sockstat.tcp_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.orphan */
  { &proc_net_sockstat.tcp_orphan,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,10), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.tw */
  { &proc_net_sockstat.tcp_tw,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,11), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.alloc */
  { &proc_net_sockstat.tcp_alloc,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,12), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp.mem */
  { &proc_net_sockstat.tcp_mem,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,13), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udp.inuse */
  { &proc_net_sockstat.udp_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udp.mem */
  { &proc_net_sockstat.udp_mem,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,14), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udplite.inuse */
  { &proc_net_sockstat.udplite_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.raw.inuse */
  { &proc_net_sockstat.raw_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,6), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.frag.inuse */
  { &proc_net_sockstat.frag_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,15), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.frag.memory */
  { &proc_net_sockstat.frag_memory,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT,16), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.tcp6.inuse */
  { &proc_net_sockstat6.tcp6_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,0), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udp6.inuse */
  { &proc_net_sockstat6.udp6_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,1), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.udplite6.inuse */
  { &proc_net_sockstat6.udplite6_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,2), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.raw6.inuse */
  { &proc_net_sockstat6.raw6_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,3), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.frag6.inuse */
  { &proc_net_sockstat6.frag6_inuse,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,4), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.sockstat.frag6.memory */
  { &proc_net_sockstat6.frag6_memory,
    { PMDA_PMID(CLUSTER_NET_SOCKSTAT6,5), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

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

/* nfs.server.threads.total */
  { &proc_fs_nfsd.th_cnt,
    { PMDA_PMID(CLUSTER_NET_NFS,71), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.threads.pools */
  { &proc_fs_nfsd.pool_cnt,
    { PMDA_PMID(CLUSTER_NET_NFS,72), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.threads.requests */
  { &proc_fs_nfsd.pkts_arrived,
    { PMDA_PMID(CLUSTER_NET_NFS,73), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.threads.enqueued */
  { &proc_fs_nfsd.sock_enqueued,
    { PMDA_PMID(CLUSTER_NET_NFS,74), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.threads.processed */
  { &proc_fs_nfsd.th_woken,
    { PMDA_PMID(CLUSTER_NET_NFS,75), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* nfs.server.threads.timedout */
  { &proc_fs_nfsd.th_timedout,
    { PMDA_PMID(CLUSTER_NET_NFS,76), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER,
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
      { PMDA_PMID(CLUSTER_PARTITIONS,2), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
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
      { PMDA_PMID(CLUSTER_PARTITIONS,5), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,6), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.write_bytes */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,7), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.total_bytes */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,8), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.read_merge */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,9), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.write_merge */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,10), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.avactive */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,11), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.aveq */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,12), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.read_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,13), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.write_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,14), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.total_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,15), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.capacity */
    { NULL,
      { PMDA_PMID(CLUSTER_PARTITIONS,16), PM_TYPE_U64, PARTITIONS_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.discard */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,17), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.blkdiscard */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,18), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.discard_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,19), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.partitions.discard_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,20), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.discard_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,21), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.partitions.flush */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,22), KERNEL_ULONG, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.partitions.flush_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_PARTITIONS,23), PM_TYPE_U32, PARTITIONS_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,0), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,1), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,2), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,3), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,4), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,5), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,6), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.write_bytes */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,7), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.total_bytes */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,8), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.read_merge */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,9), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.write_merge */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,10), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.avactive */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,11), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.aveq */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,12), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.read_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,13), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.write_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,14), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.total_rawactive */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,15), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.capacity */
    { NULL,
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,16), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_DISCRETE,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.discard */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,17), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.blkdiscard */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,18), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.discard_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,19), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.discard_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,20), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.discard_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,21), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* zram.flush */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,22), KERNEL_ULONG, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.flush_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_DEVICES,23), PM_TYPE_U32, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.dev.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,38), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,39), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.dev.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,40), KERNEL_ULONG, DISK_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,41), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,42), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,43), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.discard */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,96), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.blkdiscard */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,97), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.discard_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,98), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.all.discard_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,99), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.discard_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,100), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.all.flush */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,101), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.all.flush_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_STAT,102), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/*
 * zram IO cluster
 */

/* zram.io_stat.failed.reads */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_IO_STAT,0), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.io_stat.failed.writes */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_IO_STAT,1), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.io_stat.invalid */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_IO_STAT,2), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* zram.io_stat.notify_free */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_IO_STAT,3), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * zram MM cluster
 */

/* zram.mm_stat.data_size.original */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,0), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.data_size.compressed */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,1), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.mem.used_total */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,2), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.mem.limit */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,3), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.mem.max_used */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,4), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.pages.same */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,5), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.pages.compacted */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,6), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.mm_stat.pages.huge */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_MM_STAT,7), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/*
 * zram BD cluster
 */

/* zram.bd_stat.count */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_BD_STAT,0), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.bd_stat.reads */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_BD_STAT,1), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* zram.bd_stat.writes */
    { NULL, 
      { PMDA_PMID(CLUSTER_ZRAM_BD_STAT,2), PM_TYPE_U64, ZRAM_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/*
 * kernel_uname cluster
 */

/* kernel.uname.release */
  { kernel_uname.release,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.version */
  { kernel_uname.version,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.sysname */
  { kernel_uname.sysname,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.machine */
  { kernel_uname.machine,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.nodename */
  { kernel_uname.nodename,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 4), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* pmda.uname */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 5), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* pmda.version */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 6), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* kernel.uname.distro */
  { NULL,
    { PMDA_PMID(CLUSTER_KERNEL_UNAME, 7), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE, 
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
    { PMDA_PMID(CLUSTER_NET_SNMP,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* network.tcp.rtomin */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMIN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* network.tcp.rtomax */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_RTOMAX], 
    { PMDA_PMID(CLUSTER_NET_SNMP,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) } },

/* network.tcp.maxconn */
  { &_pm_proc_net_snmp.tcp[_PM_SNMP_TCP_MAXCONN], 
    { PMDA_PMID(CLUSTER_NET_SNMP,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

/* network.rawconn.count */
  { &proc_net_raw.count,
    { PMDA_PMID(CLUSTER_NET_RAW, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.rawconn6.count */
  { &proc_net_raw6.count,
    { PMDA_PMID(CLUSTER_NET_RAW6, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udpconn.established */
  { &proc_net_udp.established,
    { PMDA_PMID(CLUSTER_NET_UDP, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udpconn.listen */
  { &proc_net_udp.listen,
    { PMDA_PMID(CLUSTER_NET_UDP, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udpconn6.established */
  { &proc_net_udp6.established,
    { PMDA_PMID(CLUSTER_NET_UDP6, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udpconn6.listen */
  { &proc_net_udp6.listen,
    { PMDA_PMID(CLUSTER_NET_UDP6, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.unix.datagram.count */
  { &proc_net_unix.datagram_count,
    { PMDA_PMID(CLUSTER_NET_UNIX, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.unix.stream.established */
  { &proc_net_unix.stream_established,
    { PMDA_PMID(CLUSTER_NET_UNIX, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.unix.stream.listen */
  { &proc_net_unix.stream_listen,
    { PMDA_PMID(CLUSTER_NET_UNIX, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.unix.stream.count */
  { &proc_net_unix.stream_count,
    { PMDA_PMID(CLUSTER_NET_UNIX, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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

/* network.tcpconn6.established */
  { &proc_net_tcp6.stat[_PM_TCP_ESTABLISHED],
    { PMDA_PMID(CLUSTER_NET_TCP6, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.syn_sent */
  { &proc_net_tcp6.stat[_PM_TCP_SYN_SENT],
    { PMDA_PMID(CLUSTER_NET_TCP6, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.syn_recv */
  { &proc_net_tcp6.stat[_PM_TCP_SYN_RECV],
    { PMDA_PMID(CLUSTER_NET_TCP6, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.fin_wait1 */
  { &proc_net_tcp6.stat[_PM_TCP_FIN_WAIT1],
    { PMDA_PMID(CLUSTER_NET_TCP6, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.fin_wait2 */
  { &proc_net_tcp6.stat[_PM_TCP_FIN_WAIT2],
    { PMDA_PMID(CLUSTER_NET_TCP6, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.time_wait */
  { &proc_net_tcp6.stat[_PM_TCP_TIME_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP6, 6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.close */
  { &proc_net_tcp6.stat[_PM_TCP_CLOSE],
    { PMDA_PMID(CLUSTER_NET_TCP6, 7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.close_wait */
  { &proc_net_tcp6.stat[_PM_TCP_CLOSE_WAIT],
    { PMDA_PMID(CLUSTER_NET_TCP6, 8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.last_ack */
  { &proc_net_tcp6.stat[_PM_TCP_LAST_ACK],
    { PMDA_PMID(CLUSTER_NET_TCP6, 9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.listen */
  { &proc_net_tcp6.stat[_PM_TCP_LISTEN],
    { PMDA_PMID(CLUSTER_NET_TCP6, 10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcpconn6.closing */
  { &proc_net_tcp6.stat[_PM_TCP_CLOSING],
    { PMDA_PMID(CLUSTER_NET_TCP6, 11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
    { PMDA_PMID(CLUSTER_NET_SNMP,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) } },

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

/* network.udp.ignoredmulti */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_IGNOREDMULTI],
    { PMDA_PMID(CLUSTER_NET_SNMP,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp.memerrors */
  { &_pm_proc_net_snmp.udp[_PM_SNMP_UDP_MEMERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
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

/* network.udplite.ignoredmulti */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_IGNOREDMULTI],
    { PMDA_PMID(CLUSTER_NET_SNMP,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite.memerrors */
  { &_pm_proc_net_snmp.udplite[_PM_SNMP_UDPLITE_MEMERRORS],
    { PMDA_PMID(CLUSTER_NET_SNMP,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
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
 * network IPv6 cluster
 */

/* network.ip6.inreceives */
  { &_pm_proc_net_snmp6[_PM_IP6_INRECEIVES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INRECEIVES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inhdrerrors */
  { &_pm_proc_net_snmp6[_PM_IP6_INHDRERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INHDRERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.intoobigerrors */
  { &_pm_proc_net_snmp6[_PM_IP6_INTOOBIGERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INTOOBIGERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.innoroutes */
  { &_pm_proc_net_snmp6[_PM_IP6_INNOROUTES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INNOROUTES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inaddrerrors */
  { &_pm_proc_net_snmp6[_PM_IP6_INADDRERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INADDRERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inunknownprotos */
  { &_pm_proc_net_snmp6[_PM_IP6_INUNKNOWNPROTOS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INUNKNOWNPROTOS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.intruncatedpkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INTRUNCATEDPKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INTRUNCATEDPKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.indiscards */
  { &_pm_proc_net_snmp6[_PM_IP6_INDISCARDS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INDISCARDS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.indelivers */
  { &_pm_proc_net_snmp6[_PM_IP6_INDELIVERS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INDELIVERS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outforwdatagrams */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTFORWDATAGRAMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTFORWDATAGRAMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outrequests */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTREQUESTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTREQUESTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outdiscards */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTDISCARDS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTDISCARDS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outnoroutes */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTNOROUTES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTNOROUTES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.reasmtimeout */
  { &_pm_proc_net_snmp6[_PM_IP6_REASMTIMEOUT].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_REASMTIMEOUT), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.reasmreqds */
  { &_pm_proc_net_snmp6[_PM_IP6_REASMREQDS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_REASMREQDS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.reasmoks */
  { &_pm_proc_net_snmp6[_PM_IP6_REASMOKS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_REASMOKS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.reasmfails */
  { &_pm_proc_net_snmp6[_PM_IP6_REASMFAILS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_REASMFAILS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.fragoks */
  { &_pm_proc_net_snmp6[_PM_IP6_FRAGOKS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_FRAGOKS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.fragfails */
  { &_pm_proc_net_snmp6[_PM_IP6_FRAGFAILS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_FRAGFAILS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.fragcreates */
  { &_pm_proc_net_snmp6[_PM_IP6_FRAGCREATES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_FRAGCREATES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inmcastpkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INMCASTPKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INMCASTPKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outmcastpkts */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTMCASTPKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTMCASTPKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_INOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inmcastoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_INMCASTOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INMCASTOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outmcastoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTMCASTOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTMCASTOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inbcastoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_INBCASTOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INBCASTOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.outbcastoctets */
  { &_pm_proc_net_snmp6[_PM_IP6_OUTBCASTOCTETS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_OUTBCASTOCTETS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.innoectpkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INNOECTPKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INNOECTPKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inect1pkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INECT1PKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INECT1PKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.inect0pkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INECT0PKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INECT0PKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.ip6.incepkts */
  { &_pm_proc_net_snmp6[_PM_IP6_INCEPKTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_IP6_INCEPKTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inmsgs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INMSGS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INMSGS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inerrors */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outmsgs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTMSGS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTMSGS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outerrors */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.incsumerrors */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INCSUMERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INCSUMERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.indestunreachs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INDESTUNREACHS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INDESTUNREACHS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inpkttoobigs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INPKTTOOBIGS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INPKTTOOBIGS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.intimeexcds */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INTIMEEXCDS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INTIMEEXCDS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inparmproblems */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INPARMPROBLEMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INPARMPROBLEMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inechos */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INECHOS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INECHOS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inechoreplies */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INECHOREPLIES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INECHOREPLIES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.ingroupmembqueries */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INGROUPMEMBQUERIES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INGROUPMEMBQUERIES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.ingroupmembresponses */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INGROUPMEMBRESPONSES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INGROUPMEMBRESPONSES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.ingroupmembreductions */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INGROUPMEMBREDUCTIONS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INGROUPMEMBREDUCTIONS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inroutersolicits */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INROUTERSOLICITS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INROUTERSOLICITS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inrouteradvertisements */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INROUTERADVERTISEMENTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INROUTERADVERTISEMENTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inneighborsolicits */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INNEIGHBORSOLICITS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INNEIGHBORSOLICITS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inneighboradvertisements */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INNEIGHBORADVERTISEMENTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INNEIGHBORADVERTISEMENTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inredirects */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INREDIRECTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INREDIRECTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.inmldv2reports */
  { &_pm_proc_net_snmp6[_PM_ICMP6_INMLDV2REPORTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_INMLDV2REPORTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outdestunreachs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTDESTUNREACHS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTDESTUNREACHS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outpkttoobigs */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTPKTTOOBIGS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTPKTTOOBIGS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outtimeexcds */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTTIMEEXCDS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTTIMEEXCDS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outparmproblems */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTPARMPROBLEMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTPARMPROBLEMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outechos */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTECHOS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTECHOS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outechoreplies */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTECHOREPLIES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTECHOREPLIES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outgroupmembqueries */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTGROUPMEMBQUERIES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTGROUPMEMBQUERIES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outgroupmembresponses */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTGROUPMEMBRESPONSES].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTGROUPMEMBRESPONSES), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outgroupmembreductions */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTGROUPMEMBREDUCTIONS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTGROUPMEMBREDUCTIONS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outroutersolicits */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTROUTERSOLICITS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTROUTERSOLICITS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outrouteradvertisements */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTROUTERADVERTISEMENTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTROUTERADVERTISEMENTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outneighborsolicits */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTNEIGHBORSOLICITS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTNEIGHBORSOLICITS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outneighboradvertisements */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTNEIGHBORADVERTISEMENTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTNEIGHBORADVERTISEMENTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outredirects */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTREDIRECTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTREDIRECTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.icmp6.outmldv2reports */
  { &_pm_proc_net_snmp6[_PM_ICMP6_OUTMLDV2REPORTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_ICMP6_OUTMLDV2REPORTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.indatagrams */
  { &_pm_proc_net_snmp6[_PM_UDP6_INDATAGRAMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_INDATAGRAMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.noports */
  { &_pm_proc_net_snmp6[_PM_UDP6_NOPORTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_NOPORTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.inerrors */
  { &_pm_proc_net_snmp6[_PM_UDP6_INERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_INERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.outdatagrams */
  { &_pm_proc_net_snmp6[_PM_UDP6_OUTDATAGRAMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_OUTDATAGRAMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.rcvbuferrors */
  { &_pm_proc_net_snmp6[_PM_UDP6_RCVBUFERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_RCVBUFERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.sndbuferrors */
  { &_pm_proc_net_snmp6[_PM_UDP6_SNDBUFERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_SNDBUFERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.incsumerrors */
  { &_pm_proc_net_snmp6[_PM_UDP6_INCSUMERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_INCSUMERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udp6.ignoredmulti */
  { &_pm_proc_net_snmp6[_PM_UDP6_IGNOREDMULTI].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDP6_IGNOREDMULTI), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.indatagrams */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_INDATAGRAMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_INDATAGRAMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.noports */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_NOPORTS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_NOPORTS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.inerrors */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_INERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_INERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.outdatagrams */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_OUTDATAGRAMS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_OUTDATAGRAMS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.rcvbuferrors */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_RCVBUFERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_RCVBUFERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.sndbuferrors */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_SNDBUFERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_SNDBUFERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.udplite6.incsumerrors */
  { &_pm_proc_net_snmp6[_PM_UDPLITE6_INCSUMERRORS].val,
    { PMDA_PMID(CLUSTER_NET_SNMP6, _PM_UDPLITE6_INCSUMERRORS), PM_TYPE_U64,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

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

/* network.ip.reasmoverlaps */
  { &_pm_proc_net_netstat.ip[_PM_NETSTAT_IPEXT_REASMOVERLAPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,168), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
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

/* network.tcp.tcpbacklogcoalesce */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPBACKLOGCOALESCE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,132), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpmemorypressureschrono */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMEMORYPRESSURESCHRONO],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,133), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(1,0,0,PM_TIME_MSEC,0,0) } },

/* network.tcp.tcpmd5failure */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMD5FAILURE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,134), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.pfmemallocdrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_PFMEMALLOCDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,135), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpfastopenactivefail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENACTIVEFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,136), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpfastopenblackhole */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENBLACKHOLE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,137), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcphystarttraindetect */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTTRAINDETECT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,141), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcphystarttraincwnd */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTTRAINCWND],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,142), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcphystartdelaydetect */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTDELAYDETECT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,143), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcphystartdelaycwnd */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPHYSTARTDELAYCWND],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,144), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedsynrecv */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDSYNRECV],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,145), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedpaws */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDPAWS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,146), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedseq */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDSEQ],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,147), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedfinwait2 */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDFINWAIT2],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,148), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedtimewait */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDTIMEWAIT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,149), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackskippedchallenge */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKSKIPPEDCHALLENGE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,150), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpwinprobe */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWINPROBE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,151), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpkeepalive */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPKEEPALIVE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,152), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpmtupfail */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMTUPFAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,153), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpmtupsuccess */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMTUPSUCCESS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,154), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpdelivered */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDELIVERED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,155), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpdeliveredce */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDELIVEREDCE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,156), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpackcompressed */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPACKCOMPRESSED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,157), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpzerowindowdrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPZEROWINDOWDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,158), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcprcvqdrop */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPRCVQDROP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,159), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpwqueuetoobig */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPWQUEUETOOBIG],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,160), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpfastopenpassivealtkey */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPFASTOPENPASSIVEALTKEY],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,161), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcptimeoutrehash */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPTIMEOUTREHASH],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,162), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpduplicatedatarehash */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDUPLICATEDATAREHASH],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,163), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpdsackrecvsegs */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKRECVSEGS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,164), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpdsackignoreddubious */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPDSACKIGNOREDDUBIOUS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,165), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpmigratereqsuccess */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMIGRATEREQSUCCESS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,166), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcpmigratereqfailure */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPMIGRATEREQFAILURE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,167), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapablesynrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,119), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapableackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,120), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapablefallbackack */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEFALLBACKACK],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,121), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapablefallbacksynack */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLEFALLBACKSYNACK],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,122), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mptcpretrans */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPTCPRETRANS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,123), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinnotokenfound */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINNOTOKENFOUND],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,124), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinsynrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,125), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinsynackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,126), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinsynackhmacfailure */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINSYNACKHMACFAILURE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,127), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,128), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinackhmacfailure */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINACKHMACFAILURE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,129), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.dssnotmatching */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DSSNOTMATCHING],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,130), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.infinitemaprx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_INFINITEMAPRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,131), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapablesyntx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNTX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,138), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpcapablesynackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPCAPABLESYNACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,139), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpfallbacktokeninit */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFALLBACKTOKENINIT],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,140), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.dssnomatchtcp */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DSSNOMATCHTCP],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,169), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.datacsumerr */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DATACSUMERR],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,170), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.ofoqueuetail */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOQUEUETAIL],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,171), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.ofoqueue */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOQUEUE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,172), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.ofomerge */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_OFOMERGE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,173), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.nodssinwindow */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_NODSSINWINDOW],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,174), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.duplicatedata */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_DUPLICATEDATA],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,175), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.addaddr */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ADDADDR],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,176), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.echoadd */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ECHOADD],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,177), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.portadd */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_PORTADD],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,178), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinportsynrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTSYNRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,179), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinportsynackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTSYNACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,180), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpjoinportackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPJOINPORTACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,181), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mismatchportsynrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MISMATCHPORTSYNRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,182), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mismatchportackrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MISMATCHPORTACKRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,183), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.rmaddr */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMADDR],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,184), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.rmsubflow */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMSUBFLOW],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,185), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mppriotx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPPRIOTX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,186), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mppriorx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPPRIORX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,187), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.rcvpruned */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RCVPRUNED],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,188), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.tcp.tcploss */
  { &_pm_proc_net_netstat.tcp[_PM_NETSTAT_TCPEXT_TCPLOSS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,189), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpfailtx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFAILTX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,190), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpfailrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFAILRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,191), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.subflowstale */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_SUBFLOWSTALE],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,192), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.subflowrecover */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_SUBFLOWRECOVER],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,193), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.addaddrdrops */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_ADDADDRDROPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,194), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.rmaddrdrops */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_RMADDRDROPS],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,195), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpfastclosetx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFASTCLOSETX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,196), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mpfastcloserx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPFASTCLOSERX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,197), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mprsttx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPRSTTX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,198), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* network.mptcp.mprstrx */
  { &_pm_proc_net_netstat.mptcp[_PM_NETSTAT_MPTCPEXT_MPRSTRX],
    { PMDA_PMID(CLUSTER_NET_NETSTAT,199), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* hinv.map.scsi */
    { NULL, 
      { PMDA_PMID(CLUSTER_SCSI,0), PM_TYPE_STRING, SCSI_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/zoneinfo cluster
 */

/* mem.zoneinfo.free */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,0), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.min */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,1), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.low */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,2), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.high */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,3), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.scanned */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,4), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.spanned */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,5), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.present */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,6), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.managed */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,7), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_free_pages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,8), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_alloc_batch */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,9), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_inactive_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,10), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_active_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,11), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_inactive_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,12), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_active_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,13), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_unevictable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,14), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_mlock */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,15), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_anon_pages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,16), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_mapped */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,17), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_file_pages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,18), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_dirty */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,19), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_writeback */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,20), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_slab_reclaimable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,21), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_slab_unreclaimable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,22), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_page_table_pages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,23), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_kernel_stack */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,24), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_unstable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,25), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_bounce */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,26), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_vmscan_write */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,27), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_vmscan_immediate_reclaim */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,28), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_writeback_temp */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,29), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_isolated_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,30), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_isolated_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,31), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_shmem */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,32), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_dirtied */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,33), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_written */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,34), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_hit */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,35), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_miss */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,36), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_foreign */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,37), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_interleave */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,38), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_local */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,39), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.numa_other */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,40), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_refault */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,41), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_activate */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,42), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_nodereclaim */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,43), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_anon_transparent_hugepages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,44), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_free_cma */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,45), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.cma */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,46), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_swapcached */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,47), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_shmem_hugepages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,48), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_shmem_pmdmapped */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,49), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_file_hugepages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,50), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_file_pmdmapped */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,51), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_kernel_misc_reclaimable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,52), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_foll_pin_acquired */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,53), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_foll_pin_released */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,54), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_refault_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,55), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_refault_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,56), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_active_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,57), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_active_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,58), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_restore_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,59), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.workingset_restore_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,60), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zspages */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,61), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_inactive_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,62), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_active_file */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,63), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_inactive_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,64), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_active_anon */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,65), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_unevictable */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,66), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.nr_zone_write_pending */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO,67), PM_TYPE_U64, ZONEINFO_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* mem.zoneinfo.protection */
  { NULL,
    { PMDA_PMID(CLUSTER_ZONEINFO_PROTECTION,0), PM_TYPE_U64, ZONEINFO_PROTECTION_INDOM, PM_SEM_INSTANT,
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/*
 * /proc/cpuinfo cluster (cpu indom)
 */

/* hinv.cpu.clock */
  { NULL,
    { PMDA_PMID(CLUSTER_CPUINFO, 0), PM_TYPE_FLOAT, CPU_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,-1,0,0,PM_TIME_USEC,0) } },

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
    PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) } },

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

/* hinv.cpu.thermal_throttle.core.count */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 2), KERNEL_ULONG, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* hinv.cpu.thermal_throttle.core.time */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 3), KERNEL_ULONG, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0)}},

/* hinv.cpu.thermal_throttle.package.count */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 4), KERNEL_ULONG, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* hinv.cpu.thermal_throttle.package.time */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 5), KERNEL_ULONG, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* hinv.cpu.frequency_scaling.count */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 6), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* hinv.cpu.frequency_scaling.time */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 7), PM_TYPE_U64, CPU_INDOM, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },

/* hinv.cpu.frequency_scaling.max */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 8), PM_TYPE_U32, CPU_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,-1,0,0,PM_TIME_USEC,0) } },

/* hinv.cpu.frequency_scaling.min */
  { NULL,
    { PMDA_PMID(CLUSTER_SYSFS_DEVICES, 9), PM_TYPE_U32, CPU_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,-1,0,0,PM_TIME_USEC,0) } },

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

/* ipc.sem.used_sem */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_INFO, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.sem.tot_sem */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_INFO, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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

/* ipc.msg.used_queues */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_INFO, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.tot_msg */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_INFO, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.msg.tot_bytes */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_INFO, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.tot */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.rss */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.swp */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0)}},

/* ipc.shm.used_ids */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.shm.swap_attempts */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* ipc.shm.swap_successes */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_INFO, 5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
 * shared memory stat cluster
 * Cluster added by wu liming <wulm.fnst@cn.fujitsu.com>
 */

/* ipc.shm.key */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,0), PM_TYPE_STRING, IPC_STAT_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.owner */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,1), PM_TYPE_STRING, IPC_STAT_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.perms */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,2), PM_TYPE_U32, IPC_STAT_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.segsz */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,3), PM_TYPE_U32, IPC_STAT_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* ipc.shm.nattch */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,4), PM_TYPE_U32, IPC_STAT_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.status */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,5), PM_TYPE_STRING, IPC_STAT_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.creator_pid */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,6), PM_TYPE_U32, IPC_STAT_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.shm.last_access_pid */
  { NULL,
    { PMDA_PMID(CLUSTER_SHM_STAT,7), PM_TYPE_U32, IPC_STAT_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * message queues stat cluster
 */

/* ipc.msg.key */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,0), PM_TYPE_STRING, IPC_MSG_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.msg.owner */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,1), PM_TYPE_STRING, IPC_MSG_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.msg.perms */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,2), PM_TYPE_U32, IPC_MSG_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.msg.msgsz */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,3), PM_TYPE_U32, IPC_MSG_INDOM, PM_SEM_DISCRETE,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* ipc.msg.messages */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,4), PM_TYPE_U32, IPC_MSG_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.msg.last_send_pid */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,5), PM_TYPE_U32, IPC_MSG_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

/* ipc.msg.last_recv_pid */
  { NULL,
    { PMDA_PMID(CLUSTER_MSG_STAT,6), PM_TYPE_U32, IPC_MSG_INDOM, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * semaphore arrays stat cluster
 */

/* ipc.sem.key */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_STAT,0), PM_TYPE_STRING, IPC_SEM_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.sem.owner */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_STAT,1), PM_TYPE_STRING, IPC_SEM_INDOM, PM_SEM_DISCRETE, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.sem.perms */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_STAT,2), PM_TYPE_U32, IPC_SEM_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* ipc.sem.nsems */
  { NULL,
    { PMDA_PMID(CLUSTER_SEM_STAT,3), PM_TYPE_U32, IPC_SEM_INDOM, PM_SEM_INSTANT, 
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * ksm info cluster
 */
/* mem.ksm.full_scans */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 0), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* mem.ksm.merge_across_nodes */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 1), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.pages_shared */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 2), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.pages_sharing */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 3), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.pages_to_scan */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 4), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.pages_unshared */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 5), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },

/* mem.ksm.pages_volatile */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 6), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.run_state */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 7), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0)}},

/* mem.ksm.sleep_time */
  { NULL,
    { PMDA_PMID(CLUSTER_KSM_INFO, 8), KERNEL_ULONG, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0)}},

/*
 * number of users cluster
 */

/* kernel.all.nusers */
  { NULL,
    { PMDA_PMID(CLUSTER_UTMP, 0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},
/* kernel.all.nroots */
  { NULL,
    { PMDA_PMID(CLUSTER_UTMP, 1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0)}},
/* kernel.all.nsessions */
  { NULL,
    { PMDA_PMID(CLUSTER_UTMP, 2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
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
    { &proc_sys_fs.fs_aio_count,
      { PMDA_PMID(CLUSTER_VFS,7), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_sys_fs.fs_aio_max,
      { PMDA_PMID(CLUSTER_VFS,8), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/locks vfs cluster
 */

/* vfs.locks */
    { &proc_locks.posix.read,
      { PMDA_PMID(CLUSTER_LOCKS,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.posix.write,
      { PMDA_PMID(CLUSTER_LOCKS,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.posix.count,
      { PMDA_PMID(CLUSTER_LOCKS,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.flock.read,
      { PMDA_PMID(CLUSTER_LOCKS,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.flock.write,
      { PMDA_PMID(CLUSTER_LOCKS,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.flock.count,
      { PMDA_PMID(CLUSTER_LOCKS,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.lease.read,
      { PMDA_PMID(CLUSTER_LOCKS,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.lease.write,
      { PMDA_PMID(CLUSTER_LOCKS,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { &proc_locks.lease.count,
      { PMDA_PMID(CLUSTER_LOCKS,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * /proc/sys/kernel cluster (random number state, pid_max, etc)
 */

    /* random.entropy_avail */
    { &proc_sys_kernel.entropy_avail,
      { PMDA_PMID(CLUSTER_SYS_KERNEL,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* random.poolsize */
    { &proc_sys_kernel.random_poolsize,
      { PMDA_PMID(CLUSTER_SYS_KERNEL,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* kernel.all.pid_max */
    { &proc_sys_kernel.pid_max,
      { PMDA_PMID(CLUSTER_SYS_KERNEL,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* kernel.all.nptys */
    { &proc_sys_kernel.pty_nr,
      { PMDA_PMID(CLUSTER_SYS_KERNEL,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
      PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /*
     * mem.vmstat cluster
     */

    /* mem.vmstat.nr_dirty */
    { &_pm_proc_vmstat.nr_dirty,
    {PMDA_PMID(CLUSTER_VMSTAT,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_writeback */
    { &_pm_proc_vmstat.nr_writeback,
    {PMDA_PMID(CLUSTER_VMSTAT,1), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_unstable */
    { &_pm_proc_vmstat.nr_unstable,
    {PMDA_PMID(CLUSTER_VMSTAT,2), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_page_table_pages */
    { &_pm_proc_vmstat.nr_page_table_pages,
    {PMDA_PMID(CLUSTER_VMSTAT,3), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_mapped */
    { &_pm_proc_vmstat.nr_mapped,
    {PMDA_PMID(CLUSTER_VMSTAT,4), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_slab */
    { &_pm_proc_vmstat.nr_slab,
    {PMDA_PMID(CLUSTER_VMSTAT,5), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.pgpgin */
    { &_pm_proc_vmstat.pgpgin,
    {PMDA_PMID(CLUSTER_VMSTAT,6), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgpgout */
    { &_pm_proc_vmstat.pgpgout,
    {PMDA_PMID(CLUSTER_VMSTAT,7), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpin */
    { &_pm_proc_vmstat.pswpin,
    {PMDA_PMID(CLUSTER_VMSTAT,8), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pswpout */
    { &_pm_proc_vmstat.pswpout,
    {PMDA_PMID(CLUSTER_VMSTAT,9), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_high */
    { &_pm_proc_vmstat.pgalloc_high,
    {PMDA_PMID(CLUSTER_VMSTAT,10), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_normal */
    { &_pm_proc_vmstat.pgalloc_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,11), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_dma */
    { &_pm_proc_vmstat.pgalloc_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfree */
    { &_pm_proc_vmstat.pgfree,
    {PMDA_PMID(CLUSTER_VMSTAT,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgactivate */
    { &_pm_proc_vmstat.pgactivate,
    {PMDA_PMID(CLUSTER_VMSTAT,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgdeactivate */
    { &_pm_proc_vmstat.pgdeactivate,
    {PMDA_PMID(CLUSTER_VMSTAT,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgfault */
    { &_pm_proc_vmstat.pgfault,
    {PMDA_PMID(CLUSTER_VMSTAT,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgmajfault */
    { &_pm_proc_vmstat.pgmajfault,
    {PMDA_PMID(CLUSTER_VMSTAT,17), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_high */
    { &_pm_proc_vmstat.pgrefill_high,
    {PMDA_PMID(CLUSTER_VMSTAT,18), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_normal */
    { &_pm_proc_vmstat.pgrefill_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,19), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_dma */
    { &_pm_proc_vmstat.pgrefill_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,20), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_high */
    { &_pm_proc_vmstat.pgsteal_high,
    {PMDA_PMID(CLUSTER_VMSTAT,21), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_normal */
    { &_pm_proc_vmstat.pgsteal_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,22), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_dma */
    { &_pm_proc_vmstat.pgsteal_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,23), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_high */
    { &_pm_proc_vmstat.pgscan_kswapd_high,
    {PMDA_PMID(CLUSTER_VMSTAT,24), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_normal */
    { &_pm_proc_vmstat.pgscan_kswapd_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,25), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_dma */
    { &_pm_proc_vmstat.pgscan_kswapd_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_high */
    { &_pm_proc_vmstat.pgscan_direct_high,
    {PMDA_PMID(CLUSTER_VMSTAT,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_normal */
    { &_pm_proc_vmstat.pgscan_direct_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_dma */
    { &_pm_proc_vmstat.pgscan_direct_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pginodesteal */
    { &_pm_proc_vmstat.pginodesteal,
    {PMDA_PMID(CLUSTER_VMSTAT,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.slabs_scanned */
    { &_pm_proc_vmstat.slabs_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_steal */
    { &_pm_proc_vmstat.kswapd_steal,
    {PMDA_PMID(CLUSTER_VMSTAT,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_inodesteal */
    { &_pm_proc_vmstat.kswapd_inodesteal,
    {PMDA_PMID(CLUSTER_VMSTAT,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pageoutrun */
    { &_pm_proc_vmstat.pageoutrun,
    {PMDA_PMID(CLUSTER_VMSTAT,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall */
    { &_pm_proc_vmstat.allocstall,
    {PMDA_PMID(CLUSTER_VMSTAT,35), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrotated */
    { &_pm_proc_vmstat.pgrotated,
    {PMDA_PMID(CLUSTER_VMSTAT,36), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_slab_reclaimable */
    { &_pm_proc_vmstat.nr_slab_reclaimable,
    {PMDA_PMID(CLUSTER_VMSTAT,37), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_slab_unreclaimable */
    { &_pm_proc_vmstat.nr_slab_unreclaimable,
    {PMDA_PMID(CLUSTER_VMSTAT,38), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_anon_pages */
    { &_pm_proc_vmstat.nr_anon_pages,
    {PMDA_PMID(CLUSTER_VMSTAT,39), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_bounce */
    { &_pm_proc_vmstat.nr_bounce,
    {PMDA_PMID(CLUSTER_VMSTAT,40), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_file_pages */
    { &_pm_proc_vmstat.nr_file_pages,
    {PMDA_PMID(CLUSTER_VMSTAT,41), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* mem.vmstat.nr_vmscan_write */
    { &_pm_proc_vmstat.nr_vmscan_write,
    {PMDA_PMID(CLUSTER_VMSTAT,42), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.htlb_buddy_alloc_fail */
    { &_pm_proc_vmstat.htlb_buddy_alloc_fail,
    {PMDA_PMID(CLUSTER_VMSTAT,43), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.htlb_buddy_alloc_success */
    { &_pm_proc_vmstat.htlb_buddy_alloc_success,
    {PMDA_PMID(CLUSTER_VMSTAT,44), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_active_anon */
    { &_pm_proc_vmstat.nr_active_anon,
    {PMDA_PMID(CLUSTER_VMSTAT,45), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_active_file */
    { &_pm_proc_vmstat.nr_active_file,
    {PMDA_PMID(CLUSTER_VMSTAT,46), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_free_pages */
    { &_pm_proc_vmstat.nr_free_pages,
    {PMDA_PMID(CLUSTER_VMSTAT,47), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_inactive_anon */
    { &_pm_proc_vmstat.nr_inactive_anon,
    {PMDA_PMID(CLUSTER_VMSTAT,48), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_inactive_file */
    { &_pm_proc_vmstat.nr_inactive_file,
    {PMDA_PMID(CLUSTER_VMSTAT,49), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_isolated_anon */
    { &_pm_proc_vmstat.nr_isolated_anon,
    {PMDA_PMID(CLUSTER_VMSTAT,50), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_isolated_file */
    { &_pm_proc_vmstat.nr_isolated_file,
    {PMDA_PMID(CLUSTER_VMSTAT,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_kernel_stack */
    { &_pm_proc_vmstat.nr_kernel_stack,
    {PMDA_PMID(CLUSTER_VMSTAT,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_mlock */
    { &_pm_proc_vmstat.nr_mlock,
    {PMDA_PMID(CLUSTER_VMSTAT,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_shmem */
    { &_pm_proc_vmstat.nr_shmem,
    {PMDA_PMID(CLUSTER_VMSTAT,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_unevictable */
    { &_pm_proc_vmstat.nr_unevictable,
    {PMDA_PMID(CLUSTER_VMSTAT,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_writeback_temp */
    { &_pm_proc_vmstat.nr_writeback_temp,
    {PMDA_PMID(CLUSTER_VMSTAT,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_blocks_moved */
    { &_pm_proc_vmstat.compact_blocks_moved,
    {PMDA_PMID(CLUSTER_VMSTAT,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_fail */
    { &_pm_proc_vmstat.compact_fail,
    {PMDA_PMID(CLUSTER_VMSTAT,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_pagemigrate_failed */
    { &_pm_proc_vmstat.compact_pagemigrate_failed,
    {PMDA_PMID(CLUSTER_VMSTAT,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_pages_moved */
    { &_pm_proc_vmstat.compact_pages_moved,
    {PMDA_PMID(CLUSTER_VMSTAT,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_stall */
    { &_pm_proc_vmstat.compact_stall,
    {PMDA_PMID(CLUSTER_VMSTAT,61), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_success */
    { &_pm_proc_vmstat.compact_success,
    {PMDA_PMID(CLUSTER_VMSTAT,62), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_dma32 */
    { &_pm_proc_vmstat.pgalloc_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,63), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgalloc_movable */
    { &_pm_proc_vmstat.pgalloc_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,64), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_dma32 */
    { &_pm_proc_vmstat.pgrefill_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,65), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgrefill_movable */
    { &_pm_proc_vmstat.pgrefill_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,66), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_dma32 */
    { &_pm_proc_vmstat.pgscan_direct_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,67), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_movable */
    { &_pm_proc_vmstat.pgscan_direct_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,68), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_dma32 */
    { &_pm_proc_vmstat.pgscan_kswapd_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,69), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_movable */
    { &_pm_proc_vmstat.pgscan_kswapd_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,70), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_dma32 */
    { &_pm_proc_vmstat.pgsteal_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_movable */
    { &_pm_proc_vmstat.pgsteal_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_fault_alloc */
    { &_pm_proc_vmstat.thp_fault_alloc,
    {PMDA_PMID(CLUSTER_VMSTAT,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_fault_fallback */
    { &_pm_proc_vmstat.thp_fault_fallback,
    {PMDA_PMID(CLUSTER_VMSTAT,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_collapse_alloc */
    { &_pm_proc_vmstat.thp_collapse_alloc,
    {PMDA_PMID(CLUSTER_VMSTAT,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_collapse_alloc_failed */
    { &_pm_proc_vmstat.thp_collapse_alloc_failed,
    {PMDA_PMID(CLUSTER_VMSTAT,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split */
    { &_pm_proc_vmstat.thp_split,
    {PMDA_PMID(CLUSTER_VMSTAT,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_cleared */
    { &_pm_proc_vmstat.unevictable_pgs_cleared,
    {PMDA_PMID(CLUSTER_VMSTAT,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_culled */
    { &_pm_proc_vmstat.unevictable_pgs_culled,
    {PMDA_PMID(CLUSTER_VMSTAT,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_mlocked */
    { &_pm_proc_vmstat.unevictable_pgs_mlocked,
    {PMDA_PMID(CLUSTER_VMSTAT,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_mlockfreed */
    { &_pm_proc_vmstat.unevictable_pgs_mlockfreed,
    {PMDA_PMID(CLUSTER_VMSTAT,81), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_munlocked */
    { &_pm_proc_vmstat.unevictable_pgs_munlocked,
    {PMDA_PMID(CLUSTER_VMSTAT,82), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_rescued */
    { &_pm_proc_vmstat.unevictable_pgs_rescued,
    {PMDA_PMID(CLUSTER_VMSTAT,83), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_scanned */
    { &_pm_proc_vmstat.unevictable_pgs_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,84), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.unevictable_pgs_stranded */
    { &_pm_proc_vmstat.unevictable_pgs_stranded,
    {PMDA_PMID(CLUSTER_VMSTAT,85), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.zone_reclaim_failed */
    { &_pm_proc_vmstat.zone_reclaim_failed,
    {PMDA_PMID(CLUSTER_VMSTAT,86), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_low_wmark_hit_quickly */
    { &_pm_proc_vmstat.kswapd_low_wmark_hit_quickly,
    {PMDA_PMID(CLUSTER_VMSTAT,87), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_high_wmark_hit_quickly */
    { &_pm_proc_vmstat.kswapd_high_wmark_hit_quickly,
    {PMDA_PMID(CLUSTER_VMSTAT,88), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.kswapd_skip_congestion_wait */
    { &_pm_proc_vmstat.kswapd_skip_congestion_wait,
    {PMDA_PMID(CLUSTER_VMSTAT,89), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_anon_transparent_hugepages */
    { &_pm_proc_vmstat.nr_anon_transparent_hugepages,
    {PMDA_PMID(CLUSTER_VMSTAT,90), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirtied */
    { &_pm_proc_vmstat.nr_dirtied,
    {PMDA_PMID(CLUSTER_VMSTAT,91), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirty_background_threshold */
    { &_pm_proc_vmstat.nr_dirty_background_threshold,
    {PMDA_PMID(CLUSTER_VMSTAT,92), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_dirty_threshold */
    { &_pm_proc_vmstat.nr_dirty_threshold,
    {PMDA_PMID(CLUSTER_VMSTAT,93), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_written */
    { &_pm_proc_vmstat.nr_written,
    {PMDA_PMID(CLUSTER_VMSTAT,94), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_foreign */
    { &_pm_proc_vmstat.numa_foreign,
    {PMDA_PMID(CLUSTER_VMSTAT,95), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_hit */
    { &_pm_proc_vmstat.numa_hit,
    {PMDA_PMID(CLUSTER_VMSTAT,96), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_interleave */
    { &_pm_proc_vmstat.numa_interleave,
    {PMDA_PMID(CLUSTER_VMSTAT,97), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_local */
    { &_pm_proc_vmstat.numa_local,
    {PMDA_PMID(CLUSTER_VMSTAT,98), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_miss */
    { &_pm_proc_vmstat.numa_miss,
    {PMDA_PMID(CLUSTER_VMSTAT,99), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_other */
    { &_pm_proc_vmstat.numa_other,
    {PMDA_PMID(CLUSTER_VMSTAT,100), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_zero_page_alloc */
    { &_pm_proc_vmstat.thp_zero_page_alloc,
    {PMDA_PMID(CLUSTER_VMSTAT,101), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_zero_page_alloc_failed */
    { &_pm_proc_vmstat.thp_zero_page_alloc_failed,
    {PMDA_PMID(CLUSTER_VMSTAT,102), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.balloon_inflate */
    { &_pm_proc_vmstat.balloon_inflate,
    {PMDA_PMID(CLUSTER_VMSTAT,103), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.balloon_deflate */
    { &_pm_proc_vmstat.balloon_deflate,
    {PMDA_PMID(CLUSTER_VMSTAT,104), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.balloon_migrate */
    { &_pm_proc_vmstat.balloon_migrate,
    {PMDA_PMID(CLUSTER_VMSTAT,105), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_daemon_wake */
    { &_pm_proc_vmstat.compact_daemon_wake,
    {PMDA_PMID(CLUSTER_VMSTAT,106), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_free_scanned */
    { &_pm_proc_vmstat.compact_free_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,107), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_migrate_scanned */
    { &_pm_proc_vmstat.compact_migrate_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,108), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.drop_pagecache */
    { &_pm_proc_vmstat.drop_pagecache,
    {PMDA_PMID(CLUSTER_VMSTAT,109), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.drop_slab */
    { &_pm_proc_vmstat.drop_slab,
    {PMDA_PMID(CLUSTER_VMSTAT,110), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_free_cma */
    { &_pm_proc_vmstat.nr_free_cma,
    {PMDA_PMID(CLUSTER_VMSTAT,111), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_pages_scanned */
    { &_pm_proc_vmstat.nr_pages_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,112), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_vmscan_immediate_reclaim */
    { &_pm_proc_vmstat.nr_vmscan_immediate_reclaim,
    {PMDA_PMID(CLUSTER_VMSTAT,113), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_pages_migrated */
    { &_pm_proc_vmstat.numa_pages_migrated,
    {PMDA_PMID(CLUSTER_VMSTAT,114), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_pte_updates */
    { &_pm_proc_vmstat.numa_pte_updates,
    {PMDA_PMID(CLUSTER_VMSTAT,115), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pglazyfreed */
    { &_pm_proc_vmstat.pglazyfreed,
    {PMDA_PMID(CLUSTER_VMSTAT,116), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgmigrate_fail */
    { &_pm_proc_vmstat.pgmigrate_fail,
    {PMDA_PMID(CLUSTER_VMSTAT,117), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgmigrate_success */
    { &_pm_proc_vmstat.pgmigrate_success,
    {PMDA_PMID(CLUSTER_VMSTAT,118), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_deferred_split_page */
    { &_pm_proc_vmstat.thp_deferred_split_page,
    {PMDA_PMID(CLUSTER_VMSTAT,119), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split_page */
    { &_pm_proc_vmstat.thp_split_page,
    {PMDA_PMID(CLUSTER_VMSTAT,120), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split_page_failed */
    { &_pm_proc_vmstat.thp_split_page_failed,
    {PMDA_PMID(CLUSTER_VMSTAT,121), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split_pmd */ 
    { &_pm_proc_vmstat.thp_split_pmd,
    {PMDA_PMID(CLUSTER_VMSTAT,122), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.workingset_activate */
    { &_pm_proc_vmstat.workingset_activate,
    {PMDA_PMID(CLUSTER_VMSTAT,123), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.workingset_nodereclaim */
    { &_pm_proc_vmstat.workingset_nodereclaim,
    {PMDA_PMID(CLUSTER_VMSTAT,124), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.workingset_refault */
    { &_pm_proc_vmstat.workingset_refault,
    {PMDA_PMID(CLUSTER_VMSTAT,125), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_isolated */
    { &_pm_proc_vmstat.compact_isolated,
    {PMDA_PMID(CLUSTER_VMSTAT,126), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_shmem_hugepages */
    { &_pm_proc_vmstat.nr_shmem_hugepages,
    {PMDA_PMID(CLUSTER_VMSTAT,127), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_shmem_pmdmapped */
    { &_pm_proc_vmstat.nr_shmem_pmdmapped,
    {PMDA_PMID(CLUSTER_VMSTAT,128), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_inactive_anon */
    { &_pm_proc_vmstat.nr_zone_inactive_anon,
    {PMDA_PMID(CLUSTER_VMSTAT,129), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_active_anon */
    { &_pm_proc_vmstat.nr_zone_active_anon,
    {PMDA_PMID(CLUSTER_VMSTAT,130), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_inactive_file */
    { &_pm_proc_vmstat.nr_zone_inactive_file,
    {PMDA_PMID(CLUSTER_VMSTAT,131), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_active_file */
    { &_pm_proc_vmstat.nr_zone_active_file,
    {PMDA_PMID(CLUSTER_VMSTAT,132), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_unevictable */
    { &_pm_proc_vmstat.nr_zone_unevictable,
    {PMDA_PMID(CLUSTER_VMSTAT,133), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zone_write_pending */
    { &_pm_proc_vmstat.nr_zone_write_pending,
    {PMDA_PMID(CLUSTER_VMSTAT,134), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_zspages */
    { &_pm_proc_vmstat.nr_zspages,
    {PMDA_PMID(CLUSTER_VMSTAT,135), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_file_alloc */
    { &_pm_proc_vmstat.thp_file_alloc,
    {PMDA_PMID(CLUSTER_VMSTAT,136), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_file_mapped */
    { &_pm_proc_vmstat.thp_file_mapped,
    {PMDA_PMID(CLUSTER_VMSTAT,137), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_kswapd_dma */
    { &_pm_proc_vmstat.pgsteal_kswapd_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,138), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_kswapd_dma32 */
    { &_pm_proc_vmstat.pgsteal_kswapd_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,139), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_kswapd_normal */
    { &_pm_proc_vmstat.pgsteal_kswapd_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,140), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_kswapd_movable */
    { &_pm_proc_vmstat.pgsteal_kswapd_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,141), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_direct_dma */
    { &_pm_proc_vmstat.pgsteal_direct_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,142), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_direct_dma32 */
    { &_pm_proc_vmstat.pgsteal_direct_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,143), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_direct_normal */
    { &_pm_proc_vmstat.pgsteal_direct_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,144), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_direct_movable */
    { &_pm_proc_vmstat.pgsteal_direct_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,145), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct */
    { &_pm_proc_vmstat.pgscan_direct,
    {PMDA_PMID(CLUSTER_VMSTAT,146), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_throttle */
    { &_pm_proc_vmstat.pgscan_direct_throttle,
    {PMDA_PMID(CLUSTER_VMSTAT,147), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd */
    { &_pm_proc_vmstat.pgscan_kswapd,
    {PMDA_PMID(CLUSTER_VMSTAT,148), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_direct */
    { &_pm_proc_vmstat.pgsteal_direct,
    {PMDA_PMID(CLUSTER_VMSTAT,149), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_kswapd */
    { &_pm_proc_vmstat.pgsteal_kswapd,
    {PMDA_PMID(CLUSTER_VMSTAT,150), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_huge_pte_updates */
    { &_pm_proc_vmstat.numa_huge_pte_updates,
    {PMDA_PMID(CLUSTER_VMSTAT,151), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_hint_faults */
    { &_pm_proc_vmstat.numa_hint_faults,
    {PMDA_PMID(CLUSTER_VMSTAT,152), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.numa_hint_faults_local */
    { &_pm_proc_vmstat.numa_hint_faults_local,
    {PMDA_PMID(CLUSTER_VMSTAT,153), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall_dma */
    { &_pm_proc_vmstat.allocstall_dma,
    {PMDA_PMID(CLUSTER_VMSTAT,154), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall_dma32 */
    { &_pm_proc_vmstat.allocstall_dma32,
    {PMDA_PMID(CLUSTER_VMSTAT,155), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall_high */
    { &_pm_proc_vmstat.allocstall_high,
    {PMDA_PMID(CLUSTER_VMSTAT,156), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall_movable */
    { &_pm_proc_vmstat.allocstall_movable,
    {PMDA_PMID(CLUSTER_VMSTAT,157), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.allocstall_normal */
    { &_pm_proc_vmstat.allocstall_normal,
    {PMDA_PMID(CLUSTER_VMSTAT,158), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_daemon_free_scanned */
    { &_pm_proc_vmstat.compact_daemon_free_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,159), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.compact_daemon_migrate_scanned */
    { &_pm_proc_vmstat.compact_daemon_migrate_scanned,
    {PMDA_PMID(CLUSTER_VMSTAT,160), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_file_hugepages */
    { &_pm_proc_vmstat.nr_file_hugepages,
    {PMDA_PMID(CLUSTER_VMSTAT,161), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_file_pmdmapped */
    { &_pm_proc_vmstat.nr_file_pmdmapped,
    {PMDA_PMID(CLUSTER_VMSTAT,162), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_foll_pin_acquired */
    { &_pm_proc_vmstat.nr_foll_pin_acquired,
    {PMDA_PMID(CLUSTER_VMSTAT,163), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_foll_pin_released */
    { &_pm_proc_vmstat.nr_foll_pin_released,
    {PMDA_PMID(CLUSTER_VMSTAT,164), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.nr_kernel_misc_reclaimable */
    { &_pm_proc_vmstat.nr_kernel_misc_reclaimable,
    {PMDA_PMID(CLUSTER_VMSTAT,165), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.oom_kill */
    { &_pm_proc_vmstat.oom_kill,
    {PMDA_PMID(CLUSTER_VMSTAT,166), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_file */
    { &_pm_proc_vmstat.pgsteal_file,
    {PMDA_PMID(CLUSTER_VMSTAT,167), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.swap_ra */
    { &_pm_proc_vmstat.swap_ra,
    {PMDA_PMID(CLUSTER_VMSTAT,168), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.swap_ra_hit */
    { &_pm_proc_vmstat.swap_ra_hit,
    {PMDA_PMID(CLUSTER_VMSTAT,169), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_fault_fallback_charge */
    { &_pm_proc_vmstat.thp_fault_fallback_charge ,
    {PMDA_PMID(CLUSTER_VMSTAT,170), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_split_pud */
    { &_pm_proc_vmstat.thp_split_pud,
    {PMDA_PMID(CLUSTER_VMSTAT,171), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_swpout */
    { &_pm_proc_vmstat.thp_swpout,
    {PMDA_PMID(CLUSTER_VMSTAT,172), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.thp_swpout_fallback */
    { &_pm_proc_vmstat.thp_swpout_fallback,
    {PMDA_PMID(CLUSTER_VMSTAT,173), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.workingset_nodes */
    { &_pm_proc_vmstat.workingset_nodes,
    {PMDA_PMID(CLUSTER_VMSTAT,174), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.workingset_restore */
    { &_pm_proc_vmstat.workingset_restore,
    {PMDA_PMID(CLUSTER_VMSTAT,175), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_direct_total */
    { &_pm_proc_vmstat.pgscan_direct_total,
    {PMDA_PMID(CLUSTER_VMSTAT,176), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgscan_kswapd_total */
    { &_pm_proc_vmstat.pgscan_kswapd_total,
    {PMDA_PMID(CLUSTER_VMSTAT,177), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* mem.vmstat.pgsteal_total */
    { &_pm_proc_vmstat.pgsteal_total,
    {PMDA_PMID(CLUSTER_VMSTAT,178), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
    PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * sysfs_kernel cluster
 */
    /* sysfs.kernel.uevent_seqnum */
    { &sysfs_kernel.uevent_seqnum,
      { PMDA_PMID(CLUSTER_SYSFS_KERNEL,0), PM_TYPE_U64, PM_INDOM_NULL,
	PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * /proc/interrupts clusters
 */
    /* kernel.all.interrupts.total */
    { NULL, { PMDA_PMID(CLUSTER_INTERRUPTS, 0), PM_TYPE_U64,
    INTERRUPT_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.interrupts */
    { NULL, { PMDA_PMID(CLUSTER_INTERRUPTS, 1), PM_TYPE_U32,
    INTERRUPT_CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.all.interrupts.missed */
    { &irq_mis_count, { PMDA_PMID(CLUSTER_INTERRUPTS, 2), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.all.interrupts.errors */
    { &irq_err_count, { PMDA_PMID(CLUSTER_INTERRUPTS, 3), PM_TYPE_U32,
    PM_INDOM_NULL, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.intr */
    { NULL, { PMDA_PMID(CLUSTER_INTERRUPTS, 4), PM_TYPE_U64,
    CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * /proc/softirqs clusters
 */

    /* kernel.all.softirqs.total */
    { NULL, { PMDA_PMID(CLUSTER_SOFTIRQS_TOTAL, 0), PM_TYPE_U64,
    SOFTIRQ_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.softirqs */
    { NULL, { PMDA_PMID(CLUSTER_SOFTIRQS, 0), PM_TYPE_U32,
    SOFTIRQ_CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* kernel.percpu.sirq */
    { NULL, { PMDA_PMID(CLUSTER_SOFTIRQS, 1), PM_TYPE_U64,
    CPU_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * disk.dm cluster
 */
    /* disk.dm.read */
    { NULL, { PMDA_PMID(CLUSTER_DM,0), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER,  PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.write */
    { NULL, { PMDA_PMID(CLUSTER_DM,1), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.total */
    { NULL, { PMDA_PMID(CLUSTER_DM,2), PM_TYPE_U64, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blkread */
    { NULL, { PMDA_PMID(CLUSTER_DM,3), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blkwrite */
    { NULL, { PMDA_PMID(CLUSTER_DM,4), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blktotal */
    { NULL, { PMDA_PMID(CLUSTER_DM,5), PM_TYPE_U64, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.read_bytes */
    { NULL, { PMDA_PMID(CLUSTER_DM,6), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.write_bytes */
    { NULL, { PMDA_PMID(CLUSTER_DM,7), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.total_bytes */
    { NULL, { PMDA_PMID(CLUSTER_DM,8), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.read_merge */
    { NULL, { PMDA_PMID(CLUSTER_DM,9), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.write_merge */
    { NULL, { PMDA_PMID(CLUSTER_DM,10), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.avactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,11), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.aveq */
    { NULL, { PMDA_PMID(CLUSTER_DM,12), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* hinv.map.dmname */
    { NULL, { PMDA_PMID(CLUSTER_DM,13), PM_TYPE_STRING, DM_INDOM,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* disk.dm.read_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,14), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.write_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,15), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.write_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,16), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.capacity */
    { NULL, { PMDA_PMID(CLUSTER_DM,17), PM_TYPE_U64, DM_INDOM,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.discard */
    { NULL, { PMDA_PMID(CLUSTER_DM,18), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.blkdiscard */
    { NULL, { PMDA_PMID(CLUSTER_DM,19), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.discard_bytes */
    { NULL, { PMDA_PMID(CLUSTER_DM,20), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.dm.discard_merge */
    { NULL, { PMDA_PMID(CLUSTER_DM,21), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.discard_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,22), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.dm.flush */
    { NULL, { PMDA_PMID(CLUSTER_DM,23), KERNEL_ULONG, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.dm.flush_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_DM,24), PM_TYPE_U32, DM_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/*
 * disk.md cluster
 */
    /* disk.md.read */
    { NULL, { PMDA_PMID(CLUSTER_MD,0), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER,  PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.write */
    { NULL, { PMDA_PMID(CLUSTER_MD,1), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.total */
    { NULL, { PMDA_PMID(CLUSTER_MD,2), PM_TYPE_U64, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.blkread */
    { NULL, { PMDA_PMID(CLUSTER_MD,3), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.blkwrite */
    { NULL, { PMDA_PMID(CLUSTER_MD,4), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.blktotal */
    { NULL, { PMDA_PMID(CLUSTER_MD,5), PM_TYPE_U64, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.read_bytes */
    { NULL, { PMDA_PMID(CLUSTER_MD,6), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.md.write_bytes */
    { NULL, { PMDA_PMID(CLUSTER_MD,7), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.md.total_bytes */
    { NULL, { PMDA_PMID(CLUSTER_MD,8), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.md.read_merge */
    { NULL, { PMDA_PMID(CLUSTER_MD,9), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.write_merge */
    { NULL, { PMDA_PMID(CLUSTER_MD,10), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.avactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,11), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.md.aveq */
    { NULL, { PMDA_PMID(CLUSTER_MD,12), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* hinv.map.mdname */
    { NULL, { PMDA_PMID(CLUSTER_MD,13), PM_TYPE_STRING, MD_INDOM,
      PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* disk.md.read_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,14), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.md.write_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,15), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.md.write_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,16), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.md.status */
    { NULL, { PMDA_PMID(CLUSTER_MDADM,0), PM_TYPE_32, MD_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* disk.md.capacity */
    { NULL, { PMDA_PMID(CLUSTER_MD,17), PM_TYPE_U64, MD_INDOM,
      PM_SEM_DISCRETE, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.md.discard */
    { NULL, { PMDA_PMID(CLUSTER_MD,18), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.blkdiscard */
    { NULL, { PMDA_PMID(CLUSTER_MD,19), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.discard_bytes */
    { NULL, { PMDA_PMID(CLUSTER_MD,20), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

    /* disk.md.discard_merge */
    { NULL, { PMDA_PMID(CLUSTER_MD,21), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.discard_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,22), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

    /* disk.md.flush */
    { NULL, { PMDA_PMID(CLUSTER_MD,23), KERNEL_ULONG, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* disk.md.flush_rawactive */
    { NULL, { PMDA_PMID(CLUSTER_MD,24), PM_TYPE_U32, MD_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/*
 * network.softnet cluster
 */
    /* network.softnet.processed */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,0), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.dropped */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,1), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.time_squeeze */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,2), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.cpu_collision */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,3), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.received_rps */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,4), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.flow_limit_count */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,5), PM_TYPE_U64, PM_INDOM_NULL,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.processed */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,6), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.dropped */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,7), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.time_squeeze */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,8), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.cpu_collision */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,9), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.received_rps */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,10), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* network.softnet.percpu.flow_limit_count */
    { NULL, { PMDA_PMID(CLUSTER_NET_SOFTNET,11), PM_TYPE_U64, CPU_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/*
 * tapedev cluster
 */
    /* tape.dev.in_flight */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_IN_FLIGHT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_INSTANT, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* tape.dev.io_ns */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_IO_NS), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },

    /* tape.dev.other_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_OTHER_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* tape.dev.read_byte_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_READ_BYTE_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* tape.dev.read_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_READ_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* tape.dev.read_ns */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_READ_NS), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },

    /* tape.dev.resid_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_RESID_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* tape.dev.write_byte_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_WRITE_BYTE_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* tape.dev.write_cnt */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_WRITE_CNT), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* tape.dev.write_ns */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_WRITE_NS), PM_TYPE_U64, TAPEDEV_INDOM,
      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },

    /* hinv.ntape */
    { NULL, { PMDA_PMID(CLUSTER_TAPEDEV, TAPESTATS_HINV_NTAPE), PM_TYPE_U32, PM_INDOM_NULL,
	PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/*
 * tty cluster
 */
    /* tty.serial.rx */
    { NULL, {PMDA_PMID(CLUSTER_TTY, TTY_TX), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.tx */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_RX), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.frame */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_FRAME), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.parity */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_PARITY), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.brk */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_BRK), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.overrun */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_OVERRUN), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_COUNTER, PMDA_PMUNITS(0,0,0,0,0,0)} },
    /* tty.serial.irq */
    { NULL, {PMDA_PMID(CLUSTER_TTY,TTY_IRQ), PM_TYPE_U32, TTY_INDOM,
	     PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0)} },

/*
 * /proc/pressure/{cpu,memory,io} clusters
 */
    /* kernel.all.pressure.cpu.some.avg */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_CPU,0), PM_TYPE_FLOAT,
	      PRESSUREAVG_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* kernel.all.pressure.cpu.some.total */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_CPU,1), PM_TYPE_U64, PM_INDOM_NULL,
	      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0)}},
    /* kernel.all.pressure.memory.some.avg */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_MEM,0), PM_TYPE_FLOAT,
	      PRESSUREAVG_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* kernel.all.pressure.memory.some.total */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_MEM,1), PM_TYPE_U64, PM_INDOM_NULL,
	      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0)}},
    /* kernel.all.pressure.memory.full.avg */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_MEM,2), PM_TYPE_FLOAT,
	      PRESSUREAVG_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* kernel.all.pressure.memory.full.total */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_MEM,3), PM_TYPE_U64, PM_INDOM_NULL,
	      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0)}},
    /* kernel.all.pressure.io.some.avg */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_IO,0), PM_TYPE_FLOAT,
	      PRESSUREAVG_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* kernel.all.pressure.io.some.total */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_IO,1), PM_TYPE_U64, PM_INDOM_NULL,
	      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0)}},
    /* kernel.all.pressure.io.full.avg */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_IO,2), PM_TYPE_FLOAT,
	      PRESSUREAVG_INDOM, PM_SEM_INSTANT, PMDA_PMUNITS(0,0,0,0,0,0) } },
    /* kernel.all.pressure.io.full.total */
    { NULL, { PMDA_PMID(CLUSTER_PRESSURE_IO,3), PM_TYPE_U64, PM_INDOM_NULL,
	      PM_SEM_COUNTER, PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0)}},

/*
 * Fibre Channel host metrics cluster
 */
    /* fchost.in.frames */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_IN_FRAMES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* fchost.out.frames */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_OUT_FRAMES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* fchost.in.bytes */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_IN_BYTES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* fchost.out.bytes */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_OUT_BYTES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* fchost.lip_count */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_LIP_COUNT), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* fchost.nos_count */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_NOS_COUNT), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* fchost.error_frames */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_ERROR_FRAMES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* fchost.dumped_frames */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_DUMPED_FRAMES), PM_TYPE_U64,
	FCHOST_INDOM, PM_SEM_COUNTER, PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* hinv.nfchost */
    { NULL, { PMDA_PMID(CLUSTER_FCHOST, FCHOST_HINV_NFCHOST), PM_TYPE_U32,
	PM_INDOM_NULL, PM_SEM_DISCRETE, PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* disk.wwid.scsi_paths */
    { NULL,
      { PMDA_PMID(CLUSTER_WWID,3), PM_TYPE_STRING, WWID_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* disk.wwid.read */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,4), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.write */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,5), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.total */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,28), PM_TYPE_U64, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.blkread */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,6), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.blkwrite */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,7), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.blktotal */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,36), PM_TYPE_U64, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.read_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,38), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.wwid.write_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,39), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.wwid.total_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,40), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },


/* disk.wwid.avactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,46), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.aveq */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,47), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.read_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,49), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.write_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,50), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.scheduler */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,59), PM_TYPE_STRING, WWID_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },

/* hinv.map.scsi_id */
    { NULL,
      { PMDA_PMID(CLUSTER_WWID,103), PM_TYPE_STRING, WWID_INDOM, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,0,0,0,0,0) }, },


/* disk.wwid.read_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,72), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.write_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,73), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.total_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,79), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.capacity */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,87), PM_TYPE_U64, WWID_INDOM, PM_SEM_DISCRETE, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.wwid.discard */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,88), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.blkdiscard */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,89), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.discard_bytes */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,90), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },

/* disk.wwid.discard_merge */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,91), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.discard_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,92), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

/* disk.wwid.flush */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,93), KERNEL_ULONG, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

/* disk.wwid.flush_rawactive */
    { NULL, 
      { PMDA_PMID(CLUSTER_WWID,94), PM_TYPE_U32, WWID_INDOM, PM_SEM_COUNTER, 
      PMDA_PMUNITS(0,1,0,0,PM_TIME_MSEC,0) }, },

};

typedef struct {
    int			uid_flag;	/* uid attribute received */
    int			uid;		/* uid via PMDA_ATTR_USERID */
} linux_access_t;

typedef struct {
    linux_container_t	container;
    linux_access_t	access;
} perctx_t;

static perctx_t *ctxtab;
static int      num_ctx;

int linux_test_mode;	/* bit field indicating if currently doing QA tests */
char *linux_statspath = "";	/* optional path prefix for all stats files */
char *linux_mdadm = "/sbin/mdadm"; /* program for extracting MD RAID status */

FILE *
linux_statsfile(const char *path, char *buffer, int size)
{
    pmsprintf(buffer, size, "%s%s", linux_statspath, path);
    return fopen(buffer, "r");
}

static linux_access_t *
access_ctx(int ctx)
{
    if (ctx < num_ctx && ctx >= 0)
	return &ctxtab[ctx].access;
    return NULL;
}

static linux_container_t *
linux_ctx_container(int ctx)
{
    if (ctx < num_ctx && ctx >= 0 && ctxtab[ctx].container.name)
	return &ctxtab[ctx].container;
    return NULL;
}

static int
linux_refresh(pmdaExt *pmda, int *need_refresh, int context)
{
    linux_container_t *cp = linux_ctx_container(context);
    linux_access_t *laccess = access_ctx(context);
    int need_net_ioctl = 0;
    int ns_fds = 0;
    int sts = 0;
    int	lsts;

    if (cp && (sts = container_lookup(rootfd, cp)) < 0)
	return sts;

    if (need_refresh[CLUSTER_PARTITIONS] ||
	need_refresh[CLUSTER_WWID] ||
	need_refresh[CLUSTER_ZRAM_DEVICES] ||
	need_refresh[REFRESH_PROC_DISKSTATS] ||
	need_refresh[REFRESH_PROC_PARTITIONS]) {
    	lsts = refresh_proc_partitions(INDOM(DISK_INDOM),
			INDOM(PARTITIONS_INDOM), INDOM(ZRAM_INDOM),
			INDOM(DM_INDOM), INDOM(MD_INDOM), INDOM(WWID_INDOM),
			need_refresh[REFRESH_PROC_DISKSTATS],
			need_refresh[REFRESH_PROC_PARTITIONS]);
	if (lsts < 0 && sts == 0)
	    sts = lsts;
    }

    if (need_refresh[CLUSTER_STAT])
	refresh_proc_stat(&proc_stat);

    if (need_refresh[CLUSTER_CPUINFO])
	refresh_proc_cpuinfo();

    if (need_refresh[CLUSTER_MEMINFO])
	refresh_proc_meminfo(&proc_meminfo);

    if (need_refresh[CLUSTER_NUMA_MEMINFO])
	refresh_numa_meminfo();

    if (need_refresh[CLUSTER_LOADAVG])
	refresh_proc_loadavg(&proc_loadavg);

    if (need_refresh[CLUSTER_NET_NFS]) {
	refresh_proc_net_rpc(&proc_net_rpc);
	refresh_proc_fs_nfsd(&proc_fs_nfsd);
    }

    /*
     * Network interface metrics and namespaces are complicated by a
     * need to be in the right namespace at the right time (for /sys
     * -> MNTNS, for /proc or ioctl -> NETNS) - and the two have been
     * found to be mutually exclusive from the point of view of access
     * via the setns(2) syscall.  We also have a further complicating
     * factor where some values are available *either* by sysfs (newer
     * kernels), *or* ioctl (older kernels).
     */
    if (need_refresh[CLUSTER_NET_DEV] ||
	need_refresh[CLUSTER_NET_ADDR] ||
	need_refresh[CLUSTER_NET_SOCKSTAT] ||
	need_refresh[CLUSTER_NET_SOCKSTAT6] ||
	need_refresh[CLUSTER_NET_SNMP] ||
	need_refresh[CLUSTER_NET_SNMP6] ||
	need_refresh[CLUSTER_NET_RAW] ||
	need_refresh[CLUSTER_NET_RAW6] ||
	need_refresh[CLUSTER_NET_TCP] ||
	need_refresh[CLUSTER_NET_TCP6] ||
	need_refresh[CLUSTER_NET_UDP] ||
	need_refresh[CLUSTER_NET_UDP6] ||
	need_refresh[CLUSTER_NET_UNIX] ||
	need_refresh[CLUSTER_NET_NETSTAT] ||
	need_refresh[CLUSTER_FILESYS] ||
	need_refresh[CLUSTER_TMPFS] ||
	need_refresh[REFRESH_NET_MTU] ||
	need_refresh[REFRESH_NET_TYPE] ||
	need_refresh[REFRESH_NET_SPEED] ||
	need_refresh[REFRESH_NET_DUPLEX] ||
	need_refresh[REFRESH_NET_LINKUP] ||
	need_refresh[REFRESH_NET_RUNNING] ||
	need_refresh[REFRESH_NET_WIRELESS] ||
	need_refresh[REFRESH_NETADDR_INET] ||
	need_refresh[REFRESH_NETADDR_IPV6] ||
	need_refresh[REFRESH_NETADDR_HW]) {
	pmInDom netaddr = INDOM(NET_ADDR_INDOM);
	pmInDom netdev = INDOM(NET_DEV_INDOM);

	if (need_refresh[CLUSTER_NET_ADDR])
	    clear_net_addr_indom(netaddr);
	if (need_refresh[REFRESH_NETADDR_INET])
	    need_net_ioctl = 1;
	if (need_refresh[REFRESH_NETADDR_IPV6])
	    need_net_ioctl = 1;

	if (need_refresh[CLUSTER_NET_DEV] ||
	    need_refresh[CLUSTER_NET_SOCKSTAT] ||
	    need_refresh[CLUSTER_NET_SOCKSTAT6] ||
	    need_refresh[CLUSTER_NET_SNMP] ||
	    need_refresh[CLUSTER_NET_SNMP6] ||
	    need_refresh[CLUSTER_NET_RAW] ||
	    need_refresh[CLUSTER_NET_RAW6] ||
	    need_refresh[CLUSTER_NET_TCP] ||
	    need_refresh[CLUSTER_NET_TCP6] ||
	    need_refresh[CLUSTER_NET_UDP] ||
	    need_refresh[CLUSTER_NET_UDP6] ||
	    need_refresh[CLUSTER_NET_UNIX] ||
	    need_refresh[CLUSTER_NET_NETSTAT]) {

	    if ((lsts = container_nsenter(cp, LINUX_NAMESPACE_NET, &ns_fds)) < 0) {
		if (lsts < 0 && sts == 0)
		    sts = lsts;
		goto done;
	    }

	    if (need_refresh[CLUSTER_NET_DEV])
		refresh_proc_net_dev(netdev, cp);

	    if (need_refresh[CLUSTER_NET_DEV])
		refresh_proc_net_all(netdev, &proc_net_all);

	    if (need_refresh[CLUSTER_NET_SOCKSTAT])
		refresh_proc_net_sockstat(&proc_net_sockstat);

	    if (need_refresh[CLUSTER_NET_SOCKSTAT6])
		refresh_proc_net_sockstat6(&proc_net_sockstat6);

	    if (need_refresh[CLUSTER_NET_SNMP])
		refresh_proc_net_snmp(&_pm_proc_net_snmp);

	    if (need_refresh[CLUSTER_NET_SNMP6])
		refresh_proc_net_snmp6(_pm_proc_net_snmp6);

	    if (need_refresh[CLUSTER_NET_RAW])
		refresh_proc_net_raw(&proc_net_raw);

	    if (need_refresh[CLUSTER_NET_RAW6])
		refresh_proc_net_raw6(&proc_net_raw6);

	    if (need_refresh[CLUSTER_NET_TCP])
		refresh_proc_net_tcp(&proc_net_tcp);

	    if (need_refresh[CLUSTER_NET_TCP6])
		refresh_proc_net_tcp6(&proc_net_tcp6);

	    if (need_refresh[CLUSTER_NET_UDP])
		refresh_proc_net_udp(&proc_net_udp);

	    if (need_refresh[CLUSTER_NET_UDP6])
		refresh_proc_net_udp6(&proc_net_udp6);

	    if (need_refresh[CLUSTER_NET_UNIX])
		refresh_proc_net_unix(&proc_net_unix);

	    if (need_refresh[CLUSTER_NET_NETSTAT]) {
		lsts = refresh_proc_net_netstat(&_pm_proc_net_netstat);
		if (lsts < 0 && sts == 0)
		    sts = lsts;
	    }

	    container_nsleave(cp, LINUX_NAMESPACE_NET);
	}

	if (need_refresh[CLUSTER_NET_DEV] ||
	    need_refresh[CLUSTER_FILESYS] ||
	    need_refresh[CLUSTER_TMPFS] ||
	    need_refresh[REFRESH_NET_MTU] ||
	    need_refresh[REFRESH_NET_TYPE] ||
	    need_refresh[REFRESH_NET_SPEED] ||
	    need_refresh[REFRESH_NET_DUPLEX] ||
	    need_refresh[REFRESH_NET_LINKUP] ||
	    need_refresh[REFRESH_NET_RUNNING] ||
	    need_refresh[REFRESH_NET_WIRELESS] ||
	    need_refresh[REFRESH_NETADDR_INET] ||
	    need_refresh[REFRESH_NETADDR_IPV6] ||
	    need_refresh[REFRESH_NETADDR_HW]) {

	    if ((lsts = container_nsenter(cp, LINUX_NAMESPACE_MNT, &ns_fds)) < 0) {
		if (lsts < 0 && sts == 0)
		    sts = lsts;
		goto done;
	    }

	    refresh_net_addr_sysfs(netaddr, need_refresh);
	    need_net_ioctl |= refresh_net_sysfs(netdev, need_refresh);
	    if (need_refresh[CLUSTER_FILESYS] || need_refresh[CLUSTER_TMPFS])
		refresh_filesys(INDOM(FILESYS_INDOM), INDOM(TMPFS_INDOM), cp);

	    container_nsleave(cp, LINUX_NAMESPACE_MNT);
	}

	if (need_net_ioctl) {
	    if ((lsts = container_nsenter(cp, LINUX_NAMESPACE_NET, &ns_fds)) < 0) {
		if (lsts < 0 && sts == 0)
		    sts = lsts;
		goto done;
	    }
	    refresh_net_addr_ioctl(netaddr, cp, need_refresh);
	    refresh_net_ioctl(netdev, cp, need_refresh);
	    container_nsleave(cp, LINUX_NAMESPACE_NET);
	}

	if (need_refresh[CLUSTER_NET_ADDR])
	    store_net_addr_indom(netaddr, cp);
    }

    if (need_refresh[CLUSTER_KERNEL_UNAME]) {
	if ((lsts = container_nsenter(cp, LINUX_NAMESPACE_UTS, &ns_fds)) < 0) {
	    if (lsts < 0 && sts == 0)
		sts = lsts;
	    goto done;
	}
	uname(&kernel_uname);
	container_nsleave(cp, LINUX_NAMESPACE_UTS);
    }

    if (need_refresh[CLUSTER_INTERRUPTS])
	refresh_proc_interrupts();

    if (need_refresh[CLUSTER_SOFTIRQS] ||
	need_refresh[CLUSTER_SOFTIRQS_TOTAL])
	refresh_proc_softirqs();

    if (need_refresh[CLUSTER_SWAPDEV])
	refresh_swapdev(INDOM(SWAPDEV_INDOM));

    if (need_refresh[CLUSTER_SCSI])
	refresh_proc_scsi(INDOM(SCSI_INDOM));

    if (need_refresh[CLUSTER_SLAB]) {
	if (all_access ||
	    (laccess != NULL && laccess->uid == 0 && laccess->uid_flag)) {
	    proc_slabinfo.permission = 1;
	    refresh_proc_slabinfo(INDOM(SLAB_INDOM), &proc_slabinfo);
	} else {
	    proc_slabinfo.permission = 0;
	}
    }

    if (need_refresh[CLUSTER_SEM_LIMITS])
	refresh_sem_limits(&sem_limits);

    if (need_refresh[CLUSTER_MSG_LIMITS])
        refresh_msg_limits(&msg_limits);

    if (need_refresh[CLUSTER_SHM_INFO])
        refresh_shm_info(&shm_info);

    if (need_refresh[CLUSTER_SEM_INFO])
        refresh_sem_info(&sem_info);

    if (need_refresh[CLUSTER_MSG_INFO])
        refresh_msg_info(&msg_info);

    if (need_refresh[CLUSTER_SHM_LIMITS])
        refresh_shm_limits(&shm_limits);

    if (need_refresh[CLUSTER_UPTIME])
        refresh_proc_uptime(&proc_uptime);

    if (need_refresh[CLUSTER_UTMP])
        refresh_login_info(&login_info);

    if (need_refresh[CLUSTER_VFS])
    	refresh_proc_sys_fs(&proc_sys_fs);

    if (need_refresh[CLUSTER_LOCKS])
    	refresh_proc_locks(&proc_locks);

    if (need_refresh[CLUSTER_SYS_KERNEL])
    	refresh_proc_sys_kernel(&proc_sys_kernel);

    if (need_refresh[CLUSTER_VMSTAT])
    	refresh_proc_vmstat(&_pm_proc_vmstat);

    if (need_refresh[CLUSTER_SYSFS_KERNEL])
    	refresh_sysfs_kernel(&sysfs_kernel);

    if (need_refresh[CLUSTER_NET_SOFTNET])
	refresh_proc_net_softnet(&proc_net_softnet);

    if (need_refresh[CLUSTER_SHM_STAT])
	refresh_shm_stat(INDOM(IPC_STAT_INDOM));

    if (need_refresh[CLUSTER_MSG_STAT])
	refresh_msg_queue(INDOM(IPC_MSG_INDOM));

    if (need_refresh[CLUSTER_SEM_STAT])
	refresh_sem_array(INDOM(IPC_SEM_INDOM));

    if (need_refresh[CLUSTER_BUDDYINFO])
	refresh_proc_buddyinfo(&proc_buddyinfo);

    if (need_refresh[CLUSTER_ZONEINFO] ||
        need_refresh[CLUSTER_ZONEINFO_PROTECTION])
	refresh_proc_zoneinfo(INDOM(ZONEINFO_INDOM),
			      INDOM(ZONEINFO_PROTECTION_INDOM));

    if (need_refresh[CLUSTER_KSM_INFO])
	refresh_ksm_info(&ksm_info);

    if (need_refresh[CLUSTER_TAPEDEV])
	refresh_sysfs_tapestats(INDOM(TAPEDEV_INDOM));

    if (need_refresh[CLUSTER_TTY]) {
	if (all_access ||
	    (laccess != NULL && laccess->uid == 0 && laccess->uid_flag)) {
	    proc_tty_permission = 1;
	    refresh_tty(INDOM(TTY_INDOM));
	} else {
	    proc_tty_permission = 0;
	}
    }

    if (need_refresh[CLUSTER_PRESSURE_CPU])
	refresh_proc_pressure_cpu(&proc_pressure);
    if (need_refresh[CLUSTER_PRESSURE_MEM])
	refresh_proc_pressure_mem(&proc_pressure);
    if (need_refresh[CLUSTER_PRESSURE_IO])
	refresh_proc_pressure_io(&proc_pressure);

    if (need_refresh[CLUSTER_FCHOST])
	refresh_sysfs_fchosts(INDOM(FCHOST_INDOM));

done:
    container_close(cp, ns_fds);
    return sts;
}

static int
linux_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int			need_refresh[NUM_REFRESHES] = {0};
    int			sts;

    switch (pmInDom_serial(indom)) {
    case DISK_INDOM:
    case PARTITIONS_INDOM:
    case DM_INDOM:
    case MD_INDOM:
	need_refresh[CLUSTER_PARTITIONS]++;
	need_refresh[REFRESH_PROC_DISKSTATS]++;
	break;
    case ZRAM_INDOM:
	need_refresh[CLUSTER_ZRAM_DEVICES]++;
	need_refresh[REFRESH_PROC_DISKSTATS]++;
	break;
    case CPU_INDOM:
	need_refresh[CLUSTER_STAT]++;
	break;
    case NODE_INDOM:
	need_refresh[CLUSTER_NUMA_MEMINFO]++;
	break;
    case NET_DEV_INDOM:
	need_refresh[CLUSTER_NET_DEV]++;
	break;
    case NET_ADDR_INDOM:
	need_refresh[CLUSTER_NET_ADDR]++;
	need_refresh[REFRESH_NETADDR_INET]++;
	need_refresh[REFRESH_NETADDR_IPV6]++;
	need_refresh[REFRESH_NETADDR_HW]++;
	break;
    case FCHOST_INDOM:
	need_refresh[CLUSTER_FCHOST]++;
	break;
    case FILESYS_INDOM:
	need_refresh[CLUSTER_FILESYS]++;
	break;
    case TMPFS_INDOM:
	need_refresh[CLUSTER_TMPFS]++;
	break;
    case SWAPDEV_INDOM:
	need_refresh[CLUSTER_SWAPDEV]++;
	break;
    case NFS_INDOM:
    case NFS3_INDOM:
    case NFS4_CLI_INDOM:
    case NFS4_SVR_INDOM:
	need_refresh[CLUSTER_NET_NFS]++;
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
    case IPC_STAT_INDOM:
	need_refresh[CLUSTER_SHM_STAT]++;
	break;
    case IPC_MSG_INDOM:
	need_refresh[CLUSTER_MSG_STAT]++;
	break;
    case IPC_SEM_INDOM:
	need_refresh[CLUSTER_SEM_STAT]++;
	break;
    case BUDDYINFO_INDOM:
        need_refresh[CLUSTER_BUDDYINFO]++;
        break;
    case ZONEINFO_INDOM:
	need_refresh[CLUSTER_ZONEINFO]++;
        break;
    case ZONEINFO_PROTECTION_INDOM:
        need_refresh[CLUSTER_ZONEINFO_PROTECTION]++;
        break;
    case TAPEDEV_INDOM:
	need_refresh[CLUSTER_TAPEDEV]++;
	break;
    case TTY_INDOM:
	need_refresh[CLUSTER_TTY]++;
	break;
    case INTERRUPT_INDOM:
	need_refresh[CLUSTER_INTERRUPTS]++;
	break;
    case INTERRUPT_CPU_INDOM:
	need_refresh[CLUSTER_INTERRUPTS]++;
	break;
    case SOFTIRQ_INDOM:
	need_refresh[CLUSTER_SOFTIRQS_TOTAL]++;
	break;
    case SOFTIRQ_CPU_INDOM:
        need_refresh[CLUSTER_SOFTIRQS]++;
	break;
    /* no default label : pmdaInstance will pick up errors */
    }

    if ((sts = linux_refresh(pmda, need_refresh, pmda->e_context)) < 0)
	return sts;
    return pmdaInstance(indom, inst, name, result, pmda);
}

/*
 * callback provided to pmdaFetch
 */

static int
linux_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);
    pmInDom		indom = mdesc->m_desc.indom;
    int			i;
    int			sts;
    long		sl;
    percpu_t		*cp;
    pernode_t		*np;
    struct filesys	*fs;
    net_addr_t		*addrp;
    net_interface_t	*netip;
    scsi_entry_t	*scsi_entry;
    char		*name;

    if (mdesc->m_user != NULL) {
	/* 
	 * The metric value is extracted directly via the address specified
	 * in metrictab.  Note: not all metrics support this - those that
	 * don't have NULL for the m_user field in their respective
         * metrictab slot.
	 */
	switch (cluster) {
	case CLUSTER_VMSTAT:
	    if (!(_pm_have_proc_vmstat) ||
		*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
	    	return 0; /* no value available on this kernel */
	    break;
	case CLUSTER_NET_SNMP:
	    /* network.icmpmsg has an indom - deal with it now */
	    if (item == 88 || item == 89) {
		__uint64_t value;
		if (inst > NR_ICMPMSG_COUNTERS)
		    return PM_ERR_INST;
		value = *((__uint64_t *)mdesc->m_user + inst);
		if (value == (__uint64_t)-1)
		    return 0;	/* no value for this instance */
		atom->ull = value;
		return 1;
	    }
	    if (*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
		if (item != 53)	/* tcp.maxconn is special */
		    return 0; /* no value available on this kernel */
	    break;
	case CLUSTER_NET_NETSTAT:
	    if (*(__uint64_t *)mdesc->m_user == (__uint64_t)-1)
	        return 0; /* no value available on this kernel */
	    break;
	case CLUSTER_NET_NFS:
	    /*
	     * check if rpc stats are available
	     */
	    if (item >= 20 && item <= 27 && proc_net_rpc.client.errcode != 0)
		/* no values available for client rpc/nfs; expected <= 2.0.36 */
	    	return 0;
	    if (item >= 30 && item <= 47 && proc_net_rpc.server.errcode != 0)
		/* no values available - expected without /proc/net/rpc/nfsd */
	    	return 0; /* no values available */
	    if (item >= 51 && item <= 57 && proc_net_rpc.server.errcode != 0)
		/* no values available - expected without /proc/net/rpc/nfsd */
	    	return 0; /* no values available */
	    if (item >= 71 && item <= 76 && proc_fs_nfsd.errcode != 0)
		/* no values available - expected without /proc/fs/nfsd */
	    	return 0; /* no values available */
	    break;
	case CLUSTER_SYSFS_KERNEL:
	    /* no values available for udev metrics */
	    if (item == 0 && !sysfs_kernel.valid_uevent_seqnum)
		return 0;
	    break;
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
	return PMDA_FETCH_STATIC;
    }

    switch (cluster) {
    case CLUSTER_STAT:
	/*
	 * All metrics from /proc/stat
	 */
	switch (item) {
	case 0: /* kernel.percpu.cpu.user */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.user / hz);
	    break;
	case 1: /* kernel.percpu.cpu.nice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.nice / hz);
	    break;
	case 2: /* kernel.percpu.cpu.sys */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.sys / hz);
	    break;
	case 3: /* kernel.percpu.cpu.idle */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_idletime_size, atom, 1000 * (double)cp->stat.idle / hz);
	    break;
	case 30: /* kernel.percpu.cpu.wait.total */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.wait / hz);
	    break;
	case 31: /* kernel.percpu.cpu.intr */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)cp->stat.irq + (double)cp->stat.sirq) / hz);
	    break;
	case 56: /* kernel.percpu.cpu.irq.soft */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.sirq / hz);
	    break;
	case 57: /* kernel.percpu.cpu.irq.hard */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.irq / hz);
	    break;
	case 58: /* kernel.percpu.cpu.steal */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.steal / hz);
	    break;
	case 61: /* kernel.percpu.cpu.guest */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)cp->stat.guest / hz);
	    break;
	case 76: /* kernel.percpu.cpu.vuser */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(cp->stat.user - cp->stat.guest) / hz);
	    break;
	case 83: /* kernel.percpu.cpu.guest_nice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)cp->stat.guest_nice / hz);
	    break;
	case 84: /* kernel.percpu.cpu.vnice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(cp->stat.nice - cp->stat.guest_nice) / hz);
	    break;
	case 62: /* kernel.pernode.cpu.user */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.user / hz);
	    break;
	case 63: /* kernel.pernode.cpu.nice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.nice / hz);
	    break;
	case 64: /* kernel.pernode.cpu.sys */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.sys / hz);
	    break;
	case 65: /* kernel.pernode.cpu.idle */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_idletime_size, atom, 1000 * (double)np->stat.idle / hz);
	    break;
	case 69: /* kernel.pernode.cpu.wait.total */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.wait / hz);
	    break;
	case 66: /* kernel.pernode.cpu.intr */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)np->stat.irq + (double)np->stat.sirq) / hz);
	    break;
	case 70: /* kernel.pernode.cpu.irq.soft */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.sirq / hz);
	    break;
	case 71: /* kernel.pernode.cpu.irq.hard */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.irq / hz);
	    break;
	case 67: /* kernel.pernode.cpu.steal */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.steal / hz);
	    break;
	case 68: /* kernel.pernode.cpu.guest */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom, 1000 * (double)np->stat.guest / hz);
	    break;
	case 77: /* kernel.pernode.cpu.vuser */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(np->stat.user - np->stat.guest) / hz);
	    break;
	case 85: /* kernel.pernode.cpu.guest_nice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)np->stat.guest_nice / hz);
	    break;
	case 86: /* kernel.pernode.cpu.vnice */
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&np) < 0)
		return PM_ERR_INST;
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(np->stat.nice - np->stat.guest_nice) / hz);
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
	case 15: /* kernel.all.running */
	    _pm_assign_ulong(atom, proc_stat.procs_running);
	    break;
	case 16: /* kernel.all.blocked */
	    _pm_assign_ulong(atom, proc_stat.procs_blocked);
	    break;
	case 17: /* kernel.all.boottime */
	    atom->ll = proc_stat.btime;
	    break;

	case 20: /* kernel.all.cpu.user */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.user / hz);
	    break;
	case 21: /* kernel.all.cpu.nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.nice / hz);
	    break;
	case 22: /* kernel.all.cpu.sys */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.sys / hz);
	    break;
	case 23: /* kernel.all.cpu.idle */
	    _pm_assign_utype(_pm_idletime_size, atom,
			1000 * (double)proc_stat.all.idle / hz);
	    break;
	case 34: /* kernel.all.cpu.intr */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * ((double)proc_stat.all.irq +
			      	(double)proc_stat.all.sirq) / hz);
	    break;
	case 35: /* kernel.all.cpu.wait.total */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.wait / hz);
	    break;
	case 53: /* kernel.all.cpu.irq.soft */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.sirq / hz);
	    break;
	case 54: /* kernel.all.cpu.irq.hard */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.irq / hz);
	    break;
	case 55: /* kernel.all.cpu.steal */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.steal / hz);
	    break;
	case 60: /* kernel.all.cpu.guest */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.guest / hz);
	    break;
	case 78: /* kernel.all.cpu.vuser */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(proc_stat.all.user - proc_stat.all.guest) / hz);
	    break;
	case 81: /* kernel.all.cpu.guest_nice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)proc_stat.all.guest_nice / hz);
	    break;
	case 82: /* kernel.all.cpu.vnice */
	    _pm_assign_utype(_pm_cputime_size, atom,
			1000 * (double)(proc_stat.all.nice - proc_stat.all.guest_nice) / hz);
	    break;
	case 19: /* hinv.nnode */
	    atom->ul = pmdaCacheOp(INDOM(NODE_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;
	case 32: /* hinv.ncpu */
	    atom->ul = pmdaCacheOp(INDOM(CPU_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;
	case 33: /* hinv.ndisk */
	    atom->ul = pmdaCacheOp(INDOM(DISK_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;

	case 48: /* kernel.all.hz */
	    atom->ul = hz;
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
	switch (item) {
	case 0: /* kernel.all.uptime (in seconds) */
	    atom->d = proc_uptime.uptime;
	    break;
	case 1:
	    /* kernel.all.idletime (in seconds) */
	    atom->d = proc_uptime.idletime;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_MEMINFO: /* mem */
    	switch (item) {
	case 0: /* mem.physmem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemTotal;
	    break;
	case 1: /* mem.util.used (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
		return 0; /* no values available */
	    atom->ull = proc_meminfo.MemTotal - proc_meminfo.MemFree;
	    break;
	case 2: /* mem.util.free (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree;
	    break;
	case 3: /* mem.util.shared (in kbytes) */
	    /*
	     * If this metric is exported by the running kernel, it is always
	     * zero (deprecated). PCP exports it for compatibility with older
	     * PCP monitoring tools, e.g. pmgsys running on IRIX(TM).
	     */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemShared))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemShared;
	    break;
	case 4: /* mem.util.bufmem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Buffers))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Buffers;
	    break;
	case 5: /* mem.util.cached (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Cached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Cached;
	    break;
	case 6: /* swap.length (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal << 10;
	    break;
	case 7: /* swap.used (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
		return 0; /* no values available */
	    atom->ull = (proc_meminfo.SwapTotal - proc_meminfo.SwapFree) << 10;
	    break;
	case 8: /* swap.free (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree << 10;
	    break;
	case 9: /* hinv.physmem (in mbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal))
	    	return 0; /* no values available */
	    atom->ul = proc_meminfo.MemTotal >> 10;
	    break;
	case 10: /* mem.freemem (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.MemFree;
	    break;
	case 11: /* hinv.pagesize (in bytes) */
	    atom->ul = 1 << _pm_pageshift;
	    break;
	case 12: /* mem.util.other (in kbytes) */
	    /* other = used - (cached+buffers) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.MemTotal) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.MemFree) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Cached) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Buffers))
		return 0; /* no values available */
	    sl = proc_meminfo.MemTotal -
		 proc_meminfo.MemFree -
		 proc_meminfo.Cached -
		 proc_meminfo.Buffers;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	case 13: /* mem.util.swapCached (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapCached))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapCached;
	    break;
	case 14: /* mem.util.active (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Active))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Active;
	    break;
	case 15: /* mem.util.inactive (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Inactive;
	    break;
	case 16: /* mem.util.highTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.HighTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighTotal;
	    break;
	case 17: /* mem.util.highFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.HighFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.HighFree;
	    break;
	case 18: /* mem.util.lowTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.LowTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowTotal;
	    break;
	case 19: /* mem.util.lowFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.LowFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.LowFree;
	    break;
	case 20: /* mem.util.swapTotal (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapTotal))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapTotal;
	    break;
	case 21: /* mem.util.swapFree (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.SwapFree))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.SwapFree;
	    break;
	case 22: /* mem.util.dirty (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Dirty))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Dirty;
	    break;
	case 23: /* mem.util.writeback (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Writeback;
	    break;
	case 24: /* mem.util.mapped (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Mapped))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Mapped;
	    break;
	case 25: /* mem.util.slab (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Slab))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Slab;
	    break;
	case 26: /* mem.util.committed_AS (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Committed_AS))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.Committed_AS;
	    break;
	case 27: /* mem.util.pageTables (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.PageTables))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.PageTables;
	    break;
	case 28: /* mem.util.reverseMaps (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.ReverseMaps))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.ReverseMaps;
	    break;
	case 29: /* mem.util.cache_clean (in kbytes) */
	    /* clean=cached-(dirty+writeback) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Cached) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Dirty) ||
	        !MEMINFO_VALID_VALUE(proc_meminfo.Writeback))
	    	return 0; /* no values available */
	    sl = proc_meminfo.Cached -
	    	 proc_meminfo.Dirty -
	    	 proc_meminfo.Writeback;
	    atom->ull = sl >= 0 ? sl : 0;
	    break;
	case 30: /* mem.util.anonpages */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.AnonPages))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.AnonPages;
	   break;
	case 31: /* mem.util.commitLimit (in kbytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.CommitLimit))
	    	return 0; /* no values available */
	    atom->ull = proc_meminfo.CommitLimit;
	    break;
	case 32: /* mem.util.bounce */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Bounce))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Bounce;
	   break;
	case 33: /* mem.util.NFS_Unstable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.NFS_Unstable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.NFS_Unstable;
	   break;
	case 34: /* mem.util.slabReclaimable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.SlabReclaimable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.SlabReclaimable;
	   break;
	case 35: /* mem.util.slabUnreclaimable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.SlabUnreclaimable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.SlabUnreclaimable;
	   break;
	case 36: /* mem.util.active_anon */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Active_anon))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Active_anon;
	   break;
	case 37: /* mem.util.inactive_anon */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive_anon))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Inactive_anon;
	   break;
	case 38: /* mem.util.active_file */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Active_file))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Active_file;
	   break;
	case 39: /* mem.util.inactive_file */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Inactive_file))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Inactive_file;
	   break;
	case 40: /* mem.util.unevictable */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Unevictable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Unevictable;
	   break;
	case 41: /* mem.util.mlocked */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Mlocked))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Mlocked;
	   break;
	case 42: /* mem.util.shmem */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Shmem))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Shmem;
	   break;
	case 43: /* mem.util.kernelStack */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.KernelStack))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.KernelStack;
	   break;
	case 44: /* mem.util.hugepagesTotal (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesTotal))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesTotal;
	   break;
	case 45: /* mem.util.hugepagesFree (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesFree))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesFree;
	   break;
	case 46: /* mem.util.hugepagesRsvd (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesRsvd))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesRsvd;
	   break;
	case 47: /* mem.util.hugepagesSurp (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesSurp))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesSurp;
	   break;
	case 48: /* mem.util.directMap4k (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap4k))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap4k;
	   break;
	case 49: /* mem.util.directMap2M (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap2M))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap2M;
	   break;
	case 50: /* mem.util.vmallocTotal (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocTotal))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocTotal;
	   break;
	case 51: /* mem.util.vmallocUsed (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocUsed))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocUsed;
	   break;
	case 52: /* mem.util.vmallocChunk (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.VmallocChunk))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.VmallocChunk;
	   break;
	case 53: /* mem.util.mmap_copy (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.MmapCopy))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.MmapCopy;
	   break;
	case 54: /* mem.util.quicklists (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.Quicklists))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.Quicklists;
	   break;
	case 55: /* mem.util.corrupthardware (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HardwareCorrupted))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HardwareCorrupted;
	   break;
	case 56: /* mem.util.anonhugepages (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.AnonHugePages))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.AnonHugePages;
	   break;
	case 57: /* mem.util.directMap1G (in pages) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.directMap1G))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.directMap1G;
	   break;
	case 58: /* mem.util.available (in kbytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.MemAvailable))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.MemAvailable;
	   break;
	case 59: /* hinv.hugepagesize (in bytes) */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Hugepagesize))
	    	return 0; /* no values available */
	    atom->ul = (proc_meminfo.Hugepagesize << 10);
	    break;
	case 60: /* mem.util.hugepagesTotalBytes (in bytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesTotal))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesTotal *
			(proc_meminfo.Hugepagesize << 10);
	   break;
	case 61: /* mem.util.hugepagesFreeBytes (in bytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesFree))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesFree *
			(proc_meminfo.Hugepagesize << 10);
	   break;
	case 62: /* mem.util.hugepagesRsvdBytes (in bytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesRsvd))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesRsvd *
			(proc_meminfo.Hugepagesize << 10);
	   break;
	case 63: /* mem.util.hugepagesSurpBytes (in bytes) */
	   if (!MEMINFO_VALID_VALUE(proc_meminfo.HugepagesSurp))
		return 0; /* no values available */
	   atom->ull = proc_meminfo.HugepagesSurp *
			(proc_meminfo.Hugepagesize << 10);
	   break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_LOADAVG: 
	switch(item) {
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
	if (item == 27) {	/* hinv.ninterface */
	    atom->ul = pmdaCacheOp(INDOM(NET_DEV_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	    break;
	}
	sts = pmdaCacheLookup(INDOM(NET_DEV_INDOM), inst, NULL, (void **)&netip);
	if (sts < 0)
	    return sts;
	if (item <= 15) {
	    /* network.interface.{in,out} */
	    atom->ull = netip->counters[item];
	}
	else
	switch (item) {
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
	    atom->ull = ((unsigned long long)netip->ioc.speed * 1000000 / 8);
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
	case 28: /* network.interface.wireless */
	    atom->ul = netip->ioc.wireless;
	    break;
	case 29: /* network.interface.type */
	    atom->ul = netip->ioc.type;
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
	switch (item) {
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
	if (item == 0)
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

	    switch (item) {
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

	    switch (item) {
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

	switch (item) {
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
	switch (item) {
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
	if (proc_slabinfo.permission != 1)
	    return 0;
	return proc_slabinfo_fetch(INDOM(SLAB_INDOM), item, inst, atom);

    case CLUSTER_PARTITIONS:
    case CLUSTER_ZRAM_DEVICES:
    case CLUSTER_ZRAM_IO_STAT:
    case CLUSTER_ZRAM_MM_STAT:
    case CLUSTER_ZRAM_BD_STAT:
	return proc_partitions_fetch(mdesc, inst, atom);

    case CLUSTER_SCSI:
	scsi_entry = NULL;
	sts = pmdaCacheLookup(INDOM(SCSI_INDOM), inst, NULL, (void **)&scsi_entry);
	if (sts < 0)
	    return sts;
	if (sts == PMDA_CACHE_INACTIVE)
	    return PM_ERR_INST;
	switch (item) {
	case 0: /* hinv.map.scsi */
	    atom->cp = (scsi_entry && scsi_entry->dev_name) ? scsi_entry->dev_name : "";
	    break;
	default:
	    return PM_ERR_PMID;
	}
    	break;

    case CLUSTER_KERNEL_UNAME:
	switch(item) {
	case 5: /* pmda.uname */
	    pmsprintf(uname_string, sizeof(uname_string), "%s %s %s %s %s",
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
	if (inst != PM_IN_NULL &&
	    pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
	    return PM_ERR_INST;
	switch (item) {
	case 0: /* hinv.cpu.clock */
	    if (cp->info.clock == 0.0)
		return 0;
	    atom->f = cp->info.clock;
	    break;
	case 1: /* hinv.cpu.vendor */
	    i = cp->info.vendor;
	    if ((atom->cp = linux_strings_lookup(i)) == NULL)
		atom->cp = "unknown";
	    break;
	case 2: /* hinv.cpu.model */
	    if ((i = cp->info.model) < 0)
		i = cp->info.model_name;
	    if ((atom->cp = linux_strings_lookup(i)) == NULL)
		atom->cp = "unknown";
	    break;
	case 3: /* hinv.cpu.stepping */
	    i = cp->info.stepping;
	    if ((atom->cp = linux_strings_lookup(i)) == NULL)
		atom->cp = "unknown";
	    break;
	case 4: /* hinv.cpu.cache */
	    if (!cp->info.cache)
		return 0;
	    atom->ul = cp->info.cache;
	    break;
	case 5: /* hinv.cpu.bogomips */
	    if (cp->info.bogomips == 0.0)
		return 0;
	    atom->f = cp->info.bogomips;
	    break;
	case 6: /* hinv.map.cpu_num */
	    atom->ul = cp->cpuid;
	    break;
	case 7: /* hinv.machine ... not from /proc/stat */
	    atom->cp = get_machine_info(kernel_uname.machine);
	    break;
	case 8: /* hinv.map.cpu_node */
	    atom->ul = cp->node->nodeid;
	    break;
	case 9: /* hinv.cpu.model_name */
	    if ((i = cp->info.model_name) < 0)
		i = cp->info.model;
	    if ((atom->cp = linux_strings_lookup(i)) == NULL)
		atom->cp = "unknown";
	    break;
	case 10: /* hinv.cpu.flags */
	    i = cp->info.flags;
	    if ((atom->cp = linux_strings_lookup(i)) == NULL)
		atom->cp = "unknown";
	    break;
	case 11: /* hinv.cpu.cache_alignment */
	    if (!cp->info.cache_align)
		return 0;
	    atom->ul = cp->info.cache_align;
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_SYSFS_DEVICES:
	if (item == 1)	/* hinv.node.online */
	    sts = pmdaCacheLookup(INDOM(NODE_INDOM), inst, &name, (void **)&np);
	else		/* hinv.cpu metrics */
	    sts = pmdaCacheLookup(INDOM(CPU_INDOM), inst, &name, (void **)&cp);
	if (sts < 0)
	    return PM_ERR_INST;
	switch (item) {
	case 0: /* hinv.cpu.online */
	    atom->ul = refresh_sysfs_online(name, "cpu");
	    break;
	case 1: /* hinv.node.online */
	    atom->ul = refresh_sysfs_online(name, "node");
	    break;

	case 2: /* hinv.cpu.thermal_throttle.core.count */
	    _pm_assign_ulong(atom, refresh_sysfs_thermal_throttle(
				name, "core", "count"));
	    break;
	case 3: /* hinv.cpu.thermal_throttle.core.time */
	    _pm_assign_ulong(atom, refresh_sysfs_thermal_throttle(
				name, "core", "total_time_ms"));
	    break;
	case 4: /* hinv.cpu.thermal_throttle.package.count */
	    _pm_assign_ulong(atom, refresh_sysfs_thermal_throttle(
				name, "package", "count"));
	    break;
	case 5: /* hinv.cpu.thermal_throttle.package.time */
	    _pm_assign_ulong(atom, refresh_sysfs_thermal_throttle(
				name, "package", "total_time_ms"));
	    break;

	case 6: /* hinv.cpu.frequency_scaling.count */
	    if (refresh_sysfs_frequency_scaling(name, item, cp) < 0 ||
		(!(cp->freq.flags & CPUFREQ_COUNT)))
		return 0;
	    atom->ull = cp->freq.count;
	    break;
	case 7: /* hinv.cpu.frequency_scaling.time */
	    if (refresh_sysfs_frequency_scaling(name, item, cp) < 0 ||
		(!(cp->freq.flags & CPUFREQ_TIME)))
		return 0;
	    atom->ull = cp->freq.time;
	    break;
	case 8: /* hinv.cpu.frequency_scaling.max */
	    if (refresh_sysfs_frequency_scaling(name, item, cp) < 0 ||
		(!(cp->freq.flags & CPUFREQ_MAX)))
		return 0;
	    atom->ul = cp->freq.max;
	    break;
	case 9: /* hinv.cpu.frequency_scaling.min */
	    if (refresh_sysfs_frequency_scaling(name, item, cp) < 0 ||
		(!(cp->freq.flags & CPUFREQ_MIN)))
		return 0;
	    atom->ul = cp->freq.min;
	    break;

	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Wu Liming <wulm.fnst@cn.fujitsu.com>
     */
    case CLUSTER_ZONEINFO_PROTECTION: {
	zoneprot_entry_t *prot;

	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&prot);
	if (sts < 0)
	    return sts;
	if (sts == PMDA_CACHE_INACTIVE)
	    return PM_ERR_INST;
	switch (item) {
	case 0: /* mem.zoneinfo.protection */
            atom->ull = (prot->value << _pm_pageshift) / 1024;
	}
	break;
    }

    case CLUSTER_ZONEINFO: {
	zoneinfo_entry_t *info;

	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&info);
	if (sts < 0)
	    return sts;
	if (sts == PMDA_CACHE_INACTIVE)
	    return PM_ERR_INST;
	/* check if value is up-to-date (is it in this kernel?) */
	if (item < ZONE_VALUES0) {
	    if (!(info->flags & (1ULL << item)))
		return 0;
	} else if (item < ZONE_VALUES1) {
	    if (!(info->flags1 & (1ULL << (item - ZONE_VALUES0))))
		return 0;
	} else {
	    return PM_ERR_PMID;
	}
	atom->ull = (info->values[item] << _pm_pageshift) / 1024;
	break;
    }

    case CLUSTER_KSM_INFO:
	switch (item) {
	case 0: /* mem.ksm.full_scans */
	    _pm_assign_ulong(atom, ksm_info.full_scans);
	    break;
	case 1: /* mem.ksm.merge_across_nodes */
	    _pm_assign_ulong(atom, ksm_info.merge_across_nodes);
	    break;
	case 2: /* mem.ksm.pages_shared */
	    _pm_assign_ulong(atom, ksm_info.pages_shared);
	    break;
	case 3: /* mem.ksm.pages_sharing */
	    _pm_assign_ulong(atom, ksm_info.pages_sharing);
	    break;
	case 4: /* mem.ksm.pages_to_scan */
	    _pm_assign_ulong(atom, ksm_info.pages_to_scan);
	    break;
	case 5: /* mem.ksm.pages_unshared */
	    _pm_assign_ulong(atom, ksm_info.pages_unshared);
	    break;
	case 6: /* mem.ksm.pages_volatile */
	    _pm_assign_ulong(atom, ksm_info.pages_volatile);
	    break;
	case 7: /* mem.ksm.run_state */
	    _pm_assign_ulong(atom, ksm_info.run);
	    break;
	case 8: /* mem.ksm.sleep_time */
	    _pm_assign_ulong(atom, ksm_info.sleep_millisecs);
	    break;
	default:
	    return PM_ERR_PMID;
        }
	break;

    case CLUSTER_SEM_INFO:
	switch (item) {
	case 0:	/* ipc.sem.used_sem */
	    atom->ul = sem_info.semusz;
	    break;
	case 1:	/* ipc.sem.tot_sem */
	    atom->ul = sem_info.semaem;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Wu Liming <wulm.fnst@cn.fujitsu.com>
     */
    case CLUSTER_SHM_STAT: {
	shm_stat_t *shmp;

	sts = pmdaCacheLookup(INDOM(IPC_STAT_INDOM), inst, NULL, (void **)&shmp);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	switch (item) {
	case 0:	/* ipc.shm.key */
	    atom->cp = (char *)shmp->keyid;
	    break;
	case 1:	/* ipc.shm.owner */
	    atom->cp = (char *)shmp->owner;
	    break;
	case 2:	/* ipc.shm.perms */
	    atom->ul = shmp->perms;
	    break;
	case 3:	/* ipc.shm.segsz */
	    atom->ul = shmp->bytes;
	    break;
	case 4:	/* ipc.shm.nattch */
	    atom->ul = shmp->nattach;
	    break;
	case 5:	/* ipc.shm.status */
	    if (shmp->dest && shmp->locked)
		atom->cp =  "dest locked";
	    else if (shmp->locked)
		atom->cp = "locked";
	    else if (shmp->dest)
		atom->cp = "dest";
	    else
		atom->cp = "";
	    break;
	case 6:	/* ipc.shm.creator_pid */
	    atom->ul = shmp->cpid;
	    break;
	case 7:	/* ipc.shm.last_access_pid */
	    atom->ul = shmp->lpid;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
     }

    case CLUSTER_MSG_STAT: {
	msg_queue_t *msgq;

	sts = pmdaCacheLookup(INDOM(IPC_MSG_INDOM), inst, NULL, (void **)&msgq);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	switch (item) {
	case 0:	/* ipc.msg.key */
	    atom->cp = (char *)msgq->keyid;
	    break;
	case 1:	/* ipc.msg.owner */
	    atom->cp = (char *)msgq->owner;
	    break;
	case 2:	/* ipc.msg.perms */
	    atom->ul = msgq->perms;
	    break;
	case 3:	/* ipc.msg.msgsz */
	    atom->ul = msgq->bytes;
	    break;
	case 4:	/* ipc.msg.messages */
	    atom->ul = msgq->messages;
	    break;
	case 5:	/* ipc.msg.last_send_pid */
	    atom->ul = msgq->lspid;
	    break;
	case 6:	/* ipc.msg.last_recv_pid */
	    atom->ul = msgq->lrpid;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
     }

    case CLUSTER_SEM_STAT: {
	sem_array_t *semp;

	sts = pmdaCacheLookup(INDOM(IPC_SEM_INDOM), inst, NULL, (void **)&semp);
	if (sts < 0)
	    return sts;
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;
	switch (item) {
	case 0:	/* ipc.sem.key */
	    atom->cp = (char *)semp->keyid;
	    break;
	case 1:	/* ipc.sem.owner */
	    atom->cp = (char *)semp->owner;
	    break;
	case 2:	/* ipc.sem.perms */
	    atom->ul = semp->perms;
	    break;
	case 3:	/* ipc.sem.nsems */
	    atom->ul = semp->nsems;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;
    }
 
    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_SEM_LIMITS:
	switch (item) {
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

    case CLUSTER_SHM_INFO:
	switch (item) {
	case 0: /* ipc.shm.tot */
	    atom->ul = shm_info.shm_tot;
	    break;
	case 1: /* ipc.shm.rss */
	    atom->ul = shm_info.shm_rss;
	    break;
	case 2: /* ipc.shm.swp */
	    atom->ul = shm_info.shm_swp;
	    break;
	case 3: /* ipc.shm.used_ids */
	    atom->ul = shm_info.used_ids;
	    break;
	case 4: /* ipc.shm.swap_attempts */
	    atom->ul = shm_info.swap_attempts;
	    break;
	case 5: /* ipc.shm.swap_successes */
	    atom->ul = shm_info.swap_successes;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Wu Liming <wulm.fnst@cn.fujitsu.com>
     */
    case CLUSTER_MSG_INFO:
	switch (item) {
	case 0:	/* ipc.msg.used_queues */
	    atom->ul = msg_info.msgpool;
	    break;
	case 1:	/* ipc.msg.tot_msg */
	    atom->ul = msg_info.msgmap;
	    break;
	case 2:	/* ipc.msg.tot_bytes */
	    atom->ul = msg_info.msgtql;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    /*
     * Cluster added by Mike Mason <mmlnx@us.ibm.com>
     */
    case CLUSTER_MSG_LIMITS:
	switch (item) {
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
	switch (item) {
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

    case CLUSTER_UTMP:
	switch (item) {
	case 0:	/* kernel.all.nusers */
	    atom->ul = login_info.nusers;
	    break;
	case 1:	/* kernel.all.nroots */
	    atom->ul = login_info.nroots;
	    break;
	case 2:	/* kernel.all.nsessions */
	    atom->ul = login_info.nsessions;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_IB: /* deprecated: network.ib, use infiniband PMDA */
        return PM_ERR_APPVERSION;

    case CLUSTER_NUMA_MEMINFO:
	/* NUMA memory metrics from /sys/devices/system/node/nodeX */
	sts = pmdaCacheLookup(indom, inst, NULL, (void **)&np);
	if (sts != PMDA_CACHE_ACTIVE)
	    return PM_ERR_INST;

	switch (item) {
	case 0: /* mem.numa.util.total */
	    sts = linux_table_lookup("MemTotal:", np->meminfo, &atom->ull);
	    break;
	case 1: /* mem.numa.util.free */
	    sts = linux_table_lookup("MemFree:", np->meminfo, &atom->ull);
	    break;
	case 2: /* mem.numa.util.used */
	    sts = linux_table_lookup("MemUsed:", np->meminfo, &atom->ull);
	    break;
	case 3: /* mem.numa.util.active */
	    sts = linux_table_lookup("Active:", np->meminfo, &atom->ull);
	    break;
	case 4: /* mem.numa.util.inactive */
	    sts = linux_table_lookup("Inactive:", np->meminfo, &atom->ull);
	    break;
	case 5: /* mem.numa.util.active_anon */
	    sts = linux_table_lookup("Active(anon):", np->meminfo, &atom->ull);
	    break;
	case 6: /* mem.numa.util.inactive_anon */
	    sts = linux_table_lookup("Inactive(anon):", np->meminfo, &atom->ull);
	    break;
	case 7: /* mem.numa.util.active_file */
	    sts = linux_table_lookup("Active(file):", np->meminfo, &atom->ull);
	    break;
	case 8: /* mem.numa.util.inactive_file */
	    sts = linux_table_lookup("Inactive(file):", np->meminfo, &atom->ull);
	    break;
	case 9: /* mem.numa.util.highTotal */
	    sts = linux_table_lookup("HighTotal:", np->meminfo, &atom->ull);
	    break;
	case 10: /* mem.numa.util.highFree */
	    sts = linux_table_lookup("HighFree:", np->meminfo, &atom->ull);
	    break;
	case 11: /* mem.numa.util.lowTotal */
	    sts = linux_table_lookup("LowTotal:", np->meminfo, &atom->ull);
	    break;
	case 12: /* mem.numa.util.lowFree */
	    sts = linux_table_lookup("LowFree:", np->meminfo, &atom->ull);
	    break;
	case 13: /* mem.numa.util.unevictable */
	    sts = linux_table_lookup("Unevictable:", np->meminfo, &atom->ull);
	    break;
	case 14: /* mem.numa.util.mlocked */
	    sts = linux_table_lookup("Mlocked:", np->meminfo, &atom->ull);
	    break;
	case 15: /* mem.numa.util.dirty */
	    sts = linux_table_lookup("Dirty:", np->meminfo, &atom->ull);
	    break;
	case 16: /* mem.numa.util.writeback */
	    sts = linux_table_lookup("Writeback:", np->meminfo, &atom->ull);
	    break;
	case 17: /* mem.numa.util.filePages */
	    sts = linux_table_lookup("FilePages:", np->meminfo, &atom->ull);
	    break;
	case 18: /* mem.numa.util.mapped */
	    sts = linux_table_lookup("Mapped:", np->meminfo, &atom->ull);
	    break;
	case 19: /* mem.numa.util.anonPages */
	    sts = linux_table_lookup("AnonPages:", np->meminfo, &atom->ull);
	    break;
	case 20: /* mem.numa.util.shmem */
	    sts = linux_table_lookup("Shmem:", np->meminfo, &atom->ull);
	    break;
	case 21: /* mem.numa.util.kernelStack */
	    sts = linux_table_lookup("KernelStack:", np->meminfo, &atom->ull);
	    break;
	case 22: /* mem.numa.util.pageTables */
	    sts = linux_table_lookup("PageTables:", np->meminfo, &atom->ull);
	    break;
	case 23: /* mem.numa.util.NFS_Unstable */
	    sts = linux_table_lookup("NFS_Unstable:", np->meminfo, &atom->ull);
	    break;
	case 24: /* mem.numa.util.bounce */
	    sts = linux_table_lookup("Bounce:", np->meminfo, &atom->ull);
	    break;
	case 25: /* mem.numa.util.writebackTmp */
	    sts = linux_table_lookup("WritebackTmp:", np->meminfo, &atom->ull);
	    break;
	case 26: /* mem.numa.util.slab */
	    sts = linux_table_lookup("Slab:", np->meminfo, &atom->ull);
	    break;
	case 27: /* mem.numa.util.slabReclaimable */
	    sts = linux_table_lookup("SReclaimable:", np->meminfo, &atom->ull);
	    break;
	case 28: /* mem.numa.util.slabUnreclaimable */
	    sts = linux_table_lookup("SUnreclaim:", np->meminfo, &atom->ull);
	    break;
	case 29: /* mem.numa.util.hugepagesTotal */
	    sts = linux_table_lookup("HugePages_Total:", np->meminfo, &atom->ull);
	    break;
	case 30: /* mem.numa.util.hugepagesFree */
	    sts = linux_table_lookup("HugePages_Free:", np->meminfo, &atom->ull);
	    break;
	case 31: /* mem.numa.util.hugepagesSurp */
	    sts = linux_table_lookup("HugePages_Surp:", np->meminfo, &atom->ull);
	    break;
	case 32: /* mem.numa.alloc.hit */
	    sts = linux_table_lookup("numa_hit", np->memstat, &atom->ull);
	    break;
	case 33: /* mem.numa.alloc.miss */
	    sts = linux_table_lookup("numa_miss", np->memstat, &atom->ull);
	    break;
	case 34: /* mem.numa.alloc.foreign */
	    sts = linux_table_lookup("numa_foreign", np->memstat, &atom->ull);
	    break;
	case 35: /* mem.numa.alloc.interleave_hit */
	    sts = linux_table_lookup("interleave_hit", np->memstat, &atom->ull);
	    break;
	case 36: /* mem.numa.alloc.local_node */
	    sts = linux_table_lookup("local_node", np->memstat, &atom->ull);
	    break;
	case 37: /* mem.numa.alloc.other_node */
	    sts = linux_table_lookup("other_node", np->memstat, &atom->ull);
	    break;
	case 38: /* mem.numa.max_bandwidth */
	    atom->d = np->bandwidth;
	    sts = (atom->d > 0.0);
	    break;
	case 39: /* mem.numa.util.hugepagesTotalBytes */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Hugepagesize))
	    	return 0; /* no values available */
	    sts = linux_table_lookup("HugePages_Total:", np->meminfo, &atom->ull);
	    atom->ull *= (proc_meminfo.Hugepagesize << 10);
	    break;
	case 40: /* mem.numa.util.hugepagesFreeBytes */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Hugepagesize))
	    	return 0; /* no values available */
	    sts = linux_table_lookup("HugePages_Free:", np->meminfo, &atom->ull);
	    atom->ull *= (proc_meminfo.Hugepagesize << 10);
	    break;
	case 41: /* mem.numa.util.hugepagesSurpBytes */
	    if (!MEMINFO_VALID_VALUE(proc_meminfo.Hugepagesize))
	    	return 0; /* no values available */
	    sts = linux_table_lookup("HugePages_Surp:", np->meminfo, &atom->ull);
	    atom->ull *= (proc_meminfo.Hugepagesize << 10);
	    break;

	default:
	    return PM_ERR_PMID;
	}
	return sts;

    case CLUSTER_INTERRUPTS:
    case CLUSTER_SOFTIRQS_TOTAL:
    case CLUSTER_SOFTIRQS:
	return proc_interrupts_fetch(cluster, item, inst, atom);

    case CLUSTER_DM:
    case CLUSTER_MD:
    case CLUSTER_MDADM:
    case CLUSTER_WWID:
	return proc_partitions_fetch(mdesc, inst, atom);

    case CLUSTER_NET_SOFTNET:
	switch (item) {
	case 0:	/* network.softnet.processed */
	    if (!(proc_net_softnet.flags & SN_PROCESSED))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.processed;
	    break;
	case 1: /* network.softnet.dropped */
	    if (!(proc_net_softnet.flags & SN_DROPPED))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.dropped;
	    break;
	case 2: /* network.softnet.time_squeeze */
	    if (!(proc_net_softnet.flags & SN_TIME_SQUEEZE))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.time_squeeze;
	    break;
	case 3: /* network.softnet.cpu_collision */
	    if (!(proc_net_softnet.flags & SN_CPU_COLLISION))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.cpu_collision;
	    break;
	case 4: /* network.softnet.received_rps */
	    if (!(proc_net_softnet.flags & SN_RECEIVED_RPS))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.received_rps;
	    break;
	case 5: /* network.softnet.flow_limit_count */
	    if (!(proc_net_softnet.flags & SN_FLOW_LIMIT_COUNT))
		return PM_ERR_APPVERSION;
	    atom->ull = proc_net_softnet.flow_limit_count;
	    break;
	case 6:	/* network.softnet.percpu.processed */
	    if (!(proc_net_softnet.flags & SN_PROCESSED))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->processed;
	    break;
	case 7: /* network.softnet.percpu.dropped */
	    if (!(proc_net_softnet.flags & SN_DROPPED))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->dropped;
	    break;
	case 8: /* network.softnet.percpu.time_squeeze */
	    if (!(proc_net_softnet.flags & SN_TIME_SQUEEZE))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->time_squeeze;
	    break;
	case 9: /* network.softnet.percpu.cpu_collision */
	    if (!(proc_net_softnet.flags & SN_CPU_COLLISION))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->cpu_collision;
	    break;
	case 10: /* network.softnet.percpu.received_rps */
	    if (!(proc_net_softnet.flags & SN_RECEIVED_RPS))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->received_rps;
	    break;
	case 11: /* network.softnet.percpu.flow_limit_count */
	    if (!(proc_net_softnet.flags & SN_FLOW_LIMIT_COUNT))
		return PM_ERR_APPVERSION;
	    if (pmdaCacheLookup(indom, inst, NULL, (void **)&cp) < 0)
		return PM_ERR_INST;
	    atom->ull = cp->softnet->flow_limit_count;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_BUDDYINFO:
	if (inst >= proc_buddyinfo.nbuddys)
	    return PM_ERR_INST;
	switch (item) {
	case 0:
	    _pm_assign_ulong(atom, proc_buddyinfo.buddys[inst].value);
	    break;
	case 1:
	    atom->ull = (unsigned long long)proc_buddyinfo.buddys[inst].value;
	    atom->ull <<= proc_buddyinfo.buddys[inst].order;
	    atom->ull <<= _pm_pageshift;
	    atom->ull >>= 10;	/* convert to kilobytes */
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_TAPEDEV:
	if (item == TAPESTATS_HINV_NTAPE) {
	    /* hinv.ntape */
	    atom->ul = pmdaCacheOp(INDOM(TAPEDEV_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	}
	else {
	    /*
	     * tape.dev.* counters are direct indexed by item, see sysfs_tapestats.h
	     */
	    tapedev_t *tape = NULL;

	    if (item >= TAPESTATS_COUNT)
		return PM_ERR_PMID;
	    sts = pmdaCacheLookup(INDOM(TAPEDEV_INDOM), inst, NULL, (void **)&tape);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE || tape == NULL)
		return PM_ERR_INST;
	    atom->ull = tape->counts[item];
	}
	break;

    case CLUSTER_TTY:
	if (proc_tty_permission != 1)
	    return 0;
	if (indom == INDOM(TTY_INDOM)) {
	    ttydev_t *ttydev = NULL;
	    sts = pmdaCacheLookup(INDOM(TTY_INDOM), inst, NULL, (void **)&ttydev);

	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
		return 0;
	    switch (item) {
	    case TTY_TX:
		atom->ul = ttydev->tx; /* tty.serial.tx */
		break;
	    case TTY_RX:
		atom->ul = ttydev->rx; /* tty.serial.rx */
		break;
	    case TTY_FRAME:
		atom->ul = ttydev->frame; /* tty.serial.frame */
		break;
	    case TTY_PARITY:
		atom->ul = ttydev->parity; /* tty.serial.parity */
		break;
	    case TTY_BRK:
		atom->ul = ttydev->brk; /* tty.serial.brk */
		break;
	    case TTY_OVERRUN:
		atom->ul = ttydev->overrun; /* tty.serial.overrun */
		break;
	    case TTY_IRQ:
		atom->ul = ttydev->irq; /* tty.serial.irq */
		break;

	    default:
		return PM_ERR_PMID;
	    }
	}
	break;

    case CLUSTER_PRESSURE_CPU:
	switch (item) {
	case 0:	/* kernel.all.pressure.cpu.some.avg */
	    if (proc_pressure.some_cpu.updated == 0)
		return 0;
	    if (average_proc_pressure(&proc_pressure.some_cpu, inst, atom) < 0)
	    	return PM_ERR_INST;
	    break;
	case 1:	/* kernel.all.pressure.cpu.some.total */
	    if (proc_pressure.some_cpu.updated == 0)
		return 0;
	    atom->ull = proc_pressure.some_cpu.total;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PRESSURE_MEM:
	switch (item) {
	case 0:	/* kernel.all.pressure.memory.some.avg */
	    if (proc_pressure.some_mem.updated == 0)
		return 0;
	    if (average_proc_pressure(&proc_pressure.some_mem, inst, atom) < 0)
	    	return PM_ERR_INST;
	    break;
	case 1:	/* kernel.all.pressure.memory.some.total */
	    if (proc_pressure.some_mem.updated == 0)
		return 0;
	    atom->ull = proc_pressure.some_mem.total;
	    break;
	case 2:	/* kernel.all.pressure.memory.full.avg */
	    if (proc_pressure.full_mem.updated == 0)
		return 0;
	    if (average_proc_pressure(&proc_pressure.full_mem, inst, atom) < 0)
	    	return PM_ERR_INST;
	    break;
	case 3:  /* kernel.all.pressure.memory.full.total */
	    if (proc_pressure.full_mem.updated == 0)
		return 0;
	    atom->ull = proc_pressure.full_mem.total;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_PRESSURE_IO:
	switch (item) {
	case 0:	/* kernel.all.pressure.io.some.avg */
	    if (proc_pressure.some_io.updated == 0)
		return 0;
	    if (average_proc_pressure(&proc_pressure.some_io, inst, atom) < 0)
	    	return PM_ERR_INST;
	    break;
	case 1:  /* kernel.all.pressure.io.some.total */
	    if (proc_pressure.some_io.updated == 0)
		return 0;
	    atom->ull = proc_pressure.some_io.total;
	    break;
	case 2:	/* kernel.all.pressure.io.full.avg */
	    if (proc_pressure.full_io.updated == 0)
		return 0;
	    if (average_proc_pressure(&proc_pressure.full_io, inst, atom) < 0)
	    	return PM_ERR_INST;
	    break;
	case 3:  /* kernel.all.pressure.io.full.total */
	    if (proc_pressure.full_io.updated == 0)
		return 0;
	    atom->ull = proc_pressure.full_io.total;
	    break;
	default:
	    return PM_ERR_PMID;
	}
	break;

    case CLUSTER_FCHOST:
	if (item == FCHOST_HINV_NFCHOST) {
	    /* hinv.nfchost */
	    atom->ul = pmdaCacheOp(INDOM(FCHOST_INDOM), PMDA_CACHE_SIZE_ACTIVE);
	}
	else {
	    /*
	     * fchost.* metrics are direct indexed by item, see sysfs_fchost.h
	     */
	    fchost_t *fchost = NULL;

	    if (item >= FCHOST_COUNT)
		return PM_ERR_PMID;
	    sts = pmdaCacheLookup(INDOM(FCHOST_INDOM), inst, NULL, (void **)&fchost);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE || fchost == NULL)
		return PM_ERR_INST;
	    atom->ull = fchost->counts[item];
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return PMDA_FETCH_STATIC;
}


static int
linux_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i, sts, need_refresh[NUM_REFRESHES] = {0};

    for (i = 0; i < numpmid; i++) {
	unsigned int	cluster = pmID_cluster(pmidlist[i]);
	unsigned int	item = pmID_item(pmidlist[i]);

	if (cluster >= NUM_CLUSTERS)
	    continue;

	switch (cluster) {
	case CLUSTER_STAT:
	case CLUSTER_DM:
	case CLUSTER_MD:
	case CLUSTER_MDADM:
	    if (is_partitions_metric(pmidlist[i])) {
		need_refresh[REFRESH_PROC_DISKSTATS]++;
		need_refresh[CLUSTER_PARTITIONS]++;
	    }
	    else if (!(item == 48 && cluster == CLUSTER_STAT))	/* hz */
		need_refresh[cluster]++;
	    /* disk.{dev,dm,md,partitions}.capacity is in /proc/partitions */
	    if (is_capacity_metric(cluster, item))
		need_refresh[REFRESH_PROC_PARTITIONS]++;
	    /* In 2.6 kernels, swap.{pagesin,pagesout} are in /proc/vmstat */
	    if (_pm_have_proc_vmstat && cluster == CLUSTER_STAT) {
		if (item >= 8 && item <= 11)
		    need_refresh[CLUSTER_VMSTAT]++;
	    }
	    break;

	case CLUSTER_PARTITIONS:
	    if (is_capacity_metric(cluster, item))
		need_refresh[REFRESH_PROC_PARTITIONS]++;
	    need_refresh[REFRESH_PROC_DISKSTATS]++;
	    need_refresh[cluster]++;
	    break;

	case CLUSTER_ZRAM_DEVICES:
	case CLUSTER_ZRAM_IO_STAT:
	case CLUSTER_ZRAM_MM_STAT:
	case CLUSTER_ZRAM_BD_STAT:
	    need_refresh[cluster]++;
	    need_refresh[CLUSTER_ZRAM_DEVICES]++;
	    need_refresh[REFRESH_PROC_DISKSTATS]++;
	    break;

	case CLUSTER_WWID:
	    need_refresh[cluster]++;
	    need_refresh[REFRESH_PROC_DISKSTATS]++;
	    break;

	case CLUSTER_CPUINFO:
	case CLUSTER_INTERRUPT_LINES:
	case CLUSTER_INTERRUPT_OTHER:
	case CLUSTER_INTERRUPTS:
	case CLUSTER_SOFTIRQS_TOTAL:
	case CLUSTER_SOFTIRQS:
	case CLUSTER_SYSFS_DEVICES:
	case CLUSTER_NET_SOFTNET:
	    need_refresh[cluster]++;
	    need_refresh[CLUSTER_STAT]++;
	    break;

	case CLUSTER_NUMA_MEMINFO:
	    need_refresh[cluster]++;
	    need_refresh[CLUSTER_MEMINFO]++;
	    break;

	case CLUSTER_NET_ALL:
	case CLUSTER_NET_DEV:
	    need_refresh[cluster]++;
	    need_refresh[CLUSTER_NET_DEV]++;
	    switch (item) {
	    case 21:	/* network.interface.mtu */
		need_refresh[REFRESH_NET_MTU]++;
		break;
	    case 22:	/* network.interface.speed */
	    case 23:	/* network.interface.baudrate */
		need_refresh[REFRESH_NET_SPEED]++;
		break;
	    case 24:	/* network.interface.duplex */
		need_refresh[REFRESH_NET_DUPLEX]++;
		break;
	    case 25:	/* network.interface.up */
		need_refresh[REFRESH_NET_LINKUP]++;
		break;
	    case 26:	/* network.interface.running */
		need_refresh[REFRESH_NET_RUNNING]++;
		break;
	    case 28:	/* network.interface.wireless */
		need_refresh[REFRESH_NET_WIRELESS]++;
		break;
	    case 29:	/* network.interface.type */
		need_refresh[REFRESH_NET_TYPE]++;
		break;
	    }
	    break;

	case CLUSTER_NET_ADDR:
	    need_refresh[cluster]++;
	    switch (item) {
	    case 0:	/* network.interface.ipv4_addr */
		need_refresh[REFRESH_NETADDR_INET]++;
		break;
	    case 1:	/* network.interface.ipv6_addr */
	    case 2:	/* network.interface.ipv6_scope */
		need_refresh[REFRESH_NETADDR_IPV6]++;
		break;
	    case 3:	/* network.interface.hw_addr */
		need_refresh[REFRESH_NETADDR_HW]++;
		break;
	    }
	    break;

	default:
	    need_refresh[cluster]++;
	    break;
	}
    }

    if ((sts = linux_refresh(pmda, need_refresh, pmda->e_context)) < 0)
	return sts;
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
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
	pmNoMem("grow_ctxtab", (ctx+1)*sizeof(ctxtab[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    while (num_ctx <= ctx) {
	memset(&ctxtab[num_ctx], 0, sizeof(perctx_t));
	num_ctx++;
    }
    memset(&ctxtab[ctx], 0, sizeof(perctx_t));
}

static void
linux_endContextCallBack(int ctx)
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
    int		id = -1;

    if (attr == PMDA_ATTR_USERID || attr == PMDA_ATTR_CONTAINER ) {
	if (ctx >= num_ctx)
	    linux_grow_ctxtab(ctx);
    }
    if (attr == PMDA_ATTR_USERID) {
	ctxtab[ctx].access.uid_flag = 1;
	ctxtab[ctx].access.uid = id = atoi(value);
    }
    if (attr == PMDA_ATTR_CONTAINER) {
	char	*name = len > 1 ? strndup(value, len) : 0;

	if (ctxtab[ctx].container.name)
	    free(ctxtab[ctx].container.name);
	if ((ctxtab[ctx].container.name = name) != NULL)
	    ctxtab[ctx].container.length = len;
	else
	    ctxtab[ctx].container.length = 0;
	ctxtab[ctx].container.netfd = -1;
	ctxtab[ctx].container.pid = 0;
    }
    return pmdaAttribute(ctx, attr, value, len, pmda);
}

static int
linux_labelInDom(pmInDom indom, pmLabelSet **lp)
{
    int			sts;

    switch (pmInDom_serial(indom)) {
    case CPU_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"cpu\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per cpu\"}");
	return 1;
    case MD_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"block\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per md device\"}");
	return 1;
    case DM_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"block\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per dm device\"}");
	return 1;
    case DISK_INDOM:
    case SCSI_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"block\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per disk\"}");
	return 1;
    case PARTITIONS_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"block\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per partition\"}");
	return 1;
    case NODE_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"numa_node\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per numa_node\"}");
	return 1;
    case NET_DEV_INDOM:
    case NET_ADDR_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"interface\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per interface\"}");
	return 1;
    case SWAPDEV_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":[\"block\",\"memory\"]}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per swap device\"}");
	return 1;
    case SLAB_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"memory\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per slab\"}");
	return 1;
    case ZONEINFO_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":[\"memory\",\"numa_node\"]}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per zone per numa_node\"}");
	return 1;
    case BUDDYINFO_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":[\"memory\",\"numa_node\"]}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per buddy per numa_node\"}");
	return 1;
    case INTERRUPT_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"irq\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per irq\"}");
	return 1;
    case INTERRUPT_CPU_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":[\"irq\",\"cpu\"]}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per cpu, per irq\"}");
	return 1;
    case SOFTIRQ_INDOM:
	pmdaAddLabels(lp, "{\"indom_name\":\"per softirq\"}");
	return 1;
    case SOFTIRQ_CPU_INDOM:
	pmdaAddLabels(lp, "{\"device_type\":\"cpu\"}");
	pmdaAddLabels(lp, "{\"indom_name\":\"per cpu, per softirq\"}");
	return 1;
    default:
	sts = 0;
	break;
    }
    return sts;
}

static int
linux_labelItem(pmID pmid, pmLabelSet **lp)
{
    unsigned int	cluster = pmID_cluster(pmid);
    unsigned int	item = pmID_item(pmid);

    switch (cluster) {
    case CLUSTER_STAT:
	if (item >= 62 && item <= 71)	/* kernel.pernode.cpu */
	    return pmdaAddLabels(lp, "{\"device_type\":[\"numa_node\",\"cpu\"]}");
	else if ((item >= 20 && item <= 23) || /* kernel.all.cpu */
	    (item >= 53 && item <= 55))
	    return pmdaAddLabels(lp, "{\"device_type\":\"cpu\"}");
	else switch (item) {
	case 77:					/* kernel.pernode.cpu */
	case 85:
	case 86:
	    return pmdaAddLabels(lp, "{\"device_type\":[\"numa_node\",\"cpu\"]}");
	case 34:					/* kernel.all.cpu */
	case 35:
	case 60:
	case 78:
	case 81:
	case 82:
	    return pmdaAddLabels(lp, "{\"device_type\":\"cpu\"}");
	}
	break;

    default:
	break;
    }
    return 0;
}

static int
linux_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    zoneinfo_entry_t	*info;
    zoneprot_entry_t	*prot;
    interrupt_cpu_t	*cpuintr;
    interrupt_t		*intr;
    unsigned int	numanode, value;
    char		*name, *p;
    int			sts;

    if (indom == PM_INDOM_NULL)
	return 0;

    switch (pmInDom_serial(indom)) {
    case CPU_INDOM:
	return pmdaAddLabels(lp, "{\"cpu\":%u}", inst);

    case DISK_INDOM:
    case DM_INDOM:
    case MD_INDOM:
    case PARTITIONS_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, NULL);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	return pmdaAddLabels(lp, "{\"device_name\":\"%s\"}", name);

    case NET_DEV_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, NULL);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	return pmdaAddLabels(lp, "{\"interface\":\"%s\"}", name);

    case NODE_INDOM:
	return pmdaAddLabels(lp, "{\"numa_node\":%u}", inst);

    case BUDDYINFO_INDOM:
	if (inst >= proc_buddyinfo.nbuddys)
	    return PM_ERR_INST;
	numanode = atoi(proc_buddyinfo.buddys[inst].node_name);
	value = proc_buddyinfo.buddys[inst].order;
	return pmdaAddLabels(lp,
			"{\"numa_node\":%u,\"zone\":\"%s\",\"order\":%u}",
			numanode, proc_buddyinfo.buddys[inst].zone_name,
			value);

    case ZONEINFO_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&info);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	if (info->zone[0] == '\0')
	    return pmdaAddLabels(lp, "{\"numa_node\":%u}", info->node);
	return pmdaAddLabels(lp, "{\"numa_node\":%u,\"zone\":\"%s\"}",
			info->node, info->zone);

    case ZONEINFO_PROTECTION_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&prot);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	return pmdaAddLabels(lp,
			"{\"numa_node\":%u,\"zone\":\"%s\",\"lowmem_reserved\":%u}",
			prot->node, prot->zone, prot->lowmem);

    case INTERRUPT_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&intr);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	return pmdaAddLabels(lp, "{\"irq\":\"%s\",\"irqtext\":\"%s\"}",
			name, intr->label);

    case INTERRUPT_CPU_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&cpuintr);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	p = strchr(name, ':');
	return pmdaAddLabels(lp, "{\"cpu\":%u,\"irq\":\"%.*s\",\"irqtext\":\"%s\"}",
			cpuintr->cpuid, (int)(p - name), name, cpuintr->row->label);

    case SOFTIRQ_CPU_INDOM:
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&cpuintr);
	if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	    return 0;
	p = strchr(name, ':');
	return pmdaAddLabels(lp, "{\"cpu\":%u, \"softirq\":\"%.*s\"}",
			cpuintr->cpuid, (int)(p - name), name);

    default:
	break;
    }
    return 0;
}

static int
linux_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    int		sts;

    switch (type) {
    case PM_LABEL_INDOM:
	if ((sts = linux_labelInDom((pmInDom)ident, lpp)) < 0)
	    return sts;
	break;
    case PM_LABEL_ITEM:
	if ((sts = linux_labelItem((pmID)ident, lpp)) < 0)
	    return sts;
	break;
    default:
	break;
    }
    return pmdaLabel(ident, type, lpp, pmda);
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
linux_strings_lookup(int pindex)
{
    char *value;
    pmInDom dict = INDOM(STRINGS_INDOM);

    if (pmdaCacheLookup(dict, pindex, &value, NULL) == PMDA_CACHE_ACTIVE)
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

    /* optional overrides of some globals for testing */
    if ((envpath = getenv("LINUX_HERTZ")) != NULL) {
	/*
	 * If $LINUX_HERTZ is set, this is a QA setting that
	 * pretends the clock rate is $LINUX_HERTZ (assumed to
	 * match the contents of a QA stats tarball), rather than
	 * the value from sysconf() on the running system.
	 */
	linux_test_mode |= LINUX_TEST_MODE;
	hz = atoi(envpath);
    } else
	hz = sysconf(_SC_CLK_TCK);
    if ((envpath = getenv("LINUX_NCPUS")) != NULL) {
	/*
	 * If $LINUX_NCPUS is set, this is a QA setting that
	 * pretends there are $LINUX_NCPUS CPUs (assumed to match
	 * the contents of a QA stats tarball), rather than the
	 * value from sysconf() on the running system.
	 */
	linux_test_mode |= (LINUX_TEST_MODE|LINUX_TEST_NCPUS);
	_pm_ncpus = atoi(envpath);
    } else
	_pm_ncpus = sysconf(_SC_NPROCESSORS_CONF);
    if ((envpath = getenv("LINUX_NNODES")) != NULL) {
	/*
	 * If $LINUX_NNODES is set, this is a QA setting that
	 * forces all CPUs into node0 (the value of $LINUX_NNODES
	 * is ignored).
	 */
	linux_test_mode |= (LINUX_TEST_MODE|LINUX_TEST_NNODES);
    }
    if ((envpath = getenv("LINUX_PAGESIZE")) != NULL) {
	/*
	 * If $LINUX_PAGESIZE is set, this is a QA setting that
	 * pretends the page size is $LINUX_PAGESIZE (assumed to
	 * match the contents of a QA stats tarball), rather than
	 * the value from getpagesize() on the running system.
	 */
	linux_test_mode |= LINUX_TEST_MODE;
	_pm_pageshift = ffs(atoi(envpath)) - 1;
    } else
	_pm_pageshift = ffs(getpagesize()) - 1;
    if ((envpath = getenv("LINUX_STATSPATH")) != NULL) {
	/*
	 * If $LINUX_STATSPATH is set, this is a QA setting that
	 * points to the root directory holding stats files
	 * unpacked from a QA stats tarball, rather than "/".
	 */
	linux_test_mode |= (LINUX_TEST_MODE|LINUX_TEST_STATSPATH);
	linux_statspath = envpath;
    }
    if ((envpath = getenv("LINUX_MDADM")) != NULL) {
	/*
	 * If $LINUX_MDADM is set, this is a QA setting that
	 * points an alternative version of the /sbin/mdadm
	 * program used to extract MD RAID stats.
	 */
	linux_test_mode |= LINUX_TEST_MODE;
	linux_mdadm = envpath;
    }
    if ((envpath = getenv("LINUX_ACCESS")) != NULL)
	all_access = atoi(envpath);

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "linux DSO", helppath);
    }

    if (dp->status != 0)
	return;

    pmdaSetCommFlags(dp, PMDA_FLAG_AUTHORIZE|PMDA_FLAG_CONTAINER);

    dp->version.seven.instance = linux_instance;
    dp->version.seven.fetch = linux_fetch;
    dp->version.seven.text = linux_text;
    dp->version.seven.pmid = linux_pmid;
    dp->version.seven.name = linux_name;
    dp->version.seven.children = linux_children;
    dp->version.seven.attribute = linux_attribute;
    dp->version.seven.label = linux_label;
    pmdaSetLabelCallBack(dp, linux_labelCallBack);
    pmdaSetEndContextCallBack(dp, linux_endContextCallBack);
    pmdaSetFetchCallBack(dp, linux_fetchCallBack);

    proc_buddyinfo.indom = &indomtab[BUDDYINFO_INDOM];

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
	if (pmID_cluster(metrictab[i].m_desc.pmid) == CLUSTER_STAT) {
	    switch (pmID_item(metrictab[i].m_desc.pmid)) {
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
	    case 81:	/* kernel.all.cpu.guest_nice */
	    case 82:	/* kernel.all.cpu.vnice */
	    case 61:	/* kernel.percpu.cpu.guest */
	    case 76:	/* kernel.percpu.cpu.vuser */
	    case 83:	/* kernel.percpu.cpu.guest_nice */
	    case 84:	/* kernel.percpu.cpu.vnice */
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
	    case 85:	/* kernel.pernode.cpu.guest_nice */
	    case 86:	/* kernel.pernode.cpu.vnice */
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
			    pmID_cluster(metrictab[i].m_desc.pmid),
			    pmID_item(metrictab[i].m_desc.pmid));
    }

    nindoms = sizeof(indomtab)/sizeof(indomtab[0]);
    nmetrics = sizeof(metrictab)/sizeof(metrictab[0]);

    proc_vmstat_init();

    rootfd = pmdaRootConnect(NULL);
    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtab, nindoms, metrictab, nmetrics);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);
}

pmLongOptions	longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "no-access-checks", 0, 'A', 0, "no access checks will be performed (insecure, beware!)" },
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

pmdaOptions	opts = {
    .short_options = "AD:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			c, sep = pmPathSeparator();
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];
    char		*username = "root";

    _isDSO = 0;
    pmSetProgname(argv[0]);

    pmsprintf(helppath, sizeof(helppath), "%s%c" "linux" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), LINUX, "linux.log", helppath);

    while ((c = pmdaGetOptions(argc, argv, &opts, &dispatch)) != EOF) {
	switch (c) {
	case 'A':
	    all_access = 1;
	    break;
	}
    }

    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    pmSetProcessIdentity(username);

    linux_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
