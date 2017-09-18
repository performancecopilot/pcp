/*
 * Docker PMDA
 *
 * Copyright (c) 2016-2017 Red Hat.
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
    CLUSTER_BASIC,
    CLUSTER_VERSION,
    CLUSTER_STATS,
    CLUSTER_CONTROL,
    NUM_CLUSTERS
};

#define ARRAY_SIZE(array)	(sizeof(array) / sizeof(array[0]))

static pmdaIndom docker_indomtab[NUM_INDOMS];
#define INDOM(x) (docker_indomtab[x].it_indom)
#define INDOMTAB_SZ ARRAY_SIZE(docker_indomtab)

static json_metric_desc basic_metrics[] = {
    /* GET localhost/containers/$ID/json */
    { "State/Pid", 0, 1, {0}, ""},
    { "Name", 0, 1, {0}, ""},
    { "State/Running", CONTAINER_FLAG_RUNNING, 1, {0}, ""},
    { "State/Paused", CONTAINER_FLAG_PAUSED, 1, {0}, ""},
    { "State/Restarting", CONTAINER_FLAG_RESTARTING, 1, {0}, ""},
};
#define basic_metrics_size 	ARRAY_SIZE(basic_metrics)

static json_metric_desc version_metrics[] = {
    /* GET /version */
    { "Version", 0, 1, {0}, ""},
    { "Os", 0, 1, {0}, ""},
    { "KernelVersion", 0, 1, {0}, ""},
    { "GoVersion", 0, 1, {0}, ""},
    { "GitCommit", 0, 1, {0}, ""},
    { "Arch", 0, 1, {0}, ""},
    { "ApiVersion", 0, 1, {0}, ""}
};
#define version_metrics_size	ARRAY_SIZE(version_metrics)

