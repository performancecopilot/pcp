#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "indom.h"
#include "dm.h"

#include <inttypes.h>

#include <libdevmapper.h>

int
dm_stats_fetch(int item, struct dm_stats_counter *dmsc, pmAtomValue *atom)
{
	if (item < 0 || item >= NUM_DM_STATS_COUNTER)
		return  PM_ERR_PMID;

	switch(item) {
		case DM_STATS_READS_COUNT:
			atom->ull = dmsc->reads;
			break;
		case DM_STATS_READS_MERGED_COUNT:
			atom->ull = dmsc->reads_merged;
			break;
		case DM_STATS_READ_SECTORS_COUNT:
			atom->ull = dmsc->read_sectors;
			break;
		case DM_STATS_READ_NSECS:
			atom->ull = dmsc->read_nsecs;
			break;
		case DM_STATS_WRITES_COUNT:
			atom->ull = dmsc->writes;
			break;
		case DM_STATS_WRITES_MERGED_COUNT:
			atom->ull = dmsc->writes_merged;
			break;
		case DM_STATS_WRITE_SECTORS_COUNT:
			atom->ull = dmsc->write_sectors;
			break;
		case DM_STATS_WRITE_NSECS:
			atom->ull = dmsc->write_nsecs;
			break;
		case DM_STATS_IO_IN_PROGRESS_COUNT:
			atom->ull = dmsc->io_in_progress;
			break;
		case DM_STATS_IO_NSECS:
			atom->ull = dmsc->io_nsecs;
			break;
		case DM_STATS_WEIGHTED_IO_NSECS:
			atom->ull = dmsc->weighted_io_nsecs;
			break;
		case DM_STATS_TOTAL_READ_NSECS:
			atom->ull = dmsc->total_read_nsecs;
			break;
		case DM_STATS_TOTAL_WRITE_NSECS:
			atom->ull = dmsc->total_write_nsecs;
			break;
	}
	return 1;
}

int 
dm_refresh_stats_counter(const char *name, struct dm_stats_counter *dmsc)
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
	dmsc->reads             = reads;
	dmsc->reads_merged      = reads_merged;
	dmsc->read_sectors      = read_sectors;
	dmsc->read_nsecs        = read_nsecs;
	dmsc->writes            = writes;
	dmsc->writes_merged     = writes_merged;
	dmsc->write_sectors     = write_sectors;
	dmsc->write_nsecs       = write_nsecs;
	dmsc->io_in_progress    = io_in_progress;
	dmsc->io_nsecs          = io_nsecs;
	dmsc->weighted_io_nsecs = weighted_io_nsecs;
	dmsc->total_read_nsecs  = total_read_nsecs;
	dmsc->total_write_nsecs = total_write_nsecs;

	dm_stats_destroy(dms);

	return 0;

bad:
	dmsc->reads             = 0; 
	dmsc->reads_merged      = 0; 
	dmsc->read_sectors      = 0; 
	dmsc->read_nsecs        = 0; 
	dmsc->writes            = 0; 
	dmsc->writes_merged     = 0; 
	dmsc->write_sectors     = 0;
	dmsc->write_nsecs       = 0; 
	dmsc->io_in_progress    = 0; 
	dmsc->io_nsecs          = 0; 
	dmsc->weighted_io_nsecs = 0; 
	dmsc->total_read_nsecs  = 0; 
	dmsc->total_write_nsecs = 0; 

	return 0;
}

int 
dm_stats_instance_refresh(void)
{
	struct dm_stats_counter *dmsc;
	struct dm_task *dmt;
	struct dm_names *names;
	unsigned next = 0;
	int sts;
	pmInDom indom = dm_indom(DM_STATS_INDOM);

	pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

	if (!(dmt = dm_task_create(DM_DEVICE_LIST)))
		return 0;

	if (!dm_task_enable_checks(dmt))
		goto out;

	if (!dm_task_run(dmt))
		goto out;

	if(!(names = dm_task_get_names(dmt)))
		goto out;


	do {
		names = (struct dm_names*)((char *) names + next);
		sts = pmdaCacheLookupName(indom, names->name, NULL, (void **)&dmsc);
		if (sts == PM_ERR_INST || (sts >= 0 && dmsc == NULL)) {
			dmsc = calloc(1, sizeof(*dmsc));
			if (dmsc == NULL)
				return PM_ERR_AGAIN;
		}
		pmdaCacheStore(indom, PMDA_CACHE_ADD, names->name, (void *)dmsc);
		next = names->next;
	} while(next);


	dm_task_destroy(dmt);
	return 0;

out:
	dm_task_destroy(dmt);
	return 0;
}

void 
dm_stats_setup(void)
{
}
