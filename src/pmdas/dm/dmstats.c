/*
 * Device Mapper PMDA - DM (device-mapper) Stats with dmstats API
 *
 * Copyright (c) 2017 Fumiya Shigemitsu.
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
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "dmstats.h"

#include <inttypes.h>
#include <libdevmapper.h>

const char *bc_programid = "dmstats";
const char *hist_programid = "dmstats";

int
pm_dm_stats_fetch(int item, struct pm_dm_stats_counter *dmsc, pmAtomValue *atom)
{
	if (item < 0 || item >= DM_STATS_NR_COUNTERS)
		return  PM_ERR_PMID;

	switch (item) {
		case DM_STATS_READS_COUNT:
			atom->ull = dmsc->pm_reads;
			break;
		case DM_STATS_READS_MERGED_COUNT:
			atom->ull = dmsc->pm_reads_merged;
			break;
		/* Correspond with kbytes units */
		case DM_STATS_READ_SECTORS_COUNT:
			atom->ull = dmsc->pm_read_sectors / 2;
			break;
		case DM_STATS_READ_NSECS:
			atom->ull = dmsc->pm_read_nsecs;
			break;
		case DM_STATS_WRITES_COUNT:
			atom->ull = dmsc->pm_writes;
			break;
		case DM_STATS_WRITES_MERGED_COUNT:
			atom->ull = dmsc->pm_writes_merged;
			break;
		/* Correspond with kbytes units */
		case DM_STATS_WRITE_SECTORS_COUNT:
			atom->ull = dmsc->pm_write_sectors / 2;
			break;
		case DM_STATS_WRITE_NSECS:
			atom->ull = dmsc->pm_write_nsecs;
			break;
		case DM_STATS_IO_IN_PROGRESS_COUNT:
			atom->ull = dmsc->pm_io_in_progress;
			break;
		case DM_STATS_IO_NSECS:
			atom->ull = dmsc->pm_io_nsecs;
			break;
		case DM_STATS_WEIGHTED_IO_NSECS:
			atom->ull = dmsc->pm_weighted_io_nsecs;
			break;
		case DM_STATS_TOTAL_READ_NSECS:
			atom->ull = dmsc->pm_total_read_nsecs;
			break;
		case DM_STATS_TOTAL_WRITE_NSECS:
			atom->ull = dmsc->pm_total_write_nsecs;
			break;
	}
	return 1;
}

int
pm_dm_histogram_fetch(int item, struct pm_dm_histogram *pdmh, pmAtomValue *atom)
{
	if (item < 0 || item > PM_DM_HISTOGRAM_NR_HISTOGRAMS)
		return PM_ERR_PMID;

	switch (item) {
		case PM_DM_HISTOGRAM_COUNT:
			atom->ull = pdmh->pm_bin_value;
			break;
		case PM_DM_HISTOGRAM_PERCENT:
			atom->f = pdmh->pm_bin_percent;
			break;
		case PM_DM_HISTOGRAM_REGION:
			atom->ull = pdmh->pm_region;
			break;
		case PM_DM_HISTOGRAM_BIN:
			atom->ull = pdmh->pm_bin;
			break;
	}
	return 1;
}

