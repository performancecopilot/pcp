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


int
pm_dm_stats_fetch(int item, struct pm_wrap *pw, pmAtomValue *atom)
{
	if (item < 0 || item >= PM_DM_STATS_NR_COUNTERS)
		return  PM_ERR_PMID;

	switch (item) {
		case PM_DM_STATS_READS:
			atom->ull = pw->dmsc->pm_reads;
			break;
		case PM_DM_STATS_READS_MERGED:
			atom->ull = pw->dmsc->pm_reads_merged;
			break;
		/* Correspond with kbytes units */
		case PM_DM_STATS_READ_SECTORS:
			atom->ull = pw->dmsc->pm_read_sectors / 2;
			break;
		case PM_DM_STATS_READ_NSECS:
			atom->ull = pw->dmsc->pm_read_nsecs;
			break;
		case PM_DM_STATS_WRITES:
			atom->ull = pw->dmsc->pm_writes;
			break;
		case PM_DM_STATS_WRITES_MERGED:
			atom->ull = pw->dmsc->pm_writes_merged;
			break;
		/* Correspond with kbytes units */
		case PM_DM_STATS_WRITE_SECTORS:
			atom->ull = pw->dmsc->pm_write_sectors / 2;
			break;
		case PM_DM_STATS_WRITE_NSECS:
			atom->ull = pw->dmsc->pm_write_nsecs;
			break;
		case PM_DM_STATS_IO_IN_PROGRESS:
			atom->ull = pw->dmsc->pm_io_in_progress;
			break;
		case PM_DM_STATS_IO_NSECS:
			atom->ull = pw->dmsc->pm_io_nsecs;
			break;
		case PM_DM_STATS_WEIGHTED_IO_NSECS:
			atom->ull = pw->dmsc->pm_weighted_io_nsecs;
			break;
		case PM_DM_STATS_TOTAL_READ_NSECS:
			atom->ull = pw->dmsc->pm_total_read_nsecs;
			break;
		case PM_DM_STATS_TOTAL_WRITE_NSECS:
			atom->ull = pw->dmsc->pm_total_write_nsecs;
			break;
	}
	return 1;
}

int
pm_dm_histogram_fetch(int item, struct pm_wrap *pw, pmAtomValue *atom)
{
	if (item < 0 || item > PM_DM_HISTOGRAM_NR_HISTOGRAMS)
		return PM_ERR_PMID;

	switch (item) {
		case PM_DM_HISTOGRAM_COUNT:
			atom->ull = pw->pdmh->pm_bin_value;
			break;
		case PM_DM_HISTOGRAM_PERCENT:
			atom->f = pw->pdmh->pm_bin_percent;
			break;
		case PM_DM_HISTOGRAM_REGION:
			atom->ull = pw->pdmh->pm_region;
			break;
		case PM_DM_HISTOGRAM_BIN:
			atom->ull = pw->pdmh->pm_bin;
			break;
	}
	return 1;
}

#define SUM_COUNTER(STATS_COUNTER) dm_stats_get_counter(pw->dms, (STATS_COUNTER), \
		DM_STATS_REGION_CURRENT, DM_STATS_AREA_CURRENT)

static int
_pm_dm_refresh_stats_counter(const char *name, struct pm_wrap *pw)
{
	uint64_t reads = 0, reads_merged = 0, read_sectors = 0, read_nsecs = 0;
	uint64_t writes = 0, writes_merged = 0, write_sectors = 0, write_nsecs = 0;
	uint64_t io_in_progress = 0, io_nsecs = 0, weighted_io_nsecs = 0, total_read_nsecs = 0, total_write_nsecs = 0;


	dm_stats_foreach_region(pw->dms) {
		reads             += SUM_COUNTER(DM_STATS_READS_COUNT);
		reads_merged      += SUM_COUNTER(DM_STATS_READS_COUNT);
		read_sectors      += SUM_COUNTER(DM_STATS_READ_SECTORS_COUNT);
		read_nsecs        += SUM_COUNTER(DM_STATS_READ_NSECS);
		writes            += SUM_COUNTER(DM_STATS_WRITES_COUNT);
		writes_merged     += SUM_COUNTER(DM_STATS_WRITES_MERGED_COUNT);
		write_sectors     += SUM_COUNTER(DM_STATS_WRITE_SECTORS_COUNT);
		write_nsecs       += SUM_COUNTER(DM_STATS_WRITE_NSECS);
		io_in_progress    += SUM_COUNTER(DM_STATS_IO_IN_PROGRESS_COUNT);
		io_nsecs          += SUM_COUNTER(DM_STATS_IO_NSECS);
		weighted_io_nsecs += SUM_COUNTER(DM_STATS_WEIGHTED_IO_NSECS);
		total_read_nsecs  += SUM_COUNTER(DM_STATS_TOTAL_READ_NSECS);
		total_write_nsecs += SUM_COUNTER(DM_STATS_TOTAL_WRITE_NSECS);

	}

	pw->dmsc->pm_reads             += reads;
	pw->dmsc->pm_reads_merged      += reads_merged;
	pw->dmsc->pm_read_sectors      += read_sectors;
	pw->dmsc->pm_read_nsecs        += read_nsecs;
	pw->dmsc->pm_writes            += writes;
	pw->dmsc->pm_writes_merged     += writes_merged;
	pw->dmsc->pm_write_sectors     += write_sectors;
	pw->dmsc->pm_write_nsecs       += write_nsecs;
	pw->dmsc->pm_io_in_progress    += io_in_progress;
	pw->dmsc->pm_io_nsecs          += io_nsecs;
	pw->dmsc->pm_weighted_io_nsecs += weighted_io_nsecs;
	pw->dmsc->pm_total_read_nsecs  += total_read_nsecs;
	pw->dmsc->pm_total_write_nsecs += total_write_nsecs;

	return 0;

}

