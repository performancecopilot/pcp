/*
 * High Available (HA) Cluster PMDA
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

#include "pmapi.h"
#include "pmda.h"
#include "domain.h"
#include "ctype.h"

#include "pmdahacluster.h"

static int _isDSO = 1; /* for local contexts */

static char *cibadmin_command;
static char *crm_mon_command;
static char *quorumtool_command;
static char *cfgtool_command;
static char *sbd_path;
static char *drbdsetup_command;

pmdaIndom indomtable[] = {
	{ .it_indom = PACEMAKER_FAIL_INDOM },
	{ .it_indom = PACEMAKER_CONSTRAINTS_INDOM },
	{ .it_indom = PACEMAKER_NODES_INDOM },
	{ .it_indom = PACEMAKER_NODE_ATTRIB_INDOM },
	{ .it_indom = PACEMAKER_RESOURCES_INDOM },
	{ .it_indom = COROSYNC_NODE_INDOM },
	{ .it_indom = COROSYNC_RING_INDOM },
	{ .it_indom = SBD_DEVICE_INDOM },
	{ .it_indom = DRBD_RESOURCE_INDOM },
	{ .it_indom = DRBD_PEER_DEVICE_INDOM },
};

#define INDOM(x) (indomtable[x].it_indom)

/*
 * All metrics supported by this PMDA - one table entry for each metric
 */
pmdaMetric metrictable[] = {
	/* PACEMAKER */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_GLOBAL, PACEMAKER_CONFIG_LAST_CHANGE),
		PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_GLOBAL, PACEMAKER_STONITH_ENABLED),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_FAIL, PACEMAKER_FAIL_COUNT),
		PM_TYPE_U64, PACEMAKER_FAIL_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_FAIL, PACEMAKER_MIGRATION_THRESHOLD),
		PM_TYPE_U64, PACEMAKER_FAIL_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_CONSTRAINTS, PACEMAKER_CONSTRAINTS_NODE),
		PM_TYPE_STRING, PACEMAKER_CONSTRAINTS_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_CONSTRAINTS, PACEMAKER_CONSTRAINTS_RESOURCE),
		PM_TYPE_STRING, PACEMAKER_CONSTRAINTS_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_CONSTRAINTS, PACEMAKER_CONSTRAINTS_ROLE),
		PM_TYPE_STRING, PACEMAKER_CONSTRAINTS_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_CONSTRAINTS, PACEMAKER_CONSTRAINTS_SCORE),
		PM_TYPE_STRING, PACEMAKER_CONSTRAINTS_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_ONLINE),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_STANDBY),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_STANDBY_ONFAIL),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_MAINTENANCE),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_PENDING),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_UNCLEAN),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_SHUTDOWN),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_EXPECTED_UP),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },	
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_DC),
		PM_TYPE_U32, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODES, PACEMAKER_NODES_TYPE),
		PM_TYPE_STRING, PACEMAKER_NODES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_NODE_ATTRIB, PACEMAKER_NODES_ATTRIB_VALUE),
		PM_TYPE_STRING, PACEMAKER_NODE_ATTRIB_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_AGENT),
		PM_TYPE_STRING, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_CLONE),
		PM_TYPE_STRING, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_GROUP),
		PM_TYPE_STRING, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_MANAGED),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_NODES_ATTRIB_VALUE),
		PM_TYPE_STRING, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_ROLE),
		PM_TYPE_STRING, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_ACTIVE),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_ORPHANED),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_BLOCKED),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_FAILED),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_PACEMAKER_RESOURCES, PACEMAKER_RESOURCES_FAILURE_IGNORED),
		PM_TYPE_U32, PACEMAKER_RESOURCES_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	/* COROSYNC */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_NODE, COROSYNC_MEMBER_VOTES_VOTES),
		PM_TYPE_U32, COROSYNC_NODE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_NODE, COROSYNC_MEMBER_VOTES_LOCAL),
		PM_TYPE_U32, COROSYNC_NODE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_NODE, COROSYNC_MEMBER_VOTES_NODE_ID),
		PM_TYPE_U32, COROSYNC_NODE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_QUORATE),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_QUORUM_VOTES_EXPECTED_VOTES),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_QUORUM_VOTES_HIGHEST_EXPECTED),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_QUORUM_VOTES_TOTAL_VOTES),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_QUORUM_VOTES_QUORUM),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_GLOBAL, COROSYNC_RING_ERRORS),
		PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_RING, COROSYNC_RINGS_STATUS),
		PM_TYPE_U32, COROSYNC_RING_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_RING, COROSYNC_RINGS_ADDRESS),
		PM_TYPE_STRING, COROSYNC_RING_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_RING, COROSYNC_RINGS_NODE_ID),
		PM_TYPE_U64, COROSYNC_RING_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_RING, COROSYNC_RINGS_NUMBER),
		PM_TYPE_U32, COROSYNC_RING_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_COROSYNC_RING, COROSYNC_RINGS_RING_ID),
		PM_TYPE_STRING, COROSYNC_RING_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	/* SBD */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_PATH),
		PM_TYPE_STRING, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_STATUS),
		PM_TYPE_STRING, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_TIMEOUT_MSGWAIT),
		PM_TYPE_U32, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_TIMEOUT_ALLOCATE),
		PM_TYPE_U32, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_TIMEOUT_LOOP),
		PM_TYPE_U32, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_SBD_DEVICE, SBD_DEVICE_TIMEOUT_WATCHDOG),
		PM_TYPE_U32, SBD_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	/* DRBD */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_RESOURCE),
		PM_TYPE_STRING, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_ROLE),
		PM_TYPE_STRING, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_VOLUME),
		PM_TYPE_STRING, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_DISK_STATE),
		PM_TYPE_STRING, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_WRITTEN),
		PM_TYPE_U32, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,PM_SPACE_KBYTE,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_READ),
		PM_TYPE_U32, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,PM_SPACE_KBYTE,0,PM_COUNT_ONE) } },		
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_AL_WRITES),
		PM_TYPE_U64, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_BM_WRITES),
		PM_TYPE_U64, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_UPPER_PENDING),
		PM_TYPE_U64, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_LOWER_PENDING),
		PM_TYPE_U64, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },	
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_QUORUM),
		PM_TYPE_U32, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_RESOURCE),
		PM_TYPE_STRING, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_PEER_NODE_ID),
		PM_TYPE_STRING, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_PEER_ROLE),
		PM_TYPE_STRING, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_VOLUME),
		PM_TYPE_U32, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_PEER_DISK_STATE),
		PM_TYPE_STRING, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_CONNECTIONS_SYNC),
		PM_TYPE_FLOAT, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_CONNECTIONS_RECEIVED),
		PM_TYPE_U64, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,PM_SPACE_KBYTE,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_CONNECTIONS_SENT),
		PM_TYPE_U64, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,PM_SPACE_KBYTE,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_CONNECTIONS_PENDING),
		PM_TYPE_U32, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_PEER_DEVICE, DRBD_PEER_DEVICE_CONNECTIONS_UNACKED),
		PM_TYPE_U32, DRBD_PEER_DEVICE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_DRBD_RESOURCE, DRBD_RESOURCE_SPLIT_BRAIN),
		PM_TYPE_U32, DRBD_RESOURCE_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },	
};

