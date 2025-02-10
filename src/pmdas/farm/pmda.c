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
 
 #include "pmapi.h"
 #include "pmda.h"
 #include "domain.h"
 
 #include "pmdafarm.h"
 
 static int _isDSO = 1; /* for local contexts */
 static char *farm_setup_lsblk;
 static char *farm_setup_smartctl;
 
 pmdaIndom indomtable[] = {
 	{ .it_indom = DISK_INDOM },
 	{ .it_indom = FLASH_LED_INDOM },
 	{ .it_indom = PER_HEAD_INDOM },
 };
 
 /*
 * All metrics supported by this PMDA - one table entry for each metric
 */
pmdaMetric metrictable[] = {
	/* ATA - PAGE 0: FARM log header */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, LOG_VERSION),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, PAGES_SUPPORTED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, LOG_SIZE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, PAGE_SIZE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, HEADS_SUPPORTED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, NUMBER_OF_COPIES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_LOG_HEADER, REASON_FOR_FRAME_CAPTURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	/* ATA - PAGE 1: Drive Information */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, SERIAL_NUMBER),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, WORLD_WIDE_NAME),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DEVICE_INTERFACE),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DEVICE_CAPACITY_IN_SECTORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, PHYSICAL_SECTOR_SIZE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, LOGICAL_SECTOR_SIZE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DEVICE_BUFFER_SIZE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, NUMBER_OF_HEADS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DEVICE_FORM_FACTOR),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, ROTATION_RATE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, FIRMWARE_REV),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, ATA_SECURITY_STATE),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, ATA_FEATURES_SUPPORTED),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, ATA_FEATURES_ENABLED),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, POWER_ON_HOURS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,-1,0,PM_TIME_HOUR,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, SPINDLE_POWER_ON_HOURS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,-1,0,PM_TIME_HOUR,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, HEAD_FLIGHT_HOURS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,-1,0,PM_TIME_HOUR,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, HEAD_LOAD_EVENTS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, POWER_CYCLE_COUNT),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, HARDWARE_RESET_COUNT),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, SPIN_UP_TIME),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,PM_TIME_MSEC,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, TIME_TO_READY_LAST_POWER_CYCLE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,PM_TIME_MSEC,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, TIME_DRIVE_HELD_IN_STAGGERED_SPIN),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,PM_TIME_MSEC,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, MODEL_NUMBER),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DRIVE_RECORDING_TYPE),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, MAX_NUMBER_AVAILABLE_SECTORS_REASSIGNMENT),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, ASSEMBLY_DATE),
		PM_TYPE_STRING, DISK_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_DRIVE_INFORMATION, DEPOPULATION_HEAD_MASK),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	/* ATA - PAGE 2: Workload Statistics */
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, TOTAL_READ_COMMANDS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, TOTAL_WRITE_COMMANDS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, TOTAL_RANDOM_READ_COMMANDS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, TOTAL_RANDOM_WRITE_COMMANDS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
 		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, TOTAL_OTHER_COMMANDS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
  		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, LOGICAL_SECTORS_WRITTEN),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
   		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, LOGICAL_SECTORS_READ),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, DITHER_EVENTS_CURRENT_POWER_CYCLE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, DITHER_HELD_OFF_RANDOM_WORKLOADS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, DITHER_HELD_OFF_SEQUENTIAL_WORKLOADS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, READ_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, READ_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, READ_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, READ_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, WRITE_COMMANDS_0_3_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, WRITE_COMMANDS_3_25_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, WRITE_COMMANDS_25_75_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_WORKLOAD_STATISTICS, WRITE_COMMANDS_75_100_LBA_SPACE_LAST_3_SMART_SUMMARY_FRAMES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	/* ATA - PAGE 3: Error Statistics */
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, UNRECOVERABLE_READ_ERRORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, UNRECOVERABLE_WRITE_ERRORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, REALLOCATED_SECTORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, READ_RECOVERY_ATTEMPTS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, MECHANICAL_START_FAILURES),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },	
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, REALLOCATED_CANDIDATE_SECTORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, ASR_EVENTS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, INTERFACE_CRC_ERRORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, SPIN_RETRY_COUNT),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, SPIN_RETRY_COUNT_NORMALIZED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, SPIN_RETRY_COUNT_WORST),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, IOEDC_ERRORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, CTO_COUNT_TOTAL),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, CTO_COUNT_OVER_5S),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, CTO_COUNT_OVER_7S),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, TOTAL_FLASH_LED_ASSERT_EVENTS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, INDEX_OF_LAST_FLASH_LED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_LED_FLASH_EVENTS, FLASH_LED_EVENT_INFORMATION),
		PM_TYPE_U64, FLASH_LED_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_LED_FLASH_EVENTS, FLASH_LED_EVENT_TIMESTAMP),
		PM_TYPE_U64, FLASH_LED_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,PM_TIME_HOUR,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_LED_FLASH_EVENTS, FLASH_LED_EVENT_POWER_CYCLE),
		PM_TYPE_U64, FLASH_LED_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, UNCORRECTABLE_ERRORS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ERROR_STATISTICS, CUMULATIVE_LIFETIME_UNRECOVERABLE_ERRORS_DUE_TO_ERC),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_REPEATING),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, CUMULATIVE_LIFETIME_UNRECOVERABLE_READ_UNIQUE),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	/* ATA - PAGE 4: Environment Statistics */
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, CURRENT_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, HIGHEST_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, LOWEST_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, AVERAGE_SHORT_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, AVERAGE_LONG_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, HIGHEST_AVERAGE_SHORT_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, LOWEST_AVERAGE_SHORT_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, HIGHEST_AVERAGE_LONG_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, LOWEST_AVERAGE_LONG_TERM_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, TIME_IN_OVER_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, TIME_IN_UNDER_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, SPECIFIED_MAX_OPERATING_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, SPECIFIED_MIN_OPERATING_TEMPERATURE),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, CURRENT_RELATIVE_HUMIDITY),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, CURRENT_MOTOR_POWER),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, CURRENT_12_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, MINIMUM_12_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, MAXIMUM_12_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, CURRENT_5_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, MINIMUM_5_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, MAXIMUM_5_VOLTS),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _12V_POWER_AVERAGE),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _12V_POWER_MINIMUM),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _12V_POWER_MAXIMUM),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _5V_POWER_AVERAGE),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _5V_POWER_MINIMUM),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_ENVIRONMENTAL_STATISTICS, _5V_POWER_MAXIMUM),
		PM_TYPE_DOUBLE, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	/* ATA - PAGE 5: Reliability Statistics */
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, ERROR_RATE_SMART_1_RAW),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, ERROR_RATE_SMART_1_NORMALIZED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, ERROR_RATE_SMART_1_WORST),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, SEEK_ERROR_RATE_SMART_7_RAW),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, SEEK_ERROR_RATE_SMART_7_NORMALIZED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, SEEK_ERROR_RATE_SMART_7_WORST),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, HIGH_PRIORITY_UNLOAD_EVENTS),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, HELIUM_PRESSURE_THRESHOLD_TRIPPED),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_RELIABILITY_STATISTICS, LBAS_CORRECTED_BY_PARITY_SECTOR),
		PM_TYPE_U64, DISK_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, DVGA_SKIP_WRITE_DETECTED),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, RVGA_SKIP_WRITE_DETECTED),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, FVGA_SKIP_WRITE_DETECTED),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, SKIP_WRITE_DETECT_THRESHOLD_DETECT),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, WRITE_POWER_HRS),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,PM_TIME_HOUR,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, MR_HEAD_RESISTANCE),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, SECOND_MR_HEAD_RESISTANCE),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, NUMBER_REALLOCATED_SECTORS),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
	{ .m_desc = {
    		PMDA_PMID(CLUSTER_ATA_PER_HEAD_STATS, NUMBER_REALLOCATION_CANDIDATE_SECTORS),
		PM_TYPE_U64, PER_HEAD_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,0,0,0) }, },
};

