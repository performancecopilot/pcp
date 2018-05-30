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
#include "util.h"
#include "maps.h"

redisMap *instmap;
redisMap *namesmap;
redisMap *labelsmap;
redisMap *contextmap;

redisMap *instrmap;
redisMap *namesrmap;
redisMap *labelsrmap;
redisMap *contextrmap;

static uint64_t
mapHashCallBack(const void *key)
{
    return dictGenHashFunction((unsigned char *)key, sdslen((char *)key));
} 

static uint64_t
rmapHashCallBack(const void *key)
{
    return dictGenHashFunction((unsigned char *)key, sizeof(long long));
} 

static int
mapCompareCallBack(void *privdata, const void *key1, const void *key2)
{
    int		l1, l2;

    (void)privdata;
    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
}

static int
rmapCompareCallBack(void *privdata, const void *key1, const void *key2)
{
    (void)privdata;
    return memcmp(key1, key2, sizeof(long long)) == 0;
}

static void *
rmapKeyDupCallBack(void *privdata, const void *key)
{
    void	*dupkey;

    if ((dupkey = malloc(sizeof(long long))) != NULL)
	memcpy(dupkey, key, sizeof(long long));
    return dupkey;
}

static void
rmapKeyFreeCallBack(void *privdata, void *key)
{
    free(key);
}

static void *
sdsDupCallBack(void *privdata, const void *key)
{
    return sdsdup((sds)key);
}

static void
sdsFreeCallBack(void *privdata, void *val)
{
    (void)privdata;
    sdsfree(val);
}

static dictType	mapCallBackDict = {
    .hashFunction	= mapHashCallBack,
    .keyCompare		= mapCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
};

static dictType	rmapCallBackDict = {
    .hashFunction	= rmapHashCallBack,
    .keyCompare		= rmapCompareCallBack,
    .keyDup		= rmapKeyDupCallBack,
    .keyDestructor	= rmapKeyFreeCallBack,
    .valDestructor	= sdsFreeCallBack,
};

void
redisMapsInit(void)
{
    static const char * const mapnames[] = {
	"inst.name", "metric.name", "label.name", "context.name"
    };

    instmap = dictCreate(&mapCallBackDict, (void *)mapnames[0]);
    instrmap = dictCreate(&rmapCallBackDict, (void *)mapnames[0]);
    namesmap = dictCreate(&mapCallBackDict, (void *)mapnames[1]);
    namesrmap = dictCreate(&rmapCallBackDict, (void *)mapnames[1]);
    labelsmap = dictCreate(&mapCallBackDict, (void *)mapnames[2]);
    labelsrmap = dictCreate(&rmapCallBackDict, (void *)mapnames[2]);
    contextmap = dictCreate(&mapCallBackDict, (void *)mapnames[3]);
    contextrmap = dictCreate(&rmapCallBackDict, (void *)mapnames[3]);
}

const char *
redisMapName(redisMap *map)
{
    return (const char *)map->privdata;
}

void
redisMapRelease(redisMap *map)
{
    dictRelease(map);
}

/*
 * Regular map interfaces
 */
redisMap *
redisMapCreate(const char *name)
{
    return dictCreate(&mapCallBackDict, (void *)name);
}

redisMapEntry *
redisMapLookup(redisMap *map, sds key)
{
    if (map)
	return dictFind(map, key);
    return NULL;
}

void
redisMapInsert(redisMap *map, sds key, long long value)
{
    redisMapEntry	*entry;

    if (map && ((entry = dictAddRaw(map, key, NULL)) != NULL))
	dictSetSignedIntegerVal(entry, value);
}

long long
redisMapValue(redisMapEntry *entry)
{
    return entry->v.s64;
}

/*
 * Reverse map interfaces
 */
redisMap *
redisRMapCreate(const char *name)
{
    return dictCreate(&rmapCallBackDict, (void *)name);
}

redisMapEntry *
redisRMapLookup(redisMap *map, const char *key)
{
    long long		llkey = strtoll(key, NULL, 10);

    if (map)
	return dictFind(map, &llkey);
    return NULL;
}

void
redisRMapInsert(redisMap *map, const char *key, sds value)
{
    long long		llkey = strtoll(key, NULL, 10);

    dictAdd(map, &llkey, value);
}

sds
redisRMapValue(redisMapEntry *entry)
{
    return (sds)entry->v.val;
}
