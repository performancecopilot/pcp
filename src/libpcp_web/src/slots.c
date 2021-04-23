/*
 * Copyright (c) 2017-2020 Red Hat.
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
	    fprintf(stderr, "Disconnected from redis on %s:%d\n",
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
    pmUnits	units_count = MMV_UNITS(0, 0, 1, 0, 0, PM_COUNT_ONE);
    pmUnits	units_bytes = MMV_UNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0);
    pmUnits	units_us = MMV_UNITS(0, 1, 0, 0, PM_TIME_USEC, 0);
    pmInDom	noindom = MMV_INDOM_NULL;

    if (slots == NULL || slots->metrics == NULL)
	return; /* no metric registry has been set up */

    mmv_stats_add_metric(slots->metrics, "requests.total", 1,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, noindom,
	"total number of requests", NULL);

    mmv_stats_add_metric(slots->metrics, "requests.error", 2,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, noindom,
	"total number of request errors", NULL);

    mmv_stats_add_metric(slots->metrics, "responses.total", 3,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, noindom,
	"total number of responses", NULL);

    mmv_stats_add_metric(slots->metrics, "responses.error", 4,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_count, noindom,
	"total number of error responses", NULL);

    mmv_stats_add_metric(slots->metrics, "responses.wait", 5,
	MMV_TYPE_U64, MMV_SEM_COUNTER, units_us, noindom,
	"total wait time for responses", NULL);

    mmv_stats_add_metric(slots->metrics, "requests.inflight.total", 6,
	MMV_TYPE_U64, MMV_SEM_DISCRETE, units_count, noindom,
	"total number of inflight requests", NULL);

    mmv_stats_add_metric(slots->metrics, "requests.inflight.bytes", 7,
	MMV_TYPE_I64, MMV_SEM_DISCRETE, units_bytes, noindom,
	"amount of bytes allocated for inflight requests", NULL);

    slots->metrics_handle = mmv_stats_start(slots->metrics);
}

int
redisSlotsSetMetricRegistry(redisSlots *slots, mmv_registry_t *registry)
{
    if (slots) {
	slots->metrics = registry;
	return 0;
    }
    return -ENOMEM;
}

redisSlots *
redisSlotsInit(dict *config, void *events)
{
    redisSlots		*slots;
    sds			servers = NULL;
    int			sts = 0;
    struct timeval	connection_timeout = {5, 0}; // 5s
    struct timeval	command_timeout = {60, 0}; // 1m

    slots = (redisSlots *)calloc(1, sizeof(redisSlots));
    if (slots == NULL) {
	return NULL;
    }

    slots->setup = 0;
    slots->events = events;
    slots->keymap = dictCreate(&sdsKeyDictCallBacks, "keymap");

    servers = pmIniFileLookup(config, "pmseries", "servers");
    if (servers == NULL)
        servers = sdsnew(default_server);

    slots->acc = redisClusterAsyncContextInit();
    if (slots->acc && slots->acc->err) {
        fprintf(stderr, "%s: %s\n", "redisSlotsInit", slots->acc->errstr);
        return slots;
    }

    sts = redisClusterSetOptionNoclusterFallback(slots->acc->cc);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to set nocluster fallback\n");
	return slots;
    }

    sts = redisClusterSetOptionAddNodes(slots->acc->cc, servers);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to add redis nodes\n");
	return slots;
    }

    sts = redisClusterSetOptionConnectTimeout(slots->acc->cc, connection_timeout);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to set connect timeout\n");
	return slots;
    }

    sts = redisClusterSetOptionTimeout(slots->acc->cc, command_timeout);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to set command timeout\n");
	return slots;
    }

    sts = redisClusterLibuvAttach(slots->acc, slots->events);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to attach to libuv event loop\n");
	return slots;
    }

    sts = redisClusterAsyncSetConnectCallback(slots->acc, redis_connect_callback);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to set connect callback\n");
	return slots;
    }

    sts = redisClusterAsyncSetDisconnectCallback(slots->acc, redis_disconnect_callback);
    if (sts != REDIS_OK) {
	fprintf(stderr, "redisSlotsInit: failed to set disconnect callback\n");
	return slots;
    }

    sts = redisClusterConnect2(slots->acc->cc);
    if (sts != REDIS_OK) {
        fprintf(stderr, "redisSlotsInit: cannot connect to redis at %s\n", servers);
        return slots;
    }

    slots->setup = 1;
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

