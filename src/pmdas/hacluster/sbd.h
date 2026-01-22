/*
 * HA Cluster SBD statistics.
 *
 * Copyright (c) 2020 - 2026 Red Hat.
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

#ifndef SBD_H
#define SBD_H

enum {
	SBD_DEVICE_PATH = 0,
	SBD_DEVICE_STATUS,
	SBD_DEVICE_TIMEOUT_MSGWAIT,
	SBD_DEVICE_TIMEOUT_ALLOCATE,
	SBD_DEVICE_TIMEOUT_LOOP,
	SBD_DEVICE_TIMEOUT_WATCHDOG,
	NUM_SBD_DEVICE_STATS
};

struct sbd_device {
	char		path[256];
	char		status[11];
	uint32_t	msgwait;
	uint32_t	allocate;
	uint32_t	loop;
	uint32_t	watchdog;
};

extern int hacluster_sbd_device_fetch(int, struct sbd_device *, pmAtomValue *);
extern int hacluster_sbd_device_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_sbd_device(const char *, struct sbd_device *);

extern void sbd_stats_setup(void);

#endif /* SBD_H */
