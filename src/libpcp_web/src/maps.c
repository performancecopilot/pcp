/*
 * Copyright (c) 2017-2019,2022,2024 Red Hat.
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
#include "pmapi.h"
#include "libpcp.h"
#include "pmwebapi.h"
#include "slots.h"
#include "util.h"
#include "maps.h"

/* reverse hash mapping of all SHA1 hashes to strings */
keyMap *instmap;
keyMap *namesmap;
keyMap *labelsmap;
keyMap *contextmap;

static uint64_t
intHashCallBack(const void *key)
{
    const unsigned int	*i = (const unsigned int *)key;

    return dictGenHashFunction((const unsigned char *)i, sizeof(unsigned int));
}

static int
intCmpCallBack(const void *a, const void *b)
{
    const unsigned int	*ia = (const unsigned int *)a;
    const unsigned int	*ib = (const unsigned int *)b;

    return (*ia == *ib);
}

static void *
intDupCallBack(const void *key)
{
    const unsigned int	*i = (const unsigned int *)key;
    unsigned int	*k = (unsigned int *)malloc(sizeof(*i));

    if (k)
	*k = *i;
    return k;
}

static void
intFreeCallBack(void *value)
{
    if (value) free(value);
}

dictType intKeyDictCallBacks = {
    .hashFunction	= intHashCallBack,
    .keyCompare		= intCmpCallBack,
    .keyDup		= intDupCallBack,
    .keyDestructor	= intFreeCallBack,
};

static uint64_t
sdsHashCallBack(const void *key)
{
    return dictGenHashFunction((unsigned char *)key, sdslen((char *)key));
}

static int
sdsCompareCallBack(const void *key1, const void *key2)
{
    int		l1, l2;

    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

static void *
sdsDupCallBack(const void *key)
{
    return sdsdup((sds)key);
}

static void
sdsFreeCallBack(void *val)
{
    sdsfree(val);
}

dictType sdsKeyDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
};

dictType sdsOwnDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDestructor	= sdsFreeCallBack,
    .valDestructor	= sdsFreeCallBack,
};

dictType sdsDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
    .valDestructor	= sdsFreeCallBack,
};

void
keyMapsInit(void)
{
    if (instmap == NULL) {
	instmap = malloc(sizeof(keyMap));
	instmap->dict = dictCreate(&sdsDictCallBacks);
	instmap->privdata = sdsnew("inst.name");
    }
    if (namesmap == NULL) {
	namesmap = malloc(sizeof(keyMap));
	namesmap->dict = dictCreate(&sdsDictCallBacks);
	namesmap->privdata = sdsnew("metric.name");
    }
    if (labelsmap == NULL) {
	labelsmap = malloc(sizeof(keyMap));
	labelsmap->dict = dictCreate(&sdsDictCallBacks);
	labelsmap->privdata = sdsnew("label.name");
    }
    if (contextmap == NULL) {
	contextmap = malloc(sizeof(keyMap));
	contextmap->dict = dictCreate(&sdsDictCallBacks);
	contextmap->privdata = sdsnew("context.name");
    }
}

void
keyMapsClose(void)
{
    if (instmap) {
	sdsfree((sds)instmap->privdata);
	dictRelease(instmap->dict);
	free(instmap);
	instmap = NULL;
    }
    if (namesmap) {
	sdsfree((sds)namesmap->privdata);
	dictRelease(namesmap->dict);
	free(namesmap);
	namesmap = NULL;
    }
    if (labelsmap) {
	sdsfree((sds)labelsmap->privdata);
	dictRelease(labelsmap->dict);
	free(labelsmap);
	labelsmap = NULL;
    }
    if (contextmap) {
	sdsfree((sds)contextmap->privdata);
	dictRelease(contextmap->dict);
	free(contextmap);
	contextmap = NULL;
    }
}

sds
keyMapName(keyMap *map)
{
    return (sds)map->privdata;
}

keyMap *
keyMapCreate(sds name)
{
    keyMap *map = malloc(sizeof(keyMap));
    map->dict = dictCreate(&sdsDictCallBacks);
    map->privdata = name;
    return map;
}

keyMapEntry *
keyMapLookup(keyMap *map, sds key)
{
    if (map && map->dict)
	return dictFind(map->dict, key);
    return NULL;
}

void
keyMapInsert(keyMap *map, sds key, sds value)
{
    keyMapEntry *entry = keyMapLookup(map, key);

    if (entry) {
	/* fix for Coverity CID323605 Resource Leak */
	dictDelete(map->dict, key);
    }
    dictAdd(map->dict, key, value);
}

sds
keyMapValue(keyMapEntry *entry)
{
    return (sds)dictGetVal(entry);
}

void
keyMapRelease(keyMap *map)
{
    if (map) {
	sdsfree((sds)map->privdata);
	dictRelease(map->dict);
	free(map);
    }
}
