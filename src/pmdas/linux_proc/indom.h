/*
 * Copyright (c) 2012-2014 Red Hat.
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

/*
 * indom serial numbers ... to manage the indom migration after the
 * linux -> linux + proc PMDAs split these needed to match the enum
 * assigned values for *_INDOM from the linux PMDA.  Consequently,
 * the proc indom table is permanently sparse.
 */
#define STRINGS_INDOM		 1 /* - fake indom, string hash */
#define UIDNAME_INDOM		 2 /* - fake indom, uid:name hash */
#define GIDNAME_INDOM		 3 /* - fake indom, gid:name hash */
#define PROC_INDOM		 9 /* - processes */
#define DISK_INDOM		11 /* - internal-only (user device names) */
#define DEVT_INDOM		12 /* - internal-only (major:minor names) */
#define CPU_INDOM		13 /* - internal-only (user-visible CPUs) */

#define CGROUP2_INDOM		16 /* - control group v2 groups */
#define CGROUP2_PERDEV_INDOM	17 /* - control group v2 groups::device */

#define CGROUP_CPUSET_INDOM	20 /* - control group cpuset, groups */
#define CGROUP_CPUACCT_INDOM	21 /* - control group cpuacct, groups */
#define CGROUP_PERCPUACCT_INDOM	22 /* - control group cpuacct, groups::cpu */
#define CGROUP_CPUSCHED_INDOM	23 /* - control group cpusched, groups */
#define CGROUP_MEMORY_INDOM	24 /* - control group memory, groups */
#define CGROUP_NETCLS_INDOM	25 /* - control group netclassifier, groups */
#define CGROUP_BLKIO_INDOM	26 /* - control group blkio, groups */
#define CGROUP_PERDEVBLKIO_INDOM 27 /* - control group blkio, groups::device */

#define CGROUP_SUBSYS_INDOM	37 /* - control group subsystems */
#define CGROUP_MOUNTS_INDOM	38 /* - control group mounts */

#define HOTPROC_INDOM		39 /* - hot procs */
#define ACCT_INDOM		40 /* - acct */

#define MIN_INDOM  9		/* first indom number we use here */
#define NUM_INDOMS 41		/* one more than highest indom number we use here */

extern pmInDom proc_indom(int);
#define INDOM(i) proc_indom(i)

/*
 * Optional path prefix for all stats files, used for testing.
 */
extern char *proc_statspath;
extern FILE *proc_statsfile(const char *, char *, int);

/* Generic globals setup during PMDA startup */
extern size_t _pm_system_pagesize;
extern long _pm_hertz;

/*
 * static string dictionary - one copy of oft-repeated strings;
 * implemented using STRINGS_INDOM and pmdaCache(3) routines.
 */
int proc_strings_insert(const char *);
char *proc_strings_lookup(int);
char *proc_uidname_lookup(int);
char *proc_gidname_lookup(int);

#endif /* _INDOM_H */
