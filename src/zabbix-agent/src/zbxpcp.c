/*
 * Zabbix loadable PCP module
 *
 * Copyright (C) 2015-2016 Marko Myllynen <myllynen@redhat.com>
 * Copyright (C) 2016,2018 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * For description on Zabbix loadable modules, see:
 * https://www.zabbix.com/documentation/3.0/manual/config/items/loadablemodules
 */

/*
 * TODO
 * - config file support
 *   - conn type
 *   - conn target
 *   - replace defines
 *   - list of supported metrics
 *   - scaling
 */

#ifndef ZBX_PCP_METRIC_BASE
#define ZBX_PCP_METRIC_BASE ""
#endif

#ifndef ZBX_PCP_METRIC_PREFIX
#define ZBX_PCP_METRIC_PREFIX "pcp."
#endif

#ifndef ZBX_PCP_DERIVED_CONFIG
#define ZBX_PCP_DERIVED_CONFIG "/etc/zabbix/zbxpcp-derived-metrics.conf"
#endif

/*
 * We attempt to auto-detect the Zabbix agent version to deal
 * with ABI/ABI breakage at the Zabbix v2/v3 boundary.
 */
#define ZBX_VERSION2		2.0
#define ZBX_VERSION3		3.0
#define ZBX_VERSION3_2		3.2

/* PCP includes.  */
#include "pmapi.h"
#include "libpcp.h"
#if defined(HAVE_DLFCN_H)
#include <dlfcn.h>
#endif

/* Zabbix includes.  */
#include "module.h"

static float zbx_version = ZBX_VERSION2;

#define MAXBATCH 256

/*
 * PCP connection
 */
static int ctx = -1;

static int zbx_module_pcp_connect()
{
    /* Load possible derived metric definitions.  */
    if (access(ZBX_PCP_DERIVED_CONFIG, F_OK ) != -1)
        pmLoadDerivedConfig(ZBX_PCP_DERIVED_CONFIG);

    ctx = pmNewContext(PM_CONTEXT_HOST, "localhost");
    return ctx;
}

static int zbx_module_pcp_disconnect()
{
    return pmDestroyContext(ctx);
}

static void zbx_get_version()
{
#if defined(HAVE_DLOPEN)
    void *handle = dlopen(NULL, RTLD_NOW);

    if (!handle) {
        fprintf(stderr, "dlopen failed, assuming zabbix-agent version=%.1f\n",
                zbx_version);
        return;
    }
    /* lookup symbol that should exist in new versions, but not old versions */
    if (dlsym(handle, "history_log_cbs") != NULL)
        zbx_version = ZBX_VERSION3_2;
    else if (dlsym(handle, "zbx_user_macro_parse") != NULL)
        zbx_version = ZBX_VERSION3;
    dlclose(handle);
#else
    fprintf(stderr, "dlopen unsupported, assuming zabbix-agent version=%.1f\n",
                zbx_version);
#endif
}

/*
 * Zabbix connection
 */
int zbx_module_init()
{
    if (zbx_module_pcp_connect() < 0)
        return ZBX_MODULE_FAIL;
    return ZBX_MODULE_OK;
}

int zbx_module_api_version()
{
    zbx_get_version();
    if (zbx_version >= ZBX_VERSION3_2)
        return ZBX_MODULE_API_VERSION_TWO;
    return ZBX_MODULE_API_VERSION_ONE;
}

static int metric_count;
static ZBX_METRIC *metrics;
static void zbx_module_pcp_add_metric(const char *);
static void zbx_module_pcp_add_params(const char *);

ZBX_METRIC *zbx_module_item_list()
{
    int sts;
    ZBX_METRIC *mptr;
    static ZBX_METRIC empty[] = { {NULL} };

    /* Add PCP metrics to the Zabbix metric set.  */
    sts = pmTraversePMNS(ZBX_PCP_METRIC_BASE, zbx_module_pcp_add_metric);
    if (sts < 0 || !metric_count) { free(metrics); return empty; }

    /* Update metrics with parameters (instances).  */
    zbx_module_pcp_add_params(ZBX_PCP_METRIC_PREFIX);

    /* Finalize the Zabbix set.  */
    mptr = (ZBX_METRIC *)realloc(metrics, (metric_count + 1) * sizeof(ZBX_METRIC));
    if (mptr == NULL) { free(metrics); return empty; }
    metrics = mptr;
    metrics[metric_count].key = NULL;

    return metrics;
}

void zbx_module_item_timeout(int timeout)
{
    __pmSetRequestTimeout((double)timeout);
}

int zbx_module_uninit()
{
    if (zbx_module_pcp_disconnect() != 0)
        return ZBX_MODULE_FAIL;
    return ZBX_MODULE_OK;
}

/*
 * Zabbix/PCP connection
 */
