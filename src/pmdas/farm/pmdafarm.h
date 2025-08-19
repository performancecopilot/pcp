/*
 * Seagate Field Accessible Reliability Metrics (FARM) Log
 *
 * Copyright (c) 2023 - 2025 Red Hat.
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

#ifndef PMDAFARM_H
#define PMDAFARM_H

#include "farm_stats.h"

extern pmInDom farm_indom(int);
#define INDOM(i) farm_indom(i)

enum {
	CLUSTER_ATA_LOG_HEADER = 0,
	CLUSTER_ATA_DRIVE_INFORMATION = 1,
	CLUSTER_ATA_WORKLOAD_STATISTICS = 2,
	CLUSTER_ATA_ERROR_STATISTICS = 3,
	CLUSTER_ATA_ENVIRONMENTAL_STATISTICS = 4,
	CLUSTER_ATA_RELIABILITY_STATISTICS = 5,
	CLUSTER_ATA_LED_FLASH_EVENTS = 6,
	CLUSTER_ATA_PER_HEAD_STATS = 7,
	CLUSTER_SCSI_LOG_HEADER = 10,
	CLUSTER_SCSI_DRIVE_INFORMATION = 11,
	CLUSTER_SCSI_WORKLOAD_STATISTICS = 12,
	CLUSTER_SCSI_ERROR_STATISTICS = 13,
	CLUSTER_SCSI_ENVIRONMENTAL_STATISTICS = 14,
	CLUSTER_SCSI_RELIABILITY_STATISTICS = 15,
	CLUSTER_SCSI_DRIVE_INFORMATION_CONTINUED = 16,
	CLUSTER_SCSI_ENVIRONMENTAL_INFORMATION_CONTINUED = 17,
	CLUSTER_SCSI_PER_HEAD_STATS = 18,
	NUM_CLUSTERS
};

enum {
	DISK_INDOM = 0,
	FLASH_LED_INDOM = 1,
	PER_HEAD_INDOM = 2,
	SCSI_DISK_INDOM = 3,
	SCSI_PER_HEAD_INDOM = 4,
	NUM_INDOMS
};

struct seagate_disk {
	struct farm_ata_log_stats	farm_ata_log_stats;
	struct farm_scsi_log_stats	farm_scsi_log_stats;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif /* PMDAFARM_H */
