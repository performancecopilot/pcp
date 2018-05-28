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
    redisContext	*redis;
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
    redisContext	*control;	/* initial Redis context connection */
    sds			hostspec;	/* control socket host specification */
    struct timeval	timeout;	/* system wide Redis timeout setting */
    unsigned int	readonly;	/* expect no load requests (writing) */
    redisSlotRange	*slots;		/* all instances; e.g. CLUSTER SLOTS */
} redisSlots;

extern redisSlots *redisSlotsInit(sds, struct timeval *);
extern int redisSlotRangeInsert(struct redisSlots *, struct redisSlotRange *);
extern redisContext *redisGet(struct redisSlots *, const char *, sds);
extern void redisFreeSlots(struct redisSlots *);

#endif	/* SLOTS_H */
