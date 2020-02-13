/*
 * Copyright (c) 2018 Red Hat.
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

#include "podman.h"
#include "libpcp.h"
#include "domain.h"

static pmdaIndom podman_indomtab[NUM_INDOMS];
#define INDOM(x) (podman_indomtab[x].it_indom)

#define NUM_METRICS (NUM_CONTAINER_STATS + NUM_CONTAINER_INFO + NUM_POD_INFO)
static pmdaMetric podman_metrictab[] = {

    /* container stats cluster (0) */
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_NET_INPUT),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_NET_OUTPUT),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_BLOCK_INPUT),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_BLOCK_OUTPUT),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_CPU),
		PM_TYPE_DOUBLE, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,0,0,PM_TIME_SEC,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_CPU_NANO),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_SYSTEM_NANO),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,1,0,0,PM_TIME_NSEC,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_MEM_USAGE),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_MEM_LIMIT),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_MEM_PERC),
		PM_TYPE_DOUBLE, CONTAINER_INDOM, PM_SEM_COUNTER,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_STATS, STATS_PIDS),
		PM_TYPE_U32, CONTAINER_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },

    /* container info cluster (1) */
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_NAME),
		PM_TYPE_STRING, CONTAINER_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_COMMAND),
		PM_TYPE_STRING, CONTAINER_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_STATUS),
		PM_TYPE_STRING, CONTAINER_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_RWSIZE),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_ROOTFSSIZE),
		PM_TYPE_U64, CONTAINER_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(1,0,0,PM_SPACE_KBYTE,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_INFO, INFO_RUNNING),
		PM_TYPE_U32, CONTAINER_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },

    /* pod info cluster (2) */
    { .m_desc = { PMDA_PMID(CLUSTER_POD, POD_NAME),
		PM_TYPE_STRING, POD_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_POD, POD_CGROUP),
		PM_TYPE_STRING, POD_INDOM, PM_SEM_DISCRETE,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_POD, POD_STATUS),
		PM_TYPE_STRING, POD_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,0,0,0,0) }, },
    { .m_desc = { PMDA_PMID(CLUSTER_POD, POD_CONTAINERS),
		PM_TYPE_U32, POD_INDOM, PM_SEM_INSTANT,
		PMDA_PMUNITS(0,0,1,0,0,PM_COUNT_ONE) }, },
};

char *
podman_strings_lookup(int index)
{
    char	*value;
    pmInDom	dict = INDOM(STRINGS_INDOM);

    if (index == -1)
	return "";
    if (pmdaCacheLookup(dict, index, &value, NULL) == PMDA_CACHE_ACTIVE)
	return value;
    return "";
}

int
podman_strings_insert(const char *string)
{
    pmInDom	dict = INDOM(STRINGS_INDOM);

    if (string == NULL)
	return -1;
    return pmdaCacheStore(dict, PMDA_CACHE_ADD, string, NULL);
}

static void
podman_refresh_container(pmdaExt *pmda, pmInDom indom, int inst, state_flags_t flags)
{
    container_t		*cp = podman_context_container(pmda->e_context);
    char		*name = NULL;
    int			sts;

    if (cp != NULL)
	name = podman_strings_lookup(cp->id);
    else if (inst != PM_IN_NULL) {
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&cp);
	name = (sts < 0) ? NULL : podman_strings_lookup(cp->id);
    }
    if (name && name[0] == '\0')
	name = NULL;

    if (name)
	refresh_podman_container(indom, name, flags);
    else
	refresh_podman_containers(indom, flags);
}

static void
podman_refresh_info(pmdaExt *pmda, pmInDom indom, int inst, char *name)
{
    container_t		*cp = podman_context_container(pmda->e_context);
    int			sts;

    if (cp != NULL)
	name = podman_strings_lookup(cp->id);
    else if (inst != PM_IN_NULL) {
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&cp);
	name = (sts < 0) ? NULL : podman_strings_lookup(cp->id);
    }
    if (name && name[0] == '\0')
	name = NULL;

    if (name)
	refresh_podman_container(indom, name, STATE_INFO);
    else
	refresh_podman_containers(indom, STATE_INFO);
}

static void
podman_refresh_pod(pmdaExt *pmda, pmInDom indom, int inst, char *name)
{
    container_t		*cp;
    int			sts;

    if (inst != PM_IN_NULL) {
	sts = pmdaCacheLookup(indom, inst, &name, (void **)&cp);
	name = (sts < 0) ? NULL : podman_strings_lookup(cp->id);
    }
    if (name && name[0] == '\0')
	name = NULL;

    if (name)
	refresh_podman_pod_info(indom, name);
    else
	refresh_podman_pods_info(indom);
}

