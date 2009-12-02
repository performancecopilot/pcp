/*
 * Linux dev_udev cluster
 *
 * Copyright (c) 2009, Red Hat, Inc.  All Rights Reserved.
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
 */

#ifndef _DEV_UDEV_H
#define _DEV_UDEV_H

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <ctype.h>

typedef struct {
	int		valid;
	uint64_t	seqnum; /* /dev/.udev/uevent_seqnum */
	/* TODO queue length, event type counters and other metrics */
} dev_udev_t;

/* refresh dev_udev */
extern int refresh_dev_udev(dev_udev_t *);

#endif /* _DEV_UDEV_H */
