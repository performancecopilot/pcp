/*
 * Copyright (c) 2013,2016 Red Hat.
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
 * PMID cluster values ... to manage the PMID migration after the
 * linux -> linux + xfs PMDAs split, these need to match the enum
 * assigned values for CLUSTER_* from the original Linux PMDA.
 */
#define CLUSTER_XFS	16 /* sysfs/procfs global stats */
#define CLUSTER_XFSBUF	17 /* sysfs/procfs global buffer stats */
#define CLUSTER_PERDEV	18 /* sysfs per-mounted-device stats */
#define CLUSTER_QUOTA	30 /* quotactl() */

#define MIN_CLUSTER	16 /* first cluster number we use here */
#define NUM_CLUSTERS	31 /* one more than highest cluster number used */

#endif /* _CLUSTERS_H */
