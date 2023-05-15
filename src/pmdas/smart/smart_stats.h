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

#ifndef SMART_STATS_H
#define SMART_STATS_H

enum {
	HEALTH = 0,
	MODEL_FAMILY,
	DEVICE_MODEL,
	SERIAL_NUMBER,
	CAPACITY_BYTES,
	SECTOR_SIZE,
	ROTATION_RATE,
	FIRMWARE_VERSION,
	NUM_INFO_METRICS
};

enum {
	RAW_READ_ERROR_RATE = 1,
	THROUGHPUT_PERFORMANCE = 2,
	SPIN_UP_TIME = 3,
	START_STOP_COUNT = 4,
	REALLOCATED_SECTOR_COUNT = 5,
	SEEK_ERROR_COUNT = 7,
	SEEK_TIME_PERFORMANCE = 8,
	POWER_ON_HOURS = 9,
	SPIN_RETRY_COUNT = 10,
	CALIBRATION_RETRY_COUNT = 11,
	POWER_CYCLE_COUNT = 12,
	READ_SOFT_ERROR_RATE = 13,
	CURRENT_HELIUM_LEVEL = 22,
	ERASE_FAIL_COUNT_CHIP = 176,
	WEAR_LEVELING_COUNT = 177,
	USED_RESERVED_BLOCK_COUNT_TOTAL = 179,
	UNUSED_RESERVED_BLOCK_COUNT_TOTAL = 180,
	PROGRAM_FAIL_COUNT_TOTAL = 181,
	ERASE_FAIL_COUNT_TOTAL = 182,
	RUNTIME_BAD_BLOCK = 183,
	END_TO_END_ERROR = 184,
	REPORTED_UNCORRECT = 187,
	COMMAND_TIMEOUT = 188,
	HIGH_FLY_WRITES = 189,
	AIRFLOW_TEMP_CELSIUS = 190,
	G_SENSE_ERROR_RATE = 191,
	POWER_OFF_RETRACT_COUNT = 192,
	LOAD_CYCLE_COUNT = 193,
	TEMPERATURE_CELSIUS = 194,
	HARDWARE_ECC_RECOVERED = 195,
	REALLOCATED_EVENT_COUNT = 196,
	CURRENT_PENDING_SECTOR = 197,
	OFFLINE_UNCORRECTABLE = 198,
	UDMA_CRC_ERROR_COUNT = 199,
	MULTI_ZONE_ERROR_RATE = 200,
	SOFT_READ_ERROR_RATE = 201,
	DISK_SHIFT = 220,
	LOADED_HOURS = 222,
	LOAD_RETRY_COUNT = 223,
	LOAD_FRICTION = 224,
	LOAD_CYCLE = 225,
	LOAD_IN_TIME = 226,
	HEAD_FLYING_HOURS = 240,
	TOTAL_LBAS_WRITTEN = 241,
	TOTAL_LBAS_READ = 242,
	READ_ERROR_RETRY_RATE = 250,
	FREE_FALL_SENSOR = 254,
	NUM_SMART_STATS = 256
};

enum {
	SMART_ID = 0,
	SMART_VALUE,
	SMART_WORST,
	SMART_THRESH,
	SMART_RAW,
	NUM_METRICS
};

enum {
	NVME_MODEL_NUMBER = 0,
	NVME_SERIAL_NUMBER = 1,
	NVME_FIRMWARE_VERSION = 2,
	NVME_PCI_VENDOR_SUBSYSTEM_ID = 3,
	NVME_IEE_OUI_IDENTIFIER = 4,
	NVME_TOTAL_NVM_CAPACITY = 5,
	NVME_UNALLOCATED_NVM_CAPACITY = 6,
	NVME_CONTROLLER_ID = 7,
	NVME_NVME_VERSION = 8,
	NVME_NAMESPACES = 9,
	NVME_FIRMWARE_UPDATES = 10,
	NVME_MAXIMUM_DATA_TRANSFER_SIZE = 11,
	NVME_WARNING_TEMP_THRESHOLD = 12,
	NVME_CRITICAL_TEMP_THRESHOLD = 13,
	NVME_ACTIVE_POWER_STATE = 14,
	NVME_APST_STATE = 15,
	NVME_COMPLETION_QUEUE_LENGTH_COMPLETION = 16,
	NVME_COMPLETION_QUEUE_LENGTH_SUBMISSION = 17,
	NVME_NAMESPACE_1_CAPACITY = 18,
	NVME_NAMESPACE_1_UTILIZATION = 19,
	NVME_NAMESPACE_1_FORMATTED_LBA_SIZE = 20,
	NVME_NAMESPACE_1_IEEE_EUI_64 = 21
};

enum {
	CRITICAL_WARNING = 0,
	COMPOSITE_TEMPERATURE,
	AVAILABLE_SPARE,
	AVAILABLE_SPARE_THRESHOLD,
	PERCENTAGE_USED,
	DATA_UNITS_READ,
	DATA_UNITS_WRITTEN,
	HOST_READ_COMMANDS,
	HOST_WRITES_COMMANDS,
	CONTROLLER_BUSY_TIME,
	POWER_CYCLES,
	NVME_POWER_ON_HOURS,
	UNSAFE_SHUTDOWNS,
	MEDIA_AND_DATA_INTEGRITY_ERRORS,
	NUMBER_OF_ERROR_INFORMATION_LOG_ENTRIES,
	WARNING_COMPOSITE_TEMPERATRE_TIME,
	CRITICAL_COMPOSITE_TEMPERATURE_TIME,
	TEMPERATURE_SENSOR_ONE,
	TEMPERATURE_SENSOR_TWO,
	TEMPERATURE_SENSOR_THREE,
	TEMPERATURE_SENSOR_FOUR,
	TEMPERATURE_SENSOR_FIVE,
	TEMPERATURE_SENSOR_SIX,
	TEMPERATURE_SENSOR_SEVEN,
	TEMPERATURE_SENSOR_EIGHT,
	THERMAL_MANAGEMENT_TEMPERATURE_ONE_TRANSITION_COUNT,
	THERMAL_MANAGEMENT_TEMPERATURE_TWO_TRANSITION_COUNT,
	TOTAL_TIME_FOR_THERMAL_MANAGEMENT_TEMPERATURE_ONE,
	TOTAL_TIME_FOR_THERMAL_MANAGEMENT_TEMPERATURE_TWO,
	NUM_NVME_STATS
};

