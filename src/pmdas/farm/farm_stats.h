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
 
#ifndef FARM_STATS_H
#define FARM_STATS_H

#define MAX_NUMBER_OF_SUPPORTED_HEADS 24
#define MAX_NUMBER_OF_LED_EVENTS 8
 
enum {
	LOG_VERSION = 0,
	PAGES_SUPPORTED,
	LOG_SIZE,
	PAGE_SIZE,
	HEADS_SUPPORTED,
	NUMBER_OF_COPIES,
	REASON_FOR_FRAME_CAPTURE,
	NUM_LOG_HEADER_STATS
};

enum {
	SERIAL_NUMBER = 0,
	WORLD_WIDE_NAME,
	DEVICE_INTERFACE,
	DEVICE_CAPACITY_IN_SECTORS,
	PHYSICAL_SECTOR_SIZE,
	LOGICAL_SECTOR_SIZE,
	DEVICE_BUFFER_SIZE,
	NUMBER_OF_HEADS,
	DEVICE_FORM_FACTOR,
	ROTATION_RATE,
	FIRMWARE_REV,
	ATA_SECURITY_STATE,
	ATA_FEATURES_SUPPORTED,
	ATA_FEATURES_ENABLED,
	POWER_ON_HOURS,
	SPINDLE_POWER_ON_HOURS,
	HEAD_FLIGHT_HOURS,
	HEAD_LOAD_EVENTS,
	POWER_CYCLE_COUNT,
	HARDWARE_RESET_COUNT,
	SPIN_UP_TIME,
	TIME_TO_READY_LAST_POWER_CYCLE,
	TIME_DRIVE_HELD_IN_STAGGERED_SPIN,
	MODEL_NUMBER,
	DRIVE_RECORDING_TYPE,
	MAX_NUMBER_AVAILABLE_SECTORS_REASSIGNMENT,
	ASSEMBLY_DATE,
	DEPOPULATION_HEAD_MASK,
	NUM_DRIVE_INFORMATION_STATS
};

enum {
	TOTAL_READ_COMMANDS = 0,
	TOTAL_WRITE_COMMANDS,
	TOTAL_RANDOM_READ_COMMANDS,
	TOTAL_RANDOM_WRITE_COMMANDS,
	TOTAL_OTHER_COMMANDS,
	LOGICAL_SECTORS_WRITTEN,
	LOGICAL_SECTORS_READ,
	DITHER_EVENTS_CURRENT_POWER_CYCLE,
	DITHER_HELD_OFF_RANDOM_WORKLOADS,
	DITHER_HELD_OFF_SEQUENTIAL_WORKLOADS,
	READ_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	READ_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	READ_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	READ_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	WRITE_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	WRITE_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	WRITE_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	WRITE_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES,
	NUM_WORKLOAD_STATS
};
 
enum {
	UNRECOVERABLE_READ_ERRORS = 0,
	UNRECOVERABLE_WRITE_ERRORS,
	REALLOCATED_SECTORS,
	READ_RECOVERY_ATTEMPTS,
	MECHANICAL_START_FAILURES,
	REALLOCATED_CANDIDATE_SECTORS,
	ASR_EVENTS,
	INTERFACE_CRC_ERRORS,
	SPIN_RETRY_COUNT,
	SPIN_RETRY_COUNT_NORMALIZED,
	SPIN_RETRY_COUNT_WORST,
	IOEDC_ERRORS,
	CTO_COUNT_TOTAL,
	CTO_COUNT_OVER_5S,
	CTO_COUNT_OVER_7S,
	TOTAL_FLASH_LED_ASSERT_EVENTS,
	INDEX_OF_LAST_FLASH_LED,
	UNCORRECTABLE_ERRORS,
	CUMULATIVE_LIFETIME_UNRECOVERABLE_ERRORS_DUE_TO_ERC,
	NUM_ERROR_STATS
};

enum {
	CURRENT_TEMPERATURE = 0,
	HIGHEST_TEMPERATURE,
	LOWEST_TEMPERATURE,
	AVERAGE_SHORT_TERM_TEMPERATURE,
	AVERAGE_LONG_TERM_TEMPERATURE,
	HIGHEST_AVERAGE_SHORT_TERM_TEMPERATURE,
	LOWEST_AVERAGE_SHORT_TERM_TEMPERATURE,
	HIGHEST_AVERAGE_LONG_TERM_TEMPERATURE,
	LOWEST_AVERAGE_LONG_TERM_TEMPERATURE,
	TIME_IN_OVER_TEMPERATURE,
	TIME_IN_UNDER_TEMPERATURE,
	SPECIFIED_MAX_OPERATING_TEMPERATURE,
	SPECIFIED_MIN_OPERATING_TEMPERATURE,
	CURRENT_RELATIVE_HUMIDITY,
	CURRENT_MOTOR_POWER,
	CURRENT_12_VOLTS,
	MINIMUM_12_VOLTS,
	MAXIMUM_12_VOLTS,
	CURRENT_5_VOLTS,
	MINIMUM_5_VOLTS,
	MAXIMUM_5_VOLTS,
	_12V_POWER_AVERAGE,
	_12V_POWER_MINIMUM,
	_12V_POWER_MAXIMUM,
	_5V_POWER_AVERAGE,
	_5V_POWER_MINIMUM,
	_5V_POWER_MAXIMUM,
	NUM_ENVIRONMENTAL_STATS
};

