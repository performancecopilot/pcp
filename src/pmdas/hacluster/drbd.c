/*
 * HA Cluster DRDB statistics.
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
 
#include <inttypes.h>
#include <ctype.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "drbd.h"

static char *drbdsetup_command;
static char *split_brain_path;

int
hacluster_drbd_resource_fetch(int item, struct resource *resource, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_DRBD_RESOURCE_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case DRBD_RESOURCE_RESOURCE:
			atom->cp = resource->resource;
			return PMDA_FETCH_STATIC;

		case DRBD_RESOURCE_ROLE:
			atom->cp = resource->role;
			return PMDA_FETCH_STATIC;
	
		case DRBD_RESOURCE_VOLUME:
			atom->cp = resource->volume;
			return PMDA_FETCH_STATIC;
	
		case DRBD_RESOURCE_DISK_STATE:
			atom->cp = resource->disk_state;
			return PMDA_FETCH_STATIC;
	
		case DRBD_RESOURCE_WRITTEN:
			atom->ul = resource->write;
			return PMDA_FETCH_STATIC;
	
		case DRBD_RESOURCE_READ:
			atom->ul = resource->read;
			return PMDA_FETCH_STATIC;
		
		case DRBD_RESOURCE_AL_WRITES:
			atom->ull = resource->al_writes;
			return PMDA_FETCH_STATIC;
	
		case DRBD_RESOURCE_BM_WRITES:
			atom->ull = resource->bm_writes;
			return PMDA_FETCH_STATIC;
		
		case DRBD_RESOURCE_UPPER_PENDING:
			atom->ull = resource->upper_pending;
			return PMDA_FETCH_STATIC;
		
		case DRBD_RESOURCE_LOWER_PENDING:
			atom->ull = resource->lower_pending;
			return PMDA_FETCH_STATIC;
		
		case DRBD_RESOURCE_QUORUM:
			atom->ul = resource->quorum;
			return PMDA_FETCH_STATIC;
			
		case DRBD_RESOURCE_SPLIT_BRAIN:
			atom->ul = resource->split_brain;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_drbd_resource_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_drbd_peer_device_fetch(int item, struct peer_device *peer_device, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_DRBD_PEER_DEVICE_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case DRBD_PEER_DEVICE_RESOURCE:
			atom->cp = peer_device->resource;
			return PMDA_FETCH_STATIC;
		
		case DRBD_PEER_DEVICE_PEER_NODE_ID:
			atom->cp = peer_device->peer_node_id;
			return PMDA_FETCH_STATIC;
		
		case DRBD_PEER_DEVICE_PEER_ROLE:
			atom->cp = peer_device->peer_role;
			return PMDA_FETCH_STATIC;
		
		case DRBD_PEER_DEVICE_VOLUME:
			atom->ul = peer_device->volume;
			return PMDA_FETCH_STATIC;
		
		case DRBD_PEER_DEVICE_PEER_DISK_STATE:
			atom->cp = peer_device->peer_disk_state;
			return PMDA_FETCH_STATIC;
		
		case DRBD_PEER_DEVICE_CONNECTIONS_SYNC:
			atom->f = peer_device->connections_sync;
			return PMDA_FETCH_STATIC;
			
		case DRBD_PEER_DEVICE_CONNECTIONS_RECEIVED:
			atom->ull = peer_device->connections_received;
			return PMDA_FETCH_STATIC;
			
		case DRBD_PEER_DEVICE_CONNECTIONS_SENT:
			atom->ull = peer_device->connections_sent;
			return PMDA_FETCH_STATIC;
			
		case DRBD_PEER_DEVICE_CONNECTIONS_PENDING:
			atom->ul = peer_device->connections_pending;
			return PMDA_FETCH_STATIC;
			
		case DRBD_PEER_DEVICE_CONNECTIONS_UNACKED:
			atom->ul = peer_device->connections_unacked;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_drbd_peer_device_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_refresh_drbd_resource(const char *resource_name, struct resource *resource)
{
	char buffer[4096];
	char *node, *volume;
	char *buffer_ptr, *tofree, *str;
	FILE *pf;

	int found_node = 0, found_volume = 0, nesting = 0;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", drbdsetup_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	/* 
	 * We need to split our combined NODE:VOLUME instance names into their
	 * separated NODE and VOLUME fields for matching in the drbdsetup output
	 */
	tofree = str = strdup(resource_name);
	node = strsep(&str, ":");
	volume = strsep(&str, ":");

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		/* Clear whitespace at start of each line */
		buffer_ptr = buffer;
		while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;

		/* We keep track of the JSON input nesting to track node, device and volume changes */
		if (strstr(buffer_ptr, "{")) nesting++;
		if (strstr(buffer_ptr, "}")) nesting--;

		/* First locate our node we need metrics from */
		if (strstr(buffer_ptr, "\"name\":") && strstr(buffer_ptr, node)) {
			found_node = 1;
			continue;
		}

		/* Check to see if we overrun to other nodes */
		if ((nesting < DRBD_JSON_NODE) && found_node) {
			found_node = 0;
			continue;
		}

		/* We then match our volume section */
		if (strstr(buffer_ptr, "\"volume\":") && strstr(buffer_ptr, volume) && found_node && (nesting == DRBD_JSON_DEVICE)) {
			found_volume = 1;
			continue;
		}

		/* Check to see if we overrun to other volumes */
		if ((nesting < DRBD_JSON_DEVICE) && found_volume) {
			found_volume = 0;
			continue;
		}

		/* Start collecting our metrics */
		if ( found_node && (nesting == DRBD_JSON_NODE)) {
			pmstrncpy(resource->resource, sizeof resource->resource, node);
			pmstrncpy(resource->volume, sizeof resource->volume, volume);

			if (strstr(buffer_ptr, "\"role\":"))
				sscanf(buffer_ptr, "\"role\": \"%[^\",]", resource->role);
		}

		/* Collect the rest of our metrics */
		if ( found_node && found_volume ) {

			if (strstr(buffer_ptr, "\"disk-state\":"))
				sscanf(buffer_ptr, "\"disk-state\": \"%[^\",]", resource->disk_state);

			if (strstr(buffer_ptr, "\"quorum\":")) {
				if (strstr(buffer_ptr, "true"))
					resource->quorum = 1;
				else
					resource->quorum = 0;
				}
	
			if (strstr(buffer_ptr, "\"read\":"))
				sscanf(buffer_ptr, "\"read\": %"SCNu32"", &resource->read);

			if (strstr(buffer_ptr, "\"written\":"))
				sscanf(buffer_ptr, "\"written\": %"SCNu32"", &resource->write);

			if (strstr(buffer_ptr, "\"al-writes\":"))
				sscanf(buffer_ptr, "\"al-writes\": %"SCNu64"", &resource->al_writes);

			if (strstr(buffer_ptr, "\"bm-writes\":"))
				sscanf(buffer_ptr, "\"bm-writes\": %"SCNu64"", &resource->bm_writes);

			if (strstr(buffer_ptr, "\"upper-pending\":"))
				sscanf(buffer_ptr, "\"upper-pending\": %"SCNu64"", &resource->upper_pending);

			if (strstr(buffer_ptr, "\"lower-pending\":"))
				sscanf(buffer_ptr, "\"lower-pending\": %"SCNu64"", &resource->lower_pending);
		}	
	}
	pclose(pf);

	/* Final Check to see if we have a split-brain detected for our resource-volume 
	 * hook filename is - drbd-split-brain-detected-NODE-VOLUME in our case.
	 */
	pmsprintf(buffer, sizeof(buffer), "%s/drbd-split-brain-detected-%s-%s", split_brain_path, node, volume);

	/* check using access if this file exists */
	if( access( buffer, F_OK ) == 0) {
		resource->split_brain = 1;
	} else {
		resource->split_brain = 0;
	}

	free(tofree);
	return 0;
}