static json_metric_desc stats_metrics[] = {
    { "cpu_stats/cpu_usage/total_usage", 0, 1, {0}, ""},
    { "cpu_stats/cpu_usage/usage_in_kernelmode", 0, 1, {0}, ""},
    { "cpu_stats/cpu_usage/usage_in_usermode", 0, 1, {0}, ""},
    { "cpu_stats/system_cpu_usage", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/periods", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/throttled_periods", 0, 1, {0}, ""},
    { "cpu_stats/trottling_data/throttled_time", 0, 1, {0}, ""},
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
#define stats_metrics_size 	ARRAY_SIZE(stats_metrics)

static pmdaMetric metrictab[] = {
    /* docker.pid */
    { (void*) &basic_metrics[0], 
      { PMDA_PMID(CLUSTER_BASIC,0), PM_TYPE_U64, CONTAINERS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.name */
    { (void*) &basic_metrics[1], 
      { PMDA_PMID(CLUSTER_BASIC,1), PM_TYPE_STRING, CONTAINERS_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.running */
    { (void*) &basic_metrics[2], 
      { PMDA_PMID(CLUSTER_BASIC,2), PM_TYPE_U32, CONTAINERS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.paused */
    { (void*) &basic_metrics[3], 
      { PMDA_PMID(CLUSTER_BASIC,3), PM_TYPE_U32, CONTAINERS_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    /* docker.restarting */
    { (void*) &basic_metrics[4], 
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
#define METRICTAB_SZ	ARRAY_SIZE(metrictab)

static int		isDSO = 1;		/* =0 I am a daemon */
static char		*username =  "root";
static char		mypath[MAXPATHLEN];
static char		*local_path;
static int		thread_freq = 1;
static struct http_client *http_client;
static char		resulting_path[MAXPATHLEN];
static int		ready;

static pthread_t	docker_query_thread;	/* all docker.sock queries */
static pthread_mutex_t	refresh_mutex;
static pthread_mutex_t	docker_mutex;
static pthread_mutex_t	stats_mutex;

static void refresh_insts(char *);
static int grab_values(char *, pmInDom, char *, json_metric_desc *, int);
static int docker_setup(void);

/* General utility routine for checking timestamp differences */
static int
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

static void
refresh_basic(char *path)
{
    char    json_query[BUFSIZ];
    pmInDom indom = INDOM(CONTAINERS_INDOM);

    pmsprintf(json_query, BUFSIZ, "http://localhost/containers/%s/json", path);
    grab_values(json_query, indom, path, basic_metrics, basic_metrics_size);
} 

static void
refresh_version(char *path)
{
    char    json_query[BUFSIZ];
    pmInDom indom = PM_INDOM_NULL;

    pmsprintf(json_query, BUFSIZ, "http://localhost/version");
    grab_values(json_query, indom, path, version_metrics, version_metrics_size);
}

static void
refresh_stats(char *path)
{
    char    json_query[BUFSIZ];
    pmInDom indom = INDOM(CONTAINERS_STATS_CACHE_INDOM);

    /* the ?stream=0 bit is set so as to not continuously request stats */
    pmsprintf(json_query, BUFSIZ, "http://localhost/containers/%s/stats?stream=0", path);
    grab_values(json_query, indom, path, stats_metrics, stats_metrics_size);
}

static void *
docker_background_loop(void *loop)
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
    int i;

    for (i = 0; i < result->numpmid; i++) {
	pmValueSet *vsp = result->vset[i];
	__pmID_int *idp = (__pmID_int *)&(vsp->pmid);
	pmAtomValue av;
 
	if (idp->cluster != CLUSTER_CONTROL)
	    return PM_ERR_PMID;

	switch (idp->item) {
	case 0:
	    if (pmExtractValue(vsp->valfmt, &vsp->vlist[0], PM_TYPE_U64, &av, PM_TYPE_U64) < 0)
		return PM_ERR_VALUE;
	    pthread_mutex_lock(&refresh_mutex);
	    thread_freq = av.ull;
	    pthread_mutex_unlock(&refresh_mutex);
	    continue;
	default:
	    return PM_ERR_PMID;
	}
    }
    return 0;
}

/*
 * callback provided to pmdaFetch
 */
static int
docker_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    int			sts = 0;
    char		*name;
    json_metric_desc	*local_metrics;
    pmInDom		indom;

    pthread_mutex_lock(&stats_mutex);

    switch (idp->cluster) {
    case CLUSTER_BASIC:

	indom = INDOM(CONTAINERS_INDOM);
	if (inst != PM_INDOM_NULL) {
	    local_metrics = NULL;
	    sts = pmdaCacheLookup(indom, inst, &name, (void**)&local_metrics);
	    if (sts < 0 || !local_metrics) {
		sts = PM_ERR_INDOM;
		break;
	    }
	}
	switch (idp->item) {
	case 0:		/* docker.pid */
	    atom->ll = local_metrics[idp->item].values.l;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 1:             /* docker.name */
	    atom->cp = *local_metrics[idp->item].values.cp == '/' ?
			local_metrics[idp->item].values.cp + 1 :
			local_metrics[idp->item].values.cp;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 2:             /* docker.running */
	    atom->ul = (local_metrics[idp->item].values.ll & CONTAINER_FLAG_RUNNING) != 0;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 3:                /* docker.paused */
	    atom->ul = local_metrics[idp->item].values.ll;
	    sts = PMDA_FETCH_STATIC;
	    break;
	case 4:                /* docker.restarting */
	    atom->ul = local_metrics[idp->item].values.ll;
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
	    if ((atom->cp = version_metrics[idp->item].values.cp) == NULL)
		sts = PMDA_FETCH_NOVALUES;
	    else
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
	    local_metrics = NULL;
	    sts = pmdaCacheLookup(indom, inst, &name, (void**)&local_metrics);
	    if (sts < 0 || !local_metrics) {
		sts = PM_ERR_INDOM;
		break;
	    }
	}
	if (idp->item <= 48) {
	    atom->ull = local_metrics[idp->item].values.ull;
	    sts = PMDA_FETCH_STATIC;
	}
	else {
	    sts = PM_ERR_PMID;
	}
	break;

    case CLUSTER_CONTROL:
	switch (idp->item) {
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
notready(void)
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

typedef struct {
    char	json[BUFSIZ];
    int		json_len;
    int		off;
} http_data;

static int
grab_json(char *buffer, int buffer_size, void *data)
{
    http_data	*hp = (http_data *)data;
    size_t	bytes;

    /* have we reached the end of the json string? */
    if (hp->off >= hp->json_len)
	return 0;

    /* can we fill the entire buffer (or more)? */
    if (hp->off + buffer_size <= hp->json_len)
	bytes = buffer_size;
    else
	bytes = hp->json_len - hp->off;

    memcpy(buffer, hp->json + hp->off, bytes);
    hp->off += bytes;
    return bytes;
}

static int
grab_values(char *json_query, pmInDom indom, char *local_path, json_metric_desc *json, int json_size)
{
    int			sts, i;
    http_data		http_data;
    json_metric_desc	*local_metrics;

    if ((sts = pmhttpClientFetch(http_client, "unix://var/run/docker.sock",
			&http_data.json[0], sizeof(http_data.json),
			json_query, strlen(json_query))) < 0) {
	if (pmDebugOptions.appl1)
	    __pmNotifyErr(LOG_ERR, "HTTP fetch (stats) failed\n");
	return 0; // failed
    }
    http_data.json_len = strlen(http_data.json);
    http_data.off = 0;

    pthread_mutex_lock(&docker_mutex);

    sts = (indom == PM_INDOM_NULL) ? 0 :
	pmdaCacheLookupName(indom, local_path, NULL, (void **)&local_metrics);

    /* allocate space for values for this container and update indom */
    if (sts != PMDA_CACHE_INACTIVE && sts != PMDA_CACHE_ACTIVE) {
	if (pmDebugOptions.attr) {
	    fprintf(stderr, "%s: adding docker container %s\n",
		    pmProgname, local_path);
	}
	if (!(local_metrics = calloc(json_size, sizeof(json_metric_desc)))) {
	    if (pmDebugOptions.attr) {
		fprintf(stderr, "%s: cannot allocate container %s space\n",
			pmProgname, local_path);
	    }
	    return 0;
	}
    }
    memcpy(local_metrics, json, (sizeof(json_metric_desc)*json_size));
    for (i = 0; i < json_size; i++)
	local_metrics[i].json_pointer = strdup(json[i].json_pointer);
    local_metrics[0].dom = strdup(local_path);

    if ((sts = pmjsonGet(local_metrics, json_size, indom, &grab_json,
			 (void *)&http_data)) < 0)
	goto unlock;

    if (indom == PM_INDOM_NULL)
	memcpy(json, local_metrics, (sizeof(json_metric_desc) * json_size));
    else
	sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, local_path, (void *)local_metrics);
    
unlock:
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
	if (oserror() == lasterrno)
	    return 0;
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

static void
update_stats_cache(int mark_inactive)
{
    char		*name;
    int			inst;
    json_metric_desc	*local_metrics;
    pmInDom		stats, stats_cache;

    stats = INDOM(CONTAINERS_STATS_INDOM);
    stats_cache = INDOM(CONTAINERS_STATS_CACHE_INDOM);

    pthread_mutex_lock(&docker_mutex);
    pthread_mutex_lock(&stats_mutex);
    if (mark_inactive)
	pmdaCacheOp(stats, PMDA_CACHE_INACTIVE);
    for (pmdaCacheOp(stats_cache, PMDA_CACHE_WALK_REWIND);;) {
	if ((inst = pmdaCacheOp(stats_cache, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	local_metrics = NULL;
	if ((pmdaCacheLookup(stats_cache, inst, &name, (void **)&local_metrics) < 0)
	    || !local_metrics)
	    continue;
	pmdaCacheStore(stats, PMDA_CACHE_ADD, name, (void *)local_metrics);
    }
    pthread_mutex_unlock(&stats_mutex);
    pthread_mutex_unlock(&docker_mutex);
}

static void
refresh_insts(char *path)
{
    DIR               *rundir;
    struct dirent     *drp;
    int               dir_changed;
    int               active_containers = 0;

    dir_changed = check_docker_dir(path);

    if ((rundir = opendir(path)) == NULL) {
	if (pmDebugOptions.attr)
	    fprintf(stderr, "%s: skipping docker path %s\n",
		    pmProgname, path);
	return;
    }
    refresh_version(path);
    while ((drp = readdir(rundir)) != NULL) {
	if (*(local_path = &drp->d_name[0]) == '.') {
	    if (pmDebugOptions.attr)
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

static int
docker_setup(void)
{
    static const char *docker_default = "/var/lib/docker";
    const char        *docker = getenv("PCP_DOCKER_DIR");

    if (!docker)
	docker = docker_default;
    pmsprintf(resulting_path, sizeof(mypath), "%s/containers", docker);
    resulting_path[sizeof(resulting_path)-1] = '\0';
    return 0;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
docker_init(pmdaInterface *dp)
{
    int         i, sts;
    int        *loop = (int*)1;
    if (isDSO) {
	int sep = __pmPathSeparator();
	pmsprintf(mypath, sizeof(mypath), "%s%c" "docker" "%c" "help",
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
    pmdaInit(dp, docker_indomtab, INDOMTAB_SZ, metrictab, METRICTAB_SZ);
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
    int			qaflag = 0;
    int			c, err = 0;

    isDSO = 0;

    pmsprintf(mypath, sizeof(mypath), "%s%c" "docker" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_6, pmProgname, DOCKER,
		"docker.log", mypath);

    while ((c = pmdaGetOpt(argc, argv, "CD:d:l:U:?", &dispatch, &err)) != EOF) {
	switch (c) {
	case 'C':
	    qaflag++;
	    break;
	case 'U':
	    username = optarg;
	    break;
	default:
	    err++;
	}

    }

    if (err) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    
    if (qaflag) {
	docker_setup();
	docker_background_loop(0);
	exit(0);
    }
    pmdaOpenLog(&dispatch);
    docker_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