int
metrictable_size(void)
{
	return sizeof(metrictable)/sizeof(metrictable[0]);
}

int
hacluster_pacemaker_fail_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], instance_name[256], node_name[128], resource_name[127];
	int			found_node_history = 0, found_node_name = 0;
	FILE		*pf;
	pmInDom		indom = INDOM(PACEMAKER_FAIL_INDOM);

	pmsprintf(buffer, sizeof(buffer), "%s", crm_mon_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* First we need to check whether we are in <node_history> section*/
		if (strstr(buffer, "<node_history>")) {
			found_node_history = 1;
			continue;
		}

		/* Find the node name for our resource */
		if (strstr(buffer, "node name=") && found_node_history ) {
			sscanf(buffer, "\t<node name=\"%[^\"]\">", node_name);
			found_node_name = 1;
			continue;
		}

		/* Check for when we overrun to another node */
		if (strstr(buffer, "</node>")) {
			found_node_name = 0;
			continue;
		}

		/* Record our instance as node:resource-id and assign */
		if (found_node_history && found_node_name) {

			if (strstr(buffer, "resource_history id=")) {
				sscanf(buffer, "\t<resource_history id=\"%[^\"]", resource_name);

				/* 
				 * Assign indom based upon our resource_name:volume by joining our node_name
				 * with our volume number 
				 */
				snprintf(instance_name, sizeof(instance_name), "%s:%s", node_name, resource_name);

				struct  pacemaker_fail *fail;

				sts = pmdaCacheLookupName(indom, instance_name, NULL, (void **)&fail);
				if (sts == PM_ERR_INST || (sts >=0 && fail == NULL)) {
					fail = calloc(1, sizeof(struct pacemaker_fail));
					if (fail == NULL) {
						pclose(pf);
						return PM_ERR_AGAIN;
					}
				}
				else if (sts < 0)
					continue;

				pmdaCacheStore(indom, PMDA_CACHE_ADD, instance_name, (void *)fail);
			}
		}
	}
	pclose(pf);	
	return 0;
}

