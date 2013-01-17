/*
 * Global Filesystem v2 (GFS2) PMDA
 *
 * Copyright (c) 2013 Red Hat.
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
#include "dynamic.h"
#include "pmdagfs2.h"

static char *gfs2_sysfsdir = "/sys/kernel/debug/gfs2";
static pmdaIndom indomtable[] = { { .it_indom = GFS_FS_INDOM } };
#define INDOM(x) (indomtable[x].it_indom)

/*
 * all metrics supported in this PMDA - one table entry for each
 */
pmdaMetric metrictable[] = {
    { .m_desc = {
	PMDA_PMID(CLUSTER_GLOCKS, GLOCKS_TOTAL),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_GLOCKS, GLOCKS_SHARED),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_GLOCKS, GLOCKS_UNLOCKED),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_GLOCKS, GLOCKS_DEFERRED),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_GLOCKS, GLOCKS_EXCLUSIVE),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) } },
#if 0
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SRTT),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SRTTVAR),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) } },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SRTTB),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) }, },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SRTTVARB),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) }, },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SIRT),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) }, },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_SIRTVAR),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
	PMDA_PMUNITS(0,PM_TIME_NSEC,0,0,1,0) }, },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_DCOUNT),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
	PMDA_PMID(CLUSTER_SBSTATS, LOCKSTAT_QCOUNT),
	PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_COUNTER,
	PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
#endif
};

int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
}

/*
 * Update the GFS2 filesystems instance domain.  This will change
 * as filesystems are mounted and unmounted (and re-mounted, etc).
 * Using the pmdaCache interfaces simplifies things and provides us
 * with guarantees around consistent instance numbering in all of
 * those interesting corner cases.
 */
static int
gfs2_instance_refresh(void)
{
    int i, sts, count, gfs2_status;
    struct dirent **files;
    pmInDom indom = INDOM(GFS_FS_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    /* update indom cache based on scan of /sys/kernel/debug/gfs2 */
    count = scandir(gfs2_sysfsdir, &files, NULL, NULL);
    if (count < 0) {	/* give some feedback as to GFS2 kernel state */
	if (oserror() == EPERM)
	    gfs2_status = PM_ERR_PERMISSION;
	else if (oserror() == ENOENT)
	    gfs2_status = PM_ERR_AGAIN;	/* we might see a mount later */
	else
	    gfs2_status = PM_ERR_APPVERSION;
    } else {
	gfs2_status = 0;	/* we possibly have stats available */
    }

    for (i = 0; i < count; i++) {
	struct gfs2_fs *fs;
	const char *name = files[i]->d_name;

	if (name[0] == '.')
	    continue;

	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&fs);
	if (sts == PM_ERR_INST || (sts >= 0 && fs == NULL))
	    fs = calloc(1, sizeof(struct gfs2_fs));
	else if (sts < 0)
	    continue;

	/* (re)activate this entry for the current query */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)fs);
    }

    for (i = 0; i < count; i++)
	free(files[i]);
    if (count > 0)
	free(files);
    return gfs2_status;
}

static int
gfs2_instance(pmInDom indom, int inst, char *name, __pmInResult **result, pmdaExt *pmda)
{
    gfs2_instance_refresh();
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
gfs2_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    pmInDom indom = INDOM(GFS_FS_INDOM);
    struct gfs2_fs *fs;
    char *name;
    int i, sts;

    if ((sts = gfs2_instance_refresh()) < 0)
	return sts;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, i, &name, (void **)&fs) || !fs)
	    continue;
	if (need_refresh[CLUSTER_GLOCKS])
	    gfs2_refresh_glocks(gfs2_sysfsdir, name, &fs->glocks);
#if 0
	if (need_refresh[CLUSTER_SBSTATS])
	    gfs2_refresh_sbstats(gfs2_sysfsdir, name, &fs->sbstats);
#endif
    }
    return sts;
}

static int
gfs2_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int		i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	__pmID_int *idp = (__pmID_int *)&(pmidlist[i]);
	if (idp->cluster >= 0 && idp->cluster < NUM_CLUSTERS)
	    need_refresh[idp->cluster]++;
    }

    if ((sts = gfs2_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */

static int
gfs2_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    __pmID_int		*idp = (__pmID_int *)&(mdesc->m_desc.pmid);
    struct gfs2_fs	*fs;
    int			sts;

    switch (idp->cluster) {
    case CLUSTER_GLOCKS:
	sts = pmdaCacheLookup(INDOM(GFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return gfs2_glocks_fetch(idp->item, &fs->glocks, atom);

    case CLUSTER_GLSTATS:
	return PM_ERR_NYI;	/* Not Yet Implemented */
#if 0
    case CLUSTER_SBSTATS:
	sts = pmdaCacheLookup(INDOM(GFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return gfs2_sbstats_fetch(idp->item, &fs->sbstats, atom);
#endif
    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}

#if 0
static int
gfs2_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = gfs2_dynamic_lookup_text(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
gfs2_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsTree *tree = gfs2_dynamic_lookup_name(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
gfs2_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsTree *tree = gfs2_dynamic_lookup_pmid(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
gfs2_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    __pmnsTree *tree = gfs2_dynamic_lookup_name(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}
#endif

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
gfs2_init(pmdaInterface *dp)
{
    if (dp->status != 0)
	return;

    dp->version.four.instance = gfs2_instance;
    dp->version.four.fetch = gfs2_fetch;
#if 0
    dp->version.four.text = gfs2_text;
    dp->version.four.pmid = gfs2_pmid;
    dp->version.four.name = gfs2_name;
    dp->version.four.children = gfs2_children;
#endif
    pmdaSetFetchCallBack(dp, gfs2_fetchCallBack);

#if 0
    gfs2_sbstats_init();
#endif
    pmdaInit(dp, indomtable, sizeof(indomtable)/sizeof(indomtable[0]),
                 metrictable, sizeof(metrictable)/sizeof(metrictable[0]));
}

static void
usage(void)
{
    fprintf(stderr, "Usage: %s [options]\n\n", pmProgname);
    fputs("Options:\n"
	  "  -d domain    use domain (numeric) for metrics domain of PMDA\n"
	  "  -l logfile   write log into logfile rather than using default log name\n"
	  "  -U username  user account to run under (default \"pcp\")\n",
	  stderr);		
    exit(1);
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = __pmPathSeparator();
    int			err = 0;
    pmdaInterface	dispatch;
    char		helppath[MAXPATHLEN];

    __pmSetProgname(argv[0]);
    snprintf(helppath, sizeof(helppath), "%s%c" "gfs2" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_4, pmProgname, GFS2, "gfs2.log", helppath);

    if (pmdaGetOpt(argc, argv, "D:d:l:?", &dispatch, &err) != EOF)
	err++;

    if (err)
	usage();

    pmdaOpenLog(&dispatch);
    gfs2_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
