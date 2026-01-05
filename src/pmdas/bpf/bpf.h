#ifndef __BPF_H
#define __BPF_H

#include <pcp/ini.h>
#include <pcp/sds.h>
#include <pcp/dict.h>

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

dictType sdsDictCallBacks = {
    .hashFunction	= sdsHashCallBack,
    .keyCompare		= sdsCompareCallBack,
    .keyDup		= sdsDupCallBack,
    .keyDestructor	= sdsFreeCallBack,
    .valDestructor	= sdsFreeCallBack,
};

#endif
