/*
 * Common Internet File System (CIFS) PMDA
 *
 * Copyright (c) 2014 Red Hat.
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

#include "pmdacifs.h"

static int _isDSO = 1; /* for local contexts */

static char *cifs_procfsdir = "/proc/fs/cifs";
static char *cifs_statspath = "";

pmdaIndom indomtable[] = { 
    { .it_indom = CIFS_FS_INDOM }, 
};

#define INDOM(x) (indomtable[x].it_indom)

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
pmdaMetric metrictable[] = { 
    /* GLOBAL STATS */
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SESSION),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SHARES),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_BUFFER),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_POOL_SIZE),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SMALL_BUFFER),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_SMALL_POOL_SIZE),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_MID_OPS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_TOTAL_OPERATIONS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_TOTAL_RECONNECTS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc =  {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_VFS_OPS),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLOBAL_STATS, GLOBAL_VFS_OPS_MAX),
        PM_TYPE_U64, PM_INDOM_NULL, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* PER CIFS SHARE STATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CONNECTED),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SMBS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_OPLOCK_BREAKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_READ),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_READ_BYTES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_WRITE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_WRITE_BYTES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FLUSHES),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_LOCKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_HARD_LINKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_SYM_LINKS),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_OPEN),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_CLOSE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_DELETE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_POSIX_OPEN),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_POSIX_MKDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_MKDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_RMDIR),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_RENAME),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_T2_RENAME),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_FIRST),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_NEXT),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_FS_STATS, FS_FIND_CLOSE),
        PM_TYPE_U64, CIFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
}

/*
 * Update the CIFS filesystems instance domain. This will change
 * as filesystems are mounted and unmounted (and re-mounted, etc).
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
cifs_instance_refresh(void)
{
    int sts;
    char buffer[PATH_MAX], fsname[PATH_MAX];
    FILE *fp;
    pmInDom indom = INDOM(CIFS_FS_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based off of /proc/fs/cifs/Stats file */
    snprintf(buffer, sizeof(buffer), "%s%s/Stats", cifs_statspath, cifs_procfsdir);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL)
        return -oserror();

    while (fgets(buffer, sizeof(buffer) - 1, fp)) {
        if (strstr(buffer, "\\\\") == 0)
            continue;

        sscanf(buffer, "%*d%*s %s %*s", fsname);

        /* at this point fsname contains our cifs mount name: \\<server>\<share>
           this will be used to map stats to file-system instances */

        struct cifs_fs *fs;

	sts = pmdaCacheLookupName(indom, fsname, NULL, (void **)&fs);
	if (sts == PM_ERR_INST || (sts >= 0 && fs == NULL)){
	    fs = calloc(1, sizeof(struct cifs_fs));
            if (fs == NULL) {
                fclose(fp);
                return PM_ERR_AGAIN;
            }
            strcpy(fs->name, fsname);
        }   
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, fsname, (void *)fs);
    }
    fclose(fp);
    return 0;
}

static int
cifs_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    cifs_instance_refresh();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
cifs_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom = INDOM(CIFS_FS_INDOM);
    struct cifs_fs *fs;
    char *name;
    int i, sts;

    if ((sts = cifs_instance_refresh()) < 0)
	return sts;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, i, &name, (void **)&fs) || !fs)
	    continue;

        if (need_refresh[CLUSTER_FS_STATS])
            cifs_refresh_fs_stats(cifs_statspath, cifs_procfsdir, name, &fs->fs_stats);
    }
    if (need_refresh[CLUSTER_GLOBAL_STATS])
        cifs_refresh_global_stats(cifs_statspath, cifs_procfsdir, name);

    return sts;
}

static int
cifs_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }

    if ((sts = cifs_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
cifs_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int *idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    struct cifs_fs *fs;
    int sts;

    switch (idp->cluster) {
    case CLUSTER_GLOBAL_STATS:
        return cifs_global_stats_fetch(idp->item, atom);

    case CLUSTER_FS_STATS:
	sts = pmdaCacheLookup(INDOM(CIFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return cifs_fs_stats_fetch(idp->item, &fs->fs_stats, atom);

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}

static int
cifs_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
cifs_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
cifs_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
cifs_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
__PMDA_INIT_CALL
cifs_init(pmdaInterface *dp)
{
    char *envpath;

    if ((envpath = getenv("CIFS_STATSPATH")) != NULL)
	cifs_statspath = envpath;

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int	nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = __pmPathSeparator();
	snprintf(helppath, sizeof(helppath), "%s%c" "cifs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_4, "CIFS DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->version.four.instance = cifs_instance;
    dp->version.four.fetch = cifs_fetch;
    dp->version.four.text = cifs_text;
    dp->version.four.pmid = cifs_pmid;
    dp->version.four.name = cifs_name;
    dp->version.four.children = cifs_children;
    pmdaSetFetchCallBack(dp, cifs_fetchCallBack);

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

    _isDSO = 0;

    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "cifs" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, CIFS, "cifs.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    cifs_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
