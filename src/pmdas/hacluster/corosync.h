/*
 * HA Cluster Corosync statistics.
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

#ifndef COROSYNC_H
#define COROSYNC_H

enum {
	COROSYNC_MEMBER_VOTES_VOTES = 0,
	COROSYNC_MEMBER_VOTES_LOCAL,
	COROSYNC_MEMBER_VOTES_NODE_ID,
	NUM_COROSYNC_MEMBER_STATS
};

enum {
	COROSYNC_QUORATE = 0,
	COROSYNC_QUORUM_VOTES_EXPECTED_VOTES,
	COROSYNC_QUORUM_VOTES_HIGHEST_EXPECTED,
	COROSYNC_QUORUM_VOTES_TOTAL_VOTES,
	COROSYNC_QUORUM_VOTES_QUORUM,
	COROSYNC_RING_ERRORS,
	NUM_COROSYNC_GLOBAL_STATS
};

enum {
	COROSYNC_RINGS_STATUS = 0,
	COROSYNC_RINGS_ADDRESS,
	COROSYNC_RINGS_NODE_ID,
	COROSYNC_RINGS_NUMBER,
	COROSYNC_RINGS_RING_ID,
	NUM_COROSYNC_RINGS_STATS
};

struct corosync_node {
	uint32_t	votes;
	uint8_t		local;
	uint64_t	node_id;
};

struct corosync_global {
	uint32_t	quorate;
	uint32_t	expected_votes;
	uint32_t	highest_expected;
	uint32_t	total_votes;
	uint32_t	quorum;
	uint32_t	ring_errors;
};

struct corosync_ring {
	uint8_t	status;
	char	address[40];
	uint64_t	node_id;
	uint32_t	number;
	char	ring_id[44];
};

extern int hacluster_corosync_node_fetch(int, struct corosync_node *, pmAtomValue *);
extern int hacluster_refresh_corosync_node(const char *, struct corosync_node *);
extern int hacluster_corosync_node_instance_refresh(void);

extern int hacluster_corosync_global_fetch(int, pmAtomValue *);
extern int hacluster_refresh_corosync_global();

extern int hacluster_corosync_ring_fetch(int, struct corosync_ring *, pmAtomValue *);
extern int hacluster_corosync_ring_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_corosync_ring(const char *, struct corosync_ring *);
extern int hacluster_corosync_ring_instance_refresh(void);

extern void corosync_stats_setup(void);

#endif /* COROSYNC_H */