/* extracted from hiutil.c, BSD-3-Clause, https://github.com/Nordix/hiredis-cluster */
static inline int64_t
usec_now()
{
    int64_t usec;

    struct timeval now;
    int status;

    status = gettimeofday(&now, NULL);
    if (status < 0) {
        return -1;
    }

    usec = (int64_t)now.tv_sec * 1000000LL + (int64_t)now.tv_usec;
    return usec;
}

redisSlotsReplyData*
redisSlotsReplyDataInit(redisSlots *slots, size_t req_size,
                        redisClusterCallbackFn *callback, void *arg)
{
    redisSlotsReplyData *srd;

    srd = calloc(1, sizeof(redisSlotsReplyData));
    if (srd == NULL) {
        return NULL;
    }

    srd->slots = slots;
    srd->start = usec_now();
    srd->req_size = req_size;
    srd->callback = callback;
    srd->arg = arg;
    return srd;
}

void
redisSlotsReplyDataFree(redisSlotsReplyData *srd)
{
    free(srd);
}

void
redisSlotsReplyCallback(redisClusterAsyncContext *c, void *r, void *arg)
{
    redisSlotsReplyData *srd = arg;
    redisReply 		*reply = r;

    mmv_stats_add(srd->slots->metrics_handle, "responses.wait", NULL, usec_now() - srd->start);
    mmv_stats_inc(srd->slots->metrics_handle, "responses.total", NULL);
    mmv_stats_add(srd->slots->metrics_handle, "requests.inflight.total", NULL, -1);
    mmv_stats_add(srd->slots->metrics_handle, "requests.inflight.bytes", NULL, (int64_t)(-srd->req_size));

    if (reply == NULL || reply->type == REDIS_REPLY_ERROR)
	mmv_stats_inc(srd->slots->metrics_handle, "responses.error", NULL);

    srd->callback(c, r, srd->arg);
    redisSlotsReplyDataFree(arg);
}

/*
 * Submit an arbitrary request to a (set of) Redis instance(s).
 * The given key is used to determine the slot used, as per the
 * cluster specification - https://redis.io/topics/cluster-spec
 * 
 * Serves mainly as a wrapper to redisClusterAsyncFormattedCommand
 * including debug output and error handling
 */
int
redisSlotsRequest(redisSlots *slots, const sds cmd,
		redisClusterCallbackFn *callback, void *arg)
{
    int			sts;

    if (UNLIKELY(!slots->setup))
        return -ENOTCONN;

    if (UNLIKELY(pmDebugOptions.desperate))
	fprintf(stderr, "Sending raw redis command:\n%s", cmd);

    redisSlotsReplyData *srd = redisSlotsReplyDataInit(slots, sdslen(cmd), callback, arg);
    sts = redisClusterAsyncFormattedCommand(slots->acc, redisSlotsReplyCallback, srd, cmd, sdslen(cmd));
    mmv_stats_inc(slots->metrics_handle, "requests.total", NULL);

    if (sts != REDIS_OK) {
	mmv_stats_inc(slots->metrics_handle, "requests.error", NULL);
        fprintf(stderr, "%s: %s (%s)\n", "redisSlotsRequest", slots->acc->errstr, cmd);
	return -ENOMEM;
    }

    mmv_stats_inc(slots->metrics_handle, "requests.inflight.total", NULL);
    mmv_stats_add(slots->metrics_handle, "requests.inflight.bytes", NULL, sdslen(cmd));
    return REDIS_OK;
}

