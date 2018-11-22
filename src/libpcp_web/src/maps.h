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
#ifndef SERIES_MAPS_H
#define SERIES_MAPS_H

#include "sds.h"
#include "dict.h"

struct redisSlots;
typedef dict redisMap;
typedef dictEntry redisMapEntry;

/*
 * Mapping SHA1 hashes (identifiers) to sds strings.
 */
extern redisMap *instmap;
extern redisMap *namesmap;
extern redisMap *labelsmap;
extern redisMap *contextmap;

extern redisMap *redisMapCreate(const char *);
extern redisMapEntry *redisMapLookup(redisMap *, sds);
extern sds redisMapValue(redisMapEntry *);
extern void redisMapInsert(redisMap *, sds, sds);

/*
 * Helper utilities and data structures
 */
extern void redisMapsInit(void);
extern const char *redisMapName(redisMap *);
extern void redisMapRelease(redisMap *);

/*
 * Asynchronous mapping response helpers
 */
typedef void (*redisInfoCallBack)(pmLogLevel, sds, void *);
typedef void (*redisDoneCallBack)(void *);

typedef struct redisMapBaton {
    seriesBatonMagic	magic;		/* MAGIC_MAPPING */
    redisMap		*mapping;
    sds			mapKey;		/* 20-byte SHA1 */
    sds			mapStr;		/* string value */
    struct redisSlots	*slots;
    redisDoneCallBack	mapped;
    redisInfoCallBack	info;
    void		*userdata;
    void		*arg;
} redisMapBaton;

extern void redisGetMap(struct redisSlots *, redisMap *, unsigned char *,
		sds, redisDoneCallBack, redisInfoCallBack, void *, void *);

#endif	/* SERIES_MAPS_H */
