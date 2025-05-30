/*
 * Sockets PMDA
 *
 * Copyright (c) 2021-2022 Red Hat.
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

#include <ctype.h>
#include "pmapi.h"
#include "pmda.h"

#include "indom.h"
#include "domain.h"
#include "cluster.h"
#include "ss_stats.h"

static int		_isDSO = 1; /* for local contexts */
static char		*username;

/* metrics supported in this PMDA - see metrictab.c */
extern pmdaMetric metrictable[];
extern int nmetrics;

static pmdaIndom indomtable[] = {
    { .it_indom = SOCKETS_INDOM },
};

pmInDom
sockets_indom(int serial)
{
    return indomtable[serial].it_indom;
}

static int
sockets_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int sts = 0;
    int privilege = 0;

    if (access("/proc/net/tcp", R_OK) == 0)
	privilege = 1;

    if (privilege)
        if ((sts = ss_refresh(indom)) < 0)
	    return sts;

    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
sockets_fetch_refresh(pmdaExt *pmda, int *need_refresh)
{
    int privilege = 0;
    int sts = 0;

    if (access("/proc/net/tcp", R_OK) == 0)
	privilege = 1;

    if (need_refresh[CLUSTER_SS] && privilege) {
        if ((sts = ss_refresh(sockets_indom(SOCKETS_INDOM))) < 0)
	    return sts;
    }

    /* refresh other clusters here, if any ... */

    return sts;
}

