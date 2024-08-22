/*
 * HA Cluster Pacemaker statistics.
 *
 * Copyright (c) 2020 - 2021 Red Hat.
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

#ifndef PACEMAKER_H
#define PACEMAKER_H

enum {
 	PACEMAKER_CONFIG_LAST_CHANGE = 0,
 	PACEMAKER_STONITH_ENABLED,
 	NUM_PACEMAKER_GLOBAL_STATS
};

enum {
	PACEMAKER_FAIL_COUNT = 0,
	PACEMAKER_MIGRATION_THRESHOLD,
	NUM_PACEMAKER_FAIL_STATS
};

enum {
	PACEMAKER_CONSTRAINTS_NODE =0,
	PACEMAKER_CONSTRAINTS_RESOURCE,
	PACEMAKER_CONSTRAINTS_ROLE,
	PACEMAKER_CONSTRAINTS_SCORE,
	NUM_PACEMAKER_CONSTRAINTS_STATS
};

enum {
	PACEMAKER_NODES_ONLINE = 0,
	PACEMAKER_NODES_STANDBY,
	PACEMAKER_NODES_STANDBY_ONFAIL,
	PACEMAKER_NODES_MAINTENANCE,
	PACEMAKER_NODES_PENDING,
	PACEMAKER_NODES_UNCLEAN,
	PACEMAKER_NODES_SHUTDOWN,
	PACEMAKER_NODES_EXPECTED_UP,
	PACEMAKER_NODES_DC,
	PACEMAKER_NODES_TYPE,
	NUM_PACEMAKER_NODES_STATS
};

enum {
	PACEMAKER_NODES_ATTRIB_VALUE = 0,
	NUM_PACEMAKER_NODE_ATTRIB_STATS
};

enum {
	PACEMAKER_RESOURCES_AGENT = 0,
	PACEMAKER_RESOURCES_CLONE,
	PACEMAKER_RESOURCES_GROUP,
	PACEMAKER_RESOURCES_MANAGED,
	PACEMAKER_RESOURCES_ROLE,
	PACEMAKER_RESOURCES_ACTIVE,
	PACEMAKER_RESOURCES_ORPHANED,
	PACEMAKER_RESOURCES_BLOCKED,
	PACEMAKER_RESOURCES_FAILED,
	PACEMAKER_RESOURCES_FAILURE_IGNORED,
	NUM_PACEMAKER_RESOURCES_STATS
};

struct pacemaker_global {
	uint64_t config_last_change;
	uint8_t stonith_enabled;
};

struct fail_count {
	uint64_t fail_count;
	uint64_t migration_threshold;
};

struct location_constraints {
	char node[128];
	char resource[128];
	char role[18];
	char score[10];
};

struct nodes {
	uint8_t online;
	uint8_t standby;
	uint8_t standby_on_fail;
	uint8_t maintenance;
	uint8_t pending;
	uint8_t unclean;
	uint8_t shutdown;
	uint8_t expected_up;
	uint8_t dc;
	char type[7];
};

struct attributes {
	char value[256];
};

struct resources {
	char agent[128];
	char clone[128];
	char group[128];
	uint8_t managed;
	char role[18];
	uint8_t active;
	uint8_t orphaned;
	uint8_t blocked;
	uint8_t failed;
	uint8_t failure_ignored;
};

extern int hacluster_pacemaker_global_fetch(int, pmAtomValue *);
extern int hacluster_refresh_pacemaker_global();

extern int hacluster_pacemaker_fail_fetch(int, struct fail_count *, pmAtomValue *);
extern int hacluster_refresh_pacemaker_fail(const char *, struct fail_count *);

extern int hacluster_pacemaker_constraints_fetch(int, struct location_constraints *, pmAtomValue *);
extern int hacluster_pacemaker_constraints_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_pacemaker_constraints(const char *, struct location_constraints *);

extern int hacluster_pacemaker_nodes_fetch(int, struct nodes *, pmAtomValue *);
extern int hacluster_refresh_pacemaker_nodes(const char *, struct nodes *);

extern int hacluster_pacemaker_node_attribs_fetch(int, struct attributes *, pmAtomValue *);
extern int hacluster_pacemaker_node_attribs_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_pacemaker_node_attribs(const char *, struct attributes *);

extern int hacluster_pacemaker_resources_fetch(int, struct resources *, pmAtomValue *);
extern int hacluster_pacemaker_resources_all_fetch(int, pmAtomValue *);
extern int hacluster_refresh_pacemaker_resources(const char *, struct resources *);

extern void pacemaker_stats_setup(void);

#endif /* PACEMAKER_H */