int
hacluster_pacemaker_contraints_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], constraint_name[256];
	int			found_constraints = 0;
	FILE		*pf;
	pmInDom		indom = INDOM(PACEMAKER_CONSTRAINTS_INDOM);

	pmsprintf(buffer, sizeof(buffer), "%s", cibadmin_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* First we need to check whether we are in <constraints> section*/
		if (strstr(buffer, "<constraints>")) {
			found_constraints = 1;
			continue;
		}

		/* Find the node name for our resource */
		if (strstr(buffer, "rsc_location id=") && found_constraints) {
			sscanf(buffer, "\t<rsc_location id=\"%[^\"]\"", constraint_name);

			struct  pacemaker_constraints *constraints;

			sts = pmdaCacheLookupName(indom, constraint_name, NULL, (void **)&constraints);
			if (sts == PM_ERR_INST || (sts >=0 && constraints == NULL)) {
				constraints = calloc(1, sizeof(struct pacemaker_constraints));
				if (constraints == NULL) {
					pclose(pf);
					return PM_ERR_AGAIN;
				}
			}
			else if (sts < 0)
				continue;

			pmdaCacheStore(indom, PMDA_CACHE_ADD, constraint_name, (void *)constraints);
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_pacemaker_nodes_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], node_name[256];
	int			found_nodes = 0;
	FILE		*pf;
	pmInDom		indom = INDOM(PACEMAKER_NODES_INDOM);

	pmsprintf(buffer, sizeof(buffer), "%s", crm_mon_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* First we need to check whether we are in <nodes> section*/
		if (strstr(buffer, "<nodes>")) {
			found_nodes = 1;
			continue;
		}

		/* Check to see if we overun section */
		if (strstr(buffer, "</nodes>")) {
			found_nodes = 0;
			continue;
		}

		/* Collect our node names */
		if (found_nodes) {
			if (strstr(buffer, "node name=")) {
				sscanf(buffer, "\t<node name=\"%[^\"]\"", node_name);

				struct  pacemaker_nodes *pace_nodes;

				sts = pmdaCacheLookupName(indom, node_name, NULL, (void **)&pace_nodes);
				if (sts == PM_ERR_INST || (sts >=0 && pace_nodes == NULL)) {
					pace_nodes = calloc(1, sizeof(struct pacemaker_nodes));
					if (pace_nodes == NULL) {
						pclose(pf);
						return PM_ERR_AGAIN;
					}
				}
				else if (sts < 0)
					continue;

				pmdaCacheStore(indom, PMDA_CACHE_ADD, node_name, (void *)pace_nodes);
			}
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_pacemaker_node_attrib_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], node_name[128], attribute_name[127], instance_name[256];
	int			found_node_attributes = 0, found_node_name = 0;
	FILE		*pf;
	pmInDom		indom = INDOM(PACEMAKER_NODE_ATTRIB_INDOM);

	pmsprintf(buffer, sizeof(buffer), "%s", crm_mon_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* First we need to check whether we are in <node_history> section*/
		if (strstr(buffer, "<node_attributes>")) {
			found_node_attributes = 1;
			continue;
		}

		/* Check to see if we pass </node_attributes> */ 
		if (strstr(buffer, "</node_attributes>")) {
			found_node_attributes = 0;
			continue;
		}

		/* Find the node name for our node */
		if (strstr(buffer, "node name=") && found_node_attributes ) {
			sscanf(buffer, "\t<node name=\"%[^\"]\"", node_name);
			found_node_name = 1;
			continue;
		}

		/* Check for when we overrun to another node */
		if (strstr(buffer, "</node>")) {
			found_node_name = 0;
			continue;
		}

		/* Record our instance as node:resource-id and assign */
		if (found_node_attributes && found_node_name) {

			if (strstr(buffer, "attribute name=")) {
				sscanf(buffer, "\t<attribute name=\"%[^\"]\"", attribute_name);
				
				/* 
				 * Assign indom based upon our node_name:attribute_name by joining our node_name
				 * with our volume number 
				 */
				snprintf(instance_name, sizeof(instance_name), "%s:%s", node_name, attribute_name);

				struct  pacemaker_node_attrib *node_attrib;

				sts = pmdaCacheLookupName(indom, instance_name, NULL, (void **)&node_attrib);
				if (sts == PM_ERR_INST || (sts >=0 && node_attrib == NULL)) {
					node_attrib = calloc(1, sizeof(struct pacemaker_node_attrib));
					if (node_attrib == NULL) {
						pclose(pf);
						return PM_ERR_AGAIN;
					}
				}
				else if (sts < 0)
					continue;

				pmdaCacheStore(indom, PMDA_CACHE_ADD, instance_name, (void *)node_attrib);
			}
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_pacemaker_resources_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], resource_id[128] = {'\0'}, node_name[127] = {'\0'}, instance_name[256];
	int			found_resources = 0;
	FILE		*pf;
	pmInDom		indom= INDOM(PACEMAKER_RESOURCES_INDOM);

	pmsprintf(buffer, sizeof(buffer), "%s", crm_mon_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* First we need to check whether we are in <resources> section*/
		if (strstr(buffer, "<resources>")) {
			found_resources = 1;
			continue;
		}

		/* Check to see if we pass in clones <clone id=...> */ 
		if (strstr(buffer, "</resources>")) {
			found_resources = 0;
			continue;
		}

		/* Record our instance as node:resource-id and assign */
		if (found_resources) {

			if (strstr(buffer, "resource id=")) {
				sscanf(buffer, "\t<resource id=\"%[^\"]\"", resource_id);
			}

			if (strstr(buffer, "node name=")) {
				sscanf(buffer, "\t<node name=\"%[^\"]\"", node_name);
			}

			if (strstr(buffer, "/>")) {
				/* 
				 * Assign indom based upon our resource_name:node_id by joining our node_name
				 * with our volume number but only if we have both a resouce name and a node_id  
				 */
				if (node_name[0] == '\0') {
					snprintf(instance_name, sizeof(instance_name), "%s", resource_id);
				} else {
					snprintf(instance_name, sizeof(instance_name), "%s:%s", resource_id, node_name);
				}

				struct pacemaker_resources *pace_resources;

				sts = pmdaCacheLookupName(indom, instance_name, NULL, (void **)&pace_resources);
				if (sts == PM_ERR_INST || (sts >=0 && pace_resources == NULL)) {
					pace_resources = calloc(1, sizeof(struct pacemaker_resources));
					if (pace_resources == NULL) {
						pclose(pf);
						return PM_ERR_AGAIN;
					}
				}
				else if (sts < 0)
					continue;

				pmdaCacheStore(indom, PMDA_CACHE_ADD, instance_name, (void *)pace_resources);

				/* Clear node name in the event that a resource has not got a node attachment */
				memset(node_name, '\0', sizeof(node_name));
			}
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_corosync_node_instance_refresh(void)
{
	int			sts, node_id;
	char		buffer[4096], node_name[128];
	char		*buffer_ptr;
	FILE		*pf;
	pmInDom		indom = INDOM(COROSYNC_NODE_INDOM);

	/*
	 * Update indom cache based off number of nodes listed in the
	 * membership information section of corosync-quorumtool output
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if ((pf = popen(quorumtool_command, "r")) == NULL)
		return -oserror();

	while (fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* Clear whitespace at start of each line */
		buffer_ptr = buffer;
		while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;
	
		if(isdigit(buffer_ptr[0])) {
			/* 
			 * corosync-quorumtool membership information layout:
			 * Nodeid	Votes	Qdevice	Name
			 */
			sscanf(buffer_ptr, "%d %*d %*s %s", &node_id, node_name);
			
			if (node_id == 0) {
				memset(node_name, '\0', sizeof(node_name));
				strncpy(node_name, "Qdevice", 9);
			}
			
			/* 
			* At this point node_name contains our device name this will be used to
		 	* map stats to node instances 
		 	*/
			struct  corosync_node *node;

			sts = pmdaCacheLookupName(indom, node_name, NULL, (void **)&node);
			if (sts == PM_ERR_INST || (sts >=0 && node == NULL)) {
				node = calloc(1, sizeof(struct corosync_node));
				if (node == NULL) {
					pclose(pf);
					return PM_ERR_AGAIN;
				}
			}
			else if (sts < 0)
				continue;

			pmdaCacheStore(indom, PMDA_CACHE_ADD, node_name, (void *)node);			
		}
	}
	pclose(pf);
	return(0);
}

static int
hacluster_corosync_ring_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], ring_name[128];
	FILE		*pf;
	pmInDom		indom = INDOM(COROSYNC_RING_INDOM);

	/*
	 * Update indom cache based off number of nodes listed in the
	 * membership information section of corosync-quorumtool output
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	
	if ((pf = popen(cfgtool_command, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		/* 
		 * The aim is to find the id and status lines for our corresponding
		 * ring indom based on the ring id (ring_name)
		 *
		 * Check for the exact match that we are in the RING (corosync < v2.99)
		 * or LINK (corosync v2.99.0+)  details section for our given ring by 
		 * its ring id (ring_name)
		 */
		if (strstr(buffer, "RING ID") || strstr(buffer, "LINK ID")) {
			/* Collect the ring number while are matching to our ring_name */
			sscanf(buffer, "%*s %*s %s", ring_name);

			/* 
			 * At this point node_name contains our device name this will be used to
			 * map stats to node instances 
			 */
			struct  corosync_ring *ring;

			sts = pmdaCacheLookupName(indom, ring_name, NULL, (void **)&ring);
			if (sts == PM_ERR_INST || (sts >=0 && ring == NULL)) {
				ring = calloc(1, sizeof(struct corosync_ring));
				if (ring == NULL) {
					pclose(pf);
					return PM_ERR_AGAIN;
				}
			}
			else if (sts < 0)
				continue;

			pmdaCacheStore(indom, PMDA_CACHE_ADD, ring_name, (void *)ring);
		}
	}
	pclose(pf);
	return(0);
}

static int
hacluster_sbd_device_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], dev_name[256];
	char		*token;
	char		*buffer_ptr;
	FILE		*fp;
	pmInDom		indom = INDOM(SBD_DEVICE_INDOM);

	/*
	 * Update indom cache based off number of nodes listed in the
	 * membership information section of corosync-quorumtool output
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if ((fp = fopen(sbd_path, "r")) == NULL)
		/*
		 * There might not be any sbd devices configured to return 
		 * currently, so just provide no instances so cleanly return
		 */
		return 0;

	/* 
	 * We want to get the line starting with SBD_DEVICE= and then split
	 * each to each device as a new indom based on their separator ";"
	 */
	while (fgets(buffer, sizeof(buffer)-1, fp) != NULL) {

		/* Guard against default comment lines e.g. #SBD_DEVICE= */
		if (strstr(buffer, "#"))
			continue;

		if (strncmp(buffer, "SBD_DEVICE=", 11) == 0) {
			/* 
			 * Buffer now contains our SBD_DEVICE names and paths, we need
			 * to split this to each individual device and also ommit the
			 * SBD_DEVICE= start of the line
			 */
			buffer_ptr = buffer;
			while ((token = strsep(&buffer_ptr, "= ; \n")) != NULL){
				if (strstr(token, "/")) {
					/* 
					 * At this point token contains our device name this will be used to
					 * map stats to node instances 
					 */
					strncpy(dev_name, token, sizeof(dev_name)-1);	
			
					struct  sbd_device *sbd;

					sts = pmdaCacheLookupName(indom, dev_name, NULL, (void **)&sbd);
					if (sts == PM_ERR_INST || (sts >=0 && sbd == NULL)) {
						sbd = calloc(1, sizeof(struct sbd_device));
						if (sbd == NULL) {
							fclose(fp);
							return PM_ERR_AGAIN;
						}
					}
					else if (sts < 0)
						continue;

					pmdaCacheStore(indom, PMDA_CACHE_ADD, dev_name, (void *)sbd);
				}
			}
		}
	}
	fclose(fp);

	return 0;
}

