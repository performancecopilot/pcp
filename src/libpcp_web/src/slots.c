/*
 * Copyright (c) 2017-2021 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "schema.h"
#include "batons.h"
#include "slots.h"
#include "util.h"
#include <ctype.h>
#include <search.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#if defined(HAVE_LIBUV)
#include <hiredis-cluster/adapters/libuv.h>
#else
static int redisClusterLibuvAttach() { return REDIS_OK; }
#endif

static char default_server[] = "localhost:6379";

static void
redis_connect_callback(const redisAsyncContext *redis, int status)
{
    if (status == REDIS_OK) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "Connected to Redis on %s:%d\n",
			redis->c.tcp.host, redis->c.tcp.port);
	redisAsyncEnableKeepAlive((redisAsyncContext *)redis);
	/* TODO: if SSL inject redisSecureConnection() here */
    } else if (pmDebugOptions.series) {
	if (redis->c.connection_type == REDIS_CONN_UNIX)
	    fprintf(stderr, "Connecting to %s failed - %s\n",
			redis->c.unix_sock.path, redis->errstr);
	else
	    fprintf(stderr, "Connecting to %s:%d failed - %s\n",
			redis->c.tcp.host, redis->c.tcp.port, redis->errstr);
    }
}

static void
redis_disconnect_callback(const redisAsyncContext *redis, int status)
{
    if (status == REDIS_OK) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "Disconnected from key server on %s:%d\n",
			redis->c.tcp.host, redis->c.tcp.port);
    } else if (pmDebugOptions.series) {
	if (redis->c.connection_type == REDIS_CONN_UNIX)
	    fprintf(stderr, "Disconnecting from %s failed - %s\n",
			redis->c.unix_sock.path, redis->errstr);
	else
	    fprintf(stderr, "Disconnecting from %s:%d failed - %s\n",
			redis->c.tcp.host, redis->c.tcp.port, redis->errstr);
    }
}

void
redisSlotsSetupMetrics(redisSlots *slots)
{
    pmAtomValue	**table;
    pmUnits	units_count = MMV_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE);
    pmUnits	units_bytes = MMV_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0);
    pmUnits	units_us = MMV_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0);
    void	*map;

    if (slots == NULL || slots->registry == NULL)
	return; /* no metric registry has been set up */

    mmv_stats_add_metric(slots->registry, "requests.total", 1,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
	"number of requests",
	"Total number of Redis requests sent");

    mmv_stats_add_metric(slots->registry, "requests.error", 2,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
	"number of request errors",
	"Total number of Redis request errors");

    mmv_stats_add_metric(slots->registry, "responses.total", 3,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
	"number of responses",
	"Total number of Redis responses received");

    mmv_stats_add_metric(slots->registry, "responses.error", 4,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, MMV_INDOM_NULL,
	"number of error responses",
	"Total number of Redis error responses received");

    mmv_stats_add_metric(slots->registry, "responses.time", 5,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_us, MMV_INDOM_NULL,
	"total time for responses",
	"Cumulative time taken to receive all Redis responses");

    mmv_stats_add_metric(slots->registry, "requests.inflight.total", 6,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_count, MMV_INDOM_NULL,
	"inflight requests",
	"Total number of inflight Redis requests");

    mmv_stats_add_metric(slots->registry, "requests.inflight.bytes", 7,
	MMV_TYPE_U64, MMV_SEM_INSTANT, units_bytes, MMV_INDOM_NULL,
	"bytes allocated for inflight requests",
	"Memory currently allocated for inflight Redis requests");

    mmv_stats_add_metric(slots->registry, "requests.total_bytes", 8,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_bytes, MMV_INDOM_NULL,
	"total bytes sent for requests",
	"Cumulative count of bytes sent for all Redis requests");

    mmv_stats_add_metric(slots->registry, "responses.total_bytes", 9,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_bytes, MMV_INDOM_NULL,
	"total bytes received in responses",
	"Cumulative count of bytes received in Redis responses");

    slots->map = map = mmv_stats_start(slots->registry);

    table = slots->metrics;
    table[SLOT_REQUESTS_TOTAL] = mmv_lookup_value_desc(map,
					"requests.total", NULL);
    table[SLOT_REQUESTS_ERROR] = mmv_lookup_value_desc(map,
					"requests.error", NULL);
    table[SLOT_RESPONSES_TOTAL] = mmv_lookup_value_desc(map,
					"responses.total", NULL);
    table[SLOT_RESPONSES_ERROR] = mmv_lookup_value_desc(map,
					"responses.error", NULL);
    table[SLOT_RESPONSES_TIME] = mmv_lookup_value_desc(map,
					"responses.time", NULL);
    table[SLOT_REQUESTS_INFLIGHT_TOTAL] = mmv_lookup_value_desc(map,
					"requests.inflight.total", NULL);
    table[SLOT_REQUESTS_INFLIGHT_BYTES] = mmv_lookup_value_desc(map,
					"requests.inflight.bytes", NULL);
    table[SLOT_REQUESTS_TOTAL_BYTES] = mmv_lookup_value_desc(map,
					"requests.total_bytes", NULL);
    table[SLOT_RESPONSES_TOTAL_BYTES] = mmv_lookup_value_desc(map,
					"responses.total_bytes", NULL);
}

