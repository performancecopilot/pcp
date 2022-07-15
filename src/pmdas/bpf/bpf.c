/*
 * BPF wrapper metric module.
 *
 * Copyright (c) 2021 Netflix, Inc.
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
#include "modules/module.h"
#include <sys/stat.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include <bpf/bpf.h>
#include <dlfcn.h>
#include "bpf.h"

/* see libpcp.h __pmXx_int */
#define MAX_CLUSTER_ID ((1<<12) - 1)
#define MAX_INDOM_ID ((1<<22) - 1)

/* pmdaCacheOp cache IDs */
#define CACHE_CLUSTER_IDS 0
#define CACHE_INDOM_IDS 1

static int	isDSO = 1;		/* =0 I am a daemon */

/* metric and indom configuration will be dynamically filled in by modules */
static pmdaMetric * metrictab;
static pmdaIndom * indomtab;
static int metric_count;
static int indom_count;
static pmdaNameSpace *pmns;

/* command line option handling - both short and long options */
static pmLongOptions longopts[] = {
    PMDA_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    PMDAOPT_DOMAIN,
    PMDAOPT_LOGFILE,
    PMOPT_HELP,
    PMDA_OPTIONS_TEXT("\nExactly one of the following options may appear:"),
    PMDAOPT_INET,
    PMDAOPT_PIPE,
    PMDAOPT_UNIX,
    PMDAOPT_IPV6,
    PMDA_OPTIONS_END
};
static pmdaOptions opts = {
    .short_options = "D:d:i:l:pu:6:?",
    .long_options = longopts,
};

dict *pmda_config;

/*
 * callback provided to pmdaFetch
 */
static int
bpf_fetchCallBack(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    unsigned int	cluster = pmID_cluster(mdesc->m_desc.pmid);
    unsigned int	item = pmID_item(mdesc->m_desc.pmid);

    int cache_result;
    module* mod;

    // only modules that have completed their init() successfully will be
    // available in modules_by_cluster
    cache_result = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster, NULL, (void**)&mod);
    if (cache_result == PMDA_CACHE_ACTIVE) {
        return mod->fetch_to_atom(item, inst, atom);
    }

    // To get here, must have had a valid entry in PMNS (so, a known cluster),
    // and yet no cluster id -> module map was found. Implication is that this
    // module exists but was not requested in configuration, so, 'no values' is
    // appropriate as the cluster is known but we have no values for it.
    return PMDA_FETCH_NOVALUES;
}

/**
 * wrapper for libbpf logging
 */
int bpf_printfn(enum libbpf_print_level level, const char *out, va_list ap)
{
    char logline[1024];
    vsprintf(logline, out, ap);
    size_t ln = strlen(logline) - 1;
    if (*logline && logline[ln] == '\n')
        logline[ln] = '\0';

    int pmLevel;
    switch(level) {
        case LIBBPF_WARN:
            pmLevel = LOG_WARNING;
            break;
        case LIBBPF_INFO:
            pmLevel = LOG_INFO;
            break;
        case LIBBPF_DEBUG:
            pmLevel = LOG_DEBUG;
            break;
        default:
            pmLevel = LOG_ERR;
            break;
    }

    pmNotifyErr(pmLevel, "%s", logline);
    return 0;
}

#define WANT_MEM	(100LL*1024*1024)
/**
 * setrlimit required for BPF loading ... try for WANT_MEM, but will
 * accept whatever we can get
 */
void bpf_setrlimit()
{
    struct rlimit	rnew;
    int			ret;
    ret = getrlimit(RLIMIT_MEMLOCK, &rnew);
    if (ret < 0) {
        pmNotifyErr(LOG_ERR, "bpf_setrlimit: getrlimit RMLIMIT_MEMLOCK failed: %s", pmErrStr(-errno));
	return;
    }
    if (rnew.rlim_max == RLIM_INFINITY)
	rnew.rlim_cur = rnew.rlim_max = WANT_MEM;
    else if (rnew.rlim_max > WANT_MEM)
	rnew.rlim_cur = rnew.rlim_max = WANT_MEM;
    else {
	rnew.rlim_cur = rnew.rlim_max;
        pmNotifyErr(LOG_INFO, "bpf_setrlimit: setrlimit RMLIMIT_MEMLOCK %lld not %lld", (long long)rnew.rlim_cur, WANT_MEM);
    }
    ret = setrlimit(RLIMIT_MEMLOCK, &rnew);
    if (ret == 0)
        pmNotifyErr(LOG_INFO, "setrlimit RMLIMIT_MEMLOCK ok");
    else
        pmNotifyErr(LOG_ERR, "setrlimit RMLIMIT_MEMLOCK (%lld,%lld) failed: %s", (long long)rnew.rlim_cur, (long long)rnew.rlim_max, pmErrStr(-errno));
}

