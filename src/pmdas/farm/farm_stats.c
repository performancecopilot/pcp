/*
 * Seagate Field Accessible Reliability Metrics (FARM) Log
 *
 * Copyright (c) 2023 Red Hat.
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

#include "pmdafarm.h"
#include "farm_stats.h"
 
#include <ctype.h>
 
static char *farm_setup_stats;

/* Farm log output in smartctl is padded with a leading tab layout,
 * trim this out from the farm log output to make comparisons easier.
 */
char 
*strtrim(char* str) {

	// Trim leading space
	while(isspace((unsigned char)*str)) str++;

	return str;
}
 
int 
farm_ata_data_fetch(int item, int cluster, struct farm_ata_log_stats *farm_ata_log_stats, pmAtomValue *atom)
{
 	if (cluster == CLUSTER_ATA_LOG_HEADER) {
		switch (item) {
			case LOG_VERSION:
				atom->cp = farm_ata_log_stats->log_version;
				return PMDA_FETCH_STATIC;

			case PAGES_SUPPORTED:
				atom->ull = farm_ata_log_stats->pages_supported;
				return PMDA_FETCH_STATIC;

			case LOG_SIZE:
				atom->ull = farm_ata_log_stats->log_size;
				return PMDA_FETCH_STATIC;

			case PAGE_SIZE:
				atom->ull = farm_ata_log_stats->page_size;
				return PMDA_FETCH_STATIC;

			case HEADS_SUPPORTED:
				atom->ull = farm_ata_log_stats->heads_supported;
				return PMDA_FETCH_STATIC;

			case NUMBER_OF_COPIES:
				atom->ull = farm_ata_log_stats->number_of_copies;
				return PMDA_FETCH_STATIC;

			case REASON_FOR_FRAME_CAPTURE:
				atom->ull = farm_ata_log_stats->reason_for_frame_capture;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_ATA_DRIVE_INFORMATION) {
		switch (item) {
			case SERIAL_NUMBER:
				atom->cp = farm_ata_log_stats->serial_number;
				return PMDA_FETCH_STATIC;

			case WORLD_WIDE_NAME:
				atom->cp = farm_ata_log_stats->world_wide_name;
				return PMDA_FETCH_STATIC;

			case DEVICE_INTERFACE:
				atom->cp = farm_ata_log_stats->device_interface;
				return PMDA_FETCH_STATIC;

			case DEVICE_CAPACITY_IN_SECTORS:
			        atom->ull = farm_ata_log_stats->device_capacity_in_sectors;
			        return PMDA_FETCH_STATIC;

			case PHYSICAL_SECTOR_SIZE:
				atom->ull = farm_ata_log_stats->physical_sector_size;
				return PMDA_FETCH_STATIC;

			case LOGICAL_SECTOR_SIZE:
				atom->ull = farm_ata_log_stats->logical_sector_size;
				return PMDA_FETCH_STATIC;

			case DEVICE_BUFFER_SIZE:
				atom->ull = farm_ata_log_stats->device_buffer_size;
				return PMDA_FETCH_STATIC;

			case NUMBER_OF_HEADS:
				atom->ull = farm_ata_log_stats->number_of_heads;
				return PMDA_FETCH_STATIC;

			case DEVICE_FORM_FACTOR:
				atom->cp = farm_ata_log_stats->device_form_factor;
				return PMDA_FETCH_STATIC;

			case ROTATION_RATE:
				atom->ull = farm_ata_log_stats->rotational_rate;
				return PMDA_FETCH_STATIC;

			case FIRMWARE_REV:
				atom->cp = farm_ata_log_stats->firmware_rev;
				return PMDA_FETCH_STATIC;

			case ATA_SECURITY_STATE:
				atom->cp = farm_ata_log_stats->ata_security_state;
				return PMDA_FETCH_STATIC;

			case ATA_FEATURES_SUPPORTED:
				atom->cp = farm_ata_log_stats->ata_features_supported;
				return PMDA_FETCH_STATIC;

			case ATA_FEATURES_ENABLED:
				atom->cp = farm_ata_log_stats->ata_features_enabled;
				return PMDA_FETCH_STATIC;

			case POWER_ON_HOURS:
				atom->ull = farm_ata_log_stats->power_on_hours;
				return PMDA_FETCH_STATIC;

			case SPINDLE_POWER_ON_HOURS:
				atom->ull = farm_ata_log_stats->spindle_power_on_hours;
				return PMDA_FETCH_STATIC;

			case HEAD_FLIGHT_HOURS:
				atom->ull = farm_ata_log_stats->head_flight_hours;
				return PMDA_FETCH_STATIC;

			case HEAD_LOAD_EVENTS:
				atom->ull = farm_ata_log_stats->head_load_events;
				return PMDA_FETCH_STATIC;

			case POWER_CYCLE_COUNT:
				atom->ull = farm_ata_log_stats->power_cycle_count;
				return PMDA_FETCH_STATIC;

			case HARDWARE_RESET_COUNT:
				atom->ull = farm_ata_log_stats->hardware_reset_count;
				return PMDA_FETCH_STATIC;

			case SPIN_UP_TIME:
				atom->ull = farm_ata_log_stats->spin_up_time;
				return PMDA_FETCH_STATIC;

			case TIME_TO_READY_LAST_POWER_CYCLE:
				atom->ull = farm_ata_log_stats->time_to_ready_last_power_cycle;
				return PMDA_FETCH_STATIC;

			case TIME_DRIVE_HELD_IN_STAGGERED_SPIN:
				atom->ull = farm_ata_log_stats->time_drive_held_in_staggered_spin;
				return PMDA_FETCH_STATIC;

			case MODEL_NUMBER:
				atom->cp = farm_ata_log_stats->model_number;
				return PMDA_FETCH_STATIC;

			case DRIVE_RECORDING_TYPE:
				atom->cp = farm_ata_log_stats->drive_recording_type;
				return PMDA_FETCH_STATIC;

			case MAX_NUMBER_AVAILABLE_SECTORS_REASSIGNMENT:
				atom->ull = farm_ata_log_stats->max_number_available_sectors_reassignment;
				return PMDA_FETCH_STATIC;

			case ASSEMBLY_DATE:
				atom->cp = farm_ata_log_stats->assembly_date;
				return PMDA_FETCH_STATIC;

			case DEPOPULATION_HEAD_MASK:
				atom->ull = farm_ata_log_stats->depopulation_head_mask;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_ATA_WORKLOAD_STATISTICS) {
		switch (item) {
			case TOTAL_READ_COMMANDS:
				atom->ull = farm_ata_log_stats->total_read_commands;
				return PMDA_FETCH_STATIC;

			case TOTAL_WRITE_COMMANDS:
				atom->ull = farm_ata_log_stats->total_write_commands;
				return PMDA_FETCH_STATIC;

			case TOTAL_RANDOM_READ_COMMANDS:
				atom->ull = farm_ata_log_stats->total_random_read_commands;
				return PMDA_FETCH_STATIC;

			case TOTAL_RANDOM_WRITE_COMMANDS:
				atom->ull = farm_ata_log_stats->total_random_write_commands;
				return PMDA_FETCH_STATIC;

			case TOTAL_OTHER_COMMANDS:
				atom->ull = farm_ata_log_stats->total_other_commands;
				return PMDA_FETCH_STATIC;

			case LOGICAL_SECTORS_WRITTEN:
				atom->ull = farm_ata_log_stats->logical_sectors_written;
				return PMDA_FETCH_STATIC;

			case LOGICAL_SECTORS_READ:
				atom->ull = farm_ata_log_stats->logical_sectors_read;
				return PMDA_FETCH_STATIC;

			case DITHER_EVENTS_CURRENT_POWER_CYCLE:
				atom->ull = farm_ata_log_stats->dither_events_current_power_cycle;
				return PMDA_FETCH_STATIC;

			case DITHER_HELD_OFF_RANDOM_WORKLOADS:
				atom->ull = farm_ata_log_stats->dither_held_off_random_workloads;
				return PMDA_FETCH_STATIC;

			case DITHER_HELD_OFF_SEQUENTIAL_WORKLOADS:
				atom->ull = farm_ata_log_stats->dither_held_off_sequential_workloads;
				return PMDA_FETCH_STATIC;

			case READ_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->read_commands_0_3_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case READ_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->read_commands_3_25_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case READ_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->read_commands_25_75_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case READ_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->read_commands_75_100_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case WRITE_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->write_commands_0_3_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case WRITE_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->write_commands_3_25_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case WRITE_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->write_commands_25_75_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			case WRITE_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES:
				atom->ull = farm_ata_log_stats->write_commands_75_100_lba_space_last_3_smart_summary_frames;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}
		
	} else if (cluster == CLUSTER_ATA_ERROR_STATISTICS) {
		switch (item) {
			case UNRECOVERABLE_READ_ERRORS:
				atom->ull = farm_ata_log_stats->unrecoverable_read_errors;
				return PMDA_FETCH_STATIC;

			case UNRECOVERABLE_WRITE_ERRORS:
				atom->ull = farm_ata_log_stats->unrecoverable_write_errors;
				return PMDA_FETCH_STATIC;

			case REALLOCATED_SECTORS:
				atom->ull = farm_ata_log_stats->reallocated_sectors;
				return PMDA_FETCH_STATIC;

			case READ_RECOVERY_ATTEMPTS:
				atom->ull = farm_ata_log_stats->read_recovery_attempts;
				return PMDA_FETCH_STATIC;

			case MECHANICAL_START_FAILURES:
				atom->ull = farm_ata_log_stats->mechanical_start_failures;
				return PMDA_FETCH_STATIC;

			case REALLOCATED_CANDIDATE_SECTORS:
				atom->ull = farm_ata_log_stats->reallocated_candidate_sectors;
				return PMDA_FETCH_STATIC;

			case ASR_EVENTS:
				atom->ull = farm_ata_log_stats->asr_events;
				return PMDA_FETCH_STATIC;

			case INTERFACE_CRC_ERRORS:
				atom->ull = farm_ata_log_stats->interface_crc_errors;
				return PMDA_FETCH_STATIC;

			case SPIN_RETRY_COUNT:
				atom->ull = farm_ata_log_stats->spin_retry_count;
				return PMDA_FETCH_STATIC;

			case SPIN_RETRY_COUNT_NORMALIZED:
				atom->ull = farm_ata_log_stats->spin_retry_count_normalized;
				return PMDA_FETCH_STATIC;

			case SPIN_RETRY_COUNT_WORST:
				atom->ull = farm_ata_log_stats->spin_retry_count_worst;
				return PMDA_FETCH_STATIC;

			case IOEDC_ERRORS:
				atom->ull = farm_ata_log_stats->ioedc_errors;
				return PMDA_FETCH_STATIC;

			case CTO_COUNT_TOTAL:
				atom->ull = farm_ata_log_stats->cto_count_total;
				return PMDA_FETCH_STATIC;

			case CTO_COUNT_OVER_5S:
				atom->ull = farm_ata_log_stats->cto_count_over_5s;
				return PMDA_FETCH_STATIC;

			case CTO_COUNT_OVER_7S:
				atom->ull = farm_ata_log_stats->cto_count_over_7s;
				return PMDA_FETCH_STATIC;

			case TOTAL_FLASH_LED_ASSERT_EVENTS:
				atom->ull = farm_ata_log_stats->total_flash_led_assert_events;
				return PMDA_FETCH_STATIC;

			case INDEX_OF_LAST_FLASH_LED:
				atom->ull = farm_ata_log_stats->index_of_last_flash_led;
				return PMDA_FETCH_STATIC;

			case UNCORRECTABLE_ERRORS:
				atom->ull = farm_ata_log_stats->uncorrectable_errors;
				return PMDA_FETCH_STATIC;

			case CUMULATIVE_LIFETIME_UNRECOVERABLE_ERRORS_DUE_TO_ERC:
				atom->ull = farm_ata_log_stats->cumulative_lifetime_unrecoverable_errors_due_to_erc;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}
		
	} else if (cluster == CLUSTER_ATA_ENVIRONMENTAL_STATISTICS) {
		switch (item) {
			case CURRENT_TEMPERATURE:
				atom->ull = farm_ata_log_stats->current_temperature;
				return PMDA_FETCH_STATIC;

			case HIGHEST_TEMPERATURE:
				atom->ull = farm_ata_log_stats->highest_temperature;
				return PMDA_FETCH_STATIC;

			case LOWEST_TEMPERATURE:
				atom->ull = farm_ata_log_stats->lowest_temperature;
				return PMDA_FETCH_STATIC;

			case AVERAGE_SHORT_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->average_short_term_temperature;
				return PMDA_FETCH_STATIC;

			case AVERAGE_LONG_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->average_long_term_temperature;
				return PMDA_FETCH_STATIC;

			case HIGHEST_AVERAGE_SHORT_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->highest_average_short_term_temperature;
				return PMDA_FETCH_STATIC;

			case LOWEST_AVERAGE_SHORT_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->lowest_average_short_term_temperature;
				return PMDA_FETCH_STATIC;

			case HIGHEST_AVERAGE_LONG_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->highest_average_long_term_temperature;
				return PMDA_FETCH_STATIC;

			case LOWEST_AVERAGE_LONG_TERM_TEMPERATURE:
				atom->ull = farm_ata_log_stats->lowest_average_long_term_temperature;
				return PMDA_FETCH_STATIC;

			case TIME_IN_OVER_TEMPERATURE:
				atom->ull = farm_ata_log_stats->time_in_over_temperature;
				return PMDA_FETCH_STATIC;

			case TIME_IN_UNDER_TEMPERATURE:
				atom->ull = farm_ata_log_stats->time_in_under_temperature;
				return PMDA_FETCH_STATIC;

			case SPECIFIED_MAX_OPERATING_TEMPERATURE:
				atom->ull = farm_ata_log_stats->specified_max_operating_temperature;
				return PMDA_FETCH_STATIC;

			case SPECIFIED_MIN_OPERATING_TEMPERATURE:
				atom->ull = farm_ata_log_stats->specified_min_operating_temperature;
				return PMDA_FETCH_STATIC;

			case CURRENT_RELATIVE_HUMIDITY:
				atom->ull = farm_ata_log_stats->current_relative_humidity;
				return PMDA_FETCH_STATIC;

			case CURRENT_MOTOR_POWER:
				atom->ull = farm_ata_log_stats->current_motor_power;
				return PMDA_FETCH_STATIC;

			case CURRENT_12_VOLTS:
				atom->d = farm_ata_log_stats->current_12_volts;
				return PMDA_FETCH_STATIC;

			case MINIMUM_12_VOLTS:
				atom->d = farm_ata_log_stats->minimum_12_volts;
				return PMDA_FETCH_STATIC;

			case MAXIMUM_12_VOLTS:
				atom->d = farm_ata_log_stats->maximum_12_volts;
				return PMDA_FETCH_STATIC;

			case CURRENT_5_VOLTS:
				atom->d = farm_ata_log_stats->current_5_volts;
				return PMDA_FETCH_STATIC;

			case MINIMUM_5_VOLTS:
				atom->d = farm_ata_log_stats->minimum_5_volts;
				return PMDA_FETCH_STATIC;

			case MAXIMUM_5_VOLTS:
				atom->d = farm_ata_log_stats->maximum_5_volts;
				return PMDA_FETCH_STATIC;

			case _12V_POWER_AVERAGE:
				atom->d = farm_ata_log_stats->_12v_power_average;
				return PMDA_FETCH_STATIC;

			case _12V_POWER_MINIMUM:
				atom->d = farm_ata_log_stats->_12v_power_minimum;
				return PMDA_FETCH_STATIC;

			case _12V_POWER_MAXIMUM:
				atom->d = farm_ata_log_stats->_12v_power_maximum;
				return PMDA_FETCH_STATIC;

			case _5V_POWER_AVERAGE:
				atom->d = farm_ata_log_stats->_5v_power_average;
				return PMDA_FETCH_STATIC;

			case _5V_POWER_MINIMUM:
				atom->d = farm_ata_log_stats->_5v_power_minimum;
				return PMDA_FETCH_STATIC;

			case _5V_POWER_MAXIMUM:
				atom->d = farm_ata_log_stats->_5v_power_maximum;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}
		
	} else if (cluster == CLUSTER_ATA_RELIABILITY_STATISTICS) {
		switch (item) {
			case ERROR_RATE_SMART_1_RAW:
				atom->ull = farm_ata_log_stats->error_rate_smart_1_raw;
				return PMDA_FETCH_STATIC;

			case ERROR_RATE_SMART_1_NORMALIZED:
				atom->ull = farm_ata_log_stats->error_rate_smart_1_normalized;
				return PMDA_FETCH_STATIC;

			case ERROR_RATE_SMART_1_WORST:
				atom->ull = farm_ata_log_stats->error_rate_smart_1_worst;
				return PMDA_FETCH_STATIC;

			case SEEK_ERROR_RATE_SMART_7_RAW:
				atom->ull = farm_ata_log_stats->seek_error_rate_smart_7_raw;
				return PMDA_FETCH_STATIC;

			case SEEK_ERROR_RATE_SMART_7_NORMALIZED:
				atom->ull = farm_ata_log_stats->seek_error_rate_smart_7_normalized;
				return PMDA_FETCH_STATIC;

			case SEEK_ERROR_RATE_SMART_7_WORST:
				atom->ull = farm_ata_log_stats->seek_error_rate_smart_7_worst;
				return PMDA_FETCH_STATIC;

			case HIGH_PRIORITY_UNLOAD_EVENTS:
				atom->ull = farm_ata_log_stats->high_priority_unload_events;
				return PMDA_FETCH_STATIC;

			case HELIUM_PRESSURE_THRESHOLD_TRIPPED:
				atom->ull = farm_ata_log_stats->helium_pressure_threshold_tripped;
				return PMDA_FETCH_STATIC;

			case LBAS_CORRECTED_BY_PARITY_SECTOR:
				atom->ull = farm_ata_log_stats->lbas_corrected_by_parity_sector;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}
	}

	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
}

int 
farm_ata_flash_led_events_fetch(int item, unsigned int inst, pmAtomValue *atom)
{
	struct  farm_flash_led_events *flash_led_events;
	pmInDom indom;
	int sts;

	switch  (item) {
	        case FLASH_LED_EVENT_INFORMATION:
	                indom = INDOM(FLASH_LED_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&flash_led_events);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = flash_led_events->led_event_information;
	                return PMDA_FETCH_STATIC;

	        case FLASH_LED_EVENT_TIMESTAMP:
	                indom = INDOM(FLASH_LED_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&flash_led_events);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = flash_led_events->led_event_timestamp;
	                return PMDA_FETCH_STATIC;

	        case FLASH_LED_EVENT_POWER_CYCLE:
	                indom = INDOM(FLASH_LED_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&flash_led_events);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = flash_led_events->led_event_power_cycle;
	                return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;
	        
	}
}

int 
farm_ata_per_head_stats_fetch(int item, unsigned int inst, pmAtomValue *atom)
{
	struct  farm_per_head_stats *per_head_stats;
	pmInDom indom;
	int sts;

	switch  (item) {

                case CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_REPEATING:
	                indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->head_cumulative_lifetime_unrecoverable_read_repeating;
	                return PMDA_FETCH_STATIC;

                case CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_UNIQUE:
	                indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->head_cumulative_lifetime_unrecoverable_read_unique;
	                return PMDA_FETCH_STATIC;

        	case DVGA_SKIP_WRITE_DETECTED:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->dvga_skip_write_detected_head;
	                return PMDA_FETCH_STATIC;
        	
	        case RVGA_SKIP_WRITE_DETECTED:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->rvga_skip_write_detected_head;
	                return PMDA_FETCH_STATIC;
	        
        	case FVGA_SKIP_WRITE_DETECTED:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->rvga_skip_write_detected_head;
	                return PMDA_FETCH_STATIC;
        	
	        case SKIP_WRITE_DETECT_THRESHOLD_DETECT:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->skip_write_detect_threshold_detect_head;
	                return PMDA_FETCH_STATIC;	
	
	        case WRITE_POWER_SECS:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->write_power_secs_head;
	                return PMDA_FETCH_STATIC;	
	
        	case MR_HEAD_RESISTANCE:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->mr_head_resistance_head;
	                return PMDA_FETCH_STATIC;	
	
        	case SECOND_MR_HEAD_RESISTANCE:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->second_mr_head_resistance_head;
	                return PMDA_FETCH_STATIC;	
	
        	case NUMBER_REALLOCATED_SECTORS:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->number_reallocated_sectors_head;
	                return PMDA_FETCH_STATIC;

        	case NUMBER_REALLOCATION_CANDIDATE_SECTORS:
        		indom = INDOM(PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = per_head_stats->number_reallocation_candidate_sectors_head;
	                return PMDA_FETCH_STATIC;
	                
		default:
			return PM_ERR_PMID;
	        
	}
}

int 
farm_scsi_data_fetch(int item, int cluster, struct farm_scsi_log_stats *farm_scsi_log_stats, pmAtomValue *atom)
{
 	if (cluster == CLUSTER_SCSI_LOG_HEADER) {
		switch (item) {
			case SCSI_LOG_VERSION:
				atom->cp = farm_scsi_log_stats->log_version;
				return PMDA_FETCH_STATIC;

			case SCSI_PAGES_SUPPORTED:
				atom->ull = farm_scsi_log_stats->pages_supported;
				return PMDA_FETCH_STATIC;

			case SCSI_LOG_SIZE:
				atom->ull = farm_scsi_log_stats->log_size;
				return PMDA_FETCH_STATIC;

			case SCSI_HEADS_SUPPORTED:
				atom->ull = farm_scsi_log_stats->heads_supported;
				return PMDA_FETCH_STATIC;

			case SCSI_REASON_FOR_LAST_FRAME:
				atom->ull = farm_scsi_log_stats->reason_for_frame_capture;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_DRIVE_INFORMATION) {
		switch (item) {
			case SCSI_SERIAL_NUMBER:
				atom->cp = farm_scsi_log_stats->serial_number;
				return PMDA_FETCH_STATIC;

			case SCSI_WORLD_WIDE_NAME:
				atom->cp = farm_scsi_log_stats->world_wide_name;
				return PMDA_FETCH_STATIC;

			case SCSI_FIRMWARE_REVISION:
				atom->cp = farm_scsi_log_stats->firmware_revision;
				return PMDA_FETCH_STATIC;

			case SCSI_DEVICE_INTERFACE:
				atom->cp = farm_scsi_log_stats->device_interface;
				return PMDA_FETCH_STATIC;

			case SCSI_DEVICE_CAPACITY_IN_SECTORS:
				atom->ull = farm_scsi_log_stats->device_capacity_in_sectors;
				return PMDA_FETCH_STATIC;

			case SCSI_PHYSICAL_SECTOR_SIZE:
				atom->ull = farm_scsi_log_stats->physical_sector_size;
				return PMDA_FETCH_STATIC;

			case SCSI_LOGICAL_SECTOR_SIZE:
				atom->ull = farm_scsi_log_stats->logical_sector_size;
				return PMDA_FETCH_STATIC;

			case SCSI_DEVICE_BUFFER_SIZE:
				atom->ull = farm_scsi_log_stats->device_buffer_size;
				return PMDA_FETCH_STATIC;

			case SCSI_NUMBER_OF_HEADS:
				atom->ull = farm_scsi_log_stats->number_of_heads;
				return PMDA_FETCH_STATIC;

			case SCSI_DEVICE_FORM_FACTOR:
				atom->cp = farm_scsi_log_stats->device_form_factor;
				return PMDA_FETCH_STATIC;

			case SCSI_ROTATIONAL_RATE:
				atom->ull = farm_scsi_log_stats->rotational_rate;
				return PMDA_FETCH_STATIC;

			case SCSI_POWER_ON_HOURS:
				atom->ull = farm_scsi_log_stats->power_on_hours;
				return PMDA_FETCH_STATIC;

			case SCSI_POWER_CYCLE_COUNT:
				atom->ull = farm_scsi_log_stats->power_cycle_count;
				return PMDA_FETCH_STATIC;

			case SCSI_HARDWARE_RESET_COUNT:
				atom->ull = farm_scsi_log_stats->hardware_reset_count;
				return PMDA_FETCH_STATIC;

			case SCSI_ASSEMBLY_DATE:
				atom->cp = farm_scsi_log_stats->assembly_date;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_WORKLOAD_STATISTICS) {
		switch (item) {
			case SCSI_TOTAL_READ_COMMANDS:
				atom->ull = farm_scsi_log_stats->total_read_commands;
				return PMDA_FETCH_STATIC;

			case SCSI_TOTAL_WRITE_COMMANDS:
				atom->ull = farm_scsi_log_stats->total_write_commands;
				return PMDA_FETCH_STATIC;

			case SCSI_TOTAL_RANDOM_READ_COMMANDS:
				atom->ull = farm_scsi_log_stats->total_random_read_commands;
				return PMDA_FETCH_STATIC;

			case SCSI_TOTAL_RANDOM_WRITE_COMMANDS:
				atom->ull = farm_scsi_log_stats->total_random_write_commands;
				return PMDA_FETCH_STATIC;

			case SCSI_TOTAL_OTHER_COMMANDS:
				atom->ull = farm_scsi_log_stats->total_other_commands;
				return PMDA_FETCH_STATIC;

			case SCSI_LOGICAL_SECTORS_WRITTEN:
				atom->ull = farm_scsi_log_stats->logical_sectors_written;
				return PMDA_FETCH_STATIC;

			case SCSI_LOGICAL_SECTORS_READ:
				atom->ull = farm_scsi_log_stats->logical_sectors_read;
				return PMDA_FETCH_STATIC;

			case SCSI_READ_COMMANDS_0_3_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->read_commands_0_3_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_READ_COMMANDS_3_25_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->read_commands_3_25_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_READ_COMMANDS_25_75_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->read_commands_25_75_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_READ_CMMANDS_75_100_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->read_commands_75_100_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_WRITE_COMMANDS_0_3_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->write_commands_0_3_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_WRITE_COMMANDS_3_25_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->write_commands_3_25_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_WRITE_COMMANDS_25_75_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->write_commands_25_75_lba_space;
				return PMDA_FETCH_STATIC;

			case SCSI_WRITE_COMMANDS_75_100_LBA_SPACE:
				atom->ull = farm_scsi_log_stats->write_commands_75_100_lba_space;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_ERROR_STATISTICS) {
		switch (item) {
			case SCSI_UNRECOVERABLE_READ_ERRORS:
				atom->ull = farm_scsi_log_stats->unrecoverable_read_errors;
				return PMDA_FETCH_STATIC;

			case SCSI_UNRECOVERABLE_WRITE_ERRORS:
				atom->ull = farm_scsi_log_stats->unrecoverable_write_errors;
				return PMDA_FETCH_STATIC;

			case SCSI_MECHANICAL_START_FAILURES:
				atom->ull = farm_scsi_log_stats->mechanical_start_failures;
				return PMDA_FETCH_STATIC;

			case SCSI_FRU_CODE_MOST_RECENT_SMART_FRAME:
				atom->ull = farm_scsi_log_stats->fru_code_most_recent_smart_frame;
				return PMDA_FETCH_STATIC;

			case SCSI_INVALID_DWORD_COUNT_A:
				atom->ull = farm_scsi_log_stats->invalid_dword_count_a;
				return PMDA_FETCH_STATIC;

			case SCSI_INVALID_DWORD_COUNT_B:
				atom->ull = farm_scsi_log_stats->invalid_dword_count_b;
				return PMDA_FETCH_STATIC;

			case SCSI_DISPARITY_ERROR_CODE_A:
				atom->ull = farm_scsi_log_stats->dispartiy_error_code_a;
				return PMDA_FETCH_STATIC;

			case SCSI_DISPARITY_ERROR_CODE_B:
				atom->ull = farm_scsi_log_stats->dispartiy_error_code_b;
				return PMDA_FETCH_STATIC;

			case SCSI_LOSS_OF_DWORD_SYNC_A:
				atom->ull = farm_scsi_log_stats->loss_of_dword_sync_a;
				return PMDA_FETCH_STATIC;

			case SCSI_LOSS_OF_DWORD_SYNC_B:
				atom->ull = farm_scsi_log_stats->loss_of_dword_sync_b;
				return PMDA_FETCH_STATIC;

			case SCSI_PHY_RESET_PROBLEM_PORT_A:
				atom->ull = farm_scsi_log_stats->phy_reset_problem_port_a;
				return PMDA_FETCH_STATIC;

			case SCSI_PHY_RESET_PROBLEM_PORT_B:
				atom->ull = farm_scsi_log_stats->phy_reset_problem_port_b;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_ENVIRONMENTAL_STATISTICS) {
		switch (item) {
			case SCSI_CURRENT_TEMPERATURE:
				atom->ull = farm_scsi_log_stats->current_temperature;
				return PMDA_FETCH_STATIC;

			case SCSI_HIGHEST_TEMPERATURE:
				atom->ull = farm_scsi_log_stats->highest_temperature;
				return PMDA_FETCH_STATIC;

			case SCSI_LOWEST_TEMPERATURE:
				atom->ull = farm_scsi_log_stats->lowest_temperature;
				return PMDA_FETCH_STATIC;

			case SCSI_SPECIFIED_MAX_OPERATING_TEMPERATURE:
				atom->ull = farm_scsi_log_stats->specified_max_operating_temperature;
				return PMDA_FETCH_STATIC;

			case SCSI_SPECIFIED_MIN_OPERATING_TEMPERATURE:
				atom->ull = farm_scsi_log_stats->specified_min_operating_temperature;
				return PMDA_FETCH_STATIC;

			case SCSI_CURRENT_RELATIVE_HUMIDITY:
				atom->ull = farm_scsi_log_stats->current_relative_humidity;
				return PMDA_FETCH_STATIC;

			case SCSI_CURRENT_MOTOR_POWER:
				atom->ull = farm_scsi_log_stats->current_motor_power;
				return PMDA_FETCH_STATIC;

			case SCSI_12V_POWER_AVERAGE:
				atom->ull = farm_scsi_log_stats->_12v_power_average;
				return PMDA_FETCH_STATIC;

			case SCSI_12V_POWER_MINIUMUM:
				atom->ull = farm_scsi_log_stats->_12v_power_minimum;
				return PMDA_FETCH_STATIC;

			case SCSI_12V_POWER_MAXIMUM:
				atom->ull = farm_scsi_log_stats->_12v_power_maximum;
				return PMDA_FETCH_STATIC;

			case SCSI_5V_POWER_AVERAGE:
				atom->ull = farm_scsi_log_stats->_5v_power_average;
				return PMDA_FETCH_STATIC;

			case SCSI_5V_POWER_MINIMUM:
				atom->ull = farm_scsi_log_stats->_5v_power_minimum;
				return PMDA_FETCH_STATIC;

			case SCSI_5V_POWER_MAXIMUM:
				atom->ull = farm_scsi_log_stats->_5v_power_maximum;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_RELIABILITY_STATISTICS) {
		switch (item) {
			case SCSI_HELIUM_PRESSURE_THREHOLD_TRIPPED:
				atom->ull = farm_scsi_log_stats->helium_pressure_threshold_tripped;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_DRIVE_INFORMATION_CONTINUED) {
		switch (item) {
			case SCSI_DEPOPULATION_HEAD_MASK:
				atom->ull = farm_scsi_log_stats->depopulation_head_mask;
				return PMDA_FETCH_STATIC;

			case SCSI_PRODUCT_ID:
				atom->cp = farm_scsi_log_stats->product_id;
				return PMDA_FETCH_STATIC;

			case SCSI_DRIVE_RECORDING_TYPE:
				atom->cp = farm_scsi_log_stats->drive_recording_type;
				return PMDA_FETCH_STATIC;

			case SCSI_DEPOPPED:
				atom->ull = farm_scsi_log_stats->depopped;
				return PMDA_FETCH_STATIC;

			case SCSI_MAX_NUMBER_FOR_REASSIGNMENT:
				atom->ull = farm_scsi_log_stats->max_number_for_reassignment;
				return PMDA_FETCH_STATIC;

			case SCSI_TIME_TO_READY_LAST_POWER_CYCLE:
				atom->ull = farm_scsi_log_stats->time_to_ready_last_power_cycle;
				return PMDA_FETCH_STATIC;

			case SCSI_TIME_DRIVE_HELD_IN_STAGGERED_SPIN:
				atom->ull = farm_scsi_log_stats->time_drive_held_in_staggered_spin;
				return PMDA_FETCH_STATIC;

			case SCSI_SPIN_UP_TIME:
				atom->ull = farm_scsi_log_stats->spin_up_time;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	} else if (cluster == CLUSTER_SCSI_ENVIRONMENTAL_INFORMATION_CONTINUED) {
		switch (item) {
			case SCSI_CURRENT_12V:
				atom->ull = farm_scsi_log_stats->current_12_volts;
				return PMDA_FETCH_STATIC;

			case SCSI_MAXIMUM_12V:
				atom->ull = farm_scsi_log_stats->minimum_12_volts;
				return PMDA_FETCH_STATIC;

			case SCSI_MINIMUM_12V:
				atom->ull = farm_scsi_log_stats->maximum_12_volts;
				return PMDA_FETCH_STATIC;

			case SCSI_CURRENT_5V:
				atom->ull = farm_scsi_log_stats->current_5_volts;
				return PMDA_FETCH_STATIC;

			case SCSI_MAXIMUM_5V:
				atom->ull = farm_scsi_log_stats->minimum_5_volts;
				return PMDA_FETCH_STATIC;

			case SCSI_MINIMUM_5V:
				atom->ull = farm_scsi_log_stats->maximum_5_volts;
				return PMDA_FETCH_STATIC;

			default:
				return PM_ERR_PMID;
		}

	}

	/* NOTREACHED */
	return PMDA_FETCH_NOVALUES;
}

int 
farm_scsi_per_head_stats_fetch(int item, unsigned int inst, pmAtomValue *atom)
{
	struct  farm_scsi_per_head_stats *scsi_per_head_stats;
	pmInDom indom;
	int sts;

	switch  (item) {

                case SCSI_MR_HEAD_RESISTANCE:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->mr_head_resistance;
	                return PMDA_FETCH_STATIC;

                case SCSI_REALLOCATED_SECTORS:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->reallocated_sectors;
	                return PMDA_FETCH_STATIC;

                case SCSI_REALLOCATED_CANDIDATE_SECTORS:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->reallocated_candidate_sectors;
	                return PMDA_FETCH_STATIC;

                case SCSI_HEAD_POWER_ON_HOURS:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->power_on_hours;
	                return PMDA_FETCH_STATIC;

                case SCSI_CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_REPEATING:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->head_cumulative_lifetime_unrecoverable_read_repeating;
	                return PMDA_FETCH_STATIC;

                case SCSI_CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_UNIQUE:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->head_cumulative_lifetime_unrecoverable_read_unique;
	                return PMDA_FETCH_STATIC;

                case SCSI_SECOND_MR_HEAD_RESISTANCE:
	                indom = INDOM(SCSI_PER_HEAD_INDOM);
	                sts = pmdaCacheLookup(indom, inst, NULL, (void **)&scsi_per_head_stats);
	                
	                if (sts <0)
	                        return sts;

	                if (sts != PMDA_CACHE_ACTIVE)
	                        return PM_ERR_INST;
	                
	                atom->ull = scsi_per_head_stats->second_mr_head_resistance;
	                return PMDA_FETCH_STATIC;

		default:
			return PM_ERR_PMID;
	        
	}
}

int
farm_ata_refresh_data(const char *name, struct farm_ata_log_stats *farm_ata_log_stats)
{
	char buffer[4096], model_number[13], assembly_date[5] = {'\0'};
	uint64_t number_of_heads= 0, i = 0;
	int scanresult = 0, flash_led_event = -1, head_counter = -1;
	int dvga_head_counter = -1, rvga_head_counter = -1, fvga_head_counter = -1;
	int swdt_head_counter = -1, wpo_head_counter = -1, mr_head_counter = -1;
	int mr2_head_counter = -1, realloc_head_counter = -1, candidate_head_counter = -1;
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -l farm /dev/%s", farm_setup_stats, name);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while (fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

	char * trimmed = strtrim(buffer);

	if (strncmp(trimmed, "FARM Log Version:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %s", farm_ata_log_stats->log_version);

	if (strncmp(trimmed, "Pages Supported:", 16) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->pages_supported);

	if (strncmp(trimmed, "Log Size:", 9) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->log_size);

	if (strncmp(trimmed, "Page Size:", 10) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->page_size);

	if (strncmp(trimmed, "Heads Supported:", 16) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->heads_supported);

	if (strncmp(trimmed, "Number of Copies:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->number_of_copies);

	if (strncmp(trimmed, "Reason for Frame Capture:", 25) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->reason_for_frame_capture);

	if (strncmp(trimmed, "Serial Number:", 14) == 0)
		sscanf(buffer, "%*s%*s %s", farm_ata_log_stats->serial_number);

	if (strncmp(trimmed, "World Wide Name:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %s", farm_ata_log_stats->world_wide_name);

	if (strncmp(trimmed, "Device Interface:", 17) == 0)
		sscanf(buffer, "%*s%*s %s", farm_ata_log_stats->device_interface);

	if (strncmp(trimmed, "Device Capacity in Sectors:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->device_capacity_in_sectors);

	if (strncmp(trimmed, "Physical Sector Size:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->physical_sector_size);

	if (strncmp(trimmed, "Logical Sector Size:", 20) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->logical_sector_size);

	if (strncmp(trimmed, "Device Buffer Size:", 19) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->device_buffer_size);

	if (strncmp(trimmed, "Number of Heads:", 16) == 0) {
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->number_of_heads);
      
		number_of_heads = farm_ata_log_stats->number_of_heads;
	}

	if (strncmp(trimmed, "Device Form Factor:", 19) == 0)
		sscanf(buffer, "%*s%*s%*s %[^\n]", farm_ata_log_stats->device_form_factor);

	if (strncmp(trimmed, "Rotation Rate:", 14) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->rotational_rate);

	if (strncmp(trimmed, "Firmware Rev:", 13) == 0)
		sscanf(buffer, "%*s%*s %s", farm_ata_log_stats->firmware_rev);

	if (strncmp(trimmed, "ATA Security State", 18) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %s", farm_ata_log_stats->ata_security_state);

	if (strncmp(trimmed, "ATA Features Supported", 22) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %s", farm_ata_log_stats->ata_features_supported);

	if (strncmp(trimmed, "ATA Features Enabled", 20) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %s", farm_ata_log_stats->ata_features_enabled);

	if (strncmp(trimmed, "Power on Hours:", 15) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->power_on_hours);

	if (strncmp(trimmed, "Spindle Power on Hours:", 23) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->spindle_power_on_hours);

	if (strncmp(trimmed, "Head Flight Hours:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->head_flight_hours);

	if (strncmp(trimmed, "Head Load Events:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->head_load_events);

	if (strncmp(trimmed, "Power Cycle Count:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->power_cycle_count);

	if (strncmp(trimmed, "Hardware Reset Count:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->hardware_reset_count);

	if (strncmp(trimmed, "Spin-up Time:", 13) == 0)
 		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->spin_up_time);

	if (strncmp(trimmed, "Time to ready", 13) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->time_to_ready_last_power_cycle);

	if (strncmp(trimmed, "Time drive is held", 18) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->time_drive_held_in_staggered_spin);

	if (strncmp(trimmed, "Model Number:", 13) == 0) {
		scanresult = sscanf(buffer, "%*s%*s %s", model_number);
      
		if (scanresult == 1) {
			pmstrncpy(farm_ata_log_stats->model_number, sizeof(farm_ata_log_stats->model_number), model_number);
		} else {
			farm_ata_log_stats->model_number[0] = '\0';
		}
	}

	if (strncmp(trimmed, "Drive Recording Type:", 20) == 0)
		sscanf(buffer, "%*s%*s%*S %s", farm_ata_log_stats->drive_recording_type);

	if (strncmp(trimmed, "Max Number of Available Sectors for Reassignment:", 49) == 0)
 		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->max_number_available_sectors_reassignment);

	if (strncmp(trimmed, "Assembly Date (YYWW):", 21) == 0) {
		scanresult = sscanf(buffer, "%*s%*s%*s %s", assembly_date);

		if (scanresult == 1) {
			pmstrncpy(farm_ata_log_stats->assembly_date, sizeof(farm_ata_log_stats->assembly_date), assembly_date);
		} else {
			farm_ata_log_stats->assembly_date[0] = '\0';
		}
	}

	if (strncmp(trimmed, "Depopulation Head Mask:", 23) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->depopulation_head_mask);

	if (strncmp(trimmed, "Total Number of Read Commands:", 30) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_read_commands);

	if (strncmp(trimmed, "Total Number of Write Commands:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_write_commands);

	if (strncmp(trimmed, "Total Number of Random Read Commands:", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_random_read_commands);

	if (strncmp(trimmed, "Total Number of Random Write Commands:", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_random_write_commands);

	if (strncmp(trimmed, "Total Number Of Other Commands:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_other_commands);

	if (strncmp(trimmed, "Logical Sectors Written:", 24) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->logical_sectors_written);

	if (strncmp(trimmed, "Logical Sectors Read:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->logical_sectors_read);

	if (strncmp(trimmed, "Number of dither events during", 30) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->dither_events_current_power_cycle);

	if (strncmp(trimmed, "Number of times dither was held off during random", 49) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->dither_held_off_random_workloads);

	if (strncmp(trimmed, "Number of times dither was held off during sequential", 53) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->dither_held_off_sequential_workloads);

	if (strncmp(trimmed, "Number of Read commands from 0-3.125%", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->read_commands_0_3_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Read commands from 3.125-25%", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->read_commands_3_25_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Read commands from 25-75%", 35) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->read_commands_25_75_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Read commands from 75-100%", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->read_commands_75_100_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Write commands from 0-3.125%", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->write_commands_0_3_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Write commands from 3.125-25%", 39) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->write_commands_3_25_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Write commands from 25-75%", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->write_commands_25_75_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Number of Write commands from 75-100%", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->write_commands_75_100_lba_space_last_3_smart_summary_frames);

	if (strncmp(trimmed, "Unrecoverable Read Errors:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->unrecoverable_read_errors);
  
	if (strncmp(trimmed, "Unrecoverable Write Errors:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->unrecoverable_write_errors);

	if (strncmp(trimmed, "Number of Reallocated Sectors:", 30) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->reallocated_sectors);

	if (strncmp(trimmed, "Number of Read Recovery Attempts:", 33) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->read_recovery_attempts);
	
	if (strncmp(trimmed, "Number of Mechanical Start Failures:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->mechanical_start_failures);

	if (strncmp(trimmed, "Number of Reallocated Candidate Sectors:", 40) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->reallocated_candidate_sectors);

	if (strncmp(trimmed, "Number of ASR Events:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->asr_events);
  
	if (strncmp(trimmed, "Number of Interface CRC Errors:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->interface_crc_errors);
  
	if (strncmp(trimmed, "Spin Retry Count:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->spin_retry_count);

	if (strncmp(trimmed, "Spin Retry Count Normalized:", 28) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->spin_retry_count_normalized);

	if (strncmp(trimmed, "Spin Retry Count Worst:", 23) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->spin_retry_count_worst);

	if (strncmp(trimmed, "Number of IOEDC Errors (Raw):", 29) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->ioedc_errors);

	if (strncmp(trimmed, "CTO Count Total:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->cto_count_total);
      
	if (strncmp(trimmed, "CTO Count Over 5s:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->cto_count_over_5s);

	if (strncmp(trimmed, "CTO Count Over 7.5s:", 20) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->cto_count_over_7s);

	if (strncmp(trimmed, "Total Flash LED (Assert) Events:", 32) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->total_flash_led_assert_events);
  
	if (strncmp(trimmed, "Index of the last Flash LED:", 28) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->index_of_last_flash_led);

	if (strstr(buffer, "Flash LED Event"))
		flash_led_event++;

	if ((strncmp(trimmed, "Event Information:", 18) == 0)  && (flash_led_event < MAX_NUMBER_OF_LED_EVENTS))
		sscanf(buffer, "%*s%*s %"SCNx64"", &farm_ata_log_stats->led_event_information[flash_led_event]);

	if ((strncmp(trimmed, "Timestamp of Event", 18) == 0)  && (flash_led_event < MAX_NUMBER_OF_LED_EVENTS))
		sscanf(buffer, "%*s%*s%*s%*d%*s %"SCNu64"", &farm_ata_log_stats->led_event_timestamp_of_event[flash_led_event]);

	if ((strncmp(trimmed, "Power Cycle Event", 17) == 0)  && (flash_led_event < MAX_NUMBER_OF_LED_EVENTS))
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->led_event_power_cycle_event[flash_led_event]);

	if (strncmp(trimmed, "Uncorrectable errors:", 21) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->uncorrectable_errors);

	if (strncmp(trimmed, "Cumulative Lifetime Unrecoverable Read errors", 45) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->cumulative_lifetime_unrecoverable_errors_due_to_erc);

	if (strstr(buffer, "Cum Lifetime Unrecoverable"))
		head_counter++;

	if ((strncmp(trimmed, "Cumulative Lifetime Unrecoverable Read Repeating:", 49) == 0) && (head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->head_cumulative_lifetime_unrecoverable_read_repeating[head_counter]);

	if ((strncmp(trimmed, "Cumulative Lifetime Unrecoverable Read Unique:", 46) == 0)  && (head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->head_cumulative_lifetime_unrecoverable_read_unique[head_counter]);

	if (strncmp(trimmed, "Current Temperature (Celsius):", 30) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->current_temperature);

	if (strncmp(trimmed, "Highest Temperature:", 20) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->highest_temperature);

	if (strncmp(trimmed, "Lowest Temperature:", 19) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_ata_log_stats->lowest_temperature);

	if (strncmp(trimmed, "Average Short Term Temperature:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->average_short_term_temperature);

	if (strncmp(trimmed, "Average Long Term Temperature:", 30) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->average_long_term_temperature);

	if (strncmp(trimmed, "Highest Average Short Term Temperature:", 39) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->highest_average_short_term_temperature);

	if (strncmp(trimmed, "Lowest Average Short Term Temperature:", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->lowest_average_short_term_temperature);

	if (strncmp(trimmed, "Highest Average Long Term Temperature:", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->highest_average_long_term_temperature);

	if (strncmp(trimmed, "Lowest Average Long Term Temperature:", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->lowest_average_long_term_temperature);

	if (strncmp(trimmed, "Time In Over Temperature", 24) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->time_in_over_temperature);

	if (strncmp(trimmed, "Time In Under Temperature", 25) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->time_in_under_temperature);

	if (strncmp(trimmed, "Specified Max Operating Temperature:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->specified_max_operating_temperature);

	if (strncmp(trimmed, "Specified Min Operating Temperature:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->specified_min_operating_temperature);

	if (strncmp(trimmed, "Current Relative Humidity:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->current_relative_humidity);

	if (strncmp(trimmed, "Current Motor Power:", 20) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->current_motor_power);

	if (strncmp(trimmed, "Current 12 volts:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->current_12_volts);

	if (strncmp(trimmed, "Minimum 12 volts:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->minimum_12_volts);

	if (strncmp(trimmed, "Maximum 12 volts:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->maximum_12_volts);

	if (strncmp(trimmed, "Current 5 volts:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->current_5_volts);

	if (strncmp(trimmed, "Minimum 5 volts:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->minimum_5_volts);

	if (strncmp(trimmed, "Maximum 5 volts:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->maximum_5_volts);

	if (strncmp(trimmed, "12V Power Average:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_12v_power_average);

	if (strncmp(trimmed, "12V Power Minimum:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_12v_power_minimum);

	if (strncmp(trimmed, "12V Power Maximum:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_12v_power_maximum);

	if (strncmp(trimmed, "5V Power Average:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_5v_power_average);

	if (strncmp(trimmed, "5V Power Minimum:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_5v_power_minimum);

	if (strncmp(trimmed, "5V Power Maximum:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %lf", &farm_ata_log_stats->_5v_power_maximum);

	if (strncmp(trimmed, "Error Rate (SMART Attribute 1 Raw):", 35) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNx64"", &farm_ata_log_stats->error_rate_smart_1_raw);

	if (strncmp(trimmed, "Error Rate (SMART Attribute 1 Normalized):", 42) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->error_rate_smart_1_normalized);

	if (strncmp(trimmed, "Error Rate (SMART Attribute 1 Worst):", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->error_rate_smart_1_worst);

	if (strncmp(trimmed, "Seek Error Rate (SMART Attr 7 Raw):", 35) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNx64"", &farm_ata_log_stats->seek_error_rate_smart_7_raw);

	if (strncmp(trimmed, "Seek Error Rate (SMART Attr 7 Normalized):", 42) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->seek_error_rate_smart_7_normalized);

	if (strncmp(trimmed, "Seek Error Rate (SMART Attr 7 Worst):", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->seek_error_rate_smart_7_worst);

	if (strncmp(trimmed, "High Priority Unload Events:", 28) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->high_priority_unload_events);

	if (strncmp(trimmed, "Helium Pressure Threshold Tripped:", 34) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->helium_pressure_threshold_tripped);

	if (strncmp(trimmed, "LBAs Corrected By Parity Sector:", 32) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->lbas_corrected_by_parity_sector);

	if (strstr(buffer, "DVGA"))
		dvga_head_counter++;

	if ((strncmp(trimmed, "DVGA Skip", 9) == 0)  && (dvga_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->dvga_skip_write_detected_head[dvga_head_counter]);

	if (strstr(buffer, "RVGA"))
		rvga_head_counter++;

	if ((strncmp(trimmed, "RVGA Skip", 9) == 0)  && (rvga_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->rvga_skip_write_detected_head[rvga_head_counter]);

	if (strstr(buffer, "FVGA"))
		fvga_head_counter++;

	if ((strncmp(trimmed, "FVGA Skip", 9) == 0)  && (fvga_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->fvga_skip_write_detected_head[fvga_head_counter]);

	if (strstr(buffer, "Exceeded by Head"))
		swdt_head_counter++;

	if ((strncmp(trimmed, "Skip Write Detect Threshold", 27) == 0)  && (swdt_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->skip_write_detect_threshold_detect_head[swdt_head_counter]);

	if (strstr(buffer, "Write Power On"))
		wpo_head_counter++;

	if ((strncmp(trimmed, "Write Power On", 14) == 0)  && (wpo_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->write_power_secs_head[wpo_head_counter]);

	if (strstr(buffer, "Resistance from"))
		mr_head_counter++;

	if ((strncmp(trimmed, "MR Head Resistance from", 23) == 0)  && (mr_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->mr_head_resistance_head[mr_head_counter]);

	if (strstr(buffer, "Resistance by"))
		mr2_head_counter++;

	if ((strncmp(trimmed, "Second MR Head", 14) == 0)  && (mr2_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->second_mr_head_resistance_head[mr2_head_counter]);

	if (strstr(buffer, "Reallocated Sectors by"))
		realloc_head_counter++;

	if ((strncmp(trimmed, "Number of Reallocated Sectors by", 32) == 0)  && (realloc_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->number_reallocated_sectors_head[realloc_head_counter]);

	if (strstr(buffer, "Candidate Sectors by"))
		candidate_head_counter++;

	if ((strncmp(trimmed, "Number of Reallocation", 22) == 0)  && (candidate_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_ata_log_stats->number_reallocation_candidate_sectors_head[candidate_head_counter]);

	}
	pclose(pf);

	/* Set "-1" for the rest of the values where we do not have heads
	 * present for the metrics that are per head, we can mark no value
	 * recorded during fetch.
	 *
	 */
	for (i = number_of_heads; i < MAX_NUMBER_OF_SUPPORTED_HEADS; i++) {
	        farm_ata_log_stats->head_cumulative_lifetime_unrecoverable_read_repeating[i] = -1;
        	farm_ata_log_stats->head_cumulative_lifetime_unrecoverable_read_unique[i] = -1;
        	farm_ata_log_stats->dvga_skip_write_detected_head[i] = -1;
        	farm_ata_log_stats->rvga_skip_write_detected_head[i] = -1;
        	farm_ata_log_stats->fvga_skip_write_detected_head[i] = -1;
        	farm_ata_log_stats->skip_write_detect_threshold_detect_head[i] = -1;
        	farm_ata_log_stats->write_power_secs_head[i] = -1;
        	farm_ata_log_stats->mr_head_resistance_head[i] = -1;
        	farm_ata_log_stats->second_mr_head_resistance_head[i] = -1;
	        farm_ata_log_stats->number_reallocated_sectors_head[i]= -1;
		farm_ata_log_stats->number_reallocation_candidate_sectors_head[i] = -1;
	 }
	
	return 0;
}

int 
farm_ata_refresh_led_events(void)
{
	char inst_name[128], *dev_name;
	struct seagate_disk *dev;
        int inst, sts;

	pmInDom disk_indom = INDOM(DISK_INDOM);
	pmInDom flash_led_indom = INDOM(FLASH_LED_INDOM);

	for (pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_REWIND);;) {
		if ((inst = pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(disk_indom, inst, &dev_name, (void **)&dev) || !dev)
			continue;

                for (int i = 0; i < MAX_NUMBER_OF_LED_EVENTS; i++) {
                        pmsprintf(inst_name, sizeof(inst_name), "%s::event_%d", dev_name, i);

                        struct farm_flash_led_events *flash_led_events;

	                sts = pmdaCacheLookupName(flash_led_indom, inst_name, NULL, (void **)&flash_led_events);
		                if (sts == PM_ERR_INST || (sts >=0 && flash_led_events == NULL)) {
			                flash_led_events = calloc(1, sizeof(struct farm_flash_led_events));
			                if (flash_led_events == NULL) {
				                return PM_ERR_AGAIN;
			                }
		                }
		                else if (sts < 0)
			                continue;

                        flash_led_events->event_id = i;
			flash_led_events->led_event_information = dev->farm_ata_log_stats.led_event_information[i];
			flash_led_events->led_event_timestamp = dev->farm_ata_log_stats.led_event_timestamp_of_event[i];
			flash_led_events->led_event_power_cycle = dev->farm_ata_log_stats.led_event_power_cycle_event[i];

                        pmdaCacheStore(flash_led_indom, PMDA_CACHE_ADD, inst_name, (void *)flash_led_events);   
                }
	}
	return 0;
}

int 
farm_ata_refresh_per_head_stats(void)
{
	char inst_name[128], *dev_name;
	struct seagate_disk *dev;
        int inst, sts;

	pmInDom disk_indom = INDOM(DISK_INDOM);
	pmInDom per_head_indom = INDOM(PER_HEAD_INDOM);

	pmdaCacheOp(per_head_indom, PMDA_CACHE_INACTIVE);

	for (pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_REWIND);;) {
		if ((inst = pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(disk_indom, inst, &dev_name, (void **)&dev) || !dev)
			continue;

                for (int i = 0; i < dev->farm_ata_log_stats.number_of_heads; i++) {
                        pmsprintf(inst_name, sizeof(inst_name), "%s::head_%d", dev_name, i);

                        struct farm_per_head_stats *per_head_stats;

	                sts = pmdaCacheLookupName(per_head_indom, inst_name, NULL, (void **)&per_head_stats);
		                if (sts == PM_ERR_INST || (sts >=0 && per_head_stats == NULL)) {
			                per_head_stats = calloc(1, sizeof(struct farm_per_head_stats));
			                if (per_head_stats == NULL) {
				                return PM_ERR_AGAIN;
			                }
		                }
		                else if (sts < 0)
			                continue;

                        per_head_stats->head_id = i;
		        per_head_stats->head_cumulative_lifetime_unrecoverable_read_repeating  = dev->farm_ata_log_stats.head_cumulative_lifetime_unrecoverable_read_repeating[i];
                        per_head_stats->head_cumulative_lifetime_unrecoverable_read_unique  = dev->farm_ata_log_stats.head_cumulative_lifetime_unrecoverable_read_unique[i];
	                per_head_stats->dvga_skip_write_detected_head  = dev->farm_ata_log_stats.dvga_skip_write_detected_head[i];
                        per_head_stats->rvga_skip_write_detected_head  = dev->farm_ata_log_stats.rvga_skip_write_detected_head[i];
                        per_head_stats->fvga_skip_write_detected_head  = dev->farm_ata_log_stats.fvga_skip_write_detected_head[i];
                        per_head_stats->skip_write_detect_threshold_detect_head  = dev->farm_ata_log_stats.skip_write_detect_threshold_detect_head[i];
                        per_head_stats->write_power_secs_head  = dev->farm_ata_log_stats.write_power_secs_head[i];
                        per_head_stats->mr_head_resistance_head  = dev->farm_ata_log_stats.mr_head_resistance_head[i];
                        per_head_stats->second_mr_head_resistance_head  = dev->farm_ata_log_stats.second_mr_head_resistance_head[i];
                        per_head_stats->number_reallocated_sectors_head  = dev->farm_ata_log_stats.number_reallocated_sectors_head[i];
                        per_head_stats->number_reallocation_candidate_sectors_head  = dev->farm_ata_log_stats.number_reallocation_candidate_sectors_head[i];

		    	pmdaCacheStore(per_head_indom, PMDA_CACHE_ADD, inst_name, (void *)per_head_stats);
                }
	}
	return 0;
}

int
farm_scsi_refresh_data(const char *name, struct farm_scsi_log_stats *farm_scsi_log_stats)
{
	char buffer[4096], product_id[13], assembly_date[5] = {'\0'};
	uint64_t number_of_heads = 0, i = 0;
	int scanresult = 0;	
	int mr_head_counter = -1, realloc_head_counter = -1, candidate_head_counter = -1;
	int power_head_counter = - 1, repeat_head_counter = -1, unique_head_counter = -1;
	int mr2_head_counter = -1;	
	FILE *pf;

	pmsprintf(buffer, sizeof(buffer), "%s -l farm /dev/%s", farm_setup_stats, name);

	if ((pf = popen(buffer, "r")) == NULL)
		return -oserror();

	while (fgets(buffer, sizeof(buffer)-1, pf) != NULL) {

	char *trimmed = strtrim(buffer);

	if (strncmp(trimmed, "FARM Log Version:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %s", farm_scsi_log_stats->log_version);

	if (strncmp(trimmed, "Pages Supported:", 16) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_scsi_log_stats->pages_supported);

	if (strncmp(trimmed, "Log Size:", 9) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_scsi_log_stats->log_size);

	if (strncmp(trimmed, "Heads Supported:", 16) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_scsi_log_stats->heads_supported);

	if (strncmp(trimmed, "Reason for Frame Capture:", 25) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->reason_for_frame_capture);

	if (strncmp(trimmed, "Serial Number:", 14) == 0)
		sscanf(buffer, "%*s%*s %s", farm_scsi_log_stats->serial_number);

	if (strncmp(trimmed, "World Wide Name:", 16) == 0)
		sscanf(buffer, "%*s%*s%*s %s", farm_scsi_log_stats->world_wide_name);

	if (strncmp(trimmed, "Firmware Rev:", 13) == 0)
		sscanf(buffer, "%*s%*s %s", farm_scsi_log_stats->firmware_revision);

	if (strncmp(trimmed, "Device Interface:", 17) == 0)
		sscanf(buffer, "%*s%*s %s", farm_scsi_log_stats->device_interface);

	if (strncmp(trimmed, "Device Capacity in Sectors:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->device_capacity_in_sectors);

	if (strncmp(trimmed, "Physical Sector Size:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->physical_sector_size);

	if (strncmp(trimmed, "Logical Sector Size:", 20) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->logical_sector_size);

	if (strncmp(trimmed, "Device Buffer Size (Bytes):", 27) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->device_buffer_size);

	if (strncmp(trimmed, "Number of heads:", 16) == 0) {
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->number_of_heads);
      
		number_of_heads = farm_scsi_log_stats->number_of_heads;
	}

	if (strncmp(trimmed, "Device form factor:", 19) == 0)
		sscanf(buffer, "%*s%*s%*s %[^\n]", farm_scsi_log_stats->device_form_factor);

	if (strncmp(trimmed, "Rotation Rate (RPM):", 20) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->rotational_rate);

	if (strncmp(trimmed, "Power on Hour:", 14) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->power_on_hours);

	if (strncmp(trimmed, "Power Cycle count:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->power_cycle_count);

	if (strncmp(trimmed, "Hardware Reset Count:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->hardware_reset_count);

	if (strncmp(trimmed, "Date of Assembly (YYWW):", 24) == 0) {
		scanresult = sscanf(buffer, "%*s%*s%*s%*s %s", assembly_date);

		if (scanresult == 1) {
			pmstrncpy(farm_scsi_log_stats->assembly_date, sizeof(farm_scsi_log_stats->assembly_date), assembly_date);
		} else {
			farm_scsi_log_stats->assembly_date[0] = '\0';
		}
	}
	if (strncmp(trimmed, "Total Number of Read Commands:", 30) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->total_read_commands);

	if (strncmp(trimmed, "Total Number of Write Commands:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->total_write_commands);

	if (strncmp(trimmed, "Total Number of Random Read Cmds:", 33) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->total_random_read_commands);

	if (strncmp(trimmed, "Total Number of Random Write Cmds:", 34) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->total_random_write_commands);

	if (strncmp(trimmed, "Total Number of Other Commands:", 31) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->total_other_commands);

	if (strncmp(trimmed, "Logical Sectors Written:", 24) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->logical_sectors_written);

	if (strncmp(trimmed, "Logical Sectors Read:", 21) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->logical_sectors_read);

	if (strncmp(trimmed, "Number of Read commands from 0-3.125%", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->read_commands_0_3_lba_space);

	if (strncmp(trimmed, "Number of Read commands from 3.125-25%", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->read_commands_3_25_lba_space);

	if (strncmp(trimmed, "Number of Read commands from 25-75%", 35) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->read_commands_25_75_lba_space);

	if (strncmp(trimmed, "Number of Read commands from 75-100%", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->read_commands_75_100_lba_space);

	if (strncmp(trimmed, "Number of Write commands from 0-3.125%", 38) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->write_commands_0_3_lba_space);

	if (strncmp(trimmed, "Number of Write commands from 3.125-25%", 39) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->write_commands_3_25_lba_space);

	if (strncmp(trimmed, "Number of Write commands from 25-75%", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->write_commands_25_75_lba_space);

	if (strncmp(trimmed, "Number of Write commands from 75-100%", 37) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->write_commands_75_100_lba_space);

	if (strncmp(trimmed, "Unrecoverable Read Errors:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->unrecoverable_read_errors);
  
	if (strncmp(trimmed, "Unrecoverable Write Errors:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->unrecoverable_write_errors);

	if (strncmp(trimmed, "Number of Mechanical Start Failures:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->mechanical_start_failures);

	if (strncmp(trimmed, "FRU code if smart trip from most recent SMART Frame:", 52) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->fru_code_most_recent_smart_frame);

	if (strncmp(trimmed, "Invalid DWord Count Port A:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->invalid_dword_count_a);

	if (strncmp(trimmed, "Invalid DWord Count Port B:", 27) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->invalid_dword_count_b);

	if (strncmp(trimmed, "Disparity Error Count Port A:", 29) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->dispartiy_error_code_a);

	if (strncmp(trimmed, "Disparity Error Count Port B:", 29) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->dispartiy_error_code_b);

	if (strncmp(trimmed, "Loss Of DWord Sync Port A:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->loss_of_dword_sync_a);

	if (strncmp(trimmed, "Loss Of DWord Sync Port B:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->loss_of_dword_sync_b);

	if (strncmp(trimmed, "Phy Reset Problem Port A:", 25) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->phy_reset_problem_port_a);

	if (strncmp(trimmed, "Phy Reset Problem Port B:", 25) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->phy_reset_problem_port_b);

	if (strncmp(trimmed, "Current Temperature (Celsius):", 30) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->current_temperature);

	if (strncmp(trimmed, "Highest Temperature:", 20) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_scsi_log_stats->highest_temperature);

	if (strncmp(trimmed, "Lowest Temperature:", 19) == 0)
		sscanf(buffer, "%*s%*s %"SCNu64"", &farm_scsi_log_stats->lowest_temperature);

	if (strncmp(trimmed, "Specified Max Operating Temperature:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->specified_max_operating_temperature);

	if (strncmp(trimmed, "Specified Min Operating Temperature:", 36) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->specified_min_operating_temperature);

	if (strncmp(trimmed, "Current Relative Humidity:", 26) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->current_relative_humidity);

	if (strncmp(trimmed, "Current Motor Power:", 20) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->current_motor_power);

	if (strncmp(trimmed, "12V Power Average:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_12v_power_average);

	if (strncmp(trimmed, "12V Power Minimum:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_12v_power_minimum);

	if (strncmp(trimmed, "12V Power Maximum:", 18) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_12v_power_maximum);

	if (strncmp(trimmed, "5V Power Average:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_5v_power_average);

	if (strncmp(trimmed, "5V Power Minimum:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_5v_power_minimum);

	if (strncmp(trimmed, "5V Power Maximum:", 17) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->_5v_power_maximum);

	if (strncmp(trimmed, "Helium Pressure Threshold Tripped:", 34) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->helium_pressure_threshold_tripped);

	if (strncmp(trimmed, "Depopulation Head Mask:", 23) == 0)
		sscanf(buffer, "%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->depopulation_head_mask);

	if (strncmp(trimmed, "Product ID:", 11) == 0) {
		scanresult = sscanf(buffer, "%*s%*s %s", product_id);
      
		if (scanresult == 1) {
			pmstrncpy(farm_scsi_log_stats->product_id, sizeof(farm_scsi_log_stats->product_id), product_id);
		} else {
			farm_scsi_log_stats->product_id[0] = '\0';
		}
	}

	if (strncmp(trimmed, "Drive Recording Type:", 20) == 0)
		sscanf(buffer, "%*s%*s%*S %s", farm_scsi_log_stats->drive_recording_type);

	if (strncmp(trimmed, "Has Drive been Depopped:", 24) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->depopped);

	if (strncmp(trimmed, "Max Number of Available Sectors for Reassignment:", 49) == 0)
 		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->max_number_for_reassignment);

	if (strncmp(trimmed, "Time to ready", 13) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->time_to_ready_last_power_cycle);

	if (strncmp(trimmed, "Time drive is held", 18) == 0)
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->time_drive_held_in_staggered_spin);

	if (strncmp(trimmed, "Last Servo Spin up Time (ms):", 29) == 0)
 		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->spin_up_time);

	if (strncmp(trimmed, "Current 12 Volts (mV):", 17) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->current_12_volts);

	if (strncmp(trimmed, "Minimum 12 Volts (mV):", 17) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->minimum_12_volts);

	if (strncmp(trimmed, "Maximum 12 Volts (mV):", 17) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->maximum_12_volts);

	if (strncmp(trimmed, "Current 5 Volts (mV):", 16) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->current_5_volts);

	if (strncmp(trimmed, "Minimum 5 Volts (mV):", 16) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->minimum_5_volts);

	if (strncmp(trimmed, "Maximum 5 Volts (mV):", 16) == 0)
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->maximum_5_volts);

	if (strstr(buffer, "MR Head Resistance"))
		mr_head_counter++;

	if ((strncmp(trimmed, "MR Head Resistance", 18) == 0)  && (mr_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->mr_head_resistance[mr_head_counter]);

	if (strstr(buffer, "Number of Reallocated Sectors"))
		realloc_head_counter++;

	if ((strncmp(trimmed, "Number of Reallocated Sectors", 29) == 0)  && (realloc_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->reallocated_sectors[realloc_head_counter]);

	if (strstr(buffer, "Number of Reallocation"))
		candidate_head_counter++;

	if ((strncmp(trimmed, "Number of Reallocation Candidate Sectors", 40) == 0)  && (candidate_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->reallocated_candidate_sectors[candidate_head_counter]);

	if (strstr(buffer, "Write Power On"))
		power_head_counter++;

	if ((strncmp(trimmed, "Write Power On (hrs)", 20) == 0)  && (power_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->head_power_on_hours[power_head_counter]);

	if (strstr(buffer, "Cum Lifetime Unrecoverable Read Repeating"))
		repeat_head_counter++;

	if ((strncmp(trimmed, "Cum Lifetime Unrecoverable Read Repeating", 41) == 0)  && (repeat_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->head_cumulative_lifetime_unrecoverable_read_repeating[repeat_head_counter]);

	if (strstr(buffer, "Cum Lifetime Unrecoverable Read Unique"))
		unique_head_counter++;

	if ((strncmp(trimmed, "Cum Lifetime Unrecoverable Read Unique", 38) == 0)  && (unique_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->head_cumulative_lifetime_unrecoverable_read_unique[unique_head_counter]);

	if (strstr(buffer, "Second MR"))
		mr2_head_counter++;

	if ((strncmp(trimmed, "Second MR Head Resistance", 25) == 0)  && (mr2_head_counter < number_of_heads))
		sscanf(buffer, "%*s%*s%*s%*s%*s %"SCNu64"", &farm_scsi_log_stats->second_mr_head_resistance[mr2_head_counter]);

	if (strstr(buffer, "FARM Log Actuator"))
		break;

	}
	pclose(pf);

	/* Set "-1" for the rest of the values where we do not have heads
	 * present for the metrics that are per head, we can mark no value
	 * recorded during fetch.
	 *
	 */
	for (i = number_of_heads; i < MAX_NUMBER_OF_SUPPORTED_HEADS; i++) {
	        farm_scsi_log_stats->mr_head_resistance[i] = -1;
        	farm_scsi_log_stats->reallocated_sectors[i] = -1;
        	farm_scsi_log_stats->reallocated_candidate_sectors[i] = -1;
        	farm_scsi_log_stats->head_power_on_hours[i] = -1;
        	farm_scsi_log_stats->head_cumulative_lifetime_unrecoverable_read_repeating[i] = -1;
        	farm_scsi_log_stats->head_cumulative_lifetime_unrecoverable_read_unique[i] = -1;
        	farm_scsi_log_stats->second_mr_head_resistance[i] = -1;
	 }
	
	return 0;
}

int 
farm_scsi_refresh_per_head_stats(void)
{
	char inst_name[128], *dev_name;
	struct seagate_disk *dev;
        int inst, sts;

	pmInDom disk_indom = INDOM(SCSI_DISK_INDOM);
	pmInDom per_head_indom = INDOM(SCSI_PER_HEAD_INDOM);

	pmdaCacheOp(per_head_indom, PMDA_CACHE_INACTIVE);

	for (pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_REWIND);;) {
		if ((inst = pmdaCacheOp(disk_indom, PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(disk_indom, inst, &dev_name, (void **)&dev) || !dev)
			continue;

                for (int i = 0; i < dev->farm_scsi_log_stats.number_of_heads; i++) {
                        pmsprintf(inst_name, sizeof(inst_name), "%s::head_%d", dev_name, i);

                        struct farm_scsi_per_head_stats *scsi_per_head_stats;

	                sts = pmdaCacheLookupName(per_head_indom, inst_name, NULL, (void **)&scsi_per_head_stats);
		                if (sts == PM_ERR_INST || (sts >=0 && scsi_per_head_stats == NULL)) {
			                scsi_per_head_stats = calloc(1, sizeof(struct farm_scsi_per_head_stats));
			                if (scsi_per_head_stats == NULL) {
				                return PM_ERR_AGAIN;
			                }
		                }
		                else if (sts < 0)
			                continue;

                        scsi_per_head_stats->head_id = i;
		        scsi_per_head_stats->mr_head_resistance = dev->farm_scsi_log_stats.mr_head_resistance[i];
                        scsi_per_head_stats->reallocated_sectors  = dev->farm_scsi_log_stats.reallocated_sectors[i];
	                scsi_per_head_stats->reallocated_candidate_sectors  = dev->farm_scsi_log_stats.reallocated_candidate_sectors[i];
                        scsi_per_head_stats->power_on_hours  = dev->farm_scsi_log_stats.head_power_on_hours[i];
                        scsi_per_head_stats->head_cumulative_lifetime_unrecoverable_read_repeating  = dev->farm_scsi_log_stats.head_cumulative_lifetime_unrecoverable_read_repeating[i];
                        scsi_per_head_stats->head_cumulative_lifetime_unrecoverable_read_unique  = dev->farm_scsi_log_stats.head_cumulative_lifetime_unrecoverable_read_unique[i];
                        scsi_per_head_stats->second_mr_head_resistance  = dev->farm_scsi_log_stats.second_mr_head_resistance[i];

		    	pmdaCacheStore(per_head_indom, PMDA_CACHE_ADD, inst_name, (void *)scsi_per_head_stats);
                }
	}
	return 0;
}

void
farm_stats_setup(void)
{
	static char smart_command[] = "LC_ALL=C smartctl";

	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("FARM_SETUP")) != NULL)
		farm_setup_stats = env_command;
	else
		farm_setup_stats = smart_command;
}
