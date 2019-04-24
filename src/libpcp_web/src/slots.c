/*
 * Copyright (c) 2017-2019 Red Hat.
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
#include "crc16.h"
#include "libuv.h"
#include "util.h"
#include <ctype.h>
#include <search.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static char default_server[] = "localhost:6379";

static int
slotsCompare(const void *pa, const void *pb)
{
    redisSlotRange	*a = (redisSlotRange *)pa;
    redisSlotRange	*b = (redisSlotRange *)pb;

    if (a->end < b->start)
	return -1;
    if (b->end < a->start)
	return 1;
    return 0;
}

int
redisSlotRangeInsert(redisSlots *redis, redisSlotRange *range)
{
    if (pmDebugOptions.series) {
	int		i;

	fprintf(stderr, "Slot range: %05u-%05u\n", range->start, range->end);
	fprintf(stderr, "    Master: %s\n", range->master.hostspec);
	for (i = 0; i < range->nreplicas; i++)
	    fprintf(stderr, "\tReplica%05u: %s\n", i, range->replicas[i].hostspec);
    }

    if (tsearch((const void *)range, (void **)&redis->slots, slotsCompare)) {
	redis->nslots++;
	return 0;
    }
    return -ENOMEM;
}

redisSlots *
redisSlotsInit(dict *config, void *events)
{
    redisSlotRange	*range;
    redisSlots		*slots;
    int			i = 0, start, space, nservers = 0;
    sds			servers, *specs = NULL;

    if ((slots = (redisSlots *)calloc(1, sizeof(redisSlots))) == NULL)
	return NULL;

    slots->events = events;
    slots->keymap = dictCreate(&sdsKeyDictCallBacks, "keymap");
    slots->contexts = dictCreate(&sdsKeyDictCallBacks, "contexts");

    servers = pmIniFileLookup(config, "pmseries", "servers");
    if ((servers == NULL) ||
	!(specs = sdssplitlen(servers, sdslen(servers), ",", 1, &nservers))) {
	if ((range = calloc(1, sizeof(redisSlotRange))) == NULL)
	    goto fail;

	/* use the default Redis server if none specified */
	range->master.hostspec = sdsnew(default_server);
	range->start = 0;
	range->end = MAXSLOTS;
	redisSlotRangeInsert(slots, range);
	return slots;
    }

    /* given a list of one or more servers, share the slot range */
    start = 0;
    space = MAXSLOTS / nservers;

    for (i = 0; i < nservers; i++) {
	if ((range = calloc(1, sizeof(redisSlotRange))) == NULL)
	    goto fail;

	range->master.hostspec = specs[i];
	range->start = start;
	range->end = (i == nservers - 1) ? MAXSLOTS : space * i;
	redisSlotRangeInsert(slots, range);

	start += space + 1;	/* prepare for next iteration */
    }
    free(specs);
    return slots;

fail:
    while (i < nservers)
	sdsfree(specs[i++]);
    free(specs);
    redisSlotsFree(slots);
    return NULL;
}

static void
redisSlotServerFree(redisSlots *pool, redisSlotServer *server)
{
    redisAsyncContext	*context = NULL;
    dictEntry		*entry;
    sds			hostspec;

    if ((hostspec = server->hostspec) != NULL) {
	/* check the context map to ensure no dangling references */
	if ((entry = dictUnlink(pool->contexts, hostspec)) != NULL) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "%s: %s\n", "redisSlotServerFree", hostspec);
	    context = dictGetVal(entry);
	    dictFreeUnlinkedEntry(pool->contexts, entry);
	    redisAsyncDisconnect(context);
	}
	sdsfree(hostspec);
    } else if ((context = server->redis) != NULL) {
	redisAsyncDisconnect(context);
    }
    memset(server, 0, sizeof(*server));
}

void
redisSlotRangeClear(redisSlots *pool, redisSlotRange *range)
{
    int			i;

    redisSlotServerFree(pool, &range->master);
    for (i = 0; i < range->nreplicas; i++)
	redisSlotServerFree(pool, &range->replicas[i]);
    free(range->replicas);
    memset(range, 0, sizeof(*range));
}

void
redisSlotsClear(redisSlots *pool)
{
    void		*root = pool->slots;
    redisSlotRange	*range;

    while (root != NULL) {
	range = *(redisSlotRange **)root;
	tdelete(range, &root, slotsCompare);
	redisSlotRangeClear(pool, range);
    }
    pool->slots = NULL;
    pool->nslots = 0;
}

