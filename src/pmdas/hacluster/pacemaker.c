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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

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
hacluster_pacemaker_fail_fetch(int item, struct fail_count *fail_count, pmAtomValue *atom)
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
hacluster_pacemaker_constraints_fetch(int item, struct location_constraints *locations_constraints, pmAtomValue *atom)
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
hacluster_pacemaker_nodes_fetch(int item, struct nodes *nodes, pmAtomValue *atom)
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
hacluster_pacemaker_node_attribs_fetch(int item, struct attributes *attributes, pmAtomValue *atom)
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
hacluster_pacemaker_resources_fetch(int item, struct resources *resources, pmAtomValue *atom)
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
hacluster_refresh_pacemaker_fail(const char *instance_name, struct fail_count *fail_count)
{
	char buffer[4096];
	char *node, *resource_id, *tofree, *str;
	int found_node_history = 0, found_node_name = 0;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	/* 
	 * We need to split our combined NODE:RESOURCE_ID instance names into their
	 * separated NODE and RESOURCE_ID fields for matching in the output
	 */
	tofree = str = strdup(instance_name);
	node = strsep(&str, ":");
	resource_id = strsep(&str, ":");

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		/* First we need to check whether we are in <node_history> section*/
		if (strstr(buffer, "<node_history>")) {
			found_node_history = 1;
			continue;
		}

		/* Find the node name for our resource */
		if (strstr(buffer, "node name=") && strstr(buffer, node) && found_node_history ) {
			found_node_name = 1;
			continue;
		}

		/* Check for when we overrun to another node */
		if (strstr(buffer, "</node>")) {
			found_node_name = 0;
			continue;
		}

		/* Record our instance as node:resource-id and assign */
		if (strstr(buffer, "resource_history id=") && strstr(buffer, resource_id) && found_node_name) {
			sscanf(buffer, "%*s %*s %*s migration-threshold=\"%"SCNu64"\" fail-count=\"%"SCNu64"\"",
				&fail_count->migration_threshold, 
				&fail_count->fail_count
			);
		}
	}
	pclose(pf);
	free(tofree);
	return 0;
}

int
hacluster_refresh_pacemaker_constraints(const char *constraints_name, struct location_constraints *location_constraints)
{
	char buffer[4096];
	int found_constraints = 0;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", cibadmin_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
	
		/* First we need to check whether we are in <constraints> section*/
		if (strstr(buffer, "<constraints>")) {
			found_constraints = 1;
			continue;
		}

		/* Find the node name for our resource */
		if (strstr(buffer, "rsc_location id=") && strstr(buffer, constraints_name) && found_constraints) {
			sscanf(buffer, "%*s %*s rsc=\"%[^\"]\" role=\"%[^\"]\" node=\"%[^\"]\" score=\"%[^\"]\"",
				location_constraints->resource, 
				location_constraints->role, 
				location_constraints->node, 
				location_constraints->score
			); 
		}	
	}
	pclose(pf);
	return 0;
}

int
hacluster_refresh_pacemaker_nodes(const char *node_name, struct nodes *nodes)
{
	char buffer[4096];
	int found_nodes = 0;
	FILE *pf;

	char online[10], standby[10], standby_on_fail[10], maintenance[10], pending[10];
	char unclean[10], shutdown[10], expected_up[10], dc[10];

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

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
		if (found_nodes && strstr(buffer, node_name)) {
		        if(strstr(buffer, "feature_set")) {
		                sscanf(buffer, "%*s %*s %*s online=\"%9[^\"]\" standby=\"%9[^\"]\" standby_onfail=\"%9[^\"]\" maintenance=\"%9[^\"]\" pending=\"%9[^\"]\" unclean=\"%9[^\"]\" health=\"%9[^\"]\" feature_set =\"%9[^\"]\" shutdown=\"%9[^\"]\" expected_up=\"%9[^\"]\" is_dc =\"%9[^\"]\" %*s type=\"%6[^\"]\"",
				        online,
				        standby,
				        standby_on_fail,
				        maintenance,
				        pending,
				        unclean,
				        nodes->health,
				        nodes->feature_set,
				        shutdown,
				        expected_up,
				        dc,
				        nodes->type
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
				        nodes->type
			        );
			}
									
			nodes->online = bool_convert(online);
			nodes->standby = bool_convert(standby);
			nodes->standby_on_fail = bool_convert(standby_on_fail);
			nodes->maintenance = bool_convert(maintenance);
			nodes->pending = bool_convert(pending);
			nodes->unclean = bool_convert(unclean);
			nodes->shutdown = bool_convert(shutdown);
			nodes->expected_up = bool_convert(expected_up);
			nodes->dc = bool_convert(dc);
		}
	}
	pclose(pf);
	return 0;
}

int
hacluster_refresh_pacemaker_node_attribs(const char *attrib_name, struct attributes *attributes)
{
	char buffer[4096];
	char *node_name, *attribute_name, *tofree, *str;
	int found_node_attributes = 0, found_node_name = 0;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s 2>&1", crm_mon_command);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	/* 
	 * We need to split our combined NODE:ATTRIBUTE_NAME instance names into their
	 * separated NODE and ATTRIBUTE_NAME fields for matching in the output
	 */
	tofree = str = strdup(attrib_name);
	node_name = strsep(&str, ":");
	attribute_name = strsep(&str, ":");

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
		if (strstr(buffer, "node name=") && strstr(buffer, node_name) && found_node_attributes ) {
			found_node_name = 1;
			continue;
		}

		/* Check for when we overrun to another node */
		if (strstr(buffer, "</node>")) {
			found_node_name = 0;
			continue;
		}

		/* Record our instance as node:resource-id and assign */
		if (found_node_attributes && strstr(buffer, attribute_name) && found_node_name) {
			sscanf(buffer, "%*s %*s value=\"%[^\"]\"", attributes->value);
		}
	}
	pclose(pf);
	free(tofree);
	return 0;
}

int
hacluster_refresh_pacemaker_resources(const char *instance_name, struct resources *resources)
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
	static char pacemaker_command_cibadmin[] = "cibadmin --query --local";
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
