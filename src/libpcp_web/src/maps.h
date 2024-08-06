/*
 * Copyright (c) 2017-2018,2022,2024 Red Hat.
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
#include "batons.h"

struct keySlots;
typedef dict keyMap;
typedef dictEntry keyMapEntry;

/*
 * Mapping SHA1 hashes (identifiers) to sds strings.
 */
extern keyMap *instmap;
extern keyMap *namesmap;
extern keyMap *labelsmap;
extern keyMap *contextmap;

extern keyMap *keyMapCreate(sds);
extern keyMapEntry *keyMapLookup(keyMap *, sds);
extern sds keyMapValue(keyMapEntry *);
extern void keyMapInsert(keyMap *, sds, sds);

/*
 * Helper utilities and data structures
 */
extern void keyMapsInit(void);
extern void keyMapsClose(void);
extern sds keyMapName(keyMap *);
extern void keyMapRelease(keyMap *);

/*
 * Asynchronous mapping response helpers
 */
typedef void (*keysInfoCallBack)(pmLogLevel, sds, void *);
typedef void (*keysDoneCallBack)(void *);

typedef struct keyMapBaton {
    seriesBatonMagic	magic;		/* MAGIC_MAPPING */
    keyMap		*mapping;
    sds			mapKey;		/* 20-byte SHA1 */
    sds			mapStr;		/* string value */
    struct keySlots	*slots;
    keysDoneCallBack	mapped;
    keysInfoCallBack	info;
    void		*userdata;
    void		*arg;
} keyMapBaton;

extern void keyGetMap(struct keySlots *, keyMap *, unsigned char *,
		sds, keysDoneCallBack, keysInfoCallBack, void *, void *);

#endif	/* SERIES_MAPS_H */
