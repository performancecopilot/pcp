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
#include "maps.h"
#include "util.h"
#include "slots.h"

#include "libpcp.h"

#define LOAD_PHASES	5

typedef struct seriesGetContext {
    unsigned int	magic;		/* MAGIC_CONTEXT */
//    int			type;
//    const char		*name;
    context_t		context;
    unsigned long long	count;		/* number of samples processed */
    pmResult		*result;	/* currently active sample data */
    int			error;		/* PMAPI error code from fetch */

    unsigned int	refcount;
    void		*baton;
} seriesGetContext;

typedef struct seriesLoadBaton {
    unsigned int	magic;		/* MAGIC_LOAD */

    seriesBatonPhase	*current;
    seriesBatonPhase	phases[LOAD_PHASES];
    unsigned int	refcount;

    seriesGetContext	pmapi;		/* PMAPI context info */
    redisSlots		*slots;		/* Redis server slots */

    pmSeriesSettings	*settings;
    void		*userdata;
    timing_t		timing;
    pmflags		flags;

    __pmHashCtl		clusterhash;
    __pmHashCtl		domainhash;
    __pmHashCtl		indomhash;
    __pmHashCtl		pmidhash;

    __pmHashCtl		errorhash;	/* PMIDs where errors observed */
    __pmHashCtl		wanthash;	/* PMIDs from query whitelist */

    int			error;
    void		*arg;
} seriesLoadBaton;

void initSeriesLoadBaton(seriesLoadBaton *, pmSeriesSettings *, pmflags, void *);
void freeSeriesLoadBaton(seriesLoadBaton *);

void initSeriesGetContext(seriesGetContext *, void *);
void freeSeriesGetContext(seriesGetContext *, int);

#define seriesfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define seriesmsg(sp, level, message)	\
	((sp)->settings->on_info((level), (message), (sp)->userdata), sdsfree(msg))

/* cache information about this metric source (host/archive) */
static void
server_cache_source(seriesLoadBaton *baton)
{
    redis_series_source(baton->slots, &baton->pmapi.context, baton);
}

/* cache information about this metric (values/metadata) */
static void
server_cache_metric(seriesLoadBaton *baton,
		metric_t *metric, sds timestamp, int meta, int data)
{
    redis_series_metric(baton->slots, metric, timestamp, meta, data, baton);
}

/* cache a mark record (discontinuity) for metrics from this source */
static void
server_cache_mark(seriesLoadBaton *baton, sds timestamp, int data)
{
    context_t		*context = seriesLoadBatonContext(baton);

    redis_series_mark(baton->slots, context, timestamp, data, baton);
}

static void
pmiderr(seriesLoadBaton *baton, pmID pmid, const char *msg, ...)
{
    va_list		arg;
    int			numnames;
    char		**names;

    /* TODO: not on stderr */

    if (__pmHashSearch(pmid, &baton->errorhash) == NULL) {
	numnames = pmNameAll(pmid, &names);
	fprintf(stderr, "%s: ", pmGetProgname());
	__pmPrintMetricNames(stderr, numnames, names, " or ");
	fprintf(stderr, "(%s) - ", pmIDStr(pmid));
	va_start(arg, msg);
	vfprintf(stderr, msg, arg);
	va_end(arg);
	__pmHashAdd(pmid, NULL, &baton->errorhash);
	if (numnames > 0) free(names);
    }
}

