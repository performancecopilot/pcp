/*
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

#define CPU_INDOM		0
#define DISK_INDOM		1
#define LOADAVG_INDOM		2
#define NET_DEV_INDOM		3
#define PROC_INTERRUPTS_INDOM	4
#define FILESYS_INDOM		5
#define SWAPDEV_INDOM		6
#define NFS_INDOM		7
#define NFS3_INDOM		8
#define PROC_INDOM		9
#define PARTITIONS_INDOM 	10
#define SCSI_INDOM		11
#define SLAB_INDOM		12
#define IB_INDOM		13	/* deprecated: do not re-use */
#define NFS4_CLI_INDOM		14
#define NFS4_SVR_INDOM		15
#define QUOTA_PRJ_INDOM		16
#define NET_INET_INDOM		17
#define TMPFS_INDOM		18
#define NODE_INDOM		19

#define INDOM(x) (indomtab[x].it_indom)
extern pmdaIndom indomtab[];

#endif /* _INDOM_H */
