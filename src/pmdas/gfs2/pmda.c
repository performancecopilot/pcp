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
#include "pmdagfs2.h"
#include <sys/types.h>

static char *gfs2_sysfsdir = "/sys/kernel/debug/gfs2";
static char *gfs2_sysdir = "/sys/fs/gfs2";

pmdaIndom indomtable[] = { 
    { .it_indom = GFS_FS_INDOM }, 
};

#define INDOM(x) (indomtable[x].it_indom)

/*
 * all metrics supported in this PMDA - one table entry for each
 *
 */
pmdaMetric metrictable[] = {
    /* GLOCK */
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
    /* SBSTATS */
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
    /* GLSTATS */
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_TOTAL),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_TRANS),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_INODE),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_RGRP),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_META),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_IOPEN),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_FLOCK),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_QUOTA),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_GLSTATS, GLSTATS_JOURNAL),
        PM_TYPE_U64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* GFS2_GLOCK_LOCK_TIME */
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_LOCK_TYPE),
        PM_TYPE_U32, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_NUMBER),
        PM_TYPE_U32, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SRTT),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SRTTVAR),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SRTTB),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SRTTVARB),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SIRT),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_SIRTVAR),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_DLM),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = {
        PMDA_PMID(CLUSTER_LOCKTIME, LOCKTIME_QUEUE),
        PM_TYPE_64, GFS_FS_INDOM, PM_SEM_INSTANT,
        PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    /* GFS2.CONTROL */
    { NULL, {
        PMDA_PMID(CLUSTER_CONTROL, CONTROL_GLOCK_LOCK_TIME),
        PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { NULL, {
        PMDA_PMID(CLUSTER_CONTROL, CONTROL_FTRACE_GLOCK_THRESHOLD),
        PM_TYPE_U32, PM_INDOM_NULL, PM_SEM_DISCRETE,
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

int
metrictable_size(void)
{
    return sizeof(metrictable)/sizeof(metrictable[0]);
}

static dev_t
gfs2_device_identifier(const char *name)
{
    char buffer[4096]; 
    int major, minor;
    dev_t dev_id;
    FILE *fp;

    dev_id = makedev(0, 0); /* Error case */

    /* gfs2_glock_lock_time requires block device id for each filesystem
     * in order to match to the lock data, this info can be found in
     * /sys/fs/gfs2/NAME/id, we extract the data and store it in gfs2_fs->dev
     *
     */
    snprintf(buffer, sizeof(buffer), "%s/%s/id", gfs2_sysdir, name);
    buffer[sizeof(buffer)-1] = '\0';

    if ((fp = fopen(buffer, "r")) == NULL)
	return oserror();

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        sscanf(buffer, "%d:%d", &major, &minor);
        dev_id = makedev(major, minor);
    }     
    fclose(fp);
    
    return dev_id;
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

    /* update indom cache based on scan of /sys/fs/gfs2 */
    count = scandir(gfs2_sysdir, &files, NULL, NULL);
    if (count < 0) { /* give some feedback as to GFS2 kernel state */
	if (oserror() == EPERM)
	    gfs2_status = PM_ERR_PERMISSION;
	else if (oserror() == ENOENT)
	    gfs2_status = PM_ERR_AGAIN;	/* we might see a mount later */
	else
	    gfs2_status = PM_ERR_APPVERSION;
    } else {
	gfs2_status = 0; /* we possibly have stats available */
    }

    for (i = 0; i < count; i++) {
	struct gfs2_fs *fs;
	const char *name = files[i]->d_name;

	if (name[0] == '.')
	    continue;

	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&fs);
	if (sts == PM_ERR_INST || (sts >= 0 && fs == NULL)){
	    fs = calloc(1, sizeof(struct gfs2_fs));
            if (fs == NULL)
                return PM_ERR_AGAIN;

            fs->dev_id = gfs2_device_identifier(name);

            if ((major(fs->dev_id) == 0) && (minor(fs->dev_id) == 0)) {
                free(fs);
                return PM_ERR_AGAIN;
            }
        }   
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
	if (need_refresh[CLUSTER_SBSTATS])
	    gfs2_refresh_sbstats(gfs2_sysfsdir, name, &fs->sbstats);
        if (need_refresh[CLUSTER_GLSTATS])
            gfs2_refresh_glstats(gfs2_sysfsdir, name, &fs->glstats);
    }

    if (need_refresh[CLUSTER_LOCKTIME])
        gfs2_refresh_ftrace_stats(indom);

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

    case CLUSTER_SBSTATS:
	sts = pmdaCacheLookup(INDOM(GFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return gfs2_sbstats_fetch(idp->item, &fs->sbstats, atom);

    case CLUSTER_GLSTATS:
        sts = pmdaCacheLookup(INDOM(GFS_FS_INDOM), inst, NULL, (void **)&fs);
	if (sts < 0)
	    return sts;
	return gfs2_glstats_fetch(idp->item, &fs->glstats, atom);

    case CLUSTER_LOCKTIME:
        /* Is the tracepoint activated, does it exist on this system? */
        if (gfs2_control_check_value(control_locations[CONTROL_GLOCK_LOCK_TIME]) == 0)
            return 0; /* no values available */
        
        sts = pmdaCacheLookup(INDOM(GFS_FS_INDOM), inst, NULL, (void**)&fs);
        if (sts < 0)
            return sts;
        return gfs2_lock_time_fetch(idp->item, &fs->lock_time, atom);

    case CLUSTER_CONTROL:
        switch (idp->item) {
        case CONTROL_GLOCK_LOCK_TIME: /* gfs2.control.glock_lock_time */
            atom->ul = gfs2_control_fetch(idp->item);
            break;
        case CONTROL_FTRACE_GLOCK_THRESHOLD: /* gfs2.control.glock_threshold */
            atom->ul = ftrace_get_threshold();
            break;
        default:
            return PM_ERR_PMID;
        }
        break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }

    return 1;
}

static int
gfs2_store(pmResult *result, pmdaExt *pmda)
{
    int		i;
    int		sts = 0;
    pmValueSet	*vsp;
    __pmID_int	*pmidp;

    for (i = 0; i < result->numpmid && !sts; i++) {
	vsp = result->vset[i];
	pmidp = (__pmID_int *)&vsp->pmid;

	if (pmidp->cluster == CLUSTER_CONTROL && pmidp->item == CONTROL_GLOCK_LOCK_TIME) {
            sts = gfs2_control_set_value(control_locations[CONTROL_GLOCK_LOCK_TIME], vsp);
        }

        if (pmidp->cluster == CLUSTER_CONTROL && pmidp->item == CONTROL_FTRACE_GLOCK_THRESHOLD) {
            sts = ftrace_set_threshold(vsp);
        }
    }
    return sts;
}

static int
gfs2_text(int ident, int type, char **buf, pmdaExt *pmda)
{
    if ((type & PM_TEXT_PMID) == PM_TEXT_PMID) {
	int sts = pmdaDynamicLookupText(ident, type, buf, pmda);
	if (sts != -ENOENT)
	    return sts;
    }
    return pmdaText(ident, type, buf, pmda);
}

static int
gfs2_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreePMID(tree, name, pmid);
}

static int
gfs2_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupPMID(pmda, pmid);
    return pmdaTreeName(tree, pmid, nameset);
}

static int
gfs2_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    __pmnsTree *tree = pmdaDynamicLookupName(pmda, name);
    return pmdaTreeChildren(tree, name, flag, kids, sts);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
gfs2_init(pmdaInterface *dp)
{
    int		nindoms = sizeof(indomtable)/sizeof(indomtable[0]);
    int		nmetrics = sizeof(metrictable)/sizeof(metrictable[0]);

    if (dp->status != 0)
	return;

    dp->version.four.instance = gfs2_instance;
    dp->version.four.store = gfs2_store;
    dp->version.four.fetch = gfs2_fetch;
    dp->version.four.text = gfs2_text;
    dp->version.four.pmid = gfs2_pmid;
    dp->version.four.name = gfs2_name;
    dp->version.four.children = gfs2_children;
    pmdaSetFetchCallBack(dp, gfs2_fetchCallBack);

    gfs2_sbstats_init(metrictable, nmetrics);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);
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
