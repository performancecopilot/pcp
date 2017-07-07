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

#ifdef HAVE_DMSTATS

#include <inttypes.h>
#include <libdevmapper.h>

int
pm_dm_stats_fetch(int item, struct pm_dm_stats_counter *dmsc, pmAtomValue *atom)
{
	if (item < 0 || item >= DM_STATS_NR_COUNTERS)
		return  PM_ERR_PMID;

	switch(item) {
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
	if (item < 0 || item > DM_HISTOGRAM)
		return PM_ERR_PMID;

	switch (item) {
		case DM_HISTOGRAM:
			atom->f = pdmh->tmp;
			break;
	}
	return 1;
}

int
pm_dm_refresh_stats_counter(const char *name, struct pm_dm_stats_counter *dmsc)
{
	struct dm_stats *dms;
	uint64_t reads = 0, reads_merged = 0, read_sectors = 0, read_nsecs = 0;
	uint64_t writes = 0, writes_merged = 0, write_sectors = 0, write_nsecs = 0;
	uint64_t io_in_progress = 0, io_nsecs = 0, weighted_io_nsecs = 0, total_read_nsecs = 0, total_write_nsecs = 0;

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto bad;

	if (!dm_stats_bind_name(dms, name))
		goto bad;

	if (!dm_stats_populate(dms, DM_STATS_ALL_PROGRAMS, DM_STATS_REGIONS_ALL))
		goto bad;

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
	dmsc->pm_reads             += reads;
	dmsc->pm_reads_merged      += reads_merged;
	dmsc->pm_read_sectors      += read_sectors;
	dmsc->pm_read_nsecs        += read_nsecs;
	dmsc->pm_writes            += writes;
	dmsc->pm_writes_merged     += writes_merged;
	dmsc->pm_write_sectors     += write_sectors;
	dmsc->pm_write_nsecs       += write_nsecs;
	dmsc->pm_io_in_progress    += io_in_progress;
	dmsc->pm_io_nsecs          += io_nsecs;
	dmsc->pm_weighted_io_nsecs += weighted_io_nsecs;
	dmsc->pm_total_read_nsecs  += total_read_nsecs;
	dmsc->pm_total_write_nsecs += total_write_nsecs;

	dm_stats_destroy(dms);

	return 0;

bad:
	dm_stats_destroy(dms);
	return -oserror();
}

static int
pm_dm_populate()
{
}

int
pm_dm_refresh_stats_histogram(const char *name, struct pm_dm_histogram *pdmh)
{
	struct dm_stats *dms;
	struct dm_histogram *dmh;
	char buffer_name[BUFSIZ];
	char *device_name;
	char *region;
	int walk;
	uint64_t region_id, area_id;
	dm_percent_t pr;
	float val_fit;
	int bin;

	strcpy(buffer_name, name);
	device_name = strtok(buffer_name, ":");
	region = strtok(NULL , ":");
	walk = atoi(region);
	bin = atoi(strtok(NULL, "-"));

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


	if (!(dmh = dm_stats_get_histogram(dms, region_id, area_id)))
		goto nostats;

	if (bin == 0) {
	if (!dm_stats_populate(dms, DM_STATS_ALL_PROGRAMS, region_id))
		goto nostats;

	}
	pr = dm_histogram_get_bin_percent(dmh, bin);
	val_fit = dm_percent_to_float(pr);

	pdmh->tmp = val_fit;


	return 0;

nostats:
	dm_stats_destroy(dms);
	return -oserror();
}

static int
_dm_stats_search_region(struct dm_names *names)
{
	struct dm_stats *dms;
	uint64_t nr_regions;

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto nostats;

	if (!dm_stats_bind_name(dms, names->name))
		goto nostats;

	if (!dm_stats_list(dms, DM_STATS_ALL_PROGRAMS))
		goto nostats;

	if (!(nr_regions = dm_stats_get_nr_regions(dms)))
		goto nostats;


	dm_stats_destroy(dms);
	return 1;
nostats:
	dm_stats_destroy(dms);
	return 0;
}

int
pm_dm_stats_instance_refresh(void)
{
	struct pm_dm_stats_counter *dmsc;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_STATS_INDOM);

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		goto nodevice;

	if (!dm_task_enable_checks(dmt))
		goto nodevice;

	if (!dm_task_run(dmt))
		goto nodevice;

	if(!(names = dm_task_get_names(dmt)))
		goto nodevice;


	do {
		names = (struct dm_names*)((char *) names + next);
		sts = pmdaCacheLookupName(indom, names->name, NULL, (void **)&dmsc);
		if (sts == PM_ERR_INST || (sts >= 0 && dmsc == NULL)) {
			dmsc = calloc(1, sizeof(*dmsc));
			if (dmsc == NULL)
				return PM_ERR_AGAIN;

			if (!_dm_stats_search_region(names)) {
				next = names->next;
				continue;
			}
		}
		pmdaCacheStore(indom, PMDA_CACHE_ADD, names->name, (void *)dmsc);
		next = names->next;
	} while(next);


	dm_task_destroy(dmt);
	return 0;

nodevice:
	dm_task_destroy(dmt);
	return -oserror();
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
	struct dm_histogram *dmh;
	struct dm_task *dmt;
	struct dm_names *names;
	struct dm_stats *dms;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_HISTOGRAM_INDOM);
	char buffer[BUFSIZ];
	uint64_t region_id, area_id;
	int bins;
	uint64_t bound_width;
	const char *suffix = "";

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	/* search whether there are dm devices */
	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		goto nodevice;

	if (!dm_task_enable_checks(dmt))
		goto nodevice;

	if (!dm_task_run(dmt))
		goto nodevice;

	if(!(names = dm_task_get_names(dmt)))
		goto nodevice;

	do {
		names = (struct dm_names*)((char *) names + next);

		/* search whether there are dm devices stastics*/
		if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
			goto nostats;

		if (!dm_stats_bind_name(dms, names->name))
			goto nostats;

		if (!dm_stats_list(dms, DM_STATS_ALL_PROGRAMS))
			goto nostats;

		if (!dm_stats_get_nr_regions(dms))
			goto nostats;

		/* get each region*/
		dm_stats_foreach_region(dms){
			region_id = dm_stats_get_current_region(dms);
			area_id = dm_stats_get_current_area(dms);

			/* Whether there is histogram */
			if (!(dmh = dm_stats_get_histogram(dms, region_id, area_id)))
				continue;

			bins = dm_histogram_get_nr_bins(dmh);

			for (int i = 0; i < bins - 1; i++) {
				bound_width = dm_histogram_get_bin_upper(dmh, i);
				_scale_bound_value_to_suffix(&bound_width, &suffix);
				sprintf(buffer, "%s:%lu:%d-%lu%s", names->name, region_id, i, bound_width, suffix);

				sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&pdmh);
				if (sts == PM_ERR_INST || (sts >= 0 && pdmh == NULL)) {
					pdmh = calloc(1, sizeof(*pdmh));

					if (pdmh == NULL)
						return PM_ERR_AGAIN;

				}

			pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)pdmh);
			}
		}
nostats:
		dm_stats_destroy(dms);
		next = names->next;
	} while(next);

	dm_task_destroy(dmt);
	return 0;

nodevice:
	dm_task_destroy(dmt);
	return -oserror();
}

void
pm_dm_stats_setup(void)
{
}
#endif