static int zbx_module2_pcp_fetch_metric(AGENT_REQUEST *, AGENT_RESULT_V2 *);
static int zbx_module3_pcp_fetch_metric(AGENT_REQUEST *, AGENT_RESULT_V3 *);

static void zbx_module_pcp_add_metric(const char *name)
{
    char *metric;
    ZBX_METRIC *mptr;

    /* Construct the Zabbix metric name.  */
    metric = (char *)malloc(strlen(ZBX_PCP_METRIC_PREFIX) + strlen(name) + 1);
    if (metric == NULL) return;
    strcpy(metric, ZBX_PCP_METRIC_PREFIX);
    strcat(metric, name);

    /* Ready for Zabbix.  */
    mptr = (ZBX_METRIC *)realloc(metrics, (metric_count + 1) * sizeof(ZBX_METRIC));
    if (mptr == NULL) { free(metric); return; }
    metrics = mptr;
    metrics[metric_count].key = metric;
    metrics[metric_count].flags = 0;
    if (zbx_version >= ZBX_VERSION3)
	metrics[metric_count].function = (int (*)()) zbx_module3_pcp_fetch_metric;
    else
	metrics[metric_count].function = (int (*)()) zbx_module2_pcp_fetch_metric;
    metrics[metric_count].test_param = NULL;
    metric_count++;
}

static void zbx_module_pcp_add_params(const char *prefix)
{
    const int prefixlen = strlen(prefix);
    pmInDom lastindom = PM_INDOM_NULL;
    char *lastinst = NULL;
    char *names[MAXBATCH];
    pmID pmid[MAXBATCH];
    int batchsize;
    int sts, i = 0;

    /* Pick PCP metric instances for use with zabbix_agentd -p.  */
    while (i < metric_count) {
	char **namelist;
	int n, *instlist;
	pmDesc desc;

	if ((batchsize = (metric_count - i)) > MAXBATCH)
	    batchsize = MAXBATCH;

	for (n = 0; n < batchsize; n++)
	    names[n] = metrics[i+n].key + prefixlen;
	sts = pmLookupName(batchsize, (const char **)&names, pmid);
	if (sts < 0) {
	    i -= batchsize;
	    continue;
	}

	for (n = 0; n < batchsize; n++, i++) {
	    if (pmid[n] == PM_ID_NULL)
		continue;

	    sts = pmLookupDesc(pmid[n], &desc);
	    if (sts < 0) continue;

	    if (desc.indom == PM_INDOM_NULL)
		continue;

	    /* attempt to reuse the last cached indom/inst */
	    if (desc.indom == lastindom) {
		metrics[i].flags = CF_HAVEPARAMS;
		metrics[i].test_param = strdup(lastinst);
		continue;
	    }

	    sts = pmGetInDom(desc.indom, &instlist, &namelist);
	    if (sts <= 0) continue;

	    /* update the local cache */
	    if ((lastinst = strdup(namelist[0])) != NULL) {
		lastindom = desc.indom;
	        metrics[i].flags = CF_HAVEPARAMS;
	        metrics[i].test_param = lastinst;
	    } else {
		lastindom = PM_INDOM_NULL;
	    }
            free(instlist);
            free(namelist);
	}
    }
}

