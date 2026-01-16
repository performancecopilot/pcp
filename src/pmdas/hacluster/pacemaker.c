/*
 * HA Cluster Pacemaker statistics.
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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "pmdahacluster.h"
#include "pacemaker.h"

#include <time.h>

static char *cibadmin_command;
static char *crm_mon_command;

static struct pacemaker_global global_stats;

uint64_t
dateToEpoch(char *last_written_text)
{
	char str_wday[4], str_mon[4]; 
	int year, last_change_utc;
	struct tm cib_written;

	/* Initialize DST status as unknown (-1) avoiding uninitialized variable */
	cib_written.tm_isdst = -1;

	/* Token our time string into its components */
	sscanf(last_written_text, "%s %s %d %d:%d:%d %d",
		str_wday,
		str_mon,
		&cib_written.tm_mday,
		&cib_written.tm_hour,
		&cib_written.tm_min,
		&cib_written.tm_sec,
		&year
	);

	/* Year counting starts at 1900 */
	cib_written.tm_year = (year - 1900);

	/* Convert weekday to int */
	if (strstr(str_wday, "Sun"))
		cib_written.tm_wday = 0;

	if (strstr(str_wday, "Mon"))
		cib_written.tm_wday  = 1;

	if (strstr(str_wday, "Tue"))
		cib_written.tm_wday  = 2;

	if (strstr(str_wday, "Wed"))
		cib_written.tm_wday  = 3;

	if (strstr(str_wday, "Thu"))
		cib_written.tm_wday  = 4;

	if (strstr(str_wday, "Fri"))
		cib_written.tm_wday  = 5;

	if (strstr(str_wday, "Sat"))
		cib_written.tm_wday  = 6;

	/* Same for month */
	if (strstr(str_mon, "Jan"))
		cib_written.tm_mon  = 0;

	if (strstr(str_mon, "Feb"))
		cib_written.tm_mon = 1;

	if (strstr(str_mon, "Mar"))
		cib_written.tm_mon = 2;

	if (strstr(str_mon, "Apr"))
		cib_written.tm_mon = 3;

	if (strstr(str_mon, "May"))
		cib_written.tm_mon = 4;

	if (strstr(str_mon, "Jun"))
		cib_written.tm_mon = 5;

	if (strstr(str_mon, "Jul"))
		cib_written.tm_mon = 6;

	if (strstr(str_mon, "Aug"))
		cib_written.tm_mon = 7;

	if (strstr(str_mon, "Sep"))
		cib_written.tm_mon = 8;

	if (strstr(str_mon, "Oct"))
		cib_written.tm_mon = 9;

	if (strstr(str_mon, "Nov"))
		cib_written.tm_mon = 10;

	if (strstr(str_mon, "Dec"))
		cib_written.tm_mon = 11;
		
	cib_written.tm_yday = 12;
	mktime(&cib_written);

	/*
	 * Return seconds since the Epoch in UTC from localtime time_t outlined by 
	 * Open Group Base Specification
	 * Issue 7, 2018 Edition (IEEE Std 1003.1-2017) - Section 4.16
	 * POSIX.1-2017
	 *
	 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap04.html
	 *
	 * Note: The last three terms of the expression take into consideration of
	 *       adding a leap day for each leap year since epoch.
	 * 
	 */
	last_change_utc = cib_written.tm_sec + cib_written.tm_min*60 + cib_written.tm_hour*3600 + cib_written.tm_yday*86400 +
					  (cib_written.tm_year-70)*31536000 + ((cib_written.tm_year-69)/4)*86400 -
					  ((cib_written.tm_year-1)/100)*86400 + ((cib_written.tm_year+299)/400)*86400;
	        
	return (uint64_t)last_change_utc; 
}

uint8_t
bool_convert(char *input)
{
	if (strstr(input, "true"))
		return 1;
	else 
		return 0;
}

