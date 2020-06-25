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
#ifndef SLOTS_H
#define SLOTS_H

#include "batons.h"
#include "redis.h"
#include "maps.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)
#define SLOTS_PHASES	6

typedef enum redisSlotsFlags {
    SLOTS_NONE		= 0,
    SLOTS_VERSION	= (1 << 0),
    SLOTS_KEYMAP	= (1 << 1),
    SLOTS_SEARCH	= (1 << 2),
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
    unsigned int	setup;		/* slots info all successfully setup */
    unsigned int	refresh;	/* do slot refresh whenever possible */
    redisSlotRange	*slots;		/* all instances; e.g. CLUSTER SLOTS */
    redisMap		*keymap;	/* map command names to key position */
    dict		*contexts;	/* async contexts access by hostspec */
    void		*events;
    int			search;
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
    seriesBatonPhase	*current;
    seriesBatonPhase	phases[SLOTS_PHASES];
    int			version;
    int			error;
    redisSlots		*slots;
    redisInfoCallBack	info;
    redisDoneCallBack	done;
    void		*userdata;
    void		*arg;
} redisSlotsBaton;

extern void redis_slots_end_phase(void *);

/* Redis reply helper routines */
extern int testReplyError(redisReply *, const char *);
extern void reportReplyError(redisInfoCallBack, void *, redisReply *, const char *, va_list);
extern int checkStatusReplyOK(redisInfoCallBack, void *, redisReply *, const char *, ...);
extern int checkStreamReplyString(redisInfoCallBack, void *, redisReply *, sds, const char *, ...);
extern int checkArrayReply(redisInfoCallBack, void *, redisReply *, const char *, ...);
extern long long checkIntegerReply(redisInfoCallBack, void *, redisReply *, const char *, ...);
extern sds checkStringReply(redisInfoCallBack, void *, redisReply *, const char *, ...);

#endif	/* SLOTS_H */