static void
load_prepare_metric(const char *name, void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		*hname;
    pmID		pmid;
    sds			msg;
    int			sts;

    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	seriesfmt(msg, "failed to lookup metric name (pmid=%s): %s",
		name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    } else if ((hname = strdup(name)) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"cache metric name", (__int64_t)strlen(name)+1);
	seriesmsg(baton, PMLOG_ERROR, msg);
    } else {
	if (pmDebugOptions.series)
	    fprintf(stderr, "load_prepare_metric: caching PMID=%s name=%s\n",
			pmIDStr(pmid), hname);
	__pmHashAdd(pmid, hname, &baton->wanthash);
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
new_instance(seriesLoadBaton *baton, metric_t *metric, int inst, int index)
{
    instlist_t		*instlist;
    size_t		size;
    sds			msg;
    unsigned int	i;

    if (metric->u.inst == NULL) {
	assert(index == 0);
	size = sizeof(instlist_t) + sizeof(value_t);
	if ((instlist = calloc(1, size)) == NULL) {
	    seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new instlist", (__int64_t)size);
	    seriesmsg(baton, PMLOG_ERROR, msg);
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
	    seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"grew instlist", (__int64_t)size);
	    seriesmsg(baton, PMLOG_ERROR, msg);
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
update_instance_metadata(seriesLoadBaton *baton, metric_t *metric, int ninst,
	int *instlist, char **namelist, int nsets, pmLabelSet *labelsets)
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
	    new_instance(baton, metric, instlist[i], i);

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
	        seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata name", (__int64_t)length);
		seriesmsg(baton, PMLOG_ERROR, msg);
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
		seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata labels", (__int64_t)length);
		seriesmsg(baton, PMLOG_ERROR, msg);
		continue;
	    }
	    if (value->labels)
		pmFreeLabelSets(value->labels, 1);
	    value->labels = labels;
	    break;
	}

	inst = json_escaped_str(value->name);
	instance_hash(metric, value, inst, desc);

	if (pmDebugOptions.series) {
	    fprintf(stderr, "Cache insert - instance: %s", value->name);
	    fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(value->hash));
	}
	value->cached = 1;
    }
}

/*
 * Iterate over the set of metric values and extract names and labels
 * for each instance.
 */
static int
get_instance_metadata(seriesLoadBaton *baton, metric_t *metric)
{
    pmLabelSet		*labelset = NULL;
    pmInDom		indom = metric->desc.indom;
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
	    seriesfmt(msg, "failed to get InDom %s instances: %s",
			pmInDomStr_r(indom, indommsg, sizeof(indommsg)),
			pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	    seriesmsg(baton, PMLOG_ERROR, msg);
	    return -1;
	}
	ninst = sts;
	if ((sts = pmGetInstancesLabels(indom, &labelset)) < 0) {
	    if (pmDebugOptions.series)
		fprintf(stderr, "%s: failed to get PMID %s labels: %s\n",
			pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	    /* continue on with no labels for this value */
	    sts = 0;
	}
	nsets = sts;

	update_instance_metadata(baton, metric,
				 ninst, instlist, namelist, nsets, labelset);
    }

    if (labelset) pmFreeLabelSets(labelset, nsets);
    if (namelist) free(namelist);
    if (instlist) free(instlist);
    return 0;
}

