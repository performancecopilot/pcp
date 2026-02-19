/*
 * Valkey PMDA
 *
 * Copyright (c) 2026 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <valkey/valkey.h>
#include <ctype.h>
#include <string.h>

/*
 * Valkey PMDA
 *
 * Metrics are obtained by connecting to a Valkey server and issuing
 * the INFO command. The response is parsed and cached for a short
 * period to avoid overwhelming the server with requests.
 */

/* Configuration */
#define VALKEY_DEFAULT_HOST	"127.0.0.1"
#define VALKEY_DEFAULT_PORT	6379
#define VALKEY_TIMEOUT_SEC	1
#define VALKEY_TIMEOUT_USEC	500000
#define VALKEY_CACHE_TIMEOUT	2	/* seconds */

/* Metric clusters */
enum {
	CLUSTER_SERVER = 0,
	CLUSTER_CLIENTS,
	CLUSTER_MEMORY,
	CLUSTER_STATS,
	CLUSTER_CPU,
	CLUSTER_PERSISTENCE,
	CLUSTER_REPLICATION,
	CLUSTER_KEYSPACE,
	CLUSTER_ERRORSTATS,
	CLUSTER_MODULES,
	CLUSTER_EVENTLOOP
};

/* Instance domains */
enum {
	INDOM_KEYSPACE = 0,	/* per-database instances (db0, db1, ...) */
	INDOM_ERRORSTATS,	/* per-error-type instances */
	INDOM_MODULES		/* per-module instances */
};

/* Server cluster items */
enum {
	ITEM_UPTIME_SECONDS = 0,
	ITEM_PROCESS_ID,
	ITEM_UPTIME_DAYS,
	ITEM_HZ,
	ITEM_CONFIG_FILE,
	ITEM_VALKEY_VERSION,
	ITEM_SERVER_NAME,
	ITEM_TCP_PORT,
	ITEM_SERVER_TIME_USEC,
	ITEM_ARCH_BITS,
	ITEM_MULTIPLEXING_API,
	ITEM_RUN_ID,
	ITEM_IO_THREADS_ACTIVE
};

/* Client cluster items */
enum {
	ITEM_CONNECTED_CLIENTS = 0,
	ITEM_BLOCKED_CLIENTS,
	ITEM_TRACKING_CLIENTS,
	ITEM_PUBSUB_CLIENTS,
	ITEM_MAXCLIENTS,
	ITEM_CLUSTER_CONNECTIONS,
	ITEM_WATCHING_CLIENTS,
	ITEM_CLIENTS_IN_TIMEOUT_TABLE,
	ITEM_CLIENT_RECENT_MAX_INPUT_BUFFER,
	ITEM_CLIENT_RECENT_MAX_OUTPUT_BUFFER
};

/* Memory cluster items */
enum {
	ITEM_USED_MEMORY = 0,
	ITEM_USED_MEMORY_RSS,
	ITEM_USED_MEMORY_PEAK,
	ITEM_MEM_FRAGMENTATION,
	ITEM_USED_MEMORY_OVERHEAD,
	ITEM_USED_MEMORY_DATASET,
	ITEM_TOTAL_SYSTEM_MEMORY,
	ITEM_MAXMEMORY,
	ITEM_USED_MEMORY_PEAK_PERC,
	ITEM_USED_MEMORY_STARTUP,
	ITEM_USED_MEMORY_SCRIPTS,
	ITEM_ALLOCATOR_ALLOCATED,
	ITEM_ALLOCATOR_ACTIVE,
	ITEM_ALLOCATOR_RESIDENT,
	ITEM_ALLOCATOR_FRAG_RATIO,
	ITEM_RSS_OVERHEAD_RATIO,
	ITEM_MAXMEMORY_POLICY,
	ITEM_MEM_CLIENTS_NORMAL,
	ITEM_LAZYFREE_PENDING_OBJECTS,
	ITEM_LAZYFREED_OBJECTS
};

/* Stats cluster items */
enum {
	ITEM_TOTAL_COMMANDS = 0,
	ITEM_TOTAL_CONNECTIONS,
	ITEM_REJECTED_CONN,
	ITEM_KEYSPACE_HITS,
	ITEM_KEYSPACE_MISSES,
	ITEM_EVICTED_KEYS,
	ITEM_EXPIRED_KEYS,
	ITEM_INSTANTANEOUS_OPS,
	ITEM_TOTAL_NET_INPUT,
	ITEM_TOTAL_NET_OUTPUT,
	ITEM_TOTAL_READS_PROCESSED,
	ITEM_TOTAL_WRITES_PROCESSED,
	ITEM_TOTAL_ERROR_REPLIES,
	ITEM_SYNC_FULL,
	ITEM_SYNC_PARTIAL_OK,
	ITEM_SYNC_PARTIAL_ERR,
	ITEM_TOTAL_FORKS,
	ITEM_LATEST_FORK_USEC,
	ITEM_INSTANTANEOUS_INPUT_KBPS,
	ITEM_INSTANTANEOUS_OUTPUT_KBPS,
	ITEM_INSTANTANEOUS_INPUT_REPL_KBPS,
	ITEM_INSTANTANEOUS_OUTPUT_REPL_KBPS,
	ITEM_CLIENT_QUERY_BUFFER_LIMIT_DISCONNECTIONS,
	ITEM_CLIENT_OUTPUT_BUFFER_LIMIT_DISCONNECTIONS,
	ITEM_REPLY_BUFFER_SHRINKS,
	ITEM_REPLY_BUFFER_EXPANDS,
	ITEM_ACL_ACCESS_DENIED_AUTH,
	ITEM_ACL_ACCESS_DENIED_CMD,
	ITEM_ACL_ACCESS_DENIED_KEY,
	ITEM_ACL_ACCESS_DENIED_CHANNEL
};

/* CPU cluster items */
enum {
	ITEM_USED_CPU_SYS = 0,
	ITEM_USED_CPU_USER,
	ITEM_USED_CPU_SYS_CHILDREN,
	ITEM_USED_CPU_USER_CHILDREN,
	ITEM_USED_CPU_SYS_MAIN_THREAD,
	ITEM_USED_CPU_USER_MAIN_THREAD
};

/* Persistence cluster items */
enum {
	ITEM_RDB_CHANGES_SINCE_SAVE = 0,
	ITEM_RDB_LAST_SAVE_TIME,
	ITEM_RDB_LAST_BGSAVE_STATUS,
	ITEM_RDB_LAST_BGSAVE_TIME,
	ITEM_AOF_ENABLED,
	ITEM_LOADING,
	ITEM_RDB_BGSAVE_IN_PROGRESS,
	ITEM_RDB_SAVES,
	ITEM_RDB_LAST_COW_SIZE,
	ITEM_RDB_CURRENT_BGSAVE_TIME_SEC,
	ITEM_AOF_REWRITE_IN_PROGRESS,
	ITEM_AOF_REWRITES,
	ITEM_CURRENT_COW_SIZE,
	ITEM_CURRENT_COW_PEAK
};

/* Replication cluster items */
enum {
	ITEM_ROLE = 0,
	ITEM_CONNECTED_SLAVES,
	ITEM_MASTER_REPL_OFFSET,
	ITEM_REPL_BACKLOG_ACTIVE,
	ITEM_REPL_BACKLOG_SIZE,
	ITEM_REPL_BACKLOG_HISTLEN,
	ITEM_REPLICAS_WAITING_PSYNC
};

