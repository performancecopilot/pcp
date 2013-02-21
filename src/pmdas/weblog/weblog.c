/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "weblog.h"
#include "domain.h"
#include <ctype.h>
#include <sys/stat.h>
#if defined(HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif
#if defined(HAVE_SYS_PRCTL_H)
#include <sys/prctl.h>
#endif
#if defined(HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

/*
 * Types of metrics, used by fetch to more efficiently calculate metrics
 */

enum MetaType {
    wl_globalPtr, wl_offset32, wl_offset64, wl_totalAggregate, 
    wl_serverAggregate, wl_requestMethod, wl_bytesMethod,
    wl_requestSize, wl_requestCachedSize, wl_requestUncachedSize,
    wl_bytesSize, wl_bytesCachedSize, wl_bytesUncachedSize,
    wl_watched, wl_numAlive, wl_nosupport, wl_numMetaTypes
};

/*
 * Return code of checkLogFile()
 */

enum LogFileCode {
    wl_ok, wl_opened, wl_reopened, wl_closed, wl_unableToOpen, wl_unableToStat,
    wl_irregularFile, wl_dormant
};

static WebCount    dummyCount;

/*
 * Instance domain table
 * This is completed when parsing the config file
 */

pmdaIndom wl_indomTable[] =
{
#define WEBLOG_INDOM 0
    { WEBLOG_INDOM, 0, 0 }
};

/* 
 * Metric specific data to help identify each metric during a fetch
 *
 * MG: Note: must be in the same order as wl_metric table below.
 *
 */

typedef struct {
    int		m_type;
    __psint_t	m_offset;
} WebMetric;

WebMetric wl_metricInfo[] = 
{
/* config.numservers */
    { wl_globalPtr, (__psint_t)&wl_numServers },
/* config.catchup */
    { wl_globalPtr, (__psint_t)&wl_refreshDelay },
/* config.catchuptime */
    { wl_globalPtr, (__psint_t)&wl_catchupTime },
/* config.check */
    { wl_globalPtr, (__psint_t)&wl_chkDelay },
/* allserves.numwatched */
    { wl_globalPtr, (__psint_t)&wl_numActive },
/* allserves.numalive */
    { wl_numAlive, (__psint_t)0 },
/* allservers.errors */
    { wl_totalAggregate, (__psint_t)0 },
/* allservers.requests.total */
    { wl_totalAggregate, (__psint_t)1 },
/* allservers.requests.get */
    { wl_requestMethod, (__psint_t)wl_httpGet },
/* allservers.requests.head */
    { wl_requestMethod, (__psint_t)wl_httpHead },
/* allservers.requests.post */
    { wl_requestMethod, (__psint_t)wl_httpPost },
/* allservers.requests.other */
    { wl_requestMethod, (__psint_t)wl_httpOther },
/* allservers.bytes.total */
    { wl_totalAggregate, (__psint_t)2 },
/* allservers.bytes.get */
    { wl_bytesMethod, (__psint_t)wl_httpGet },
/* allservers.bytes.head */
    { wl_bytesMethod, (__psint_t)wl_httpHead },
/* allservers.bytes.post */
    { wl_bytesMethod, (__psint_t)wl_httpPost },
/* allservers.bytes.other */
    { wl_bytesMethod, (__psint_t)wl_httpOther },
/* allservers.requests.size.zero */
    { wl_requestSize, (__psint_t)wl_zero },
/* allservers.requests.size.le3k */
    { wl_requestSize, (__psint_t)wl_le3k },
/* allservers.requests.size.le10k */
    { wl_requestSize, (__psint_t)wl_le10k },
/* allservers.requests.size.le30k */
    { wl_requestSize, (__psint_t)wl_le30k },
/* allservers.requests.size.le100k */
    { wl_requestSize, (__psint_t)wl_le100k },
/* allservers.requests.size.le300k */
    { wl_requestSize, (__psint_t)wl_le300k },
/* allservers.requests.size.le1m */
    { wl_requestSize, (__psint_t)wl_le1m },
/* allservers.requests.size.le3m */
    { wl_requestSize, (__psint_t)wl_le3m },
/* allservers.requests.size.gt3m */
    { wl_requestSize, (__psint_t)wl_gt3m },
/* allservers.requests.size.unknown */
    { wl_requestSize, (__psint_t)wl_unknownSize },
/* allservers.requests.client.total */
    { wl_totalAggregate, (__psint_t)3 },
/* allservers.requests.cached.total */
    { wl_totalAggregate, (__psint_t)4 },
/* allservers.requests.cached.size.zero */
    { wl_requestCachedSize, (__psint_t)wl_zero },
/* allservers.requests.cached.size.le3k */
    { wl_requestCachedSize, (__psint_t)wl_le3k },
/* allservers.requests.cached.size.le10k */
    { wl_requestCachedSize, (__psint_t)wl_le10k },
/* allservers.requests.cached.size.le30k */
    { wl_requestCachedSize, (__psint_t)wl_le30k },
/* allservers.requests.cached.size.le100k */
    { wl_requestCachedSize, (__psint_t)wl_le100k },
/* allservers.requests.cached.size.le300k */
    { wl_requestCachedSize, (__psint_t)wl_le300k },
/* allservers.requests.cached.size.le1m */
    { wl_requestCachedSize, (__psint_t)wl_le1m },
/* allservers.requests.cached.size.le3m */
    { wl_requestCachedSize, (__psint_t)wl_le3m },
/* allservers.requests.cached.size.gt3m */
    { wl_requestCachedSize, (__psint_t)wl_gt3m },
/* allservers.requests.cached.size.unknown */
    { wl_requestCachedSize, (__psint_t)wl_unknownSize },
/* allservers.requests.uncached.total */
    { wl_totalAggregate, (__psint_t)5 },
/* allservers.requests.uncached.size.zero */
    { wl_requestUncachedSize, (__psint_t)wl_zero },
/* allservers.requests.uncached.size.le3k */
    { wl_requestUncachedSize, (__psint_t)wl_le3k },
/* allservers.requests.uncached.size.le10k */
    { wl_requestUncachedSize, (__psint_t)wl_le10k },
/* allservers.requests.uncached.size.le30k */
    { wl_requestUncachedSize, (__psint_t)wl_le30k },
/* allservers.requests.uncached.size.le100k */
    { wl_requestUncachedSize, (__psint_t)wl_le100k },
/* allservers.requests.uncached.size.le300k */
    { wl_requestUncachedSize, (__psint_t)wl_le300k },
/* allservers.requests.uncached.size.le1m */
    { wl_requestUncachedSize, (__psint_t)wl_le1m },
/* allservers.requests.uncached.size.le3m */
    { wl_requestUncachedSize, (__psint_t)wl_le3m },
/* allservers.requests.uncached.size.gt3m */
    { wl_requestUncachedSize, (__psint_t)wl_gt3m },
/* allservers.requests.uncached.size.unknown */
    { wl_requestUncachedSize, (__psint_t)wl_unknownSize },
/* allservers.bytes.size.zero */
    { wl_bytesSize, (__psint_t)wl_zero },
/* allservers.bytes.size.le3k */
    { wl_bytesSize, (__psint_t)wl_le3k },
/* allservers.bytes.size.le10k */
    { wl_bytesSize, (__psint_t)wl_le10k },
/* allservers.bytes.size.le30k */
    { wl_bytesSize, (__psint_t)wl_le30k },
/* allservers.bytes.size.le100k */
    { wl_bytesSize, (__psint_t)wl_le100k },
/* allservers.bytes.size.le300k */
    { wl_bytesSize, (__psint_t)wl_le300k },
/* allservers.bytes.size.le1m */
    { wl_bytesSize, (__psint_t)wl_le1m },
/* allservers.bytes.size.le3m */
    { wl_bytesSize, (__psint_t)wl_le3m },
/* allservers.bytes.size.gt3m */
    { wl_bytesSize, (__psint_t)wl_gt3m },
/* allservers.bytes.cached.total */
    { wl_totalAggregate, (__psint_t)6 },
/* allservers.bytes.cached.size.zero */
    { wl_bytesCachedSize, (__psint_t)wl_zero },
/* allservers.bytes.cached.size.le3k */
    { wl_bytesCachedSize, (__psint_t)wl_le3k },
/* allservers.bytes.cached.size.le10k */
    { wl_bytesCachedSize, (__psint_t)wl_le10k },
/* allservers.bytes.cached.size.le30k */
    { wl_bytesCachedSize, (__psint_t)wl_le30k },
/* allservers.bytes.cached.size.le100k */
    { wl_bytesCachedSize, (__psint_t)wl_le100k },
/* allservers.bytes.cached.size.le300k */
    { wl_bytesCachedSize, (__psint_t)wl_le300k },
/* allservers.bytes.cached.size.le1m */
    { wl_bytesCachedSize, (__psint_t)wl_le1m },
/* allservers.bytes.cached.size.le3m */
    { wl_bytesCachedSize, (__psint_t)wl_le3m },
/* allservers.bytes.cached.size.gt3m */
    { wl_bytesCachedSize, (__psint_t)wl_gt3m },
/* allservers.bytes.uncached.total */
    { wl_totalAggregate, (__psint_t)7 },
/* allservers.bytes.uncached.size.zero */
    { wl_bytesUncachedSize, (__psint_t)wl_zero },
/* allservers.bytes.uncached.size.le3k */
    { wl_bytesUncachedSize, (__psint_t)wl_le3k },
/* allservers.bytes.uncached.size.le10k */
    { wl_bytesUncachedSize, (__psint_t)wl_le10k },
/* allservers.bytes.uncached.size.le30k */
    { wl_bytesUncachedSize, (__psint_t)wl_le30k },
/* allservers.bytes.uncached.size.le100k */
    { wl_bytesUncachedSize, (__psint_t)wl_le100k },
/* allservers.bytes.uncached.size.le300k */
    { wl_bytesUncachedSize, (__psint_t)wl_le300k },
/* allservers.bytes.uncached.size.le1m */
    { wl_bytesUncachedSize, (__psint_t)wl_le1m },
/* allservers.bytes.uncached.size.le3m */
    { wl_bytesUncachedSize, (__psint_t)wl_le3m },
/* allservers.bytes.uncached.size.gt3m */
    { wl_bytesUncachedSize, (__psint_t)wl_gt3m },
/* perserver.watched */
    { wl_watched, (__psint_t)&dummyCount.active },
/* perserver.numlogs */
    { wl_offset32, (__psint_t)&dummyCount.numLogs },
/* perserver.errors */
    { wl_offset32, (__psint_t)&dummyCount.errors },
/* perserver.requests.total */
    { wl_serverAggregate, (__psint_t)0 },
/* perserver.requests.get */
    { wl_requestMethod, (__psint_t)wl_httpGet },
/* perserver.requests.head */
    { wl_requestMethod, (__psint_t)wl_httpHead },
/* perserver.requests.post */
    { wl_requestMethod, (__psint_t)wl_httpPost },
/* perserver.requests.other */
    { wl_requestMethod, (__psint_t)wl_httpOther },
/* perserver.bytes.total */
    { wl_serverAggregate, (__psint_t)1 },
/* perserver.bytes.get */
    { wl_bytesMethod, (__psint_t)wl_httpGet },
/* perserver.bytes.head */
    { wl_bytesMethod, (__psint_t)wl_httpHead },
/* perserver.bytes.post */
    { wl_bytesMethod, (__psint_t)wl_httpPost },
/* perserver.bytes.other */
    { wl_bytesMethod, (__psint_t)wl_httpOther },
/* perserver.requests.size.zero */
    { wl_requestSize, (__psint_t)wl_zero },
/* perserver.requests.size.le3k */
    { wl_requestSize, (__psint_t)wl_le3k },
/* perserver.requests.size.le10k */
    { wl_requestSize, (__psint_t)wl_le10k },
/* perserver.requests.size.le30k */
    { wl_requestSize, (__psint_t)wl_le30k },
/* perserver.requests.size.le100k */
    { wl_requestSize, (__psint_t)wl_le100k },
/* perserver.requests.size.le300k */
    { wl_requestSize, (__psint_t)wl_le300k },
/* perserver.requests.size.le1m */
    { wl_requestSize, (__psint_t)wl_le1m },
/* perserver.requests.size.le3m */
    { wl_requestSize, (__psint_t)wl_le3m },
/* perserver.requests.size.gt3m */
    { wl_requestSize, (__psint_t)wl_gt3m },
/* perserver.requests.size.unknown */
    { wl_requestSize, (__psint_t)wl_unknownSize },
/* perserver.requests.client.total */
    { wl_serverAggregate, (__psint_t)2 },
/* perserver.requests.cached.total */
    { wl_serverAggregate, (__psint_t)3 },
/* perserver.requests.cached.size.zero */
    { wl_requestCachedSize, (__psint_t)wl_zero },
/* perserver.requests.cached.size.le3k */
    { wl_requestCachedSize, (__psint_t)wl_le3k },
/* perserver.requests.cached.size.le10k */
    { wl_requestCachedSize, (__psint_t)wl_le10k },
/* perserver.requests.cached.size.le30k */
    { wl_requestCachedSize, (__psint_t)wl_le30k },
/* perserver.requests.cached.size.le100k */
    { wl_requestCachedSize, (__psint_t)wl_le100k },
/* perserver.requests.cached.size.le300k */
    { wl_requestCachedSize, (__psint_t)wl_le300k },
/* perserver.requests.cached.size.le1m */
    { wl_requestCachedSize, (__psint_t)wl_le1m },
/* perserver.requests.cached.size.le3m */
    { wl_requestCachedSize, (__psint_t)wl_le3m },
/* perserver.requests.cached.size.gt3m */
    { wl_requestCachedSize, (__psint_t)wl_gt3m },
/* perserver.requests.cached.size.unknown */
    { wl_requestCachedSize, (__psint_t)wl_unknownSize },
/* perserver.requests.uncached.total */
    { wl_serverAggregate, (__psint_t)4 },
/* perserver.requests.uncached.size.zero */
    { wl_requestUncachedSize, (__psint_t)wl_zero },
/* perserver.requests.uncached.size.le3k */
    { wl_requestUncachedSize, (__psint_t)wl_le3k },
/* perserver.requests.uncached.size.le10k */
    { wl_requestUncachedSize, (__psint_t)wl_le10k },
/* perserver.requests.uncached.size.le30k */
    { wl_requestUncachedSize, (__psint_t)wl_le30k },
/* perserver.requests.uncached.size.le100k */
    { wl_requestUncachedSize, (__psint_t)wl_le100k },
/* perserver.requests.uncached.size.le300k */
    { wl_requestUncachedSize, (__psint_t)wl_le300k },
/* perserver.requests.uncached.size.le1m */
    { wl_requestUncachedSize, (__psint_t)wl_le1m },
/* perserver.requests.uncached.size.le3m */
    { wl_requestUncachedSize, (__psint_t)wl_le3m },
/* perserver.requests.uncached.size.gt3m */
    { wl_requestUncachedSize, (__psint_t)wl_gt3m },
/* perserver.requests.uncached.size.unknown */
    { wl_requestUncachedSize, (__psint_t)wl_unknownSize },
/* perserver.bytes.size.zero */
    { wl_bytesSize, (__psint_t)wl_zero },
/* perserver.bytes.size.le3k */
    { wl_bytesSize, (__psint_t)wl_le3k },
/* perserver.bytes.size.le10k */
    { wl_bytesSize, (__psint_t)wl_le10k },
/* perserver.bytes.size.le30k */
    { wl_bytesSize, (__psint_t)wl_le30k },
/* perserver.bytes.size.le100k */
    { wl_bytesSize, (__psint_t)wl_le100k },
/* perserver.bytes.size.le300k */
    { wl_bytesSize, (__psint_t)wl_le300k },
/* perserver.bytes.size.le1m */
    { wl_bytesSize, (__psint_t)wl_le1m },
/* perserver.bytes.size.le3m */
    { wl_bytesSize, (__psint_t)wl_le3m },
/* perserver.bytes.size.gt3m */
    { wl_bytesSize, (__psint_t)wl_gt3m },
/* perserver.bytes.cached.total */
    { wl_serverAggregate, (__psint_t)5 },
/* perserver.bytes.cached.size.zero */
    { wl_bytesCachedSize, (__psint_t)wl_zero },
/* perserver.bytes.cached.size.le3k */
    { wl_bytesCachedSize, (__psint_t)wl_le3k },
/* perserver.bytes.cached.size.le10k */
    { wl_bytesCachedSize, (__psint_t)wl_le10k },
/* perserver.bytes.cached.size.le30k */
    { wl_bytesCachedSize, (__psint_t)wl_le30k },
/* perserver.bytes.cached.size.le100k */
    { wl_bytesCachedSize, (__psint_t)wl_le100k },
/* perserver.bytes.cached.size.le300k */
    { wl_bytesCachedSize, (__psint_t)wl_le300k },
/* perserver.bytes.cached.size.le1m */
    { wl_bytesCachedSize, (__psint_t)wl_le1m },
/* perserver.bytes.cached.size.le3m */
    { wl_bytesCachedSize, (__psint_t)wl_le3m },
/* perserver.bytes.cached.size.gt3m */
    { wl_bytesCachedSize, (__psint_t)wl_gt3m },
/* perserver.bytes.uncached.total */
    { wl_serverAggregate, (__psint_t)6 },
/* perserver.bytes.uncached.size.zero */
    { wl_bytesUncachedSize, (__psint_t)wl_zero },
/* perserver.bytes.uncached.size.le3k */
    { wl_bytesUncachedSize, (__psint_t)wl_le3k },
/* perserver.bytes.uncached.size.le10k */
    { wl_bytesUncachedSize, (__psint_t)wl_le10k },
/* perserver.bytes.uncached.size.le30k */
    { wl_bytesUncachedSize, (__psint_t)wl_le30k },
/* perserver.bytes.uncached.size.le100k */
    { wl_bytesUncachedSize, (__psint_t)wl_le100k },
/* perserver.bytes.uncached.size.le300k */
    { wl_bytesUncachedSize, (__psint_t)wl_le300k },
/* perserver.bytes.uncached.size.le1m */
    { wl_bytesUncachedSize, (__psint_t)wl_le1m },
/* perserver.bytes.uncached.size.le3m */
    { wl_bytesUncachedSize, (__psint_t)wl_le3m },
/* perserver.bytes.uncached.size.gt3m */
    { wl_bytesUncachedSize, (__psint_t)wl_gt3m },
/* perserver.logidletime */
    { wl_offset32, (__psint_t)&dummyCount.modTime },
};