/**
 * Load a single module from modules/
 *
 * This will call dlopen, look up the bpf_module to load the module.
 */
module* bpf_load_module(char * name)
{
    module *loaded_module = NULL;
    char *fullpath;
    char *error;

    int ret;
    ret = asprintf(&fullpath, "%s/bpf/modules/%s", pmGetConfig("PCP_PMDAS_DIR"), name);
    if (ret < 0) {
        pmNotifyErr(LOG_ERR, "could not construct log string?");
        return NULL;
    }

    pmNotifyErr(LOG_INFO, "loading: %s from %s", name, fullpath);

    void * dl_module = NULL;
    dl_module = dlopen(fullpath, RTLD_NOW);
    if (!dl_module) {
        error = dlerror();
        pmNotifyErr(LOG_INFO, "dlopen failed: %s, %s", fullpath, error);
        goto cleanup;
    }

    loaded_module = dlsym(dl_module, "bpf_module");
    if ((error = dlerror()) != NULL) {
        pmNotifyErr(LOG_ERR, "dlsym failed to find module: %s, %s", fullpath, error);
    }

cleanup:
    free(fullpath);
    return loaded_module;
}

/**
 * load all known modules
 */
void
bpf_load_modules(dict *cfg)
{
    int ret, split_count;
    char errorstring[1024];
    int module_count = 0;
    char *module_name;
    module *bpf_module;
    dictIterator *iterator;
    dictEntry *entry;
    sds entry_key;
    sds entry_val;
    sds *entry_key_split;
    sds sds_enabled;
    sds sds_true;
    unsigned int cluster_id;

    sds_enabled = (sdsnew("enabled"));
    sds_true = (sdsnew("true"));

    pmdaCacheResize(CACHE_CLUSTER_IDS, MAX_CLUSTER_ID);
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_STRINGS);
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_LOAD);
    pmdaCacheResize(CACHE_INDOM_IDS, MAX_INDOM_ID);
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_STRINGS);
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_LOAD);

    pmNotifyErr(LOG_INFO, "loading modules");

    iterator = dictGetIterator(cfg);
    while((entry = dictNext(iterator)) != NULL)
    {
        entry_key = dictGetKey(entry);
        entry_val = dictGetVal(entry);

        // to find sections, we look for 'module.so.enabled' = 'true'
        // we don't know what module.so is, so we have to do a little string setup
        entry_key_split = sdssplitlen(entry_key, sdslen(entry_key), ".", 1, &split_count);
        if (split_count != 3
            || (sdscmp(entry_key_split[2], sds_enabled) != 0)
            || (sdscmp(entry_val, sds_true) != 0))
        {
            sdsfreesplitres(entry_key_split, split_count);
            continue;
        }

        // if we have a module, we can load and initialise it

        ret = asprintf(&module_name, "%s%s", entry_key_split[0], ".so");
        if (ret < 0) {
            pmNotifyErr(LOG_ERR, "could not parse module_name");
            continue;
        }

        sdsfreesplitres(entry_key_split, split_count);
        pmNotifyErr(LOG_INFO, "loading %s", module_name);

        bpf_module = bpf_load_module(module_name);
        if (bpf_module == NULL) {
            pmNotifyErr(LOG_ERR, "could not load module (%s)", module_name);
            free(module_name);
            continue;
        }

        cluster_id = pmdaCacheStore(CACHE_CLUSTER_IDS, PMDA_CACHE_ADD, module_name, bpf_module);
        free(module_name);
        // replace module_name with a pointer to memory allocated by pmdaCacheStore
        ret = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, &module_name, NULL);
        if (ret < 0) {
            pmNotifyErr(LOG_ERR, "failed to load module name from cache: %s\n", pmErrStr(ret));
            continue;
        }

        pmNotifyErr(LOG_INFO, "initialising %s", module_name);

        // module_name is passed so that the recipient can prepend the string
        // when looking up values in the dict
        ret = bpf_module->init(cfg, module_name);
        if (ret != 0) {
            libbpf_strerror(ret, errorstring, 1023);
            pmNotifyErr(LOG_ERR, "module initialisation failed: %s, %d, %s", module_name, ret, errorstring);
            ret = pmdaCacheStore(CACHE_CLUSTER_IDS, PMDA_CACHE_HIDE, module_name, bpf_module);
            if (ret < 0) {
                pmNotifyErr(LOG_ERR, "failed to set state of pmda cache entry to hidden: %s\n", pmErrStr(ret));
            }
            continue;
        }

        pmNotifyErr(LOG_INFO, "module (%s) initialised with cluster_id = %d", module_name, cluster_id);
        module_count++;
    }

    dictReleaseIterator(iterator);
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_SAVE);
    pmNotifyErr(LOG_INFO, "loaded modules (%d)", module_count);
    sdsfree(sds_enabled);
    sdsfree(sds_true);
}

