#define HAVE_DMSTATS 1

#ifdef HAVE_DMSTATS

#ifndef DM_H
#define DM_H

struct dm_stats_counter {
	uint64_t reads;		    /* Num reads completed */
	uint64_t reads_merged;	    /* Num reads merged */
	uint64_t read_sectors;	    /* Num sectors read */
	uint64_t read_nsecs;	    /* Num milliseconds spent reading */
	uint64_t writes;	    /* Num writes completed */
	uint64_t writes_merged;	    /* Num writes merged */
	uint64_t write_sectors;	    /* Num sectors written */
	uint64_t write_nsecs;	    /* Num milliseconds spent writing */
	uint64_t io_in_progress;    /* Num I/Os currently in progress */
	uint64_t io_nsecs;	    /* Num milliseconds spent doing I/Os */
	uint64_t weighted_io_nsecs; /* Weighted num milliseconds doing I/Os */
	uint64_t total_read_nsecs;  /* Total time spent reading in milliseconds */
	uint64_t total_write_nsecs; /* Total time spent writing in milliseconds */
	struct dm_histogram *histogram; /* Histogram. */
};

/*
struct dm_stats_metric {
};
*/

extern int dm_stats_fetch(int, struct dm_stats_counter *, pmAtomValue *);
extern int dm_refresh_stats_counter(const char *, struct dm_stats_counter *);
extern int dm_stats_instance_refresh(void);
extern void dm_stats_setup(void);

#endif
#endif
