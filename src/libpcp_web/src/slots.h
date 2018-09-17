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

#include "batons.h"
#include "redis.h"
#include "maps.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)

typedef enum {
    SLOTS_NONE		= 0,
    SLOTS_VERSION	= 1,
    SLOTS_KEYMAP	= 2,
} redisSlotsFlags;

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

typedef struct redisSlots {
    redisSlotServer	control;	/* control socket/host specification */
    redisSlotRange	*slots;		/* all instances; e.g. CLUSTER SLOTS */
    redisMap		*keymap;	/* map command names to key position */
    void		*events;
} redisSlots;

typedef void (*redisPhase)(redisSlots *, void *);	/* phased operations */

extern redisSlots *redisSlotsInit(sds, void *);
extern int redisSlotRangeInsert(redisSlots *, redisSlotRange *);
extern redisAsyncContext *redisAttach(redisSlots *, const char *);
extern redisAsyncContext *redisGetAsyncContext(redisSlots *, const char *, sds);

extern redisSlots *redisSlotsConnect(sds, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern int redisSlotsRequest(redisSlots *, const char *, sds, sds,
		redisAsyncCallBack *, void *);
extern int redisSlotsProxy(redisSlots *,
		redisInfoCallBack, redisReader **, ssize_t, const char *,
		redisAsyncCallBack *, void *);
extern void redisSlotsFree(redisSlots *);

typedef struct {
    seriesBatonMagic	magic;		/* MAGIC_SLOTS */
    redisSlotsFlags	flags;
    int			version;
    redisSlots		*slots;
    redisInfoCallBack	info;
    redisDoneCallBack	done;
    void		*userdata;
    void		*arg;
} redisSlotsBaton;

extern void initRedisSlotsBaton(redisSlotsBaton *, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern void doneRedisSlotsBaton(redisSlotsBaton *);

#endif	/* SLOTS_H */
