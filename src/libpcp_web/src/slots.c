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
#include "slots.h"
#include "crc16.h"
#include <search.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

static char default_server[] = "localhost:6379";
static struct timeval default_timeout = { 1, 500000 }; /* 1.5 secs */

redisSlots *
redisSlotsInit(sds hostspec, struct timeval *timeout)
{
    redisSlots		*pool;

    if ((pool = (redisSlots *)calloc(1, sizeof(redisSlots))) == NULL)
	return NULL;

    pool->hostspec = sdsdup(hostspec);
    if (timeout == NULL)
	timeout = &default_timeout;
    else
	pool->timeout = *timeout;
    pool->control = redis_connect(hostspec, timeout);
    return pool;
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

	fprintf(stderr, "Slot range: %u-%u\n", range->start, range->end);
	fprintf(stderr, "    Master: %s\n", range->master.hostspec);
	for (i = 0; i < range->nslaves; i++)
	    fprintf(stderr, "\tSlave%u: %s\n", i, range->slaves[i].hostspec);
    }

    if (tsearch((const void *)range, (void **)&redis->slots, slotsCompare))
	return 0;
    return -ENOMEM;
}

static void
redisSlotServerFree(redisSlots *pool, redisSlotServer *server)
{
    if (server->redis != pool->control)
	redisFree(server->redis);
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
redisFreeSlots(redisSlots *pool)
{
    void		*root = pool->slots;
    redisSlotRange	*range;

    while (root != NULL) {
	range = *(redisSlotRange **)root;
	tdelete(range, &root, slotsCompare);
	redisSlotRangeFree(pool, range);
    }
    redisFree(pool->control);
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

redisContext *
redisGet(redisSlots *redis, const char *command, sds key)
{
    redisSlotServer	*server;
    redisSlotRange	*range, s;
    unsigned int	slot;
    void		*p;

    if (key == NULL)
	return redis->control;

    slot = keySlot(key, sdslen(key));
    if (UNLIKELY(pmDebugOptions.series))
	fprintf(stderr, "redisGet[slot=%u] %s %s\n", slot, command, key);
    s.start = s.end = slot;

    p = tfind((const void *)&s, (void **)&redis->slots, slotsCompare);
    if ((range = *(redisSlotRange **)p) == NULL)
	return NULL;

    range->counter++;
    server = (range->nslaves == 0 || redis->readonly == 0) ? &range->master
	   : &range->slaves[range->counter % range->nslaves];
    if (server->redis == NULL)
	server->redis = redis_connect(server->hostspec, &redis->timeout);
    return server->redis;
}

redisContext *
redis_connect(char *server, struct timeval *timeout)
{
    redisContext	*redis;

    if (server == NULL)
	server = default_server;
    if (timeout == NULL)
	timeout = &default_timeout;

    if (strncmp(server, "unix:", 5) == 0) {
	redis = redisConnectUnixWithTimeout(server + 5, *timeout);
    } else {
	unsigned int	port;
	char		*endnum, *p;
	char		hostname[MAXHOSTNAMELEN];

	pmsprintf(hostname, sizeof(hostname), "%s", server);
	if ((p = rindex(hostname, ':')) == NULL) {
	    port = 6379;  /* default redis port */
	} else {
	    port = (unsigned int)strtoul(p + 1, &endnum, 10);
	    if (*endnum != '\0')
		port = 6379;
	    else
		*p = '\0';
	}
	/* redis = redisConnectWithTimeout(server, port, *timeout); */
	redis = redisConnect(hostname, port);
    }

    /* TODO: messages need to be passed back via an info callback */
    if (!redis || redis->err) {
	if (redis) {
	    fprintf(stderr, "Redis connection error: %s\n", redis->errstr);
	    redisFree(redis);
	} else {
	    fprintf(stderr, "Redis connection error: can't allocate context\n");
	}
	return NULL;
    }

    /* redisSetTimeout(redis, *timeout); */
    redisEnableKeepAlive(redis);
    return redis;
}
