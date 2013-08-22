/*
 * Copyright (c) 2013 Red Hat, Inc. All Rights Reserved.
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
 * linux -> linux + xfs PMDAs split, these need to match the enum
 * assigned values for *_INDOM from the linux PMDA. Consequently,
 * the xfs indom table is sparse.
 */
#define FILESYS_INDOM		5  /* mounted filesystems */
#define QUOTA_PRJ_INDOM		16 /* project quota */

#define MIN_INDOM 		5  /* first indom number we use here */
#define NUM_INDOMS		17 /* one more than highest indom number used */

#endif /* _INDOM_H */