/* Keyspace cluster items */
enum {
	ITEM_KEYSPACE_KEYS = 0,
	ITEM_KEYSPACE_EXPIRES,
	ITEM_KEYSPACE_AVG_TTL
};

/* Errorstats cluster items */
enum {
	ITEM_ERRORSTATS_COUNT = 0
};

/* Modules cluster items */
enum {
	ITEM_MODULES_NAME = 0,
	ITEM_MODULES_VERSION
};

/* Eventloop cluster items */
enum {
	ITEM_EVENTLOOP_CYCLES = 0,
	ITEM_EVENTLOOP_DURATION_SUM,
	ITEM_EVENTLOOP_DURATION_CMD_SUM,
	ITEM_INSTANTANEOUS_EVENTLOOP_CYCLES_PER_SEC,
	ITEM_INSTANTANEOUS_EVENTLOOP_DURATION_USEC
};

/* Maximum instances for each INDOM */
#define MAX_KEYSPACE_INSTANCES	16	/* db0-db15 */
#define MAX_ERRORSTATS_INSTANCES 32	/* error types */
#define MAX_MODULES_INSTANCES	16	/* loaded modules */

/* INDOM instance storage */
static pmdaInstid *keyspace_instances;
static pmdaInstid *errorstats_instances;
static pmdaInstid *modules_instances;
static int num_keyspace = 0;
static int num_errorstats = 0;
static int num_modules = 0;

/* INDOM table */
static pmdaIndom indomtab[] = {
    { INDOM_KEYSPACE, 0, NULL },
    { INDOM_ERRORSTATS, 0, NULL },
    { INDOM_MODULES, 0, NULL }
};

/* Per-instance data structures */
typedef struct {
	uint64_t keys;
	uint64_t expires;
	uint64_t avg_ttl;
} keyspace_data_t;

typedef struct {
	uint64_t count;
} errorstats_data_t;

typedef struct {
	char name[64];
	uint32_t version;
} modules_data_t;

static keyspace_data_t *keyspace_data;
static errorstats_data_t *errorstats_data;
static modules_data_t *modules_data;

static pmdaMetric metrictab[] = {
    /* Server metrics - cluster 0 */
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_UPTIME_SECONDS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_PROCESS_ID), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_UPTIME_DAYS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_HZ), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_CONFIG_FILE), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_VALKEY_VERSION), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_SERVER_NAME), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_TCP_PORT), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_SERVER_TIME_USEC), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_ARCH_BITS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_MULTIPLEXING_API), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_RUN_ID), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_IO_THREADS_ACTIVE), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },

    /* Client metrics - cluster 1 */
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_CONNECTED_CLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_BLOCKED_CLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_TRACKING_CLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_PUBSUB_CLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_MAXCLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_CLUSTER_CONNECTIONS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_WATCHING_CLIENTS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_CLIENTS_IN_TIMEOUT_TABLE), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_CLIENT_RECENT_MAX_INPUT_BUFFER), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CLIENTS, ITEM_CLIENT_RECENT_MAX_OUTPUT_BUFFER), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },

    /* Memory metrics - cluster 2 */
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_RSS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_PEAK), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_MEM_FRAGMENTATION), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_OVERHEAD), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_DATASET), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_TOTAL_SYSTEM_MEMORY), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_MAXMEMORY), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_PEAK_PERC), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_STARTUP), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_USED_MEMORY_SCRIPTS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_ALLOCATOR_ALLOCATED), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_ALLOCATOR_ACTIVE), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_ALLOCATOR_RESIDENT), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_ALLOCATOR_FRAG_RATIO), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_RSS_OVERHEAD_RATIO), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_MAXMEMORY_POLICY), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_MEM_CLIENTS_NORMAL), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_LAZYFREE_PENDING_OBJECTS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MEMORY, ITEM_LAZYFREED_OBJECTS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },

    /* Stats metrics - cluster 3 */
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_COMMANDS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_CONNECTIONS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_REJECTED_CONN), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_KEYSPACE_HITS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_KEYSPACE_MISSES), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_EVICTED_KEYS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_EXPIRED_KEYS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_INSTANTANEOUS_OPS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_NET_INPUT), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_NET_OUTPUT), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_READS_PROCESSED), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_WRITES_PROCESSED), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_ERROR_REPLIES), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_SYNC_FULL), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_SYNC_PARTIAL_OK), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_SYNC_PARTIAL_ERR), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_TOTAL_FORKS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_LATEST_FORK_USEC), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_INSTANTANEOUS_INPUT_KBPS), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_INSTANTANEOUS_OUTPUT_KBPS), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_INSTANTANEOUS_INPUT_REPL_KBPS), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_INSTANTANEOUS_OUTPUT_REPL_KBPS), PM_TYPE_FLOAT, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, -1, 0, PM_SPACE_KBYTE, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_CLIENT_QUERY_BUFFER_LIMIT_DISCONNECTIONS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_CLIENT_OUTPUT_BUFFER_LIMIT_DISCONNECTIONS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_REPLY_BUFFER_SHRINKS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_REPLY_BUFFER_EXPANDS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_ACL_ACCESS_DENIED_AUTH), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_ACL_ACCESS_DENIED_CMD), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_ACL_ACCESS_DENIED_KEY), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_STATS, ITEM_ACL_ACCESS_DENIED_CHANNEL), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },

    /* CPU metrics - cluster 4 */
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_SYS), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_USER), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_SYS_CHILDREN), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_USER_CHILDREN), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_SYS_MAIN_THREAD), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_USER_MAIN_THREAD), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },

    /* Persistence metrics - cluster 5 */
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_CHANGES_SINCE_SAVE), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_LAST_SAVE_TIME), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_LAST_BGSAVE_STATUS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_LAST_BGSAVE_TIME), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_AOF_ENABLED), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_LOADING), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_BGSAVE_IN_PROGRESS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_SAVES), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_LAST_COW_SIZE), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_RDB_CURRENT_BGSAVE_TIME_SEC), PM_TYPE_32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_AOF_REWRITE_IN_PROGRESS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_AOF_REWRITES), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_CURRENT_COW_SIZE), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_PERSISTENCE, ITEM_CURRENT_COW_PEAK), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },

    /* Replication metrics - cluster 6 */
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_ROLE), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_CONNECTED_SLAVES), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_MASTER_REPL_OFFSET), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_REPL_BACKLOG_ACTIVE), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_REPL_BACKLOG_SIZE), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_REPL_BACKLOG_HISTLEN), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_REPLICATION, ITEM_REPLICAS_WAITING_PSYNC), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },

    /* Keyspace metrics - cluster 7 (with INDOM) */
    { NULL, { PMDA_PMID(CLUSTER_KEYSPACE, ITEM_KEYSPACE_KEYS), PM_TYPE_U64, INDOM_KEYSPACE, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_KEYSPACE, ITEM_KEYSPACE_EXPIRES), PM_TYPE_U64, INDOM_KEYSPACE, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_KEYSPACE, ITEM_KEYSPACE_AVG_TTL), PM_TYPE_U64, INDOM_KEYSPACE, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_MSEC, 0) }, },

    /* Errorstats metrics - cluster 8 (with INDOM) */
    { NULL, { PMDA_PMID(CLUSTER_ERRORSTATS, ITEM_ERRORSTATS_COUNT), PM_TYPE_U64, INDOM_ERRORSTATS, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },

    /* Modules metrics - cluster 9 (with INDOM) */
    { NULL, { PMDA_PMID(CLUSTER_MODULES, ITEM_MODULES_NAME), PM_TYPE_STRING, INDOM_MODULES, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_MODULES, ITEM_MODULES_VERSION), PM_TYPE_U32, INDOM_MODULES, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },

    /* Eventloop metrics - cluster 10 */
    { NULL, { PMDA_PMID(CLUSTER_EVENTLOOP, ITEM_EVENTLOOP_CYCLES), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_EVENTLOOP, ITEM_EVENTLOOP_DURATION_SUM), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_EVENTLOOP, ITEM_EVENTLOOP_DURATION_CMD_SUM), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_EVENTLOOP, ITEM_INSTANTANEOUS_EVENTLOOP_CYCLES_PER_SEC), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, -1, 1, 0, PM_TIME_SEC, PM_COUNT_ONE) }, },
    { NULL, { PMDA_PMID(CLUSTER_EVENTLOOP, ITEM_INSTANTANEOUS_EVENTLOOP_DURATION_USEC), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_USEC, 0) }, },
};