void
redisSlotsFree(redisSlots *pool)
{
    redisSlotsClear(pool);
    dictRelease(pool->keymap);
    dictRelease(pool->contexts);
    memset(pool, 0, sizeof(*pool));
    free(pool);
}

/*
 * Hash slot lookup based on the Redis cluster specification.
 */
static unsigned int
keySlot(const char *key, unsigned int keylen)
{
    int			start, end;	/* curly brace indices */

    for (start = 0; start < keylen; start++)
	if (key[start] == '{')
	    break;

    /* No curly braces - hash the entire key */
    if (start == keylen)
	return crc16(key, keylen) & SLOTMASK;

    /* Start curly found - check if we have an end brace */
    for (end = start + 1; end < keylen; end++)
	if (key[end] == '}')
	    break;

    /* No end brace, or nothing in-between, use full key */
    if (end == keylen || end == start + 1)
	return crc16(key, keylen) & SLOTMASK;

    /* Hash the key content in-between the braces */
    return crc16(key + start + 1, end - start - 1) & SLOTMASK;
}

static void
redis_connect_callback(const redisAsyncContext *redis, int status)
{
    if (status == REDIS_OK) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "Connected to redis on %s:%d\n",
			redis->c.tcp.host, redis->c.tcp.port);
	redisAsyncEnableKeepAlive((redisAsyncContext *)redis);
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

static redisAsyncContext *
redis_connect(const char *server)
{
    char		hostname[MAXHOSTNAMELEN];
    char		*endnum, *p;
    unsigned int	port;

    if (server == NULL)
	server = default_server;
    if (strncmp(server, "unix:", 5) != 0) {
	pmsprintf(hostname, sizeof(hostname), "%s", server);
	if ((p = rindex(hostname, ':')) == NULL) {
	    port = 6379;  /* default Redis port */
	} else {
	    port = (unsigned int)strtoul(p + 1, &endnum, 10);
	    if (*endnum != '\0')
		port = 6379;
	    else
		*p = '\0';
	}
	return redisAsyncConnect(hostname, port);
    }
    return redisAsyncConnectUnix(server + 5);
}

redisAsyncContext *
redisAttach(redisSlots *slots, const char *server)
{
    redisAsyncContext	*redis = redis_connect(server);

    if (redis) {
	redis->data = (void *)slots;
	redisEventAttach(redis, slots->events);
	redisAsyncSetConnectCallBack(redis, redis_connect_callback);
	redisAsyncSetDisconnectCallBack(redis, redis_disconnect_callback);
    }
    return redis;
}

redisAsyncContext *
redisGetAsyncContextByHost(redisSlots *slots, sds hostspec)
{
    redisAsyncContext	*context;
    dictEntry		*entry;

    if ((entry = dictFind(slots->contexts, hostspec)) != NULL)
	return (redisAsyncContext *)dictGetVal(entry);
    context = redisAttach(slots, hostspec);
    dictAdd(slots->contexts, hostspec, (void *)context);
    return context;
}

redisAsyncContext *
redisGetAsyncContextBySlot(redisSlots *slots, unsigned int slot)
{
    redisSlotServer	*server;
    redisSlotRange	*range, s = {.start = slot, .end = slot};
    void		*p;

    p = tfind((const void *)&s, (void **)&slots->slots, slotsCompare);
    if ((range = *(redisSlotRange **)p) == NULL)
	return NULL;

#if 1
    server = &range->master;
#else
    /*
     * Using replicas seems to always invoke cluster MOVED responses back
     * to the master, even for read-only requests, which was not the plan
     * (leads to worse performance not better), so this is disabled until
     * further analysis is done as to why that may be.
     */
    range->counter++;
    server = (range->nreplicas == 0) ? &range->master :
	     &range->replicas[range->counter % range->nreplicas];
#endif
    if (server->redis == NULL)
	server->redis = redisGetAsyncContextByHost(slots, server->hostspec);

    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "Redis [slot=%05u] %s\n", slot, server->hostspec);

    return server->redis;
}

redisAsyncContext *
redisGetAsyncContext(redisSlots *slots, sds key, const char *topic)
{
    unsigned int	slot;

    if (LIKELY(key))
	slot = keySlot(key, sdslen(key));	/* cluster specification */
    else if (slots->nslots)
	slot = slots->counter++ % slots->nslots;	/* round-robin */
    else
	slot = 0;						/* ? */

    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "Redis [slot=%05u] %s %s\n", slot, topic, key);

    return redisGetAsyncContextBySlot(slots, slot);
}

