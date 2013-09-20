/*
 * Copyright (c) 2005,2007-2008 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat, Inc. All Rights Reserved.
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
 * linux -> linux + proc PMDAs split, these need to match the enum
 * assigned values for *_INDOM from the linux PMDA. Consequently,
 * the proc indom table is sparse.
 */
#define CPU_INDOM		 0 /* - percpu */
#define PROC_INDOM		 9 /* - processes */
#define STRINGS_INDOM		10 /* - fake indom, string hash */
#define CGROUP_SUBSYS_INDOM	20 /* - control group subsystems */
#define CGROUP_MOUNTS_INDOM	21 /* - control group mounts */

#define MIN_INDOM  0		/* first indom number we use here */
#define NUM_INDOMS 22		/* one more than highest indom number we use here */

/*
 * static string dictionary - one copy of oft-repeated strings;
 * implemented using STRINGS_INDOM and pmdaCache(3) routines.
 */
char *proc_strings_lookup(int);
int proc_strings_insert(const char *);

#endif /* _INDOM_H */