int
redisSlotsSetMetricRegistry(redisSlots *slots, mmv_registry_t *registry)
{
    if (slots) {
	slots->registry = registry;
	return 0;
    }
    return -ENOMEM;
}

redisSlots *
redisSlotsInit(dict *config, void *events)
{
    redisSlots		*slots;
    sds			servers = NULL;
    sds			def_servers = NULL;
    sds			username = NULL;
    sds			password = NULL;
    int			sts = 0;
    struct timeval	connection_timeout = {5, 0}; // 5s
    struct timeval	command_timeout = {60, 0}; // 1m

    if ((slots = (redisSlots *)calloc(1, sizeof(redisSlots))) == NULL) {
	pmNotifyErr(LOG_ERR, "%s: failed to allocate redisSlots\n",
			"redisSlotsInit");
	return NULL;
    }

    slots->state = SLOTS_DISCONNECTED;
    slots->events = events;
    slots->keymap = dictCreate(&sdsKeyDictCallBacks, "keymap");
    if (slots->keymap == NULL) {
	pmNotifyErr(LOG_ERR, "%s: failed to allocate keymap\n",
			"redisSlotsInit");
	free(slots);
	return NULL;
    }

    servers = pmIniFileLookup(config, "keys", "servers");
    if (servers == NULL)
	servers = pmIniFileLookup(config, "redis", "servers");
    if (servers == NULL)
	servers = pmIniFileLookup(config, "pmseries", "servers");
    if (servers == NULL)
	servers = def_servers = sdsnew(default_server);

    username = pmIniFileLookup(config, "keys", "username");
    if (username == NULL)
	username = pmIniFileLookup(config, "redis", "username");
    if (username == NULL)
	username = pmIniFileLookup(config, "pmseries", "auth.username");
    password = pmIniFileLookup(config, "keys", "password");
    if (password == NULL)
	password = pmIniFileLookup(config, "redis", "password");
    if (password == NULL)
	password = pmIniFileLookup(config, "pmseries", "auth.password");

    if ((slots->acc = redisClusterAsyncContextInit()) == NULL) {
	/* Coverity CID370635 */
	pmNotifyErr(LOG_ERR, "%s: %s failed\n",
			"redisSlotsInit", "redisClusterAsyncContextInit");
	sdsfree(def_servers);
	return slots;
    }

    if (slots->acc->err) {
        pmNotifyErr(LOG_ERR, "%s: %s\n", "redisSlotsInit", slots->acc->errstr);
	sdsfree(def_servers);
	return slots;
    }

    sts = redisClusterSetOptionAddNodes(slots->acc->cc, servers);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to add redis nodes: %s\n",
			"redisSlotsInit", slots->acc->cc->errstr);
	sdsfree(def_servers);
	return slots;
    }
    sdsfree(def_servers); /* Coverity CID370634 */

    /*
     * the ini parser already removes spaces at the beginning and end of the
     * configuration values, so checking for empty strings using sdslen() is
     * fine
     */
    if (username != NULL && sdslen(username) > 0) {
	sts = redisClusterSetOptionUsername(slots->acc->cc, username);
	if (sts != REDIS_OK) {
	    pmNotifyErr(LOG_ERR, "%s: failed to set Redis username: %s\n",
		"redisClusterSetOptionUsername", slots->acc->cc->errstr);
	    return slots;
	}
    }

    /*
     * see note above re empty configuration values
     * having only a password set and no username is a valid Redis
     * configuration, see https://redis.io/commands/auth
     */
    if (password != NULL && sdslen(password) > 0) {
	sts = redisClusterSetOptionPassword(slots->acc->cc, password);
	if (sts != REDIS_OK) {
	    pmNotifyErr(LOG_ERR, "%s: failed to set Redis password: %s\n",
		"redisClusterSetOptionPassword", slots->acc->cc->errstr);
	    return slots;
	}
    }

    sts = redisClusterSetOptionConnectTimeout(slots->acc->cc, connection_timeout);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to set connect timeout: %s\n",
			"redisSlotsInit", slots->acc->errstr);
	return slots;
    }

    sts = redisClusterSetOptionTimeout(slots->acc->cc, command_timeout);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to set command timeout: %s\n",
			"redisSlotsInit", slots->acc->cc->errstr);
	return slots;
    }

    sts = redisClusterLibuvAttach(slots->acc, slots->events);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to attach to event loop: %s\n",
			"redisSlotsInit", slots->acc->errstr);
	return slots;
    }

    sts = redisClusterAsyncSetConnectCallback(slots->acc, redis_connect_callback);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to set connect callback: %s\n",
			"redisSlotsInit", slots->acc->errstr);
	return slots;
    }

    sts = redisClusterAsyncSetDisconnectCallback(slots->acc, redis_disconnect_callback);
    if (sts != REDIS_OK) {
	pmNotifyErr(LOG_ERR, "%s: failed to set disconnect callback: %s\n",
			"redisSlotsInit", slots->acc->errstr);
	return slots;
    }

    return slots;
}