/* Valkey connection and cache */
static valkeyContext *vk_conn;
static char *vk_host;
static int vk_port;
static char *cached_info;

/* Cached metric values */
static struct {
    /* Server */
    uint64_t uptime_seconds;
    uint32_t process_id;
    uint32_t uptime_days;
    uint32_t hz;
    char config_file[256];
    char valkey_version[64];
    char server_name[64];
    uint32_t tcp_port;
    uint64_t server_time_usec;
    uint32_t arch_bits;
    char multiplexing_api[64];
    char run_id[64];
    uint32_t io_threads_active;
    /* Clients */
    uint32_t connected_clients;
    uint32_t blocked_clients;
    uint32_t tracking_clients;
    uint32_t pubsub_clients;
    uint32_t maxclients;
    uint32_t cluster_connections;
    uint32_t watching_clients;
    uint32_t clients_in_timeout_table;
    uint64_t client_recent_max_input_buffer;
    uint64_t client_recent_max_output_buffer;
    /* Memory */
    uint64_t used_memory;
    uint64_t used_memory_rss;
    uint64_t used_memory_peak;
    float mem_fragmentation_ratio;
    uint64_t used_memory_overhead;
    uint64_t used_memory_dataset;
    uint64_t total_system_memory;
    uint64_t maxmemory;
    float used_memory_peak_perc;
    uint64_t used_memory_startup;
    uint64_t used_memory_scripts;
    uint64_t allocator_allocated;
    uint64_t allocator_active;
    uint64_t allocator_resident;
    float allocator_frag_ratio;
    float rss_overhead_ratio;
    char maxmemory_policy[64];
    uint64_t mem_clients_normal;
    uint64_t lazyfree_pending_objects;
    uint64_t lazyfreed_objects;
    /* Stats */
    uint64_t total_commands_processed;
    uint64_t total_connections_received;
    uint64_t rejected_connections;
    uint64_t keyspace_hits;
    uint64_t keyspace_misses;
    uint64_t evicted_keys;
    uint64_t expired_keys;
    uint64_t instantaneous_ops_per_sec;
    uint64_t total_net_input_bytes;
    uint64_t total_net_output_bytes;
    uint64_t total_reads_processed;
    uint64_t total_writes_processed;
    uint64_t total_error_replies;
    uint64_t sync_full;
    uint64_t sync_partial_ok;
    uint64_t sync_partial_err;
    uint64_t total_forks;
    uint64_t latest_fork_usec;
    float instantaneous_input_kbps;
    float instantaneous_output_kbps;
    float instantaneous_input_repl_kbps;
    float instantaneous_output_repl_kbps;
    uint64_t client_query_buffer_limit_disconnections;
    uint64_t client_output_buffer_limit_disconnections;
    uint64_t reply_buffer_shrinks;
    uint64_t reply_buffer_expands;
    uint64_t acl_access_denied_auth;
    uint64_t acl_access_denied_cmd;
    uint64_t acl_access_denied_key;
    uint64_t acl_access_denied_channel;
    /* CPU */
    double used_cpu_sys;
    double used_cpu_user;
    double used_cpu_sys_children;
    double used_cpu_user_children;
    double used_cpu_sys_main_thread;
    double used_cpu_user_main_thread;
    /* Persistence */
    uint64_t rdb_changes_since_last_save;
    uint64_t rdb_last_save_time;
    uint32_t rdb_last_bgsave_status;  /* 1=ok, 0=failed */
    int32_t rdb_last_bgsave_time_sec;
    uint32_t aof_enabled;
    uint32_t loading;
    uint32_t rdb_bgsave_in_progress;
    uint64_t rdb_saves;
    uint64_t rdb_last_cow_size;
    int32_t rdb_current_bgsave_time_sec;
    uint32_t aof_rewrite_in_progress;
    uint64_t aof_rewrites;
    uint64_t current_cow_size;
    uint64_t current_cow_peak;
    /* Replication */
    char role[16];
    uint32_t connected_slaves;
    uint64_t master_repl_offset;
    uint32_t repl_backlog_active;
    uint64_t repl_backlog_size;
    uint64_t repl_backlog_histlen;
    uint32_t replicas_waiting_psync;
    /* Eventloop */
    uint64_t eventloop_cycles;
    uint64_t eventloop_duration_sum;
    uint64_t eventloop_duration_cmd_sum;
    uint32_t instantaneous_eventloop_cycles_per_sec;
    uint32_t instantaneous_eventloop_duration_usec;
    int valid;
} vk_stats;

static int isDSO = 1;
static char *username;

/* Forward declarations */
static void valkey_parse_indoms(char *info_str);

/*
 * Parse a line from INFO output: "key:value"
 */
static char *
parse_info_value(const char *line, const char *key)
{
    size_t keylen = strlen(key);
    if (strncmp(line, key, keylen) == 0 && line[keylen] == ':') {
        return (char *)(line + keylen + 1);
    }
    return NULL;
}

/*
 * Connect to Valkey server
 */
static int
valkey_connect(void)
{
    struct timeval timeout = { VALKEY_TIMEOUT_SEC, VALKEY_TIMEOUT_USEC };
    char *host = vk_host? vk_host : VALKEY_DEFAULT_HOST;
    int port = vk_port? vk_port : VALKEY_DEFAULT_PORT;

    if (vk_conn != NULL) {
        valkeyFree(vk_conn);
        vk_conn = NULL;
    }

    vk_conn = valkeyConnectWithTimeout(host, port, timeout);
    if (vk_conn == NULL || vk_conn->err) {
        if (vk_conn) {
            pmNotifyErr(LOG_ERR, "Valkey connection error: %s", vk_conn->errstr);
            valkeyFree(vk_conn);
            vk_conn = NULL;
        } else {
            pmNotifyErr(LOG_ERR, "Valkey connection error: can't allocate context");
        }
        return -1;
    }

    return 0;
}