static int zbx_module_pcp_fetch_metric(AGENT_REQUEST *request, int *type, pmAtomValue *atom, char **errmsg)
{
    int sts;
    const char *metric[] = {request->key + strlen(ZBX_PCP_METRIC_PREFIX)};
    char *inst;
    pmID pmid[1];
    pmDesc desc[1];
    pmResult *rp;
    int iid = 0;
    int i;

    /* Parameter is the instance.  */
    switch (request->nparam) {
        case 0:
            inst = NULL;
            break;
        case 1:
            inst = get_rparam(request, 0);
            break;
        default:
            *errmsg = "Extraneous instance specification.";
            return SYSINFO_RET_FAIL;
    }

    /* Try to reconnect if the initial lookup fails.  */
    sts = pmLookupName(1, metric, pmid);
    if (sts < 0 && (sts == PM_ERR_IPC || sts == -ECONNRESET)) {
        ctx = pmReconnectContext(ctx);
        if (ctx < 0) {
            *errmsg = "Not connected to pmcd.";
            return SYSINFO_RET_FAIL;
        }
        sts = pmLookupName(1, metric, pmid);
    }

    /* Preparations and sanity checks.  */
    if (sts < 0) {
        *errmsg = "Failed metric name lookup.";
	return SYSINFO_RET_FAIL;
    }
    sts = pmLookupDesc(pmid[0], desc);
    if (sts < 0) {
        *errmsg = "Failed metric descriptor lookup.";
	return SYSINFO_RET_FAIL;
    }
    if (inst != NULL && desc[0].indom == PM_INDOM_NULL) {
        *errmsg = "Extraneous instance specification.";
        return SYSINFO_RET_FAIL;
    }
    if ((inst == NULL && desc[0].indom != PM_INDOM_NULL) ||
        (request->nparam == 1 && !strlen(inst))) {
        *errmsg = "Missing instance specification.";
        return SYSINFO_RET_FAIL;
    }
    if (desc[0].indom != PM_INDOM_NULL) {
        iid = pmLookupInDom(desc[0].indom, inst);
        if (iid < 0) {
            *errmsg = "Instance not available.";
            return SYSINFO_RET_FAIL;
        }
    }

    /* Fetch the metric values.  */
    sts = pmFetch(1, pmid, &rp);
    if (sts < 0) {
	*errmsg = "Failed to sample metric values.";
	return SYSINFO_RET_FAIL;
    }
    if (rp->vset[0]->numval < 1) {
        pmFreeResult(rp);
        *errmsg = "No value available.";
        return SYSINFO_RET_FAIL;
    }

    /* Locate the correct instance.  */
    for (i = 0; desc[0].indom != PM_INDOM_NULL && i < rp->vset[0]->numval; i++)
        if (rp->vset[0]->vlist[i].inst == iid)
            break;
    if (i == rp->vset[0]->numval) {
	*errmsg = "Failed to locate metric instance.";
        pmFreeResult(rp);
        return SYSINFO_RET_FAIL;
    }

    /* Extract the wanted value.  */
    sts = pmExtractValue(rp->vset[0]->valfmt, &rp->vset[0]->vlist[i],
                         desc[0].type, atom, desc[0].type);
    pmFreeResult(rp);
    if (sts < 0) {
	*errmsg = "Failed to extract metric value.";
	return SYSINFO_RET_FAIL;
    }
    *type = desc[0].type;

    /* Success.  */
    return SYSINFO_RET_OK;
}

static int zbx_module3_pcp_fetch_metric(AGENT_REQUEST *request, AGENT_RESULT_V3 *result)
{
    char *errmsg = NULL;
    pmAtomValue atom;
    int type;
    int sts;

    /* note: SET_*_RESULT macros evaluate to different code for v2/v3 */
    sts = zbx_module_pcp_fetch_metric(request, &type, &atom, &errmsg);
    if (sts != SYSINFO_RET_OK) {
        if (errmsg)
            SET_MSG_RESULT(result, strdup(errmsg));
        return sts;
    }
    switch (type) {
        case PM_TYPE_32:
            SET_UI64_RESULT(result, atom.l);
            break;
        case PM_TYPE_U32:
            SET_UI64_RESULT(result, atom.ul);
            break;
        case PM_TYPE_64:
            SET_UI64_RESULT(result, atom.ll);
            break;
        case PM_TYPE_U64:
            SET_UI64_RESULT(result, atom.ull);
            break;
        case PM_TYPE_FLOAT:
            SET_DBL_RESULT(result, atom.f);
            break;
        case PM_TYPE_DOUBLE:
            SET_DBL_RESULT(result, atom.d);
            break;
        case PM_TYPE_STRING:
            SET_STR_RESULT(result, strdup(atom.cp));
            break;
        default:
            SET_MSG_RESULT(result, strdup("Unsupported metric value type."));
            sts = SYSINFO_RET_FAIL;
            break;
    }

    return sts;
}

static int zbx_module2_pcp_fetch_metric(AGENT_REQUEST *request, AGENT_RESULT_V2 *result)
{
    char *errmsg = NULL;
    pmAtomValue atom;
    int type;
    int sts;

    /* note: SET_*_RESULT macros evaluate to different code for v2/v3 */
    sts = zbx_module_pcp_fetch_metric(request, &type, &atom, &errmsg);
    if (sts != SYSINFO_RET_OK) {
        if (errmsg)
            SET_MSG_RESULT(result, strdup(errmsg));
        return sts;
    }
    switch (type) {
        case PM_TYPE_32:
            SET_UI64_RESULT(result, atom.l);
            break;
        case PM_TYPE_U32:
            SET_UI64_RESULT(result, atom.ul);
            break;
        case PM_TYPE_64:
            SET_UI64_RESULT(result, atom.ll);
            break;
        case PM_TYPE_U64:
            SET_UI64_RESULT(result, atom.ull);
            break;
        case PM_TYPE_FLOAT:
            SET_DBL_RESULT(result, atom.f);
            break;
        case PM_TYPE_DOUBLE:
            SET_DBL_RESULT(result, atom.d);
            break;
        case PM_TYPE_STRING:
            SET_STR_RESULT(result, strdup(atom.cp));
            break;
        default:
            SET_MSG_RESULT(result, strdup("Unsupported metric value type."));
            sts = SYSINFO_RET_FAIL;
            break;
    }
    return sts;
}