/*
 * Submit an arbitrary request to a (set of) Redis instance(s).
 * The given key is used to determine the slot used, as per the
 * cluster specification - https://redis.io/topics/cluster-spec
 */
int
redisSlotsRequest(redisSlots *slots, const char *topic,
		const sds key, const sds cmd, redisAsyncCallBack *callback, void *arg)
{
    redisAsyncContext	*context = redisGetAsyncContext(slots, key, topic);
    int			sts;

    if (UNLIKELY(pmDebugOptions.desperate))
	fputs(cmd, stderr);

    sts = redisAsyncFormattedCommand(context, callback, cmd, arg);
    if (key)
	sdsfree(key);
    if (sts != REDIS_OK)
	return -ENOMEM;
    return 0;
}

/*
 * Given a Redis reply, check whether this is a cluster redirect
 * response.  If so, decode the target server and resend command
 * to the specified server.  If not, release the command memory.
 */
int
redisSlotsRedirect(redisSlots *slots, redisReply *reply,
		void *userdata, redisInfoCallBack info, const sds cmd,
		redisAsyncCallBack *callback, void *arg)
{
    redisAsyncContext	*context;
    const char		*slot, *p = reply->str;
    sds			hostspec, msg;
    int			moved = 0, asked = 0, len, sts = -1;

    if (LIKELY(reply == NULL || reply->type != REDIS_REPLY_ERROR))
	goto complete;

    /* Redirection and resharding - resubmit cmd/cb/arg to new server */
    if (strncmp(p, "MOVED ", sizeof("MOVED")) == 0)
	moved = sizeof("MOVED");
    else if (strncmp(p, "ASK ", sizeof("ASK")) == 0)
	asked = sizeof("ASK");

    if (moved || asked) {
	if (moved)
	    slots->refresh = 1;
	slot = (p += (moved + asked));
	while (isdigit(*p))	/* skip over slot# */
	    p++;
	len = p - slot;
	while (isspace(*p))	/* skip over space */
	    p++;

	hostspec = sdsnew(p);
	if (pmDebugOptions.series) {
	    fprintf(stderr, "redisSlotsRedirect: send to slot %.*s (%s)\n",
			    len, slot, hostspec);
	}
	context = redisGetAsyncContextByHost(slots, hostspec);
	sdsfree(hostspec);
	sts = redisAsyncFormattedCommand(context, callback, cmd, arg);
	if (sts == REDIS_OK)
	    return 1;

	infofmt(msg, "Request redirection to instance %s failed", p);
	info(PMLOG_REQUEST, msg, userdata);
	sdsfree(msg);
	sts = 0;
    }

complete:
    sdsfree(cmd);
    return sts;
}

int
redisSlotsProxyConnect(redisSlots *slots, redisInfoCallBack info,
	redisReader **readerp, const char *buffer, ssize_t nread,
	redisAsyncCallBack *callback, void *arg)
{
    redisAsyncContext	*context;
    redisReader		*reader = *readerp;
    redisReply		*reply = NULL;
    dictEntry		*entry;
    long long		position;
    sds			cmd, key, msg;
    int			sts;

    if (!reader &&
	(reader = *readerp = redisReaderCreate()) == NULL) {
	infofmt(msg, "out-of-memory for Redis client reader");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -ENOMEM;
    }

    if (redisReaderFeed(reader, buffer, nread) != REDIS_OK ||
	redisReaderGetReply(reader, (void **)&reply) != REDIS_OK) {
	infofmt(msg, "failed to parse Redis protocol request");
	info(PMLOG_REQUEST, msg, arg), sdsfree(msg);
	return -EPROTO;
    }

    if (reply != NULL) {	/* client request is complete */
	key = cmd = NULL;
	if (reply->type == REDIS_REPLY_ARRAY)
	    cmd = sdsnew(reply->element[0]->str);
	if (cmd && (entry = dictFind(slots->keymap, cmd)) != NULL) {
	    position = dictGetSignedIntegerVal(entry);
	    if (position < reply->elements)
		key = sdsnew(reply->element[position]->str);
	}
	context = redisGetAsyncContext(slots, key, cmd);
	sdsfree(key);
	sdsfree(cmd);
	cmd = sdsdup(reader->buf);
	sts = redisAsyncFormattedCommand(context, callback, cmd, arg);
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
