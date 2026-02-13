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
	CLUSTER_PERSISTENCE
};

/* Server cluster items */
enum {
	ITEM_UPTIME_SECONDS = 0,
	ITEM_PROCESS_ID,
	ITEM_UPTIME_DAYS,
	ITEM_HZ,
	ITEM_CONFIG_FILE
};

/* Client cluster items */
enum {
	ITEM_CONNECTED_CLIENTS = 0,
	ITEM_BLOCKED_CLIENTS,
	ITEM_TRACKING_CLIENTS,
	ITEM_PUBSUB_CLIENTS
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
	ITEM_MAXMEMORY
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
	ITEM_TOTAL_NET_OUTPUT
};

/* CPU cluster items */
enum {
	ITEM_USED_CPU_SYS = 0,
	ITEM_USED_CPU_USER,
	ITEM_USED_CPU_SYS_CHILDREN,
	ITEM_USED_CPU_USER_CHILDREN
};

/* Persistence cluster items */
enum {
	ITEM_RDB_CHANGES_SINCE_SAVE = 0,
	ITEM_RDB_LAST_SAVE_TIME,
	ITEM_RDB_LAST_BGSAVE_STATUS,
	ITEM_RDB_LAST_BGSAVE_TIME,
	ITEM_AOF_ENABLED
};

static pmdaMetric metrictab[] = {
    /* Server metrics - cluster 0 */
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_UPTIME_SECONDS), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_PROCESS_ID), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_UPTIME_DAYS), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_HZ), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0, 0, 0, 0, 0, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_SERVER, ITEM_CONFIG_FILE), PM_TYPE_STRING, PM_INDOM_NULL, PM_SEM_DISCRETE,
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

    /* CPU metrics - cluster 4 */
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_SYS), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_USER), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_SYS_CHILDREN), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
        PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) }, },
    { NULL, { PMDA_PMID(CLUSTER_CPU, ITEM_USED_CPU_USER_CHILDREN), PM_TYPE_DOUBLE, PM_INDOM_NULL, PM_SEM_COUNTER,
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
};

/* Valkey connection and cache */
static valkeyContext *vk_conn = NULL;
static char *vk_host = VALKEY_DEFAULT_HOST;
static int vk_port = VALKEY_DEFAULT_PORT;
static time_t last_fetch_time = 0;
static char *cached_info = NULL;

/* Cached metric values */
static struct {
    /* Server */
    uint64_t uptime_seconds;
    uint32_t process_id;
    uint32_t uptime_days;
    uint32_t hz;
    char config_file[256];
    /* Clients */
    uint32_t connected_clients;
    uint32_t blocked_clients;
    uint32_t tracking_clients;
    uint32_t pubsub_clients;
    /* Memory */
    uint64_t used_memory;
    uint64_t used_memory_rss;
    uint64_t used_memory_peak;
    float mem_fragmentation_ratio;
    uint64_t used_memory_overhead;
    uint64_t used_memory_dataset;
    uint64_t total_system_memory;
    uint64_t maxmemory;
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
    /* CPU */
    double used_cpu_sys;
    double used_cpu_user;
    double used_cpu_sys_children;
    double used_cpu_user_children;
    /* Persistence */
    uint64_t rdb_changes_since_last_save;
    uint64_t rdb_last_save_time;
    uint32_t rdb_last_bgsave_status;  /* 1=ok, 0=failed */
    int32_t rdb_last_bgsave_time_sec;
    uint32_t aof_enabled;
    int valid;
} vk_stats;

static int isDSO = 1;
static char *username;

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

    if (vk_conn != NULL) {
        valkeyFree(vk_conn);
        vk_conn = NULL;
    }

    vk_conn = valkeyConnectWithTimeout(vk_host, vk_port, timeout);
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
        /* Clients */
        else if ((value = parse_info_value(line, "connected_clients")) != NULL)
            vk_stats.connected_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "blocked_clients")) != NULL)
            vk_stats.blocked_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "tracking_clients")) != NULL)
            vk_stats.tracking_clients = strtoul(value, NULL, 10);
        else if ((value = parse_info_value(line, "pubsub_clients")) != NULL)
            vk_stats.pubsub_clients = strtoul(value, NULL, 10);
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
        /* CPU */
        else if ((value = parse_info_value(line, "used_cpu_sys")) != NULL)
            vk_stats.used_cpu_sys = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_user")) != NULL)
            vk_stats.used_cpu_user = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_sys_children")) != NULL)
            vk_stats.used_cpu_sys_children = strtod(value, NULL);
        else if ((value = parse_info_value(line, "used_cpu_user_children")) != NULL)
            vk_stats.used_cpu_user_children = strtod(value, NULL);
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

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    vk_stats.valid = 1;
    last_fetch_time = now;

    return 0;
}

/*
 * Fetch callback
 */
static int
valkey_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int item = pmID_item(mdesc->m_desc.pmid);

    if (inst != PM_IN_NULL)
        return PM_ERR_INST;

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
            if (strcmp(vk_host, VALKEY_DEFAULT_HOST) != 0)
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

    pmdaSetFetchCallBack(dp, valkey_fetchCallBack);

    pmdaInit(dp, NULL, 0, metrictab, sizeof(metrictab)/sizeof(metrictab[0]));

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
    if (vk_conn)
        valkeyFree(vk_conn);
    if (cached_info)
        free(cached_info);

    exit(0);
}