enum {
	POWER_STATE_0 = 0,
	POWER_STATE_1,
	POWER_STATE_2,
	POWER_STATE_3,
	POWER_STATE_4,
	POWER_STATE_5,
	NUM_POWER_STATES
};

enum {
	STATE = 0,
	MAX_POWER,
	NON_OPERATIONAL_STATE,
	ACTIVE_POWER,
	IDLE_POWER,
	RELATIVE_READ_LATENCY,
	RELATIVE_READ_THROUGHPUT,
	RELATIVE_WRITE_LATENCY,
	RELATIVE_WRITE_THROUGHPUT,
	ENTRY_LATENCY,
	EXIT_LATENCY,
	NUM_POWER_STATS
};

struct device_info {
	char			health[9];
	char			model_family[41];
	char			device_model[41];
	char			serial_number[21];
	uint64_t		capacity_bytes;
	char			sector_size[64];
	char			rotation_rate[18];
	char			firmware_version[9];
};

struct nvme_device_info {
	char			model_number[41];
	char			serial_number[21];
	char			firmware_version[9];
	char			pci_vendor_subsystem_id[7];
	char			ieee_oui_identifier[9];
	uint64_t		total_nvm_capacity;
	uint64_t		unallocated_nvm_capacity;
	uint8_t			controller_id;
	char			nvme_version[4];
	uint8_t			namespaces;
	char			firmware_updates[64];
	uint32_t		maximum_data_transfer_size;
	uint8_t 		warning_temp_threshold;
	uint8_t			critical_temp_threshold;
	uint8_t			active_power_state;
	char			apst_state[9];
	uint32_t		completion_queue_length_completion;
	uint32_t		completion_queue_length_submission;
	uint64_t		namespace_1_capacity;
	uint64_t		namespace_1_utilization;
	uint32_t		namespace_1_formatted_lba_size;
	char			namespace_1_ieee_eui_64[64];
};

struct smart_data {
	uint8_t			id[NUM_SMART_STATS];
	uint8_t			type[NUM_SMART_STATS];
	uint8_t			value[NUM_SMART_STATS];
	uint8_t			worst[NUM_SMART_STATS];
	uint8_t 		thresh[NUM_SMART_STATS];
	uint32_t		raw[NUM_SMART_STATS];
};

struct nvme_smart_data{
	char			critical_warning[9];
	uint8_t			composite_temperature;
	uint8_t			available_spare;
	uint8_t			available_spare_threshold;
	uint8_t			percentage_used;
	uint64_t		data_units_read;
	uint64_t		data_units_written;
	uint64_t		host_read_commands;
	uint64_t		host_write_commands;
	uint32_t		controller_busy_time;
	uint32_t		power_cycles;
	uint32_t		power_on_hours;
	uint32_t		unsafe_shutdowns;
	uint32_t		media_and_data_integrity_errors;
	uint32_t		number_of_error_information_log_entries;
	uint32_t		warning_composite_temperature_time;
	uint32_t		critical_composite_temperature_time;
	uint8_t			temperature_sensor_one;
	uint8_t			temperature_sensor_two;
	uint8_t			temperature_sensor_three;
	uint8_t			temperature_sensor_four;
	uint8_t			temperature_sensor_five;
	uint8_t			temperature_sensor_six;
	uint8_t			temperature_sensor_seven;
	uint8_t			temperature_sensor_eight;
};

struct nvme_power_states {
	uint8_t			state[NUM_POWER_STATES];
	double			max_power[NUM_POWER_STATES];
	uint8_t			non_operational_state[NUM_POWER_STATES];
	double			active_power[NUM_POWER_STATES];
	double			idle_power[NUM_POWER_STATES];
	uint32_t		relative_read_latency[NUM_POWER_STATES];
	uint32_t		relative_read_throughput[NUM_POWER_STATES];
	uint32_t		relative_write_latency[NUM_POWER_STATES];
	uint32_t		relative_write_throughput[NUM_POWER_STATES];
	uint32_t		entry_latency[NUM_POWER_STATES];
	uint32_t		exit_latency[NUM_POWER_STATES];
};

extern int smart_device_info_fetch(int, struct device_info *, pmAtomValue *);
extern int smart_refresh_device_info(const char *, struct device_info *, int);

extern int smart_data_fetch(int, int, struct smart_data *, pmAtomValue *);
extern int smart_refresh_data(const char *, struct smart_data *, int);

extern int nvme_device_info_fetch(int, int, struct nvme_device_info *, pmAtomValue *, int);
extern int nvme_device_refresh_data(const char *, struct nvme_device_info *, int);

extern int nvme_smart_data_fetch(int, int, struct nvme_smart_data *, pmAtomValue *, int);
extern int nvme_smart_refresh_data(const char *, struct nvme_smart_data *, int);

extern int nvme_power_data_fetch(int, int, struct nvme_power_states *, pmAtomValue *, int);
extern int nvme_power_refesh_data(const char *, struct nvme_power_states *, int);

extern void smart_stats_setup(void);

#endif /* SMART_STATS_H */
