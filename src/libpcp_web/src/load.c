/*
 * Copyright (c) 2017-2018 Red Hat.
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

#include <math.h>
#include <stdarg.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>

#include "series.h"
#include "query.h"
#include "schema.h"
#include "load.h"
#include "util.h"
#include "slots.h"

#include "libpcp.h"

typedef struct {
    redisSlots		*redis;

    settings_t		*settings;
    void		*arg;

    int			verbose;
    context_t		context;

    __pmHashCtl		clusterhash;
    __pmHashCtl		domainhash;
    __pmHashCtl		indomhash;
    __pmHashCtl		pmidhash;

    __pmHashCtl		errorhash;	/* PMIDs where errors observed */
    __pmHashCtl		wanthash;	/* PMIDs from query whitelist */
} SOURCE;

#define loadfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define loadmsg(SP, level, message)	\
	((SP)->settings->on_info((level), (message), (SP)->arg), sdsfree(msg))

static void
server_cache_source(SOURCE *sp)
{
    redis_series_source(sp->redis, &sp->context);
}

static void
server_cache_metric(SOURCE *sp, metric_t *metric)
{
    redis_series_metric(sp->redis, &sp->context, metric);
}

static void
server_cache_stream(SOURCE *sp, sds timestamp, metric_t *metric)
{
    redis_series_stream(sp->redis, timestamp, metric);
}

static void
server_cache_mark(SOURCE *sp, sds timestamp)
{
    redis_series_mark(sp->redis, &sp->context, timestamp);
}

static void
pmiderr(SOURCE *sp, pmID pmid, const char *msg, ...)
{
    va_list	arg;
    int		numnames;
    char	**names;

    if (sp->verbose == 0)
	return;

    if (__pmHashSearch(pmid, &sp->errorhash) == NULL) {
	numnames = pmNameAll(pmid, &names);
	fprintf(stderr, "%s: ", pmGetProgname());
	__pmPrintMetricNames(stderr, numnames, names, " or ");
	fprintf(stderr, "(%s) - ", pmIDStr(pmid));
	va_start(arg, msg);
	vfprintf(stderr, msg, arg);
	va_end(arg);
	__pmHashAdd(pmid, NULL, &sp->errorhash);
	if (numnames > 0) free(names);
    }
}

static void
cache_prepare(const char *name, void *arg)
{
    SOURCE		*sp = (SOURCE *)arg;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		*hname;
    pmID		pmid;
    sds			msg;
    int			sts;

    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	loadfmt(msg, "failed to lookup metric name (pmid=%s): %s",
		name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    } else if ((hname = strdup(name)) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"cache metric name", (__int64_t)strlen(name)+1);
	loadmsg(sp, PMLOG_ERROR, msg);
    } else {
	if (sp->verbose || pmDebugOptions.series)
	    fprintf(stderr, "cache_prepare: caching PMID=%s name=%s\n",
			pmIDStr(pmid), hname);
	__pmHashAdd(pmid, hname, &sp->wanthash);
    }
}

static int
labelsetlen(pmLabelSet *lp)
{
    if (lp->nlabels <= 0)
	return 0;
    return sizeof(pmLabelSet) + lp->jsonlen + (lp->nlabels * sizeof(pmLabel));
}

static pmLabelSet *
labelsetdup(pmLabelSet *lp)
{
    pmLabelSet		*dup;
    char		*json;

    if ((dup = calloc(1, sizeof(pmLabelSet))) == NULL)
	return NULL;
    *dup = *lp;
    if (lp->nlabels <= 0)
	return dup;
    if ((json = strdup(lp->json)) == NULL) {
	free(dup);
	return NULL;
    }
    if ((dup->labels = calloc(lp->nlabels, sizeof(pmLabel))) == NULL) {
	free(dup);
	free(json);
	return NULL;
    }
    memcpy(dup->labels, lp->labels, sizeof(pmLabel) * lp->nlabels);
    dup->json = json;
    return dup;
}