#define PER_COUNTER(STATS_COUNTER) dm_stats_get_counter(pw->dms, (STATS_COUNTER), \
		pw->region_id, pw->area_id)
static int
_pm_dm_refresh_stats_hcounter(const char *name, struct pm_wrap *pw)
{
	uint64_t reads = 0, reads_merged = 0, read_sectors = 0, read_nsecs = 0;
	uint64_t writes = 0, writes_merged = 0, write_sectors = 0, write_nsecs = 0;
	uint64_t io_in_progress = 0, io_nsecs = 0, weighted_io_nsecs = 0, total_read_nsecs = 0, total_write_nsecs = 0;


	reads             += SUM_COUNTER(DM_STATS_READS_COUNT);
	reads_merged      += SUM_COUNTER(DM_STATS_READS_COUNT);
	read_sectors      += SUM_COUNTER(DM_STATS_READ_SECTORS_COUNT);
	read_nsecs        += SUM_COUNTER(DM_STATS_READ_NSECS);
	writes            += SUM_COUNTER(DM_STATS_WRITES_COUNT);
	writes_merged     += SUM_COUNTER(DM_STATS_WRITES_MERGED_COUNT);
	write_sectors     += SUM_COUNTER(DM_STATS_WRITE_SECTORS_COUNT);
	write_nsecs       += SUM_COUNTER(DM_STATS_WRITE_NSECS);
	io_in_progress    += SUM_COUNTER(DM_STATS_IO_IN_PROGRESS_COUNT);
	io_nsecs          += SUM_COUNTER(DM_STATS_IO_NSECS);
	weighted_io_nsecs += SUM_COUNTER(DM_STATS_WEIGHTED_IO_NSECS);
	total_read_nsecs  += SUM_COUNTER(DM_STATS_TOTAL_READ_NSECS);
	total_write_nsecs += SUM_COUNTER(DM_STATS_TOTAL_WRITE_NSECS);


	pw->dmsc->pm_reads             += reads;
	pw->dmsc->pm_reads_merged      += reads_merged;
	pw->dmsc->pm_read_sectors      += read_sectors;
	pw->dmsc->pm_read_nsecs        += read_nsecs;
	pw->dmsc->pm_writes            += writes;
	pw->dmsc->pm_writes_merged     += writes_merged;
	pw->dmsc->pm_write_sectors     += write_sectors;
	pw->dmsc->pm_write_nsecs       += write_nsecs;
	pw->dmsc->pm_io_in_progress    += io_in_progress;
	pw->dmsc->pm_io_nsecs          += io_nsecs;
	pw->dmsc->pm_weighted_io_nsecs += weighted_io_nsecs;
	pw->dmsc->pm_total_read_nsecs  += total_read_nsecs;
	pw->dmsc->pm_total_write_nsecs += total_write_nsecs;

	return 0;

}
static float
_make_percent(uint64_t numerator, uint64_t denominator)
{
	if (denominator)
		return ((double)numerator / (double)denominator);

	return 0;
}

int
pm_dm_stats_instance_refresh(void);

