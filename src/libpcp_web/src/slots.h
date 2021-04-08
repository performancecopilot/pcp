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

#include <hiredis-cluster/hircluster.h>
#include "batons.h"
#include "redis.h"
#include "maps.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)
#define SLOTS_PHASES	5

/* Unfortunately there is no error code for this error to match */
#define REDIS_ENOCLUSTER     "ERR This instance has cluster support disabled"

typedef enum redisSlotsFlags {
    SLOTS_NONE		= 0,
    SLOTS_VERSION	= (1 << 0),
    SLOTS_KEYMAP	= (1 << 1),
    SLOTS_SEARCH	= (1 << 2),
} redisSlotsFlags;

typedef struct redisSlots {
    redisClusterAsyncContext *acc;	/* cluster context */
    unsigned int	setup;		/* connected to redis */
    int			cluster_mode;	/* Redis cluster mode enabled */
    redisMap		*keymap;	/* map command names to key position */
    void		*events;	/* libuv event loop */
    int			search;		/* RediSearch status */
} redisSlots;

typedef void (*redisPhase)(redisSlots *, void *);	/* phased operations */

extern redisSlots *redisSlotsInit(dict *, void *);
extern redisSlots *redisSlotsConnect(dict *, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern int redisSlotsRequest(redisSlots *, sds, redisClusterCallbackFn *, void *);
extern int redisSlotsRequestFirstNode(redisSlots *slots, const sds cmd,
                                        redisClusterCallbackFn *callback, void *arg);
extern void redisSlotsFree(redisSlots *);

extern int redisSlotsProxyConnect(redisSlots *,
		redisInfoCallBack, redisReader **, const char *, ssize_t,
		redisClusterCallbackFn *, void *);
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