static int
new_instance(SOURCE	*sp,
	struct metric	*metric,	/* updated by this function */
	int		inst,
	int		index)
{
    instlist_t		*instlist;
    size_t		size;
    sds			msg;
    unsigned int	i;

    if (metric->u.inst == NULL) {
	assert(index == 0);
	size = sizeof(instlist_t) + sizeof(value_t);
	if ((instlist = calloc(1, size)) == NULL) {
	    loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new instlist", (__int64_t)size);
	    loadmsg(sp, PMLOG_ERROR, msg);
	    return -ENOMEM;
	}
	instlist->listcount = instlist->listsize = 1;
	instlist->value[0].inst = inst;
	metric->u.inst = instlist;
	return 0;
    }

    instlist = metric->u.inst;
    assert(instlist->listcount <= instlist->listsize);

    if (index >= instlist->listsize) {
	size = instlist->listsize * 2;
	assert(index < size);
	size = sizeof(instlist_t) + (size * sizeof(value_t));
	if ((instlist = (instlist_t *)realloc(instlist, size)) == NULL) {
	    loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"grew instlist", (__int64_t)size);
	    loadmsg(sp, PMLOG_ERROR, msg);
	    return -ENOMEM;
	}
	instlist->listsize *= 2;
	for (i = instlist->listcount; i < instlist->listsize; i++) {
	    memset(&instlist->value[i], 0, sizeof(value_t));
	}
    }

    i = instlist->listcount++;
    instlist->value[i].inst = inst;
    metric->u.inst = instlist;
    return 0;
}

/*
 * Iterate over each value associated with this metric, and complete
 * the metadata (instance name, labels) associated with each.
 */
static void
update_instance_metadata(SOURCE *sp, metric_t *metric,
	int ninst, int *instlist, char **namelist,
	int nsets, pmLabelSet *labelsets)
{
    pmDesc		*desc = &metric->desc;
    pmLabelSet		*labels;
    value_t		*value;
    size_t		length;
    char		*name;
    sds			msg, inst;
    int			i, j;

    if (metric->u.inst == NULL)
	for (i = 0; i < ninst; i++)
	    new_instance(sp, metric, instlist[i], i);

    for (i = 0; i < metric->u.inst->listcount; i++) {
	value = &metric->u.inst->value[i];

	if (value->cached)
	    continue;

	for (j = 0; j < ninst; j++) {
	    if (value->inst != instlist[j])
		continue;
	    name = namelist[j];
	    length = strlen(name) + 1;
	    if ((name = strndup(name, length)) == NULL) {
	        loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata name", (__int64_t)length);
		loadmsg(sp, PMLOG_ERROR, msg);
		continue;
	    }
	    if (value->name)
		free(value->name);
	    value->name = name;
	    break;
	}

	for (j = 0; j < nsets; j++) {
	    if (value->inst != labelsets[j].inst)
		continue;
	    labels = &labelsets[j];
	    length = labelsetlen(labels);
	    if (length == 0)
		continue;
	    if ((labels = labelsetdup(labels)) == NULL) {
		loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata labels", (__int64_t)length);
		loadmsg(sp, PMLOG_ERROR, msg);
		continue;
	    }
	    if (value->labels)
		pmFreeLabelSets(value->labels, 1);
	    value->labels = labels;
	    break;
	}

	inst = json_escaped_str(value->name);
	instance_hash(metric, value, inst, desc);

	if (sp->verbose || pmDebugOptions.series) {
	    fprintf(stderr, "Cache insert - instance: %s", value->name);
	    fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(value->hash));
	}
	value->cached = 1;
    }
}

/*
 * Iterate over the set of metric values and extract names and labels
 * for each instance.  Finally cache metadata once its all available.
 */
