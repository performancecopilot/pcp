/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2005,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
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

#ifndef _CLUSTERS_H
#define _CLUSTERS_H

/*
 * fetch cluster numbers
 */
enum {
	CLUSTER_STAT = 0,	/*  0 /proc/stat */
	CLUSTER_MEMINFO,	/*  1 /proc/meminfo */
	CLUSTER_LOADAVG,	/*  2 /proc/loadavg */
	CLUSTER_NET_DEV,	/*  3 /proc/net/dev */
	CLUSTER_INTERRUPTS,	/*  4 /proc/interrupts */
	CLUSTER_FILESYS,	/*  5 /proc/mounts + statfs */
	CLUSTER_SWAPDEV,	/*  6 /proc/swaps */
	CLUSTER_NET_NFS,	/*  7 /proc/net/rpc/nfs + /proc/net/rpc/nfsd */
	PROC_PID_STAT,		/*  8 /proc/<pid>/stat -> proc PMDA */
	PROC_PID_STATM,		/*  9 /proc/<pid>/statm + /proc/<pid>/maps -> proc PMDA */
	CLUSTER_PARTITIONS,	/* 10 /proc/partitions */
	CLUSTER_NET_SOCKSTAT,	/* 11 /proc/net/sockstat */
	CLUSTER_KERNEL_UNAME,	/* 12 uname() system call */
	PROC_PROC_RUNQ,		/* 13 number of processes in various states -> proc PMDA */
	CLUSTER_NET_SNMP,	/* 14 /proc/net/snmp */
	CLUSTER_SCSI,		/* 15 /proc/scsi/scsi */
	CLUSTER_XFS,		/* 16 /proc/fs/xfs/stat -> xfs PMDA */
	CLUSTER_XFSBUF,		/* 17 /proc/fs/pagebuf/stat -> xfs PMDA */
	CLUSTER_CPUINFO,	/* 18 /proc/cpuinfo */
	CLUSTER_NET_TCP,	/* 19 /proc/net/tcp */
	CLUSTER_SLAB,		/* 20 /proc/slabinfo */
	CLUSTER_SEM_LIMITS,	/* 21 semctl(IPC_INFO) system call */
	CLUSTER_MSG_LIMITS,	/* 22 msgctl(IPC_INFO) system call */
	CLUSTER_SHM_LIMITS,	/* 23 shmctl(IPC_INFO) system call */
	PROC_PID_STATUS,	/* 24 /proc/<pid>/status -> proc PMDA */
	CLUSTER_NUSERS,		/* 25 number of users */
	CLUSTER_UPTIME,		/* 26 /proc/uptime */
	CLUSTER_VFS,		/* 27 /proc/sys/fs */
	CLUSTER_VMSTAT,		/* 28 /proc/vmstat */
	CLUSTER_IB,		/* deprecated: do not re-use 29 infiniband */
	CLUSTER_QUOTA,		/* 30 quotactl() -> xfs PMDA */
	PROC_PID_SCHEDSTAT,	/* 31 /proc/<pid>/schedstat -> proc PMDA */
	PROC_PID_IO,		/* 32 /proc/<pid>/io -> proc PMDA */
	CLUSTER_NET_ADDR,	/* 33 /proc/net/dev and ioctl(SIOCGIFCONF) */
	CLUSTER_TMPFS,		/* 34 /proc/mounts + statfs (tmpfs only) */
	CLUSTER_SYSFS_KERNEL,	/* 35 /sys/kernel metrics */
	CLUSTER_NUMA_MEMINFO,	/* 36 /sys/devices/system/node* NUMA memory */
	PROC_CGROUP_SUBSYS,	/* 37 /proc/cgroups control group subsystems -> proc PMDA */
	PROC_CGROUP_MOUNTS,	/* 38 /proc/mounts active control groups -> proc PMDA */
	PROC_CPUSET_GROUPS,	/* 39 cpuset control groups -> proc PMDA */
	PROC_CPUSET_PROCS,	/* 40 cpuset control group processes -> proc PMDA */
	PROC_CPUACCT_GROUPS,	/* 41 cpu accounting control groups -> proc PMDA */
	PROC_CPUACCT_PROCS,	/* 42 cpu accounting group processes -> proc PMDA */
	PROC_CPUSCHED_GROUPS,	/* 43 scheduler control groups -> proc PMDA */
	PROC_CPUSCHED_PROCS,	/* 44 scheduler group processes -> proc PMDA */
	PROC_MEMORY_GROUPS,	/* 45 memory control groups -> proc PMDA */
	PROC_MEMORY_PROCS,	/* 46 memory group processes -> proc PMDA */
	PROC_NET_CLS_GROUPS,	/* 47 network classification control groups -> proc PMDA */
	PROC_NET_CLS_PROCS,	/* 48 network classification group processes -> proc PMDA */
	CLUSTER_INTERRUPT_LINES,/* 49 /proc/interrupts percpu interrupts */
	CLUSTER_INTERRUPT_OTHER,/* 50 /proc/interrupts percpu interrupts */
	PROC_PID_FD,		/* 51 /proc/<pid>/fd -> proc PMDA */
	CLUSTER_LV,		/* deprecated: do not re-use 52 */
	CLUSTER_NET_NETSTAT,    /* 53 /proc/net/netstat */
	CLUSTER_DM,		/* 54 disk.dm.* */
	CLUSTER_SYSFS_DEVICES,	/* 55 /sys/devices metrics */
	CLUSTER_SHM_INFO,       /* 56 shmctl(SHM_INFO) system call */

	NUM_CLUSTERS		/* one more than highest numbered cluster */
};

/*
 * Extra refresh array indices for fine-grained (within-cluster) refresh
 */
enum {
	REFRESH_NET_MTU = NUM_CLUSTERS,
	REFRESH_NET_SPEED,
	REFRESH_NET_DUPLEX,
	REFRESH_NET_LINKUP,
	REFRESH_NET_RUNNING,

	NUM_REFRESHES		/* one more than highest refresh index */
};

#endif /* _CLUSTERS_H */