static void
_pm_dm_stats_value(struct dm_stats *dms, uint64_t *stats_val, uint64_t region_id, uint64_t area_id, int flag)
{
	static uint64_t reads = 0, reads_merged = 0, read_sectors = 0, read_nsecs = 0;
	static uint64_t writes = 0, writes_merged = 0, write_sectors = 0, write_nsecs = 0;
	static uint64_t io_in_progress = 0, io_nsecs = 0, weighted_io_nsecs = 0, total_read_nsecs = 0, total_write_nsecs = 0;

	if (flag) {
	dm_stats_foreach_region(dms) {
		reads
			+= dm_stats_get_counter(dms, DM_STATS_READS_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		reads_merged
			+= dm_stats_get_counter(dms, DM_STATS_READS_MERGED_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		read_sectors
			+= dm_stats_get_counter(dms, DM_STATS_READ_SECTORS_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		read_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_READ_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		writes
			+= dm_stats_get_counter(dms, DM_STATS_WRITES_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		writes_merged
			+= dm_stats_get_counter(dms, DM_STATS_WRITES_MERGED_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		write_sectors
			+= dm_stats_get_counter(dms, DM_STATS_WRITE_SECTORS_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		write_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_WRITE_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		io_in_progress
			+= dm_stats_get_counter(dms, DM_STATS_IO_IN_PROGRESS_COUNT, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		io_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_IO_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		weighted_io_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_WEIGHTED_IO_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		total_read_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_TOTAL_READ_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
		total_write_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_TOTAL_WRITE_NSECS, DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT);
	}

		stats_val[0]  = reads;
		stats_val[1]  = reads_merged;
		stats_val[2]  = read_sectors;
		stats_val[3]  = read_nsecs;
		stats_val[4]  = writes;
		stats_val[5]  = writes_merged;
		stats_val[6]  = write_sectors;
		stats_val[7]  = write_nsecs;
		stats_val[8]  = io_in_progress;
		stats_val[9]  = io_nsecs;
		stats_val[10] = weighted_io_nsecs;
		stats_val[11] = total_read_nsecs;
		stats_val[12] = total_write_nsecs;

	} else {
		reads
			+= dm_stats_get_counter(dms, DM_STATS_READS_COUNT, region_id, area_id);
		reads_merged
			+= dm_stats_get_counter(dms, DM_STATS_READS_MERGED_COUNT, region_id, area_id);
		read_sectors
			+= dm_stats_get_counter(dms, DM_STATS_READ_SECTORS_COUNT, region_id, area_id);
		read_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_READ_NSECS, region_id, area_id);
		writes
			+= dm_stats_get_counter(dms, DM_STATS_WRITES_COUNT, region_id, area_id);
		writes_merged
			+= dm_stats_get_counter(dms, DM_STATS_WRITES_MERGED_COUNT, region_id, area_id);
		write_sectors
			+= dm_stats_get_counter(dms, DM_STATS_WRITE_SECTORS_COUNT, region_id, area_id);
		write_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_WRITE_NSECS, region_id, area_id);
		io_in_progress
			+= dm_stats_get_counter(dms, DM_STATS_IO_IN_PROGRESS_COUNT, region_id, area_id);
		io_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_IO_NSECS, region_id, area_id);
		weighted_io_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_WEIGHTED_IO_NSECS, region_id, area_id);
		total_read_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_TOTAL_READ_NSECS, region_id, area_id);
		total_write_nsecs
			+= dm_stats_get_counter(dms, DM_STATS_TOTAL_WRITE_NSECS, region_id, area_id);
	}

}

#define SUM 1
#define EACH  0
#define COUNTER_METRIC 13

int
pm_dm_refresh_stats_counter(const char *name, struct pm_dm_stats_counter *dmsc)
{
	struct dm_stats *dms;
	uint64_t stats_val[COUNTER_METRIC] = {};

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto nostats;

	if (!dm_stats_bind_name(dms, name))
		goto nostats;

	if (!dm_stats_populate(dms, bc_programid, DM_STATS_REGIONS_ALL))
		goto nostats;

	_pm_dm_stats_value(dms, stats_val, 0, 0, SUM);

	dmsc->pm_reads             = stats_val[0];
	dmsc->pm_reads_merged      = stats_val[1];
	dmsc->pm_read_sectors      = stats_val[2];
	dmsc->pm_read_nsecs        = stats_val[3];
	dmsc->pm_writes            = stats_val[4];
	dmsc->pm_writes_merged     = stats_val[5];
	dmsc->pm_write_sectors     = stats_val[6];
	dmsc->pm_write_nsecs       = stats_val[7];
	dmsc->pm_io_in_progress    = stats_val[8];
	dmsc->pm_io_nsecs          = stats_val[9];
	dmsc->pm_weighted_io_nsecs = stats_val[10];
	dmsc->pm_total_read_nsecs  = stats_val[11];
	dmsc->pm_total_write_nsecs = stats_val[12];
	dm_stats_destroy(dms);

	return 0;
	dm_stats_destroy(dms);

nostats:
	dm_stats_destroy(dms);
	return -oserror();
}

static float
_make_percent(uint64_t numerator, uint64_t denominator)
{
	if (denominator)
		return ((double)numerator / (double)denominator);

	return 0;
}

int
pm_dm_refresh_stats_histogram(const char *name, struct pm_dm_histogram *pdmh)
{
	struct dm_stats *dms;
	struct dm_histogram *dmh;
	static uint64_t *buffer_count_data;
	static int number_of_bins = 0, bin = 0;
	static uint64_t total = 0;
	char buffer_name[BUFSIZ], *device_name, *region;
	int walk;
	uint64_t region_id, area_id;
	uint64_t stats_val[COUNTER_METRIC] = {};

	strcpy(buffer_name, name);
	device_name = strtok(buffer_name, ":");
	region = strtok(NULL , ":");
	walk = atoi(region);

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto nostats;

	if (!dm_stats_bind_name(dms, device_name))
		goto nostats;

	if (!dm_stats_list(dms, DM_STATS_ALL_PROGRAMS))
		goto nostats;

	for (int i = 0; i < walk; i++)
		dm_stats_walk_next_region(dms);

	region_id = dm_stats_get_current_region(dms);
	area_id = dm_stats_get_current_area(dms);


	if (bin == 0) {
		if (!dm_stats_populate(dms, hist_programid, region_id))
			goto nostats;

		_pm_dm_stats_value(dms, stats_val, region_id, area_id, EACH);

		if (!(dmh = dm_stats_get_histogram(dms, region_id, area_id)))
			goto nostats;

		number_of_bins = dm_histogram_get_nr_bins(dmh);
		total = dm_histogram_get_sum(dmh);

		buffer_count_data = (uint64_t *)malloc(sizeof(uint64_t)*number_of_bins);
		for (int i = 0; i < number_of_bins; i++) {
			buffer_count_data[i] = dm_histogram_get_bin_count(dmh, i);
		}
	}

	pdmh->pm_bin_value += buffer_count_data[bin];
	pdmh->pm_bin_percent = _make_percent(buffer_count_data[bin], total);
	pdmh->pm_region = walk;
	pdmh->pm_bin = number_of_bins;

	bin++;

	if (bin == number_of_bins) {
		bin = 0;
		total = 0;
		number_of_bins = 0;
		free(buffer_count_data);
	}

	dm_stats_destroy(dms);
	return 0;

nostats:
	dm_stats_destroy(dms);
	return -oserror();
}

static struct dm_names*
_dm_device_search(struct dm_names *names, struct dm_task *dmt)
{

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		goto nodevice;

	if (!dm_task_enable_checks(dmt))
		goto nodevice;

	if (!dm_task_run(dmt))
		goto nodevice;

	if (!(names = dm_task_get_names(dmt)))
		goto nodevice;

	return names;

nodevice:
	dm_task_destroy(dmt);
	return NULL;
}

static struct dm_stats*
_dm_stats_search_region(struct dm_names *names, struct dm_stats *dms, const char *programid)
{
	uint64_t nr_regions;

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto nostats;

	if (!dm_stats_bind_name(dms, names->name))
		goto nostats;

	if (!dm_stats_list(dms, programid))
		goto nostats;

	if (!(nr_regions = dm_stats_get_nr_regions(dms)))
		goto nostats;

	return dms;

nostats:
	dm_stats_destroy(dms);
	return NULL;
}

int
pm_dm_stats_instance_refresh(void)
{
	struct pm_dm_stats_counter *dmsc;
	struct dm_task *dmt = NULL;
	struct dm_stats *dms = NULL;
	struct dm_names *names = NULL;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_STATS_INDOM);

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if (!(names = _dm_device_search(names, dmt)))
		return -oserror();

	do {
		/* need dm_stats_destroy()? */
		if (!(dms = _dm_stats_search_region(names, dms, bc_programid))) {
			next = names->next;
			continue;
		}

		names = (struct dm_names*)((char *) names + next);
		sts = pmdaCacheLookupName(indom, names->name, NULL, (void **)&dmsc);
		if (sts == PM_ERR_INST || (sts >= 0 && dmsc == NULL)) {
			dmsc = calloc(1, sizeof(*dmsc));
			if (dmsc == NULL)
				return PM_ERR_AGAIN;

		}
		pmdaCacheStore(indom, PMDA_CACHE_ADD, names->name, (void *)dmsc);
		next = names->next;
		dm_stats_destroy(dms);
	} while(next);

	return 0;

}