static domain_t *
new_domain(seriesLoadBaton *baton, int domain, context_t *context)
{
    domain_t		*domainp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((domainp = calloc(1, sizeof(domain_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new domain", (__int64_t)sizeof(domain_t));
	seriesmsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }
    domainp->domain = domain;
    domainp->context = context;
    if ((sts = pmGetDomainLabels(domain, &domainp->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to get domain (%d) labels: %s\n",
		    pmGetProgname(), domain, pmErrStr(sts));
	/* continue on with no labels for this domain */
    }
    if (__pmHashAdd(domain, (void *)domainp, &baton->domainhash) < 0) {
	seriesfmt(msg, "failed to store domain labels (domain=%d): %s",
		domain, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    }
    return domainp;
}

static cluster_t *
new_cluster(seriesLoadBaton *baton, int cluster, domain_t *domain)
{
    cluster_t		*clusterp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((clusterp = calloc(1, sizeof(cluster_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new cluster", (__int64_t)sizeof(cluster_t));
	seriesmsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }
    clusterp->cluster = cluster;
    clusterp->domain = domain;
    if ((sts = pmGetClusterLabels(cluster, &clusterp->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr,
		    "%s: failed to get cluster (%u.%u) labels: %s\n",
		    pmGetProgname(), pmID_domain(cluster), pmID_cluster(cluster),
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	/* continue on with no labels for this cluster */
    }
    if (__pmHashAdd(cluster, (void *)clusterp, &baton->clusterhash) < 0) {
	seriesfmt(msg, "failed to store cluster labels (cluster=%u.%u): %s",
		pmID_domain(cluster), pmID_cluster(cluster),
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    }
    return clusterp;
}

static indom_t *
new_indom(seriesLoadBaton *baton, pmInDom indom, domain_t *domain)
{
    indom_t		*indomp;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((indomp = calloc(1, sizeof(indom_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"new indom", (__int64_t)sizeof(indom_t));
	seriesmsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }
    indomp->indom = indom;
    indomp->domain = domain;
    if ((sts = pmGetInDomLabels(indom, &indomp->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to get indom (%s) labels: %s\n",
		    pmGetProgname(), pmInDomStr(indom),
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	/* continue on with no labels for this indom */
    }
    if (__pmHashAdd(indom, (void *)indomp, &baton->indomhash) < 0) {
	seriesfmt(msg, "failed to store indom (%s) labels: %s",
		pmInDomStr(indom), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    }
    return indomp;
}

static metric_t *
new_metric(seriesLoadBaton *baton, pmValueSet *vsp)
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
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new metric", (__int64_t)sizeof(metric_t));
	seriesmsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	seriesfmt(msg, "failed to lookup metric %s descriptor: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    } else if ((sts = pmNameAll(pmid, &names)) < 0) {
	seriesfmt(msg, "failed to lookup metric %s names: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    } else if ((mapids = calloc(sts, sizeof(__int64_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"mapids", (__int64_t)sts * sizeof(__int64_t));
	seriesmsg(baton, PMLOG_ERROR, msg);
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
    if ((hptr = __pmHashSearch(domain, &baton->domainhash)) != NULL)
	dp = (domain_t *)hptr->data;
    else
	dp = new_domain(baton, domain, &baton->pmapi.context);
    cluster = pmID_build(domain, pmID_cluster(pmid), 0);
    if ((hptr = __pmHashSearch(cluster, &baton->clusterhash)) != NULL)
	cp = (cluster_t *)hptr->data;
    else
	cp = new_cluster(baton, cluster, dp);
    if (desc.indom == PM_INDOM_NULL)
	ip = NULL;
    else if ((hptr = __pmHashSearch(desc.indom, &baton->indomhash)) != NULL)
	ip = (indom_t *)hptr->data;
    else
	ip = new_indom(baton, desc.indom, dp);

    metric->cluster = cp;
    metric->indom = ip;
    metric->desc = desc;
    metric->mapids = mapids;
    metric->names = names;
    metric->numnames = sts;

    if ((sts = pmGetItemLabels(pmid, &metric->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to get metric %s labels: %s\n",
		    pmGetProgname(), pmIDStr(pmid), pmErrStr(sts));
	/* continue on without item labels for this PMID */
    }

    metric_hash(metric, &desc);

    if (pmDebugOptions.series) {
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
new_instances(seriesLoadBaton *baton, metric_t *metric, pmValueSet *vsp)
{
    pmAtomValue		*avp;
    pmValue		*vp;
    int			j, k, type, count = 0;

    if (vsp->numval <= 0)
	return 0;

    type = metric->desc.type;
    if (metric->desc.indom == PM_INDOM_NULL) {
	if (!(baton->flags & PMFLAG_METADATA)) {	/* not metadata only */
	    vp = &vsp->vlist[0];
	    avp = &metric->u.atom;
	    pmExtractValue(vsp->valfmt, vp, type, avp, type);
	}
	return 0;
    }

    for (j = 0; j < vsp->numval; j++) {
	vp = &vsp->vlist[j];

	if (metric->u.inst == NULL) {
	    new_instance(baton, metric, vp->inst, j);
	    count++;
	    k = 0;
	}
	else if (j >= metric->u.inst->listcount) {
	    new_instance(baton, metric, vp->inst, j);
	    count++;
	    k = j;
	}
	else if (vp->inst != metric->u.inst->value[j].inst) {
	    for (k = 0; k < metric->u.inst->listcount; k++) {
		if (vp->inst == metric->u.inst->value[k].inst)
		    break;	/* k is now the correct offset */
	    }
	    if (k == metric->u.inst->listcount) {    /* no matching instance */
		new_instance(baton, metric, vp->inst, k);
		count++;
	    }
	} else {
	    k = j;		/* successful direct mapping */
	}
	if (baton->flags & PMFLAG_METADATA)	/* metadata only */
	    continue;
	avp = &metric->u.inst->value[k].atom;
	pmExtractValue(vsp->valfmt, vp, type, avp, type);
    }

    metric->u.inst->listcount = vsp->numval;

    return count;
}

static void
series_cache_update(seriesLoadBaton *baton)
{
    pmResult		*result = baton->pmapi.result;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    metric_t		*metric = NULL;
    sds			timestamp;
    int			i, write_meta, write_data;

    timestamp = sdsnew(timeval_str(&result->timestamp));
    write_data = (!(baton->flags & PMFLAG_METADATA));

    if (result->numpmid == 0) {
	server_cache_mark(baton, timestamp, write_data);
	sdsfree(timestamp);
	return;
    }

    pmSortInstances(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;

	/* check if in the restricted group (command line optional) */
	if (baton->wanthash.nodes &&
	    __pmHashSearch(vsp->pmid, &baton->wanthash) == NULL)
	    continue;

	/* check if pmid already in hash list */
	if ((hptr = __pmHashSearch(vsp->pmid, &baton->pmidhash)) == NULL) {

	    /* create a new one & add to hash */
	    if ((metric = new_metric(baton, vsp)) == NULL)
		continue;

	    if (__pmHashAdd(vsp->pmid, (void *)metric, &baton->pmidhash) < 0) {
		pmiderr(baton, vsp->pmid, "failed hash table insertion\n",
			pmGetProgname());
		/* free memory allocated above on insert failure */
		free_metric(metric);
		continue;
	    }

	    write_meta = 1;
	} else {	/* pmid exists */
	    metric = (metric_t *)hptr->data;
	    clear_metric_updated(metric);
	    write_meta = 0;
	}

	/* iterate through result instances and ensure metric_t is complete */
	if (metric->error == 0 && vsp->numval < 0)
	    write_meta = 1;
	if (new_instances(baton, metric, vsp) != 0)
	    write_meta = 1;

	/* record the error code in the cache */
	metric->error = (vsp->numval < 0) ? vsp->numval : 0;

	if (write_meta && get_instance_metadata(baton, metric) != 0)
	    continue;

	server_cache_metric(baton, metric, timestamp, write_meta, write_data);
    }
    sdsfree(timestamp);
}

static void server_cache_window(void *);	/* TODO */

static int
server_cache_series(seriesLoadBaton *baton)
{
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if (baton->pmapi.context.type != PM_CONTEXT_ARCHIVE)
	return -ENOTSUP;

    if ((sts = pmSetMode(PM_MODE_FORW, &baton->timing.start, 0)) < 0) {
	seriesfmt(msg, "pmSetMode failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_ERROR, msg);
	return sts;
    }

    server_cache_window(baton);
    return sts;
}

#if 0   /*TODO*/
static void
server_cache_update_done(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;

    assert(baton->magic == MAGIC_LOAD);
    pmFreeResult(context->result);
    context->result = NULL;
    context->count++;

    /* move onto the next fetch */
    server_cache_window(baton);
}
#endif

static void
doneSeriesGetContext(seriesLoadBaton *baton, seriesGetContext *context)
{
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;

    if (context->error == 0) {
	seriesfmt(msg, "processed %llu archive records from %s",
			context->count, context->context.name);
	seriesmsg(baton, PMLOG_INFO, msg);
    } else {
	seriesfmt(msg, "fetch failed: %s",
			pmErrStr_r(context->error, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_ERROR, msg);
    }
}

void
server_cache_window(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;
    struct timeval	*finish = &baton->timing.end;
    pmResult		*result;
    int			sts;

    assert(baton->magic == MAGIC_LOAD);
    assert(context->result == NULL);
    assert(context->refcount == 0);

    if ((sts = pmFetchArchive(&result)) >= 0) {
	context->result = result;
	if (finish->tv_sec > result->timestamp.tv_sec ||
	    (finish->tv_sec == result->timestamp.tv_sec &&
	     finish->tv_usec >= result->timestamp.tv_usec)) {
	    series_cache_update(baton /* TODO:, server_cache_update_done */);
	}
	else {
	    pmFreeResult(result);
	    context->result = NULL;
	    sts = PM_ERR_EOL;
	}
    }

    if (sts < 0 && sts != PM_ERR_EOL)
	baton->error = sts;
    if (sts < 0) {
	doneSeriesGetContext(baton, context);
	doneSeriesLoadBaton(baton);
    }
}

static void
set_context_source(seriesLoadBaton *baton, const char *source)
{
    baton->pmapi.context.name = sdsnew(source);
}

static void
set_context_type(seriesLoadBaton *baton, const char *name)
{
    if (strcmp(name, "path") == 0 ||
	strcmp(name, "archive") == 0 ||
	strcmp(name, "directory") == 0) {
	baton->pmapi.context.type = PM_CONTEXT_ARCHIVE;
	return;
    }
    if (strcmp(name, "host") == 0 ||
	strcmp(name, "hostname") == 0 ||
	strcmp(name, "hostspec") == 0) {
	baton->pmapi.context.type = PM_CONTEXT_HOST;
	return;
    }
}

static int
add_source_metric(seriesLoadBaton *baton, const char *metric)
{
    int			count = baton->pmapi.context.nmetrics;
    int			length = (count + 1) * sizeof(char *);
    const char		**metrics;

    if ((metrics = (const char **)realloc(baton->pmapi.context.metrics, length)) == NULL)
	return -ENOMEM;
    metrics[count++] = metric;
    baton->pmapi.context.metrics = metrics;
    baton->pmapi.context.nmetrics = count;
    return 0;
}

static void
load_prepare_metrics(seriesLoadBaton *baton)
{
    const char		**metrics = baton->pmapi.context.metrics;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			i, sts;

    for (i = 0; i < baton->pmapi.context.nmetrics; i++) {
	if ((sts = pmTraversePMNS_r(metrics[i], load_prepare_metric, baton)) >= 0)
	    continue;
	seriesfmt(msg, "PMNS traversal failed for %s: %s",
			metrics[i], pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	seriesmsg(baton, PMLOG_WARNING, msg);
    }
}

static int
load_prepare_timing(seriesLoadBaton *baton)
{
    struct timeval	*finish = &baton->timing.end;
    struct timeval	*start = &baton->timing.start;

    /*
     * If no time window given,
     * - in general archive mode, scan the entire log
     * - in live-updating mode, scan back from 12 hours ago
     */
    if (finish->tv_sec == 0)
	finish->tv_sec = time(NULL);
    if (start->tv_sec == 0 && (baton->flags & PMFLAG_ACTIVE))
	start->tv_sec = finish->tv_sec - (12 * 60 * 60);

    /* TODO: handle PMAPI timezone settings in baton->timing */
    return 0;
}

static void
load_prepare_source(seriesLoadBaton *baton, node_t *np, int level)
{
    const char	*name;

    if (np == NULL)
	return;

    /* descend to the leaves first */
    load_prepare_source(baton, np->left, level+1);
    load_prepare_source(baton, np->right, level+1);

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
	set_context_type(baton, name);
	break;

    case N_EQ:
    case N_GLOB:
	if (np->right->type != N_STRING)
	    break;
	if (np->left->type == N_NAME || np->left->type == N_STRING) {
	    if (np->left->subtype == N_CONTEXT)
		 set_context_source(baton, np->right->value);
	}
	if (np->left->type == N_METRIC)
	    add_source_metric(baton, np->right->value);
	break;

    default:
	break;
    }
}

static void
new_pmapi_context(seriesLoadBaton *baton)
{
    context_t		*cp = &baton->pmapi.context;
    char		labels[PM_MAXLABELJSONLEN];
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    /* establish PMAPI context */
    if ((sts = cp->context = pmNewContext(cp->type, cp->name)) < 0) {
	if (cp->type == PM_CONTEXT_HOST)
	    seriesfmt(msg, "cannot connect to PMCD on host \"%s\": %s",
		    cp->name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else if (cp->type == PM_CONTEXT_LOCAL)
	    seriesfmt(msg, "cannot make standalone connection on localhost: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else
	    seriesfmt(msg, "cannot open archive \"%s\": %s",
		    cp->name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_meta(cp, labels, sizeof(labels))) < 0) {
	seriesfmt(msg, "failed to get context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_hash(cp->hash, labels, sts)) < 0) {
	seriesfmt(msg, "failed to merge context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else {
	if (pmDebugOptions.series) {
	    fprintf(stderr, "Cache insert context - name: %s", cp->name);
	    fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(cp->hash));
	}
	return /*success*/;
    }

    seriesmsg(baton, PMLOG_ERROR, msg);
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

void
source_mapping_callback(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    assert(baton->magic == MAGIC_LOAD);
    seriesPassBaton(&baton->current, &baton->refcount, baton);
}

void
series_source_mapping(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    context_t		*context = &baton->pmapi.context;

    assert(baton->magic == MAGIC_LOAD);

    if (context->mapid > 0 && context->hostid > 0) {
	/* fast path - string maps are already resolved */
	seriesPassBaton(&baton->current, &baton->refcount, baton);
    } else {
	assert(baton->refcount == 0);
	incSeriesLoadBatonRef(baton);
	if (context->mapid <= 0) {
	    incSeriesLoadBatonRef(baton);
	    redisGetMap(baton->slots, contextmap, context->name,
			&context->mapid, source_mapping_callback,
			baton->settings->on_info, baton->arg, (void *)baton);
	}
	if (context->hostid <= 0) {
	    incSeriesLoadBatonRef(baton);
	    redisGetMap(baton->slots, contextmap, context->host,
			&context->hostid, source_mapping_callback,
			baton->settings->on_info, baton->arg, (void *)baton);
	}
	decSeriesLoadBatonRef(baton);
    }
}

void
initSeriesGetContext(seriesGetContext *baton /*, int type, const char *name*/, void *arg)
{
    baton->magic = MAGIC_CONTEXT;
    //baton->type = type;
    //baton->name = name;
    baton->baton = arg;
}

void
freeSeriesGetContext(seriesGetContext *baton, int release)
{
    context_t		*cp = &baton->context;

    pmDestroyContext(cp->context);
    if (cp->name)
	sdsfree(cp->name);
    if (cp->origin)
	sdsfree(cp->origin);

    if (release) {
	memset(baton, 0, sizeof(*baton));
	free(baton);
    }
}

static void
series_cache_source(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    assert(baton->magic == MAGIC_LOAD);
    server_cache_source(baton);
}

static void
series_cache_metrics(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    int			sts;

    assert(baton->magic == MAGIC_LOAD);
    if ((sts = server_cache_series(baton)) < 0)
	baton->error = sts;
}

static void
series_load_finished(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    freeSeriesLoadBaton(baton);
}

static void
series_load_end_phase(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    assert(baton->magic == MAGIC_LOAD);

    if (baton->error == 0) {
	seriesPassBaton(&baton->current, &baton->refcount, baton);
    } else {	/* fail after waiting on outstanding I/O */
	if (baton->refcount != 0)
	    baton->refcount--;
	if (baton->refcount == 0)
	    series_load_finished(baton);
    }
}

static void
connect_pmapi_source_service(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    assert(baton->magic == MAGIC_LOAD);

    new_pmapi_context(baton);
    if (baton->error == 0) {
	/* setup metric and time-based filtering for source load */
	load_prepare_timing(baton);
	load_prepare_metrics(baton);
    }
    series_load_end_phase(baton);
}

static void
connect_redis_source_service(seriesLoadBaton *baton)
{
    pmSeriesSettings	*settings = baton->settings;

    redis_init(&baton->slots, settings->hostspec, 1, settings->on_info,
		series_load_end_phase, baton->userdata, settings->events,
		(void *)baton);
}

static void
setup_source_services(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    /*context_t		*cp; TODO */

    assert(baton->magic == MAGIC_LOAD);
    baton->refcount = 2;
    /*cp = &baton->pmapi.context;*/
    initSeriesGetContext(&baton->pmapi, /*cp->type, cp->name,*/ baton);

    connect_pmapi_source_service(baton);
    connect_redis_source_service(baton);
}

void
setSeriesLoadBatonRef(seriesLoadBaton *baton, unsigned int refcount)
{
    assert(baton->magic == MAGIC_LOAD);
    assert(baton->refcount == 0);
    baton->refcount = refcount;
}

void
incSeriesLoadBatonRef(seriesLoadBaton *baton)
{
    assert(baton->magic == MAGIC_LOAD);
    baton->refcount++;
}

void
decSeriesLoadBatonRef(seriesLoadBaton *baton)
{
    assert(baton->magic == MAGIC_LOAD);
    baton->refcount--;
}

void
initSeriesLoadBaton(seriesLoadBaton *baton,
		pmSeriesSettings *settings, pmflags flags, void *userdata)
{
    baton->magic = MAGIC_LOAD;
    baton->settings = settings;
    baton->userdata = userdata;
    baton->flags = flags;
}

void
freeSeriesLoadBaton(seriesLoadBaton *baton)
{
    pmSeriesSettings	*settings;

    assert(baton->magic == MAGIC_LOAD);
    freeSeriesGetContext(&baton->pmapi, 0);
    settings = baton->settings;
    if (settings->on_done)
	settings->on_done(baton->error, baton->userdata);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

void
doneSeriesLoadBaton(seriesLoadBaton *baton)
{
    seriesPassBaton(&baton->current, &baton->refcount, baton);
}

void
seriesLoadBatonRefcount(seriesLoadBaton *baton, int refcount)
{
    assert(baton->magic == MAGIC_LOAD);
    assert(baton->refcount == 0);
    baton->refcount = refcount;
}

redisInfoCallBack
seriesLoadBatonInfo(seriesLoadBaton *baton)
{
    return baton->settings->on_info;
}

void *
seriesLoadBatonSlots(seriesLoadBaton *baton)
{
    return baton->slots;
}

context_t *
seriesLoadBatonContext(seriesLoadBaton *baton)
{
    return &baton->pmapi.context;
}

void *
seriesLoadBatonUser(seriesLoadBaton *baton)
{
    return baton->userdata;
}

int
series_load(pmSeriesSettings *settings,
	node_t *root, timing_t *timing, pmflags flags, void *arg)
{
    seriesLoadBaton	*baton;
    sds			msg;
    int			i = 0;

    if ((baton = (seriesLoadBaton *)calloc(1, sizeof(seriesLoadBaton))) == NULL)
	return -ENOMEM;
    initSeriesLoadBaton(baton, settings, flags, arg);
    baton->timing = *timing;

    /* initial setup (non-blocking) */
    load_prepare_source(baton, root, 0);
    if (baton->pmapi.context.type) {
	set_source_origin(&baton->pmapi.context);
    } else {
	seriesfmt(msg, "found no context to load");
	seriesmsg(baton, PMLOG_ERROR, msg);
	freeSeriesLoadBaton(baton);
	return -EINVAL;
    }

    /* ordering of async operations */
    baton->current = &baton->phases[0];
    baton->phases[i++].func = setup_source_services;
    baton->phases[i++].func = series_source_mapping;	/* assign source/host string map */
    baton->phases[i++].func = series_cache_source;	/* write source info into schema */
    baton->phases[i++].func = series_cache_metrics;	/* write time series into schema */
    baton->phases[i++].func = series_load_finished;
    assert(i <= LOAD_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}
