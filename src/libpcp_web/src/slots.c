/*
 * Copyright (c) 2017 Red Hat.
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
#include "redis.h"
#include "slots.h"
#include "crc16.h"

#define MAXSLOTS	(1<<14)	/* see CLUSTER_SLOTS in Redis sources */
#define SLOTMASK	(MAXSLOTS-1)

typedef struct redisSlots {
    redisContext	*contexts[MAXSLOTS];
    const char		*hostspec;
    struct timeval	timeout;
} redisSlots;

redisSlots *
redisSlotsInit(const char *hostspec, struct timeval *timeout)
{
    redisSlots		*pool;

    if ((pool = (redisSlots *)calloc(1, sizeof(redisSlots))) == NULL)
	return NULL;
    pool->hostspec = hostspec;
    pool->timeout = *timeout;
    return pool;
}

void
redisFreeSlots(redisSlots *pool)
{
    int			i;

    for (i = 0; i < MAXSLOTS; i++) {
	if (pool->contexts[i]) {
	    redisFree(pool->contexts[i]);
	    pool->contexts[i] = NULL;
	}
    }
    free(pool->contexts);
    memset(pool, 0, sizeof(*pool));
}

/*
 * Hash slot lookup based on the Redis cluster specification.
 */
unsigned int
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

/* TODO: externalise Redis configuration */
static char default_server[] = "localhost:6379";
static struct timeval default_timeout = { 1, 500000 }; /* 1.5 secs */

redisContext *
redisGet(redisSlots *pool, const char *key, unsigned int keylen)
{
    redisContext	*ctxp;
    unsigned int	i, slot = keySlot(key, keylen);
    struct timeval	*timeout = &default_timeout;
    char		*server = &default_server[0];

    if ((ctxp = pool->contexts[slot]) != NULL)
	return ctxp;

    if ((ctxp = redis_connect(server, timeout)) == NULL)
	return NULL;

    /* TODO: spread this context properly throughout the array */
    /* This requires a list of available Redis servers, first. */

    for (i = 0; i < MAXSLOTS; i++)
	pool->contexts[i] = ctxp;

    return ctxp;
}

redisContext *
redis_connect(char *server, struct timeval *timeout)
{
    redisContext *redis;

    if (server == NULL)
	server = strdup(default_server);
    if (timeout == NULL)
	timeout = &default_timeout;

    if (strncmp(server, "unix:", 5) == 0) {
	redis = redisConnectUnixWithTimeout(server + 5, *timeout);
    } else {
	unsigned int    port;
	char	    *endnum, *p;

	if ((p = rindex(server, ':')) == NULL) {
	    port = 6379;  /* default redis port */
	} else {
	    port = (unsigned int) strtoul(p + 1, &endnum, 10);
	    if (*endnum != '\0')
		port = 6379;
	    else
		*p = '\0';
	}
	redis = redisConnectWithTimeout(server, port, *timeout);
    }

    if (!redis || redis->err) {
	if (redis) {
	    fprintf(stderr, "Redis connection error: %s\n", redis->errstr);
	    redisFree(redis);
	} else {
	    fprintf(stderr, "Redis connection error: can't allocate context\n");
	}
	return NULL;
    }

    redisSetTimeout(redis, *timeout);
    redisEnableKeepAlive(redis);
    return redis;
}
