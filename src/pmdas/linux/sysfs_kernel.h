/*
 * Linux sysfs_kernel cluster
 *
 * Copyright (c) 2009,2023-2024 Red Hat.
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
#ifndef SYSFS_KERNEL_H
#define SYSFS_KERNEL_H

typedef struct {
	/* /sys/kernel/uevent_seqnum */
	uint64_t	uevent_seqnum;
	int		valid_uevent_seqnum;

	/* /sys/module/zswap */
	uint32_t	zswap_max_pool_percent;
	char		zswap_enabled[4];

	/* /sys/kernel/debug/vmmemctl */
	uint64_t	vmmemctl_current;
	uint64_t	vmmemctl_target;

	/* /sys/kernel/debug/hv-balloon */
	uint32_t	hv_balloon_state;
	uint32_t	hv_balloon_pagesize;
	uint64_t	hv_balloon_added;
	uint64_t	hv_balloon_onlined;
	uint64_t	hv_balloon_ballooned;
	uint64_t	hv_balloon_total_committed;
} sysfs_kernel_t;

/* refresh sysfs_kernel */
extern int refresh_sysfs_kernel(sysfs_kernel_t *, int *);

#endif /* SYSFS_KERNEL_H */