static int
podman_instance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    unsigned int	serial = pmInDom_serial(indom);

    switch(serial) {
    case CONTAINER_INDOM:
	podman_refresh_info(pmda, indom, inst, name);
	break;
    case POD_INDOM:
	podman_refresh_pod(pmda, indom, inst, name);
	break;
    }
    return pmdaInstance(indom, inst, name, result, pmda);
}

static int
podman_stats_fetchCallBack(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    container_t		*cp;
    int			sts;

    sts = pmdaCacheLookup(INDOM(CONTAINER_INDOM), inst, NULL, (void **)&cp);
    if (sts < 0)
	return sts;
    if (sts != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;
    if (!(cp->flags & STATE_STATS))
	return 0;

    switch (item) {
    case STATS_NET_INPUT:
	atom->ull = cp->stats.net_input;
	break;
    case STATS_NET_OUTPUT:
	atom->ull = cp->stats.net_output;
	break;
    case STATS_BLOCK_INPUT:
	atom->ull = cp->stats.block_input;
	break;
    case STATS_BLOCK_OUTPUT:
	atom->ull = cp->stats.block_output;
	break;
    case STATS_CPU:
	atom->d = cp->stats.cpu;
	break;
    case STATS_CPU_NANO:
	atom->ull = cp->stats.cpu_nano;
	break;
    case STATS_SYSTEM_NANO:
	atom->ull = cp->stats.system_nano;
	break;
    case STATS_MEM_USAGE:
	atom->ull = cp->stats.mem_usage;
	break;
    case STATS_MEM_LIMIT:
	atom->ull = cp->stats.mem_limit;
	break;
    case STATS_MEM_PERC:
	atom->d = cp->stats.mem_perc;
	break;
    case STATS_PIDS:
	atom->ul = cp->stats.nprocesses;
	break;
    default:
	return PM_ERR_PMID;
    }
    return 1;
}

static int
podman_info_fetchCallBack(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    container_t		*cp;
    int			sts;

    sts = pmdaCacheLookup(INDOM(CONTAINER_INDOM), inst, NULL, (void **)&cp);
    if (sts < 0)
	return sts;
    if (sts != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;
    if (!(cp->flags & STATE_INFO))
	return 0;

    switch (item) {
    case INFO_NAME:
	atom->cp = podman_strings_lookup(cp->info.name);
	break;
    case INFO_COMMAND:
	atom->cp = podman_strings_lookup(cp->info.command);
	break;
    case INFO_STATUS:
	atom->cp = podman_strings_lookup(cp->info.status);
	break;
    case INFO_RWSIZE:
	atom->ull = cp->info.rwsize;
	break;
    case INFO_ROOTFSSIZE:
	atom->ull = cp->info.rootfssize;
	break;
    case INFO_RUNNING:
	atom->ul = cp->info.running;
	break;
    default:
	return PM_ERR_PMID;
    }
    return 1;
}

static int
podman_pod_fetchCallBack(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
    pod_info_t		*pp;
    int			sts;

    sts = pmdaCacheLookup(INDOM(POD_INDOM), inst, NULL, (void **)&pp);
    if (sts < 0)
	return sts;
    if (sts != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    switch (item) {
    case POD_NAME:
	atom->cp = podman_strings_lookup(pp->name);
	break;
    case POD_CGROUP:
	atom->cp = podman_strings_lookup(pp->cgroup);
	break;
    case POD_STATUS:
	atom->cp = podman_strings_lookup(pp->status);
	break;
    case POD_CONTAINERS:
	atom->ul = pp->ncontainers;
	break;
    default:
	return PM_ERR_PMID;
    }
    return 1;
}

static int
podman_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);

    switch (cluster) {
    case CLUSTER_STATS:
	return podman_stats_fetchCallBack(item, inst, atom);
    case CLUSTER_INFO:
	return podman_info_fetchCallBack(item, inst, atom);
    case CLUSTER_POD:
	return podman_pod_fetchCallBack(item, inst, atom);
    default:
	break;
    }
    return PM_ERR_PMID;
}

static int
podman_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    unsigned int	cluster, need_refresh[NUM_CLUSTERS] = { 0 };
    state_flags_t	flags = STATE_NONE;
    int			i;

    for (i = 0; i < numpmid; i++) {
	cluster = pmID_cluster(pmidlist[i]);
	if (cluster < NUM_CLUSTERS)
	    need_refresh[cluster]++;
    }

    if (need_refresh[CLUSTER_STATS])
	flags |= STATE_STATS;
    if (need_refresh[CLUSTER_INFO])
	flags |= STATE_INFO;

    if (flags != STATE_NONE)
	podman_refresh_container(pmda, INDOM(CONTAINER_INDOM), PM_IN_NULL, flags);
    if (need_refresh[CLUSTER_POD])
	podman_refresh_pod(pmda, INDOM(POD_INDOM), PM_IN_NULL, NULL);

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

static int
podman_labelCallBack(pmInDom indom, unsigned int inst, pmLabelSet **lp)
{
    container_t		*cp;
    pod_info_t		*pp;
    void		*vp;
    int			sts;

    sts = pmdaCacheLookup(indom, inst, NULL, &vp);
    if (sts < 0)
	return 0;
    if (sts != PMDA_CACHE_ACTIVE)
	return 0;
    if (indom == INDOM(POD_INDOM)) {
	pp = (pod_info_t *)vp;
	// for (i = 0; i < cp->info.labels; i++)
	//	pmdaAddLabels(lp, "{\"%s\":\"%s\"}",
	//		podman_strings_lookup(pp->labelnames[n]),
	//		podman_strings_lookup(pp->labelvalue[n]));
	return pp->labels;
    }
    if (indom == INDOM(CONTAINER_INDOM)) {
	cp = (container_t *)vp;
	if (cp->podmap)
	    pmdaAddLabels(lp, "{\"pod\":\"%s\"}",
			    podman_strings_lookup(cp->podmap));
	// for (i = 0; i < cp->info.labels; i++)
	//	pmdaAddLabels(lp, "{\"%s\":\"%s\"}",
	//		podman_strings_lookup(cp->info.labelnames[n]),
	//		podman_strings_lookup(cp->info.labelvalues[n]));
	return cp->info.labels + (cp->podmap? 1 : 0);
    }
    return 0;
}

static int
podman_label(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    return pmdaLabel(ident, type, lpp, pmda);
}

static int
podman_attribute(int ctx, int attr, const char *value, int len, pmdaExt *pmda)
{
    switch (attr) {
    case PMDA_ATTR_CONTAINER:
	podman_context_set_container(ctx, INDOM(CONTAINER_INDOM), value, len);
	break;
    default:
	break;
    }
    return pmdaAttribute(ctx, attr, value, len, pmda);
}

static pmLongOptions   longopts[] = {
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

static int		_isDSO = 1; /* for local contexts */

void
__PMDA_INIT_CALL
podman_init(pmdaInterface *dp)
{
    if (_isDSO) {
	char helppath[MAXPATHLEN];
	int sep = pmPathSeparator();
	pmsprintf(helppath, sizeof(helppath), "%s%c" "podman" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
	pmdaDSO(dp, PMDA_INTERFACE_7, "podman DSO", helppath);
    }

    if (dp->status != 0)
	return;

    dp->version.seven.fetch = podman_fetch;
    dp->version.seven.label = podman_label;
    dp->version.seven.instance = podman_instance;
    dp->version.seven.attribute = podman_attribute;
    pmdaSetFetchCallBack(dp, podman_fetchCallBack);
    pmdaSetLabelCallBack(dp, podman_labelCallBack);
    pmdaSetEndContextCallBack(dp, podman_context_end);

    podman_indomtab[CONTAINER_INDOM].it_indom = CONTAINER_INDOM;
    podman_indomtab[STRINGS_INDOM].it_indom = STRINGS_INDOM;
    podman_indomtab[POD_INDOM].it_indom = POD_INDOM;

    pmdaInit(dp, podman_indomtab, NUM_INDOMS, podman_metrictab, NUM_METRICS);

    /* string metrics use the pmdaCache API for value indexing */
    pmdaCacheOp(INDOM(STRINGS_INDOM), PMDA_CACHE_STRINGS);

    /* container and pod metrics use the pmdaCache API for indom indexing */
    pmdaCacheOp(INDOM(CONTAINER_INDOM), PMDA_CACHE_CULL);
    pmdaCacheOp(INDOM(POD_INDOM), PMDA_CACHE_CULL);
}

int
main(int argc, char **argv)
{
    int			sep = pmPathSeparator();
    char		helppath[MAXPATHLEN];
    pmdaInterface	dispatch;

    _isDSO = 0;
    pmSetProgname(argv[0]);
    pmsprintf(helppath, sizeof(helppath), "%s%c" "podman" "%c" "help",
		pmGetConfig("PCP_PMDAS_DIR"), sep, sep);
    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), PODMAN, "podman.log", helppath);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
	pmdaUsageMessage(&opts);
	exit(1);
    }

    pmdaOpenLog(&dispatch);
    podman_init(&dispatch);
    pmdaConnect(&dispatch);
    pmdaMain(&dispatch);
    exit(0);
}
