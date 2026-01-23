/*
 * HA Cluster DRDB statistics.
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

#ifndef DRBD_H
#define DRBD_H

enum {
	DRBD_JSON_HEAD = 0,
	DRBD_JSON_NODE,
	DRBD_JSON_DEVICE,
	DRBD_JSON_VOLUME
};

enum {
	DRBD_RESOURCE_RESOURCE = 0,
	DRBD_RESOURCE_ROLE,
	DRBD_RESOURCE_VOLUME,
	DRBD_RESOURCE_DISK_STATE,
	DRBD_RESOURCE_WRITTEN,
	DRBD_RESOURCE_READ,
	DRBD_RESOURCE_AL_WRITES,
	DRBD_RESOURCE_BM_WRITES,
	DRBD_RESOURCE_UPPER_PENDING,
	DRBD_RESOURCE_LOWER_PENDING,
	DRBD_RESOURCE_QUORUM,
	DRBD_RESOURCE_SPLIT_BRAIN,
	NUM_DRBD_RESOURCE_STATS
};

enum {
	DRBD_PEER_DEVICE_RESOURCE = 0,
	DRBD_PEER_DEVICE_PEER_NODE_ID,
	DRBD_PEER_DEVICE_PEER_ROLE,
	DRBD_PEER_DEVICE_VOLUME,
	DRBD_PEER_DEVICE_PEER_DISK_STATE,
	DRBD_PEER_DEVICE_CONNECTIONS_SYNC,
	DRBD_PEER_DEVICE_CONNECTIONS_RECEIVED,
	DRBD_PEER_DEVICE_CONNECTIONS_SENT,
	DRBD_PEER_DEVICE_CONNECTIONS_PENDING,
	DRBD_PEER_DEVICE_CONNECTIONS_UNACKED,
	NUM_DRBD_PEER_DEVICE_STATS
};

struct drbd_resource {
	char resource[128];
	char role[10];
	char volume[128];
	char disk_state[13];
	uint32_t read;
	uint32_t write;
	uint64_t al_writes;
	uint64_t bm_writes;
	uint64_t upper_pending;
	uint64_t lower_pending;
	uint8_t quorum;
	uint8_t split_brain;
};

struct drbd_peer_device {
	char resource[128];
	char peer_node_id[128];
	char peer_role[10];
	uint32_t volume;
	char peer_disk_state[13];
	float connections_sync;
	uint64_t connections_received;
	uint64_t connections_sent;
	uint32_t connections_pending;
	uint32_t connections_unacked;
};

extern int hacluster_drbd_resource_fetch(int, struct drbd_resource *, pmAtomValue *);
extern int hacluster_drbd_resource_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_drbd_resource(const char *, struct drbd_resource *);

extern int hacluster_drbd_peer_device_fetch(int, struct drbd_peer_device *, pmAtomValue *);
extern int hacluster_drbd_peer_device_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_drbd_peer_device(const char *, struct drbd_peer_device *);

extern void drbd_stats_setup(void);

#endif /* DRBD_H */