void
bpf_shutdown()
{
    int cache_op_status;
    module* bpf_module;
    char *name;

    pmNotifyErr(LOG_INFO, "shutting down");

    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_REWIND);
    cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    while(cache_op_status != -1) {
        int cluster_id = cache_op_status;
        cache_op_status = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, &name, (void**)&bpf_module);
        pmNotifyErr(LOG_INFO, "module (%s) shutting down", name);
        bpf_module->shutdown();
        cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    }

    if (pmda_config) {
        dictRelease(pmda_config);
    }

    pmNotifyErr(LOG_INFO, "shutdown complete");
}

/**
 * register metrics for all known modules
 *
 * all modules will be registered to ensure PMNS matches
 */
void
bpf_register_module_metrics()
{
    // identify how much space we need and set up metric table area
    size_t total_metrics = 0;
    size_t total_indoms = 0;
    int cache_op_status;
    module* bpf_module;
    char indom[64];
    char *name;

    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_REWIND);
    cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    while(cache_op_status != -1) {
        int cluster_id = cache_op_status;
        cache_op_status = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, NULL, (void**)&bpf_module);
        total_metrics += bpf_module->metric_count();
        total_indoms += bpf_module->indom_count();
        cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    }

    // set up indom mapping
    metrictab = total_metrics ? (pmdaMetric*) calloc(total_metrics, sizeof(pmdaMetric)) : NULL;
    indomtab = total_indoms ? (pmdaIndom*) calloc(total_indoms, sizeof(pmdaIndom)) : NULL;

    // each module needs to set up its tables, starting at the next available slot
    int current_metric = 0;
    int current_indom = 0;
    pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_REWIND);
    cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    while(cache_op_status != -1) {
        int cluster_id = cache_op_status;
        cache_op_status = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, &name, (void**)&bpf_module);

        // set up indom mapping for the module
        for(int i = 0; i < bpf_module->indom_count(); i++) {
            pmsprintf(indom, sizeof(indom), "%s/%d", name, i);
            int serial = pmdaCacheStore(CACHE_INDOM_IDS, PMDA_CACHE_ADD, indom, NULL);
            bpf_module->set_indom_serial(i, serial);
        }

        // register all of the metrics
        bpf_module->register_metrics(cluster_id, &metrictab[current_metric], &indomtab[current_indom]);

        // progress
        current_metric += bpf_module->metric_count();
        current_indom += bpf_module->indom_count();
        cache_op_status = pmdaCacheOp(CACHE_CLUSTER_IDS, PMDA_CACHE_WALK_NEXT);
    }

    metric_count = current_metric;
    indom_count = current_indom;
}

/**
 * Fetch callback for pre-refresh
 */
int
bpf_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    module* target;
    int cache_result;

    for(int i = 0; i < numpmid; i++) {
        unsigned int cluster_id = pmID_cluster(pmidlist[i]);
        unsigned int item = pmID_item(pmidlist[i]);
        cache_result = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, NULL, (void**)&target);
        if (cache_result == PMDA_CACHE_ACTIVE) {
            target->refresh(item);
        }
    }

    return pmdaFetch(numpmid, pmidlist, resp, pmda);
}

void
bpf_setup_pmns()
{
    int ret;
    char name[64];
    module* target;

    ret = pmdaTreeCreate(&pmns);
    if (ret < 0)
    {
        pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n", pmGetProgname(), pmErrStr(ret));
        pmns = NULL;
        return;
    }

    for (int i = 0; i < metric_count; i++) {
        unsigned int cluster_id = pmID_cluster(metrictab[i].m_desc.pmid);
        unsigned int item = pmID_item(metrictab[i].m_desc.pmid);

        ret = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, NULL, (void**)&target);
        if (ret == PMDA_CACHE_ACTIVE) {
            pmsprintf(name, sizeof(name), "bpf.%s", target->metric_name(item));
            pmdaTreeInsert(pmns, metrictab[i].m_desc.pmid, name);
        }
    }

    pmdaTreeRebuildHash(pmns, metric_count);
}

