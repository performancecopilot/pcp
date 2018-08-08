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
#ifndef SLOTS_H
#define SLOTS_H

#include "redis.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)

typedef struct redisSlotServer {
    sds			hostspec;	/* hostname:port or unix socket file */
    redisAsyncContext	*redis;
} redisSlotServer;

typedef struct redisSlotRange {
    unsigned int	start;
    unsigned int	end;
    redisSlotServer	master;
    unsigned int	counter;
    unsigned int	nslaves;
    redisSlotServer	*slaves;
} redisSlotRange;

typedef void (*redisInfoCallBack)(pmloglevel, sds, void *);
typedef void (*redisDoneCallBack)(void *);

typedef struct redisSlots {
    redisAsyncContext	*control;	/* initial Redis context connection */
    sds			hostspec;	/* control socket host specification */
    redisSlotRange	*slots;		/* all instances; e.g. CLUSTER SLOTS */
    void		*events;

    redisInfoCallBack	info;		/* TODO: remove - use baton */
    void		*userdata;	/* TODO: remove - use baton */
} redisSlots;

typedef void (*redisPhase)(redisSlots *, void *);	/* phased operations */

extern redisSlots *redisSlotsInit(sds, redisInfoCallBack, void *, void *);
extern int redisSlotRangeInsert(struct redisSlots *, struct redisSlotRange *);
extern redisAsyncContext *redisSlotsConnect(redisSlots *, const char *);
extern redisAsyncContext *redisGet(struct redisSlots *, const char *, sds);
extern void redisFreeSlots(struct redisSlots *);

extern int redisSlotsRequest(redisSlots *, const char *, sds, sds, redisAsyncCallBack *, void *);

typedef struct {
    unsigned int	magic;
    int			version;
    redisSlots		*redis;
    redisInfoCallBack	info;
    redisDoneCallBack	done;
    void		*userdata;
    void		*arg;
} redisSlotsBaton;

extern void initRedisSlotsBaton(redisSlotsBaton *, int,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern void doneRedisSlotsBaton(redisSlotsBaton *);

#endif	/* SLOTS_H */
