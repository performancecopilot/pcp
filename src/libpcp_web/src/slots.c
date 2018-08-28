/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "schema.h"
#include "batons.h"
#include "slots.h"
#include "crc16.h"
#include "libuv.h"
#include "util.h"
#include <search.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static char default_server[] = "localhost:6379";

redisSlots *
redisSlotsInit(sds hostspec, redisInfoCallBack info, void *events, void *userdata)
{
    redisSlots		*slots;

    if ((slots = (redisSlots *)calloc(1, sizeof(redisSlots))) == NULL)
	return NULL;

    slots->hostspec = sdsdup(hostspec);
    slots->info = info;
    slots->events = events;
    slots->userdata = userdata;
    slots->control = redisAttach(slots, hostspec);
    return slots;
}

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
	for (i = 0; i < range->nslaves; i++)
	    fprintf(stderr, "\tSlave%05u: %s\n", i, range->slaves[i].hostspec);
    }

    if (tsearch((const void *)range, (void **)&redis->slots, slotsCompare))
	return 0;
    return -ENOMEM;
}

static void
redisSlotServerFree(redisSlots *pool, redisSlotServer *server)
{
    if (server->redis != pool->control)
	redisAsyncDisconnect(server->redis);
    if (server->hostspec != pool->hostspec)
	sdsfree(server->hostspec);
    memset(server, 0, sizeof(*server));
}

static void
redisSlotRangeFree(redisSlots *pool, redisSlotRange *range)
{
    int			i;

    redisSlotServerFree(pool, &range->master);
    for (i = 0; i < range->nslaves; i++)
	redisSlotServerFree(pool, &range->slaves[i]);
    free(range->slaves);
    memset(range, 0, sizeof(*range));
}

void
redisSlotsFree(redisSlots *pool)
{
    void		*root = pool->slots;
    redisSlotRange	*range;

    while (root != NULL) {
	range = *(redisSlotRange **)root;
	tdelete(range, &root, slotsCompare);
	redisSlotRangeFree(pool, range);
    }
    redisAsyncDisconnect(pool->control);
    sdsfree(pool->hostspec);
    free(pool->control);
    memset(pool, 0, sizeof(*pool));
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
    redisSlots		*slots = (redisSlots *)redis->data;
    sds			msg;

    if (status == REDIS_OK) {
	seriesfmt(msg, "Connected to redis on %s:%d",
			redis->c.tcp.host, redis->c.tcp.port);
	seriesmsg(slots, PMLOG_INFO, msg);
	redisAsyncEnableKeepAlive((redisAsyncContext *)redis);
    } else {
	if (redis->c.connection_type == REDIS_CONN_UNIX)
	    seriesfmt(msg, "Connecting to %s failed - %s",
			redis->c.unix_sock.path, redis->errstr);
	else
	    seriesfmt(msg, "Connecting to %s:%d failed - %s",
			redis->c.tcp.host, redis->c.tcp.port, redis->errstr);
	seriesmsg(slots, PMLOG_ERROR, msg);
    }
}

static void
redis_disconnect_callback(const redisAsyncContext *redis, int status)
{
    redisSlots		*slots;
    sds			msg;

    slots = (redisSlots *)redis->data;
    if (status == REDIS_OK) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "Disconnected from redis on %s:%d\n",
			redis->c.tcp.host, redis->c.tcp.port);
    } else {
	if (redis->c.connection_type == REDIS_CONN_UNIX)
	    seriesfmt(msg, "Disconnecting from %s failed - %s",
			redis->c.unix_sock.path, redis->errstr);
	else
	    seriesfmt(msg, "Disconnecting from %s:%d failed - %s",
			redis->c.tcp.host, redis->c.tcp.port, redis->errstr);
	seriesmsg(slots, PMLOG_ERROR, msg);
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
redisGet(redisSlots *slots, const char *command, sds key)
{
    redisSlotServer	*server;
    redisSlotRange	*range, s;
    unsigned int	slot;
    void		*p;

    if (key == NULL)
	return slots->control;

    slot = keySlot(key, sdslen(key));
    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "Redis [slot=%05u] %s %s\n", slot, command, key);
    s.start = s.end = slot;

    p = tfind((const void *)&s, (void **)&slots->slots, slotsCompare);
    if ((range = *(redisSlotRange **)p) == NULL)
	return NULL;

    range->counter++;
    server = (range->nslaves == 0) ? &range->master :
	     &range->slaves[range->counter % range->nslaves];
    if (server->redis == NULL)
	server->redis = redisAttach(slots, server->hostspec);
    return server->redis;
}

int
redisSlotsRequest(redisSlots *slots, const char *command, sds key, sds cmd,
	redisAsyncCallBack *callback, void *arg)
{
    redisAsyncContext	*context = redisGet(slots, command, key);
    int			sts;

    if (UNLIKELY(pmDebugOptions.desperate))
	fputs(cmd, stderr);

    sts = redisAsyncFormattedCommand(context, callback, arg, cmd, sdslen(cmd));
    if (key)
	sdsfree(key);
    sdsfree(cmd);
    if (sts != REDIS_OK)
	return -ENOMEM;
    return 0;
}
