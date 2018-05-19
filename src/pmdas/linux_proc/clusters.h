/*
 * Copyright (c) 2013-2014,2018 Red Hat.
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
#define CLUSTER_NETCLS_GROUPS	47 /* network classification control groups */
#define CLUSTER_BLKIO_GROUPS	49 /* block layer I/O control groups */
#define CLUSTER_PID_FD		51 /* /proc/<pid>/fd */

#define CLUSTER_HOTPROC_PID_STAT        52 /*  /proc/<pid>/stat */
#define CLUSTER_HOTPROC_PID_STATM       53 /*  /proc/<pid>/statm + /proc/<pid>/maps */
#define CLUSTER_HOTPROC_PID_CGROUP      54 /* /proc/<pid>/cgroup */
#define CLUSTER_HOTPROC_PID_LABEL       55 /* /proc/<pid>/attr/current (label) */
#define CLUSTER_HOTPROC_PID_STATUS      56 /* /proc/<pid>/status */
#define CLUSTER_HOTPROC_PID_SCHEDSTAT   57 /* /proc/<pid>/schedstat */
#define CLUSTER_HOTPROC_PID_IO          58 /* /proc/<pid>/io */
#define CLUSTER_HOTPROC_PID_FD          59 /* /proc/<pid>/fd */
#define CLUSTER_HOTPROC_GLOBAL		60 /* overall hotproc stats and controls*/
#define CLUSTER_HOTPROC_PRED      	61 /* derived hotproc metrics */

#define CLUSTER_PID_OOM_SCORE	62 /* /proc/<pid>/oom_score */
#define CLUSTER_HOTPROC_PID_OOM_SCORE   63 /* /proc/<pid>/oom_score */

#define MIN_CLUSTER  8		/* first cluster number we use here */
#define MAX_CLUSTER 64		/* one more than highest cluster number used */

#endif /* _CLUSTERS_H */