int
hacluster_drbd_resource_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], node_name[128], resource_name[256];
	int			volume;
	char		*buffer_ptr;
	FILE		*pf;
	pmInDom		indom = INDOM(DRBD_RESOURCE_INDOM);

	int found_node = 0, found_volume = 0, nesting = 0;

	/*
	 * Update indom cache based off the reading of drbd resources listed in
	 * the json output from drbdsetup
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	
	if ((pf = popen(drbdsetup_command, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* Clear whitespace at start of each line */
		buffer_ptr = buffer;
		while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;

		/* We keep track of the JSON input nesting to track node, device and volume changes */
		if (strstr(buffer_ptr, "{")) nesting++;
		if (strstr(buffer_ptr, "}")) nesting--;

		/* First locate our node we need metrics from */
		if (strstr(buffer_ptr, "\"name\":") && (nesting == DRBD_JSON_NODE)) {
			sscanf(buffer_ptr, "\"name\": \"%[^\",]", node_name);
			found_node = 1;
		}

		/* Check to see if we overrun to other nodes */
		if ((nesting < DRBD_JSON_NODE) && found_node) {
			found_node = 0;
			continue;
		}

		if (strstr(buffer_ptr, "\"volume\":") && (nesting == DRBD_JSON_DEVICE)) {
			sscanf(buffer_ptr, "\"volume\": %d", &volume);
			found_volume = 1;
		}

		if ( found_node && found_volume ) {
			/* 
			 * Assign indom based upon our resource_name:volume by joining our node_name
			 * with our volume number 
			 */
			snprintf(resource_name, sizeof(resource_name), "%s:%d", node_name, volume);

			struct  drbd_resource *resource;

			sts = pmdaCacheLookupName(indom, resource_name, NULL, (void **)&resource);
			if (sts == PM_ERR_INST || (sts >=0 && resource == NULL)) {
				resource = calloc(1, sizeof(struct drbd_resource));
				if (resource == NULL) {
					pclose(pf);
					return PM_ERR_AGAIN;
				}
			}
			else if (sts < 0)
				continue;

			pmdaCacheStore(indom, PMDA_CACHE_ADD, resource_name, (void *)resource);
			found_volume = 0;
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_drbd_peer_device_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], node_name[128], peer_name[256];
	int			peer_node_id;
	char		*buffer_ptr;
	FILE		*pf;
	pmInDom		indom = INDOM(DRBD_PEER_DEVICE_INDOM);

	int found_node = 0, found_peer_node = 0, nesting = 0;
	
	/*
	 * Update indom cache based off the reading of drbd resources listed in
	 * the json output from drbdsetup
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if ((pf = popen(drbdsetup_command, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* Clear whitespace at start of each line */
		buffer_ptr = buffer;
		while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;

		/* We keep track of the JSON input nesting to track node, device and volume changes */
		if (strstr(buffer_ptr, "{")) nesting++;
		if (strstr(buffer_ptr, "}")) nesting--;

		/* First locate our node we need metrics from */
		if (strstr(buffer_ptr, "\"name\":") && (nesting == DRBD_JSON_NODE)) {
			sscanf(buffer_ptr, "\"name\": \"%[^\",]", node_name);
			found_node = 1;
		}

		/* Check to see if we overrun to other nodes */
		if ((nesting < DRBD_JSON_NODE) && found_node) {
			found_node = 0;
			continue;
		}

		if (strstr(buffer_ptr, "\"peer-node-id\":") && (nesting == DRBD_JSON_DEVICE)) {
			sscanf(buffer_ptr, "\"peer-node-id\": %d", &peer_node_id);
			found_peer_node = 1;
		}

		if ( found_node && found_peer_node ) {
			/* 
			 * Assign indom based upon our resource_name:volume by joining our node_name
			 * with our volume number 
			 */
			snprintf(peer_name, sizeof(peer_name), "%s:%d", node_name, peer_node_id);

			struct  drbd_peer_device *peer_device;

			sts = pmdaCacheLookupName(indom, peer_name, NULL, (void **)&peer_device);
			if (sts == PM_ERR_INST || (sts >=0 && peer_device == NULL)) {
				peer_device = calloc(1, sizeof(struct drbd_peer_device));
				if (peer_device == NULL) {
					pclose(pf);
					return PM_ERR_AGAIN;
				}
			}
			else if (sts < 0)
				continue;

			pmdaCacheStore(indom, PMDA_CACHE_ADD, peer_name, (void *)peer_device);
			found_peer_node = 0;
		}
	}
	pclose(pf);
	return 0;
}

