/*
 * Device Mapper PMDA
 *
 * Copyright (c) 2015 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "indom.h"
#include "domain.h"
#include "dmthin.h"
#include "dmcache.h"
#include "dmstats.h"

enum {
    CLUSTER_CACHE = 0,		/* DM-Cache Caches */
    CLUSTER_POOL = 1,		/* DM-Thin Pools */
    CLUSTER_VOL = 2,		/* DM-Thin Volumes */
    CLUSTER_DM_COUNTER = 3,     /* 3 -- Dmstats basic counter */
    CLUSTER_DM_HISTOGRAM = 4,   /* 4 -- Dmstats latency histogram */
    NUM_CLUSTERS
};

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
static pmdaMetric metrictable[] = {
    /* DMCACHE_STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_SIZE),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_BLOCKSIZE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_USED),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_META_TOTAL),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_BLOCKSIZE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_BYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_USED),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_TOTAL),
        PM_TYPE_U64, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_READHITS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_READMISSES),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_WRITEHITS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_WRITEMISSES),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_DEMOTIONS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_PROMOTIONS),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_DIRTY),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_IOMODE_CODE),
        PM_TYPE_U32, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_CACHE, CACHE_IOMODE),
        PM_TYPE_STRING, DM_CACHE_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,0) }, },
    /* DMTHIN_POOL_STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_SIZE),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
       { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_TRANS_ID),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_USED),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_TOTAL),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_USED),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_TOTAL),
        PM_TYPE_U64, DM_THIN_POOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_HELD_ROOT),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DISCARD_PASSDOWN),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_READ_MODE),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_NO_SPACE_MODE),
        PM_TYPE_STRING, DM_THIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* DMTHIN_VOL_STATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_SIZE),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_DISCRETE,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_NUM_MAPPED_SECTORS),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_HIGHEST_MAPPED_SECTORS),
        PM_TYPE_U64, DM_THIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* DM_STATS Basic Counters */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READS_MERGED),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READ_SECTORS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_READ_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITES),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITES_MERGED),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITE_SECTORS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WRITE_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_IO_IN_PROGRESS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_IO_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_WEIGHTED_IO_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_TOTAL_READ_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_COUNTER, PM_DM_STATS_TOTAL_WRITE_NSECS),
        PM_TYPE_U64, DM_STATS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    /* DM STATS latency histogram */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_COUNT),
        PM_TYPE_U64, DM_HISTOGRAM_INDOM, PM_SEM_COUNTER,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /*
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_PERCENT),
        PM_TYPE_FLOAT, DM_HISTOGRAM_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    */
    { .m_desc = {
        PMDA_PMID(CLUSTER_DM_HISTOGRAM, PM_DM_HISTOGRAM_BIN),
        PM_TYPE_U64, DM_HISTOGRAM_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static pmdaIndom indomtable[] = {
    { .it_indom = DM_CACHE_INDOM },
    { .it_indom = DM_THIN_POOL_INDOM },
    { .it_indom = DM_THIN_VOL_INDOM },
    { .it_indom = DM_STATS_INDOM},
    { .it_indom = DM_HISTOGRAM_INDOM},
};

pmInDom
dm_indom(int serial)
{
    return indomtable[serial].it_indom;
}

static int
dm_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    dm_cache_instance_refresh();
    dm_thin_pool_instance_refresh();
    dm_thin_vol_instance_refresh();
    (void)pm_dm_stats_instance_refresh();
    (void)pm_dm_histogram_instance_refresh();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
dm_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom;
    char *name;
    int inst;
    int sts = 0;

    if (need_refresh[CLUSTER_CACHE]) {
        struct cache_stats *cache;

        if ((sts = dm_cache_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_CACHE_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, inst, &name, (void **)&cache) || !cache)
	        continue;
            if (need_refresh[CLUSTER_CACHE])
                dm_refresh_cache(name, cache);
        }
    }

    if (need_refresh[CLUSTER_POOL]) {
        struct pool_stats *pool;

        if ((sts = dm_thin_pool_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_THIN_POOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, inst, &name, (void **)&pool) || !pool)
	        continue;
            if (need_refresh[CLUSTER_POOL])
                dm_refresh_thin_pool(name, pool);
        }
    }

    if (need_refresh[CLUSTER_VOL]) {
        struct vol_stats *vol;

        if ((sts = dm_thin_vol_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_THIN_VOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, inst, &name, (void **)&vol) || !vol)
	        continue;
            if (need_refresh[CLUSTER_VOL])
                dm_refresh_thin_vol(name, vol);
        }
    }

    if (need_refresh[CLUSTER_DM_COUNTER]) {
        struct pm_wrap *pw;

        if ((sts = pm_dm_stats_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_STATS_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, inst, &name, (void **)&pw) || !pw)
	        continue;
            if (need_refresh[CLUSTER_DM_COUNTER])
                pm_dm_refresh_stats(pw, DM_STATS_INDOM);
        }
    }

    if (need_refresh[CLUSTER_DM_HISTOGRAM]) {
        struct pm_wrap *pw;

        if ((sts = pm_dm_histogram_instance_refresh()) < 0)
	    return sts;

        indom = dm_indom(DM_HISTOGRAM_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, inst, &name, (void **)&pw) || !pw)
	        continue;
            if (need_refresh[CLUSTER_DM_HISTOGRAM])
                pm_dm_refresh_stats(pw, DM_HISTOGRAM_INDOM);
        }
    }

    return sts;
}

static int
dm_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }

    if ((sts = dm_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
dm_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    struct cache_stats *cache;
    struct pool_stats *pool;
    struct vol_stats *vol;
    struct pm_wrap *pw;
    int sts;

    switch (idp->cluster) {
        case CLUSTER_CACHE:
            sts = pmdaCacheLookup(dm_indom(DM_CACHE_INDOM), inst, NULL, (void **)&cache);
            if (sts < 0)
                return sts;
            return dm_cache_fetch(idp->item, cache, atom);

        case CLUSTER_POOL:
	    sts = pmdaCacheLookup(dm_indom(DM_THIN_POOL_INDOM), inst, NULL, (void **)&pool);
	    if (sts < 0)
	        return sts;
	    return dm_thin_pool_fetch(idp->item, pool, atom);

        case CLUSTER_VOL:
	    sts = pmdaCacheLookup(dm_indom(DM_THIN_VOL_INDOM), inst, NULL, (void **)&vol);
	    if (sts < 0)
	        return sts;
	    return dm_thin_vol_fetch(idp->item, vol, atom);

	case CLUSTER_DM_COUNTER:
	    sts = pmdaCacheLookup(dm_indom(DM_STATS_INDOM), inst, NULL, (void**)&pw);
	    if (sts < 0)
	        return sts;
	    return pm_dm_stats_fetch(idp->item, pw, atom);

	case CLUSTER_DM_HISTOGRAM:
	    sts = pmdaCacheLookup(dm_indom(DM_HISTOGRAM_INDOM), inst, NULL, (void**)&pw);
	    if (sts < 0)
	        return sts;
	    return pm_dm_histogram_fetch(idp->item, pw, atom);

        default: /* unknown cluster */
	    return PM_ERR_PMID;
    }
    return 1;
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void
__PMDA_INIT_CALL
dm_init(pmdaInterface *dp)
{
    /* Check for environment variables allowing test injection */
    dm_cache_setup();
    dm_thin_setup();

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (dp->status != 0)
	return;

    dp->version.four.instance = dm_instance;
    dp->version.four.fetch = dm_fetch;
    pmdaSetFetchCallBack(dp, dm_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int	sep = __pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "dm" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, DM, "dm.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    dm_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