static int
sockets_fetch(int numpmid, pmID pmidlist[], pmdaResult **resp, pmdaExt *pmda)
{
    int i, sts, need_refresh[NUM_CLUSTERS] = { 0 };

    for (i = 0; i < numpmid; i++) {
	unsigned int	cluster = pmID_cluster(pmidlist[i]);
	if (cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }

    if ((sts = sockets_fetch_refresh(pmda, need_refresh)) < 0)
	return sts;

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

/*
 * callback provided to pmdaFetch
 */
static int
sockets_fetchCallBack(pmdaMetric *metric, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(metric->m_desc.pmid);
    ss_stats_t		*ss = NULL;
    int sts;

    switch (cluster) {
    case CLUSTER_GLOBAL:
	atom->cp = ss_filter;
    	break;

    case CLUSTER_SS:
	sts = pmdaCacheLookup(sockets_indom(SOCKETS_INDOM), inst, NULL, (void **)&ss);

	if (sts != PMDA_CACHE_ACTIVE || ss == NULL)
	    return PM_ERR_INST;

	/*
	 * Using the offset in metric->m_user, extract the field from ss
	 * for an item of type m_desc.type into the atom value
	 */
	switch (metric->m_desc.type) {
	case PM_TYPE_32:
	    atom->l = *(__int32_t *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_U32:
	    atom->ul = *(__uint32_t *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_64:
	    atom->ll = *(__int64_t *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_U64:
	    atom->ull = *(__uint64_t *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_FLOAT:
	    atom->f = *(float *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_DOUBLE:
	    atom->d = *(double *)((char *)ss + (intptr_t)(metric->m_user));
	    break;
	case PM_TYPE_STRING:
	    atom->cp = (char *)ss + (intptr_t)(metric->m_user);
	    break;
	}
	break;

    default: /* unknown cluster */
	return PM_ERR_PMID;
    }
    return PMDA_FETCH_STATIC;
}

/*
 * Restrict the allowed filter strings to only limited special
 * characters (open and close brackets - everthing else can be
 * done with alphanumerics) to limit any attack surface here.
 * The ss filtering language is more complex than we ever want
 * to be attempting to parse ourself, so we leave that side of
 * things to the ss command itself.
 */
int
sockets_check_filter(const char *string)
{
    const char *p;

    for (p = string; *p; p++) {
	if (isspace(*p))
	    continue;
	if (isalnum(*p))
	    continue;
	if (*p == '(' || *p == ')')
	    continue;
	return 0; /* disallow */
    }
    return 1;
}

static int
sockets_store(pmdaResult *result, pmdaExt *pmda)
{
    int i;
    int sts = 0;

    for (i = 0; i < result->numpmid; i++) {
    	pmValueSet *vsp = result->vset[i];
	pmAtomValue av;

	switch (pmID_cluster(vsp->pmid)) {
	case CLUSTER_GLOBAL:
	    if (vsp->numval != 1)
		sts = PM_ERR_INST;
	    else switch (pmID_item(vsp->pmid)) {
	    	case 0: /* network.persocket.filter */
		    if ((sts = pmExtractValue(vsp->valfmt, &vsp->vlist[0],
			PM_TYPE_STRING, &av, PM_TYPE_STRING)) >= 0) {
			if (sockets_check_filter(av.cp)) {
			    sts = PM_ERR_BADSTORE;
			    free(av.cp);
			    break;
			}
			if (ss_filter)
			    free(ss_filter);
			ss_filter = av.cp;
		    }
		    break;
		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	    break;
	case CLUSTER_SS:
	    sts = PM_ERR_PERMISSION;
	    break;
	default:
	    sts = PM_ERR_PMID;
	    break;
	}
    }

    return sts;
}

/*
 * Load the initial filter from filter.conf. This may subsequently be
 * overridden by pmstore network.persocket.filter ...
 */
static void
load_filter_config(void)
{
    FILE *fp;
    int sep = pmPathSeparator();
    char *p;
    char filterpath[MAXPATHLEN];
    char buf[MAXPATHLEN];

    pmsprintf(filterpath, sizeof(filterpath), "%s%c" "sockets" "%c" "filter.conf",
		pmGetConfig("PCP_SYSCONF_DIR"), sep, sep);
    if ((fp = fopen(filterpath, "r")) != NULL) {
	while (fgets(buf, sizeof(buf), fp) != NULL) {
	    if (buf[0] == '#' || buf[0] == '\n')
		continue;
	    if ((p = strrchr(buf, '\n')) != NULL)
		*p = '\0';
	    ss_filter = strndup(buf, sizeof(buf));
	    break;
	}
	fclose(fp);
    }
    if (pmDebugOptions.appl0)
    	pmNotifyErr(LOG_DEBUG, "loaded %s = \"%s\"\n", filterpath, ss_filter ? ss_filter : ""); 
}

/*
 * Initialise the agent (both daemon and DSO).
 */
void
__PMDA_INIT_CALL
sockets_init(pmdaInterface *dp)
{
    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "sockets" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "SOCKETS DSO", helppath);
    }
    else
	pmSetProcessIdentity(username);

    if (dp->status != 0)
	return;

    /* load the initial filter */
    load_filter_config();

    int	nindoms = sizeof(indomtable)/sizeof(indomtable[0]);

    if (dp->status != 0)
	return;

    dp->version.seven.instance = sockets_instance;
    dp->version.seven.fetch = sockets_fetch;
    dp->version.seven.store = sockets_store;
    pmdaSetFetchCallBack(dp, sockets_fetchCallBack);

    pmdaSetFlags(dp, PMDA_EXT_FLAG_HASHED);
    pmdaInit(dp, indomtable, nindoms, metrictable, nmetrics);

    /* load the cache (if any) and run an initial refresh */
    pmdaCacheOp(sockets_indom(SOCKETS_INDOM), PMDA_CACHE_LOAD);
    ss_refresh(sockets_indom(SOCKETS_INDOM));
}

static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMDAOPT_USERNAME,
    PMOPT_HELP,
    PMDA_OPTIONS_END
};

static pmdaOptions opts = {
    .short_options = "D:d:l:U:?",
    .long_options = longopts,
};

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    int	sep = pmPathSeparator();
    pmdaInterface dispatch;
    char helppath[MAXPATHLEN];

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmGetUsername(&username);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "sockets" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), SOCKETS, "sockets.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }
    if (opts.username)
	username = opts.username;

    pmdaOpenLog(&dispatch);
    sockets_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
