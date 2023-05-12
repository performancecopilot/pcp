/*
 * S.M.A.R.T stats using smartctl and smartmontools
 *
 * Copyright (c) 2018-2023 Red Hat.
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
static char *smart_setup_nvmecli;

static int _POWER_STATE_CLUSTER_OFFSET = 257;

static int nvmecli_support = 1; // Assume we have support by default until checked

/*
 * NVME Spec allows for S.M.A.R.T attribute field formats to be 
 * determined by the individual manufacturers, some use comma dilimeters
 * and others use space delimiters.
 *
 * Strip both so we can easily convert to integers for metric values.
 *
 */
char
smart_strip_input(char *units)
{
	char *r, *w;
	
	/* strip comma separated units from string */
	for (w = r = units; *r; r++) if (*r != ',') *w++ = *r;
	
	*w = '\0';
	
	r = units;
	
	/* second pass to strip space separators from string */
	do while (*r == ' ') r++; while (((*units++) = *r++));

	return *units;
}

int
smart_device_info_fetch(int item, struct device_info *device_info, pmAtomValue *atom)
{
	switch (item) {
                
		case HEALTH:
			atom->cp = device_info->health;
			return PMDA_FETCH_STATIC;

		case MODEL_FAMILY: /* Note: There is not always a model family value */
			if (strlen(device_info->model_family) == 0)
				return PMDA_FETCH_NOVALUES;

			atom->cp = device_info->model_family;
			return PMDA_FETCH_STATIC;

		case DEVICE_MODEL:
			atom->cp = device_info->device_model;
			return PMDA_FETCH_STATIC;

		case SERIAL_NUMBER:
			atom->cp = device_info->serial_number;
			return PMDA_FETCH_STATIC;

		case CAPACITY_BYTES:
			atom->ull = device_info->capacity_bytes;
			return PMDA_FETCH_STATIC;

		case SECTOR_SIZE: /* Note: There is not always a sector size value */
			if (strlen(device_info->sector_size) == 0)
				return PMDA_FETCH_NOVALUES;
				
			atom->cp = device_info->sector_size;
			return PMDA_FETCH_STATIC;

		case ROTATION_RATE: /* Note: There is not always a ratation rate value */
			if (strlen(device_info->rotation_rate) == 0)
				return PMDA_FETCH_NOVALUES;
				
			atom->cp = device_info->rotation_rate;
			return PMDA_FETCH_STATIC;
			
		case FIRMWARE_VERSION: /* Note: There is not always a firmware version */
			if (strlen(device_info->firmware_version) == 0)
				return PMDA_FETCH_NOVALUES;
				
			atom->cp = device_info->firmware_version;
			return PMDA_FETCH_STATIC;			

		default:
			return PM_ERR_PMID;
	}
	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
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
nvme_device_info_fetch(int item, int cluster, struct nvme_device_info *nvme_device_info, pmAtomValue *atom, int is_nvme)
{
	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
		return 0;

	switch (item) {

                case NVME_MODEL_NUMBER:
                        atom->cp = nvme_device_info->model_number;
                        return 1;

                case NVME_SERIAL_NUMBER:
                        atom->cp = nvme_device_info->serial_number;
                        return 1;

                case NVME_FIRMWARE_VERSION:
                        atom->cp = nvme_device_info->firmware_version;
                        return 1;

                case NVME_PCI_VENDOR_SUBSYSTEM_ID:
                        atom->cp = nvme_device_info->pci_vendor_subsystem_id;
                        return 1;

                case NVME_IEE_OUI_IDENTIFIER:
                        atom->cp = nvme_device_info->ieee_oui_identifier;
                        return 1;

                case NVME_TOTAL_NVM_CAPACITY:
                        atom->ull = nvme_device_info->total_nvm_capacity;
                        return 1;

                case NVME_UNALLOCATED_NVM_CAPACITY:
                        atom->ull = nvme_device_info->unallocated_nvm_capacity;
                        return 1;

                case NVME_CONTROLLER_ID:
                        atom->ul = nvme_device_info->controller_id;
                        return 1;

                case NVME_NVME_VERSION:
                        atom->cp = nvme_device_info->nvme_version;
                        return 1;

                case NVME_NAMESPACES:
                        atom->ul = nvme_device_info->namespaces;
                        return 1;

                case NVME_FIRMWARE_UPDATES:
                        /* NVME Spec 1.4 metric, if no value return */
			if (strlen(nvme_device_info->firmware_updates) == 0)
				return PMDA_FETCH_NOVALUES;

                        atom->cp = nvme_device_info->firmware_updates;
                        return 1;

                case NVME_MAXIMUM_DATA_TRANSFER_SIZE:
                        /* NVME Spec 1.4 metric, if no value return */
                        if (nvme_device_info->maximum_data_transfer_size == 0)
				return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->maximum_data_transfer_size;
                        return 1;

                case NVME_WARNING_TEMP_THRESHOLD:
                        /* NVME Spec 1.4 metric, if no value return */
                        if (nvme_device_info->warning_temp_threshold == 0)
				return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->warning_temp_threshold;
                        return 1;

                case NVME_CRITICAL_TEMP_THRESHOLD:
                        /* NVME Spec 1.4 metric, if no value return */
                        if (nvme_device_info->critical_temp_threshold == 0)
				return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->critical_temp_threshold;
                        return 1;

                case NVME_NAMESPACE_1_CAPACITY:
                        atom->ull = nvme_device_info->namespace_1_capacity;
                        return 1;

                case NVME_NAMESPACE_1_UTILIZATION:
                        atom->ull = nvme_device_info->namespace_1_utilization;
                        return 1;

                case NVME_NAMESPACE_1_FORMATTED_LBA_SIZE:
                        atom->ul = nvme_device_info->namespace_1_formatted_lba_size;
                        return 1;

                case NVME_NAMESPACE_1_IEEE_EUI_64:
                        atom->cp = nvme_device_info->namespace_1_ieee_eui_64;
                        return 1;

                case NVME_ACTIVE_POWER_STATE:
                        if (!nvmecli_support)
                                return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->active_power_state;
                        return 1;  

                case NVME_APST_STATE:
                        if (!nvmecli_support)
                                return PMDA_FETCH_NOVALUES;

                        atom->cp = nvme_device_info->apst_state;
                        return 1;  

                case NVME_COMPLETION_QUEUE_LENGTH_COMPLETION:
                        if (!nvmecli_support)
                                return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->completion_queue_length_completion;
                        return 1;  

                case NVME_COMPLETION_QUEUE_LENGTH_SUBMISSION:
                        if (!nvmecli_support)
                                return PMDA_FETCH_NOVALUES;

                        atom->ul = nvme_device_info->completion_queue_length_submission;
                        return 1;  

		default:
			return PM_ERR_PMID;
	}
	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
}

int
nvme_smart_data_fetch(int item, int cluster, struct nvme_smart_data *nvme_smart_data, pmAtomValue *atom, int is_nvme)
{
	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
		return 0;

	switch (item) {

		case CRITICAL_WARNING:
			atom->cp = nvme_smart_data->critical_warning;
			return 1;
			
		case COMPOSITE_TEMPERATURE:
			atom->ul = nvme_smart_data->composite_temperature;
			return 1;
			
		case AVAILABLE_SPARE:
			atom->ul = nvme_smart_data->available_spare;
			return 1;
			
		case AVAILABLE_SPARE_THRESHOLD:
			atom->ul = nvme_smart_data->available_spare_threshold;
			return 1;
			
		case PERCENTAGE_USED:
			atom->ul = nvme_smart_data->percentage_used;
			return 1;
			
		case DATA_UNITS_READ:
			atom->ull = nvme_smart_data->data_units_read;
			return 1;
			
		case DATA_UNITS_WRITTEN:
			atom->ull = nvme_smart_data->data_units_written;
			return 1;
			
		case HOST_READ_COMMANDS: 
			atom->ull = nvme_smart_data->host_read_commands;
			return 1;
			
		case HOST_WRITES_COMMANDS:
			atom->ull = nvme_smart_data->host_write_commands;
			return 1;
			
		case CONTROLLER_BUSY_TIME:
			atom->ul = nvme_smart_data->controller_busy_time;
			return 1;
			
		case POWER_CYCLES:
			atom->ul = nvme_smart_data->power_cycles;
			return 1;
			
		case NVME_POWER_ON_HOURS:
			atom->ul = nvme_smart_data->power_on_hours;
			return 1;
			
		case UNSAFE_SHUTDOWNS:
			atom->ul = nvme_smart_data->unsafe_shutdowns;
			return 1;
			
		case MEDIA_AND_DATA_INTEGRITY_ERRORS:
			atom->ul = nvme_smart_data->media_and_data_integrity_errors;
			return 1;
			
		case NUMBER_OF_ERROR_INFORMATION_LOG_ENTRIES:
			atom->ul = nvme_smart_data->number_of_error_information_log_entries;
			return 1;
			
		case WARNING_COMPOSITE_TEMPERATRE_TIME:
			atom->ul = nvme_smart_data->warning_composite_temperature_time;
			return 1;
			
		case CRITICAL_COMPOSITE_TEMPERATURE_TIME:
			atom->ul = nvme_smart_data->critical_composite_temperature_time;
			return 1;
			
		case TEMPERATURE_SENSOR_ONE:
			atom->ul = nvme_smart_data->temperature_sensor_one;
			return 1;
			
		case TEMPERATURE_SENSOR_TWO:
			atom->ul = nvme_smart_data->temperature_sensor_two;
			return 1;
			
		case TEMPERATURE_SENSOR_THREE:
			atom->ul = nvme_smart_data->temperature_sensor_three;
			return 1;
			
		case TEMPERATURE_SENSOR_FOUR:
			atom->ul = nvme_smart_data->temperature_sensor_four;
			return 1;
			
		case TEMPERATURE_SENSOR_FIVE:
			atom->ul = nvme_smart_data->temperature_sensor_five;
			return 1;
			
		case TEMPERATURE_SENSOR_SIX:
			atom->ul = nvme_smart_data->temperature_sensor_six;
			return 1;
			
		case TEMPERATURE_SENSOR_SEVEN:
			atom->ul = nvme_smart_data->temperature_sensor_seven;
			return 1;
			
		case TEMPERATURE_SENSOR_EIGHT:
			atom->ul = nvme_smart_data->temperature_sensor_eight;
			return 1;
	
		default:
			return PM_ERR_PMID;
	}
	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
}

int 
nvme_power_data_fetch(int item, int cluster, struct nvme_power_states *nvme_power_states, pmAtomValue *atom, int is_nvme)
{
	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
	    return 0;
	
	/* Adjust cluster number to our power state position in nvme_power_states array */
	cluster -= _POWER_STATE_CLUSTER_OFFSET;

	switch (item) {

		case STATE:
			atom->ul = nvme_power_states->state[cluster];
			return 1;
		
		case MAX_POWER:
		        atom->d = nvme_power_states->max_power[cluster];
		        return 1;

                case NON_OPERATIONAL_STATE:
                        atom->ul = nvme_power_states->non_operational_state[cluster];
                        return 1;

                case ACTIVE_POWER:
                        if (nvme_power_states->active_power[cluster] == -1) {
                                return PMDA_FETCH_NOVALUES;
                        } else {
                                atom->d = nvme_power_states->active_power[cluster];
                                return 1;
                        }

                case IDLE_POWER:
                        if (nvme_power_states->idle_power[cluster] == -1) {
                                return PMDA_FETCH_NOVALUES;
                        } else {
                                atom->d = nvme_power_states->idle_power[cluster];
                                return 1;
                        }

                case RELATIVE_READ_LATENCY:
                        atom->ul = nvme_power_states->relative_read_latency[cluster];
                        return 1;

                case RELATIVE_READ_THROUGHPUT:
                        atom->ul = nvme_power_states->relative_read_throughput[cluster];
                        return 1;

                case RELATIVE_WRITE_LATENCY:
                        atom->ul = nvme_power_states->relative_write_latency[cluster];
                        return 1;

                case RELATIVE_WRITE_THROUGHPUT:
                        atom->ul = nvme_power_states->relative_write_throughput[cluster];
                        return 1;

                case ENTRY_LATENCY:
                        atom->ul = nvme_power_states->entry_latency[cluster];
                        return 1;

                case EXIT_LATENCY:
                        atom->ul = nvme_power_states->exit_latency[cluster];
                        return 1;

		default:
			return PM_ERR_PMID;
	}
	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
}

int
smart_refresh_device_info(const char *name, struct device_info *device_info, int is_nvme)
{
	char buffer[4096], capacity[64] = {'\0'};
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -Hi /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		if (strncmp(buffer, "Model Family:", 13) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->model_family);

		if (is_nvme) {
			if (strncmp(buffer, "Model Number:", 13) == 0)
				sscanf(buffer, "%*s%*s %[^\n]", device_info->device_model);
		
		} else {
			if (strncmp(buffer, "Device Model:", 13) == 0)
				sscanf(buffer, "%*s%*s %[^\n]", device_info->device_model);
		}

		if (strncmp(buffer, "Serial Number:", 14) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->serial_number);

		if (is_nvme) {
			if (strncmp(buffer, "Total NVM Capacity:", 19) == 0)
				sscanf(buffer, "%*s%*s%*s %s", capacity); 
				
			smart_strip_input(capacity);
			device_info->capacity_bytes = strtoull(capacity, NULL, 10);
				
		} else {
			if (strncmp(buffer, "User Capacity:", 14) == 0) 
				sscanf(buffer, "%*s%*s %s", capacity);
				
			smart_strip_input(capacity);
			device_info->capacity_bytes = strtoull(capacity, NULL, 10);
		}

		if (strncmp(buffer, "Sector Size:", 12) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->sector_size);

		if (strncmp(buffer, "Rotation Rate:", 14) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->rotation_rate);
	
		if (strncmp(buffer, "SMART overall-health", 20) == 0)
			sscanf(buffer, "%*s %*s %*s %*s %*s %s", device_info->health);
			
		if (strncmp(buffer, "Firmware Version:", 17) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", device_info->firmware_version);
	}
	pclose(pf);
	return 0;
}

int
smart_refresh_data(const char *name, struct smart_data *smart_data, int is_nvme)
{
	char buffer[MAXPATHLEN];
	FILE *pf;

	uint8_t id, value, worst, thresh; 
	uint32_t raw;
	
	/* Test to see if we have an NVME disk, if so we don't want to try and collect
	   SAT S.M.A.R.T data */
	if(is_nvme)
		return 0;

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
			 * not efficient but allows easy expansion with new smart values
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

int
nvme_device_refresh_data(const char *name, struct nvme_device_info *nvme_device_info, int is_nvme)
{
	char buffer[4096], capacity[64] = {'\0'};
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -Hi /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
		return 0;

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
	        if (strncmp(buffer, "Model Number:", 13) == 0)
	                sscanf(buffer, "%*s%*s %[^\n]", nvme_device_info->model_number);

	        if (strncmp(buffer, "Serial Number:", 14) == 0)
		        sscanf(buffer, "%*s%*s %[^\n]", nvme_device_info->serial_number);

                if (strncmp(buffer, "Firmware Version:", 17) == 0)
		        sscanf(buffer, "%*s%*s %[^\n]", nvme_device_info->firmware_version);

                if (strncmp(buffer, "PCI Vendor/Subsystem ID:", 24) == 0)
		        sscanf(buffer, "%*s%*s%*s %[^\n]", nvme_device_info->pci_vendor_subsystem_id);

                if (strncmp(buffer, "IEEE OUI Identifier:", 20) == 0)
		        sscanf(buffer, "%*s%*s%*s %[^\n]", nvme_device_info->ieee_oui_identifier);

                if (strncmp(buffer, "Total NVM Capacity:", 19) == 0) {
		        sscanf(buffer, "%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->total_nvm_capacity = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Unallocated NVM Capacity:", 25) == 0) {
		        sscanf(buffer, "%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->unallocated_nvm_capacity = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Controller ID:", 14) == 0) {
		        sscanf(buffer, "%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->controller_id = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "NVMe Version:", 13) == 0)
		        sscanf(buffer, "%*s%*s %[^\n]", nvme_device_info->nvme_version);

                if (strncmp(buffer, "Number of Namespaces:", 21) == 0) {
		        sscanf(buffer, "%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->namespaces = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Namespace 1 Size/Capacity:", 26) == 0) {
		        sscanf(buffer, "%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->namespace_1_capacity = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Namespace 1 Utilization:", 23) == 0) {
		        sscanf(buffer, "%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->namespace_1_utilization = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Namespace 1 Formatted LBA Size:", 31) == 0) {
		        sscanf(buffer, "%*s%*s%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->namespace_1_formatted_lba_size = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Namespace 1 IEEE EUI-64:", 24) == 0)
		        sscanf(buffer, "%*s%*s%*s%*s %[^\n]", nvme_device_info->namespace_1_ieee_eui_64);

                if (strncmp(buffer, "Firmware Updates", 16) == 0)
		        sscanf(buffer, "%*s%*s%*s %[^\n]", nvme_device_info->firmware_updates);

                if (strncmp(buffer, "Maximum Data Transfer Size:", 27) == 0) {
		        sscanf(buffer, "%*s%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->maximum_data_transfer_size = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Warning Comp. Temp. Threshold:", 30) == 0) {
		        sscanf(buffer, "%*s%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->warning_temp_threshold = strtoull(capacity, NULL, 10);
		}

                if (strncmp(buffer, "Critical Comp. Temp. Threshold:", 30) == 0) {
		        sscanf(buffer, "%*s%*s%*s%*s %s", capacity); 

		        smart_strip_input(capacity);
		        nvme_device_info->critical_temp_threshold = strtoull(capacity, NULL, 10);
		}
        }
        pclose(pf);

        if (nvmecli_support) {

                /*
                  smart.nvme_info.active_power_state
                */

	        pmsprintf(buffer, sizeof(buffer), "%s get-feature -f 0x02 -H /dev/%s 2>&1", smart_setup_nvmecli, name);
	        buffer[sizeof(buffer)-1] = '\0';

	        if ((pf = popen(buffer, "r")) == NULL)
		        return -oserror();

	        while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

                        if (strstr(buffer, "command not found")){
                                // Make use of redirected stderr to stdout to check if nvme-cli is
                                // installed and and disable if not
                               nvmecli_support = 0;
                               continue;
                        }
                               
                        if (strstr(buffer, "Power State")) {
	                        sscanf(buffer, "%*s%*s%*s %s", capacity);

                                nvme_device_info->active_power_state = strtoull(capacity, NULL, 10);
                        }
	        }
	        pclose(pf);

                /*
                  smart.nvme_info.apste_state
                */

        	pmsprintf(buffer, sizeof(buffer), "%s get-feature -f 0x0c -H /dev/%s", smart_setup_nvmecli, name);
	        buffer[sizeof(buffer)-1] = '\0';

	        if ((pf = popen(buffer, "r")) == NULL)
	        	return -oserror();

	        while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
                        if (strstr(buffer, "(APSTE):"))
                                sscanf(buffer, "%*s%*s%*s%*s%*s%*s %s", nvme_device_info->apst_state);
	        }
	        pclose(pf);

                /* 
                  smart.nvme_info.completion_queue_length_completion
                  smart.nvme_info.completion_queue_length_submission
                */

        	pmsprintf(buffer, sizeof(buffer), "%s get-feature -f 0x07 -H /dev/%s", smart_setup_nvmecli, name);
        	buffer[sizeof(buffer)-1] = '\0';

        	if ((pf = popen(buffer, "r")) == NULL)
        		return -oserror();

        	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
                        if (strstr(buffer, "(NCQA):")){
                                sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %s", capacity);

                                nvme_device_info->completion_queue_length_completion = strtoull(capacity, NULL, 10);
                        }
            
                        if (strstr(buffer, "(NSQA):")){
                                sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %s", capacity);

                                nvme_device_info->completion_queue_length_submission = strtoull(capacity, NULL, 10);
                        }
                }
                pclose(pf);
        }
        return 0;
}

int
nvme_smart_refresh_data(const char *name, struct nvme_smart_data *nvme_smart_data, int is_nvme)
{
	char buffer[4096], units[64] = {'\0'};
	FILE *pf;
	
	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
		return 0;

	pmsprintf(buffer, sizeof(buffer), "%s -A /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
		if (strncmp(buffer, "Critical Warning:", 17) == 0)
			sscanf(buffer, "%*s%*s %[^\n]", nvme_smart_data->critical_warning);
		
		if (strncmp(buffer, "Temperature:", 12) == 0)
			sscanf(buffer, "%*s %"SCNu8"", &nvme_smart_data->composite_temperature);
		 
		if (strncmp(buffer, "Available Spare:", 16) == 0)
			sscanf(buffer, "%*s%*s %"SCNu8"", &nvme_smart_data->available_spare);
			
		if (strncmp(buffer, "Available Spare Threshold:", 26) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->available_spare_threshold);
			
		if (strncmp(buffer, "Percentage Used:", 16) == 0)
			sscanf(buffer, "%*s%*s %"SCNu8"", &nvme_smart_data->percentage_used);
			
		if (strncmp(buffer, "Data Units Read:", 16) == 0) {
			sscanf(buffer, "%*s%*s%*s %s", units);
			
			smart_strip_input(units);
			nvme_smart_data->data_units_read = strtoull(units, NULL, 10);
		}
		
		if (strncmp(buffer, "Data Units Written:", 19) == 0) {
			sscanf(buffer, "%*s%*s%*S %s", units);

			smart_strip_input(units);
			nvme_smart_data->data_units_written = strtoull(units, NULL, 10);
		}
		
		if (strncmp(buffer, "Host Read Commands:", 19) == 0) {
			sscanf(buffer, "%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->host_read_commands = strtoull(units, NULL, 10);
		}
		
		if (strncmp(buffer, "Host Write Commands:", 20) == 0) {
			sscanf(buffer, "%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->host_write_commands = strtoull(units, NULL, 10);
		}
		
		if (strncmp(buffer, "Controller Busy Time:", 21) == 0) {
			sscanf(buffer, "%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->controller_busy_time = strtoul(units, NULL, 10);
		}
	
		if (strncmp(buffer, "Power Cycles:", 13) == 0) {
			sscanf(buffer, "%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->power_cycles = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Power On Hours:", 15) == 0) {
			sscanf(buffer, "%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->power_on_hours = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Unsafe Shutdowns:", 17) == 0) {
			sscanf(buffer, "%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->unsafe_shutdowns = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Media and Data Integrity Errors:", 32) == 0) {
			sscanf(buffer, "%*s%*s%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->media_and_data_integrity_errors = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Error Information Log Entries:", 30) == 0) {
			sscanf(buffer, "%*s%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->number_of_error_information_log_entries = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Warning Comp. Temperature Time:", 31) == 0) {
			sscanf(buffer, "%*s%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->warning_composite_temperature_time = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Critical Comp. Temperature Time:", 32) == 0) {
			sscanf(buffer, "%*s%*s%*s%*s %[^\n]", units);

			smart_strip_input(units);
			nvme_smart_data->critical_composite_temperature_time = strtoul(units, NULL, 10);
		}

		if (strncmp(buffer, "Temperature Sensor 1:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_one);
			
		if (strncmp(buffer, "Temperature Sensor 2:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_two);
			
		if (strncmp(buffer, "Temperature Sensor 3:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_three);
			
		if (strncmp(buffer, "Temperature Sensor 4:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_four);		

		if (strncmp(buffer, "Temperature Sensor 5:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_five);
			
		if (strncmp(buffer, "Temperature Sensor 6:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_six);	

		if (strncmp(buffer, "Temperature Sensor 7:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_seven);

		if (strncmp(buffer, "Temperature Sensor 8:", 21) == 0)
			sscanf(buffer, "%*s%*s%*s %"SCNu8"", &nvme_smart_data->temperature_sensor_eight);	
	}
	pclose(pf);
	return 0;
}

int
nvme_power_refesh_data(const char *name, struct nvme_power_states *nvme_power_states, int is_nvme)
{
	char buffer[4096] = {'\0'};
	FILE *pf;
	
	int found_ps = 0, current_ps = 0;
	char operational[2], active_power[8], idle_power[8] = {'\0'};
        
        char *opp = operational;
        char *active = active_power;
        char *idle = idle_power;
	
	/* Test to see if we have an NVME disk, if not we can return */
	if (!is_nvme)
		return 0;

	pmsprintf(buffer, sizeof(buffer), "%s -c /dev/%s", smart_setup_stats, name);
	buffer[sizeof(buffer)-1] = '\0';

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while(fgets(buffer, sizeof(buffer)-1, pf) != NULL) {
                /*
                        Skip over the lines which do not include power state information,
                        then work out which lines have the details for each individual
                        power state. Lastly we need to keep track of which of the 3 power
                        state information lines we are on for metric collection.

                        found_ps   = Found the Power State Section, each new one has the
                                     term operational in it.
                        current_ps = Pointer to which current power state we are looking
                                     at.
                */
                
                if (strstr(buffer, "Supported Power States")){
                        found_ps = 1; // Found the power state section
                        continue;
                }

                if (strstr(buffer, "LBA")) {
                        found_ps = 0; // End of power state section
                        continue;
                }

                if ((found_ps) && strstr(buffer, "St Op"))
                        continue; // Ignore header line
	    
                if (strncmp(buffer, "\n", 1) == 0)
                        continue; // Avoid blank line in output
	    
                if (found_ps) {
                        nvme_power_states->state[current_ps] = current_ps;

                        sscanf(buffer, "%*s %s %lfW %s %s %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32" %"SCNu32"",
	    		        opp,
	         		&nvme_power_states->max_power[current_ps],
	        		active,
	        		idle,
	        		&nvme_power_states->relative_read_latency[current_ps],
	        		&nvme_power_states->relative_read_throughput[current_ps],
	        		&nvme_power_states->relative_write_latency[current_ps],
	        		&nvme_power_states->relative_write_throughput[current_ps],
	        		&nvme_power_states->entry_latency[current_ps],
                                &nvme_power_states->exit_latency[current_ps]
	    	        );
                
                
                if (strstr(operational, "-"))
			nvme_power_states->non_operational_state[current_ps] = 1;

	    	if (strstr(operational, "+"))
	    		nvme_power_states->non_operational_state[current_ps] = 0;
	    		
	    	if (strstr(active_power, "-")) {
	    		nvme_power_states->active_power[current_ps] = -1;
	    	} else {
			nvme_power_states->active_power[current_ps] = atof(active_power);
	    	}

	    	if (strstr(idle_power, "-")) {
	    		nvme_power_states->idle_power[current_ps] = -1;
	    	} else {
	    		nvme_power_states->idle_power[current_ps] = atof(idle_power);
	    	}
	    	
	    	current_ps++;
	    }
	}
        pclose(pf);
        
        return 0;
}

void
smart_stats_setup(void)
{
	static char smart_command[] = "LC_ALL=C smartctl";
        static char nvmecli_command[] = "LC_ALL=C nvme";
        
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("SMART_SETUP")) != NULL)
		smart_setup_stats = env_command;
	else
		smart_setup_stats = smart_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("NVME_CLI_SETUP")) != NULL)
		smart_setup_nvmecli = env_command;
	else
		smart_setup_nvmecli = nvmecli_command;
}
