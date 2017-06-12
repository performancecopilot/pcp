#ifndef DM_H
#define DM_H

typedef enum {
	DM_STATS_RD_MERGES_PER_SEC,
	DM_STATS_WR_MERGES_PER_SEC,
	DM_STATS_READS_PER_SEC,
	DM_STATS_WRITES_PER_SEC,
	DM_STATS_READ_SECTORS_PER_SEC,
	DM_STATS_WRITE_SECTORS_PER_SEC,
	DM_STATS_AVERAGE_REQUEST_SIZE,
	DM_STATS_AVERAGE_QUEUE_SIZE,
	DM_STATS_AVERAGE_WAIT_TIME,
	DM_STATS_AVERAGE_RD_WAIT_TIME,
	DM_STATS_AVERAGE_WR_WAIT_TIME,
	DM_STATS_SERVICE_TIME,
	DM_STATS_THROUGHPUT,
	DM_STATS_UTILIZATION,
	DM_STATS_NR_METRICS,
	NUM_DM_STATS
} dm_stats_metric_t;


typedef enum {
	DM_STATS_READS_COUNT,
	DM_STATS_READS_MERGED_COUNT,
	DM_STATS_READ_SECTORS_COUNT,
	DM_STATS_READ_NSECS,
	DM_STATS_WRITES_COUNT,
	DM_STATS_WRITES_MERGED_COUNT,
	DM_STATS_WRITE_SECTORS_COUNT,
	DM_STATS_WRITE_NSECS,
	DM_STATS_IO_IN_PROGRESS_COUNT,
	DM_STATS_IO_NSECS,
	DM_STATS_WEIGHTED_IO_NSECS,
	DM_STATS_TOTAL_READ_NSECS,
	DM_STATS_TOTAL_WRITE_NSECS,
	DM_STATS_NR_COUNTERS,
	NUM_DM_STATS_COUNTER
} dm_stats_counter_t;


struct dm_pcp_stats {
	uint64_t test_0;
	uint64_t test_1;
	char *dname;
	uint64_t tt;
};

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

struct dm_stats_metric {
};

extern int dm_stats_fetch(int, struct dm_stats_counter *, pmAtomValue *);
extern int dm_stats_instance_refresh(void);
extern int dm_fetch_refresh_stats_counter(const char *, struct dm_stats_counter *);
extern void dm_stats_setup(void);

#endif