/**
 * despite the name, this function also handles the initial
 * connection to Redis
 */
void
redisSlotsReconnect(redisSlots *slots, redisSlotsFlags flags,
		redisInfoCallBack info, redisDoneCallBack done,
		void *userdata, void *events, void *arg)
{
    dictIterator	*iterator;
    dictEntry		*entry;
    int			sts = 0;
    static int		log_connection_errors = 1;

    if (slots == NULL)
	return;

    slots->state = SLOTS_CONNECTING;
    slots->conn_seq++;

    /* reset Redis context in case of reconnect */
    if (slots->acc->err) {
	/* reset possible 'Connection refused' error before reconnecting */
	slots->acc->err = 0;
	memset(slots->acc->errstr, '\0', strlen(slots->acc->errstr));
    }
    redisClusterAsyncDisconnect(slots->acc);

    /* reset redisSlots in case of reconnect */
    slots->cluster = 0;
    slots->search = 0;
    dictEmpty(slots->keymap, NULL);

    sts = redisClusterConnect2(slots->acc->cc);
    if (sts == REDIS_OK) {
	slots->cluster = 1;
    }
    else if (slots->acc->cc->err &&
		strcmp(slots->acc->cc->errstr, REDIS_ENOCLUSTER) == 0) {
	/* Redis instance has cluster support disabled */
	slots->acc->cc->err = 0;
	memset(slots->acc->cc->errstr, '\0', strlen(slots->acc->cc->errstr));
	slots->cluster = 0;

	/*
	 * Sanity check: show error message if more than one node
	 * is configured, but cluster mode is disabled
	 * otherwise all other nodes silently don't get any data
	 */
	iterator = dictGetSafeIterator(slots->acc->cc->nodes);
	entry = dictNext(iterator);
	if (entry && dictNext(iterator)) {
	    dictReleaseIterator(iterator);
	    pmNotifyErr(LOG_ERR, "%s: more than one node is configured, "
			"but cluster mode is disabled", "redisSlotsReconnect");
	    slots->state = SLOTS_ERR_FATAL;
	    return;
	}
	dictReleaseIterator(iterator);
    }
    else {
	if (log_connection_errors || pmDebugOptions.desperate) {
	    pmNotifyErr(LOG_INFO, "Cannot connect to Redis: %s\n",
			slots->acc->cc->errstr);
	    log_connection_errors = 0;
	}
	slots->state = SLOTS_DISCONNECTED;
	return;
    }

    slots->state = SLOTS_CONNECTED;
    log_connection_errors = 1;
    redisSchemaLoad(slots, flags, info, done, userdata, events, arg);
}

