/*
 * Docker PMDA
 *
 * Copyright (c) 2016 Red Hat.
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
#include "pmjson.h"
#include "domain.h"
#include "pmhttp.h"
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>


enum {
    CONTAINER_FLAG_RUNNING	= (1<<0),
    CONTAINER_FLAG_PAUSED	= (1<<1),
    CONTAINER_FLAG_RESTARTING	= (1<<2),
};

enum {
    CONTAINERS_INDOM,
    CONTAINERS_STATS_INDOM,
    CONTAINERS_STATS_CACHE_INDOM,
    NUM_INDOMS
};

enum {
    CLUSTER_BASIC = 0,
    CLUSTER_VERSION,
    CLUSTER_STATS,
    //    CLUSTER_SYSWIDE,
    CLUSTER_CONTROL,
    NUM_CLUSTERS
};

static pthread_t docker_query_thread;	/* runs all libpcp_web/docker.sock queries */
static struct http_client *http_client;
static pmdaIndom docker_indomtab[NUM_INDOMS];
#define INDOM(x) (docker_indomtab[x].it_indom)
#define INDOMTAB_SZ (sizeof(docker_indomtab)/sizeof(docker_indomtab[0]))
char resulting_path[MAXPATHLEN];
static int ready = 0;

static json_metric_desc json_metrics[] = {
    /* GET localhost/containers/$ID/json */
    { "State/Pid", 0, 1, {0}, ""},
    { "Name", 0, 1, {0}, ""},
    { "State/Running", CONTAINER_FLAG_RUNNING, 1, {0}, ""},
    { "State/Paused", CONTAINER_FLAG_PAUSED, 1, {0}, ""},
    { "State/Restarting", CONTAINER_FLAG_RESTARTING, 1, {0}, ""},
};
static json_metric_desc version_metrics[] = {
    /* GET /version */
    { "Version", 0, 1, {0}, ""},
    { "Os", 0, 1, {0}, ""},
    { "KernelVersion", 0, 1, {0}, ""},
    { "GoVersion", 0, 1, {0}, ""},
    { "GitCommit", 0, 1, {0}, ""},
    { "Arch", 0, 1, {0}, ""},
    { "ApiVersion", 0, 1, {0}, ""},
    //    { "BuildTime", 0, 1, {0}, ""},
    //    { "Experimental", 0, 1, {0}, ""},
};