/**
 * Callbacks to support dynamic namespace
 */

static int
bpf_pmid(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return pmdaTreePMID(pmns, name, pmid);
}

static int
bpf_name(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    return pmdaTreeName(pmns, pmid, nameset);
}

static int
bpf_children(const char *name, int flag, char ***kids, int **sts, pmdaExt *pmda)
{
    return pmdaTreeChildren(pmns, name, flag, kids, sts);
}

static int
bpf_text(int ident, int type, char **buffer, pmdaExt *pmda)
{
    module* target;
    int cache_result;

    if (type & PM_TEXT_PMID) {
        unsigned int cluster_id = pmID_cluster(ident);
        unsigned int item = pmID_item(ident);
        cache_result = pmdaCacheLookup(CACHE_CLUSTER_IDS, cluster_id, NULL, (void**)&target);
        if (cache_result == PMDA_CACHE_ACTIVE) {
            return target->metric_text(item, type, buffer);
        }
    }

    return pmdaText(ident, type, buffer, pmda);
}

static int
dict_handler(void *arg, const char *section, const char *key, const char *value)
{
    dict	*config = (dict *)arg;
    sds		name;
    int		sts;

    name = sdscatfmt(sdsempty(), "%s.%s", section ? section : pmGetProgname(), key);
    sts = dictReplace(config, name, sdsnew(value)) != DICT_OK;
    sdsfree(name);
    return sts;
}

static dict*
bpf_config_load()
{
    char* config_filename;
    int ret;
    dict *config;

    ret = asprintf(&config_filename, "%s/bpf/bpf.conf", pmGetConfig("PCP_PMDAS_DIR"));
    if (ret <= 0) {
        pmNotifyErr(LOG_ERR, "could not construct config filename");
    } else {
        pmNotifyErr(LOG_INFO, "loading configuration: %s", config_filename);
    }

    config = dictCreate(&sdsDictCallBacks, NULL);
    if (config == NULL)
    {
        pmNotifyErr(LOG_ERR, "could not init dictionary");
        return NULL;
    }

    ret = ini_parse(config_filename, dict_handler, config);
    if (ret != 0) {
        pmNotifyErr(LOG_ERR, "could not parse config file %s, ret=%d", config_filename, ret);
        dictRelease(config);
        free(config_filename);
        return NULL;
    }

    pmNotifyErr(LOG_INFO, "loaded configuration: %s", config_filename);
    free(config_filename);

    return config;
}
/*
 * Initialise the agent (both daemon and DSO).
 */
void
bpf_init(pmdaInterface *dp)
{
    if (isDSO) {
        pmdaDSO(dp, PMDA_INTERFACE_7, "bpf", NULL);
        atexit(bpf_shutdown);
    }

    if (dp->status != 0)
        return;

    pmda_config = bpf_config_load();

    // libbpf setup
    bpf_setrlimit();
    libbpf_set_print(bpf_printfn);

    pmNotifyErr(LOG_INFO, "loading modules");
    bpf_load_modules(pmda_config);

    pmNotifyErr(LOG_INFO, "registering metrics");
    bpf_register_module_metrics();

    // set up PMDA callbacks
    pmdaSetFetchCallBack(dp, bpf_fetchCallBack);
    dp->version.any.fetch = bpf_fetch;
    dp->version.four.pmid = bpf_pmid;
    dp->version.four.name = bpf_name;
    dp->version.four.children = bpf_children;
    dp->version.four.text = bpf_text;

    pmdaInit(dp, indomtab, indom_count, metrictab, metric_count);

    pmNotifyErr(LOG_INFO, "setting up namespace");
    bpf_setup_pmns();

    pmNotifyErr(LOG_INFO, "bpf pmda init complete");
}

/*
 * Set up the agent if running as a daemon.
 */
int
main(int argc, char **argv)
{
    pmdaInterface dispatch;

    isDSO = 0;
    pmSetProgname(argv[0]);

    pmdaDaemon(&dispatch, PMDA_INTERFACE_7, pmGetProgname(), BPF,
        "bpf.log", NULL);

    pmdaGetOptions(argc, argv, &opts, &dispatch);
    if (opts.errors) {
        pmdaUsageMessage(&opts);
        exit(1);
    }

    pmdaOpenLog(&dispatch);
    pmdaConnect(&dispatch);
    bpf_init(&dispatch);
    pmdaMain(&dispatch);

    bpf_shutdown();

    exit(0);
}
