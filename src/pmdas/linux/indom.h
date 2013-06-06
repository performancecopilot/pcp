/*
 * Copyright (c) 2005,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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

#ifndef _INDOM_H
#define _INDOM_H

enum {
	CPU_INDOM = 0,		/* 0 - percpu */
	DISK_INDOM,		/* 1 - disks */
	LOADAVG_INDOM,		/* 2 - 1, 5, 15 minute load averages */
	NET_DEV_INDOM,		/* 3 - network interfaces */
	PROC_INTERRUPTS_INDOM,	/* 4 - interrupt lines -> proc PMDA */
	FILESYS_INDOM,		/* 5 - mounted bdev filesystems */
	SWAPDEV_INDOM,		/* 6 - swap devices */
	NFS_INDOM,		/* 7 - nfs operations */
	NFS3_INDOM,		/* 8 - nfs v3 operations */
	PROC_PROC_INDOM,	/* 9 - processes */
	PARTITIONS_INDOM, 	/* 10 - disk partitions */
	SCSI_INDOM,		/* 11 - scsi devices */
	SLAB_INDOM,		/* 12 - kernel slabs */
	IB_INDOM,		/* 13 - deprecated: do not re-use */
	NFS4_CLI_INDOM,		/* 14 - nfs v4 client operations */
	NFS4_SVR_INDOM,		/* 15 - nfs n4 server operations */
	QUOTA_PRJ_INDOM,	/* 16 - project quota */
	NET_INET_INDOM,		/* 17 - inet addresses */
	TMPFS_INDOM,		/* 18 - tmpfs mounts */
	NODE_INDOM,		/* 19 - NUMA nodes */
	PROC_CGROUP_SUBSYS_INDOM,	/* 20 - control group subsystems -> proc PMDA */
	PROC_CGROUP_MOUNTS_INDOM,	/* 21 - control group mounts -> proc PMDA */
	LV_INDOM,               /* 22 - lvm devices */

	NUM_INDOMS		/* one more than highest numbered cluster */
};

#define INDOM(x) (indomtab[x].it_indom)
extern pmdaIndom indomtab[];

#endif /* _INDOM_H */