static void
cache_metric_metadata(SOURCE *sp, metric_t *mp)
{
    pmLabelSet		*labelset = NULL;
    pmInDom		indom = mp->desc.indom;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		indommsg[20];
    char		**namelist = NULL;
    int			*instlist = NULL;
    sds			msg;
    int			sts;
    int			ninst = 0;
    int			nsets = 0;

    if (indom != PM_INDOM_NULL) {
	if ((sts = pmGetInDom(indom, &instlist, &namelist)) < 0) {
	    loadfmt(msg, "failed to get InDom %s instances: %s",
			pmInDomStr_r(indom, indommsg, sizeof(indommsg)),
			pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	    loadmsg(sp, PMLOG_ERROR, msg);
	    return;
	}
	ninst = sts;
	if ((sts = pmGetInstancesLabels(indom, &labelset)) < 0) {
	    if (sp->verbose || pmDebugOptions.series)
		fprintf(stderr, "%s: failed to get PMID %s labels: %s\n",
			pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	    /* continue on with no labels for this value */
	    sts = 0;
	}
	nsets = sts;

	update_instance_metadata(sp, mp, ninst, instlist, namelist, nsets, labelset);
    }

    /* insert metadata into the cache for this metric and all its instances */
    server_cache_metric(sp, mp);

    if (labelset) pmFreeLabelSets(labelset, nsets);
    if (namelist) free(namelist);
    if (instlist) free(instlist);
}

static domain_t *
new_domain(SOURCE *sp, int domain, context_t *context)
{
    domain_t		*domainp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((domainp = calloc(1, sizeof(domain_t))) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new domain", (__int64_t)sizeof(domain_t));
	loadmsg(sp, PMLOG_ERROR, msg);
	return NULL;
    }
    domainp->domain = domain;
    domainp->context = context;
    if ((sts = pmGetDomainLabels(domain, &domainp->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr, "%s: failed to get domain (%d) labels: %s\n",
		    pmGetProgname(), domain, pmErrStr(sts));
	/* continue on with no labels for this domain */
    }
    if (__pmHashAdd(domain, (void *)domainp, &sp->domainhash) < 0) {
	loadfmt(msg, "failed to store domain labels (domain=%d): %s",
		domain, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    }
    return domainp;
}

static cluster_t *
new_cluster(SOURCE *sp, int cluster, domain_t *domain)
{
    cluster_t		*clusterp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((clusterp = calloc(1, sizeof(cluster_t))) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new cluster", (__int64_t)sizeof(cluster_t));
	loadmsg(sp, PMLOG_ERROR, msg);
	return NULL;
    }
    clusterp->cluster = cluster;
    clusterp->domain = domain;
    if ((sts = pmGetClusterLabels(cluster, &clusterp->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr,
		    "%s: failed to get cluster (%u.%u) labels: %s\n",
		    pmGetProgname(), pmID_domain(cluster), pmID_cluster(cluster),
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	/* continue on with no labels for this cluster */
    }
    if (__pmHashAdd(cluster, (void *)clusterp, &sp->clusterhash) < 0) {
	loadfmt(msg, "failed to store cluster labels (cluster=%u.%u): %s",
		pmID_domain(cluster), pmID_cluster(cluster),
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    }
    return clusterp;
}

static indom_t *
new_indom(SOURCE *sp, pmInDom indom, domain_t *domain)
{
    indom_t		*indomp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((indomp = calloc(1, sizeof(indom_t))) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new indom", (__int64_t)sizeof(indom_t));
	loadmsg(sp, PMLOG_ERROR, msg);
	return NULL;
    }
    indomp->indom = indom;
    indomp->domain = domain;
    if ((sts = pmGetInDomLabels(indom, &indomp->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr, "%s: failed to get indom (%s) labels: %s\n",
		    pmGetProgname(), pmInDomStr(indom),
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	/* continue on with no labels for this indom */
    }
    if (__pmHashAdd(indom, (void *)indomp, &sp->indomhash) < 0) {
	loadfmt(msg, "failed to store indom (%s) labels: %s",
		pmInDomStr(indom), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    }
    return indomp;
}

static metric_t *
new_metric(SOURCE	*sp,
	pmValueSet	*vsp)
{
    __pmHashNode	*hptr;
    long long		*mapids;
    cluster_t		*cp;
    domain_t		*dp;
    indom_t		*ip;
    metric_t		*metric;
    pmDesc		desc;
    pmID		pmid = vsp->pmid;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		**names = NULL;
    sds			msg;
    int			cluster, domain, sts;

    if ((metric = (metric_t *)calloc(1, sizeof(metric_t))) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new metric", (__int64_t)sizeof(metric_t));
	loadmsg(sp, PMLOG_ERROR, msg);
	return NULL;
    }

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	loadfmt(msg, "failed to lookup metric %s descriptor: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    } else if ((sts = pmNameAll(pmid, &names)) < 0) {
	loadfmt(msg, "failed to lookup metric %s names: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
    } else if ((mapids = calloc(sts, sizeof(__int64_t))) == NULL) {
	loadfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"mapids", (__int64_t)sts * sizeof(__int64_t));
	loadmsg(sp, PMLOG_ERROR, msg);
	sts = -ENOMEM;
    }
    if (sts <= 0) {
	if (names)
	    free(names);
	free(metric);
	return NULL;
    }

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Metric [%s] ", pmIDStr_r(pmid, pmmsg, sizeof(pmmsg)));
	__pmPrintMetricNames(stderr, sts, names, " or ");
    }

    /* pick out domain#, indom# and cluster# and update label caches */
    domain = pmID_domain(pmid);
    if ((hptr = __pmHashSearch(domain, &sp->domainhash)) != NULL)
	dp = (domain_t *)hptr->data;
    else
	dp = new_domain(sp, domain, &sp->context);
    cluster = pmID_build(domain, pmID_cluster(pmid), 0);
    if ((hptr = __pmHashSearch(cluster, &sp->clusterhash)) != NULL)
	cp = (cluster_t *)hptr->data;
    else
	cp = new_cluster(sp, cluster, dp);
    if (desc.indom == PM_INDOM_NULL)
	ip = NULL;
    else if ((hptr = __pmHashSearch(desc.indom, &sp->indomhash)) != NULL)
	ip = (indom_t *)hptr->data;
    else
	ip = new_indom(sp, desc.indom, dp);

    metric->cluster = cp;
    metric->indom = ip;
    metric->desc = desc;
    metric->mapids = mapids;
    metric->names = names;
    metric->numnames = sts;

    if ((sts = pmGetItemLabels(pmid, &metric->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr, "%s: failed to get metric %s labels: %s\n",
		    pmGetProgname(), pmIDStr(pmid), pmErrStr(sts));
	/* continue on without item labels for this PMID */
    }

    metric_hash(metric, &desc);

    if (sp->verbose || pmDebugOptions.series) {
	fprintf(stderr, "Cache insert - name(s): ");
	__pmPrintMetricNames(stderr, metric->numnames, metric->names, " or ");
	fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(metric->hash));
    }

    return metric;
}

static void
free_metric(metric_t *metric)
{
    value_t		*value;
    int			i;

    if (metric->desc.indom != PM_INDOM_NULL && metric->u.inst != NULL) {
	for (i = 0; i < metric->u.inst->listsize; i++) {
	    value = &metric->u.inst->value[i];
	    if (value->name)
		free(value->name);
	}
	free(metric->u.inst);
    }
    if (metric->mapids)
	free(metric->mapids);
    if (metric->names)
	free(metric->names);
    free(metric);
}

static void
clear_metric_updated(metric_t *metric)
{
    int			i, count;

    metric->updated = 0;
    if (metric->desc.indom == PM_INDOM_NULL || metric->u.inst == NULL)
	return;
    count = metric->u.inst->listcount;
    for (i = 0; i < count; i++)
	metric->u.inst->value[i].updated = 0;
}

static int
new_instances(SOURCE *sp, metric_t *metric, pmValueSet *vsp, pmflags flags)
{
    pmAtomValue		*avp;
    pmValue		*vp;
    int			j, k, type, count = 0;

    if (vsp->numval <= 0)
	return 0;

    type = metric->desc.type;
    if (metric->desc.indom == PM_INDOM_NULL) {
	if (!(flags & PMFLAG_METADATA)) {	/* not metadata only */
	    vp = &vsp->vlist[0];
	    avp = &metric->u.atom;
	    pmExtractValue(vsp->valfmt, vp, type, avp, type);
	}
	return 0;
    }

    for (j = 0; j < vsp->numval; j++) {
	vp = &vsp->vlist[j];

	if (metric->u.inst == NULL) {
	    new_instance(sp, metric, vp->inst, j);
	    count++;
	    k = 0;
	}
	else if (j >= metric->u.inst->listcount) {
	    new_instance(sp, metric, vp->inst, j);
	    count++;
	    k = j;
	}
	else if (vp->inst != metric->u.inst->value[j].inst) {
	    for (k = 0; k < metric->u.inst->listcount; k++) {
		if (vp->inst == metric->u.inst->value[k].inst)
		    break;	/* k is now the correct offset */
	    }
	    if (k == metric->u.inst->listcount) {    /* no matching instance */
		new_instance(sp, metric, vp->inst, k);
		count++;
	    }
	} else {
	    k = j;		/* successful direct mapping */
	}
	if (flags & PMFLAG_METADATA)	/* metadata only */
	    continue;
	avp = &metric->u.inst->value[k].atom;
	pmExtractValue(vsp->valfmt, vp, type, avp, type);
    }

    metric->u.inst->listcount = vsp->numval;

    return count;
}

static void
series_cache_update(SOURCE *sp, pmResult *result, pmflags flags)
{
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    metric_t		*metric = NULL;
    sds			timestamp;
    int			i, refresh;

    timestamp = sdsnew(timeval_str(&result->timestamp));

    if (result->numpmid == 0) {
	if (!(flags & PMFLAG_METADATA))	/* not metadata only */
	    server_cache_mark(sp, timestamp);
	sdsfree(timestamp);
	return;
    }

    pmSortInstances(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;

	/* check if in the restricted group (command line optional) */
	if (sp->wanthash.nodes &&
	    __pmHashSearch(vsp->pmid, &sp->wanthash) == NULL)
	    continue;

	/* check if pmid already in hash list */
	if ((hptr = __pmHashSearch(vsp->pmid, &sp->pmidhash)) == NULL) {

	    /* create a new one & add to hash */
	    if ((metric = new_metric(sp, vsp)) == NULL)
		continue;

	    if (__pmHashAdd(vsp->pmid, (void *)metric, &sp->pmidhash) < 0) {
		pmiderr(sp, vsp->pmid, "failed hash table insertion\n",
			pmGetProgname());
		/* free memory allocated above on insert failure */
		free_metric(metric);
		continue;
	    }

	    refresh = 1;
	} else {	/* pmid exists */
	    metric = (metric_t *)hptr->data;
	    clear_metric_updated(metric);
	    refresh = 0;
	}

	/* iterate through result instances and ensure metric_t is complete */
	if (metric->error == 0 && vsp->numval < 0)
	    refresh = 1;
	if (new_instances(sp, metric, vsp, flags) != 0)
	    refresh = 1;
	if (refresh)
	    cache_metric_metadata(sp, metric);

	/* record the error code in the cache */
	metric->error = (vsp->numval < 0) ? vsp->numval : 0;

	if (flags & PMFLAG_METADATA)	/* metadata only */
	    continue;

	/* push values for all instances, no values or error into the cache */
	server_cache_stream(sp, timestamp, metric);
    }
    sdsfree(timestamp);
}

static int
series_cache_load(SOURCE *sp, timing_t *tp, pmflags flags)
{
    struct timeval	*finish = &tp->end;
    pmResult		*result;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts, count = 0;

    if ((sts = pmSetMode(PM_MODE_FORW, &tp->start, 0)) < 0) {
	loadfmt(msg, "pmSetMode failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_ERROR, msg);
	return sts;
    }

    /* TODO: in metadata only loading mode, there's no need to use pmFetch */
    /* TODO: support a tail-mode of operation - need pmDiscoverArchives(3) */
    /* TODO: support a live series loading mode of operation */
    if (sp->context.type != PM_CONTEXT_ARCHIVE)
	return -ENOTSUP;

    for ( ; ; ) {
	if ((sts = pmFetchArchive(&result)) < 0)
	    break;

	if (finish->tv_sec > result->timestamp.tv_sec ||
	    (finish->tv_sec == result->timestamp.tv_sec &&
	     finish->tv_usec >= result->timestamp.tv_usec)) {
	    series_cache_update(sp, result, flags);
	    pmFreeResult(result);
	    count++;
	}
	else {
	    pmFreeResult(result);
	    sts = PM_ERR_EOL;
	    break;
	}
    }

    loadfmt(msg, "processed %d archive records from %s", count,
		sp->context.name);
    loadmsg(sp, PMLOG_INFO, msg);

    if (sts == PM_ERR_EOL)
	sts = 0;
    else {
	loadfmt(msg, "fetch failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_ERROR, msg);
	sts = 1;
    }
    return sts;
}

static void
set_context_source(SOURCE *sp, const char *source)
{
    sp->context.name = sdsnew(source);
}

static void
set_context_type(SOURCE *sp, const char *name)
{
    if (strcmp(name, "path") == 0 ||
	strcmp(name, "archive") == 0 ||
	strcmp(name, "directory") == 0) {
	sp->context.type = PM_CONTEXT_ARCHIVE;
	return;
    }
    if (strcmp(name, "host") == 0 ||
	strcmp(name, "hostname") == 0 ||
	strcmp(name, "hostspec") == 0)
	sp->context.type = PM_CONTEXT_HOST;
}

static int
add_source_metric(SOURCE *sp, const char *metric)
{
    int		count = sp->context.nmetrics;
    int		length = (count + 1) * sizeof(char *);
    const char	**metrics;

    if ((metrics = (const char **)realloc(sp->context.metrics, length)) == NULL)
	return -ENOMEM;
    metrics[count++] = metric;
    sp->context.metrics = metrics;
    sp->context.nmetrics = count;
    return 0;
}

static int
load_prepare_metrics(SOURCE *sp)
{
    const char	**metrics = sp->context.metrics;
    char	pmmsg[PM_MAXERRMSGLEN];
    sds		msg;
    int		i, sts, errors = 0;

    for (i = 0; i < sp->context.nmetrics; i++) {
	if ((sts = pmTraversePMNS_r(metrics[i], cache_prepare, sp)) >= 0)
	    continue;
	loadfmt(msg, "PMNS traversal failed for %s: %s",
			metrics[i], pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_WARNING, msg);
	errors++;
    }
    return (errors && errors == sp->context.nmetrics) ? -ESRCH : 0;
}

static int
load_prepare_timing(SOURCE *sp, timing_t *tp, pmflags flags)
{
    struct timeval	*finish = &tp->end;
    struct timeval	*start = &tp->start;

    /*
     * If no time window given,
     * - in general archive mode, scan the entire log
     * - in live-updating mode, scan back from 12 hours ago
     */
    if (finish->tv_sec == 0)
	finish->tv_sec = time(NULL);
    if (start->tv_sec == 0 && (flags & PMFLAG_ACTIVE))
	start->tv_sec = finish->tv_sec - (12 * 60 * 60);

    /* TODO - handle timezones and so on correctly */

    return 0;
}

static void
load_prepare_source(SOURCE *sp, node_t *np, int level)
{
    const char	*name;

    if (np == NULL)
	return;

    /* descend to the leaves first */
    load_prepare_source(sp, np->left, level+1);
    load_prepare_source(sp, np->right, level+1);

    switch (np->type) {
    case N_STRING:
    case N_NAME:
	if ((name = series_instance_name(np->value)) != NULL)
	    np->subtype = N_INSTANCE;
	else if ((name = series_context_name(np->value)) != NULL)
	    np->subtype = N_CONTEXT;
	else if ((name = series_metric_name(np->value)) != NULL)
	    np->subtype = N_METRIC;
	else {
	    if ((name = series_label_name(np->value)) == NULL)
		name = np->value;
	    np->subtype = N_LABEL;
	}
	set_context_type(sp, name);
	break;

    case N_EQ:
    case N_GLOB:
	if (np->right->type != N_STRING)
	    break;
	if (np->left->type == N_NAME || np->left->type == N_STRING) {
	    if (np->left->subtype == N_CONTEXT)
		 set_context_source(sp, np->right->value);
	}
	if (np->left->type == N_METRIC)
	    add_source_metric(sp, np->right->value);
	break;

    default:
	break;
    }
}

static void
destroy_context(SOURCE *sp)
{
    context_t		*cp = &sp->context;

    pmDestroyContext(cp->context);
    cp->context = -1;

    if (cp->name)
	sdsfree(cp->name);
    cp->name = NULL;
    if (cp->origin)
	sdsfree(cp->origin);
    cp->origin = NULL;
}

static int
new_context(SOURCE *sp)
{
    context_t		*cp = &sp->context;
    char		labels[PM_MAXLABELJSONLEN];
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    /* establish PMAPI context */
    if ((sts = pmNewContext(cp->type, cp->name)) < 0) {
	if (cp->type == PM_CONTEXT_HOST)
	    loadfmt(msg, "cannot connect to PMCD on host \"%s\": %s",
		    cp->name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else if (sp->context.type == PM_CONTEXT_LOCAL)
	    loadfmt(msg, "cannot make standalone connection on localhost: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else
	    loadfmt(msg, "cannot open archive \"%s\": %s",
		    cp->name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMLOG_ERROR, msg);
	return -ESRCH;
    }
    cp->context = sts;

    /* extract unique identification information */
    if ((sts = pmwebapi_source_meta(cp, labels, sizeof(labels))) < 0) {
	loadfmt(msg, "failed to get context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	goto fail;
    }
    if ((sts = pmwebapi_source_hash(cp->hash, labels, sts)) < 0) {
	loadfmt(msg, "failed to merge context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	goto fail;
    }

    if (sp->verbose || pmDebugOptions.series) {
	fprintf(stderr, "Cache insert context - name: %s", cp->name);
	fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(cp->hash));
    }
    return 0;

fail:
    loadmsg(sp, PMLOG_ERROR, msg);
    destroy_context(sp);
    return -ESRCH;
}

static void
set_source_origin(context_t *cp)
{
    char		host[MAXHOSTNAMELEN];
    char		path[MAXPATHLEN];
    size_t		bytes;

    if (cp->type == PM_CONTEXT_ARCHIVE) {
	if (realpath(cp->name, path) != NULL)
	    bytes = strlen(path);
	else
	    bytes = pmsprintf(path, sizeof(path), "%s", cp->name);
	cp->name = sdscpylen(cp->name, path, bytes);
    }

    if ((gethostname(host, sizeof(host))) == 0)
	bytes = strlen(host);
    else
	bytes = pmsprintf(host, sizeof(host), "localhost");
    cp->origin = sdsnewlen(host, bytes);
}

int
series_source(pmSeriesSettings *settings,
	node_t *root, timing_t *timing, pmflags flags, void *arg)
{
    SOURCE	source = { .settings = settings, .arg = arg };
    sds		msg;
    int		sts;

    source.redis = redis_init(settings->hostspec);

    load_prepare_source(&source, root, 0);
    if (source.context.type) {
	set_source_origin(&source.context);
    } else {
	loadfmt(msg, "found no context to load");
	loadmsg(&source, PMLOG_ERROR, msg);
	return -ESRCH;
    }

    if ((sts = new_context(&source)) < 0)
	return sts;
    server_cache_source(&source);

    /* metric and time-based filtering */
    if ((sts = load_prepare_metrics(&source)) < 0 ||
	(sts = load_prepare_timing(&source, timing, flags)) < 0) {
	destroy_context(&source);
	return sts;
    }

    series_cache_load(&source, timing, flags);
    return 0;
}
