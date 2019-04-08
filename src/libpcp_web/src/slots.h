/*
 * Copyright (c) 2017-2018 Red Hat.
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
    unsigned int	nreplicas;
    redisSlotServer	*replicas;
} redisSlotRange;

typedef struct redisSlots {
    unsigned int	counter;
    unsigned int	nslots;
    redisSlotRange	*slots;		/* all instances; e.g. CLUSTER SLOTS */
    redisMap		*keymap;	/* map command names to key position */
    dict		*contexts;	/* async contexts access by hostspec */
    unsigned int	refresh;	/* do slot refresh whenever possible */
    void		*events;
} redisSlots;

typedef void (*redisPhase)(redisSlots *, void *);	/* phased operations */

extern redisSlots *redisSlotsInit(dict *, void *);
extern redisAsyncContext *redisGetSlotContext(redisSlots *, unsigned int, const char *);
extern redisAsyncContext *redisGetAsyncContextBySlot(redisSlots *, unsigned int);
extern redisAsyncContext *redisGetAsyncContextByHost(redisSlots *, sds);

extern redisSlots *redisSlotsConnect(dict *, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern int redisSlotRangeInsert(redisSlots *, redisSlotRange *);
extern int redisSlotsRequest(redisSlots *, const char *, sds, sds,
		redisAsyncCallBack *, void *);
extern void redisSlotsClear(redisSlots *);
extern void redisSlotsFree(redisSlots *);

extern int redisSlotsRedirect(redisSlots *, redisReply *, void *,
		redisInfoCallBack, const sds, redisAsyncCallBack, void *);

extern int redisSlotsProxyConnect(redisSlots *,
		redisInfoCallBack, redisReader **, const char *, ssize_t,
		redisAsyncCallBack *, void *);
extern void redisSlotsProxyFree(redisReader *);

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