pmInDom
farm_indom(int serial)
{
	return indomtable[serial].it_indom;
}

int
metrictable_size(void)
{
	return sizeof(metrictable)/sizeof(metrictable[0]);
}

static int
farm_instance_refresh(void)
{
	int sts;
	char buffer[4096], buffer2[4096], dev_name[128];
	FILE *pf, *pf2;
	pmInDom indom = INDOM(DISK_INDOM);

	/*
	 * update indom cache based off number of disks reported by "lsblk",
	 * smartctl requires us to know the block device id/path for each of
	 * our disks in order to be able to get our stats, we get this info
	 * using "lsblk" and store the name of each device. We additionally
	 * check whether the drive supports farm log output before adding as
	 * an instance.
	 */

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if ((pf = popen(farm_setup_lsblk, "r")) == NULL)
		return -oserror();

	while (fgets(buffer, sizeof(buffer)-1, pf)) {	
		sscanf(buffer, "%s", dev_name);
		buffer[sizeof(dev_name)-1] = '\0';

		/* at this point dev_name contains our device name this will be used to
		 *  map stats to disk drive instances, although lets check for FARM
		 *  support
		 */
		pmsprintf(buffer2, sizeof(buffer2), "%s -l farm /dev/%s", farm_setup_smartctl, dev_name);
		buffer2[sizeof(buffer2)-1] = '\0';

		if ((pf2 = popen(buffer2, "r")) == NULL) {
		        pclose(pf);
			return -oserror();
		}

		while (fgets(buffer2, sizeof(buffer2)-1, pf2) != NULL) {
			if (strstr(buffer2, "(FARM)")) {
			        struct seagate_disk *dev;

		                sts = pmdaCacheLookupName(indom, dev_name, NULL, (void **)&dev);
		                if (sts == PM_ERR_INST || (sts >=0 && dev == NULL)) {
			                dev = calloc(1, sizeof(struct seagate_disk));
			                if (dev == NULL) {
				                pclose(pf);
				                pclose(pf2);
				                return PM_ERR_AGAIN;
			                }
		                }
		                else if (sts < 0)
			                continue;

		                pmdaCacheStore(indom, PMDA_CACHE_ADD, dev_name, (void *)dev);
		                
		                break; //Save reading the entire FARM Log at this point.
			}
		}

		pclose(pf2);
	}

	pclose(pf);
	return(0);	
}