int
hacluster_pacemaker_global_fetch(int item, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_GLOBAL_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_CONFIG_LAST_CHANGE:
			atom->ull = global_stats.config_last_change;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_STONITH_ENABLED:
			atom->ul = global_stats.stonith_enabled;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_fail_fetch(int item, struct pacemaker_fail *fail_count, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_FAIL_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_FAIL_COUNT:
			atom->ull = fail_count->fail_count;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_MIGRATION_THRESHOLD:
			atom->ull = fail_count->migration_threshold;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_constraints_fetch(int item, struct pacemaker_constraints *locations_constraints, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_CONSTRAINTS_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_CONSTRAINTS_NODE:
			atom->cp = locations_constraints->node;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_CONSTRAINTS_RESOURCE:
			atom->cp = locations_constraints->resource;
			return PMDA_FETCH_STATIC;
	
		case PACEMAKER_CONSTRAINTS_ROLE:
			atom->cp = locations_constraints->role;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_CONSTRAINTS_SCORE:
			atom->cp = locations_constraints->score;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_constraints_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_pacemaker_nodes_fetch(int item, struct pacemaker_nodes *nodes, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_NODES_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_NODES_ONLINE:
			atom->ul = nodes->online;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_STANDBY:
			atom->ul = nodes->standby;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_STANDBY_ONFAIL:
			atom->ul = nodes->standby_on_fail;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_MAINTENANCE:
			atom->ul = nodes->maintenance;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_PENDING:
			atom->ul = nodes->pending;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_UNCLEAN:
			atom->ul = nodes->unclean;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_SHUTDOWN:
			atom->ul = nodes->shutdown;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_EXPECTED_UP:
			atom->ul = nodes->expected_up;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_DC:
			atom->ul = nodes->dc;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_TYPE:
			atom->cp = nodes->type;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_HEALTH:
			atom->cp = nodes->health;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_NODES_FEATURE_SET:
			atom->cp = nodes->feature_set;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_node_attribs_fetch(int item, struct pacemaker_node_attrib *attributes, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_NODE_ATTRIB_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_NODES_ATTRIB_VALUE:
			atom->cp = attributes->value;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_node_attribs_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_pacemaker_resources_fetch(int item, struct pacemaker_resources *resources, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_PACEMAKER_RESOURCES_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case PACEMAKER_RESOURCES_AGENT:
			atom->cp = resources->agent;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_CLONE:
			atom->cp = resources->clone;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_GROUP:
			atom->cp = resources->group;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_MANAGED:
			atom->ul = resources->managed;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_ROLE:
			atom->cp = resources->role;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_ACTIVE:
			atom->ul = resources->active;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_ORPHANED:
			atom->ul = resources->orphaned;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_BLOCKED:
			atom->ul = resources->blocked;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_FAILED:
			atom->ul = resources->failed;
			return PMDA_FETCH_STATIC;

		case PACEMAKER_RESOURCES_FAILURE_IGNORED:
			atom->ul = resources->failure_ignored;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int
hacluster_pacemaker_resources_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_refresh_pacemaker_global()
{
	char buffer[4096];
	char last_written_text[128], stonith[6];
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cibadmin_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		if (strstr(buffer, "cib-last-written=")) {
			sscanf(buffer, "<cib %*s %*s %*s %*s %*s cib-last-written=\"%[^\"]]", last_written_text);
			global_stats.config_last_change = dateToEpoch(last_written_text);
		}
	}
	pclose(pf);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		if (strstr(buffer, "<cluster_options stonith-enabled=")) {
			sscanf(buffer, "\t<cluster_options stonith-enabled=\"%[^\"]]", stonith);

			if (strstr(stonith, "true"))
				global_stats.stonith_enabled = 1;
			else
				global_stats.stonith_enabled = 0;
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_pacemaker_fail_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], instance_name[256], node_name[128], resource_name[127];
	int			found_node_history = 0, found_node_name = 0;
	FILE		*pf;
	pmInDom		indom = hacluster_indom(PACEMAKER_FAIL_INDOM);

	/*
	 * Update indom cache based off the reading of crm_mon listed in
	 * the output from crm_mon
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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

				struct pacemaker_fail *fail;

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
				
				sscanf(buffer, "%*s %*s %*s migration-threshold=\"%"SCNu64"\" fail-count=\"%"SCNu64"\"",
					&fail->migration_threshold, 
					&fail->fail_count
				);

				pmdaCacheStore(indom, PMDA_CACHE_ADD, instance_name, (void *)fail);
			}
		}
	}
	pclose(pf);	
	return 0;
}

int
hacluster_pacemaker_constraints_instance_refresh(void)
{
	int			sts;
	char		buffer[4096], constraint_name[256];
	int			found_constraints = 0;
	FILE		*pf;
	pmInDom		indom = hacluster_indom(PACEMAKER_CONSTRAINTS_INDOM);
	pmInDom		indom_all = hacluster_indom(PACEMAKER_CONSTRAINTS_ALL_INDOM);

	/*
	 * Update indom cache based off the reading of cibadmin listed in
	 * the output from cibadmin
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	pmdaCacheOp(indom_all, PMDA_CACHE_INACTIVE);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cibadmin_command);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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

			sscanf(buffer, "%*s %*s rsc=\"%[^\"]\" role=\"%[^\"]\" node=\"%[^\"]\" score=\"%[^\"]\"",
				constraints->resource, 
				constraints->role, 
				constraints->node, 
				constraints->score
			); 

			pmdaCacheStore(indom, PMDA_CACHE_ADD, constraint_name, (void *)constraints);
			pmdaCacheStore(indom_all, PMDA_CACHE_ADD, constraint_name, NULL);
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
	pmInDom		indom = hacluster_indom(PACEMAKER_NODES_INDOM);

	char online[10], standby[10], standby_on_fail[10], maintenance[10], pending[10];
	char unclean[10], shutdown[10], expected_up[10], dc[10];

	/*
	 * Update indom cache based off the reading of crm_mon listed in
	 * the output from crm_mon
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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

				if(strstr(buffer, "feature_set")) {
		                	sscanf(buffer, "%*s %*s %*s online=\"%9[^\"]\" standby=\"%9[^\"]\" standby_onfail=\"%9[^\"]\" maintenance=\"%9[^\"]\" pending=\"%9[^\"]\" unclean=\"%9[^\"]\" health=\"%9[^\"]\" feature_set =\"%9[^\"]\" shutdown=\"%9[^\"]\" expected_up=\"%9[^\"]\" is_dc =\"%9[^\"]\" %*s type=\"%6[^\"]\"",
					        online,
					        standby,
					        standby_on_fail,
					        maintenance,
					        pending,
					        unclean,
					        pace_nodes->health,
					        pace_nodes->feature_set,
					        shutdown,
					        expected_up,
				        	dc,
				        	pace_nodes->type
			        	);
		        	} else {
			        	sscanf(buffer, "%*s %*s %*s online=\"%9[^\"]\" standby=\"%9[^\"]\" standby_onfail=\"%9[^\"]\" maintenance=\"%9[^\"]\" pending=\"%9[^\"]\" unclean=\"%9[^\"]\" shutdown=\"%9[^\"]\" expected_up=\"%9[^\"]\" is_dc =\"%9[^\"]\" %*s type=\"%6[^\"]\"",
					        online,
					        standby,
					        standby_on_fail,
					        maintenance,
				        	pending,
					        unclean,
					        shutdown,
					        expected_up,
					        dc,
					        pace_nodes->type
				        );
				}
									
				pace_nodes->online = bool_convert(online);
				pace_nodes->standby = bool_convert(standby);
				pace_nodes->standby_on_fail = bool_convert(standby_on_fail);
				pace_nodes->maintenance = bool_convert(maintenance);
				pace_nodes->pending = bool_convert(pending);
				pace_nodes->unclean = bool_convert(unclean);
				pace_nodes->shutdown = bool_convert(shutdown);
				pace_nodes->expected_up = bool_convert(expected_up);
				pace_nodes->dc = bool_convert(dc);

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
	pmInDom		indom = hacluster_indom(PACEMAKER_NODE_ATTRIB_INDOM);
	pmInDom		indom_all = hacluster_indom(PACEMAKER_NODE_ATTRIB_ALL_INDOM);

	/*
	 * Update indom cache based off the reading of crm_mon listed in
	 * the output from crm_mon
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	pmdaCacheOp(indom_all, PMDA_CACHE_INACTIVE);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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
				
				sscanf(buffer, "%*s %*s value=\"%[^\"]\"", node_attrib->value);

				pmdaCacheStore(indom, PMDA_CACHE_ADD, instance_name, (void *)node_attrib);
				pmdaCacheStore(indom_all, PMDA_CACHE_ADD, instance_name, NULL);
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
	pmInDom		indom= hacluster_indom(PACEMAKER_RESOURCES_INDOM);
	pmInDom		indom_all = hacluster_indom(PACEMAKER_RESOURCES_ALL_INDOM);

	/*
	 * Update indom cache based off the reading of crm_mon listed in
	 * the output from crm_mon
	 */
	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
	pmdaCacheOp(indom_all, PMDA_CACHE_INACTIVE);

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

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
				 * with our volume number but only if we have both a resource name and a node_id  
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
				pmdaCacheStore(indom_all, PMDA_CACHE_ADD, instance_name, NULL);

				/* Clear node name in the event that a resource has not got a node attachment */
				memset(node_name, '\0', sizeof(node_name));
			}
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_refresh_pacemaker_resources(const char *instance_name, struct pacemaker_resources *resources)
{
	char buffer[4096];
	char *node, *resource_id, *tofree, *str;
	int found_resources = 0, no_node_attachment = 0, found_instance = 0, found_resource = 0;
	FILE *pf;

	char active[8], orphaned[8], blocked[8], managed[8], failed[8], failure_ignored[8];

	/* 
	 * We need to split our combined RESOURCE_ID:NODE instance names into their
	 * separated RESOURCE_ID:NODE fields for matching in the output. There may be
	 * cases where a resource is not tied to a node so NODE may be blank
	 */
	if (strchr(instance_name, ':') == NULL) {
		resource_id = (char*)instance_name;
		no_node_attachment = 1;
		node = NULL;
	} else {
		tofree = str = strdup(instance_name);
		resource_id = strsep(&str, ":");
		node = strsep(&str, ":");
	}

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL) {
		if (!no_node_attachment)
		    free(tofree);
		return -oserror();
	}

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

			/* If we are in a clone, record the id */
			if (strstr(buffer, "clone id=")) {
				sscanf(buffer, "\t<clone id=\"%[^\"]\"", resources->clone);
			}

			/* Clear clone value if we overrun */
			if (strstr(buffer, "</clone>")) {
				memset(resources->clone, '\0', sizeof(resources->clone));
			}

			/* If we are in a group, record the id */
			if (strstr(buffer, "group id=")) {
				sscanf(buffer, "\t<group id=\"%[^\"]\"", resources->group);
			}

			/* Clear group value if we overrun */
			if (strstr(buffer, "</group>")) {
				memset(resources->group, '\0', sizeof(resources->group));
			}

			/* Collect our metrics */
			if (strstr(buffer, "resource id=") && strstr(buffer, resource_id)) {

                                /* Pacemaker v2.14 and prior crm_mon format */
				if (strstr(buffer, "target_role")) { 
					sscanf(buffer, "%*s %*s resource_agent=\"%[^\"]\" role=\"%[^\"]\" %*s active=\"%7[^\"]\" orphaned=\"%7[^\"]\" blocked=\"%7[^\"]\" managed=\"%7[^\"]\" failed=\"%7[^\"]\" failure_ignored=\"%7[^\"]\"",
						resources->agent,
						resources->role,
						active,
						orphaned,
						blocked,
						managed,
						failed,
						failure_ignored
					);

                                /* Pacemaker v2.16+ crm_mon format */
                                } else if (strstr(buffer, "maintenance")) {
					sscanf(buffer, "%*s %*s resource_agent=\"%[^\"]\" role=\"%[^\"]\" active=\"%7[^\"]\" orphaned=\"%7[^\"]\" blocked=\"%7[^\"]\" %*s managed=\"%7[^\"]\" failed=\"%7[^\"]\" failure_ignored=\"%7[^\"]\"",
						resources->agent,
						resources->role,
						active,
						orphaned,
						blocked,
						managed,
						failed,
						failure_ignored
					);

				/* Pacemaker v2.15 crm_mon format */
				} else {
					sscanf(buffer, "%*s %*s resource_agent=\"%[^\"]\" role=\"%[^\"]\" active=\"%7[^\"]\" orphaned=\"%7[^\"]\" blocked=\"%7[^\"]\" managed=\"%7[^\"]\" failed=\"%7[^\"]\" failure_ignored=\"%7[^\"]\"",
						resources->agent,
						resources->role,
						active,
						orphaned,
						blocked,
						managed,
						failed,
						failure_ignored
					);
				}

				resources->active = bool_convert(active);
				resources->orphaned = bool_convert(orphaned);
				resources->blocked = bool_convert(blocked);
				resources->managed = bool_convert(managed);
				resources->failed = bool_convert(failed);
				resources->failure_ignored = bool_convert(failure_ignored);

				if (no_node_attachment)
					found_instance = 1;
				else
					found_resource = 1;

			}

			/* Guard to see if NODE is not NULL before entering */
			if (!no_node_attachment && node != NULL) {
				if (strstr(buffer, "node name=") && strstr(buffer, node)) {
					if (found_resource)
						found_instance = 1;
				}
			}

			if (strstr(buffer, "/>") && found_instance)
				break;
		}
	}
	pclose(pf);

	if (!no_node_attachment)
		free(tofree);

	return 0;
}

void
pacemaker_stats_setup(void)
{
	static char pacemaker_command_cibadmin[] = "cibadmin --query";
	static char pacemaker_command_crm_mon[] = "crm_mon -X --inactive";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_CIBADMIN")) != NULL)
		cibadmin_command = env_command;
	else
		cibadmin_command = pacemaker_command_cibadmin;

	/* further env setup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_CRM_MON")) != NULL)
		crm_mon_command = env_command;
	else	
		crm_mon_command = pacemaker_command_crm_mon;
}