/**
 * this method allocates the redisSlots struct and exists for backwards
 * compatibility, the actual connection to Redis happens in
 * redisSlotsReconnect()
 */
redisSlots *
redisSlotsConnect(dict *config, redisSlotsFlags flags,
		redisInfoCallBack info, redisDoneCallBack done,
		void *userdata, void *events, void *arg)
{
    redisSlots			*slots;
    sds				enabled, msg;

    if (!(enabled = pmIniFileLookup(config, "resp", "enabled")))
	enabled = pmIniFileLookup(config, "redis", "enabled");
    if (enabled && strcmp(enabled, "false") == 0)
	return NULL;

    slots = redisSlotsInit(config, events);
    if (slots == NULL) {
	infofmt(msg, "Failed to allocate memory for key server slots");
	info(PMLOG_ERROR, msg, arg);
	sdsfree(msg);
	return NULL;
    }

    redisSlotsReconnect(slots, flags, info, done, userdata, events, arg);
    return slots;
}

void
redisSlotsFree(redisSlots *slots)
{
    redisClusterAsyncDisconnect(slots->acc);
    redisClusterAsyncFree(slots->acc);
    dictRelease(slots->keymap);
    memset(slots, 0, sizeof(*slots));
    free(slots);
}

static inline uint64_t
gettimeusec(void)
{
    struct timeval now;
    if (gettimeofday(&now, NULL) < 0)
        return -1;
    return (uint64_t)now.tv_sec * 1000000 + (uint64_t)now.tv_usec;
}

static redisSlotsReplyData *
redisSlotsReplyDataAlloc(redisSlots *slots, size_t req_size,
			redisClusterCallbackFn *callback, void *arg)
{
    redisSlotsReplyData *srd;

    srd = calloc(1, sizeof(redisSlotsReplyData));
    if (srd == NULL) {
        return NULL;
    }

    srd->slots = slots;
    srd->conn_seq = slots->conn_seq;
    srd->start = gettimeusec();
    srd->req_size = req_size;
    srd->callback = callback;
    srd->arg = arg;
    return srd;
}

static inline void
redisSlotsReplyDataFree(redisSlotsReplyData *srd)
{
    free(srd);
}

uint64_t
redisSlotsInflightRequests(redisSlots *slots)
{
    pmAtomValue		*atom;

    atom = slots ? slots->metrics[SLOT_REQUESTS_INFLIGHT_TOTAL] : NULL;
    return atom ? atom->ull : 0;
}

