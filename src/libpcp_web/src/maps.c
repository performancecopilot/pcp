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
#include "pmwebapi.h"
#include "slots.h"
#include "util.h"
#include "maps.h"

/* reverse hash mapping of all SHA1 hashes to strings */
redisMap *instmap;
redisMap *namesmap;
redisMap *labelsmap;
redisMap *contextmap;

static uint64_t
intHashCallBack(const void *key)
{
    const unsigned int	*i = (const unsigned int *)key;

    return dictGenHashFunction(i, sizeof(unsigned int));
}

static int
intCmpCallBack(void *privdata, const void *a, const void *b)
{
    const unsigned int	*ia = (const unsigned int *)a;
    const unsigned int	*ib = (const unsigned int *)b;

    (void)privdata;
    return (*ia == *ib);
}

static void *
intDupCallBack(void *privdata, const void *key)
{
    unsigned int	*i = (unsigned int *)key;
    unsigned int	*k = (unsigned int *)malloc(sizeof(*i));

    (void)privdata;
    if (k)
	*k = *i;
    return k;
}

static void
intFreeCallBack(void *privdata, void *value)
{
    (void)privdata;
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
sdsCompareCallBack(void *privdata, const void *key1, const void *key2)
{
    int		l1, l2;

    (void)privdata;
    l1 = sdslen((sds)key1);
    l2 = sdslen((sds)key2);
    if (l1 != l2) return 0;
    return memcmp(key1, key2, l1) == 0;
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

dictType sdsKeyDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
};

dictType sdsDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
    .valDestructor	= sdsFreeCallBack,
};

void
redisMapsInit(void)
{
    static const char * const mapnames[] = {
	"inst.name", "metric.name", "label.name", "context.name"
    };
    static int		setup;

    if (setup)
	return;
    setup = 1;

    instmap = dictCreate(&sdsDictCallBacks, (void *)mapnames[0]);
    namesmap = dictCreate(&sdsDictCallBacks, (void *)mapnames[1]);
    labelsmap = dictCreate(&sdsDictCallBacks, (void *)mapnames[2]);
    contextmap = dictCreate(&sdsDictCallBacks, (void *)mapnames[3]);
}

redisMap *
redisKeyMapCreate(const char *name)
{
    return dictCreate(&sdsDictCallBacks, (void *)name);
}

const char *
redisMapName(redisMap *map)
{
    return (const char *)map->privdata;
}

redisMap *
redisMapCreate(const char *name)
{
    return dictCreate(&sdsDictCallBacks, (void *)name);
}

redisMapEntry *
redisMapLookup(redisMap *map, sds key)
{
    if (map)
	return dictFind(map, key);
    return NULL;
}

void
redisMapInsert(redisMap *map, sds key, sds value)
{
    dictAdd(map, key, value);
}

sds
redisMapValue(redisMapEntry *entry)
{
    return (sds)dictGetVal(entry);
}

void
redisMapRelease(redisMap *map)
{
    dictRelease(map);
}