static int
farm_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
	farm_instance_refresh();
	return pmdaInstance(indom, inst, name, result, pmda);
}

static int
farm_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
	pmInDom indom = INDOM(DISK_INDOM);
	struct seagate_disk *dev;
	char *dev_name;
	int i, sts;

	if ((sts = farm_instance_refresh()) < 0)
		return sts;

	for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
		if ((i= pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
			break;
		if (!pmdaCacheLookup(indom, i, &dev_name, (void **)&dev) || !dev)
			continue;

		if (need_refresh[CLUSTER_ATA_LOG_HEADER] ||
			need_refresh[CLUSTER_ATA_DRIVE_INFORMATION] ||
			need_refresh[CLUSTER_ATA_WORKLOAD_STATISTICS] ||
			need_refresh[CLUSTER_ATA_ERROR_STATISTICS] ||
			need_refresh[CLUSTER_ATA_ENVIRONMENTAL_STATISTICS] ||
			need_refresh[CLUSTER_ATA_RELIABILITY_STATISTICS])
			farm_ata_refresh_data(dev_name, &dev->farm_ata_log_stats);
	}

        if (( i = pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE)) > 0) {
	        if (need_refresh[CLUSTER_ATA_LED_FLASH_EVENTS])
	                farm_ata_refresh_led_events();
	        
	        if (need_refresh[CLUSTER_ATA_PER_HEAD_STATS])
	                farm_ata_refresh_per_head_stats();
	}
	
	return sts;
}

static int
farm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
	int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

	for (i = 0; i < numpmid; i++) {
		unsigned int	cluster = pmID_cluster(pmidlist[i]);
		if (cluster < NUM_CLUSTERS)
			need_refresh[cluster]++;
	}

	if ((sts = farm_fetch_refresh(pmda, need_refresh)) < 0)
		return sts;

	return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
farm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
	unsigned int 		item = pmID_item(mdesc->m_desc.pmid);
	unsigned int		cluster = pmID_cluster(mdesc->m_desc.pmid);
	struct seagate_disk 	*dev;
	int 			sts;

	switch (cluster) {
		case CLUSTER_ATA_LOG_HEADER:
		case CLUSTER_ATA_DRIVE_INFORMATION:
		case CLUSTER_ATA_WORKLOAD_STATISTICS:
		case CLUSTER_ATA_ERROR_STATISTICS:
		case CLUSTER_ATA_ENVIRONMENTAL_STATISTICS:
		case CLUSTER_ATA_RELIABILITY_STATISTICS:
			sts = pmdaCacheLookup(INDOM(DISK_INDOM), inst, NULL, (void **)&dev);
			if (sts < 0)
				return sts;
			return farm_ata_data_fetch(item, cluster, &dev->farm_ata_log_stats, atom);

		case CLUSTER_ATA_LED_FLASH_EVENTS:
		        return farm_ata_flash_led_events_fetch(item, inst, atom);

                case CLUSTER_ATA_PER_HEAD_STATS:
                        return farm_ata_per_head_stats_fetch(item, inst, atom);

		default:
			return PM_ERR_PMID;
	}

	return PMDA_FETCH_STATIC;
}

static int
farm_labelInDom(pmID pmid, pmLabelSet **lp)
{
        unsigned int cluster = pmID_cluster(pmid);
        
        switch (cluster) {
                case CLUSTER_ATA_LED_FLASH_EVENTS:
                        pmdaAddLabels(lp, "{\"device_type\":[\"disk\",\"led_flash_event\"]}");
                        pmdaAddLabels(lp, "{\"indom_name\":\"per disk, per led_flash_event\"}");
                        return 1;
                
                case CLUSTER_ATA_PER_HEAD_STATS:
                        pmdaAddLabels(lp, "{\"device_type\":[\"disk\",\"disk_head\"]}");
                        pmdaAddLabels(lp, "{\"indom_name\":\"per disk, per disk_head\"}");
                        return 1;

                default:
                        break;
        }
        return 0;
}

