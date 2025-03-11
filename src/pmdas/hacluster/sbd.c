/*
 * HA Cluster SBD statistics.
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

#include "sbd.h"

static char *sbd_command;

static char *sbd_status_healthy = "Healthy";
static char *sbd_status_unhealthy = "Unhealthy";

int
hacluster_sbd_device_fetch(int item, struct sbd *sbd, pmAtomValue *atom)
{
	/* check for bounds */
	if (item < 0 || item >= NUM_SBD_DEVICE_STATS)
		return PMDA_FETCH_NOVALUES;

	switch (item) {

		case SBD_DEVICE_PATH:
			atom->cp = sbd->path;
			return PMDA_FETCH_STATIC;

		case SBD_DEVICE_STATUS:
			atom->cp = sbd->status;
			return PMDA_FETCH_STATIC;

		case SBD_DEVICE_TIMEOUT_MSGWAIT:
			atom->ul = sbd->msgwait;
			return PMDA_FETCH_STATIC;

		case SBD_DEVICE_TIMEOUT_ALLOCATE:
			atom->ul = sbd->allocate;
			return PMDA_FETCH_STATIC;

		case SBD_DEVICE_TIMEOUT_LOOP:
			atom->ul = sbd->loop;
			return PMDA_FETCH_STATIC;

		case SBD_DEVICE_TIMEOUT_WATCHDOG:
			atom->ul = sbd->watchdog;
			return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;

	}
	return PMDA_FETCH_NOVALUES;
}

int 
hacluster_sbd_device_all_fetch(int item, pmAtomValue *atom)
{
	atom->ul = 1; /* Assign default exists value 1 */
	return PMDA_FETCH_STATIC;
}

int
hacluster_refresh_sbd_device(const char *sbd_dev, struct sbd *sbd)
{
	char buffer[4096];
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -d %s dump 2>&1", sbd_command, sbd_dev);

	if ((pf = popen(buffer, "r")) == NULL)
		return oserror();

	pmstrncpy(sbd->path, sizeof(sbd->path), sbd_dev);

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		if (strstr(buffer, "failed")){
			pmstrncpy(sbd->status, sizeof(sbd->status), sbd_status_unhealthy);
		} else {
			pmstrncpy(sbd->status, sizeof(sbd->status), sbd_status_healthy);
		}
		
		if (strncmp(buffer, "Timeout (watchdog)", 18) == 0)
			sscanf(buffer, "%*s %*s %*s %"SCNu32"", &sbd->watchdog);
			
		if (strncmp(buffer, "Timeout (allocate)", 18) == 0)
			sscanf(buffer, "%*s %*s %*s %"SCNu32"", &sbd->allocate);
	
		if (strncmp(buffer, "Timeout (loop)", 14) == 0)
			sscanf(buffer, "%*s %*s %*s %"SCNu32"", &sbd->loop);
			
		if (strncmp(buffer, "Timeout (msgwait)", 17) == 0)
			sscanf(buffer, "%*s %*s %*s %"SCNu32"", &sbd->msgwait);
	}
	pclose(pf);
	return 0;
}

void
sbd_stats_setup(void)
{
	static char sbd_command_sbd[] = "sbd";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("HACLUSTER_SETUP_SBD")) != NULL)
		sbd_command = env_command;
	else
		sbd_command = sbd_command_sbd;
}