static void
redisSlotsReplyCallback(redisClusterAsyncContext *c, void *r, void *arg)
{
    redisSlotsReplyData *srd = arg;
    redisReply 		*reply = r;
    void		*map = srd->slots->map;

    if (map) {
	pmAtomValue	**metrics = srd->slots->metrics;
	pmAtomValue	*value;
	uint64_t	delta = gettimeusec();

	delta = (delta < srd->start) ? 0 : delta - srd->start;
	mmv_add(map, metrics[SLOT_RESPONSES_TIME], &delta);

	value = metrics[SLOT_REQUESTS_INFLIGHT_TOTAL];
	delta = value && value->ull > 0 ? value->ull - 1 : 0;
	mmv_set(map, metrics[SLOT_REQUESTS_INFLIGHT_TOTAL], &delta);

	value = metrics[SLOT_REQUESTS_INFLIGHT_BYTES];
	delta = value ? value->ull : 0;
	delta = (delta < srd->req_size) ? 0 : delta - srd->req_size;
	mmv_set(map, metrics[SLOT_REQUESTS_INFLIGHT_BYTES], &delta);

	delta = srd->req_size;
	mmv_add(map, metrics[SLOT_RESPONSES_TOTAL_BYTES], &delta);
	mmv_inc(map, metrics[SLOT_RESPONSES_TOTAL]);

	if (reply == NULL || reply->type == REDIS_REPLY_ERROR)
	    mmv_inc(map, metrics[SLOT_RESPONSES_ERROR]);
    }

    /**
     * handle connection resets
     *
     * Why here and not in redis_disconnect_callback?
     * Access to redisSlots->state is required. We cannot save a pointer to
     * redisSlots in redisAsyncContext->data, because this member is already
     * used by hiredis-cluster
     *
     * fwiw, in case one node of a cluster is down, slots->state should not be
     * set to SLOTS_DISCONNECTED, because there (should) be a failover in place
     * and another node will handle the requests.
     *
     * A future improvement would be to update hiredis-cluster and add a 'data'
     * member to the async cluster context, analogue to the 'data' member of
     * hiredis, update the disconnect callback to return the cluster context and
     * use this new disconnect callback instead of the conditional below.
     *
     * Register a server disconnect if:
     * * The server returns an I/O error.  In this case errno is also set, but
     *   there are lots of different error codes for connection failures (for example
     *   ECONNRESET, ENETUNREACH, ENETDOWN, ...) - defensively assume all require a
     *   reconnect
     * * Server returns the "LOADING ... is loading the dataset in memory" error
     * * Ignore any errors for server requests pre-dating the latest (current)
     *   connection (to handle the case where a callback returns after a new
     *   connection was already established)
     * * Ignore any errors if the state is already set to SLOTS_DISCONNECTED
     * * Ignore errors if cluster mode is enabled.
     */
    if (((reply == NULL && c->err == REDIS_ERR_IO) ||
         (reply != NULL && reply->type == REDIS_REPLY_ERROR &&
	  (strncmp(reply->str, KEYS_ELOADING, strlen(KEYS_ELOADING)) == 0 &&
	   strstr(reply->str, KEYS_ELOADDATA) != NULL))) &&
	srd->conn_seq == srd->slots->conn_seq &&
	srd->slots->state != SLOTS_DISCONNECTED &&
	srd->slots->cluster == 0) {
	pmNotifyErr(LOG_ERR, "Lost connection to Redis.\n");
	srd->slots->state = SLOTS_DISCONNECTED;
    }

    srd->callback(c, r, srd->arg);
    redisSlotsReplyDataFree(arg);
}

/*
 * Submit an arbitrary request to a (set of) key server instance(s).
 * The given key is used to determine the slot used, as per the
 * cluster specification - https://valkey.io/topics/cluster-spec
 * 
 * Serves mainly as a wrapper to redisClusterAsyncFormattedCommand
 * including debug output and error handling
 */
int
redisSlotsRequest(redisSlots *slots, const sds cmd,
		redisClusterCallbackFn *callback, void *arg)
{
    int			sts;
    uint64_t		size;
    redisSlotsReplyData	*srd;

    /*
     * redisSlotsSetupStart() also sends Redis requests,
     * therefore both SLOTS_CONNECTED and SLOTS_READY states are valid
     */
    if (UNLIKELY(slots->state != SLOTS_CONNECTED && slots->state != SLOTS_READY))
	return -ENOTCONN;

    if (!slots->cluster)
	return redisSlotsRequestFirstNode(slots, cmd, callback, arg);

    if (UNLIKELY(pmDebugOptions.desperate))
	fprintf(stderr, "%s: sending raw redis command:\n%s",
			"redisSlotsRequest", cmd);

    size = sdslen(cmd);
    if ((srd = redisSlotsReplyDataAlloc(slots, size, callback, arg)) == NULL) {
	mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_ERROR]);
	pmNotifyErr(LOG_ERR, "%s: failed to allocate reply data (%llu bytes)\n",
			"redisSlotsRequest", (unsigned long long)size);
	return -ENOMEM;
    }
    if ((sts = redisClusterAsyncFormattedCommand(slots->acc,
		    redisSlotsReplyCallback, srd, cmd, size)) != REDIS_OK) {
	mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_ERROR]);
	pmNotifyErr(LOG_ERR, "%s: %s (%s)\n", "redisSlotsRequest",
			slots->acc->errstr, cmd);
	return -ENOMEM;
    }

    mmv_add(slots->map, slots->metrics[SLOT_REQUESTS_INFLIGHT_BYTES], &size);
    mmv_add(slots->map, slots->metrics[SLOT_REQUESTS_TOTAL_BYTES], &size);
    mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_INFLIGHT_TOTAL]);
    mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_TOTAL]);

    return REDIS_OK;
}