int
redisSlotsRequestFirstNode(redisSlots *slots, const sds cmd,
		redisClusterCallbackFn *callback, void *arg)
{
    dictIterator	*iterator;
    dictEntry		*entry;
    int			sts;

    iterator = dictGetSafeIterator(slots->acc->cc->nodes);
    entry = dictNext(iterator);
    dictReleaseIterator(iterator);
    if (!entry) {
	fprintf(stderr, "%s: %s", "redisSlotsRequestFirstNode", "No redis not configured.\n");
	return REDIS_ERR;
    }

    if (UNLIKELY(pmDebugOptions.desperate))
	fprintf(stderr, "Sending raw redis command to first node:\n%s", cmd);

    redisSlotsReplyData *srd = redisSlotsReplyDataInit(slots, sdslen(cmd), callback, arg);
    sts = redisClusterAsyncFormattedCommandToNode(slots->acc, dictGetVal(entry), redisSlotsReplyCallback, srd, cmd, sdslen(cmd));
    mmv_stats_inc(slots->metrics_handle, "requests.total", NULL);

    if (sts != REDIS_OK) {
	mmv_stats_inc(slots->metrics_handle, "requests.error", NULL);
	fprintf(stderr, "%s: %s (%s)\n", "redisSlotsRequestFirstNode", slots->acc->errstr, cmd);
	return -ENOMEM;
    }

    mmv_stats_inc(slots->metrics_handle, "requests.inflight.total", NULL);
    mmv_stats_add(slots->metrics_handle, "requests.inflight.bytes", NULL, sdslen(cmd));
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
    long long		position, offset, length;
    sds			cmd, key, msg;
    int			sts;

    if (!reader &&
	(reader = *readerp = redisReaderCreate()) == NULL) {
	infofmt(msg, "out-of-memory for Redis client reader");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -ENOMEM;
    }

    offset = reader->pos;
    length = sdslen(reader->buf);
    if (redisReaderFeed(reader, buffer, nread) != REDIS_OK ||
	redisReaderGetReply(reader, (void **)&reply) != REDIS_OK) {
	infofmt(msg, "failed to parse Redis protocol request");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -EPROTO;
    }

    if (reply != NULL) {	/* client request is complete */
	key = cmd = NULL;
	if (reply->type == REDIS_REPLY_ARRAY ||
	    reply->type == REDIS_REPLY_MAP ||
	    reply->type == REDIS_REPLY_SET)
	    cmd = sdsnew(reply->element[0]->str);
	if (cmd && (entry = dictFind(slots->keymap, cmd)) != NULL) {
	    position = dictGetSignedIntegerVal(entry);
	    if (position < reply->elements)
		key = sdsnew(reply->element[position]->str);
	}
	sdsfree(cmd);
	cmd = sdsnewlen(reader->buf + offset, sdslen(reader->buf) - length);
	if (key != NULL && position > 0)
	    sts = redisClusterAsyncFormattedCommand(slots->acc, callback, arg, cmd, sdslen(cmd));
	else
	    sts = redisSlotsRequestFirstNode(slots, cmd, callback, arg);
	sdsfree(key);
	if (sts != REDIS_OK)
	    return -EPROTO;
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
	redisReply *reply, const char *format, va_list argp)
{
    sds			msg = sdsnew("Error: ");

    msg = sdscatvprintf(msg, format, argp);
    if (reply && reply->type == REDIS_REPLY_ERROR)
	msg = sdscatfmt(msg, "\nRedis: %s\n", reply->str);
    else
	msg = sdscat(msg, "\n");
    info(PMLOG_RESPONSE, msg, userdata);
    sdsfree(msg);
}

int
checkStatusReplyOK(redisInfoCallBack info, void *userdata,
		redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STATUS &&
	(strcmp("OK", reply->str) == 0 || strcmp("QUEUED", reply->str) == 0))
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

int
checkStreamReplyString(redisInfoCallBack info, void *userdata,
	redisReply *reply, sds s, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STRING && strcmp(s, reply->str) == 0)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

int
checkArrayReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_ARRAY)
	return 0;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

long long
checkIntegerReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_INTEGER)
	return reply->integer;
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return -1;
}

sds
checkStringReply(redisInfoCallBack info, void *userdata,
	redisReply *reply, const char *format, ...)
{
    va_list		argp;

    if (reply && reply->type == REDIS_REPLY_STRING)
	return sdsnew(reply->str);
    va_start(argp, format);
    reportReplyError(info, userdata, reply, format, argp);
    va_end(argp);
    return NULL;
}
