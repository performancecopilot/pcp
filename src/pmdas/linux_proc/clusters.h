/*
 * Copyright (c) 2013 Red Hat.
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
 * fetch cluster numbers ... to manage the PMID migration after the
 * linux -> linux + proc PMDAs split, these need to match the enum
 * assigned values for CLUSTER_* from the linux PMDA.
 */
#define CLUSTER_PID_STAT	 8 /*  /proc/<pid>/stat */
#define CLUSTER_PID_STATM	 9 /*  /proc/<pid>/statm + /proc/<pid>/maps */
#define CLUSTER_CONTROL		10 /* instance + value fetch control metrics */
#define CLUSTER_PID_CGROUP	11 /* /proc/<pid>/cgroup */
#define CLUSTER_PID_LABEL	12 /* /proc/<pid>/attr/current (label) */
#define CLUSTER_PROC_RUNQ	13 /* number of processes in various states */
#define CLUSTER_PID_STATUS	24 /* /proc/<pid>/status */
#define CLUSTER_PID_SCHEDSTAT	31 /* /proc/<pid>/schedstat */
#define CLUSTER_PID_IO		32 /* /proc/<pid>/io */
#define CLUSTER_CGROUP_SUBSYS	37 /* /proc/cgroups control group subsystems */
#define CLUSTER_CGROUP_MOUNTS	38 /* /proc/mounts active control groups */
#define CLUSTER_CPUSET_GROUPS	39 /* cpuset control groups */
#define CLUSTER_CPUACCT_GROUPS	41 /* cpu accounting control groups */
#define CLUSTER_CPUSCHED_GROUPS	43 /* scheduler control groups */
#define CLUSTER_MEMORY_GROUPS	45 /* memory control groups */
#define CLUSTER_NET_CLS_GROUPS	47 /* network classification control groups */
#define CLUSTER_PID_FD		51 /* /proc/<pid>/fd */

#define MIN_CLUSTER  8		/* first cluster number we use here */
#define NUM_CLUSTERS 52		/* one more than highest cluster number used */

#endif /* _CLUSTERS_H */
