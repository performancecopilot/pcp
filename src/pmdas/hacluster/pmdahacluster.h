/*
 * High Availability (HA) Cluster PMDA
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

#ifndef PMDAHACLUSTER_H
#define PMDAHACLUSTER_H

#include "pacemaker.h"
#include "corosync.h"
#include "sbd.h"
#include "drbd.h"

enum {
	CLUSTER_PACEMAKER_GLOBAL = 0,		/* 0  -- NULL INDOM */
	CLUSTER_PACEMAKER_FAIL,				/* 1  -- PACEMAKER_FAIL_INDOM */
	CLUSTER_PACEMAKER_CONSTRAINTS,		/* 2  -- PACEMAKER_CONSTRAINTS_INDOM */
	CLUSTER_PACEMAKER_NODES,			/* 3  -- PACEMAKER_NODES_IDOM*/
	CLUSTER_PACEMAKER_NODE_ATTRIB,		/* 4  -- PACEMAKER_NODE_ATRRIB_INDOM */
	CLUSTER_PACEMAKER_RESOURCES,		/* 5  -- PACEMAKER_RESOURCES_INDOM */
	CLUSTER_COROSYNC_NODE,				/* 6  -- COROSYNC_NODE_INDOM */
	CLUSTER_COROSYNC_GLOBAL,			/* 7  -- NULL INDOM */
	CLUSTER_COROSYNC_RING,				/* 8  -- COROSYNC_RING INDOM */
	CLUSTER_SBD_DEVICE,					/* 9  -- SBD_DEVICES_INDOM */
	CLUSTER_DRBD_RESOURCE,				/* 10 -- DRBD_RESOURCE_INDOM */
	CLUSTER_DRBD_PEER_DEVICE,			/* 11 -- DRBD_PEER_DEVICE_INDOM */
	CLUSTER_PACEMAKER_CONSTRAINTS_ALL, 	/* 12 -- PACEMAKER_CONSTRAINTS_ALL_INDOM */
	CLUSTER_PACEMAKER_NODE_ATTRIB_ALL,	/* 13 -- PACEMAKER_NODE_ATTRIB_ALL_INDOM */
	CLUSTER_PACEMAKER_RESOURCES_ALL,	/* 14 -- PACEMAKER_RESOURCES_ALL_INDOM */
	CLUSTER_COROSYNC_RING_ALL,			/* 15 -- COROSYNC_RING_ALL_INDOM */
	CLUSTER_SBD_DEVICE_ALL,				/* 16 -- SBD_DEVICES_ALL_INDOM */
	CLUSTER_DRBD_RESOURCE_ALL,			/* 17 -- DRBD_RESOURCE_ALL_INDOM */
	CLUSTER_DRBD_PEER_DEVICE_ALL,		/* 18 -- DRBD_PEER_DEVICE_ALL_INDOM */
	NUM_CLUSTERS
};

enum {
	PACEMAKER_FAIL_INDOM = 0,			/* 0  -- Pacemaker failure/migrations */
	PACEMAKER_CONSTRAINTS_INDOM,		/* 1  -- Pacemaker location constraints */
	PACEMAKER_NODES_INDOM,				/* 2  -- Pacemaker nodes data */
	PACEMAKER_NODE_ATTRIB_INDOM,		/* 3  -- Pacemaker node attributes */
	PACEMAKER_RESOURCES_INDOM,			/* 4  -- Pacemaker resources */
	COROSYNC_NODE_INDOM, 				/* 5  -- Corosync available nodes  */
	COROSYNC_RING_INDOM,				/* 6  -- Corosync available rings */
	SBD_DEVICE_INDOM,		 			/* 7  -- SBD available devices */
	DRBD_RESOURCE_INDOM,	 			/* 8  -- DRBD Resources */
	DRBD_PEER_DEVICE_INDOM,	 			/* 9  -- DRBD Peer Devices */
	PACEMAKER_CONSTRAINTS_ALL_INDOM,	/* 10 -- Pacemaker location constraints all (labels) */
	PACEMAKER_NODE_ATTRIB_ALL_INDOM,	/* 11 -- Pacemaker node attributes all(labels) */
	PACEMAKER_RESOURCES_ALL_INDOM,		/* 12 -- Pacemaker resources all (labels) */
	COROSYNC_RING_ALL_INDOM,			/* 13 -- Corosync available rings all (labels) */
	SBD_DEVICE_ALL_INDOM,				/* 14 -- SBD available devices all (labels) */
	DRBD_RESOURCE_ALL_INDOM,			/* 15 -- DRBD Resources all (labels) */
	DRBD_PEER_DEVICE_ALL_INDOM,			/* 16 -- DRBD Peer Devicesall (labels) */
	NUM_INDOMS
};

extern pmInDom hacluster_indom(int);

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif /* PMDACLUSTER_H */