/*
 * all metrics supported in this PMDA - one table entry for each
 */

static pmdaMetric    wl_metrics[] = {

/*
 * web.config
 */

/* config.numservers */
{ (void *)0,
    { PMDA_PMID(0,0), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, 0) } },

/* config.catchup */
{ (void *)0,
    { PMDA_PMID(0,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) } },

/* config.catchuptime */
{ (void *)0,
    { PMDA_PMID(0,2), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) } },

/* config.check */
{ (void *)0,
    { PMDA_PMID(0,3), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) } },

/*
 * web.allservers
 */

/* allserves.numwatched */
{ (void *)0,
    { PMDA_PMID(0,4), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, 0) } },

/* allserves.numalive */
{ (void *)0,
    { PMDA_PMID(0,5), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, 0) } },

/* allservers.errors */
{ (void *)0,
    { PMDA_PMID(1,6), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.total */
{ (void *)0,
    { PMDA_PMID(1,7), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.get */
{ (void *)0,
    { PMDA_PMID(1,8), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.head */
{ (void *)0,
    { PMDA_PMID(1,9), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.post */
{ (void *)0,
    { PMDA_PMID(1,10), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.other */
{ (void *)0,
    { PMDA_PMID(1,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.bytes.total */
{ (void *)0,
    { PMDA_PMID(1,12), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.get */
{ (void *)0,
    { PMDA_PMID(1,13), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.head */
{ (void *)0,
    { PMDA_PMID(1,14), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.post */
{ (void *)0,
    { PMDA_PMID(1,15), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.other */
{ (void *)0,
    { PMDA_PMID(1,16), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.requests.size.zero */
{ (void *)0,
    { PMDA_PMID(1,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le3k */
{ (void *)0,
    { PMDA_PMID(1,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le10k */
{ (void *)0,
    { PMDA_PMID(1,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le30k */
{ (void *)0,
    { PMDA_PMID(1,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le100k */
{ (void *)0,
    { PMDA_PMID(1,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le300k */
{ (void *)0,
    { PMDA_PMID(1,22), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le1m */
{ (void *)0,
    { PMDA_PMID(1,23), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.le3m */
{ (void *)0,
    { PMDA_PMID(1,24), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.gt3m */
{ (void *)0,
    { PMDA_PMID(1,25), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.size.unknown */
{ (void *)0,
    { PMDA_PMID(1,66), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.client.total */
{ (void *)0,
    { PMDA_PMID(3,1), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.total */
{ (void *)0,
    { PMDA_PMID(3,11), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.zero */
{ (void *)0,
    { PMDA_PMID(3,12), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le3k */
{ (void *)0,
    { PMDA_PMID(3,13), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le10k */
{ (void *)0,
    { PMDA_PMID(3,14), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le30k */
{ (void *)0,
    { PMDA_PMID(3,15), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le100k */
{ (void *)0,
    { PMDA_PMID(3,16), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le300k */
{ (void *)0,
    { PMDA_PMID(3,17), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le1m */
{ (void *)0,
    { PMDA_PMID(3,18), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.le3m */
{ (void *)0,
    { PMDA_PMID(3,19), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(3,20), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.cached.size.unknown */
{ (void *)0,
    { PMDA_PMID(3,21), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.total */
{ (void *)0,
    { PMDA_PMID(3,31), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.zero */
{ (void *)0,
    { PMDA_PMID(3,32), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le3k */
{ (void *)0,
    { PMDA_PMID(3,33), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le10k */
{ (void *)0,
    { PMDA_PMID(3,34), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le30k */
{ (void *)0,
    { PMDA_PMID(3,35), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le100k */
{ (void *)0,
    { PMDA_PMID(3,36), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le300k */
{ (void *)0,
    { PMDA_PMID(3,37), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le1m */
{ (void *)0,
    { PMDA_PMID(3,38), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.le3m */
{ (void *)0,
    { PMDA_PMID(3,39), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(3,40), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.requests.uncached.size.unknown */
{ (void *)0,
    { PMDA_PMID(3,41), PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* allservers.bytes.size.zero */
{ (void *)0,
    { PMDA_PMID(1,26), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le3k */
{ (void *)0,
    { PMDA_PMID(1,27), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le10k */
{ (void *)0,
    { PMDA_PMID(1,28), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le30k */
{ (void *)0,
    { PMDA_PMID(1,29), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le100k */
{ (void *)0,
    { PMDA_PMID(1,30), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le300k */
{ (void *)0,
    { PMDA_PMID(1,31), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le1m */
{ (void *)0,
    { PMDA_PMID(1,32), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.le3m */
{ (void *)0,
    { PMDA_PMID(1,33), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.size.gt3m */
{ (void *)0,
    { PMDA_PMID(1,34), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.total */
{ (void *)0,
    { PMDA_PMID(3,51), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.zero */
{ (void *)0,
    { PMDA_PMID(3,52), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le3k */
{ (void *)0,
    { PMDA_PMID(3,53), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le10k */
{ (void *)0,
    { PMDA_PMID(3,54), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le30k */
{ (void *)0,
    { PMDA_PMID(3,55), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le100k */
{ (void *)0,
    { PMDA_PMID(3,56), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le300k */
{ (void *)0,
    { PMDA_PMID(3,57), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le1m */
{ (void *)0,
    { PMDA_PMID(3,58), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.le3m */
{ (void *)0,
    { PMDA_PMID(3,59), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.cached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(3,60), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.total */
{ (void *)0,
    { PMDA_PMID(3,71), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.zero */
{ (void *)0,
    { PMDA_PMID(3,72), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le3k */
{ (void *)0,
    { PMDA_PMID(3,73), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le10k */
{ (void *)0,
    { PMDA_PMID(3,74), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le30k */
{ (void *)0,
    { PMDA_PMID(3,75), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le100k */
{ (void *)0,
    { PMDA_PMID(3,76), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le300k */
{ (void *)0,
    { PMDA_PMID(3,77), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le1m */
{ (void *)0,
    { PMDA_PMID(3,78), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.le3m */
{ (void *)0,
    { PMDA_PMID(3,79), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* allservers.bytes.uncached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(3,80), PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/*
 * web.perserver
 */

/* perserver.watched */
{ (void *)0,
    { PMDA_PMID(0,35), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 0, 0, 0, 0, 0) } },

/* perserver.numlogs */
{ (void *)0,
    { PMDA_PMID(2,36), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.errors */
{ (void *)0,
    { PMDA_PMID(2,37), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.total */
{ (void *)0,
    { PMDA_PMID(2,38), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.get */
{ (void *)0,
    { PMDA_PMID(2,39), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.head */
{ (void *)0,
    { PMDA_PMID(2,40), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.post */
{ (void *)0,
    { PMDA_PMID(2,41), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.other */
{ (void *)0,
    { PMDA_PMID(2,42), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.bytes.total */
{ (void *)0,
    { PMDA_PMID(2,43), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.get */
{ (void *)0,
    { PMDA_PMID(2,44), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.head */
{ (void *)0,
    { PMDA_PMID(2,45), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.post */
{ (void *)0,
    { PMDA_PMID(2,46), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.other */
{ (void *)0,
    { PMDA_PMID(2,47), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.requests.size.zero */
{ (void *)0,
    { PMDA_PMID(2,48), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le3k */
{ (void *)0,
    { PMDA_PMID(2,49), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le10k */
{ (void *)0,
    { PMDA_PMID(2,50), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le30k */
{ (void *)0,
    { PMDA_PMID(2,51), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le100k */
{ (void *)0,
    { PMDA_PMID(2,52), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le300k */
{ (void *)0,
    { PMDA_PMID(2,53), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le1m */
{ (void *)0,
    { PMDA_PMID(2,54), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.le3m */
{ (void *)0,
    { PMDA_PMID(2,55), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.gt3m */
{ (void *)0,
    { PMDA_PMID(2,56), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.size.unknown */
{ (void *)0,
    { PMDA_PMID(2,67), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.client.total */
{ (void *)0,
    { PMDA_PMID(4,1), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.total */
{ (void *)0,
    { PMDA_PMID(4,11), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.zero */
{ (void *)0,
    { PMDA_PMID(4,12), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le3k */
{ (void *)0,
    { PMDA_PMID(4,13), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le10k */
{ (void *)0,
    { PMDA_PMID(4,14), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le30k */
{ (void *)0,
    { PMDA_PMID(4,15), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le100k */
{ (void *)0,
    { PMDA_PMID(4,16), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le300k */
{ (void *)0,
    { PMDA_PMID(4,17), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le1m */
{ (void *)0,
    { PMDA_PMID(4,18), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.le3m */
{ (void *)0,
    { PMDA_PMID(4,19), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(4,20), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.cached.size.unknown */
{ (void *)0,
    { PMDA_PMID(4,21), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.total */
{ (void *)0,
    { PMDA_PMID(4,31), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.zero */
{ (void *)0,
    { PMDA_PMID(4,32), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le3k */
{ (void *)0,
    { PMDA_PMID(4,33), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le10k */
{ (void *)0,
    { PMDA_PMID(4,34), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le30k */
{ (void *)0,
    { PMDA_PMID(4,35), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le100k */
{ (void *)0,
    { PMDA_PMID(4,36), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le300k */
{ (void *)0,
    { PMDA_PMID(4,37), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le1m */
{ (void *)0,
    { PMDA_PMID(4,38), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.le3m */
{ (void *)0,
    { PMDA_PMID(4,39), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(4,40), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.requests.uncached.size.unknown */
{ (void *)0,
    { PMDA_PMID(4,41), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(0, 0, 1, 0, 0, PM_COUNT_ONE) } },

/* perserver.bytes.size.zero */
{ (void *)0,
    { PMDA_PMID(2,57), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le3k */
{ (void *)0,
    { PMDA_PMID(2,58), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le10k */
{ (void *)0,
    { PMDA_PMID(2,59), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le30k */
{ (void *)0,
    { PMDA_PMID(2,60), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le100k */
{ (void *)0,
    { PMDA_PMID(2,61), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le300k */
{ (void *)0,
    { PMDA_PMID(2,62), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le1m */
{ (void *)0,
    { PMDA_PMID(2,63), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.le3m */
{ (void *)0,
    { PMDA_PMID(2,64), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
    	PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.size.gt3m */
{ (void *)0,
    { PMDA_PMID(2,65), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.total */
{ (void *)0,
    { PMDA_PMID(4,51), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.zero */
{ (void *)0,
    { PMDA_PMID(4,52), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le3k */
{ (void *)0,
    { PMDA_PMID(4,53), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le10k */
{ (void *)0,
    { PMDA_PMID(4,54), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le30k */
{ (void *)0,
    { PMDA_PMID(4,55), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le100k */
{ (void *)0,
    { PMDA_PMID(4,56), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le300k */
{ (void *)0,
    { PMDA_PMID(4,57), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le1m */
{ (void *)0,
    { PMDA_PMID(4,58), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.le3m */
{ (void *)0,
    { PMDA_PMID(4,59), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.cached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(4,60), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.total */
{ (void *)0,
    { PMDA_PMID(4,71), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.zero */
{ (void *)0,
    { PMDA_PMID(4,72), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le3k */
{ (void *)0,
    { PMDA_PMID(4,73), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le10k */
{ (void *)0,
    { PMDA_PMID(4,74), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le30k */
{ (void *)0,
    { PMDA_PMID(4,75), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le100k */
{ (void *)0,
    { PMDA_PMID(4,76), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le300k */
{ (void *)0,
    { PMDA_PMID(4,77), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le1m */
{ (void *)0,
    { PMDA_PMID(4,78), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.le3m */
{ (void *)0,
    { PMDA_PMID(4,79), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },

/* perserver.bytes.uncached.size.gt3m */
{ (void *)0,
    { PMDA_PMID(4,80), PM_TYPE_U64, WEBLOG_INDOM, PM_SEM_COUNTER, 
        PMDA_PMUNITS(1, 0, 0, PM_SPACE_BYTE, 0, 0) } },
/*
 * Added in PCPWEB 1.1.1
 */

/* perserver.logidletime */
{ (void *)0,
    { PMDA_PMID(2,68), PM_TYPE_U32, WEBLOG_INDOM, PM_SEM_DISCRETE, 
    	PMDA_PMUNITS(0, 1, 0, 0, PM_TIME_SEC, 0) } },

};

/* number of metrics */
static int      	numMetrics = (sizeof(wl_metrics)/sizeof(wl_metrics[0]));

/* number of instance domains */
static int		numIndoms = sizeof(wl_indomTable)/sizeof(wl_indomTable[0]);

/* mask to get the cluster from a PMID */
static int		_clusterMask = (1<<22) - (1<<10);

/* refresh all logs if wl_updateAll != 0 */
int			wl_updateAll = 0;

/* time in milliseconds to update all the logs */
__uint32_t		wl_catchupTime = 0;

/* time log refresh was started */
time_t			wl_timeOfRefresh = 0;

/* flag to indicate DSO or Daemon */
int		wl_isDSO = 0;

/* request size categories */
long wl_sizes[] = {
    0, 3*1024, 10*1024, 30*1024, 100*1024, 300*1024, 1024*1024, 3*1024*1024, 0
};

#define BUFFER_LEN	2048

static pmdaExt		*extp;		/* set in web_init() */

#ifdef HAVE_SIGHUP
/*
 * Signal handler for an sproc receiving TERM (probably from parent)
 */
static void
onhup(int s)
{
    _exit(s != SIGHUP);
}
#endif

/*
 * Replacement for fgets using the FileInfo structure
 */

int
wl_gets(FileInfo *fip, char **line)
{
    char	*p;
    int		nch;
    int		sts;

    if (fip->filePtr < 0) {
	return -1;
    }
    
    p = fip->bp;

more:
    while (p < fip->bend) {
	if (*p == '\n') {
	    /* newline, we are done */
	    *p++ = '\0';
	    *line = fip->bp;
	    fip->bp = p;
	    return p - *line;
	}
	p++;
    }

    /* out the end of the buffer, and no newline */
    nch = fip->bend - fip->bp;
    if (nch == FIBUFSIZE) {
	/* buffer full, and no newline! ... truncate and return */
	fip->buf[FIBUFSIZE-1] = '\n';
	p = &fip->buf[FIBUFSIZE-1];
	goto more;
    }
    if (nch)
	/* shuffle partial line to start of buffer */
	memcpy(fip->buf, fip->bp, nch);
    fip->bp = fip->buf;
    fip->bend = &fip->buf[nch];

    /* refill */
    sts = read(fip->filePtr, fip->bend, FIBUFSIZE-nch);
    if (sts <= 0) {
	/* no more, either terminate last line, or really return status */
	if (nch) {
	    *fip->bend = '\n';
	    sts = 1;
	}
	else {
	    return sts;
	}
    }
    p = fip->bend;
    fip->bend = &fip->bend[sts];
    goto more;
}

/*
 * Open a log file and seek to the end
 */

int
openLogFile(FileInfo *theFile)
{
    int		diff = theFile->filePtr;
    char	*line = (char *)0;

    theFile->filePtr = open(theFile->fileName, O_RDONLY);

    if (theFile->filePtr == -1) {
    	if (theFile->filePtr != diff) {
	    logmessage(LOG_ERR, "openLogFile: open %s: %s\n", 
		       theFile->fileName, osstrerror());
	}
	return -1;
    }

    if (fstat(theFile->filePtr, &(theFile->fileStat)) < 0) {
    	logmessage(LOG_ERR, "openLogFile: stat for %s: %s\n", 
		   theFile->fileName, osstrerror());
    	wl_close(theFile->filePtr);
	return -1;
    }

    logmessage(LOG_INFO, "%s opened (fd=%d, inode=%d)\n",
	       theFile->fileName,
	       theFile->filePtr, 
	       theFile->fileStat.st_ino);

    /* throw away last line in file */
    if (theFile->fileStat.st_size != 0) {
    	lseek(theFile->filePtr, -2L, SEEK_END);
	wl_gets(theFile, &line);
    }

    if (fstat(theFile->filePtr, &(theFile->fileStat)) < 0) {
    	logmessage(LOG_ERR, "openLogFile: update stat for %s: %s\n", 
		   theFile->fileName, osstrerror());
	wl_close(theFile->filePtr);
	return -1;	
    }

/* 
* Check and warn if a log file has not been modified in the last 24 hours,
* as this may indicate something is wrong with the PMDA's configuration,
* or the Web server's configuration
*/

    diff = time((time_t*)0) - theFile->fileStat.st_mtime;
    if (diff > DORMANT_WARN) {
    	logmessage(LOG_WARNING, 
		   "log file %s has not been modified for at least %d days",
		   theFile->fileName,
		   diff / DORMANT_WARN);
    }

    return 0;
}

/* 
 * Check the log file is still the correct log file.
 *
 * If the file has not been modified in wl_chkDelay seconds, then reopen
 * the file and compare the inodes.
 * Otherwise the current inode and size of the file are checked.
 *
 * Returns a LogFileCode indicating the status of the log file.
 */

static int
checkLogFile(FileInfo *theFile,
	     struct stat *tmpStat)
{
    int		tmpFd = -1;
    int 	result = wl_ok;

/*
 *  File is closed, if enough time has elasped since last attempt, try to
 *  open it
 */

    if (theFile->filePtr < 0)
    {
    	if (wl_timeOfRefresh - theFile->lastActive > wl_chkDelay)
	{
	    theFile->lastActive = wl_timeOfRefresh;
	    if (openLogFile(theFile) < 0)
	    	result = wl_unableToOpen;
	    else
	    	result =  wl_opened;
	}
	else
	    result = wl_closed;
    }

/*  Get the file stat info on the open file */

    if (theFile->filePtr >= 0) {
    	if (fstat(theFile->filePtr, tmpStat) < 0) {
	    logmessage(LOG_ERR, "checkLogFile: stat on open %s: %s\n",
		       theFile->fileName, osstrerror());
	    wl_close(theFile->filePtr);
	    result = wl_unableToStat;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		logmessage(LOG_DEBUG,
			   "checkLogFile: could not stat %s\n",
			   theFile->fileName);
	    }
#endif

	}
    }

/*
 *  Check that we are dealing with a regular file. If is a character
 *  device or directory etc just ignore it.
 */

    if (result == wl_ok && !(theFile->fileStat.st_mode & S_IFREG)) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    logmessage(LOG_DEBUG,
		       "%s is not a regular file. Skipping...\n",
		       theFile->fileName);
#endif

    	result = wl_irregularFile;
    }

/*
 *  Check that the size hasn't gotten any smaller e.g. from ftruncate(2)
 */

    if (result == wl_ok && tmpStat->st_size < theFile->fileStat.st_size) {

	logmessage(LOG_WARNING, 
		   "%s stat - inode %d, size %d -> %d\n",
		   theFile->fileName,
		   theFile->fileStat.st_ino,
		   theFile->fileStat.st_size,
		   tmpStat->st_size);

    	result = wl_reopened;
    }

/*
 *  File was already open, check to see if it hasn't been modified, and
 *  that the last time we checked it was > wl_chkDelay ago.
 */

    if (result == wl_ok && 
    	tmpStat->st_mtime == theFile->fileStat.st_mtime &&
	wl_timeOfRefresh - theFile->lastActive > wl_chkDelay) {

	tmpFd = open(theFile->fileName, O_RDONLY);

	if (tmpFd < 0) {
	    logmessage(LOG_ERR, 
		       "checkLogFile: 2nd open to %s: %s\n",
		       theFile->fileName,
		       osstrerror());
	    wl_close(theFile->filePtr);

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		logmessage(LOG_DEBUG,
			   "checkLogFile: could not check %s\n",
			   theFile->fileName);
	    }
#endif

	}
	else if (fstat(tmpFd, tmpStat) < 0) {
	    logmessage(LOG_ERR,
		       "checkLogFile: stat on inactive %s: %s\n",
		       theFile->fileName,
		       osstrerror());
	    wl_close(theFile->filePtr);
	    result = wl_unableToStat;

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL0) {
		logmessage(LOG_DEBUG,
			   "checkLogFile: could not stat inactive %s\n",
			   theFile->fileName);
	    }
#endif

	}
	else if (tmpStat->st_ino != theFile->fileStat.st_ino) {

	    logmessage(LOG_WARNING, 
		       "%s inactive - inode %d -> %d\n",
		       theFile->fileName,
		       theFile->fileStat.st_ino,
		       tmpStat->st_ino);

	    result = wl_reopened;
	}
	else 
	    theFile->lastActive = wl_timeOfRefresh;


	if (tmpFd >= 0)
	    close(tmpFd);
    }


/*
 *  File needs to be reopened due to change in inode, smaller size, or lack
 *  of activity
 */

    if (result == wl_reopened) {

	theFile->lastActive = wl_timeOfRefresh;

	wl_close(theFile->filePtr);

	if (openLogFile(theFile) < 0) {

	    logmessage(LOG_WARNING,
		       "checkLogFile: unable to reopen %s\n",
		       theFile->fileName);
	    result = wl_unableToOpen;
	}

/*      update the stat information using new file desc */

	else if (fstat(theFile->filePtr, tmpStat) < 0) {
	    logmessage(LOG_ERR,
		       "checkLogFile - stat on reopened %s: %s\n",
		       theFile->fileName,
		       osstrerror());
	    wl_close(theFile->filePtr);
	    result = wl_unableToStat;
	}
    }

/*  if the size has increased in the logs, then change the lastActive time */

    if (result == wl_ok && tmpStat->st_mtime > theFile->fileStat.st_mtime) {

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    logmessage(LOG_DEBUG, "%s grew %d bytes\n", 
		       theFile->fileName,
		       tmpStat->st_size - theFile->fileStat.st_size);
#endif
    	theFile->lastActive = wl_timeOfRefresh;
    }

    return result;
}

/*
 * Main function for sprocs. Contains an infinite loop selecting on the pipe from the
 * main process. Anything on the pipe indicates a refresh is required.
 */

void
sprocMain(void *sprocNum)
{
    int 	mySprocNum = *((int*)sprocNum);
    int		i = 0;
    int		sts = 0;
    WebSproc	*sprocData = &wl_sproc[mySprocNum];
    WebServer	*server = (WebServer*)0;

    /* Pause a sec' so the output log doesn't get mucked up */
    sleep(1);

#ifdef HAVE_SIGHUP
    /* SIGHUP when the parent dies */
    signal(SIGHUP, onhup);
#endif

#ifdef HAVE_PRCTL
#ifdef HAVE_PR_TERMCHILD
    prctl(PR_TERMCHILD);
#elif HAVE_PR_SET_PDEATHSIG
    prctl(PR_SET_PDEATHSIG, SIGHUP);
#endif
#endif

/* close channel to pmcd */
    if (__pmSocketIPC(extp->e_infd))
        __pmCloseSocket(extp->e_infd);
    else if (close(extp->e_infd) < 0) {
    	logmessage(LOG_ERR, "sprocMain: pmcd ch. close(fd=%d) failed: %s\n",
		   extp->e_infd, osstrerror());
    }
    if (__pmSocketIPC(extp->e_outfd))
        __pmCloseSocket(extp->e_outfd);
    else if (close(extp->e_outfd) < 0) {
    	logmessage(LOG_ERR, "sprocMain: pmcd ch. close(fd=%d) failed: %s\n",
		   extp->e_outfd, osstrerror());
    }

/* close pipes to main process which are not to be used */
    
    if(close(sprocData->inFD[1]) < 0) {
    	logmessage(LOG_ERR, "sprocMain[%d]: pipe close(fd=%d) failed: %s\n",
		   mySprocNum, sprocData->inFD[1], osstrerror());
    }
    if(close(sprocData->outFD[0]) < 0) {
    	logmessage(LOG_ERR, "sprocMain[%d]: pipe close(fd=%d) failed: %s\n",
		   mySprocNum, sprocData->outFD[0], osstrerror());
    }
    
/* open up all file descriptors */

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2)
    	logmessage(LOG_DEBUG, "Sproc %d started for servers %d to %d\n",
		   mySprocNum,
		   sprocData->firstServer,
		   sprocData->lastServer);
#endif

    for (i=sprocData->firstServer; i<=sprocData->lastServer; i++)
    {
    	server = &wl_servers[i];
	if (server->counts.active) {
	    openLogFile(&(server->access));
	    openLogFile(&(server->error));
	}
    }

/* wait for message from pmda to probe files */

    for (;;) {
    	sts = read(sprocData->inFD[0], &i, sizeof(i));
	if (sts <= 0) {
	    logmessage(LOG_ERR, "Sproc[%d] read(fd=%d) failed: %s\n",
		       mySprocNum, sprocData->inFD[0], osstrerror());
	    exit(1);
	}
	refresh(sprocData);
	sts = write(sprocData->outFD[1], &i, sizeof(i));
	if (sts <= 0) {
	    logmessage(LOG_ERR, "Sproc[%d] write(fd=%d) failed: %s\n",
		       sprocData->outFD[1], mySprocNum, osstrerror());
	    exit(1);
	}
    }
}

/*
 * Refresh all the server log files that this process monitors.
 * Any entries are parsed, categorised and added to the appropriate metrics.
 */

void
refresh(WebSproc* proc)
{
    struct stat		tmpStat;

    WebServer		*server = (WebServer *)0;
    WebCount		*count = (WebCount *)0;
    FileInfo		*accessFile = (FileInfo *)0;
    FileInfo		*errorFile = (FileInfo *)0;

    char		*line = (char *)0;
    char		*end = (char *)0;
    int			httpMethod = 0;
    long		size = 0;
    int			sizeIndex = 0;
    int			newLength = 0;
    int			i = 0;
    int			sts = 0;
    int			result = wl_ok;
    int			ok = 0;
    time_t		currentTime;
    size_t		nmatch = 5;
    regmatch_t		pmatch[5];


    currentTime = time((time_t*)0);

/*  iterate through each flagged server */

    for (i=proc->firstServer; i<=proc->lastServer; i++) {

	server = &(wl_servers[i]);
	accessFile = &(server->access);
	errorFile = &(server->error);

	if ((server->update || wl_updateAll) && server->counts.active) {

	    server->counts.numLogs = 0;

/*	    check access log still exists */

	    result = checkLogFile(accessFile, &tmpStat);

#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
	    	logmessage(LOG_DEBUG, 
			   "checkLogFile returned %d for server %d (access)\n",
			   result,
			   i);
#endif

/*	    scan access log */

	    if (result == wl_ok || result == wl_reopened || 
	    	result == wl_opened) {

	        server->counts.numLogs++;
		server->counts.modTime = (__uint32_t)(currentTime - 
						      tmpStat.st_mtime);

		while (accessFile->fileStat.st_size < tmpStat.st_size) {

		    sts = wl_gets(accessFile, &line);
		    if (sts <= 0) {

#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    logmessage(LOG_DEBUG, 
				       "Short read of %s by %d bytes\n",
				       accessFile->fileName,
				       tmpStat.st_size - accessFile->fileStat.st_size);
#endif

			if (sts == 0)  {
			    logmessage(LOG_WARNING, 
				       "refresh %s: unexpected eof\n",
				       accessFile->fileName);
			}
			else {
			    logmessage(LOG_ERR, "refresh %s: %s\n",
				       accessFile->fileName, osstrerror());
			}

			wl_close(accessFile->filePtr);
			accessFile->lastActive -= wl_chkDelay;
			break;
		    }

		    accessFile->fileStat.st_size += sts;

		    if (proc->strLength == 0 || proc->strLength <= sts)
			newLength = sts > 255 ? ((sts / 256) + 1) * 256 : 256;
		    else
		    	newLength = proc->strLength;

		    if (newLength > proc->strLength)
		    {
#ifdef PCP_DEBUG
                       if (pmDebug & DBG_TRACE_APPL2) {
                            logmessage(LOG_DEBUG, 
                                   "Resizing strings from %d to %d bytes\n",
                                   proc->strLength,
                                   newLength);
                        }
#endif
                        proc->methodStr = (char*)realloc(proc->methodStr, 
                                     newLength * sizeof(char));
                        proc->sizeStr = (char*)realloc(proc->sizeStr,
                                       newLength * sizeof(char));
                        proc->c_statusStr = (char*)realloc(proc->c_statusStr,
                                           newLength * sizeof(char));
                        proc->s_statusStr = (char*)realloc(proc->s_statusStr,
                                           newLength * sizeof(char));
                        proc->strLength = newLength;
                    }

                    if (proc->methodStr == (char *)0 || 
                        proc->sizeStr == (char *)0 ||
                        proc->c_statusStr == (char *)0 ||
                        proc->s_statusStr == (char *)0 ) {
                        logmessage(LOG_ERR, 
                               "Unable to allocate %d bytes to strings",
                               newLength);
                        proc->strLength = 0;
                        if (proc->methodStr != (char *)0)
                            free(proc->methodStr);
                        if (proc->sizeStr != (char *)0)
                            free(proc->sizeStr);
                        if (proc->c_statusStr != (char *)0)
                            free(proc->c_statusStr);
                        if (proc->s_statusStr != (char *)0)
                            free(proc->s_statusStr);
                        
                        break;
                    }

                    ok = 0;

                    if (wl_regexTable[accessFile->format].posix_regexp) {
                        if (regexec(wl_regexTable[accessFile->format].regex,
                            line, nmatch, pmatch, 0) == 0) {
            
                            if(pmatch[1].rm_so < 0 || pmatch[2].rm_so < 0) {
                                logmessage(LOG_ERR,
                                       "failed to match method and size: %s\n",
                                       line);
                                continue;
                            }
                
                            if(server->counts.extendedp) {
                                if(pmatch[3].rm_so < 0 || pmatch[4].rm_so < 0) {
				    logmessage(LOG_ERR,
					   "failed to match status codes: %s\n",
					   line);
				    continue;
                                }
                            }
                
                            line[pmatch[wl_regexTable[accessFile->format].methodPos].rm_eo] = '\0';
                            strncpy(proc->methodStr, &line[pmatch[wl_regexTable[accessFile->format].methodPos].rm_so],
                                (pmatch[wl_regexTable[accessFile->format].methodPos].rm_eo -
                                 pmatch[wl_regexTable[accessFile->format].methodPos].rm_so) + 1);
                
                            line[pmatch[wl_regexTable[accessFile->format].sizePos].rm_eo] = '\0';
                            strncpy(proc->sizeStr, &line[pmatch[wl_regexTable[accessFile->format].sizePos].rm_so],
                                (pmatch[wl_regexTable[accessFile->format].sizePos].rm_eo -
                                 pmatch[wl_regexTable[accessFile->format].sizePos].rm_so) + 1);
                
                            if(server->counts.extendedp) {
				line[pmatch[wl_regexTable[accessFile->format].c_statusPos].rm_eo] = '\0';
				strncpy(proc->c_statusStr, &line[pmatch[wl_regexTable[accessFile->format].c_statusPos].rm_so],
				    (pmatch[wl_regexTable[accessFile->format].c_statusPos].rm_eo -
				     pmatch[wl_regexTable[accessFile->format].c_statusPos].rm_so) + 1);
		    
				line[pmatch[wl_regexTable[accessFile->format].s_statusPos].rm_eo] = '\0';
				strncpy(proc->s_statusStr, &line[pmatch[wl_regexTable[accessFile->format].s_statusPos].rm_so],
				    (pmatch[wl_regexTable[accessFile->format].s_statusPos].rm_eo -
				     pmatch[wl_regexTable[accessFile->format].s_statusPos].rm_so) + 1);
                            } else {
                                proc->c_statusStr[0] = '\0';
                                proc->s_statusStr[0] = '\0';
                            }
			    ok = 1;
			}
#ifdef PCP_DEBUG
			else if (pmDebug & DBG_TRACE_APPL2)
			    logmessage(LOG_DEBUG, "Regex failed on %s\n", line);
#endif
                    }
#ifdef NON_POSIX_REGEX
                    else if (regex(wl_regexTable[accessFile->format].np_regex,
                              line, proc->methodStr, proc->sizeStr, proc->c_statusStr, proc->s_statusStr) != NULL) {
                        ok = 1;
		    }
#ifdef PCP_DEBUG
		    else if (pmDebug & DBG_TRACE_APPL2)
			logmessage(LOG_DEBUG, "Regex failed on %s\n", line);
#endif
#endif
                    if ( ok ) {

                        for (line = proc->methodStr; *line; line++)
                            *line = toupper((int)*line);
            
                        httpMethod = wl_httpOther;
                        switch(proc->methodStr[0]) {
                        case 'G':
                            if(strcmp(proc->methodStr, "GET") == 0) {
				httpMethod = wl_httpGet;
                            }
                            break;
                        case 'O':
                            if(strcmp(proc->methodStr, "O") == 0) {
				httpMethod = wl_httpGet;
                            }
                            break;
                        case 'H':
                            if(strcmp(proc->methodStr, "HEAD") == 0) {
				httpMethod = wl_httpHead;
                            }
                            break;
                        case 'P':
                            if(strcmp(proc->methodStr, "POST") == 0 ||
                               strcmp(proc->methodStr, "PUT") == 0) {
				httpMethod = wl_httpPost;
                            }
                            break;
                        case 'I':
                            if(strcmp(proc->methodStr, "I") == 0) {
				httpMethod = wl_httpPost;
                            }
                            break;
                        }
            
                        if (strcmp(proc->sizeStr, "-") == 0 ||
                            strcmp(proc->sizeStr, " ") == 0) {
                            size = 0;
                            sizeIndex = wl_unknownSize;
                        } 
			else {
                            size = strtol(proc->sizeStr, &end, 10);
                            if (*end != '\0') {
                                logmessage(LOG_ERR, "Bad size (%s) @ %s", 
                                   proc->sizeStr, 
				   line);
                                continue;
                            }
            
                            for (sizeIndex = 0; 
                             sizeIndex < wl_gt3m && size > wl_sizes[sizeIndex];
                             sizeIndex++);
                        }
                        
                        count = &(server->counts);
                        count->methodReq[httpMethod]++;
                        count->methodBytes[httpMethod] += size;
            
                        count->sizeReq[sizeIndex]++;
                        count->sizeBytes[sizeIndex] += size;
            
                        count->sumReq++;
                        count->sumBytes += size;
            
                        if(server->counts.extendedp == 1) {
                            /* common extended format */
#ifdef PCP_DEBUG
                            if (pmDebug & DBG_TRACE_APPL2) {
                                logmessage(LOG_DEBUG, 
                                       "Access: Server=%d, line=%s [CEF]\n        M: %s S: %s CS: %s, SS: %s",
                                       i,
                                       line,
                                       proc->methodStr,
                                       proc->sizeStr,
                                       proc->c_statusStr,
                                       proc->s_statusStr);
                            }
#endif
            
                            /*
                             * requested page is not in client/browser cache, nor in the 
                             * server's cache so it has been fetched from the remote server
                             */
                            if(strcmp(proc->c_statusStr, "200") == 0 &&
                               strcmp(proc->s_statusStr, "200") == 0) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
                                    logmessage(LOG_DEBUG, 
                                           "Access: Server=%d, REMOTE fetch: of %.0f bytes\n",
                                           i,
                                           atof(proc->sizeStr));
                                }
#endif
                                /*
                                 * now bucket the size
                                 */
                                if (strcmp(proc->sizeStr, "-") == 0 ||
                                    strcmp(proc->sizeStr, " ") == 0) {
                                    size = 0;
                                    sizeIndex = wl_unknownSize;
                                } 
				else {
                                    size = strtol(proc->sizeStr, &end, 10);
                                    if (*end != '\0') {
                                        logmessage(LOG_ERR, "Bad size (%s) @ %s", 
                                               proc->sizeStr,
                                               line);
                                        continue;
                                    }
                                    
                                    for (sizeIndex = 0; 
                                         sizeIndex < wl_gt3m && size > wl_sizes[sizeIndex];
                                         sizeIndex++);
                                }
                                count->uncached_sumReq++;
                                count->uncached_sumBytes += size;
                                count->uncached_sizeReq[sizeIndex]++;
                                count->uncached_sizeBytes[sizeIndex] += size;
                                
                            }
            
                            /*
                             * requested page is not in client/browser cache, but is in the 
                             * server's cache so it is just returned to the client (a cache hit)
                             */
                            if(strcmp(proc->c_statusStr, "200") == 0 &&
                               (strcmp(proc->s_statusStr, "304") == 0 ||
                                strcmp(proc->s_statusStr, "-") == 0)) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
				    logmessage(LOG_DEBUG, 
                                       "Access: Server=%d, CACHE return: of %.0f bytes\n",
                                       i,
                                       atof(proc->sizeStr));
                                }
#endif
                                /*
                                 * now bucket the size
                                 */
                                if (strcmp(proc->sizeStr, "-") == 0 ||
				    strcmp(proc->sizeStr, " ") == 0) {
                                    size = 0;
                                    sizeIndex = wl_unknownSize;
                                } 
				else {
                                    size = strtol(proc->sizeStr, &end, 10);
                                    if (*end != '\0') {
                                        logmessage(LOG_ERR, "Bad size (%s) @ %s", 
                                               proc->sizeStr,
                                               line);
                                        continue;
                                    }
                                    
                                    for (sizeIndex = 0; 
                                         sizeIndex < wl_gt3m && size > wl_sizes[sizeIndex];
                                         sizeIndex++);
                                }
                                count->cached_sumReq++;
                                count->cached_sumBytes += size;
                                count->cached_sizeReq[sizeIndex]++;
                                count->cached_sizeBytes[sizeIndex] += size;
                                
                            }
                            
                            /*
                             * requested page is in client/browser cache
                             */
                            if(strcmp(proc->c_statusStr, "304") == 0 &&
                               (strcmp(proc->s_statusStr, "304") == 0 ||
                                strcmp(proc->s_statusStr, "-") == 0)) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
                                    logmessage(LOG_DEBUG, 
                                           "Access: Server=%d, CLIENT hit\n",
                                           i);
                                }
#endif
                                count->client_sumReq++;
                            }
                        } else if(server->counts.extendedp == 2) {
                            /* default squid format */
#ifdef PCP_DEBUG
                            if (pmDebug & DBG_TRACE_APPL2) {
                                logmessage(LOG_DEBUG, 
                                       "Access: Server=%d, line=%s [squid]\n        M: %s S: %s CS: %s, SS: %s",
                                       i,
                                       line,
                                       proc->methodStr,
                                       proc->sizeStr,
                                       proc->c_statusStr,
                                       proc->s_statusStr);
                            }
#endif
                            
                            /*
                             * requested page is not in client/browser cache, nor in the 
                             * server's cache so it has been fetched from the remote server
                             */
                            if(strcmp(proc->c_statusStr, "200") == 0 &&
                               (strstr(proc->s_statusStr, "_MISS") != NULL ||
                                strstr(proc->s_statusStr, "_CLIENT_REFRESH") != NULL ||
                                strstr(proc->s_statusStr, "_SWAPFAIL") != NULL)) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
				    logmessage(LOG_DEBUG, 
                                       "Access: Server=%d, REMOTE fetch: of %.0f bytes\n",
                                       i,
                                       atof(proc->sizeStr));
                                }
#endif
                                /*
                                 * now bucket the size
                                 */
                                if (strcmp(proc->sizeStr, "-") == 0 ||
                                    strcmp(proc->sizeStr, " ") == 0) {
                                    size = 0;
                                    sizeIndex = wl_unknownSize;
                                } 
				else {
                                    size = strtol(proc->sizeStr, &end, 10);
                                    if (*end != '\0') {
                                        logmessage(LOG_ERR, "Bad size (%s) @ %s", 
                                               proc->sizeStr,
                                               line);
                                        continue;
                                    }
                                    
                                    for (sizeIndex = 0; 
                                         sizeIndex < wl_gt3m && size > wl_sizes[sizeIndex];
                                         sizeIndex++);
                                }
                                count->uncached_sumReq++;
                                count->uncached_sumBytes += size;
                                count->uncached_sizeReq[sizeIndex]++;
                                count->uncached_sizeBytes[sizeIndex] += size;
                                
                            }
                            
                            /*
                             * requested page is not in client/browser cache, but is in the 
                             * server's cache so it is just returned to the client (a cache hit)
                             */
                            if(strcmp(proc->c_statusStr, "200") == 0 &&
                               strstr(proc->s_statusStr, "_HIT") != NULL) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
				    logmessage(LOG_DEBUG, 
                                       "Access: Server=%d, CACHE return: of %.0f bytes\n",
                                       i,
                                       atof(proc->sizeStr));
                                }
#endif
                                /*
                                 * now bucket the size
                                 */
                                if (strcmp(proc->sizeStr, "-") == 0 ||
				    strcmp(proc->sizeStr, " ") == 0) {
                                    size = 0;
                                    sizeIndex = wl_unknownSize;
                                } 
				else {
                                    size = strtol(proc->sizeStr, &end, 10);
                                    if (*end != '\0') {
                                        logmessage(LOG_ERR, "Bad size (%s) @ %s", 
                                               proc->sizeStr,
                                               line);
                                        continue;
                                    }
                                    
                                    for (sizeIndex = 0; 
                                         sizeIndex < wl_gt3m && size > wl_sizes[sizeIndex];
                                         sizeIndex++);
                                }
                                count->cached_sumReq++;
                                count->cached_sumBytes += size;
                                count->cached_sizeReq[sizeIndex]++;
                                count->cached_sizeBytes[sizeIndex] += size;
                            }
                            
                            /*
                             * requested page is in client/browser cache
                             */
                            if(strcmp(proc->c_statusStr, "304") == 0) {
#ifdef PCP_DEBUG
                                if (pmDebug & DBG_TRACE_APPL2) {
                                    logmessage(LOG_DEBUG, 
                                           "Access: Server=%d, CLIENT hit\n",
                                           i);
                                }
#endif
                                count->client_sumReq++;
                            }
                        }
                        
#ifdef PCP_DEBUG
                        if (pmDebug & DBG_TRACE_APPL2) {
                            logmessage(LOG_DEBUG, 
                                   "Access: Server=%d, line=%s\n        method=%s [%d], size=%s=%d [%d]\n",
                                   i,
                                   line,
                                   proc->methodStr,
                                   httpMethod, 
                                   proc->sizeStr,
                                   size,
                                   sizeIndex);
                        }
#endif
            
		    }
                }
                accessFile->fileStat = tmpStat;
            }

            result = checkLogFile(errorFile, &tmpStat);

#ifdef PCP_DEBUG
            if (pmDebug & DBG_TRACE_APPL2)
                logmessage(LOG_DEBUG, 
                   "checkLogFile returned %d for server %d (error)\n",
                   result,
                   i);
#endif

    /*        scan error log */

            if (result == wl_ok || result == wl_reopened || 
                result == wl_opened) {

                server->counts.numLogs++;

                while (errorFile->fileStat.st_size < tmpStat.st_size) {
                    sts = wl_gets(errorFile, &line);
                    if (sts <= 0) {
#ifdef PCP_DEBUG
			if (pmDebug & DBG_TRACE_APPL0)
			    logmessage(LOG_DEBUG, "%s was %d bytes short\n",
				   errorFile->fileName,
				   tmpStat.st_size - 
				   errorFile->fileStat.st_size);
#endif

			if (sts < 0) {
			    logmessage(LOG_ERR, "refresh %s: %s\n",
				   errorFile->fileName, osstrerror());
			}
			else {
			    logmessage(LOG_WARNING, 
				   "refresh %s: unexpected eof\n",
				   errorFile->fileName);
			}

			wl_close(errorFile->filePtr);
			errorFile->lastActive -= wl_chkDelay;
			break;
                    }

                    errorFile->fileStat.st_size += sts;

                    if(wl_regexTable[errorFile->format].posix_regexp) {
			if (regexec(wl_regexTable[errorFile->format].regex,
			      line, nmatch, pmatch, 0) == 0) {
			    server->counts.errors++;
			}
#ifdef NON_POSIX_REGEX
                    } else {
			if (regex(wl_regexTable[errorFile->format].np_regex,
			      line, proc->methodStr, proc->sizeStr) != NULL) {
			    server->counts.errors++;
			}
#endif
                    }
                }
                errorFile->fileStat = tmpStat;
            }
        }

    /*      check to see if a server is inactive but has a file open. It may
            have just been deactivated */

        else if ((server->update || wl_updateAll) && !server->counts.active) {

            if (accessFile->filePtr >= 0) {

                logmessage(LOG_WARNING, 
                   "Closing inactive server %d access file: %s\n",
                   i,
                   accessFile->fileName);

                wl_close(accessFile->filePtr);
            }

            if (errorFile->filePtr >= 0) {

                logmessage(LOG_WARNING,
                   "Closing inactive server %d error file: %s\n",
                   i,
                   errorFile->fileName);

                wl_close(errorFile->filePtr);
            }
        }
    }
}

/*
 * Initialise the indom and meta tables
 * Check that we can do direct mapping.
 */

/* 
 * Mark servers that are required in the latest profile.
 */
static int
web_profile(__pmProfile *prof, pmdaExt *ext)
{
    pmdaIndom	*idp = wl_indomTable;
    int		j;

    ext->e_prof = prof;	
    for (j = 0; j < idp->it_numinst; j++) {
	if (__pmInProfile(idp->it_indom, prof, idp->it_set[j].i_inst))
	    wl_servers[j].update = 1;
	else
	    wl_servers[j].update = 0;
    }

    return 0;
}

/*
 * Probe servers for log file changes.
 * Only those servers that are marked will be requsted. Therefore, if an sproc does
 * not have any marked servers, it will not be signalled.
 * NOTE: The main process completes its refresh before signalling the other sprocs.
 */

void
probe(void)
{
    int			i = 0;
    int			j = 0;
    int			sts = 0;
    int			dummy = 1;
    int			sprocsUsed = 0;
    int			nfds = 0;
    fd_set		rfds;
    fd_set		tmprfds;
    int			thisFD;
    WebSproc		*sprocData = (WebSproc*)0;
    struct timeval	theTime;

    __pmtimevalNow(&theTime);

    wl_timeOfRefresh = theTime.tv_sec;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
    	logmessage(LOG_DEBUG, "Starting probe at %d\n", wl_timeOfRefresh);
#endif

    FD_ZERO(&rfds);

/*
 * Determine which sprocs have servers that must be refreshed.
 * Add those sprocs pipes to the file descriptor list.
 */

    for (i=1; i<=wl_numSprocs; i++) {
        sprocData = &wl_sproc[i];

	if (!wl_updateAll) {
	    for (j=sprocData->firstServer; j<=sprocData->lastServer; j++)
		if (wl_servers[j].update)
		    break;
 	}
	else {
	    for (j=sprocData->firstServer; j<=sprocData->lastServer; j++)
		if (wl_servers[j].counts.active)
		    break;
	}

	if (j > sprocData->lastServer) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
		logmessage(LOG_DEBUG, "Skipping sproc %d\n", i);
#endif
	    continue;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    logmessage(LOG_DEBUG, 
		       "Told sproc %d to probe for at least server %d\n", 
		       i,
		       j);
#endif

	sprocsUsed++;
	thisFD = sprocData->outFD[0];
    	sts = write(sprocData->inFD[1], &dummy, sizeof(dummy));
	if (sts < 0) {
	    logmessage(LOG_ERR, "Error on fetch write(fd=%d): %s", 
		       sprocData->inFD[1], osstrerror());
	    exit(1);
	}

	FD_SET(thisFD, &rfds);
	nfds = nfds < (thisFD + 1) ? thisFD + 1 : nfds;
    }

/* 
 * Check that we have to update the main process servers
 */

    sprocData = &wl_sproc[0];
    if (!wl_updateAll) {
	for (j=sprocData->firstServer; j<=sprocData->lastServer; j++)
	    if (wl_servers[j].update)
		break;
    }
    else {
	for (j=sprocData->firstServer; j<=sprocData->lastServer; j++)
	    if (wl_servers[j].counts.active)
		break;
    }

    if (j <= sprocData->lastServer) {
	refresh(&wl_sproc[0]);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    logmessage(LOG_DEBUG, "Done probe for 0 to %d\n", 
	    	       sprocData->lastServer);
#endif
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL2)
	logmessage(LOG_DEBUG, "Skipping refresh of main process\n");
#endif

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL2) {
    	logmessage(LOG_DEBUG, "Waiting for reply from %d out of %d sprocs\n", 
		   sprocsUsed,
		   wl_numSprocs);
    }
#endif

/*
 * Wait for all sprocs to reply
 * Note: This could get into a hard select loop if an sproc losses it
 */

    for (i=0; i<sprocsUsed;) {
        memcpy(&tmprfds, &rfds, sizeof(tmprfds));
	sts = select(nfds, &tmprfds, (fd_set*)0, (fd_set*)0, 
		     (struct timeval*)0);
	if (sts < 0) {
	    logmessage(LOG_ERR, "Error on fetch select: %s", netstrerror());
	    exit(1);
	}
	else if (sts == 0)
	    continue;

	i += sts;
	for (j=1; j<=wl_numSprocs; j++) {
	    sprocData = &wl_sproc[j];
	    thisFD = sprocData->outFD[0];

	    if (FD_ISSET(thisFD, &tmprfds)) {
	    	FD_CLR(sprocData->outFD[0], &rfds);
		sts = read(thisFD, &dummy, sizeof(dummy));
		if (sts < 0) {
		    logmessage(LOG_ERR, "Error on fetch read: %s",
			       osstrerror());
		    exit(1);
		}
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
    	logmessage(LOG_DEBUG, "Finished probe\n");
#endif
}

/*
 * Refresh all servers
 * Usually called if no fetches have been received after a set time
 */

void
refreshAll(void)
{
    struct timeval before;
    struct timeval after;

    wl_updateAll = 1;

#ifdef PCP_DEBUG 
    if (pmDebug & DBG_TRACE_APPL1) {
    	logmessage(LOG_DEBUG, "Starting a refreshAll\n");
    }
#endif

    __pmtimevalNow(&before);
    probe();

    __pmtimevalNow(&after);
    wl_catchupTime = (after.tv_sec - before.tv_sec) * 1000;
    wl_catchupTime += (after.tv_usec - before.tv_usec) / 1000;

    wl_updateAll = 0;

#ifdef PCP_DEBUG 
    if (pmDebug & DBG_TRACE_APPL1)
    	logmessage(LOG_DEBUG, "Probed all logs, took %d msec\n",
		   wl_catchupTime);
#endif

}

/*
 * Build a pmResult table of the requested metrics
 */

static int
web_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *ext)
{
    int			i;		/* over pmidlist[] */
    int			j;		/* over vset->vlist[] */
    int			s;		/* over server */
    int			sts;
    int			need;
    int			inst;
    int			numval;
    static pmResult	*res = (pmResult *)0;
    static int		maxnpmids = 0;
    pmValueSet		*vset = (pmValueSet *)0;
    pmDesc		*dp = (pmDesc *)0;
    __pmID_int		*pmidp;
    pmAtomValue		atom;
    int			haveValue = 0;
    int			type;
    __psint_t		m_offset = 0;	/* initialize to pander to gcc */
    int			m_type = 0;	/* initialize to pander to gcc */
    int			cluster;
    __uint32_t		tmp32;
    __uint64_t		tmp64;

/*  determine if the total aggregates are required, which forces a refresh
    of all servers, and if a probe is require at all */

    j = 0;
    for (i = 0; i < numpmid; i++) {
    	pmidp = (__pmID_int *)&pmidlist[i];
	if (pmidp->cluster == 1)
	    break;
	else
	    j += pmidp->cluster;
    }

    if (i < numpmid) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    logmessage(LOG_DEBUG, "web_fetch: refreshAll\n");
#endif
    	refreshAll();
    }
    else if (j) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL1)
	    logmessage(LOG_DEBUG, "web_fetch: probe\n");
#endif
	probe();
    }
#ifdef PCP_DEBUG
    else if (pmDebug & DBG_TRACE_APPL1)
	logmessage(LOG_DEBUG, "web_fetch: no probes required\n");
#endif


    if (numpmid > maxnpmids) {
	if (res != (pmResult *)0)
	    free(res);

/* (numpmid - 1) because there's room for one valueSet in a pmResult */

	need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
	if ((res = (pmResult *) malloc(need)) == (pmResult *)0)
	    return -oserror();
	maxnpmids = numpmid;
    }

    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

/*
 * Get each corresponding metric from the meta table.
 * Check that the metric has the correct cluster
 */

    for (i = 0; i < numpmid; i++) {

    	pmidp = (__pmID_int*)&pmidlist[i];
	dp = (pmDesc *)0;

	if (ext->e_direct) {

 	    if (pmidp->item < numMetrics && 
		pmidlist[i] == wl_metrics[pmidp->item].m_desc.pmid) {

		dp = &wl_metrics[pmidp->item].m_desc;
		m_offset = wl_metricInfo[pmidp->item].m_offset;
		m_type = wl_metricInfo[pmidp->item].m_type;
	    }
	}
	else {
	    for (j = 0; j<numMetrics; j++) {
	    	if (wl_metrics[j].m_desc.pmid == pmidlist[i]) {

		    dp = &wl_metrics[j].m_desc;
		    m_offset = wl_metricInfo[j].m_offset;
		    m_type = wl_metricInfo[j].m_type;
		    break;
		}
	    }
	}

/*
 * count the number of instances in profile
 */

	if (dp == (pmDesc *)0)
	    numval = PM_ERR_PMID;
	else {
	    if (dp->indom != PM_INDOM_NULL) {
		numval = 0;
		__pmdaStartInst(dp->indom, ext);
		while(__pmdaNextInst(&inst, ext)) {
		    numval++;
		}
	    }
	    else {
                numval = 1;
            }
        }


        /* Must use individual malloc()s because of pmFreeResult() */

        if (numval >= 1)
            res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) + 
                            (numval - 1)*sizeof(pmValue));
        else
            res->vset[i] = vset = (pmValueSet*)malloc(sizeof(pmValueSet) -
                         sizeof(pmValue));

        if (vset == (pmValueSet *)0) {
            if (i) {
                res->numpmid = i;
                __pmFreeResultValues(res);
            }
            return -oserror();
        }

        vset->pmid = pmidlist[i];
        vset->numval = numval;
        vset->valfmt = PM_VAL_INSITU;
        if (vset->numval <= 0)
            continue;
        
        if (dp->indom == PM_INDOM_NULL)
            inst = PM_IN_NULL;
        else {
            __pmdaStartInst(dp->indom, ext);
            __pmdaNextInst(&inst, ext);
        }

        type = dp->type;
        pmidp = (__pmID_int *)&pmidlist[i];
        j = 0;

        do {
            if (j == numval) {

		/* more instances than expected! */

		numval++;
		res->vset[i] = vset = (pmValueSet *)realloc(vset,
			sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (vset == (pmValueSet *)0) {
		    if (i) {
			res->numpmid = i;
			__pmFreeResultValues(res);
		    }
		    return -oserror();
		}
            }

            vset->vlist[j].inst = inst;

            cluster = (dp->pmid & _clusterMask) >> 10;
            haveValue = 1;

            switch(m_type) {
            case wl_globalPtr:
                atom.ul = *(__uint32_t *)(m_offset);
                break;

            case wl_offset32:
                if (wl_servers[inst].counts.active)
                    atom.ul = *((__uint32_t *)(((__psint_t)(&(wl_servers[inst].counts))) + m_offset));
                else
                    haveValue = 0;
                break;

            case wl_offset64:
                if (wl_servers[inst].counts.active)
                    atom.ull = *((__uint64_t *)(((__psint_t)(&(wl_servers[inst].counts))) + m_offset));
                else
                    haveValue = 0;
                break;

            case wl_totalAggregate:

                if (wl_numActive == 0)
                    haveValue = 0;
                else {
		    switch(m_offset) {
                    case 0:
                        /* errors */
                        tmp32 = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active) {
                                tmp32 += wl_servers[s].counts.errors;
                            }
                        }
                        atom.ul = tmp32;
                        break;
                    case 1:
                        /* requests */
                        tmp32 = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active) {
                                tmp32 += wl_servers[s].counts.sumReq;
                            }
                        }
                        atom.ul = tmp32;
                        break;
                    case 2:
                        /* bytes */
                        tmp64 = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active) {
                                tmp64 += wl_servers[s].counts.sumBytes;
                            }
                        }
                        atom.ull = tmp64;
                        break;
                    case 3:
                        /* client hit requests */
                        tmp32 = haveValue = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active &&
                                wl_servers[s].counts.extendedp) {
                                haveValue = 1;
                                tmp32 += wl_servers[s].counts.client_sumReq;
                            }
                        }
                        atom.ul = tmp32;
                        break;
                    case 4:
                        /* cached hit requests */
                        tmp32 = haveValue = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active &&
                                wl_servers[s].counts.extendedp) {
                                haveValue = 1;
                                tmp32 += wl_servers[s].counts.cached_sumReq;
                            }
                        }
                        atom.ul = tmp32;
                        break;
                    case 5:
                        /* cached hit requests */
                        tmp32 = haveValue = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active &&
                                wl_servers[s].counts.extendedp) {
                                haveValue = 1;
                                tmp32 += wl_servers[s].counts.uncached_sumReq;
                            }
                        }
                        atom.ul = tmp32;
                        break;
                    case 6:
                        /* cached hit bytes */
                        tmp64 = haveValue = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active &&
                                wl_servers[s].counts.extendedp) {
                                haveValue = 1;
                                tmp64 += wl_servers[s].counts.cached_sumBytes;
                            }
                        }
                        atom.ull = tmp64;
                        break;
                    case 7:
                        /* uncached bytes */
                        tmp64 = haveValue = 0;
                        for (s = 0; s < wl_numServers; s++) {
                            if (wl_servers[s].counts.active &&
                                wl_servers[s].counts.extendedp) {
                                haveValue = 1;
                                tmp64 += wl_servers[s].counts.uncached_sumBytes;
                            }
                        }
                        atom.ull = tmp64;
                        break;
                    default:
                        break;
                    }

		}
                break;
                
            case wl_serverAggregate:

                if (wl_servers[inst].counts.active == 0)
                    haveValue = 0;
                else switch(m_offset) {
                    case 0:
                        /* requests */
                        atom.ul = wl_servers[inst].counts.sumReq;
                        break;
                    case 1:
                        /* bytes */
                        atom.ull = wl_servers[inst].counts.sumBytes;
                        break;
                    case 2:
                        /* client hit requests */
                        if( wl_servers[inst].counts.extendedp ) {
                            atom.ul = wl_servers[inst].counts.client_sumReq;
                        } else
                            haveValue = 0;
                        break;
                    case 3:
                        /* cache hit requests */
                        if( wl_servers[inst].counts.extendedp ) {
                            atom.ul = wl_servers[inst].counts.cached_sumReq;
                        } else
                            haveValue = 0;
                        break;
                    case 4:
                        /* uncached requests */
                        if( wl_servers[inst].counts.extendedp ) {
                            atom.ul = wl_servers[inst].counts.uncached_sumReq;
                        } else
                            haveValue = 0;
                        break;
                    case 5:
                        /* cached bytes */
                        if( wl_servers[inst].counts.extendedp ) {
                            atom.ull = wl_servers[inst].counts.cached_sumBytes;
                        } else
                            haveValue = 0;
                        break;
                    case 6:
                        /* uncached bytes */
                        if( wl_servers[inst].counts.extendedp ) {
                            atom.ull = wl_servers[inst].counts.uncached_sumBytes;
                        } else
                            haveValue = 0;
                    default:
                        break;
                    }
                break;
                
            case wl_requestMethod:

                if (cluster == 1) {        /* all servers */
                    if (wl_numActive == 0)
                        haveValue = 0;
                    else {
                        tmp32 = 0;
                        for (s = 0; s < wl_numServers; s++)
                            if (wl_servers[s].counts.active)
                                tmp32 += wl_servers[s].counts.methodReq[m_offset];
                            atom.ul = tmp32;
                    }
                } 
		else if (wl_servers[inst].counts.active)
                    atom.ul = wl_servers[inst].counts.methodReq[m_offset];
                else
                    haveValue = 0;

                break;

            case wl_bytesMethod:
                if (cluster == 1) {        /* all servers */
                    if (wl_numActive == 0)
                        haveValue = 0;
                    else {
                        tmp64 = 0;
                        for (s = 0; s < wl_numServers; s++)
                            if (wl_servers[s].counts.active)
                                tmp64 += wl_servers[s].counts.methodBytes[m_offset];
                        atom.ull = tmp64;
                     }
                } 
		else if (wl_servers[inst].counts.active)
                    atom.ull = wl_servers[inst].counts.methodBytes[m_offset];
                else
                    haveValue = 0;
                break;

            case wl_requestSize:
                if (cluster == 1) {        /* all servers */
                    if (wl_numActive == 0)
                        haveValue = 0;
                    else {
                        tmp32 = 0;
                        for (s = 0; s < wl_numServers; s++)
                            if (wl_servers[s].counts.active)
                                tmp32 += wl_servers[s].counts.sizeReq[m_offset];
                        atom.ul = tmp32;
                    }
                } 
		else if (wl_servers[inst].counts.active)
                    atom.ul = wl_servers[inst].counts.sizeReq[m_offset];
                else
                    haveValue = 0;
                break;

            case wl_bytesSize:
                if (cluster == 1) {        /* all servers */
                    if (wl_numActive == 0)
                        haveValue = 0;
                    else {
                        tmp64 = 0;
                        for (s = 0; s < wl_numServers; s++)
                            if (wl_servers[s].counts.active)
                                tmp64 += wl_servers[s].counts.sizeBytes[m_offset];
                        atom.ull = tmp64;
                    }
                } 
		else if (wl_servers[inst].counts.active)
                    atom.ull = wl_servers[inst].counts.sizeBytes[m_offset];
                else
                    haveValue = 0;
                break;

            case wl_requestCachedSize:
                haveValue = 0;
                if (cluster == 3) {        /* all servers */
                    tmp32 = 0;
                    for (s = 0; s < wl_numServers; s++)
                        if (wl_servers[s].counts.active &&
                            wl_servers[s].counts.extendedp) {
                            haveValue = 1;
                        tmp32 += wl_servers[s].counts.cached_sizeReq[m_offset];
                    }
                    atom.ul = tmp32;
                } 
		else if (wl_servers[inst].counts.active &&
		   wl_servers[inst].counts.extendedp) {
		   haveValue = 1;
		   atom.ul = wl_servers[inst].counts.cached_sizeReq[m_offset];
                }
                break;

            case wl_bytesCachedSize:
                haveValue = 0;
                if (cluster == 3) {        /* all servers */
                    tmp64 = 0;
                    for (s = 0; s < wl_numServers; s++) {
                        if (wl_servers[s].counts.active &&
                            wl_servers[s].counts.extendedp) {
                            haveValue = 1;
                            tmp64 += wl_servers[s].counts.cached_sizeBytes[m_offset];
                        }
		    }
                    atom.ull = tmp64;
                } 
		else if (wl_servers[inst].counts.active &&
                           wl_servers[inst].counts.extendedp) {
                    haveValue = 1;
                    atom.ull = wl_servers[inst].counts.cached_sizeBytes[m_offset];
                }
                break;
                
            case wl_requestUncachedSize:
                haveValue = 0;
                if (cluster == 3) {        /* all servers */
                    tmp32 = 0;
                    for (s = 0; s < wl_numServers; s++) {
                        if (wl_servers[s].counts.active &&
			    wl_servers[s].counts.extendedp ) {
			    haveValue = 1;
			    tmp32 += wl_servers[s].counts.uncached_sizeReq[m_offset];
                        }
		    }
                    atom.ul = tmp32;
                } 
		else if (wl_servers[inst].counts.active &&
                           wl_servers[inst].counts.extendedp) {
                    haveValue = 1;
                    atom.ul = wl_servers[inst].counts.uncached_sizeReq[m_offset];
                }
                break;

            case wl_bytesUncachedSize:
                haveValue = 0;
                if (cluster == 3) {        /* all servers */
                    tmp64 = 0;
                    for (s = 0; s < wl_numServers; s++) {
                        if (wl_servers[s].counts.active &&
                            wl_servers[s].counts.extendedp) {
                            haveValue = 1;
                            tmp64 += wl_servers[s].counts.uncached_sizeBytes[m_offset];
                        }
		    }
                    atom.ull = tmp64;
                } 
		else if (wl_servers[inst].counts.active &&
                           wl_servers[inst].counts.extendedp) {
                    haveValue = 1;
                    atom.ull = wl_servers[inst].counts.uncached_sizeBytes[m_offset];
                }
                break;
                
            case wl_watched:
                atom.ul = *((__uint32_t *)(((__psint_t)(&(wl_servers[inst].counts))) + m_offset));
                break;
            case wl_numAlive:
                tmp32 = 0;
                for (s = 0; s < wl_numServers; s++) {
                    if (wl_servers[s].counts.active)
                        tmp32 += wl_servers[s].counts.numLogs > 0 ?1:0;
		}
                atom.ul = tmp32;
                break;

            case wl_nosupport:
                haveValue = 0;
                break;

            default:
                logmessage(LOG_CRIT, 
                       "Illegal Meta Type (%d) for metric %d\n",
                       m_type,
                       pmidp->item);
                exit(1);
            }
            
            if (haveValue) {
                sts = __pmStuffValue(&atom, &vset->vlist[j], type);
                if (sts < 0) {
                    __pmFreeResultValues(res);
                    return sts;
                }

                vset->valfmt = sts;
                j++;    /* next element in vlist[] for next instance */
            }

	} while (dp->indom != PM_INDOM_NULL && __pmdaNextInst(&inst, ext));

	vset->numval = j;
    }
    *resp = res;
    return 0;
}

/*
 * Store into one of three metrics:
 * web.activity.config.catchup, web.activity.config.check and web.activity.server.watched
 */

static int
web_store(pmResult *result, pmdaExt *ext)
{
    int		i;
    int		j;
    pmValueSet	*vsp;
    int		sts = 0;
    __pmID_int	*pmidp;
    WebServer	*server = (WebServer*)0;

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;
	if (pmidp->cluster == 0) {
	    if (pmidp->item == 1) {	/* web.activity.config.catchup */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    val = 20;
		}
		wl_refreshDelay = val;
	    }
	    else if (pmidp->item == 3) {/* web.activity.config.check */
		int	val = vsp->vlist[0].value.lval;
		if (val < 0) {
		    sts = PM_ERR_SIGN;
		    val = 20;
		}
		wl_chkDelay = val;
	    }
	    else if (pmidp->item == 35) {/* web.activity.server.watched */
	        for (j = 0; j < vsp->numval; j++) {
		    int val = vsp->vlist[j].value.lval;
		    if (val < 0) {
			sts = PM_ERR_SIGN;
			val = 1;
		    }

		    server = &wl_servers[vsp->vlist[j].inst];
		    if (val > 0 && server->counts.active == 0) {
			wl_numActive++;
			server->counts.active = 1;
		    }
		    else if (val == 0 && server->counts.active > 0){
			wl_numActive--;
			server->counts.active = 0;
		    }
		}
	    }
	    else {
		sts = PM_ERR_PMID;
		break;
	    }
	}
	else {
	    /* not one of the metrics we are willing to change */
	    sts = PM_ERR_PMID;
	    break;
	}
    }
    return sts;
}

/*
 * Initialise the callback table, and open the help text.
 * This also calls the routine that to initialise the indom and meta tables.
 */

void
web_init(pmdaInterface *dp)
{
    int		m;
    int		type;

    extp = dp->version.two.ext;

    if (wl_isDSO)
    	pmdaDSO(dp, PMDA_INTERFACE_2, "weblog DSO", wl_helpFile);

    if (dp->status != 0)
	return;

    dp->version.two.profile = web_profile;
    dp->version.two.fetch = web_fetch;
    dp->version.two.store = web_store;

    if (numMetrics != (sizeof(wl_metricInfo)/sizeof(wl_metricInfo[0]))) {
	logmessage(LOG_CRIT, 
		   "Metric and Metric Info tables are not the same size\n");
	dp->status = -1;
	return;
    }

    pmdaInit(dp, wl_indomTable, numIndoms, wl_metrics, numMetrics);

    for (m = 0; m < numMetrics; m++) {
	type = wl_metricInfo[m].m_type;
	if (type == wl_offset32 || type == wl_offset64 ||
	    type == wl_watched) {
	    wl_metricInfo[m].m_offset -= (__psint_t)&dummyCount;
	}
    }

    return;
}