static int
farm_label(int ident, int type, pmLabelSet **lp, pmdaExt *pmda)
{
        int sts;
        
        switch (type) {
                case PM_LABEL_ITEM:
                        if ((sts = farm_labelInDom((pmID)ident, lp)) <0)
                                return sts;
                        break;

                default:
                        break;
        }
        return pmdaLabel(ident, type, lp, pmda);
}

static int
farm_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
        struct farm_flash_led_events *flash_led_events;
        struct farm_per_head_stats *per_head_stats;
        
        int sts;
        char *name, *disk_name;

        if (indom == PM_INDOM_NULL)
                return 0;

        switch (pmInDom_serial(indom)) {
                case FLASH_LED_INDOM:
                        sts = pmdaCacheLookup(INDOM(FLASH_LED_INDOM), inst, &name, (void **)&flash_led_events);
                        if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
                                return 0;
                                
                        disk_name = strsep(&name, ":");
                        return pmdaAddLabels(lp, "{\"disk\":\"%s\", \"led_flash_event\":\"event_%u\"}",
                                disk_name,
                                flash_led_events->event_id
                        );
                        
                case PER_HEAD_INDOM:
                        sts = pmdaCacheLookup(INDOM(PER_HEAD_INDOM), inst, &name, (void **)&per_head_stats);
                        if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
                                return 0;
                        
                        disk_name = strsep(&name, ":");
                        return pmdaAddLabels(lp, "{\"disk\":\"%s\", \"disk_head\":\"head_%u\"}",
                                disk_name,
                                per_head_stats->head_id
                        );

                default:
                        break;
        }
        return 0;
}

void
farm_instance_setup(void)
{
	static char lsblk_command[] = "lsblk -d -n -e 1,2,7,11,252 -o name";
	static char smart_command[] = "LC_ALL=C smartctl";
	char *env_command;

	/* allow override at startup for QA testing */
	if ((env_command = getenv("FARM_SETUP_LSBLK")) != NULL)
		farm_setup_lsblk = env_command;
	else
		farm_setup_lsblk = lsblk_command;
		
	/* allow override at startup for QA testing */
	if ((env_command = getenv("FARM_SETUP_SMARTCTL")) != NULL)
		farm_setup_smartctl = env_command;
	else
		farm_setup_smartctl = smart_command;

}

void
__PMDA_INIT_CALL
farm_init(pmdaInterface *dp)
{
	int nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
	int nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

	if (_isDSO) {
		char helppath[MAXPATHLEN];
		int sep = pmPathSeparator();
		pmsprintf(helppath, sizeof(helppath), "%s%c" "farm" "%c" "help",
			pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
		pmdaDSO(dp, PMDA_INTERFACE_7, "FARM DSO", helppath);
	}

	if (dp->status != 0)
		return;

	/* Check for environment variables allowing test injection */
	farm_instance_setup();
	farm_stats_setup();

	dp->version.seven.instance = farm_instance;
	dp->version.seven.fetch = farm_fetch;
	dp->version.seven.label = farm_label;
	pmdaSetLabelCallBack(dp, farm_labelCallBack);
	pmdaSetFetchCallBack(dp, farm_fetchCallBack);

	pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
	pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
}

static pmLongOptions longopts[] = {
	PMDA_OPTIONS_HEADER("Options"),
	PMOPT_DEBUG,
	PMDAOPT_DOMAIN,
	PMDAOPT_LOGFILE,
	PMOPT_HELP,
	PMDA_OPTIONS_END
};

static pmdaOptions opts = {
	.short_options = "D:d:l:U:?",
	.long_options = longopts,
};

int
main(int argc, char **argv)
{
	int sep = pmPathSeparator();
	char helppath[MAXPATHLEN];
	pmdaInterface dispatch;

	_isDSO = 0;
	pmSetProgname(argv[0]);
	pmsprintf(helppath, sizeof(helppath), "%s%c" "farm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), FARM, "farm.log", helppath);

	pmdaGetOptions(argc, argv, &opts, &dispatch);
	if (opts.errors) {
		pmdaUsageMessage(&opts);
		exit(1);
	}

	pmdaOpenLog(&dispatch);

	farm_init(&dispatch);
	pmdaConnect(&dispatch);
	pmdaMain(&dispatch);
	exit(0);
}