static int
hacluster_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
	hacluster_pacemaker_fail_instance_refresh();
	hacluster_pacemaker_contraints_instance_refresh();
	hacluster_pacemaker_nodes_instance_refresh();
	hacluster_pacemaker_node_attrib_instance_refresh();
	hacluster_pacemaker_resources_instance_refresh();
	hacluster_corosync_node_instance_refresh();
	hacluster_corosync_ring_instance_refresh();
	hacluster_sbd_device_instance_refresh();
	hacluster_drbd_resource_instance_refresh();
	hacluster_drbd_peer_device_instance_refresh();
	return pmdaInstance(indom, inst, name, result, pmda);
}

static int
hacluster_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
	struct pacemaker_fail			*fail;
	struct pacemaker_constraints	*constraints;
	struct pacemaker_nodes			*pace_nodes;
	struct pacemaker_node_attrib	*node_attribs;
	struct pacemaker_resources		*pace_resources;
	struct corosync_node			*node;
	struct corosync_ring			*ring;
	struct sbd_device				*sbd;
	struct drbd_resource			*resource;
	struct drbd_peer_device			*peer;
	char 							*node_name, *ring_name, *sbd_dev, *resource_name, *peer_device; 
	char							*instance_name, *constraint_name, *pace_node_name, *attrib_name;
	char							*pace_resource_name;
	int 							i, sts;
		
	if ((sts = hacluster_pacemaker_fail_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_pacemaker_contraints_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_pacemaker_nodes_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_pacemaker_node_attrib_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_pacemaker_resources_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_corosync_node_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_corosync_ring_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_sbd_device_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_drbd_resource_instance_refresh()) < 0)
		return sts;
		
	if ((sts = hacluster_drbd_peer_device_instance_refresh()) < 0)
		return sts;	

	if (need_refresh[CLUSTER_PACEMAKER_GLOBAL])
		hacluster_refresh_pacemaker_global();
		
	for (pmdaCacheOp(INDOM(PACEMAKER_FAIL_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(PACEMAKER_FAIL_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(PACEMAKER_FAIL_INDOM), i, &instance_name, (void **)&fail) || !fail)
			continue;

		if (need_refresh[CLUSTER_PACEMAKER_FAIL])
			hacluster_refresh_pacemaker_fail(instance_name, &fail->fail_count);
	}
	
	for (pmdaCacheOp(INDOM(PACEMAKER_CONSTRAINTS_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(PACEMAKER_CONSTRAINTS_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(PACEMAKER_CONSTRAINTS_INDOM), i, &constraint_name, (void **)&constraints) || !constraints)
			continue;

		if (need_refresh[CLUSTER_PACEMAKER_CONSTRAINTS])
			hacluster_refresh_pacemaker_constraints(constraint_name, &constraints->location_constraints);
	}
	
	for (pmdaCacheOp(INDOM(PACEMAKER_NODES_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(PACEMAKER_NODES_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(PACEMAKER_NODES_INDOM), i, &pace_node_name, (void **)&pace_nodes) || !pace_nodes)
			continue;

		if (need_refresh[CLUSTER_PACEMAKER_NODES])
			hacluster_refresh_pacemaker_nodes(pace_node_name, &pace_nodes->nodes);
	}
	
	for (pmdaCacheOp(INDOM(PACEMAKER_NODE_ATTRIB_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(PACEMAKER_NODE_ATTRIB_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(PACEMAKER_NODE_ATTRIB_INDOM), i, &attrib_name, (void **)&node_attribs) || !node_attribs)
			continue;

		if (need_refresh[CLUSTER_PACEMAKER_NODE_ATTRIB])
			hacluster_refresh_pacemaker_node_attribs(attrib_name, &node_attribs->attributes);
	}
	
	for (pmdaCacheOp(INDOM(PACEMAKER_RESOURCES_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(PACEMAKER_RESOURCES_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(PACEMAKER_RESOURCES_INDOM), i, &pace_resource_name, (void **)&pace_resources) || !pace_resources)
			continue;

		if (need_refresh[CLUSTER_PACEMAKER_RESOURCES])
			hacluster_refresh_pacemaker_resources(pace_resource_name, &pace_resources->resources);
	}

	for (pmdaCacheOp(INDOM(COROSYNC_NODE_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(COROSYNC_NODE_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(COROSYNC_NODE_INDOM), i, &node_name, (void **)&node) || !node)
			continue;

		if (need_refresh[CLUSTER_COROSYNC_NODE])
			hacluster_refresh_corosync_node(node_name, &node->member_votes);
	}
	
	if (need_refresh[CLUSTER_COROSYNC_GLOBAL])
		hacluster_refresh_corosync_global();
		
	for (pmdaCacheOp(INDOM(COROSYNC_RING_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(COROSYNC_RING_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(COROSYNC_RING_INDOM), i, &ring_name, (void **)&ring) || !ring)
			continue;

		if (need_refresh[CLUSTER_COROSYNC_RING])
			hacluster_refresh_corosync_ring(ring_name, &ring->rings);
	}
	
	for (pmdaCacheOp(INDOM(SBD_DEVICE_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(SBD_DEVICE_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(SBD_DEVICE_INDOM), i, &sbd_dev, (void **)&sbd) || !sbd)
			continue;

		if (need_refresh[CLUSTER_SBD_DEVICE])
			hacluster_refresh_sbd_device(sbd_dev, &sbd->sbd);
	}
	
	for (pmdaCacheOp(INDOM(DRBD_RESOURCE_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(DRBD_RESOURCE_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(DRBD_RESOURCE_INDOM), i, &resource_name, (void **)&resource) || !resource)
			continue;

		if (need_refresh[CLUSTER_DRBD_RESOURCE])
			hacluster_refresh_drbd_resource(resource_name, &resource->resource);
	}
	
	for (pmdaCacheOp(INDOM(DRBD_PEER_DEVICE_INDOM), PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(INDOM(DRBD_PEER_DEVICE_INDOM), PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(INDOM(DRBD_PEER_DEVICE_INDOM), i, &peer_device, (void **)&peer) || !peer)
			continue;

		if (need_refresh[CLUSTER_DRBD_PEER_DEVICE])
			hacluster_refresh_drbd_peer_device(peer_device, &peer->peer_device);
	}
	
	return sts;
}

static int
hacluster_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
	int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

	for (i = 0; i < numpmid; i++) {
		unsigned int	cluster = pmID_cluster(pmidlist[i]);
		if (cluster < NUM_CLUSTERS)
			need_refresh[cluster]++;
	}

	if ((sts = hacluster_fetch_refresh(pmda, need_refresh)) < 0)
		return sts;

	return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
hacluster_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
	unsigned int					item = pmID_item(mdesc->m_desc.pmid);
	unsigned int					cluster = pmID_cluster(mdesc->m_desc.pmid);
	struct pacemaker_fail			*fail;
	struct pacemaker_constraints	*constraints;
	struct pacemaker_nodes			*pace_nodes;
	struct pacemaker_node_attrib	*pace_attribs;
	struct pacemaker_resources		*pace_resources;
	struct corosync_node			*node;
	struct corosync_ring			*ring;
	struct sbd_device				*sbd;
	struct drbd_resource			*resource;
	struct drbd_peer_device			*peer;
	int								sts;

	switch (cluster) {
		case CLUSTER_PACEMAKER_GLOBAL:
			return hacluster_pacemaker_global_fetch(item, atom);

		case CLUSTER_PACEMAKER_FAIL:
			sts = pmdaCacheLookup(INDOM(PACEMAKER_FAIL_INDOM), inst, NULL, (void **)&fail);
			if (sts < 0)
				return sts;
			return hacluster_pacemaker_fail_fetch(item, &fail->fail_count, atom);
			
		case CLUSTER_PACEMAKER_CONSTRAINTS:
			sts = pmdaCacheLookup(INDOM(PACEMAKER_CONSTRAINTS_INDOM), inst, NULL, (void **)&constraints);
			if (sts < 0)
				return sts;
			return hacluster_pacemaker_constraints_fetch(item, &constraints->location_constraints, atom);
			
		case CLUSTER_PACEMAKER_NODES:
			sts = pmdaCacheLookup(INDOM(PACEMAKER_NODES_INDOM), inst, NULL, (void **)&pace_nodes);
			if (sts < 0)
				return sts;
			return hacluster_pacemaker_nodes_fetch(item, &pace_nodes->nodes, atom);
			
		case CLUSTER_PACEMAKER_NODE_ATTRIB:
			sts = pmdaCacheLookup(INDOM(PACEMAKER_NODE_ATTRIB_INDOM), inst, NULL, (void **)&pace_attribs);
			if (sts < 0)
				return sts;
			return hacluster_pacemaker_node_attribs_fetch(item, &pace_attribs->attributes, atom);
			
		case CLUSTER_PACEMAKER_RESOURCES:
			sts = pmdaCacheLookup(INDOM(PACEMAKER_RESOURCES_INDOM), inst, NULL, (void **)&pace_resources);
			if (sts < 0)
				return sts;
			return hacluster_pacemaker_resources_fetch(item, &pace_resources->resources, atom);

		case CLUSTER_COROSYNC_NODE:
			sts = pmdaCacheLookup(INDOM(COROSYNC_NODE_INDOM), inst, NULL, (void **)&node);
			if (sts < 0)
				return sts;
			return hacluster_corosync_node_fetch(item, &node->member_votes, atom);

		case CLUSTER_COROSYNC_GLOBAL:
			return hacluster_corosync_global_fetch(item, atom);
			
		case CLUSTER_COROSYNC_RING:
			sts = pmdaCacheLookup(INDOM(COROSYNC_RING_INDOM), inst, NULL, (void **)&ring);
			if (sts < 0)
				return sts;
			return hacluster_corosync_ring_fetch(item, &ring->rings, atom);
			
		case CLUSTER_SBD_DEVICE:
			sts = pmdaCacheLookup(INDOM(SBD_DEVICE_INDOM), inst, NULL, (void **)&sbd);
			if (sts < 0)
				return sts;
			return hacluster_sbd_device_fetch(item, &sbd->sbd, atom);
			
		case CLUSTER_DRBD_RESOURCE:
			sts = pmdaCacheLookup(INDOM(DRBD_RESOURCE_INDOM), inst, NULL, (void **)&resource);
			if (sts < 0)
				return sts;
			return hacluster_drbd_resource_fetch(item, &resource->resource, atom);		
		
		case CLUSTER_DRBD_PEER_DEVICE:
			sts = pmdaCacheLookup(INDOM(DRBD_PEER_DEVICE_INDOM), inst, NULL, (void **)&peer);
			if (sts < 0)
				return sts;
			return hacluster_drbd_peer_device_fetch(item, &peer->peer_device, atom);

		default:
			return PM_ERR_PMID;
	}

	return PMDA_FETCH_STATIC;
}

void
hacluster_inst_setup(void)
{
	static char pacemaker_command_cibadmin[] = "cibadmin --query --local";
	static char pacemaker_command_crm_mon[] = "crm_mon -X --inactive";
	static char corosync_command_quorumtool[] = "corosync-quorumtool -p";
	static char corosync_command_cfgtool[] = "corosync-cfgtool -s";
	static char sbd_config_path[] = "/etc/sysconfig/sbd";
	static char drbd_command_drbdsetup[] = "drbdsetup status --json";
	char *env_command;

	/* allow override at startup for QA testing - PACEMAKER */
	if ((env_command = getenv("HACLUSTER_SETUP_CIBADMIN")) != NULL)
		cibadmin_command = env_command;
	else
		cibadmin_command = pacemaker_command_cibadmin;

	/* further env setup for QA testing - PACEMAKER */
	if ((env_command = getenv("HACLUSTER_SETUP_CRM_MON")) != NULL)
		crm_mon_command = env_command;
	else	
		crm_mon_command = pacemaker_command_crm_mon;

	/* further env setup for QA testing - COROSYNC */
	if ((env_command = getenv("HACLUSTER_SETUP_QUORUM")) != NULL)
		quorumtool_command = env_command;
	else
		quorumtool_command = corosync_command_quorumtool;

	/* further env setup for QA testing - COROSYNC */
	if ((env_command = getenv("HACLUSTER_SETUP_CFG")) != NULL)
		cfgtool_command = env_command;
	else	
		cfgtool_command = corosync_command_cfgtool;
	
	/* further env setup for QA testing - SBD */
	if ((env_command = getenv("HACLUSTER_SETUP_SBD_PATH")) != NULL)
		sbd_path = env_command;
	else	
		sbd_path = sbd_config_path;

	/* further env setup for QA testing - DRBD */
	if ((env_command = getenv("HACLUSTER_SETUP_DRBD")) != NULL)
		drbdsetup_command = env_command;
	else
		drbdsetup_command = drbd_command_drbdsetup;
}

static int
hacluster_text(int ident, int type, char **buf, pmdaExt *pmda)
{
	if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
		int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
		if (sts != -ENOENT)
			return sts;
	}
	return pmdaText(ident, type, buf, pmda);
}

static int
hacluster_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
	pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
	return pmdaTreePMID(tree, name, pmid);
}

static int
hacluster_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
	pmdaNameSpace *tree = pmdaDynamicLookupPMID(pmda, pmid);
	return pmdaTreeName(tree, pmid, nameset);
}

static int
hacluster_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
	pmdaNameSpace *tree = pmdaDynamicLookupName(pmda, name);
	return pmdaTreeChildren(tree, name, flag, kids, sts);
}

void
__PMDA_INIT_CALL
hacluster_init(pmdaInterface *dp)
{
	int nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
	int nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

	if (_isDSO) {
		char helppath[MAXPATHLEN];
		int sep = pmPathSeparator();
		pmsprintf(helppath, sizeof(helppath), "%s%c" "hacluster" "%c" "help",
			pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
		pmdaDSO(dp, PMDA_INTERFACE_4, "HACLUSTER DSO", helppath);
	}

	if (dp->status != 0)
		return;

	/* Check for environment variables allowing test injection */
	hacluster_inst_setup();
	pacemaker_stats_setup();
	corosync_stats_setup();
	sbd_stats_setup();
	drbd_stats_setup();

	dp->version.four.instance = hacluster_instance;
	dp->version.four.fetch = hacluster_fetch;
	dp->version.four.text = hacluster_text;
	dp->version.four.pmid = hacluster_pmid;
	dp->version.four.name = hacluster_name;
	dp->version.four.children = hacluster_children;
	pmdaSetFetchCallBack(dp, hacluster_fetchCallBack);

	pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
	pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
}

static pmLongOptions longopts[] = {
	PMDA_OPTIONS_HEADER("Options"),
	PMOPT_DEBUG,
	PMDAOPT_DOMAIN,
	PMDAOPT_LOGFILE,
	PMOPT_HELP,
	PMDA_OPTIONS_END
};

static pmdaOptions opts = {
	.short_options = "D:d:l:U:?",
	.long_options = longopts,
};

int
main(int argc, char **argv)
{
	int sep = pmPathSeparator();
	char helppath[MAXPATHLEN];
	pmdaInterface dispatch;

	_isDSO = 0;
	pmSetProgname(argv[0]);
	pmsprintf(helppath, sizeof(helppath), "%s%c" "hacluster" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmGetProgname(), HACLUSTER, "hacluster.log", helppath);

	pmdaGetOptions(argc, argv, &opts, &dispatch);
	if (opts.errors) {
		pmdaUsageMessage(&opts);
		exit(1);
	}

	pmdaOpenLog(&dispatch);

	hacluster_init(&dispatch);
	pmdaConnect(&dispatch);
	pmdaMain(&dispatch);
	exit(0);
}
