/*
 * Linux kernel Resource Control PMDA
 *
 * Copyright (c) 2023 Red Hat.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "domain.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include <stdio.h>

/*
 * RESCTRL LLC (Last-Level Cache) of CPU info.
 *
 *  Linux supports resctrl which can be used to
 *  control QoS of CPU cache and monitor LLC(last level cache).
 *  This feature depends on the 'resctrl' pseudo filesystem.
 *  The kernel should be built with the relevant config and
 *  the pseudo-filesystem needs to be mounted:
 *  mount -t resctrl resctrl -o mba_MBps /sys/fs/resctrl (on Intel)
 *  mount -t resctrl resctrl -o cdp      /sys/fs/resctrl (on AMD)
 *
 *  Which results in directories like:
 *   /sys/fs/resctrl/mon_data/mon_L3_XX
 *   
 *  These counters can be found in doc:
 *   https://github.com/torvalds/linux/tree/master/Documentation/arch/x86/resctrl.rst
 *
 * Metrics
 *   resctrl.llc.occupancy % of LLC in use
 *   resctrl.llc.mbm_local local memory bandwidth of this LLC
 *   resctrl.llc.mbm_total total memory bandwidth of this LLC
 *
 */

/*
 * instance domains
 */

static pmdaIndom indomtab[] = {
#define LLC_INDOM 0
    { LLC_INDOM, 0, NULL }
};

static pmInDom	*llc_indom = &indomtab[LLC_INDOM].it_indom;

