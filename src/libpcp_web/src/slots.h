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
#include "keys.h"
#include "maps.h"

#define MAXSLOTS	(1 << 14)
#define SLOTMASK	(MAXSLOTS-1)
#define SLOTS_PHASES	5

typedef enum keySlotsFlags {
    SLOTS_NONE		= 0,
    SLOTS_VERSION	= (1 << 0),
    SLOTS_KEYMAP	= (1 << 1),
    SLOTS_SEARCH	= (1 << 2),
} keySlotsFlags;

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

typedef enum keySlotsState {
    SLOTS_DISCONNECTED,
    SLOTS_CONNECTING,
    SLOTS_CONNECTED,
    SLOTS_READY,	/* version check, keymap loaded, search schema done */
    SLOTS_ERR_FATAL	/* fatal error, do not try to reconnect */
} keySlotsState;

/* note: this struct persists for reconnects */
typedef struct keySlots {
    keyClusterAsyncContext *acc;	/* cluster context */
    keySlotsState	state;		/* connection state */
    unsigned int	conn_seq;	/* connection sequence (incremented for every connection) */
    unsigned int	search : 1;	/* search module enabled */
    unsigned int	cluster : 1;	/* cluster mode enabled */
    keyMap		*keymap;	/* map command names to key position */
    void		*events;	/* libuv event loop */
    mmv_registry_t	*registry;	/* MMV metrics for instrumentation */
    void		*map;		/* MMV mapped metric values handle */
    pmAtomValue		*metrics[NUM_SLOT_METRICS]; /* direct handle lookup */
} keySlots;

/* wraps the actual callback and data */
typedef struct keySlotsReplyData {
    keySlots			*slots;
    uint64_t			start;		/* time of the request (usec) */
    unsigned int		conn_seq;	/* connection sequence when this request was issued */
    size_t			req_size;	/* size of request */

    keyClusterCallbackFn	*callback;	/* actual callback */
    void			*arg;		/* actual callback args */
} keySlotsReplyData;

typedef void (*keyPhase)(keySlots *, void *);	/* phased operations */

extern void keySlotsSetupMetrics(keySlots *);
extern int keySlotsSetMetricRegistry(keySlots *, mmv_registry_t *);
extern keySlots *keySlotsInit(dict *, void *);
extern keySlots *keySlotsConnect(dict *, keySlotsFlags,
		keysInfoCallBack, keysDoneCallBack, void *, void *, void *);
extern void keySlotsReconnect(keySlots *, keySlotsFlags,
		keysInfoCallBack, keysDoneCallBack, void *, void *, void *);
extern uint64_t keySlotsInflightRequests(keySlots *);
extern int keySlotsRequest(keySlots *, sds, keyClusterCallbackFn *, void *);
extern int keySlotsRequestFirstNode(keySlots *slots, const sds cmd,
		keyClusterCallbackFn *callback, void *arg);
extern void keySlotsFree(keySlots *);

extern int keySlotsProxyConnect(keySlots *,
		keysInfoCallBack, respReader **, const char *, ssize_t,
		keyClusterCallbackFn *, void *);
extern void keySlotsProxyFree(respReader *);

typedef struct {
    seriesBatonMagic	magic;		/* MAGIC_SLOTS */
    seriesBatonPhase	*current;
    seriesBatonPhase	phases[SLOTS_PHASES];
    int			version;
    int			error;
    keySlots		*slots;
    keysInfoCallBack	info;
    keysDoneCallBack	done;
    void		*userdata;
    void		*arg;
} keySlotsBaton;

extern void keys_slots_end_phase(void *);

/* Key server reply helper routines */
extern int testReplyError(respReply *, const char *);
extern void reportReplyError(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, const char *, va_list);
extern int checkStatusReplyOK(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, const char *, ...);
extern int checkStreamReplyString(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, sds, const char *, ...);
extern int checkArrayReply(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, const char *, ...);
extern long long checkIntegerReply(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, const char *, ...);
extern sds checkStringReply(keysInfoCallBack, void *,
	keyClusterAsyncContext *, respReply *, const char *, ...);

#endif	/* SLOTS_H */