/*
 * Fetch INFO from Valkey and parse it
 */
static int
valkey_fetch_info(void)
{
    valkeyReply *reply;
    char *line, *saveptr, *value;
    time_t now = time(NULL);
    static time_t last_fetch_time;

    /* Check if cached data is still valid */
    if (vk_stats.valid && (now - last_fetch_time) < VALKEY_CACHE_TIMEOUT)
        return 0;

    /* Ensure we're connected */
    if (vk_conn == NULL || vk_conn->err) {
        if (valkey_connect() < 0)
            return -1;
    }

    /* Execute INFO command */
    reply = valkeyCommand(vk_conn, "INFO");
    if (reply == NULL || vk_conn->err) {
        if (vk_conn->err) {
            pmNotifyErr(LOG_ERR, "Valkey command error: %s", vk_conn->errstr);
        }
        if (reply)
            freeReplyObject(reply);
        valkeyFree(vk_conn);
        vk_conn = NULL;
        vk_stats.valid = 0;
        return -1;
    }

    if (reply->type != VALKEY_REPLY_STRING) {
        pmNotifyErr(LOG_ERR, "Valkey INFO returned unexpected type: %d", reply->type);
        freeReplyObject(reply);
        vk_stats.valid = 0;
        return -1;
    }

    /* Free old cached data */
    if (cached_info != NULL)
        free(cached_info);

    cached_info = strdup(reply->str);
    freeReplyObject(reply);

    if (cached_info == NULL) {
        vk_stats.valid = 0;
        return -1;
    }

    /* Parse the INFO response */
    memset(&vk_stats, 0, sizeof(vk_stats));

    line = strtok_r(cached_info, "\r\n", &saveptr);
    while (line != NULL) {
        /* Skip comments and empty lines */
        if (*line == '#' || *line == '\0') {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        /* Parse key:value pairs - Server */
        if ((value = parse_info_value(line, "uptime_in_seconds")) != NULL)
            vk_stats.uptime_seconds = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "uptime_in_days")) != NULL)
            vk_stats.uptime_days = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "process_id")) != NULL)
            vk_stats.process_id = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "hz")) != NULL)
            vk_stats.hz = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "config_file")) != NULL)
            strncpy(vk_stats.config_file, value, sizeof(vk_stats.config_file) - 1);
        else if ((value = parse_info_value(line, "valkey_version")) != NULL)
            strncpy(vk_stats.valkey_version, value, sizeof(vk_stats.valkey_version) - 1);
        else if ((value = parse_info_value(line, "server_name")) != NULL)
            strncpy(vk_stats.server_name, value, sizeof(vk_stats.server_name) - 1);
        else if ((value = parse_info_value(line, "tcp_port")) != NULL)
            vk_stats.tcp_port = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "server_time_usec")) != NULL)
            vk_stats.server_time_usec = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "arch_bits")) != NULL)
            vk_stats.arch_bits = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "multiplexing_api")) != NULL)
            strncpy(vk_stats.multiplexing_api, value, sizeof(vk_stats.multiplexing_api) - 1);
        else if ((value = parse_info_value(line, "run_id")) != NULL)
            strncpy(vk_stats.run_id, value, sizeof(vk_stats.run_id) - 1);
        else if ((value = parse_info_value(line, "io_threads_active")) != NULL)
            vk_stats.io_threads_active = strtoul(value, NULL, 10);
        /* Clients */
        else if ((value = parse_info_value(line, "connected_clients")) != NULL)
            vk_stats.connected_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "blocked_clients")) != NULL)
            vk_stats.blocked_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "tracking_clients")) != NULL)
            vk_stats.tracking_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "pubsub_clients")) != NULL)
            vk_stats.pubsub_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "maxclients")) != NULL)
            vk_stats.maxclients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "cluster_connections")) != NULL)
            vk_stats.cluster_connections = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "watching_clients")) != NULL)
            vk_stats.watching_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "clients_in_timeout_table")) != NULL)
            vk_stats.clients_in_timeout_table = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "client_recent_max_input_buffer")) != NULL)
            vk_stats.client_recent_max_input_buffer = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "client_recent_max_output_buffer")) != NULL)
            vk_stats.client_recent_max_output_buffer = strtoull(value, NULL, 10);
        /* Memory */
        else if ((value = parse_info_value(line, "used_memory")) != NULL)
            vk_stats.used_memory = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "used_memory_rss")) != NULL)
            vk_stats.used_memory_rss = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "used_memory_peak")) != NULL)
            vk_stats.used_memory_peak = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "mem_fragmentation_ratio")) != NULL)
            vk_stats.mem_fragmentation_ratio = strtof(value, NULL);
        else if ((value = parse_info_value(line, "used_memory_overhead")) != NULL)
            vk_stats.used_memory_overhead = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "used_memory_dataset")) != NULL)
            vk_stats.used_memory_dataset = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_system_memory")) != NULL)
            vk_stats.total_system_memory = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "maxmemory")) != NULL)
            vk_stats.maxmemory = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "used_memory_peak_perc")) != NULL) {
            /* Remove the '%' character if present */
            char *pct = strchr(value, '%');
            if (pct) *pct = '\0';
            vk_stats.used_memory_peak_perc = strtof(value, NULL);
        }
        else if ((value = parse_info_value(line, "used_memory_startup")) != NULL)
            vk_stats.used_memory_startup = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "used_memory_scripts")) != NULL)
            vk_stats.used_memory_scripts = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "allocator_allocated")) != NULL)
            vk_stats.allocator_allocated = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "allocator_active")) != NULL)
            vk_stats.allocator_active = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "allocator_resident")) != NULL)
            vk_stats.allocator_resident = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "allocator_frag_ratio")) != NULL)
            vk_stats.allocator_frag_ratio = strtof(value, NULL);
        else if ((value = parse_info_value(line, "rss_overhead_ratio")) != NULL)
            vk_stats.rss_overhead_ratio = strtof(value, NULL);
        else if ((value = parse_info_value(line, "maxmemory_policy")) != NULL)
            strncpy(vk_stats.maxmemory_policy, value, sizeof(vk_stats.maxmemory_policy) - 1);
        else if ((value = parse_info_value(line, "mem_clients_normal")) != NULL)
            vk_stats.mem_clients_normal = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "lazyfree_pending_objects")) != NULL)
            vk_stats.lazyfree_pending_objects = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "lazyfreed_objects")) != NULL)
            vk_stats.lazyfreed_objects = strtoull(value, NULL, 10);
        /* Stats */
        else if ((value = parse_info_value(line, "total_commands_processed")) != NULL)
            vk_stats.total_commands_processed = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_connections_received")) != NULL)
            vk_stats.total_connections_received = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "rejected_connections")) != NULL)
            vk_stats.rejected_connections = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "keyspace_hits")) != NULL)
            vk_stats.keyspace_hits = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "keyspace_misses")) != NULL)
            vk_stats.keyspace_misses = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "evicted_keys")) != NULL)
            vk_stats.evicted_keys = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "expired_keys")) != NULL)
            vk_stats.expired_keys = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "instantaneous_ops_per_sec")) != NULL)
            vk_stats.instantaneous_ops_per_sec = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_net_input_bytes")) != NULL)
            vk_stats.total_net_input_bytes = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_net_output_bytes")) != NULL)
            vk_stats.total_net_output_bytes = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_reads_processed")) != NULL)
            vk_stats.total_reads_processed = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_writes_processed")) != NULL)
            vk_stats.total_writes_processed = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_error_replies")) != NULL)
            vk_stats.total_error_replies = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "sync_full")) != NULL)
            vk_stats.sync_full = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "sync_partial_ok")) != NULL)
            vk_stats.sync_partial_ok = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "sync_partial_err")) != NULL)
            vk_stats.sync_partial_err = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "total_forks")) != NULL)
            vk_stats.total_forks = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "latest_fork_usec")) != NULL)
            vk_stats.latest_fork_usec = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "instantaneous_input_kbps")) != NULL)
            vk_stats.instantaneous_input_kbps = strtof(value, NULL);
        else if ((value = parse_info_value(line, "instantaneous_output_kbps")) != NULL)
            vk_stats.instantaneous_output_kbps = strtof(value, NULL);
        else if ((value = parse_info_value(line, "instantaneous_input_repl_kbps")) != NULL)
            vk_stats.instantaneous_input_repl_kbps = strtof(value, NULL);
        else if ((value = parse_info_value(line, "instantaneous_output_repl_kbps")) != NULL)
            vk_stats.instantaneous_output_repl_kbps = strtof(value, NULL);
        else if ((value = parse_info_value(line, "client_query_buffer_limit_disconnections")) != NULL)
            vk_stats.client_query_buffer_limit_disconnections = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "client_output_buffer_limit_disconnections")) != NULL)
            vk_stats.client_output_buffer_limit_disconnections = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "reply_buffer_shrinks")) != NULL)
            vk_stats.reply_buffer_shrinks = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "reply_buffer_expands")) != NULL)
            vk_stats.reply_buffer_expands = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "acl_access_denied_auth")) != NULL)
            vk_stats.acl_access_denied_auth = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "acl_access_denied_cmd")) != NULL)
            vk_stats.acl_access_denied_cmd = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "acl_access_denied_key")) != NULL)
            vk_stats.acl_access_denied_key = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "acl_access_denied_channel")) != NULL)
            vk_stats.acl_access_denied_channel = strtoull(value, NULL, 10);
        /* CPU */
        else if ((value = parse_info_value(line, "used_cpu_sys")) != NULL)
            vk_stats.used_cpu_sys = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_user")) != NULL)
            vk_stats.used_cpu_user = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_sys_children")) != NULL)
            vk_stats.used_cpu_sys_children = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_user_children")) != NULL)
            vk_stats.used_cpu_user_children = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_sys_main_thread")) != NULL)
            vk_stats.used_cpu_sys_main_thread = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_user_main_thread")) != NULL)
            vk_stats.used_cpu_user_main_thread = strtod(value, NULL);
        /* Persistence */
        else if ((value = parse_info_value(line, "rdb_changes_since_last_save")) != NULL)
            vk_stats.rdb_changes_since_last_save = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_last_save_time")) != NULL)
            vk_stats.rdb_last_save_time = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_last_bgsave_status")) != NULL)
            vk_stats.rdb_last_bgsave_status = (strcmp(value, "ok") == 0) ? 1 : 0;
        else if ((value = parse_info_value(line, "rdb_last_bgsave_time_sec")) != NULL)
            vk_stats.rdb_last_bgsave_time_sec = strtol(value, NULL, 10);
        else if ((value = parse_info_value(line, "aof_enabled")) != NULL)
            vk_stats.aof_enabled = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "loading")) != NULL)
            vk_stats.loading = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_bgsave_in_progress")) != NULL)
            vk_stats.rdb_bgsave_in_progress = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_saves")) != NULL)
            vk_stats.rdb_saves = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_last_cow_size")) != NULL)
            vk_stats.rdb_last_cow_size = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "rdb_current_bgsave_time_sec")) != NULL)
            vk_stats.rdb_current_bgsave_time_sec = strtol(value, NULL, 10);
        else if ((value = parse_info_value(line, "aof_rewrite_in_progress")) != NULL)
            vk_stats.aof_rewrite_in_progress = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "aof_rewrites")) != NULL)
            vk_stats.aof_rewrites = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "current_cow_size")) != NULL)
            vk_stats.current_cow_size = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "current_cow_peak")) != NULL)
            vk_stats.current_cow_peak = strtoull(value, NULL, 10);
        /* Replication */
        else if ((value = parse_info_value(line, "role")) != NULL)
            strncpy(vk_stats.role, value, sizeof(vk_stats.role) - 1);
        else if ((value = parse_info_value(line, "connected_slaves")) != NULL)
            vk_stats.connected_slaves = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "master_repl_offset")) != NULL)
            vk_stats.master_repl_offset = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "repl_backlog_active")) != NULL)
            vk_stats.repl_backlog_active = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "repl_backlog_size")) != NULL)
            vk_stats.repl_backlog_size = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "repl_backlog_histlen")) != NULL)
            vk_stats.repl_backlog_histlen = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "replicas_waiting_psync")) != NULL)
            vk_stats.replicas_waiting_psync = strtoul(value, NULL, 10);
        /* Eventloop */
        else if ((value = parse_info_value(line, "eventloop_cycles")) != NULL)
            vk_stats.eventloop_cycles = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "eventloop_duration_sum")) != NULL)
            vk_stats.eventloop_duration_sum = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "eventloop_duration_cmd_sum")) != NULL)
            vk_stats.eventloop_duration_cmd_sum = strtoull(value, NULL, 10);
        else if ((value = parse_info_value(line, "instantaneous_eventloop_cycles_per_sec")) != NULL)
            vk_stats.instantaneous_eventloop_cycles_per_sec = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "instantaneous_eventloop_duration_usec")) != NULL)
            vk_stats.instantaneous_eventloop_duration_usec = strtoul(value, NULL, 10);

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    /* Parse INDOM data (keyspace, errorstats, modules) */
    valkey_parse_indoms(cached_info);

    vk_stats.valid = 1;
    last_fetch_time = now;

    return 0;
}