static pmdaMetric metrictab[] = {
    // occupancy
    { NULL, 
      { PMDA_PMID(0,0), PM_TYPE_FLOAT, LLC_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    // mbm_local
    { NULL, 
      { PMDA_PMID(0,1), PM_TYPE_U64, LLC_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
    // mbm_total
    { NULL, 
      { PMDA_PMID(0,2), PM_TYPE_U64, LLC_INDOM, PM_SEM_INSTANT, 
        PMDA_PMUNITS(0,0,0,0,0,0) }, },
};

static int	isDSO = 1;		/* =0 I am a daemon */
static char	*username;
static char	mypath[MAXPATHLEN];
static char	llcdir[MAXPATHLEN];

typedef struct {
    float llc_occupancy;
    unsigned long long llc_mbm_local;
    unsigned long long llc_mbm_total;
} llc_metrics;
    
/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions     opts = {
    .short_options = "D:d:l:?",
    .long_options = longopts,
};

/*
 * callback provided to pmdaFetch
 */
static int
llc_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int cache_item = pmID_item(mdesc->m_desc.pmid);
    llc_metrics *lspm;
    
    int sts = pmdaCacheLookup(*llc_indom, inst, 0, (void*)&lspm);
    if (sts < 0 || sts == PMDA_CACHE_INACTIVE)
	return PM_ERR_INST;

    switch (cache_item) {
    case 0:		/* occupancy */
	atom->f = lspm->llc_occupancy;
	break;
    case 1:		/* mbm local */
	atom->ull = lspm->llc_mbm_local;
	break;
    case 2:		/* mbm total */
	atom->ull = lspm->llc_mbm_total;
	break;
    }
	
    return PMDA_FETCH_STATIC;
}

/*
 * This routine is called once for each pmFetch(3) operation, so caches
 * each metric for each multiple mon_L3_XX instance
 */
static int
llc_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    DIR *dirp;
    FILE *fp;
    static char	fn[MAXPATHLEN + 1024];
    char linebuf[1024];
    struct dirent *dentry;
    float llc_occupancy = 0;
    unsigned long long llc_mbm_local = 0;
    unsigned long long llc_mbm_total = 0;
    unsigned long long llc_occupancy_total;
    static const char *l3_sys_cache_size  = "/sys/devices/system/cpu/cpu0/cache/index3/size";

    /*
     * gather per LLC related statistics from the file
     * /sys/fs/llc/mon_data/mon_L3_XX
     */
    dirp = opendir(llcdir);
    if (!dirp)
	return PM_ERR_INST;

    static int l3_cache_size;

    if (!l3_cache_size) {
	if ((fp = fopen(l3_sys_cache_size, "r")) != NULL) {
	    if (fgets(linebuf, sizeof(linebuf), fp) != NULL) {
		sscanf(linebuf, "%uK\n", &l3_cache_size);
		l3_cache_size *= 1024;
	    }
	    fclose(fp);
	}
    }

    /*
     * walk the LLC directory, gather each LLC
     */
    while ((dentry = readdir(dirp))) {
	if (strncmp(dentry->d_name, "mon_L3_", 7))
	    continue;

	pmsprintf(fn, sizeof fn, "%s/%s/llc_occupancy", llcdir, dentry->d_name);
	if ((fp = fopen(fn, "r")) != NULL) {
	    if (fgets(linebuf, sizeof(linebuf), fp) != NULL && l3_cache_size) {
		sscanf(linebuf, "%llu\n", &llc_occupancy_total);
		llc_occupancy = (float)llc_occupancy_total / l3_cache_size;
	    }
	    fclose(fp);
	}

	pmsprintf(fn, sizeof fn, "%s/%s/mbm_local_bytes", llcdir, dentry->d_name);
	if ((fp = fopen(fn, "r")) != NULL) {
	    if (fgets(linebuf, sizeof(linebuf), fp) != NULL)
		sscanf(linebuf, "%llu\n", &llc_mbm_local);
	    fclose(fp);
	}

	pmsprintf(fn, sizeof fn, "%s/%s/mbm_total_bytes", llcdir, dentry->d_name);
	if ((fp = fopen(fn, "r")) != NULL) {
	    if (fgets(linebuf, sizeof(linebuf), fp) != NULL)
		sscanf(linebuf, "%llu\n", &llc_mbm_total);
	    fclose(fp);
	}

	llc_metrics *lspm = NULL;
	int sts = pmdaCacheLookupName(*llc_indom, dentry->d_name, NULL, (void **)&lspm);
	if (sts < 0 || lspm == NULL) {
	    if ((lspm = calloc(1, sizeof(*lspm))) == NULL) {
		closedir(dirp);
		return PM_ERR_INST;
	    }
	}
	/* store the completed info values into the cached structure */
	lspm->llc_occupancy = llc_occupancy;
	lspm->llc_mbm_local = llc_mbm_local;
	lspm->llc_mbm_total = llc_mbm_total;
	pmdaCacheStore(*llc_indom, PMDA_CACHE_ADD, dentry->d_name, (void *)lspm);
    }
    closedir(dirp);
    
    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void 
resctrl_init(pmdaInterface *dp)
{
    static const char *llcdir_default = "/sys/fs/resctrl/";
    const char        *pcp_llc_dir = getenv("PCP_RESCTRL_DIR");

    pcp_llc_dir = pmGetOptionalConfig("PCP_RESCTRL_DIR");
    if (!pcp_llc_dir)
	pcp_llc_dir = llcdir_default;
    pmsprintf(llcdir, sizeof(llcdir), "%s/mon_data", pcp_llc_dir);
    fprintf (stderr, "Using llc metrics: %s\n", llcdir);

    if (isDSO) {
	int sep = pmPathSeparator();
	pmsprintf(mypath, sizeof(mypath), "%s%c" "resctrl" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_LATEST, "resctrl DSO", mypath);
    } else {
	pmSetProcessIdentity(username);
    }

    if (dp->status != 0)
	return;

    dp->version.any.fetch = llc_fetch;

    pmdaSetFetchCallBack(dp, llc_fetchCallBack);

    pmdaInit(dp, indomtab, sizeof(indomtab)/sizeof(indomtab[0]), metrictab,
	     sizeof(metrictab)/sizeof(metrictab[0]));
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int			sep = pmPathSeparator();
    pmdaInterface	dispatch;

    isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);

    pmsprintf(mypath, sizeof(mypath), "%s%c" "resctrl" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_LATEST, pmGetProgname(), RESCTRL,
		"resctrl.log", mypath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    resctrl_init(&dispatch);
    pmdaMain(&dispatch);

    exit(0);
}
