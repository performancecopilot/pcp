/*
 * Copyright (c) 2013-2014 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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
	STRINGS_INDOM,		/* 13 - string dictionary */
	NFS4_CLI_INDOM,		/* 14 - nfs v4 client operations */
	NFS4_SVR_INDOM,		/* 15 - nfs n4 server operations */
	QUOTA_PRJ_INDOM,	/* 16 - project quota -> xfs PMDA */
	NET_ADDR_INDOM,		/* 17 - inet/ipv6 addresses */
	TMPFS_INDOM,		/* 18 - tmpfs mounts */
	NODE_INDOM,		/* 19 - NUMA nodes */
	PROC_CGROUP_SUBSYS_INDOM,	/* 20 - control group subsystems -> proc PMDA */
	PROC_CGROUP_MOUNTS_INDOM,	/* 21 - control group mounts -> proc PMDA */
	LV_INDOM_DEPRECATED,	/* deprecated 22 - lvm devices. do not re-use. Use DM_INDOM instead */
	ICMPMSG_INDOM,          /* 23 - icmp message types */
	DM_INDOM,		/* 24 - device mapper devices */

	NUM_INDOMS		/* one more than highest numbered cluster */
};

extern pmInDom linux_indom(int);
#define INDOM(i) linux_indom(i)

extern pmdaIndom *linux_pmda_indom(int);
#define PMDAINDOM(i) linux_pmda_indom(i)

/*  
 * Optional path prefix for all stats files, used for testing.
 */
extern char *linux_statspath;
extern FILE *linux_statsfile(const char *, char *, int);

/*
 * static string dictionary - one copy of oft-repeated strings;
 * implemented using STRINGS_INDOM and pmdaCache(3) routines.
 */
char *linux_strings_lookup(int);
int linux_strings_insert(const char *);

#endif /* _INDOM_H */
