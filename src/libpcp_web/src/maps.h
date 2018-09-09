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
#ifndef SERIES_MAPS_H
#define SERIES_MAPS_H

#include "sds.h"
#include "dict.h"

struct redisSlots;
typedef dict redisMap;
typedef dictEntry redisMapEntry;

/*
 * Regular maps - mapping sds strings to identifiers
 */
extern redisMap *instmap;
extern redisMap *namesmap;
extern redisMap *labelsmap;
extern redisMap *contextmap;

extern redisMap *redisMapCreate(const char *);
extern redisMap *redisKeyMapCreate(const char *);
extern redisMapEntry *redisMapLookup(redisMap *, sds);
extern long long redisMapValue(redisMapEntry *);
extern void redisMapInsert(redisMap *, sds, long long);

/*
 * Reverse maps - mapping identifiers to sds strings
 */
extern redisMap *instrmap;
extern redisMap *namesrmap;
extern redisMap *labelsrmap;
extern redisMap *contextrmap;

extern redisMap *redisRMapCreate(const char *);
extern redisMapEntry *redisRMapLookup(redisMap *, const char *);
extern sds redisRMapValue(redisMapEntry *);
extern void redisRMapInsert(redisMap *, const char *, sds);

/*
 * Helper utilities applicable to all map caches
 */
extern void redisMapsInit(void);
extern const char *redisMapName(redisMap *);
extern void redisMapRelease(redisMap *);

/*
 * Asynchronous mapping response helpers
 */
typedef void (*redisInfoCallBack)(pmloglevel, sds, void *);
typedef void (*redisDoneCallBack)(void *);

typedef struct redisMapBaton {
    seriesBatonMagic	magic;		/* MAGIC_MAPPING */
    int			newMapping;
    redisMap		*mapping;
    sds			mapKey;
    long long		*mapID;
    struct redisSlots	*slots;
    int			loaded;
    int			scriptID;
    sds			scriptHash;
    redisDoneCallBack	mapped;
    redisInfoCallBack	info;
    void		*userdata;
    void		*arg;
} redisMapBaton;

extern void redisGetMap(struct redisSlots *, redisMap *, sds, long long *,
		redisDoneCallBack, redisInfoCallBack, void *, void *);

#endif	/* SERIES_MAPS_H */