int
hacluster_refresh_drbd_peer_device(const char *peer_name, struct peer_device *peer_device)
{
	char buffer[4096];
	char *node, *peer_node_id;
	char *buffer_ptr, *tofree, *str;
	FILE *pf;

	int found_node = 0, found_peer_node = 0, nesting = 0;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", drbdsetup_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	/* 
	 * We need to split our combined NODE:PEER_NODE_ID instance names into
	 * their separated NODE and PEER_NODE_ID fields for matching in the 
	 * drbdsetup output
	 */
	tofree = str = strdup(peer_name);
	node = strsep(&str, ":");
	peer_node_id = strsep(&str, ":");

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		/* Clear whitespace at start of each line */
		buffer_ptr = buffer;
		while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;

		/* We keep track of the JSON input nesting to track node, device and volume changes */
		if (strstr(buffer_ptr, "{")) nesting++;
		if (strstr(buffer_ptr, "}")) nesting--;

		/* First locate our node we need metrics from */
		if (strstr(buffer_ptr, "\"name\":") && strstr(buffer_ptr, node)) {
			found_node = 1;
			continue;
		}

		/* Check to see if we overrun to other nodes */
		if ((nesting < DRBD_JSON_NODE) && found_node) {
			found_node = 0;
			continue;
		}

		/* We then match our peer-node-id section */
		if (strstr(buffer_ptr, "\"peer-node-id\":") && strstr(buffer_ptr, peer_node_id) && found_node && (nesting == DRBD_JSON_DEVICE)) {
			found_peer_node = 1;
			continue;
		}

		/* Check to see if we overrun to other peer-nodes */
		if ((nesting < DRBD_JSON_DEVICE) && found_peer_node) {	
			found_peer_node = 0;
			continue;
		}

		/* Start collecting our metrics */
		if ( found_node && found_peer_node ) {
			pmstrncpy(peer_device->resource, sizeof peer_device->resource, node);
			pmstrncpy(peer_device->peer_node_id, sizeof peer_device->peer_node_id, peer_node_id);

			if (strstr(buffer_ptr, "\"peer-role\":"))
				sscanf(buffer_ptr, "\"peer-role\": \"%[^\",]", peer_device->peer_role);

			if (strstr(buffer_ptr, "\"volume\":"))
				sscanf(buffer_ptr, "\"volume\": \%"SCNu32"", &peer_device->volume);

			if (strstr(buffer_ptr, "\"peer-disk-state\":"))
				sscanf(buffer_ptr, "\"peer-disk-state\": \"%[^\",]", peer_device->peer_disk_state);

			if (strstr(buffer_ptr, "\"received\":"))
				sscanf(buffer_ptr, "\"received\": %"SCNu64"", &peer_device->connections_received);

			if (strstr(buffer_ptr, "\"sent\":"))
				sscanf(buffer_ptr, "\"sent\": %"SCNu64"", &peer_device->connections_sent);

			if (strstr(buffer_ptr, "\"pending\":"))
				sscanf(buffer_ptr, "\"pending\": %"SCNu32"", &peer_device->connections_pending);

			if (strstr(buffer_ptr, "\"unacked\":"))
				sscanf(buffer_ptr, "\"unacked\": %"SCNu32"", &peer_device->connections_unacked);

			if (strstr(buffer_ptr, "\"percent-in-sync\":"))
				sscanf(buffer_ptr, "\"percent-in-sync\": %f", &peer_device->connections_sync);
		}
	}
	pclose(pf);
	free(tofree);
	return 0;
}

void
drbd_stats_setup(void) 
{
	static char drbd_command_drbdsetup[] = "drbdsetup status --json";
	static char drbd_split_brain_path[] = "/var/run/drbd/splitbrain/";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_DRBD")) != NULL)
		drbdsetup_command = env_command;
	else
		drbdsetup_command = drbd_command_drbdsetup;
		
	/* further override for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_DRBD_SPLITBRAIN")) != NULL)
		split_brain_path = env_command;
	else
		split_brain_path = drbd_split_brain_path;
}