enum {
	ERROR_RATE_SMART_1_RAW = 0,
	ERROR_RATE_SMART_1_NORMALIZED,
	ERROR_RATE_SMART_1_WORST,
	SEEK_ERROR_RATE_SMART_7_RAW,
	SEEK_ERROR_RATE_SMART_7_NORMALIZED,
	SEEK_ERROR_RATE_SMART_7_WORST,
	HIGH_PRIORITY_UNLOAD_EVENTS,
	HELIUM_PRESSURE_THRESHOLD_TRIPPED,
	LBAS_CORRECTED_BY_PARITY_SECTOR,
	NUM_RELIABILITY_STATS
};
 
enum {
	FLASH_LED_EVENT_INFORMATION = 0,
	FLASH_LED_EVENT_TIMESTAMP,
	FLASH_LED_EVENT_POWER_CYCLE,
	NUM_FLASH_LED_STATS
};

enum {
	CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_REPEATING = 0,
	CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_UNIQUE,
	DVGA_SKIP_WRITE_DETECTED,
	RVGA_SKIP_WRITE_DETECTED,
	FVGA_SKIP_WRITE_DETECTED,
	SKIP_WRITE_DETECT_THRESHOLD_DETECT,
	WRITE_POWER_SECS,
	MR_HEAD_RESISTANCE,
	SECOND_MR_HEAD_RESISTANCE,
	NUMBER_REALLOCATED_SECTORS,
	NUMBER_REALLOCATION_CANDIDATE_SECTORS,
	NUM_PER_HEAD_STATS
};

