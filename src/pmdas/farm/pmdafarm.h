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

#ifndef PMDAFARM_H
#define PMDAFARM_H

#include "farm_stats.h"

extern pmInDom farm_indom(int);
#define INDOM(i) farm_indom(i)

enum {
	CLUSTER_ATA_LOG_HEADER = 0,
	CLUSTER_ATA_DRIVE_INFORMATION,
	CLUSTER_ATA_WORKLOAD_STATISTICS,
	CLUSTER_ATA_ERROR_STATISTICS,
	CLUSTER_ATA_ENVIRONMENTAL_STATISTICS,
	CLUSTER_ATA_RELIABILITY_STATISTICS,
	CLUSTER_ATA_LED_FLASH_EVENTS,
	CLUSTER_ATA_PER_HEAD_STATS,
	NUM_CLUSTERS
};

enum {
	DISK_INDOM = 0,
	FLASH_LED_INDOM = 1,
	PER_HEAD_INDOM = 2,
	NUM_INDOMS
};

struct seagate_disk {
	struct farm_ata_log_stats	farm_ata_log_stats;
};

extern pmdaMetric metrictable[];
extern int metrictable_size();

#endif /* PMDAFARM_H */