// this so needs another name, WHAT COLOUR IS THE BIKESHED?!
static json_metric_desc stats_metrics[] = {

    //    { "cpu_stats/cpu_usage/percpu_usage", 0, 1, {0}, ""}, //XXX fix arrays
    /*        "cpu_usage": {
            "percpu_usage": [
                163882262,
                47891411,
                39057464,
                31342456,
                0,
                0,
                0,
                0
		],*/
    { "cpu_stats/cpu_usage/total_usage", 0, 1, {0}, ""},
    { "cpu_stats/cpu_usage/usage_in_kernelmode", 0, 1, {0}, ""},
    { "cpu_stats/cpu_usage/usage_in_usermode", 0, 1, {0}, ""},
        /*
            "total_usage": 282173593,
            "usage_in_kernelmode": 80000000,
            "usage_in_usermode": 190000000
	    },*/

    { "cpu_stats/system_cpu_usage", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/periods", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/throttled_periods", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/throttled_time", 0, 1, {0}, ""},
    /*        "system_cpu_usage": 54927880000000,*/
    /*    "throttling_data": {
            "periods": 0,
            "throttled_periods": 0,
            "throttled_time": 0
        }
	},*/
    { "memory_stats/failcnt", 0, 1, {0}, ""},
    { "memory_stats/limit", 0, 1, {0}, ""},
    { "memory_stats/max_usage", 0, 1, {0}, ""},
    { "memory_stats/stats/active_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/active_file", 0, 1, {0}, ""},
    { "memory_stats/stats/cache", 0, 1, {0}, ""},
    { "memory_stats/stats/dirty", 0, 1, {0}, ""},
    { "memory_stats/stats/hierarchical_memory_limit", 0, 1, {0}, ""},
    { "memory_stats/stats/hierarchical_memsw_limit", 0, 1, {0}, ""},
    { "memory_stats/stats/inactive_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/inactive_file", 0, 1, {0}, ""},
    { "memory_stats/stats/mapped_file", 0, 1, {0}, ""},
    { "memory_stats/stats/pgfault", 0, 1, {0}, ""},
    { "memory_stats/stats/pgmajfault", 0, 1, {0}, ""},
    { "memory_stats/stats/pgpgin", 0, 1, {0}, ""},
    { "memory_stats/stats/pgpgout", 0, 1, {0}, ""},
    { "memory_stats/stats/recent_rotated_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/recent_rotated_file", 0, 1, {0}, ""},
    { "memory_stats/stats/recent_scanned_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/recent_scanned_file", 0, 1, {0}, ""},
    { "memory_stats/stats/rss", 0, 1, {0}, ""},
    { "memory_stats/stats/rss_huge", 0, 1, {0}, ""},
    { "memory_stats/stats/swap", 0, 1, {0}, ""},
    { "memory_stats/stats/total_active_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/total_active_file", 0, 1, {0}, ""},
    { "memory_stats/stats/total_cache", 0, 1, {0}, ""},
    { "memory_stats/stats/total_dirty", 0, 1, {0}, ""},
    { "memory_stats/stats/total_inactive_anon", 0, 1, {0}, ""},
    { "memory_stats/stats/total_inactive_file", 0, 1, {0}, ""},
    { "memory_stats/stats/total_mapped_file", 0, 1, {0}, ""},
    { "memory_stats/stats/total_pgfault", 0, 1, {0}, ""},
    { "memory_stats/stats/total_pgmajfault", 0, 1, {0}, ""},
    { "memory_stats/stats/total_pgpgin", 0, 1, {0}, ""},
    { "memory_stats/stats/total_pgpgout", 0, 1, {0}, ""},
    { "memory_stats/stats/total_rss", 0, 1, {0}, ""},
    { "memory_stats/stats/total_rss_huge", 0, 1, {0}, ""},
    { "memory_stats/stats/total_swap", 0, 1, {0}, ""},
    { "memory_stats/stats/total_unevictable", 0, 1, {0}, ""},
    { "memory_stats/stats/total_writeback", 0, 1, {0}, ""},
    { "memory_stats/stats/unevictable", 0, 1, {0}, ""},
    { "memory_stats/stats/writeback", 0, 1, {0}, ""},
    { "memory_stats/usage", 0, 1, {0}, ""},

};

//static json_metric_desc startup = { "ApiVersion", 0, 1, {0}, ""};

static pmdaMetric metrictab[] = {
    /* docker.pid */
    { (void*) &json_metrics[0], 
      { PMDA_PMID(CLUSTER_BASIC,0), PM_TYPE_U64, CONTAINERS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.name */
    { (void*) &json_metrics[1], 
      { PMDA_PMID(CLUSTER_BASIC,1), PM_TYPE_STRING, CONTAINERS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.running */
    { (void*) &json_metrics[2], 
      { PMDA_PMID(CLUSTER_BASIC,2), PM_TYPE_U32, CONTAINERS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.paused */
    { (void*) &json_metrics[3], 
      { PMDA_PMID(CLUSTER_BASIC,3), PM_TYPE_U32, CONTAINERS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.restarting */
    { (void*) &json_metrics[4], 
      { PMDA_PMID(CLUSTER_BASIC,4), PM_TYPE_U32, CONTAINERS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    

    /* version */
    { (void*) &version_metrics[0], 
      { PMDA_PMID(CLUSTER_VERSION,0), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* OS */
    { (void*) &version_metrics[1], 
      { PMDA_PMID(CLUSTER_VERSION,1), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* Kernel Version */
        { (void*) &version_metrics[2], 
	  { PMDA_PMID(CLUSTER_VERSION,2), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* GoVersion */
        { (void*) &version_metrics[3], 
	  { PMDA_PMID(CLUSTER_VERSION,3), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* Git Commit */
    { (void*) &version_metrics[4], 
      { PMDA_PMID(CLUSTER_VERSION,4), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* Arch */
    { (void*) &version_metrics[5], 
      { PMDA_PMID(CLUSTER_VERSION,5), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* API Version */
    { (void*) &version_metrics[6], 
      { PMDA_PMID(CLUSTER_VERSION,6), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* Build Time */
    /*    { (void*) &version_metrics[7], 
      { PMDA_PMID(CLUSTER_VERSION,7), PM_TYPE_STRING, CONTAINERS_VERSION_INDOM, PM_SEM_INSTANT, 
      PMDA_PMUNITS(0,0,0,0,0,0) }, },*/
    /* Experimental */
    //    { (void*) &version_metrics[13], 
    //      { PMDA_PMID(CLUSTER_VERSION,14), PM_TYPE_32, CONTAINERS_VERSION_INDOM, PM_SEM_INSTANT, 
    //        PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* cpu_stats.cpu_usage.total_usage */
    { (void*) &stats_metrics[0], 
      { PMDA_PMID(CLUSTER_STATS,0), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* cpu_stats.cpu_usage.usage_in_kernelmode */
    { (void*) &stats_metrics[1], 
      { PMDA_PMID(CLUSTER_STATS,1), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* cpu_stats.cpu_usage.usage_in_usermode */
    { (void*) &stats_metrics[2], 
      { PMDA_PMID(CLUSTER_STATS,2), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* cpu_stats.system_cpu_usage */
    { (void*) &stats_metrics[3], 
      { PMDA_PMID(CLUSTER_STATS,3), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* cpu_stats.trottling_data.periods */
    { (void*) &stats_metrics[4], 
      { PMDA_PMID(CLUSTER_STATS,4), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
    /* cpu_stats.trottling_data.throttled_periods */
    { (void*) &stats_metrics[5], 
      { PMDA_PMID(CLUSTER_STATS,5), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },
    /* cpu_stats.throttling_data.throttled_time */
    { (void*) &stats_metrics[6], 
      { PMDA_PMID(CLUSTER_STATS,6), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,1,0,0,PM_TIME_USEC,0) }, },

    /* memory_stats.failcnt */
    { (void*) &stats_metrics[7], 
      { PMDA_PMID(CLUSTER_STATS,7), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.limit */
    { (void*) &stats_metrics[8], 
      { PMDA_PMID(CLUSTER_STATS,8), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.max_usage */
    { (void*) &stats_metrics[9], 
      { PMDA_PMID(CLUSTER_STATS,9), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.active_anon */
    { (void*) &stats_metrics[10], 
      { PMDA_PMID(CLUSTER_STATS,10), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.active_file */
    { (void*) &stats_metrics[11], 
      { PMDA_PMID(CLUSTER_STATS,11), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0, PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.cache */
    { (void*) &stats_metrics[12], 
      { PMDA_PMID(CLUSTER_STATS,12), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.dirty */
    { (void*) &stats_metrics[13], 
      { PMDA_PMID(CLUSTER_STATS,13), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    /* memory_stats.stats.hierarchical_memory_limit */
    { (void*) &stats_metrics[14], 
      { PMDA_PMID(CLUSTER_STATS,14), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.hierarchical_memsw_limit */
    { (void*) &stats_metrics[15], 
      { PMDA_PMID(CLUSTER_STATS,15), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.inactive_anon */
    { (void*) &stats_metrics[16], 
      { PMDA_PMID(CLUSTER_STATS,16), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.inactive_file */
    { (void*) &stats_metrics[17], 
      { PMDA_PMID(CLUSTER_STATS,17), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.mapped_file */
    { (void*) &stats_metrics[18], 
      { PMDA_PMID(CLUSTER_STATS,18), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.pgfault */
    { (void*) &stats_metrics[19], 
      { PMDA_PMID(CLUSTER_STATS,19), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.pgmajfault */
    { (void*) &stats_metrics[20], 
      { PMDA_PMID(CLUSTER_STATS,20), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0, PM_COUNT_ONE) }, },
    /* memory_stats.stats.pgpgin */
    { (void*) &stats_metrics[21], 
      { PMDA_PMID(CLUSTER_STATS,21), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.pgpgout */
    { (void*) &stats_metrics[22], 
      { PMDA_PMID(CLUSTER_STATS,22), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.recent_rotated_anon */
    { (void*) &stats_metrics[23], 
      { PMDA_PMID(CLUSTER_STATS,23), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.recent_rotated_file */
    { (void*) &stats_metrics[24], 
      { PMDA_PMID(CLUSTER_STATS,24), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.recent_scanned_anon */
    { (void*) &stats_metrics[25], 
      { PMDA_PMID(CLUSTER_STATS,25), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.recent_scanned_file */
    { (void*) &stats_metrics[26], 
      { PMDA_PMID(CLUSTER_STATS,26), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.rss */
    { (void*) &stats_metrics[27], 
      { PMDA_PMID(CLUSTER_STATS,27), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.rss_huge */
    { (void*) &stats_metrics[28], 
      { PMDA_PMID(CLUSTER_STATS,28), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.swap */
    { (void*) &stats_metrics[29], 
      { PMDA_PMID(CLUSTER_STATS,29), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_active_anon */
    { (void*) &stats_metrics[30], 
      { PMDA_PMID(CLUSTER_STATS,30), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_active_file */
    { (void*) &stats_metrics[31], 
      { PMDA_PMID(CLUSTER_STATS,31), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_cache */
    { (void*) &stats_metrics[32], 
      { PMDA_PMID(CLUSTER_STATS,32), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_dirty */
    { (void*) &stats_metrics[33], 
      { PMDA_PMID(CLUSTER_STATS,33), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* memory_stats.stats.total_inactive_anon */
    { (void*) &stats_metrics[34], 
      { PMDA_PMID(CLUSTER_STATS,34), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_inactive_file */
    { (void*) &stats_metrics[35], 
      { PMDA_PMID(CLUSTER_STATS,35), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_mapped_file */
    { (void*) &stats_metrics[36], 
      { PMDA_PMID(CLUSTER_STATS,36), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_pgfault */
    { (void*) &stats_metrics[37], 
      { PMDA_PMID(CLUSTER_STATS,37), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.total_pgmajfault */
    { (void*) &stats_metrics[38], 
      { PMDA_PMID(CLUSTER_STATS,38), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.total_pgpgin */
    { (void*) &stats_metrics[39], 
      { PMDA_PMID(CLUSTER_STATS,39), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.total_pgpgout */
    { (void*) &stats_metrics[40], 
      { PMDA_PMID(CLUSTER_STATS,40), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* memory_stats.stats.total_rss */
    { (void*) &stats_metrics[41], 
      { PMDA_PMID(CLUSTER_STATS,41), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_rss_huge */
    { (void*) &stats_metrics[42], 
      { PMDA_PMID(CLUSTER_STATS,42), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_swap */
    { (void*) &stats_metrics[43], 
      { PMDA_PMID(CLUSTER_STATS,43), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_unevictable */
    { (void*) &stats_metrics[44], 
      { PMDA_PMID(CLUSTER_STATS,44), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.total_writeback */
    { (void*) &stats_metrics[45], 
      { PMDA_PMID(CLUSTER_STATS,45), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.unevictable */
    { (void*) &stats_metrics[46], 
      { PMDA_PMID(CLUSTER_STATS,46), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.stats.writeback */
    { (void*) &stats_metrics[47], 
      { PMDA_PMID(CLUSTER_STATS,47), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    /* memory_stats.usage */
    { (void*) &stats_metrics[48], 
      { PMDA_PMID(CLUSTER_STATS,48), PM_TYPE_U64, CONTAINERS_STATS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },

    /* control frequency of the thread fetching the metrics */
    { NULL,
      { PMDA_PMID(CLUSTER_CONTROL,0), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
	PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
};

static int             isDSO = 1;		/* =0 I am a daemon */
static char            *username =  "root";
static char            mypath[MAXPATHLEN];
static char            *local_path;
static int             thread_freq = 1;
static pthread_mutex_t refresh_mutex;
static pthread_mutex_t docker_mutex;
static pthread_mutex_t stats_mutex;

void refresh_insts(char *path);
int grab_values(char* json_query, pmInDom indom, char* local_path, json_metric_desc* json, int json_size);
int docker_setup();

/* General utility routine for checking timestamp differences */
int
stat_time_differs(struct stat *statbuf, struct stat *lastsbuf)
{
#if defined(HAVE_ST_MTIME_WITH_E) && defined(HAVE_STAT_TIME_T)
    if (statbuf->st_mtime != lastsbuf->st_mtime)
	return 1;
#elif defined(HAVE_ST_MTIME_WITH_SPEC)
    if ((statbuf->st_mtimespec.tv_sec != lastsbuf->st_mtimespec.tv_sec) ||
	(statbuf->st_mtimespec.tv_nsec != lastsbuf->st_mtimespec.tv_nsec))
	return 1;
#elif defined(HAVE_STAT_TIMESTRUC) || defined(HAVE_STAT_TIMESPEC) || defined(HAVE_STAT_TIMESPEC_T)
    if ((statbuf->st_mtim.tv_sec != lastsbuf->st_mtim.tv_sec) ||
	(statbuf->st_mtim.tv_nsec != lastsbuf->st_mtim.tv_nsec))
	return 1;
#else
!bozo!
#endif
    return 0;
}


void refresh_basic(char* local_path)
{
    char    json_query[BUFSIZ] = "";
    pmInDom indom = INDOM(CONTAINERS_INDOM);
    sprintf(json_query, "http://localhost/containers/%s/json", local_path);
    grab_values(json_query, indom, local_path, json_metrics, (sizeof(json_metrics)/sizeof(json_metrics[0])));
} 

void refresh_version(char* local_path)
{
    char    json_query[BUFSIZ] = "";
    pmInDom indom = PM_INDOM_NULL;
    sprintf(json_query, "http://localhost/version");
    grab_values(json_query, indom, local_path, version_metrics,(sizeof(version_metrics)/sizeof(version_metrics[0])));
}
void refresh_stats(char* local_path)
{
    char    json_query[BUFSIZ] = "";
    pmInDom indom = INDOM(CONTAINERS_STATS_CACHE_INDOM);
    /* we need to add the ?stream=0 bit as to not continuously request stats*/
    sprintf(json_query, "http://localhost/containers/%s/stats?stream=0", local_path);
    grab_values(json_query, indom, local_path, stats_metrics, (sizeof(stats_metrics)/sizeof(stats_metrics[0])));
}
/*
void refresh_syswide(char* local_path)
{
    char json_query[BUFSIZ] = "";
    pmInDom indom = PM_INDOM_NULL
    __pmNotifyErr(LOG_DEBUG, "hit syswide debug\n");
    sprintf(json_query, "http://localhost/info");
    grab_values(json_query, indom, local_path, syswide_metrics, (sizeof(syswide_metrics)/sizeof(syswide_metrics[0]);
    return;
}

void refresh_syswide(char* local_path)
{
    char json_query[BUFSIZ] = "";
    pmInDom indom = PM_INDOM_NULL
    __pmNotifyErr(LOG_DEBUG, "hit syswide debug\n");
    sprintf(json_query, "http://localhost/info");
    grab_values(json_query, indom, local_path, syswide_metrics, (sizeof(syswide_metrics)/sizeof(syswide_metrics[0]);
    return;
}

*/

void*
docker_background_loop(void* loop)
{
    int local_freq;
    while (1) {
	pthread_mutex_lock(&refresh_mutex);
	local_freq = thread_freq;
	pthread_mutex_unlock(&refresh_mutex);
	sleep(local_freq);
	refresh_insts(resulting_path);
	if (!loop)
	    exit(0);
    }
}

static int
docker_store(pmResult *result, pmdaExt *pmda)
{
    int i = 0;
    int sts = 0;
    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;
 
	if (idp->cluster != CLUSTER_CONTROL) {
	    return PM_ERR_PMID;
	}
	switch (idp->item) {
	case 0:
	    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0], PM_TYPE_U64, &av, PM_TYPE_U64)) < 0)
		return PM_ERR_VALUE;
	    pthread_mutex_lock(&refresh_mutex);
	    thread_freq = av.ull;
	    pthread_mutex_unlock(&refresh_mutex);
	    continue;
	default:
	    return PM_ERR_PMID;
	}
    }
	return sts;
}
/*
 * callback provided to pmdaFetch
 */
static int
docker_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int	     *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int               sts = 0;
    char             *name;
    json_metric_desc *local_json_metrics = NULL;
    pmInDom           indom;
    pthread_mutex_lock(&stats_mutex);
    switch (idp->cluster) {
    case CLUSTER_BASIC:

	indom = INDOM(CONTAINERS_INDOM);
	if (inst != PM_INDOM_NULL) {
	    local_json_metrics = NULL;
	    sts = pmdaCacheLookup(indom, inst, &name, (void**)&local_json_metrics);
	    if (sts < 0){
		sts = PM_ERR_INDOM;
		break;
	    }
	}
	switch (idp->item) {
	case 0:		/* docker.pid */
	    atom->ll = local_json_metrics[idp->item].values.l;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 1:             /* docker.name */
	    atom->cp = *local_json_metrics[idp->item].values.cp == '/' ? local_json_metrics[idp->item].values.cp+1 : local_json_metrics[idp->item].values.cp;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 2:             /* docker.running */
	    atom->ul = (local_json_metrics[idp->item].values.ll & CONTAINER_FLAG_RUNNING) != 0;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 3:                /* docker.paused */
	    atom->ul = local_json_metrics[idp->item].values.ll;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 4:                /* docker.restarting */
	    atom->ul = local_json_metrics[idp->item].values.ll;
	    sts = PMDA_FETCH_STATIC;
	    break;
	default:
	    sts = PM_ERR_PMID;
	    break;
	}
	break;
    case CLUSTER_VERSION:
	switch (idp->item) {
	case 0:               /* docker.version */
	case 1:               /* docker.os */
	case 2:               /* docker.kernel */
	case 3:               /* docker.go version */
	case 4:               /* docker.commit */
	case 5:               /* docker.arch */
	case 6:               /* docker.apiversion */
	case 7:               /* docker.buildversion */
	    atom->cp = version_metrics[idp->item].values.cp;
	    sts = PMDA_FETCH_STATIC;
	    break;
	default:
	    sts = PM_ERR_PMID;
	    break;
	}
	break;
    case CLUSTER_STATS:
	indom = INDOM(CONTAINERS_STATS_INDOM);
	if (inst != PM_INDOM_NULL) {
	    local_json_metrics = NULL;
	    sts = pmdaCacheLookup(indom, inst, &name, (void**)&local_json_metrics);
	    if (sts < 0){
		sts = PM_ERR_INDOM;
		break;
	    }
	}
	if (idp->item <= 48) {
	    atom->ull = local_json_metrics[idp->item].values.ull;
	    sts = PMDA_FETCH_STATIC;
	}
	else {
	    sts = PM_ERR_PMID;
	}
	break;
    case CLUSTER_CONTROL:
	switch (idp->item){
	case 0:
	    pthread_mutex_lock(&refresh_mutex);
	    atom->ull = thread_freq;
	    pthread_mutex_unlock(&refresh_mutex);
	    sts = PMDA_FETCH_STATIC;
	    break;
	default:
	    sts = PM_ERR_PMID;
	    break;
	}
	break;
    default:
	sts = PM_ERR_PMID;
	break;
    }
    pthread_mutex_unlock(&stats_mutex);
    return sts;
 
}

static int
notready()
{
    int local_ready;
    int iterations = 0;
    while (1) {
	pthread_mutex_lock(&docker_mutex);
	local_ready = ready;
	pthread_mutex_unlock(&docker_mutex);
	if (local_ready)
	    break;
	else
	    sleep(1);
	if (iterations++ > 30) { /* Complain every 30 seconds. */
	    __pmNotifyErr(LOG_WARNING, "notready waited too long");
	    iterations = 0; /* XXX: or exit? */
	}
    }
    return PM_ERR_PMDAREADY;
}

static int
docker_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{

    int local_ready;
    pthread_mutex_lock(&docker_mutex);
    local_ready = ready;
    pthread_mutex_unlock(&docker_mutex);
    if (!local_ready) {
    	__pmSendError(pmda->e_outfd, FROM_ANON, PM_ERR_PMDANOTREADY);
	return notready();
    }
    return pmdaFetch(numpmid, pmidlist, resp, pmda); 
}

static int
docker_instance(pmInDom id, int i, char *name, __pmInResult **in, pmdaExt *pmda)
{
    int local_ready;
    pthread_mutex_lock(&docker_mutex);
    local_ready = ready;
    pthread_mutex_unlock(&docker_mutex);
    if (!local_ready) {
    	__pmSendError(pmda->e_outfd, FROM_ANON, PM_ERR_PMDANOTREADY);
	return notready();
    }
    return pmdaInstance(id, i, name, in, pmda);
}

void
buffer_callback (char *buffer, int *buffer_size, void *extra)
{
    if (extra == NULL){
	return;
    }
    *buffer_size = strlen((char*)extra);
    strncpy(buffer, (char*)extra, *buffer_size);
    return;
		      
}

int grab_values(char* json_query, pmInDom indom, char* local_path, json_metric_desc* json, int json_size)
{
    int               len, sts, i;
    char              res[BUFSIZ] = "";
    char             *url = "unix://var/run/docker.sock";
    json_metric_desc *local_json_metrics = NULL;

    len = pmhttpClientFetch(http_client, url, res, sizeof(res), json_query, strlen(json_query));

    if (len < 0) {
	if (pmDebug & DBG_TRACE_APPL1)
	    __pmNotifyErr(LOG_ERR, "HTTP fetch (stats) failed\n");
	return 0; // failed
    }
    pthread_mutex_lock(&docker_mutex);

    if (indom != PM_INDOM_NULL)
	sts = pmdaCacheLookupName(indom, local_path, NULL, (void **)&local_json_metrics);
    else
	sts = 0;
    /* allocate space for values for this container and update indom */
    if (sts != PMDA_CACHE_INACTIVE) {
	if (pmDebug & DBG_TRACE_ATTR) {
	    fprintf(stderr, "%s: adding docker container %s\n",
		    pmProgname, local_path);
	}
	if ((local_json_metrics = calloc(json_size, sizeof(json_metric_desc))) == NULL){
	    if (pmDebug & DBG_TRACE_ATTR) {
		fprintf(stderr, "%s: could not alloc more space for container %s\n",
			pmProgname, local_path);
	    }
	    return 0;
	}
    }
    memcpy(local_json_metrics, json, (sizeof(json_metric_desc)*json_size));
    for (i = 0; i < json_size; i++)
	local_json_metrics[i].json_pointer = strdup(json[i].json_pointer);

    local_json_metrics[0].dom = strdup(local_path);
    sts = pmjsonInitIterable(local_json_metrics, json_size, indom, &buffer_callback, res);

    if (indom == PM_INDOM_NULL)
	memcpy(json, local_json_metrics, (sizeof(json_metric_desc)*json_size));
    else
	sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, local_path, (void*)local_json_metrics);
    
    pthread_mutex_unlock(&docker_mutex);
    return sts;
}

static int
check_docker_dir(char *path)
{
    static int		lasterrno;
    static struct stat	lastsbuf;
    struct stat		statbuf;
    pmInDom             stats_cache;

    stats_cache = INDOM(CONTAINERS_STATS_CACHE_INDOM);
    if (stat(path, &statbuf) != 0) {
	if (oserror() == lasterrno) {
	    return 0;
	}
	lasterrno = oserror();
    }
    lasterrno = 0;
    if (stat_time_differs(&statbuf, &lastsbuf)) {
	lastsbuf = statbuf;
	pthread_mutex_lock(&docker_mutex);
	pmdaCacheOp(stats_cache, PMDA_CACHE_INACTIVE);
	pthread_mutex_unlock(&docker_mutex);
	return 1;
    }
    return 0;
}

void update_stats_cache(int mark_inactive)
{
    char             *name;
    int               sts = 0;
    json_metric_desc *local_json_metrics = NULL;
    pmInDom           stats, stats_cache;
    stats = INDOM(CONTAINERS_STATS_INDOM);
    stats_cache = INDOM(CONTAINERS_STATS_CACHE_INDOM);

    pthread_mutex_lock(&docker_mutex);
    pthread_mutex_lock(&stats_mutex);
    if (mark_inactive)
	pmdaCacheOp(stats, PMDA_CACHE_INACTIVE);
    for (sts = pmdaCacheOp(stats_cache, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(stats_cache, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if ((pmdaCacheLookup(stats_cache, sts, &name, (void **)&local_json_metrics) < 0) || !local_json_metrics)
	    continue;
	pmdaCacheStore(stats, PMDA_CACHE_ADD, name, (void *)local_json_metrics);
    }
    pthread_mutex_unlock(&stats_mutex);
    pthread_mutex_unlock(&docker_mutex);
}

void refresh_insts(char *path)
{
    DIR               *rundir;
    struct dirent     *drp;
    int               dir_changed;
    int               active_containers = 0;

    dir_changed = check_docker_dir(path);

    if ((rundir = opendir(path)) == NULL) {
	if (pmDebug & DBG_TRACE_ATTR)
	    fprintf(stderr, "%s: skipping docker path %s\n",
		    pmProgname, path);
	return;
    }
    refresh_version(path);
    while ((drp = readdir(rundir)) != NULL) {
	if (*(local_path = &drp->d_name[0]) == '.') {
	    if (pmDebug & DBG_TRACE_ATTR)
		__pmNotifyErr(LOG_DEBUG, "%s: skipping %s\n",
			      pmProgname, drp->d_name);
	    continue;
	}
	refresh_basic(local_path);
	refresh_stats(local_path);
	active_containers = 1;
    }
    closedir(rundir);

    update_stats_cache((dir_changed && active_containers));

    pthread_mutex_lock(&docker_mutex);
    ready = 1;
    pthread_mutex_unlock(&docker_mutex);
    return;
}

int
docker_setup()
{
    static const char *docker_default = "/var/lib/docker";
    const char        *docker = getenv("PCP_DOCKER_DIR");

    if (!docker)
	docker = docker_default;
    snprintf(resulting_path, sizeof(mypath), "%s/containers", docker);
    resulting_path[sizeof(resulting_path)-1] = '\0';
    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
docker_init(pmdaInterface *dp)
{
    int         i, sts;
    int        *loop = (int*)1;
    if (isDSO) {
	int sep = __pmPathSeparator();
	snprintf(mypath, sizeof(mypath), "%s%c" "docker" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_6, "docker DSO", mypath);
    } else {
	__pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;
    
    if ((http_client = pmhttpNewClient()) == NULL) {
	__pmNotifyErr(LOG_ERR, "HTTP client creation failed\n");
	exit(1);
    }

    pthread_mutex_init(&docker_mutex, NULL);
    pthread_mutex_init(&refresh_mutex, NULL);
    pthread_mutex_init(&stats_mutex, NULL);
    dp->version.any.fetch = docker_fetch;
    dp->version.any.instance = docker_instance;
    dp->version.any.store = docker_store;
    pmdaSetFetchCallBack(dp, docker_fetchCallBack);
    docker_indomtab[CONTAINERS_INDOM].it_indom = CONTAINERS_INDOM;
    docker_indomtab[CONTAINERS_STATS_INDOM].it_indom = CONTAINERS_STATS_INDOM;
    docker_indomtab[CONTAINERS_STATS_CACHE_INDOM].it_indom = CONTAINERS_STATS_CACHE_INDOM;
    pmdaInit(dp, docker_indomtab, INDOMTAB_SZ, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));
    for (i = 0; i < NUM_INDOMS; i++)
	pmdaCacheOp(INDOM(i), PMDA_CACHE_CULL);
	
    docker_setup();
    sts = pthread_create(&docker_query_thread, NULL, docker_background_loop, loop);
    if (sts != 0) {
	__pmNotifyErr(LOG_DEBUG, "docker_init: Cannot spawn new thread: %d\n", sts);
	dp->status = sts;
    }
    else
	__pmNotifyErr(LOG_DEBUG, "docker_init: properly spawned new thread");
    
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
    .short_options = "CD:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    pmdaInterface	dispatch;
    int                 qaflag = 0;
    int                 c, err = 0;

    isDSO = 0;

    snprintf(mypath, sizeof(mypath), "%s%c" "docker" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    if(!isDSO)
	pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, DOCKER,
		   "docker.log", mypath);
    while (( c = pmdaGetOpt(argc, argv, "CD:d:l:U:?", &dispatch, &err)) != EOF){
	switch (c){
	case 'C':
	    qaflag++;
	    break;
	case 'U':
	    username = optarg;
	    break;
	case 'l':
	    printf("hit l option");
	    break;
	default:
	    err++;
	}

    }

    if (err) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    
    if (qaflag){
	docker_background_loop(0);
	exit(0);
    }
    pmdaOpenLog(&dispatch);
    docker_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
