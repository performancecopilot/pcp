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
#include <mmv_stats.h>
#include "batons.h"
#include "redis.h"
#include "maps.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)
#define SLOTS_PHASES	5

/*
 * Unfortunately there is no error code for these errors to match
 * In LOADING case, the full error contains the key server name -
 * so here we split the check into the two guaranteed substrings.
 */
#define KEYS_ELOADING	"LOADING"
#define KEYS_ELOADDATA	"loading the dataset in memory"
#define KEYS_ENOCLUSTER	"ERR This instance has cluster support disabled"

typedef enum redisSlotsFlags {
    SLOTS_NONE		= 0,
    SLOTS_VERSION	= (1 << 0),
    SLOTS_KEYMAP	= (1 << 1),
    SLOTS_SEARCH	= (1 << 2),
} redisSlotsFlags;

enum {
    SLOT_REQUESTS_TOTAL,
    SLOT_REQUESTS_ERROR,
    SLOT_RESPONSES_TOTAL,
    SLOT_RESPONSES_ERROR,
    SLOT_RESPONSES_TIME,
    SLOT_REQUESTS_INFLIGHT_TOTAL,
    SLOT_REQUESTS_INFLIGHT_BYTES,
    SLOT_REQUESTS_TOTAL_BYTES,
    SLOT_RESPONSES_TOTAL_BYTES,
    NUM_SLOT_METRICS
};

typedef enum redisSlotsState {
    SLOTS_DISCONNECTED,
    SLOTS_CONNECTING,
    SLOTS_CONNECTED,
    SLOTS_READY		/* Redis version check done, keymap loaded, search schema setup completed */,
    SLOTS_ERR_FATAL	/* fatal error, do not try to reconnect */
} redisSlotsState;

/* note: this struct persists for reconnects */
typedef struct redisSlots {
    redisClusterAsyncContext *acc;	/* cluster context */
    redisSlotsState	state;		/* connection state */
    unsigned int	conn_seq;	/* connection sequence (incremented for every connection) */
    unsigned int	search : 1;	/* RediSearch use enabled */
    unsigned int	cluster : 1;	/* Redis cluster mode enabled */
    redisMap		*keymap;	/* map command names to key position */
    void		*events;	/* libuv event loop */
    mmv_registry_t	*registry;	/* MMV metrics for instrumentation */
    void		*map;		/* MMV mapped metric values handle */
    pmAtomValue		*metrics[NUM_SLOT_METRICS]; /* direct handle lookup */
} redisSlots;

/* wraps the actual Redis callback and data */
typedef struct redisSlotsReplyData {
    redisSlots			*slots;
    uint64_t			start;		/* time of the request (usec) */
    unsigned int		conn_seq;	/* connection sequence when this request was issued */
    size_t			req_size;	/* size of request */

    redisClusterCallbackFn	*callback;	/* actual callback */
    void			*arg;		/* actual callback args */
} redisSlotsReplyData;

typedef void (*redisPhase)(redisSlots *, void *);	/* phased operations */

extern void redisSlotsSetupMetrics(redisSlots *);
extern int redisSlotsSetMetricRegistry(redisSlots *, mmv_registry_t *);
extern redisSlots *redisSlotsInit(dict *, void *);
extern redisSlots *redisSlotsConnect(dict *, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern void redisSlotsReconnect(redisSlots *, redisSlotsFlags,
		redisInfoCallBack, redisDoneCallBack, void *, void *, void *);
extern uint64_t redisSlotsInflightRequests(redisSlots *);
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
extern void reportReplyError(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, const char *, va_list);
extern int checkStatusReplyOK(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, const char *, ...);
extern int checkStreamReplyString(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, sds, const char *, ...);
extern int checkArrayReply(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, const char *, ...);
extern long long checkIntegerReply(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, const char *, ...);
extern sds checkStringReply(redisInfoCallBack, void *,
	redisClusterAsyncContext *, redisReply *, const char *, ...);

#endif	/* SLOTS_H */
