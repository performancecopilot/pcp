/*
 * S.M.A.R.T stats using smartcl and smartmontools
 *
 * Copyright (c) 2018 Red Hat.
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

#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"

#include "smart_stats.h"

static char *smart_setup_stats;

int
smart_device_info_fetch(int item, struct device_info *device_info, pmAtomValue *atom)
{
	switch (item) {
                
		case HEALTH:
			atom->cp = device_info->health;
			return 1;

		case MODEL_FAMILY: /* Note: There is not always a model family value */
			if (strlen(device_info->model_family) == 0)
				return 0;

			atom->cp = device_info->model_family;
			return 1;

		case DEVICE_MODEL:
			atom->cp = device_info->device_model;
			return 1;

		case SERIAL_NUMBER:
			atom->cp = device_info->serial_number;
			return 1;

		case CAPACITY_BYTES:
			atom->ull = device_info->capacity_bytes;
			return 1;

		case SECTOR_SIZE:
			atom->cp = device_info->sector_size;
			return 1;

		case ROTATION_RATE:
			atom->cp = device_info->rotation_rate;
			return 1;		

		default:
			return PM_ERR_PMID;
	}
	return 0;
}

int
smart_data_fetch(int item, int cluster, struct smart_data *smart_data, pmAtomValue *atom)
{
	/*
	 * Check for empty ID field, if so we have no data for that
	 * S.M.A.R.T ID and return.
	 */
	if (smart_data->id[cluster] == 0)
		return 0;

	switch (item) {
                
		case SMART_ID:
			atom->ul = smart_data->id[cluster];
			return 1;

		case SMART_VALUE:
			atom->ul = smart_data->value[cluster];
			return 1;

		case SMART_WORST:
			atom->ul = smart_data->worst[cluster];
			return 1;

		case SMART_THRESH:
			atom->ul = smart_data->thresh[cluster];
			return 1;

		case SMART_RAW:
			atom->ull = smart_data->raw[cluster];
			return 1;

		default:
			return PM_ERR_PMID;
	}
	return 0;

}

int
smart_refresh_device_info(const char *name, struct device_info *device_info)
{
	char buffer[4096], capacity[64], *r, *w;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -Hi /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		if (strncmp(buffer, "Model Family:", 13) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->model_family);

		if (strncmp(buffer, "Device Model:", 13) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->device_model);

		if (strncmp(buffer, "Serial Number:", 14) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->serial_number);

		if (strncmp(buffer, "User Capacity:", 14) == 0) {
			sscanf(buffer, "%*s%*s %s", capacity);

			/* convert comma separated capacity string to uint64 */
			for (w = r = capacity; *r; r++) if (*r != ',') *w++ = *r;
			device_info->capacity_bytes = strtoll(capacity, NULL, 10);
		}

		if (strncmp(buffer, "Sector Size:", 12) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->sector_size);

		if (strncmp(buffer, "Rotation Rate:", 14) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->rotation_rate);
	
		if (strncmp(buffer, "SMART overall-health", 20) == 0)
			sscanf(buffer, " %*s %*s %*s %*s %*s %s", device_info->health);

	}
	pclose(pf);
	return 0;
}

int
smart_refresh_data(const char *name, struct smart_data *smart_data)
{
	char buffer[MAXPATHLEN];
	FILE *pf;

	uint8_t id, value, worst, thresh; 
	uint32_t raw;

	pmsprintf(buffer, sizeof(buffer), "%s -A /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

		/* Check if we are looking at smart data by checking the character 
		 * in the ID# field is actually a digit (the field is right aligned)
		 * so we are looking at rightmost digit.
		 *
		 * We also handle the trailing blank line for attribute output at the
		 * end of the table by ignoring blank lines of input
		 */
		if((buffer[2] >= '0' && buffer[2] <= '9') && (buffer[0] != '\n')) {

			/* smartmontools attribute table layout:
			ID# ATTRIBUTE_NAME FLAG VALUE WORST THRESH TYPE UPDATED WHEN_FAILED RAW_VALUE
			*/
			sscanf(buffer, "%"SCNu8" %*s %*x %"SCNu8" %"SCNu8" %"SCNu8" %*s %*s %*s %"SCNu32"", 
				&id, 
				&value, 
				&worst, 
				&thresh, 
				&raw
			);

			/*
			 * Apply smart data values, id directly links with smart value id,
			 * not efficent but allows easy expansion with new smart values
			 */
			smart_data->id[id] = id;
			smart_data->value[id] = value;
			smart_data->worst[id] = worst;
			smart_data->thresh[id] = thresh;
			smart_data->raw[id] = raw;
		}
	}
	pclose(pf);
	return 0;
}

void
smart_stats_setup(void)
{
	static char smart_command[] = "smartctl";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("SMART_SETUP")) != NULL)
		smart_setup_stats = env_command;
	else
		smart_setup_stats = smart_command;
}