struct farm_ata_log_stats {
	/* ATA - PAGE 0: FARM log header */
	char		log_version[9];
	uint64_t	pages_supported;
	uint64_t	log_size;
	uint64_t	page_size;
	uint64_t	heads_supported;
	uint64_t	number_of_copies;
	uint64_t	reason_for_frame_capture;
	/* ATA - PAGE 1: Drive Information */
	char		serial_number[9];
	char		world_wide_name[19];
	char		device_interface[8];
	uint64_t	device_capacity_in_sectors;
	uint64_t	physical_sector_size;
	uint64_t	logical_sector_size;
	uint64_t	device_buffer_size;
	uint64_t	number_of_heads;
	char		device_form_factor[13];
	uint64_t	rotational_rate;
	char		firmware_rev[9];
	char		ata_security_state[19];
	char		ata_features_supported[19];
	char		ata_features_enabled[19];
	uint64_t	power_on_hours;
	uint64_t	spindle_power_on_hours;
	uint64_t	head_flight_hours;
	uint64_t	head_load_events;
	uint64_t	power_cycle_count;
	uint64_t	hardware_reset_count;
	uint64_t	spin_up_time;
	uint64_t	time_to_ready_last_power_cycle;
	uint64_t	time_drive_held_in_staggered_spin;
	char		model_number[13];
	char		drive_recording_type[8];
	uint64_t	max_number_available_sectors_reassignment;
	char		assembly_date[5];
	uint64_t	depopulation_head_mask;
	/* ATA - PAGE 2: Workload Statistics */
	uint64_t	total_read_commands;
	uint64_t	total_write_commands;
	uint64_t	total_random_read_commands;
	uint64_t	total_random_write_commands;
	uint64_t	total_other_commands;
	uint64_t	logical_sectors_written;
	uint64_t	logical_sectors_read;
	uint64_t	dither_events_current_power_cycle;
	uint64_t	dither_held_off_random_workloads;
	uint64_t	dither_held_off_sequential_workloads;
	uint64_t	read_commands_0_3_lba_space_last_3_smart_summary_frames;
	uint64_t	read_commands_3_25_lba_space_last_3_smart_summary_frames;
	uint64_t	read_commands_25_75_lba_space_last_3_smart_summary_frames;
	uint64_t	read_commands_75_100_lba_space_last_3_smart_summary_frames;
	uint64_t	write_commands_0_3_lba_space_last_3_smart_summary_frames;
	uint64_t	write_commands_3_25_lba_space_last_3_smart_summary_frames;
	uint64_t	write_commands_25_75_lba_space_last_3_smart_summary_frames;
	uint64_t	write_commands_75_100_lba_space_last_3_smart_summary_frames;
	/* ATA - PAGE 3: Error Statistics */
	uint64_t	unrecoverable_read_errors;
	uint64_t	unrecoverable_write_errors;
	uint64_t	reallocated_sectors;
	uint64_t	read_recovery_attempts;
	uint64_t	mechanical_start_failures;
	uint64_t	reallocated_candidate_sectors;
	uint64_t	asr_events;
	uint64_t	interface_crc_errors;
	uint64_t	spin_retry_count;
	uint64_t	spin_retry_count_normalized;
	uint64_t	spin_retry_count_worst;
	uint64_t	ioedc_errors;
	uint64_t	cto_count_total;
	uint64_t	cto_count_over_5s;
	uint64_t	cto_count_over_7s;
	uint64_t	total_flash_led_assert_events;
	uint64_t	index_of_last_flash_led;
	uint64_t	led_event_information[MAX_NUMBER_OF_LED_EVENTS];
	uint64_t	led_event_timestamp_of_event[MAX_NUMBER_OF_LED_EVENTS];
	uint64_t	led_event_power_cycle_event[MAX_NUMBER_OF_LED_EVENTS];
	uint64_t	uncorrectable_errors;
	uint64_t	cumulative_lifetime_unrecoverable_errors_due_to_erc;
	uint64_t	head_cumulative_lifetime_unrecoverable_read_repeating[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	head_cumulative_lifetime_unrecoverable_read_unique[MAX_NUMBER_OF_SUPPORTED_HEADS];
	/* ATA - PAGE 4: Environment Statistics */
 	uint64_t	current_temperature;
	uint64_t	highest_temperature;
	uint64_t	lowest_temperature;
	uint64_t	average_short_term_temperature;
	uint64_t	average_long_term_temperature;
	uint64_t	highest_average_short_term_temperature;
	uint64_t	lowest_average_short_term_temperature;
	uint64_t	highest_average_long_term_temperature;
	uint64_t	lowest_average_long_term_temperature;
	uint64_t	time_in_over_temperature;
	uint64_t	time_in_under_temperature;
	uint64_t	specified_max_operating_temperature;
	uint64_t	specified_min_operating_temperature;
	uint64_t	current_relative_humidity;
	uint64_t	current_motor_power;
	double		current_12_volts;
	double		minimum_12_volts;
	double		maximum_12_volts;
	double		current_5_volts;
	double		minimum_5_volts;
	double		maximum_5_volts;
	double		_12v_power_average;
	double		_12v_power_minimum;
	double		_12v_power_maximum;
	double		_5v_power_average;
	double		_5v_power_minimum;
	double		_5v_power_maximum;
	/* ATA - PAGE 5: Reliability Statistics */
	uint64_t	error_rate_smart_1_raw;
	uint64_t	error_rate_smart_1_normalized;
	uint64_t	error_rate_smart_1_worst;
	uint64_t	seek_error_rate_smart_7_raw;
	uint64_t	seek_error_rate_smart_7_normalized;
	uint64_t	seek_error_rate_smart_7_worst;
	uint64_t	high_priority_unload_events;
	uint64_t	helium_pressure_threshold_tripped;
	uint64_t	lbas_corrected_by_parity_sector;
	uint64_t	dvga_skip_write_detected_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	rvga_skip_write_detected_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	fvga_skip_write_detected_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	skip_write_detect_threshold_detect_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	write_power_secs_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	mr_head_resistance_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	second_mr_head_resistance_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	number_reallocated_sectors_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
	uint64_t	number_reallocation_candidate_sectors_head[MAX_NUMBER_OF_SUPPORTED_HEADS];
};
 
struct farm_flash_led_events {
	uint8_t 	event_id;
	/* ATA - PAGE 3: Error Statistics */
	uint64_t	led_event_information;
	uint64_t	led_event_timestamp;
	uint64_t	led_event_power_cycle;
};

struct farm_per_head_stats {
	uint8_t	head_id;
	/* ATA - PAGE 3: Error Statistics */
	uint64_t	head_cumulative_lifetime_unrecoverable_read_repeating;
	uint64_t	head_cumulative_lifetime_unrecoverable_read_unique;
      	/* ATA - PAGE 5: Reliability Statistics */
	uint64_t	dvga_skip_write_detected_head;
	uint64_t	rvga_skip_write_detected_head;
	uint64_t	fvga_skip_write_detected_head;
	uint64_t	skip_write_detect_threshold_detect_head;
	uint64_t	write_power_secs_head;
	uint64_t	mr_head_resistance_head;
	uint64_t	second_mr_head_resistance_head;
	uint64_t	number_reallocated_sectors_head;
	uint64_t	number_reallocation_candidate_sectors_head;
};
 
extern int farm_ata_data_fetch(int, int, struct farm_ata_log_stats *, pmAtomValue *);
extern int farm_ata_refresh_data(const char *, struct farm_ata_log_stats *);

extern int farm_ata_flash_led_events_fetch(int, unsigned int, pmAtomValue *);
extern int farm_ata_refresh_led_events(void);

extern int farm_ata_per_head_stats_fetch(int, unsigned int, pmAtomValue *);
extern int farm_ata_refresh_per_head_stats (void);

extern void farm_stats_setup(void);

#endif /* FARM_STATS_H */