/*
 * Parse keyspace, errorstats, and modules data from INFO output
 */
static void
valkey_parse_indoms(char *info_str)
{
    char *line, *saveptr, *value, *p;
    char *info_copy = strdup(info_str);
    int db_num;

    if (!info_copy)
        return;

    /* Reset INDOM counters */
    num_keyspace = 0;
    num_errorstats = 0;
    num_modules = 0;

    line = strtok_r(info_copy, "\r\n", &saveptr);
    while (line != NULL) {
        /* Skip comments and empty lines */
        if (*line == '#' || *line == '\0') {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        /* Parse keyspace: db0:keys=123,expires=45,avg_ttl=67890 */
        if (strncmp(line, "db", 2) == 0 && num_keyspace < MAX_KEYSPACE_INSTANCES) {
            if (sscanf(line, "db%d:", &db_num) == 1) {
                keyspace_instances[num_keyspace].i_inst = db_num;
                keyspace_instances[num_keyspace].i_name = strdup(line);
                if (keyspace_instances[num_keyspace].i_name) {
                    /* Truncate at colon for instance name */
                    char *colon = strchr(keyspace_instances[num_keyspace].i_name, ':');
                    if (colon) *colon = '\0';
                }

                /* Parse values */
                p = strchr(line, ':');
                if (p++) {
                    if ((value = strstr(p, "keys=")) != NULL)
                        keyspace_data[num_keyspace].keys = strtoull(value + 5, NULL, 10);
                    if ((value = strstr(p, "expires=")) != NULL)
                        keyspace_data[num_keyspace].expires = strtoull(value + 8, NULL, 10);
                    if ((value = strstr(p, "avg_ttl=")) != NULL)
                        keyspace_data[num_keyspace].avg_ttl = strtoull(value + 8, NULL, 10);
                }
                num_keyspace++;
            }
        }
        /* Parse errorstats: errorstat_ERR:count=123 */
        else if (strncmp(line, "errorstat_", 10) == 0 && num_errorstats < MAX_ERRORSTATS_INSTANCES) {
            char *colon = strchr(line, ':');
            if (colon) {
                *colon = '\0';
                errorstats_instances[num_errorstats].i_inst = num_errorstats;
                errorstats_instances[num_errorstats].i_name = strdup(line + 10); /* Skip "errorstat_" */

                /* Parse count */
                if ((value = strstr(colon + 1, "count=")) != NULL)
                    errorstats_data[num_errorstats].count = strtoull(value + 6, NULL, 10);

                *colon = ':'; /* Restore for next iteration */
                num_errorstats++;
            }
        }
        /* Parse modules: module:name=json,ver=999999,api=1,... */
        else if (strncmp(line, "module:", 7) == 0 && num_modules < MAX_MODULES_INSTANCES) {
            p = line + 7;

            modules_instances[num_modules].i_inst = num_modules;

            /* Extract module name */
            if ((value = strstr(p, "name=")) != NULL) {
                char *comma;
                value += 5;
                comma = strchr(value, ',');
                if (comma) *comma = '\0';
                strncpy(modules_data[num_modules].name, value, sizeof(modules_data[num_modules].name) - 1);
                modules_instances[num_modules].i_name = strdup(modules_data[num_modules].name);
                if (comma) *comma = ',';
            }

            /* Extract version */
            if ((value = strstr(p, "ver=")) != NULL) {
                modules_data[num_modules].version = strtoul(value + 4, NULL, 10);
            }

            num_modules++;
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    /* Update INDOMs */
    indomtab[INDOM_KEYSPACE].it_numinst = num_keyspace;
    indomtab[INDOM_KEYSPACE].it_set = keyspace_instances;

    indomtab[INDOM_ERRORSTATS].it_numinst = num_errorstats;
    indomtab[INDOM_ERRORSTATS].it_set = errorstats_instances;

    indomtab[INDOM_MODULES].it_numinst = num_modules;
    indomtab[INDOM_MODULES].it_set = modules_instances;

    free(info_copy);
}

/*
 * Fetch callback
 */
static int
valkey_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);

    /* Check instance - only valid for INDOM metrics */
    if (cluster != CLUSTER_KEYSPACE && cluster != CLUSTER_ERRORSTATS &&
        cluster != CLUSTER_MODULES) {
        if (inst != PM_IN_NULL)
            return PM_ERR_INST;
    }

    /* Fetch fresh data if needed */
    if (valkey_fetch_info() < 0)
        return PM_ERR_VALUE;

    switch (cluster) {
    case CLUSTER_SERVER:
        switch (item) {
        case ITEM_UPTIME_SECONDS:
            atom->ull = vk_stats.uptime_seconds;
            break;
        case ITEM_PROCESS_ID:
            atom->ul = vk_stats.process_id;
            break;
        case ITEM_UPTIME_DAYS:
            atom->ul = vk_stats.uptime_days;
            break;
        case ITEM_HZ:
            atom->ul = vk_stats.hz;
            break;
        case ITEM_CONFIG_FILE:
            atom->cp = vk_stats.config_file;
            break;
        case ITEM_VALKEY_VERSION:
            atom->cp = vk_stats.valkey_version;
            break;
        case ITEM_SERVER_NAME:
            atom->cp = vk_stats.server_name;
            break;
        case ITEM_TCP_PORT:
            atom->ul = vk_stats.tcp_port;
            break;
        case ITEM_SERVER_TIME_USEC:
            atom->ull = vk_stats.server_time_usec;
            break;
        case ITEM_ARCH_BITS:
            atom->ul = vk_stats.arch_bits;
            break;
        case ITEM_MULTIPLEXING_API:
            atom->cp = vk_stats.multiplexing_api;
            break;
        case ITEM_RUN_ID:
            atom->cp = vk_stats.run_id;
            break;
        case ITEM_IO_THREADS_ACTIVE:
            atom->ul = vk_stats.io_threads_active;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_CLIENTS:
        switch (item) {
        case ITEM_CONNECTED_CLIENTS:
            atom->ul = vk_stats.connected_clients;
            break;
        case ITEM_BLOCKED_CLIENTS:
            atom->ul = vk_stats.blocked_clients;
            break;
        case ITEM_TRACKING_CLIENTS:
            atom->ul = vk_stats.tracking_clients;
            break;
        case ITEM_PUBSUB_CLIENTS:
            atom->ul = vk_stats.pubsub_clients;
            break;
        case ITEM_MAXCLIENTS:
            atom->ul = vk_stats.maxclients;
            break;
        case ITEM_CLUSTER_CONNECTIONS:
            atom->ul = vk_stats.cluster_connections;
            break;
        case ITEM_WATCHING_CLIENTS:
            atom->ul = vk_stats.watching_clients;
            break;
        case ITEM_CLIENTS_IN_TIMEOUT_TABLE:
            atom->ul = vk_stats.clients_in_timeout_table;
            break;
        case ITEM_CLIENT_RECENT_MAX_INPUT_BUFFER:
            atom->ull = vk_stats.client_recent_max_input_buffer;
            break;
        case ITEM_CLIENT_RECENT_MAX_OUTPUT_BUFFER:
            atom->ull = vk_stats.client_recent_max_output_buffer;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_MEMORY:
        switch (item) {
        case ITEM_USED_MEMORY:
            atom->ull = vk_stats.used_memory;
            break;
        case ITEM_USED_MEMORY_RSS:
            atom->ull = vk_stats.used_memory_rss;
            break;
        case ITEM_USED_MEMORY_PEAK:
            atom->ull = vk_stats.used_memory_peak;
            break;
        case ITEM_MEM_FRAGMENTATION:
            atom->f = vk_stats.mem_fragmentation_ratio;
            break;
        case ITEM_USED_MEMORY_OVERHEAD:
            atom->ull = vk_stats.used_memory_overhead;
            break;
        case ITEM_USED_MEMORY_DATASET:
            atom->ull = vk_stats.used_memory_dataset;
            break;
        case ITEM_TOTAL_SYSTEM_MEMORY:
            atom->ull = vk_stats.total_system_memory;
            break;
        case ITEM_MAXMEMORY:
            atom->ull = vk_stats.maxmemory;
            break;
        case ITEM_USED_MEMORY_PEAK_PERC:
            atom->f = vk_stats.used_memory_peak_perc;
            break;
        case ITEM_USED_MEMORY_STARTUP:
            atom->ull = vk_stats.used_memory_startup;
            break;
        case ITEM_USED_MEMORY_SCRIPTS:
            atom->ull = vk_stats.used_memory_scripts;
            break;
        case ITEM_ALLOCATOR_ALLOCATED:
            atom->ull = vk_stats.allocator_allocated;
            break;
        case ITEM_ALLOCATOR_ACTIVE:
            atom->ull = vk_stats.allocator_active;
            break;
        case ITEM_ALLOCATOR_RESIDENT:
            atom->ull = vk_stats.allocator_resident;
            break;
        case ITEM_ALLOCATOR_FRAG_RATIO:
            atom->f = vk_stats.allocator_frag_ratio;
            break;
        case ITEM_RSS_OVERHEAD_RATIO:
            atom->f = vk_stats.rss_overhead_ratio;
            break;
        case ITEM_MAXMEMORY_POLICY:
            atom->cp = vk_stats.maxmemory_policy;
            break;
        case ITEM_MEM_CLIENTS_NORMAL:
            atom->ull = vk_stats.mem_clients_normal;
            break;
        case ITEM_LAZYFREE_PENDING_OBJECTS:
            atom->ull = vk_stats.lazyfree_pending_objects;
            break;
        case ITEM_LAZYFREED_OBJECTS:
            atom->ull = vk_stats.lazyfreed_objects;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_STATS:
        switch (item) {
        case ITEM_TOTAL_COMMANDS:
            atom->ull = vk_stats.total_commands_processed;
            break;
        case ITEM_TOTAL_CONNECTIONS:
            atom->ull = vk_stats.total_connections_received;
            break;
        case ITEM_REJECTED_CONN:
            atom->ull = vk_stats.rejected_connections;
            break;
        case ITEM_KEYSPACE_HITS:
            atom->ull = vk_stats.keyspace_hits;
            break;
        case ITEM_KEYSPACE_MISSES:
            atom->ull = vk_stats.keyspace_misses;
            break;
        case ITEM_EVICTED_KEYS:
            atom->ull = vk_stats.evicted_keys;
            break;
        case ITEM_EXPIRED_KEYS:
            atom->ull = vk_stats.expired_keys;
            break;
        case ITEM_INSTANTANEOUS_OPS:
            atom->ull = vk_stats.instantaneous_ops_per_sec;
            break;
        case ITEM_TOTAL_NET_INPUT:
            atom->ull = vk_stats.total_net_input_bytes;
            break;
        case ITEM_TOTAL_NET_OUTPUT:
            atom->ull = vk_stats.total_net_output_bytes;
            break;
        case ITEM_TOTAL_READS_PROCESSED:
            atom->ull = vk_stats.total_reads_processed;
            break;
        case ITEM_TOTAL_WRITES_PROCESSED:
            atom->ull = vk_stats.total_writes_processed;
            break;
        case ITEM_TOTAL_ERROR_REPLIES:
            atom->ull = vk_stats.total_error_replies;
            break;
        case ITEM_SYNC_FULL:
            atom->ull = vk_stats.sync_full;
            break;
        case ITEM_SYNC_PARTIAL_OK:
            atom->ull = vk_stats.sync_partial_ok;
            break;
        case ITEM_SYNC_PARTIAL_ERR:
            atom->ull = vk_stats.sync_partial_err;
            break;
        case ITEM_TOTAL_FORKS:
            atom->ull = vk_stats.total_forks;
            break;
        case ITEM_LATEST_FORK_USEC:
            atom->ull = vk_stats.latest_fork_usec;
            break;
        case ITEM_INSTANTANEOUS_INPUT_KBPS:
            atom->f = vk_stats.instantaneous_input_kbps;
            break;
        case ITEM_INSTANTANEOUS_OUTPUT_KBPS:
            atom->f = vk_stats.instantaneous_output_kbps;
            break;
        case ITEM_INSTANTANEOUS_INPUT_REPL_KBPS:
            atom->f = vk_stats.instantaneous_input_repl_kbps;
            break;
        case ITEM_INSTANTANEOUS_OUTPUT_REPL_KBPS:
            atom->f = vk_stats.instantaneous_output_repl_kbps;
            break;
        case ITEM_CLIENT_QUERY_BUFFER_LIMIT_DISCONNECTIONS:
            atom->ull = vk_stats.client_query_buffer_limit_disconnections;
            break;
        case ITEM_CLIENT_OUTPUT_BUFFER_LIMIT_DISCONNECTIONS:
            atom->ull = vk_stats.client_output_buffer_limit_disconnections;
            break;
        case ITEM_REPLY_BUFFER_SHRINKS:
            atom->ull = vk_stats.reply_buffer_shrinks;
            break;
        case ITEM_REPLY_BUFFER_EXPANDS:
            atom->ull = vk_stats.reply_buffer_expands;
            break;
        case ITEM_ACL_ACCESS_DENIED_AUTH:
            atom->ull = vk_stats.acl_access_denied_auth;
            break;
        case ITEM_ACL_ACCESS_DENIED_CMD:
            atom->ull = vk_stats.acl_access_denied_cmd;
            break;
        case ITEM_ACL_ACCESS_DENIED_KEY:
            atom->ull = vk_stats.acl_access_denied_key;
            break;
        case ITEM_ACL_ACCESS_DENIED_CHANNEL:
            atom->ull = vk_stats.acl_access_denied_channel;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_CPU:
        switch (item) {
        case ITEM_USED_CPU_SYS:
            atom->d = vk_stats.used_cpu_sys;
            break;
        case ITEM_USED_CPU_USER:
            atom->d = vk_stats.used_cpu_user;
            break;
        case ITEM_USED_CPU_SYS_CHILDREN:
            atom->d = vk_stats.used_cpu_sys_children;
            break;
        case ITEM_USED_CPU_USER_CHILDREN:
            atom->d = vk_stats.used_cpu_user_children;
            break;
        case ITEM_USED_CPU_SYS_MAIN_THREAD:
            atom->d = vk_stats.used_cpu_sys_main_thread;
            break;
        case ITEM_USED_CPU_USER_MAIN_THREAD:
            atom->d = vk_stats.used_cpu_user_main_thread;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_PERSISTENCE:
        switch (item) {
        case ITEM_RDB_CHANGES_SINCE_SAVE:
            atom->ull = vk_stats.rdb_changes_since_last_save;
            break;
        case ITEM_RDB_LAST_SAVE_TIME:
            atom->ull = vk_stats.rdb_last_save_time;
            break;
        case ITEM_RDB_LAST_BGSAVE_STATUS:
            atom->ul = vk_stats.rdb_last_bgsave_status;
            break;
        case ITEM_RDB_LAST_BGSAVE_TIME:
            atom->l = vk_stats.rdb_last_bgsave_time_sec;
            break;
        case ITEM_AOF_ENABLED:
            atom->ul = vk_stats.aof_enabled;
            break;
        case ITEM_LOADING:
            atom->ul = vk_stats.loading;
            break;
        case ITEM_RDB_BGSAVE_IN_PROGRESS:
            atom->ul = vk_stats.rdb_bgsave_in_progress;
            break;
        case ITEM_RDB_SAVES:
            atom->ull = vk_stats.rdb_saves;
            break;
        case ITEM_RDB_LAST_COW_SIZE:
            atom->ull = vk_stats.rdb_last_cow_size;
            break;
        case ITEM_RDB_CURRENT_BGSAVE_TIME_SEC:
            atom->l = vk_stats.rdb_current_bgsave_time_sec;
            break;
        case ITEM_AOF_REWRITE_IN_PROGRESS:
            atom->ul = vk_stats.aof_rewrite_in_progress;
            break;
        case ITEM_AOF_REWRITES:
            atom->ull = vk_stats.aof_rewrites;
            break;
        case ITEM_CURRENT_COW_SIZE:
            atom->ull = vk_stats.current_cow_size;
            break;
        case ITEM_CURRENT_COW_PEAK:
            atom->ull = vk_stats.current_cow_peak;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_REPLICATION:
        switch (item) {
        case ITEM_ROLE:
            atom->cp = vk_stats.role;
            break;
        case ITEM_CONNECTED_SLAVES:
            atom->ul = vk_stats.connected_slaves;
            break;
        case ITEM_MASTER_REPL_OFFSET:
            atom->ull = vk_stats.master_repl_offset;
            break;
        case ITEM_REPL_BACKLOG_ACTIVE:
            atom->ul = vk_stats.repl_backlog_active;
            break;
        case ITEM_REPL_BACKLOG_SIZE:
            atom->ull = vk_stats.repl_backlog_size;
            break;
        case ITEM_REPL_BACKLOG_HISTLEN:
            atom->ull = vk_stats.repl_backlog_histlen;
            break;
        case ITEM_REPLICAS_WAITING_PSYNC:
            atom->ul = vk_stats.replicas_waiting_psync;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_KEYSPACE:
        if (inst == PM_IN_NULL || inst >= num_keyspace)
            return PM_ERR_INST;
        switch (item) {
        case ITEM_KEYSPACE_KEYS:
            atom->ull = keyspace_data[inst].keys;
            break;
        case ITEM_KEYSPACE_EXPIRES:
            atom->ull = keyspace_data[inst].expires;
            break;
        case ITEM_KEYSPACE_AVG_TTL:
            atom->ull = keyspace_data[inst].avg_ttl;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_ERRORSTATS:
        if (inst == PM_IN_NULL || inst >= num_errorstats)
            return PM_ERR_INST;
        switch (item) {
        case ITEM_ERRORSTATS_COUNT:
            atom->ull = errorstats_data[inst].count;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_MODULES:
        if (inst == PM_IN_NULL || inst >= num_modules)
            return PM_ERR_INST;
        switch (item) {
        case ITEM_MODULES_NAME:
            atom->cp = modules_data[inst].name;
            break;
        case ITEM_MODULES_VERSION:
            atom->ul = modules_data[inst].version;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    case CLUSTER_EVENTLOOP:
        switch (item) {
        case ITEM_EVENTLOOP_CYCLES:
            atom->ull = vk_stats.eventloop_cycles;
            break;
        case ITEM_EVENTLOOP_DURATION_SUM:
            atom->ull = vk_stats.eventloop_duration_sum;
            break;
        case ITEM_EVENTLOOP_DURATION_CMD_SUM:
            atom->ull = vk_stats.eventloop_duration_cmd_sum;
            break;
        case ITEM_INSTANTANEOUS_EVENTLOOP_CYCLES_PER_SEC:
            atom->ul = vk_stats.instantaneous_eventloop_cycles_per_sec;
            break;
        case ITEM_INSTANTANEOUS_EVENTLOOP_DURATION_USEC:
            atom->ul = vk_stats.instantaneous_eventloop_duration_usec;
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    default:
        return PM_ERR_PMID;
    }

    return PMDA_FETCH_STATIC;
}

/*
 * Parse configuration file
 */
static void
valkey_parse_config(void)
{
    FILE *fp;
    char buf[256];
    char *p, *key, *value;
    int sep = pmPathSeparator();
    char path[MAXPATHLEN];

    pmsprintf(path, sizeof(path), "%s%c" "valkey" "%c" "valkey.conf",
              pmGetConfig("PCP_PMDAS_DIR"), sep, sep);

    fp = fopen(path, "r");
    if (fp == NULL)
        return;  /* Use defaults */

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        /* Remove newline */
        p = strchr(buf, '\n');
        if (p)
            *p = '\0';

        /* Skip comments and empty lines */
        p = buf;
        while (isspace(*p))
            p++;
        if (*p == '#' || *p == '\0')
            continue;

        /* Parse key=value */
        key = p;
        value = strchr(p, '=');
        if (value == NULL)
            continue;
        *value++ = '\0';

        /* Trim trailing whitespace from key */
        p = key + strlen(key) - 1;
        while (p > key && isspace(*p))
            *p-- = '\0';

        /* Trim leading whitespace from value */
        while (isspace(*value))
            value++;

        if (strcmp(key, "host") == 0) {
            if (vk_host)
                free(vk_host);
            vk_host = strdup(value);
        } else if (strcmp(key, "port") == 0) {
            vk_port = atoi(value);
        }
    }

    fclose(fp);
}

/* command line options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:i:l:pu:U:6:?",
    .long_options = longopts,
};

/*
 * Initialize the PMDA
 */
void
valkey_init(pmdaInterface *dp)
{
    if (isDSO) {
        int sep = pmPathSeparator();
        char mypath[MAXPATHLEN];
        pmsprintf(mypath, sizeof(mypath), "%s%c" "valkey" "%c" "help",
                  pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
        pmdaDSO(dp, PMDA_INTERFACE_7, "valkey DSO", mypath);
    } else {
        pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
        return;

    /* Allocate INDOM instance arrays */
    keyspace_instances = calloc(MAX_KEYSPACE_INSTANCES, sizeof(pmdaInstid));
    keyspace_data = calloc(MAX_KEYSPACE_INSTANCES, sizeof(keyspace_data_t));

    errorstats_instances = calloc(MAX_ERRORSTATS_INSTANCES, sizeof(pmdaInstid));
    errorstats_data = calloc(MAX_ERRORSTATS_INSTANCES, sizeof(errorstats_data_t));

    modules_instances = calloc(MAX_MODULES_INSTANCES, sizeof(pmdaInstid));
    modules_data = calloc(MAX_MODULES_INSTANCES, sizeof(modules_data_t));

    if (!keyspace_instances || !keyspace_data ||
        !errorstats_instances || !errorstats_data ||
        !modules_instances || !modules_data) {
        pmNotifyErr(LOG_ERR, "Failed to allocate INDOM instance arrays");
        return;
    }

    pmdaSetFetchCallBack(dp, valkey_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]),
             metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

    /* Parse configuration */
    valkey_parse_config();
}

/*
 * Main - run as a daemon
 */
int
main(int argc, char **argv)
{
    int sep = pmPathSeparator();
    char mypath[MAXPATHLEN];
    pmdaInterface dispatch;

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "valkey" "%c" "help",
              pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), VALKEY,
               "valkey.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }
    if (opts.username)
        username = opts.username;

    pmdaOpenLog(&dispatch);
    valkey_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);

    /* Cleanup */
    if (vk_host)
        free(vk_host);
    if (vk_conn)
        valkeyFree(vk_conn);
    if (cached_info)
        free(cached_info);

    exit(0);
}
