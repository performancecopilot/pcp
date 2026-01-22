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

#include <inttypes.h>
#include <ctype.h>

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "pmdahacluster.h"
#include "corosync.h"

static char *quorumtool_command;
static char *cfgtool_command;

static struct corosync_global global_stats;

int
hacluster_corosync_node_fetch(int item, struct corosync_node *node, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_COROSYNC_MEMBER_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case COROSYNC_MEMBER_VOTES_VOTES:
			atom->ul = node->votes;
			return PMDA_FETCH_STATIC;

		case COROSYNC_MEMBER_VOTES_LOCAL:
			atom->ul = node->local;
			return PMDA_FETCH_STATIC;

		case COROSYNC_MEMBER_VOTES_NODE_ID:
			atom->ull = node->node_id;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_corosync_global_fetch(int item, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_COROSYNC_GLOBAL_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case COROSYNC_QUORATE:
			atom->ul = global_stats.quorate;
			return PMDA_FETCH_STATIC;

		case COROSYNC_QUORUM_VOTES_EXPECTED_VOTES:
			atom->ul = global_stats.expected_votes;	
			return PMDA_FETCH_STATIC;

		case COROSYNC_QUORUM_VOTES_HIGHEST_EXPECTED:
			atom->ul = global_stats.highest_expected;
			return PMDA_FETCH_STATIC;

		case COROSYNC_QUORUM_VOTES_TOTAL_VOTES:
			atom->ul = global_stats.total_votes;
			return PMDA_FETCH_STATIC;

		case COROSYNC_QUORUM_VOTES_QUORUM:
			atom->ul = global_stats.quorum;
			return PMDA_FETCH_STATIC;

		case COROSYNC_RING_ERRORS:
			atom->ul = global_stats.ring_errors;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_corosync_ring_fetch(int item, struct corosync_ring *rings, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_COROSYNC_RINGS_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case COROSYNC_RINGS_STATUS:
			atom->ul = rings->status;
			return PMDA_FETCH_STATIC;

		case COROSYNC_RINGS_ADDRESS:
			atom->cp = rings->address;
			return PMDA_FETCH_STATIC;

		case COROSYNC_RINGS_NODE_ID:
			atom->ull = rings->node_id;
			return PMDA_FETCH_STATIC;

		case COROSYNC_RINGS_NUMBER:
			atom->ul = rings->number;
			return PMDA_FETCH_STATIC;

		case COROSYNC_RINGS_RING_ID:
			atom->cp = rings->ring_id;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_corosync_ring_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_corosync_node_instance_refresh(void)
{
	int			sts, node_id;
	char		buffer[4096], node_name[128];
	char		*buffer_ptr;
	FILE		*pf;
	pmInDom		indom = hacluster_indom(COROSYNC_NODE_INDOM);

	/*
	 * Update indom cache based off number of nodes listed in the
	 * membership information section of corosync-quorumtool output
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	
	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", quorumtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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
				memcpy(node_name, "Qdevice", strlen("Qdevice"));
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

int
hacluster_corosync_ring_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], ring_name[128];
	FILE		*pf;
	pmInDom		indom = hacluster_indom(COROSYNC_RING_INDOM);
	pmInDom		indom_all = hacluster_indom(COROSYNC_RING_ALL_INDOM);

	/*
	 * Update indom cache based off number of nodes listed in the
	 * membership information section of corosync-quorumtool output
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	pmdaCacheOp(indom_all, PMDA_CACHE_INACTIVE);
	
	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cfgtool_command);
	
	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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
			pmdaCacheStore(indom_all, PMDA_CACHE_ADD, ring_name, NULL);
		}
	}
	pclose(pf);
	return(0);
}

int
hacluster_refresh_corosync_node(const char *node_name, struct corosync_node *node)
{
	char buffer[4096], local[8];
	char *buffer_ptr;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", quorumtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		if (strstr(buffer, node_name)) {

			/* 
			 * Initially strip white space at beginning of line, our membership 
			 * information section lines are right aligned, we also need to check
			 * that we are looking at the lines starting with the Nodeid values
			 */
			buffer_ptr = buffer;
			while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;
		
			/* 
			 * Remember to reset local as it will only be listed on the local node 
			 * and the field is blank otherwise
			 */
			memset(local, 0, sizeof(local));

			if (isdigit(*buffer_ptr)) {
				sscanf(buffer_ptr, "%"SCNu64" %"SCNu32" %*s %*s %s",
					&node->node_id,
					&node->votes,
					local
				);
		
				if (strncmp(local, "(local)", 7) == 0) {
					node->local = 1;
				} else {
					node->local = 0;
				}
			}	
		}
	}

	pclose(pf);
	return(0);	
}

int
hacluster_refresh_corosync_global()
{
	char buffer[4096], quorate[6];
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", quorumtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
	
		if (strncmp(buffer, "Quorate:", 8) == 0) {
			sscanf(buffer, "%*s %s", quorate);

			if (strncmp(quorate, "Yes", 3) == 0) {
				global_stats.quorate = 1;
			} else {
				global_stats.quorate = 0;
			}
		}

		if (strncmp(buffer, "Expected votes:", 15) == 0)
			sscanf(buffer, "%*s %*s %"SCNu32"", &global_stats.expected_votes);

		if (strncmp(buffer, "Highest expected:", 17) == 0)
			sscanf(buffer, "%*s %*s %"SCNu32"", &global_stats.highest_expected);

		if (strncmp(buffer, "Total votes:", 12) == 0)
			sscanf(buffer, "%*s %*s %"SCNu32"", &global_stats.total_votes);

		if (strncmp(buffer, "Quorum:", 7) == 0)
			sscanf(buffer, "%*s %"SCNu32"", &global_stats.quorum);
	}
	pclose(pf);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cfgtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		if (strstr(buffer, "FAULTY"))
			global_stats.ring_errors = 1;
	}
	pclose(pf); 
	return 0;
}