int
redisSlotsRequestFirstNode(redisSlots *slots, const sds cmd,
		redisClusterCallbackFn *callback, void *arg)
{
    dictIterator	*iterator;
    dictEntry		*entry;
    cluster_node	*node;
    redisSlotsReplyData	*srd;
    uint64_t		size;
    int			sts;

    /*
     * redisSlotsSetupStart() also sends Redis requests,
     * therefore both SLOTS_CONNECTED and SLOTS_READY states are valid
     */
    if (UNLIKELY(slots->state != SLOTS_CONNECTED && slots->state != SLOTS_READY))
	return -ENOTCONN;

    iterator = dictGetSafeIterator(slots->acc->cc->nodes);
    entry = dictNext(iterator);
    dictReleaseIterator(iterator);
    if (!entry) {
	pmNotifyErr(LOG_ERR, "%s: No Redis node configured.",
			"redisSlotsRequestFirstNode");
	return REDIS_ERR;
    }

    node = dictGetVal(entry);
    if (UNLIKELY(pmDebugOptions.desperate))
	fprintf(stderr, "%s: sending raw redis command to node %s\n%s",
			"redisSlotsRequestFirstNode", node->addr, cmd);

    size = sdslen(cmd);
    if ((srd = redisSlotsReplyDataAlloc(slots, size, callback, arg)) == NULL) {
	mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_ERROR]);
	pmNotifyErr(LOG_ERR, "%s: failed to allocate reply data (%llu bytes)",
			"redisSlotsRequestFirstNode", (unsigned long long)size);
	return -ENOMEM;
    }
    sts = redisClusterAsyncFormattedCommandToNode(slots->acc, node,
			redisSlotsReplyCallback, srd, cmd, size);
    if (sts != REDIS_OK) {
	mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_ERROR]);
	pmNotifyErr(LOG_ERR, "%s: %s (%s)\n",
			"redisSlotsRequestFirstNode", slots->acc->errstr, cmd);
	return -ENOMEM;
    }

    mmv_add(slots->map, slots->metrics[SLOT_REQUESTS_INFLIGHT_BYTES], &size);
    mmv_add(slots->map, slots->metrics[SLOT_REQUESTS_TOTAL_BYTES], &size);
    mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_INFLIGHT_TOTAL]);
    mmv_inc(slots->map, slots->metrics[SLOT_REQUESTS_TOTAL]);

    return REDIS_OK;
}