#define NSEC_PER_USEC   1000L
#define NSEC_PER_MSEC   1000000L
#define NSEC_PER_SEC    1000000000L

#define DM_HISTOGRAM_BOUNDS_MAX 0x30

static void _scale_bound_value_to_suffix(uint64_t *bound, const char **suffix)
{
	*suffix = "ns";
	if (!(*bound % NSEC_PER_SEC)) {
		*bound /= NSEC_PER_SEC;
		*suffix = "s";
	} else if (!(*bound % NSEC_PER_MSEC)) {
		*bound /= NSEC_PER_MSEC;
		*suffix = "ms";
	} else if (!(*bound % NSEC_PER_USEC)) {
		*bound /= NSEC_PER_USEC;
		*suffix = "us";
	}
}

int
pm_dm_histogram_instance_refresh(void)
{
	struct pm_dm_histogram *pdmh;
	struct dm_histogram *dmh = NULL;
	struct dm_names *names = NULL;
	struct dm_task *dmt = NULL;
	struct dm_stats *dms = NULL;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_HISTOGRAM_INDOM);
	char buffer[BUFSIZ];
	uint64_t region_id, area_id;
	int bins;
	uint64_t bound_width;
	const char *suffix = "";

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if (!(names = _dm_device_search(names, dmt)))
		return -oserror();

	do {
		names = (struct dm_names*)((char *) names + next);

		if (!(dms = _dm_stats_search_region(names, dms, hist_programid))) {
			next = names->next;
			continue;
		}

		dm_stats_foreach_region(dms){
			region_id = dm_stats_get_current_region(dms);
			area_id = dm_stats_get_current_area(dms);

			if (!(dmh = dm_stats_get_histogram(dms, region_id, area_id)))
				continue;

			bins = dm_histogram_get_nr_bins(dmh);

			for (int i = 0; i < bins; i++) {
				bound_width = dm_histogram_get_bin_lower(dmh, i);
				_scale_bound_value_to_suffix(&bound_width, &suffix);
				sprintf(buffer, "%s:%lu:%lu%s", names->name, region_id, bound_width, suffix);

				sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&pdmh);
				if (sts == PM_ERR_INST || (sts >= 0 && pdmh == NULL)) {
					pdmh = calloc(1, sizeof(*pdmh));

					if (pdmh == NULL)
						return PM_ERR_AGAIN;

				}

			pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)pdmh);
			}
		}
		next = names->next;
		dm_stats_destroy(dms);
	} while(next);

	return 0;
}