int
hacluster_refresh_corosync_ring(const char *ring_name, struct corosync_ring *rings)
{
	char buffer[4096];
	char *buffer_ptr;
	FILE *pf;	
	int ring_found = 0;
	
	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cfgtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		/* 
		 * The aim is to find the id and status lines for our corresponding
		 * ring indom based on the ring id (ring_name)
		 *
		 * Check for the exact match that we are in the RING (corosync < v2.99)
		 * or LINK (corosync v2.99.0+) details section for our given ring by its
		 * ring id (ring_name)
		 */
		if ((strstr(buffer, "RING ID") || strstr(buffer, "LINK ID")) && strstr(buffer, ring_name)) {
			ring_found = 1;
			
			/* Collect the ring number while are matching to our ring_name */
			sscanf(buffer, "%*s %*s %"SCNu32"", &rings->number);
		}

		if (ring_found) {
			/* Strip white space at beginning of our collection lines */
			buffer_ptr = buffer; 
			while(isspace((unsigned char)*buffer_ptr)) buffer_ptr++;

			/* Address field match on Corosync < v2.99 */
			if (strncmp(buffer_ptr, "id", 2) == 0) 
				sscanf(buffer_ptr, "%*s %*s %[^\n]", rings->address);

			/* Address field capture updated in Corosync v2.99.0+  */
			if (strncmp(buffer_ptr, "addr", 2) == 0) 
				sscanf(buffer_ptr, "%*s %*s %[^\n]", rings->address);
		
			if (strncmp(buffer_ptr, "status", 6) == 0){
				if (strstr(buffer_ptr, "FAULTY")) {
					rings->status = 1;
				} else {
					rings->status = 0;
				}
	
				/* 
				 * We've finished collecting for our requested rings, however we
				 * we need to ensure that we don't accidentally collect information
				 * for any other rings so break out
				 */
				break;
			}
		}
	}
	pclose(pf);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", quorumtool_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();
	
	/* 
	 * Check corosync-quorumtool for our node_id and ring_id values for our
	 * current node that we are collecting this data from.
	 */	
	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {	
		if (strncmp(buffer, "Node ID:", 2) == 0) 
			sscanf(buffer, "%*s %*s %"SCNu64"", &rings->node_id);
					
		if (strncmp(buffer, "Ring ID:", 2) == 0) 
			sscanf(buffer, "%*s %*s %s", rings->ring_id);
	}
	pclose(pf);
	return 0;
}

void
corosync_stats_setup(void)
{
	static char corosync_command_quorumtool[] = "corosync-quorumtool -p";
	static char corosync_command_cfgtool[] = "corosync-cfgtool -s";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_QUORUM")) != NULL)
		quorumtool_command = env_command;
	else
		quorumtool_command = corosync_command_quorumtool;

	/* further env setup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_CFG")) != NULL)
		cfgtool_command = env_command;
	else	
		cfgtool_command = corosync_command_cfgtool;
}