int
redisSlotsProxyConnect(redisSlots *slots, redisInfoCallBack info,
	redisReader **readerp, const char *buffer, ssize_t nread,
	redisClusterCallbackFn *callback, void *arg)
{
    redisReader		*reader = *readerp;
    redisReply		*reply = NULL;
    dictEntry		*entry;
    size_t		replyStartPosition;
    long long		position;
    sds			cmd, msg;
    int			hasKey;
    int			sts;

    if (!reader &&
	(reader = *readerp = redisReaderCreate()) == NULL) {
	infofmt(msg, "out-of-memory for Redis client reader");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -ENOMEM;
    }

    if (redisReaderFeed(reader, buffer, nread) != REDIS_OK) {
	infofmt(msg, "failed to parse Redis protocol request");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -EPROTO;
    }

    /* parse all Redis requests contained in buffer (Redis pipelining) */
    while (1) {
	replyStartPosition = reader->pos;
	sts = redisReaderGetReply(reader, (void **)&reply);
	if (sts != REDIS_OK) {
	    infofmt(msg, "failed to parse Redis protocol request");
	    info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	    return -EPROTO;
	}
	if (reply == NULL) {
	    break;
	}

	cmd = NULL;
	hasKey = 0;
	if (reply->type == REDIS_REPLY_ARRAY ||
	    reply->type == REDIS_REPLY_MAP ||
	    reply->type == REDIS_REPLY_SET)
	    cmd = sdsnew(reply->element[0]->str);
	if (cmd && (entry = dictFind(slots->keymap, cmd)) != NULL) {
	    position = dictGetSignedIntegerVal(entry);
	    if (position > 0 && position < reply->elements)
		hasKey = 1;
	}
	sdsfree(cmd);

	cmd = sdsnewlen(reader->buf + replyStartPosition, reader->pos - replyStartPosition);
	if (hasKey)
	    sts = redisSlotsRequest(slots, cmd, callback, arg);
	else
	    sts = redisSlotsRequestFirstNode(slots, cmd, callback, arg);
	sdsfree(cmd);

	if (sts != REDIS_OK) {
	    redisReply *errorReply = calloc(1, sizeof(redisReply));
	    errorReply->type = REDIS_REPLY_ERROR;
	    errorReply->str = slots->acc->errstr;
	    errorReply->len = strlen(slots->acc->errstr);
	    callback(slots->acc, errorReply, arg);
	}
    }
    return 0;
}

void
redisSlotsProxyFree(redisReader *reader)
{
    if (reader)
	redisReaderFree(reader);
}

/*
 * Helper routines for handling various expected Redis reply types.
 */

int
testReplyError(redisReply *reply, const char *server_message)
{
    return (reply && reply->type == REDIS_REPLY_ERROR &&
	    strcmp(reply->str, server_message) == 0);
}

void
reportReplyError(redisInfoCallBack info, void *userdata,
	redisClusterAsyncContext *acc, redisReply *reply, const char *format, va_list argp)
{
    sds			msg;

    msg = sdscatvprintf(sdsempty(), format, argp);
    if (reply && reply->type == REDIS_REPLY_ERROR)
	msg = sdscatfmt(msg, "\nRedis reply error: %s", reply->str);
    else if (acc->err)
	msg = sdscatfmt(msg, "\nRedis acc error: %s", acc->errstr);
    else if (acc->cc->err)
	msg = sdscatfmt(msg, "\nRedis cc error: %s", acc->cc->errstr);
    info(PMLOG_RESPONSE, msg, userdata);
    sdsfree(msg);
}

int
checkStatusReplyOK(redisInfoCallBack info, void *userdata,
		redisClusterAsyncContext *acc, redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STATUS &&
	(strcmp("OK", reply->str) == 0 || strcmp("QUEUED", reply->str) == 0))
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, acc, reply, format, argp);
    va_end(argp);
    return -1;
}

int
checkStreamReplyString(redisInfoCallBack info, void *userdata,
	redisClusterAsyncContext *acc, redisReply *reply, sds s, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STRING && strcmp(s, reply->str) == 0)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, acc, reply, format, argp);
    va_end(argp);
    return -1;
}

int
checkArrayReply(redisInfoCallBack info, void *userdata,
	redisClusterAsyncContext *acc, redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_ARRAY)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, acc, reply, format, argp);
    va_end(argp);
    return -1;
}

long long
checkIntegerReply(redisInfoCallBack info, void *userdata,
	redisClusterAsyncContext *acc, redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_INTEGER)
	return reply->integer;
    va_start(argp, format);
    reportReplyError(info, userdata, acc, reply, format, argp);
    va_end(argp);
    return -1;
}

sds
checkStringReply(redisInfoCallBack info, void *userdata,
	redisClusterAsyncContext *acc, redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STRING)
	return sdsnew(reply->str);
    va_start(argp, format);
    reportReplyError(info, userdata, acc, reply, format, argp);
    va_end(argp);
    return NULL;
}
