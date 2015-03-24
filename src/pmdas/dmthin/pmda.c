/*
 * Device Mapper - Thin Provisioning (dm-thin) PMDA
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

#include "pmdadmthin.h"

static int _isQA = 0;
static char *dmthin_statspath = "";

pmdaIndom indomtable[] = { 
    { .it_indom = DMTHIN_POOL_INDOM },
    { .it_indom = DMTHIN_VOL_INDOM },
};

#define INDOM(x) (indomtable[x].it_indom)

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
pmdaMetric metrictable[] = { 
    /* POOL STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_USED),
        PM_TYPE_U64, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_META_TOTAL),
        PM_TYPE_U64, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_USED),
        PM_TYPE_U64, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DATA_TOTAL),
        PM_TYPE_U64, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_HELD_ROOT),
        PM_TYPE_STRING, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_DISCARD_PASSDOWN),
        PM_TYPE_STRING, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_READ_MODE),
        PM_TYPE_STRING, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_POOL, POOL_NO_SPACE_MODE),
        PM_TYPE_STRING, DMTHIN_POOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* VOL STATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_NUM_MAPPED_SECTORS),
        PM_TYPE_U64, DMTHIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_VOL, VOL_HIGHEST_MAPPED_SECTORS),
        PM_TYPE_U64, DMTHIN_VOL_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
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
dmthin_pool_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX];
    FILE *fp;
    pmInDom indom = INDOM(DMTHIN_POOL_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     * _isQA is set if statspath has been set during pmda init
     */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer), "%s/dmthin-pool", dmthin_statspath);
        buffer[sizeof(buffer)-1] = '\0';

        if ((fp = fopen(buffer, "r")) == NULL )
            return -oserror();

    } else {
        if ((fp = popen("dmsetup status --target thin-pool", "r")) == NULL)
            return -oserror();
    }

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;

        strtok(buffer, ":");

        /* at this point buffer contains our thin pool lvm names
           this will be used to map stats to file-system instances */

        struct dmthin_pool *pool;

	sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&pool);
	if (sts == PM_ERR_INST || (sts >= 0 && pool == NULL)){
	    pool = calloc(1, sizeof(struct dmthin_pool));
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

    /* Close process (or file if _isQA) */
    if (_isQA) {
        fclose(fp);
    } else {
        if (pclose(fp) != 0)
            return -oserror(); 
    }
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
dmthin_vol_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX];
    FILE *fp;
    pmInDom indom = INDOM(DMTHIN_VOL_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* 
     * update indom cache based off of thin pools listed by dmsetup
     * _isQA is set if statspath has been set during pmda init
     */
    if (_isQA) {
        snprintf(buffer, sizeof(buffer), "%s/dmthin-thin", dmthin_statspath);
        buffer[sizeof(buffer)-1] = '\0';

        if ((fp = fopen(buffer, "r")) == NULL )
            return -oserror();

    } else {
        if ((fp = popen("dmsetup status --target thin", "r")) == NULL)
            return -oserror();
    }

    while (fgets(buffer, sizeof(buffer) -1, fp)) {
        if (!strstr(buffer, ":"))
            continue;

        strtok(buffer, ":");

        /* at this point buffer contains our thin volume names
           this will be used to map stats to file-system instances */

        struct dmthin_vol *vol;

	sts = pmdaCacheLookupName(indom, buffer, NULL, (void **)&vol);
	if (sts == PM_ERR_INST || (sts >= 0 && vol == NULL)){
	    vol = calloc(1, sizeof(struct dmthin_vol));
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

    /* Close process (or file if _isQA) */
    if (_isQA) {
        fclose(fp);
    } else {
        if (pclose(fp) != 0)
            return -oserror(); 
    }
    return 0;
}

static int
dmthin_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    dmthin_pool_instance_refresh();
    dmthin_vol_instance_refresh();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
dmthin_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom;
    struct dmthin_pool *pool;
    struct dmthin_vol *vol;
    char *name;
    int i;
    int sts = 0;

    if (need_refresh[CLUSTER_POOL]) {

        if ((sts = dmthin_pool_instance_refresh()) < 0)
	    return sts;

        indom = INDOM(DMTHIN_POOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, i, &name, (void **)&pool) || !pool)
	        continue;

            if (need_refresh[CLUSTER_POOL])
                dmthin_refresh_pool(_isQA, dmthin_statspath, name, &pool->pool_stats);
        }
    }

    if (need_refresh[CLUSTER_VOL]) {

        if ((sts = dmthin_vol_instance_refresh()) < 0)
	    return sts;

        indom = INDOM(DMTHIN_VOL_INDOM);

        for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	    if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	        break;
	    if (!pmdaCacheLookup(indom, i, &name, (void **)&vol) || !vol)
	        continue;

            if (need_refresh[CLUSTER_VOL])
                dmthin_refresh_vol(_isQA, dmthin_statspath, name, &vol->vol_stats);
        }
    }
    return sts;
}

static int
dmthin_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }

    if ((sts = dmthin_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
dmthin_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    struct dmthin_pool *pool;
    struct dmthin_vol *vol;
    int sts;

    switch (idp->cluster) {
        case CLUSTER_POOL:
	    sts = pmdaCacheLookup(INDOM(DMTHIN_POOL_INDOM), inst, NULL, (void **)&pool);
	    if (sts < 0)
	        return sts;
	    return dmthin_pool_fetch(idp->item, &pool->pool_stats, atom);

        case CLUSTER_VOL:
	    sts = pmdaCacheLookup(INDOM(DMTHIN_VOL_INDOM), inst, NULL, (void **)&vol);
	    if (sts < 0)
	        return sts;
	    return dmthin_vol_fetch(idp->item, &vol->vol_stats, atom);

        default: /* unknown cluster */
	    return PM_ERR_PMID;
    }
    return 1;
}

static int
dmthin_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
dmthin_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
dmthin_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
dmthin_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
dmthin_init(pmdaInterface *dp)
{
    char *envpath;

    /* Check for environment variable to signify if we are under QA testing */
    if ((envpath = getenv("DMTHIN_STATSPATH")) != NULL) {
	dmthin_statspath = envpath;
        _isQA = 1;
    }

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (dp->status != 0)
	return;

    dp->version.four.instance = dmthin_instance;
    dp->version.four.fetch = dmthin_fetch;
    dp->version.four.text = dmthin_text;
    dp->version.four.pmid = dmthin_pmid;
    dp->version.four.name = dmthin_name;
    dp->version.four.children = dmthin_children;
    pmdaSetFetchCallBack(dp, dmthin_fetchCallBack);

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
    snprintf(helppath, sizeof(helppath), "%s%c" "dmthin" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, DMTHIN, "dmthin.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    dmthin_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
