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

#define MAXSLOTS	(1<<14)	/* see CLUSTER_SLOTS in Redis sources */
#define SLOTMASK	(MAXSLOTS-1)

typedef struct redisSlots {
    redisContext	*contexts[MAXSLOTS];
    const char		*hostspec;
    struct timeval	timeout;
} redisSlots;

extern struct redisSlots *redisInitSlots(const char *);
extern void redisFreeSlots(struct redisSlots *);
extern redisContext *redisGet(struct redisSlots *, const char *, unsigned int);
extern redisSlots *redisSlotsInit(const char*, struct timeval *);
extern unsigned int keySlot(const char *, unsigned int);

#endif	/* SLOTS_H */
