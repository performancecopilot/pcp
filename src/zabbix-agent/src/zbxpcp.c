/*
 * Zabbix loadable PCP module
 *
 * Copyright (C) 2015 Marko Myllynen <myllynen@redhat.com>
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
 * - derived metrics support
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

/* PCP includes.  */
#include "pmapi.h"
#include "impl.h"

/* Zabbix includes.  */
#include "module.h"

/*
 * PCP connection
 */
static int ctx = -1;

int zbx_module_pcp_init()
{
    ctx = pmNewContext(PM_CONTEXT_HOST, "localhost");
    return ctx;
}

int zbx_module_pcp_uninit()
{
    return pmDestroyContext(ctx);
}

/*
 * Zabbix connection
 */
int zbx_module_init()
{
    if (zbx_module_pcp_init() < 0)
        return ZBX_MODULE_FAIL;
    return ZBX_MODULE_OK;
}

int zbx_module_api_version()
{
    return ZBX_MODULE_API_VERSION_ONE;
}

static int metric_count = 0;
static ZBX_METRIC *metrics = NULL;
void zbx_module_pcp_add_metric(const char *name);

ZBX_METRIC *zbx_module_item_list()
{
    int sts;
    ZBX_METRIC *mptr;
    static ZBX_METRIC empty[] = { {NULL} };

    /* Add PCP metrics to the Zabbix metric set.  */
    sts = pmTraversePMNS(ZBX_PCP_METRIC_BASE, zbx_module_pcp_add_metric);
    if (sts < 0 || !metric_count) { free(metrics); return empty; }

    /* Finalize the Zabbix set.  */
    mptr = metrics;
    metrics = (ZBX_METRIC *)realloc(mptr, (metric_count + 1) * sizeof(ZBX_METRIC));
    if (metrics == NULL) { free(mptr); return empty; }
    metrics[metric_count].key = NULL;

    return metrics;
}

void zbx_module_item_timeout(int timeout)
{
    __pmSetRequestTimeout((double)timeout);
}

int zbx_module_uninit()
{
    if (zbx_module_pcp_uninit() != 0)
        return ZBX_MODULE_FAIL;
    return ZBX_MODULE_OK;
}

/*
 * Zabbix/PCP connection
 */
int zbx_module_pcp_fetch_metric(AGENT_REQUEST *request, AGENT_RESULT *result);

void zbx_module_pcp_add_metric(const char *name)
{
    int sts;
    pmID pmid[1];
    pmDesc desc[1];
    char *metric;
    unsigned flags = 0;
    char *param = NULL;
    int *instlist;
    char **namelist;
    ZBX_METRIC *mptr = metrics;

    /* PCP preparations.  */
    sts = pmLookupName(1, (char **)&name, pmid);
    if (sts < 0) return;
    sts = pmLookupDesc(pmid[0], desc);
    if (sts < 0) return;

    /* Construct the Zabbix metric name.  */
    metric = (char *)malloc(strlen(ZBX_PCP_METRIC_PREFIX) + strlen(name) + 1);
    if (metric == NULL) return;
    strcpy(metric, ZBX_PCP_METRIC_PREFIX);
    strcat(metric, name);

    /* Pick a PCP metric instance for use with zabbix_agentd -p.  */
    if (desc[0].indom != PM_INDOM_NULL) {
        sts = pmGetInDom(desc[0].indom, &instlist, &namelist);
        if (sts < 0) { free(metric); return; }
        if (sts) {
            flags = CF_HAVEPARAMS;
            param = strdup(namelist[0]);
            free(instlist);
            free(namelist);
        }
    }

    /* Ready for Zabbix.  */
    metrics = (ZBX_METRIC *)realloc(mptr, (metric_count + 1) * sizeof(ZBX_METRIC));
    if (metrics == NULL) { metrics = mptr; free(metric); free(param); return; }
    metrics[metric_count].key = metric;
    metrics[metric_count].flags = flags;
    metrics[metric_count].function = zbx_module_pcp_fetch_metric;
    metrics[metric_count].test_param = param;
    metric_count++;
}

int zbx_module_pcp_fetch_metric(AGENT_REQUEST *request, AGENT_RESULT *result)
{
    int sts;
    char *metric[] = { request->key + strlen(ZBX_PCP_METRIC_PREFIX) };
    char *inst;
    pmID pmid[1];
    pmDesc desc[1];
    pmResult *rp;
    pmAtomValue atom;
    int iid = 0;
    int i;

    /* Parameter is the instance.  */
    switch(request->nparam) {
        case 0:
            inst = NULL;
            break;
        case 1:
            inst = get_rparam(request, 0);
            break;
        default:
            SET_MSG_RESULT(result, strdup("Extraneous instance specification."));
            return SYSINFO_RET_FAIL;
            break;
    }

    /* Preparations and sanity checks.  */
    sts = pmLookupName(1, metric, pmid);
    if (sts < 0) return SYSINFO_RET_FAIL;
    sts = pmLookupDesc(pmid[0], desc);
    if (sts < 0) return SYSINFO_RET_FAIL;
    if (inst != NULL && desc[0].indom == PM_INDOM_NULL) {
        SET_MSG_RESULT(result, strdup("Extraneous instance specification."));
        return SYSINFO_RET_FAIL;
    }
    if ((inst == NULL && desc[0].indom != PM_INDOM_NULL) ||
        (request->nparam == 1 && !strlen(inst))) {
        SET_MSG_RESULT(result, strdup("Missing instance specification."));
        return SYSINFO_RET_FAIL;
    }
    if (desc[0].indom != PM_INDOM_NULL) {
        iid = pmLookupInDom(desc[0].indom, inst);
        if (iid < 0) {
            SET_MSG_RESULT(result, strdup("Instance not available."));
            return SYSINFO_RET_FAIL;
        }
    }

    /* Fetch the metric values.  */
    sts = pmFetch(1, pmid, &rp);
    if (sts < 0) return SYSINFO_RET_FAIL;
    if (rp->vset[0]->numval < 1) {
        pmFreeResult(rp);
        SET_MSG_RESULT(result, strdup("No value available."));
        return SYSINFO_RET_FAIL;
    }

    /* Locate the correct instance.  */
    for (i = 0; desc[0].indom != PM_INDOM_NULL && i < rp->vset[0]->numval; i++) {
        if (rp->vset[0]->vlist[i].inst == iid) {
            break;
        }
    }
    if (i == rp->vset[0]->numval) {
        pmFreeResult(rp);
        return SYSINFO_RET_FAIL;
    }

    /* Extract the wanted value.  */
    sts = pmExtractValue(rp->vset[0]->valfmt, &rp->vset[0]->vlist[i],
                         desc[0].type, &atom, desc[0].type);
    pmFreeResult(rp);
    if (sts < 0) return SYSINFO_RET_FAIL;

    /* Hand it to the caller.  */
    switch(desc[0].type) {
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
            return SYSINFO_RET_FAIL;
            break;
    }

    /* Success.  */
    return SYSINFO_RET_OK;
}
