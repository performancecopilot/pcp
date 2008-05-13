/*
 * Copyright (c) 2005 Silicon Graphics, Inc.  All Rights Reserved.
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
	CLUSTER_PID_STAT,	/*  8 /proc/<pid>/stat */
	CLUSTER_PID_STATM,	/*  9 /proc/<pid>/statm + /proc/<pid>/maps */
	CLUSTER_PARTITIONS,	/* 10 /proc/partitions */
	CLUSTER_NET_SOCKSTAT,	/* 11 /proc/net/sockstat */
	CLUSTER_KERNEL_UNAME,	/* 12 uname() system call */
	CLUSTER_PROC_RUNQ,	/* 13 number of processes in various states */
	CLUSTER_NET_SNMP,	/* 14 /proc/net/snmp */
	CLUSTER_SCSI,		/* 15 /proc/scsi/scsi */
	CLUSTER_XFS,		/* 16 /proc/fs/xfs/stat */
	CLUSTER_XFSBUF,		/* 17 (deprecated /proc/fs/pagebuf/stat) */
	CLUSTER_CPUINFO,	/* 18 /proc/cpuinfo */
	CLUSTER_NET_TCP,	/* 19 /proc/net/tcp */
	CLUSTER_SLAB,		/* 20 /proc/slabinfo */
	CLUSTER_SEM_LIMITS,	/* 21 semctl(IPC_INFO) system call */
	CLUSTER_MSG_LIMITS,	/* 22 msgctl(IPC_INFO) system call */
	CLUSTER_SHM_LIMITS,	/* 23 shmctl(IPC_INFO) system call */
	CLUSTER_PID_STATUS,	/* 24 /proc/<pid>/status */
	CLUSTER_NUSERS,		/* 25 number of users */
	CLUSTER_UPTIME,		/* 26 /proc/uptime */
	CLUSTER_VFS,		/* 27 /proc/sys/fs */
	CLUSTER_VMSTAT,		/* 28 /proc/vmstat */
	CLUSTER_IB,		/* 29 /sys/class/infiniband */
	CLUSTER_NET_INET,	/* 30 /proc/net/dev and ioctl(SIOCGIFCONF) */
	CLUSTER_PID_SCHEDSTAT,	/* 31 /proc/<pid>/schedstat */
	CLUSTER_PID_IO,		/* 32 /proc/<pid>/io */

	NUM_CLUSTERS		/* one more than highest numbered cluster */
};

#endif /* _CLUSTERS_H */