static int
_pm_dm_refresh_stats_histogram(const char *name, struct pm_wrap *pw)
{
	struct dm_histogram *dmh;
	struct pm_wrap *pw2;
	static uint64_t *buffer_count_data;
	static int number_of_bins = 0, bin = 0;
	static uint64_t total = 0;
	int walk;
	uint64_t region_id, area_id;
	char *dev;
	pmInDom indom = dm_indom(DM_STATS_INDOM);

	dev = pw->dev;
	region_id = pw->region_id;
	area_id = pw->area_id;

	if (bin == 0) {
		if (!dm_stats_populate(pw->dms, DM_STATS_ALL_PROGRAMS, pw->region_id))
			goto nostats;
		if (!(dmh = dm_stats_get_histogram(pw->dms, region_id, area_id)))
			goto nostats;

		number_of_bins = dm_histogram_get_nr_bins(dmh);
		total = dm_histogram_get_sum(dmh);

		buffer_count_data = (uint64_t *)malloc(sizeof(uint64_t)*number_of_bins);
		for (int i = 0; i < number_of_bins; i++) {
			buffer_count_data[i] = dm_histogram_get_bin_count(dmh, i);
		}
	}

	pw->pdmh->pm_bin_value += buffer_count_data[bin];
	pw->pdmh->pm_bin_percent = _make_percent(buffer_count_data[bin], total);
	pw->pdmh->pm_region = region_id;
	pw->pdmh->pm_bin = number_of_bins;

	bin++;

	if (bin == number_of_bins) {
		bin = 0;
		total = 0;
		number_of_bins = 0;
		free(buffer_count_data);
	}

	return 0;

nostats:
	dm_stats_destroy(pw->dms);
	return -oserror();
}

int pm_dm_refresh_stats(struct pm_wrap *pw, int flag)
{
	const char *tmp = "";
    	pmInDom indom;
    	char *name;
    	int inst;
    	int sts = 0;



	if (flag == 1) {
		if (!dm_stats_populate(pw->dms, DM_STATS_ALL_PROGRAMS, DM_STATS_REGIONS_ALL))
			goto nostats;
		_pm_dm_refresh_stats_counter(tmp, pw);
		/* whether there is hitogram */
	}
	if (flag == 0) {
		_pm_dm_refresh_stats_histogram(tmp, pw);
		struct pm_wrap *pw2;

        	if ((sts = pm_dm_stats_instance_refresh()) < 0)
		    return sts;

        	indom = dm_indom(DM_STATS_INDOM);

        	for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
			if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
		        	break;
		    	if (!pmdaCacheLookup(indom, inst, &name, (void **)&pw2) || !pw2)
		        	continue;
        	    	if (!strcmp(pw2->dev, pw->dev)) {
			   	 pw2->dms = pw->dms;
				_pm_dm_refresh_stats_hcounter(tmp, pw2);
			   	break;
		    	}
        	}
	}

	return 0;

nostats:
	dm_stats_destroy(pw->dms);
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
_dm_stats_search_region(struct dm_names *names, struct dm_stats *dms)
{
	uint64_t nr_regions;

	if (!(dms = dm_stats_create(DM_STATS_ALL_PROGRAMS)))
		goto nostats;

	if (!dm_stats_bind_name(dms, names->name))
		goto nostats;

	if (!dm_stats_list(dms, DM_STATS_ALL_PROGRAMS))
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
	struct pm_wrap *pw;
	struct dm_task *dmt = NULL;
	struct dm_names *names = NULL;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_STATS_INDOM);

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if (!(names = _dm_device_search(names, dmt)))
		return -oserror();

	do {
		names = (struct dm_names*)((char *) names + next);

		sts = pmdaCacheLookupName(indom, names->name, NULL, (void **)&pw);
		if (sts == PM_ERR_INST || (sts >= 0 && pw == NULL)) {
			pw = (struct pm_wrap *)malloc(sizeof(struct pm_wrap));
			pw->dmsc = calloc(1, sizeof(pw->dmsc));
			if (pw == NULL)
				return PM_ERR_AGAIN;
		}
		if (!(pw->dms = _dm_stats_search_region(names, pw->dms))) {
			next = names->next;
			continue;
		}
		pw->dev = names->name;
		pmdaCacheStore(indom, PMDA_CACHE_ADD, names->name, (void *)pw);
		next = names->next;
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
	struct pm_wrap *pw;
	struct dm_stats *dms;
	struct dm_histogram *dmh;
	struct dm_names *names = NULL;
	struct dm_task *dmt = NULL;
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

		if (!(dms = _dm_stats_search_region(names, dms))) {
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

				sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&pw);
				if (sts == PM_ERR_INST || (sts >= 0 && pw == NULL)) {
					pw = (struct pm_wrap *)malloc(sizeof(struct pm_wrap));
					pw->dmsc = calloc(1, sizeof(pw->dmsc));
					pw->pdmh = calloc(1, sizeof(pw->pdmh));

					if (pw == NULL)
						return PM_ERR_AGAIN;

				}
				pw->dms = dms;
				pw->region_id = region_id;
				pw->area_id = area_id;
				pw->dev = names->name;
				pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)pw);
			}
		}
		next = names->next;
	} while(next);

	return 0;
}
