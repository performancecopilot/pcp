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
#include "domain.h"

#include "pmdadm.h"

static int _isQA = 0;
static char *dm_statspath = "";

pmdaIndom indomtable[] = {
    { .it_indom = DM_CACHE_INDOM }, 
    { .it_indom = DM_THIN_POOL_INDOM },
    { .it_indom = DM_THIN_VOL_INDOM },
};

#define INDOM(x) (indomtable[x].it_indom)

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
pmdaMetric metrictable[] = { 
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
        PMDA_PMID(CLUSTER_CACHE, CACHE_IOMODE),
        PM_TYPE_STRING, DM_CACHE_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
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
};

int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
}

/*
 * Update the device mapper cache instance domain. This will change
 * as volumes are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
dm_cache_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX] = "dmsetup status --target cache";
    FILE *fp;
    pmInDom indom = INDOM(DM_CACHE_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     * _isQA is set if statspath has been set during pmda init
     */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer),"/bin/cat %s/dmcache-caches", dm_statspath);
        buffer[sizeof(buffer)-1] = '\0';
    }

    if ((fp = popen(buffer, "r")) == NULL )
        return - oserror();

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;

        strtok(buffer, ":");

        /* at this point buffer contains our thin volume names
           this will be used to map stats to file-system instances */

        struct dm_cache *cache;

	sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&cache);
	if (sts == PM_ERR_INST || (sts >= 0 && cache == NULL)){
	    cache = calloc(1, sizeof(struct dm_cache));
            if (cache == NULL) {
                if (pclose(fp) != 0)
                    return -oserror();
                return PM_ERR_AGAIN;
            }
            strcpy(cache->name, buffer);
        }   
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)cache);
    }

    if (pclose(fp) != 0)
        return -oserror(); 

    return 0;
}

/*
 * Update the thin provisioning pool instance domain. This will change
 * as volumes are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
dm_thin_pool_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX] = "dmsetup status --target thin-pool";
    FILE *fp;
    pmInDom indom = INDOM(DM_THIN_POOL_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     * _isQA is set if statspath has been set during pmda init
     */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer),"/bin/cat %s/dmthin-pool", dm_statspath);
        buffer[sizeof(buffer)-1] = '\0';
    }

    if ((fp = popen(buffer, "r")) == NULL )
        return - oserror();

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;

        strtok(buffer, ":");

        /* at this point buffer contains our thin pool lvm names
           this will be used to map stats to file-system instances */

        struct dm_thin_pool *pool;

	sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&pool);
	if (sts == PM_ERR_INST || (sts >= 0 && pool == NULL)){
	    pool = calloc(1, sizeof(struct dm_thin_pool));
            if (pool == NULL) {
                if (pclose(fp) != 0)
                    return -oserror();
                return PM_ERR_AGAIN;
            }
            strcpy(pool->name, buffer);
        }   
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)pool);
    }

    if (pclose(fp) != 0)
        return -oserror(); 

    return 0;
}

/*
 * Update the thin provisioning volume instance domain. This will change
 * as are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
dm_thin_vol_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX] = "dmsetup status --target thin";
    FILE *fp;
    pmInDom indom = INDOM(DM_THIN_VOL_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     * _isQA is set if statspath has been set during pmda init
     */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer),"/bin/cat %s/dmthin-thin", dm_statspath);
        buffer[sizeof(buffer)-1] = '\0';
    }

    if ((fp = popen(buffer, "r")) == NULL )
        return - oserror();

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;

        strtok(buffer, ":");

        /* at this point buffer contains our thin volume names
           this will be used to map stats to file-system instances */

        struct dm_thin_vol *vol;

	sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&vol);
	if (sts == PM_ERR_INST || (sts >= 0 && vol == NULL)){
	    vol = calloc(1, sizeof(struct dm_thin_vol));
            if (vol == NULL) {
                if (pclose(fp) != 0)
                    return -oserror();
                return PM_ERR_AGAIN;
            }
            strcpy(vol->name, buffer);
        }   
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, (void *)vol);
    }

    if (pclose(fp) != 0)
        return -oserror(); 

    return 0;
}

static int
dm_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    dm_cache_instance_refresh();
    dm_thin_pool_instance_refresh();
    dm_thin_vol_instance_refresh();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
dm_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom;
    char *name;
    int i;
    int sts = 0;

    if (need_refresh[CLUSTER_CACHE]) {
        struct dm_cache *cache;

        if ((sts = dm_cache_instance_refresh()) < 0)
	    return sts;

        indom = INDOM(DM_CACHE_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, i, &name, (void **)&cache) || !cache)
	        continue;

            if (need_refresh[CLUSTER_CACHE])
                dm_refresh_cache(_isQA, dm_statspath, name, &cache->cache_stats);
        }
    }

    if (need_refresh[CLUSTER_POOL]) {
        struct dm_thin_pool *pool;

        if ((sts = dm_thin_pool_instance_refresh()) < 0)
	    return sts;

        indom = INDOM(DM_THIN_POOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, i, &name, (void **)&pool) || !pool)
	        continue;

            if (need_refresh[CLUSTER_POOL])
                dm_refresh_thin_pool(_isQA, dm_statspath, name, &pool->pool_stats);
        }
    }

    if (need_refresh[CLUSTER_VOL]) {
        struct dm_thin_vol *vol;

        if ((sts = dm_thin_vol_instance_refresh()) < 0)
	    return sts;

        indom = INDOM(DM_THIN_VOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, i, &name, (void **)&vol) || !vol)
	        continue;

            if (need_refresh[CLUSTER_VOL])
                dm_refresh_thin_vol(_isQA, dm_statspath, name, &vol->vol_stats);
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
    int sts;

    struct dm_cache *cache;
    struct dm_thin_pool *pool;
    struct dm_thin_vol *vol;

    switch (idp->cluster) {
        case CLUSTER_CACHE:
            sts = pmdaCacheLookup(INDOM(DM_CACHE_INDOM), inst, NULL, (void **)&cache);
            if (sts < 0)
                return sts;
            return dm_cache_fetch(idp->item, &cache->cache_stats, atom);

        case CLUSTER_POOL:
	    sts = pmdaCacheLookup(INDOM(DM_THIN_POOL_INDOM), inst, NULL, (void **)&pool);
	    if (sts < 0)
	        return sts;
	    return dm_thin_pool_fetch(idp->item, &pool->pool_stats, atom);

        case CLUSTER_VOL:
	    sts = pmdaCacheLookup(INDOM(DM_THIN_VOL_INDOM), inst, NULL, (void **)&vol);
	    if (sts < 0)
	        return sts;
	    return dm_thin_vol_fetch(idp->item, &vol->vol_stats, atom);

        default: /* unknown cluster */
	    return PM_ERR_PMID;
    }
    return 1;
}

static int
dm_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
dm_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
dm_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
dm_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
dm_init(pmdaInterface *dp)
{
    char *envpath;

    /* Check for environment variable to signify if we are under QA testing */
    if ((envpath = getenv("DM_STATSPATH")) != NULL) {
	dm_statspath = envpath;
        _isQA = 1;
    }

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (dp->status != 0)
	return;

    dp->version.four.instance = dm_instance;
    dp->version.four.fetch = dm_fetch;
    dp->version.four.text = dm_text;
    dp->version.four.pmid = dm_pmid;
    dp->version.four.name = dm_name;
    dp->version.four.children = dm_children;
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
    snprintf(helppath, sizeof(helppath), "%s%c" "dm" "%c" "help",
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
