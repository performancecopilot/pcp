/*
 * Copyright (c) 2017-2019 Red Hat.
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
 */
#include <math.h>
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include "discover.h"
#include "schema.h"
#include "util.h"

void initSeriesLoadBaton(seriesLoadBaton *, void *, pmSeriesFlags, 
	pmLogInfoCallBack, pmSeriesDoneCallBack, redisSlots *, void *);
void freeSeriesLoadBaton(seriesLoadBaton *);

void initSeriesGetContext(seriesGetContext *, void *);
void freeSeriesGetContext(seriesGetContext *, int);

/* cache information about this metric source (host/archive) */
static void
server_cache_source(seriesLoadBaton *baton)
{
    redis_series_source(baton->slots, baton);
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
    redis_series_mark(baton->slots, timestamp, data, baton);
}

#if 0
static void
pmidErr(seriesLoadBaton *baton, pmID pmid, const char *fmt, ...)
{
    va_list		arg;
    int			i, numnames;
    char		**names;
    sds			msg;

    if (dictFetchValue(baton->errors, &pmid) == NULL) {
	dictAdd(baton->errors, &pmid, NULL);
	if ((numnames = pmNameAll(pmid, &names)) < 1)
	    msg = sdsnew("<no metric names>");
	else {
	    msg = sdsnew(names[0]);
	    for (i = 1; i < numnames; i++)
		msg = sdscatfmt(msg, " or %s", names[i]);
	    free(names);
	}
	msg = sdscatfmt(msg, "(%s) - ", pmIDStr(pmid));
	va_start(arg, fmt);
	msg = sdscatvprintf(msg, fmt, arg);
	va_end(arg);
	batoninfo(baton, PMLOG_WARNING, msg);
    }
}
#endif

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
	infofmt(msg, "failed to lookup metric name (pmid=%s): %s",
		name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	batoninfo(baton, PMLOG_WARNING, msg);
    } else if ((hname = strdup(name)) == NULL) {
	infofmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"cache metric name", (__int64_t)strlen(name)+1);
	batoninfo(baton, PMLOG_ERROR, msg);
    } else {
	if (pmDebugOptions.series)
	    fprintf(stderr, "load_prepare_metric: caching PMID=%s name=%s\n",
			pmIDStr(pmid), hname);
	dictAdd(baton->wanted, &pmid, hname);
    }
}

/*
 * Iterate over an instance domain and extract names and labels
 * for each instance.
 */
static unsigned int
get_instance_metadata(seriesLoadBaton *baton, pmInDom indom)
{
    context_t		*cp = &baton->pmapi.context;
    unsigned int	count = 0;
    domain_t		*dp;
    indom_t		*ip;

    if (indom != PM_INDOM_NULL) {
	if ((dp = pmwebapi_add_domain(cp, pmInDom_domain(indom))))
	    pmwebapi_add_domain_labels(dp);
	if ((ip = pmwebapi_add_indom(cp, dp, indom)) &&
	    (count = pmwebapi_add_indom_instances(ip)) > 0)
	    pmwebapi_add_instances_labels(ip);
    }
    return count;
}

static metric_t *
new_metric(seriesLoadBaton *baton, pmValueSet *vsp)
{
    context_t		*context = &baton->pmapi.context;
    metric_t		*metric;
    pmDesc		desc;
    char		errmsg[PM_MAXERRMSGLEN], idbuf[64];
    char		**nameall = NULL;
    sds			msg;
    int			count, sts, i;

    if ((sts = pmLookupDesc(vsp->pmid, &desc)) < 0) {
	infofmt(msg, "failed to lookup metric %s descriptor: %s",
		pmIDStr_r(vsp->pmid, idbuf, sizeof(idbuf)),
		pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	batoninfo(baton, PMLOG_WARNING, msg);
    } else if ((sts = count = pmNameAll(vsp->pmid, &nameall)) < 0) {
	infofmt(msg, "failed to lookup metric %s names: %s",
		pmIDStr_r(vsp->pmid, idbuf, sizeof(idbuf)),
		pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	batoninfo(baton, PMLOG_WARNING, msg);
    }
    if (sts < 0)
	return NULL;

    if ((metric = pmwebapi_new_metric(context, &desc, count, nameall)) == NULL)
	return NULL;
    if (metric->cluster) {
	if (metric->cluster->domain)
	    pmwebapi_add_domain_labels(metric->cluster->domain);
	pmwebapi_add_cluster_labels(metric->cluster);
    }
    if (metric->indom)
	pmwebapi_add_instances_labels(metric->indom);
    pmwebapi_add_item_labels(metric);
    pmwebapi_metric_hash(metric);

    if (pmDebugOptions.series) {
	fprintf(stderr, "new_metric [%s] names:",
		pmIDStr_r(vsp->pmid, idbuf, sizeof(idbuf)));
	for (i = 0; i < count; i++) {
	    pmwebapi_hash_str(metric->names[i].hash, idbuf, sizeof(idbuf));
	    fprintf(stderr, "SHA1=%s [%s]\n", idbuf, metric->names[i].sds);
	}
	if (count == 0)
	    fprintf(stderr, "(none)\n");
    }

    return metric;
}

#if 0
static void
free_metric(metric_t *metric)
{
    int			i;

    if (metric->desc.indom != PM_INDOM_NULL && metric->u.vlist != NULL) 
	free(metric->u.vlist);
    if (metric->names) {
	for (i = 0; i < metric->numnames; i++)
	    sdsfree(metric->names[i].sds);
	free(metric->names);
    }
    free(metric);
}
#endif

static int
pmwebapi_add_value(metric_t *metric, int inst, int index)
{
    valuelist_t		*vlist;
    size_t		size;
    unsigned int	i;

    if (metric->u.vlist == NULL) {
	assert(index == 0);
	size = sizeof(valuelist_t) + sizeof(value_t);
	if ((vlist = calloc(1, size)) == NULL)
	    return -ENOMEM;
	vlist->listcount = vlist->listsize = 1;
	vlist->value[0].inst = inst;
	metric->u.vlist = vlist;
	return 0;
    }

    vlist = metric->u.vlist;
    assert(vlist->listcount <= vlist->listsize);

    if (index >= vlist->listsize) {
	size = vlist->listsize * 2;
	assert(index < size);
	size = sizeof(valuelist_t) + (size * sizeof(value_t));
	if ((vlist = (valuelist_t *)realloc(vlist, size)) == NULL)
	    return -ENOMEM;
	vlist->listsize *= 2;
	for (i = vlist->listcount; i < vlist->listsize; i++) {
	    memset(&vlist->value[i], 0, sizeof(value_t));
	}
    }

    i = vlist->listcount++;
    vlist->value[i].inst = inst;
    metric->u.vlist = vlist;
    return 0;
}

static void
pmwebapi_clear_metric_updated(metric_t *metric)
{
    int			i, count;

    metric->updated = 0;
    if (metric->desc.indom == PM_INDOM_NULL || metric->u.vlist == NULL)
	return;
    count = metric->u.vlist->listcount;
    for (i = 0; i < count; i++)
	metric->u.vlist->value[i].updated = 0;
}

int
pmwebapi_add_valueset(metric_t *metric, pmValueSet *vsp)
{
    pmValue		*vp;
    value_t		*value;
    int			j, k, type, count = 0;

    pmwebapi_clear_metric_updated(metric);

    if (vsp->numval <= 0)
	return 0;

    metric->updated = 1;
    type = metric->desc.type;
    if (metric->desc.indom == PM_INDOM_NULL) {
	vp = &vsp->vlist[0];
	pmExtractValue(vsp->valfmt, vp, type, &metric->u.atom, type);
	return 1;
    }

    for (j = 0; j < vsp->numval; j++) {
	vp = &vsp->vlist[j];

	if (metric->u.vlist == NULL) {
	    pmwebapi_add_value(metric, vp->inst, j);
	    count++;
	    k = 0;
	}
	else if (j >= metric->u.vlist->listcount) {
	    pmwebapi_add_value(metric, vp->inst, j);
	    count++;
	    k = j;
	}
	else if (vp->inst != metric->u.vlist->value[j].inst) {
	    for (k = 0; k < metric->u.vlist->listcount; k++) {
		if (vp->inst == metric->u.vlist->value[k].inst)
		    break;	/* k is now the correct offset */
	    }
	    if (k == metric->u.vlist->listcount) {    /* no matching instance */
		pmwebapi_add_value(metric, vp->inst, k);
		count++;
	    }
	} else {
	    k = j;		/* successful direct mapping */
	}
	value = &metric->u.vlist->value[k];
	pmExtractValue(vsp->valfmt, vp, type, &value->atom, type);
	value->updated = 1;
    }

    if (metric->u.vlist)
	metric->u.vlist->listcount = vsp->numval;
    return count;
}

static void
series_cache_update(seriesLoadBaton *baton)
{
    seriesGetContext	*context = &baton->pmapi;
    context_t		*cp = &context->context;
    pmResult		*result = context->result;
    pmValueSet		*vsp;
    metric_t		*metric = NULL;
    char		ts[64];
    sds			timestamp;
    int			i, write_meta, write_data;

    timestamp = sdsnew(timeval_stream_str(&result->timestamp, ts, sizeof(ts)));
    write_data = (!(baton->flags & PM_SERIES_FLAG_METADATA));

    if (result->numpmid == 0) {
	seriesBatonReference(context, "series_cache_update[mark]");
	server_cache_mark(baton, timestamp, write_data);
	goto out;
    }

    pmSortInstances(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;

	/* check if in the restricted group (optional metric filter) */
	if (dictSize(baton->wanted) &&
	    dictFetchValue(baton->wanted, &vsp->pmid) == NULL)
	    continue;

	/* check if pmid already in hash list */
	if ((metric = dictFetchValue(cp->pmids, &vsp->pmid)) == NULL) {
	    /* create a new metric, and add it to load context */
	    if ((metric = new_metric(baton, vsp)) == NULL)
		continue;
	    write_meta = 1;
	} else {	/* pmid already observed */
	    write_meta = 0;
	}

	/* iterate through result instances and ensure metric_t is complete */
	if (metric->error == 0 && vsp->numval < 0)
	    write_meta = 1;
	if (pmwebapi_add_valueset(metric, vsp) != 0)
	    write_meta = 1;

	/* record the error code in the cache */
	metric->error = (vsp->numval < 0) ? vsp->numval : 0;

	/* make PMAPI calls to cache metadata */
	if (write_meta && get_instance_metadata(baton, metric->desc.indom) != 0)
	    continue;

	/* initiate writes to backend caching servers (Redis) */
	server_cache_metric(baton, metric, timestamp, write_meta, write_data);
    }

out:
    sdsfree(timestamp);
    /* drop reference taken in server_cache_window */
    doneSeriesGetContext(context, "series_cache_update");
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
	infofmt(msg, "pmSetMode failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	batoninfo(baton, PMLOG_ERROR, msg);
	return sts;
    }

    seriesBatonReference(baton, "server_cache_series");
    server_cache_window(baton);
    return 0;
}

static void
server_cache_series_finished(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;

    assert(context->result == NULL);

    /* drop load reference taken in server_cache_series */
    doneSeriesLoadBaton(baton, "server_cache_series_finished");
}

static void
server_cache_update_done(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;

    /* finish book-keeping for the current record */
    pmFreeResult(context->result);
    context->result = NULL;
    context->count++;
    context->done = NULL;

    /* begin processing of the next record if any */
    server_cache_window(baton);
}

void
server_cache_window(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    seriesGetContext	*context = &baton->pmapi;
    struct timeval	*finish = &baton->timing.end;
    pmResult		*result;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "server_cache_window");
    seriesBatonCheckCount(context, "server_cache_window");
    assert(context->result == NULL);

    if (pmDebugOptions.series)
	fprintf(stderr, "server_cache_window: fetching next result\n");

    seriesBatonReference(context, "server_cache_window");
    context->done = server_cache_series_finished;

    if ((sts = pmFetchArchive(&result)) >= 0) {
	context->result = result;
	if (finish->tv_sec > result->timestamp.tv_sec ||
	    (finish->tv_sec == result->timestamp.tv_sec &&
	     finish->tv_usec >= result->timestamp.tv_usec)) {
	    context->done = server_cache_update_done;
	    series_cache_update(baton);
	}
	else {
	    if (pmDebugOptions.series)
		fprintf(stderr, "server_cache_window: end of time window\n");
	    sts = PM_ERR_EOL;
	    pmFreeResult(result);
	    context->result = NULL;
	}
    }

    if (sts < 0) {
	context->error = sts;
	if (sts != PM_ERR_EOL)
	    baton->error = sts;
	doneSeriesGetContext(context, "server_cache_window");
    }
}

static void
set_context_source(seriesLoadBaton *baton, const char *source)
{
    baton->pmapi.context.name.sds = sdsnew(source);
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
    int			count = baton->nmetrics;
    int			length = (count + 1) * sizeof(char *);
    const char		**metrics;

    if ((metrics = (const char **)realloc(baton->metrics, length)) == NULL)
	return -ENOMEM;
    metrics[count++] = metric;
    baton->metrics = metrics;
    baton->nmetrics = count;
    return 0;
}

static void
load_prepare_metrics(seriesLoadBaton *baton)
{
    const char		**metrics = baton->metrics;
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			i, sts;

    for (i = 0; i < baton->nmetrics; i++) {
	if ((sts = pmTraversePMNS_r(metrics[i], load_prepare_metric, baton)) >= 0)
	    continue;
	infofmt(msg, "PMNS traversal failed for %s: %s",
			metrics[i], pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	batoninfo(baton, PMLOG_WARNING, msg);
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
    if (start->tv_sec == 0 && (baton->flags & PM_SERIES_FLAG_ACTIVE))
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
	    if (np->left->subtype == N_METRIC)
		add_source_metric(baton, np->right->value);
	}
	if (np->left->type == N_METRIC)
	    add_source_metric(baton, np->right->value);
	break;

    default:
	break;
    }
}

static void
set_source_origin(context_t *cp)
{
    char		host[MAXHOSTNAMELEN];
    char		path[MAXPATHLEN];
    size_t		bytes;

    if (cp->type == PM_CONTEXT_ARCHIVE) {
	if (realpath(cp->name.sds, path) != NULL)
	    bytes = strlen(path);
	else
	    bytes = pmsprintf(path, sizeof(path), "%s", cp->name.sds);
	cp->name.sds = sdscpylen(cp->name.sds, path, bytes);
    }

    if ((gethostname(host, sizeof(host))) == 0)
	bytes = strlen(host);
    else
	bytes = pmsprintf(host, sizeof(host), "localhost");
    cp->origin = sdsnewlen(host, bytes);
}

static void
series_string_mapping_callback(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    doneSeriesLoadBaton(baton, "series_string_mapping");
}

static void
series_string_mapping(seriesLoadBaton *baton, redisMap *mapping,
			unsigned char *hash, sds string)
{
    /* completes immediately (fills hash), but may issue async I/O too */
    redisGetMap(baton->slots, mapping, hash, string,
		series_string_mapping_callback,
		baton->info, baton->userdata, baton);
}

void
series_source_mapping(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    context_t		*context = &baton->pmapi.context;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_source_mapping");
    seriesBatonCheckCount(baton, "series_source_mapping");

    seriesBatonReferences(baton, 2, "series_source_mapping");
    series_string_mapping(baton, contextmap, context->name.id, context->name.sds);
    series_string_mapping(baton, contextmap, context->hostid, context->host);
}

void
initSeriesGetContext(seriesGetContext *baton, void *arg)
{
    initSeriesBatonMagic(baton, MAGIC_CONTEXT);
    baton->baton = arg;
}

void
freeSeriesGetContext(seriesGetContext *baton, int release)
{
    context_t		*cp = &baton->context;

    seriesBatonCheckMagic(baton, MAGIC_CONTEXT, "freeSeriesGetContext");
    pmwebapi_free_context(cp);
    if (release) {
	memset(baton, 0, sizeof(*baton));
	free(baton);
    }
}

void
doneSeriesGetContext(seriesGetContext *context, const char *caller)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)context->baton;

    seriesBatonCheckMagic(context, MAGIC_CONTEXT, "doneSeriesGetContext");
    seriesBatonCheckMagic(baton, MAGIC_LOAD, "doneSeriesGetContext");

    if (seriesBatonDereference(context, caller) && context->done != NULL)
	context->done(baton);

    if (context->error) {
	char		pmmsg[PM_MAXERRMSGLEN];
	sds		msg;

	if (context->error == PM_ERR_EOL) {
	    infofmt(msg, "processed %llu archive records from %s",
			context->count, context->context.name.sds);
	    batoninfo(baton, PMLOG_INFO, msg);
	    context->error = 0;
	} else {
	    infofmt(msg, "fetch failed: %s",
			pmErrStr_r(context->error, pmmsg, sizeof(pmmsg)));
	    batoninfo(baton, PMLOG_ERROR, msg);
	}
    }
}

void
seriesLoadBatonFetch(seriesLoadBaton *baton)
{
    doneSeriesGetContext(&baton->pmapi, "seriesLoadBatonFetch");
}

static void
series_cache_source(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_cache_source");
    server_cache_source(baton);
}

static void
series_cache_metrics(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_cache_metrics");
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

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_load_end_phase");

    if (baton->error == 0) {
	seriesPassBaton(&baton->current, baton, "series_load_end_phase");
    } else {	/* fail after waiting on outstanding I/O */
	if (seriesBatonDereference(baton, "series_load_end_phase"))
	    series_load_finished(baton);
    }
}

static void
connect_pmapi_source_service(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    context_t		*cp = &baton->pmapi.context;
    sds			msg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "connect_pmapi_source_service");

    if ((msg = pmwebapi_new_context(cp)) != NULL) {
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = PM_ERR_NOCONTEXT;
    } else if (baton->error == 0) {
	/* setup metric and time-based filtering for source load */
	load_prepare_timing(baton);
	load_prepare_metrics(baton);
    }
    series_load_end_phase(baton);
}

static void
connect_redis_source_service(seriesLoadBaton *baton)
{
    pmSeriesModule	*module = (pmSeriesModule *)baton->module;
    seriesModuleData	*data = getSeriesModuleData(module);

    /* attempt to re-use existing slots connections */
    if (data == NULL) {
	baton->error = -ENOMEM;
    } else if (data->slots) {
	baton->slots = data->slots;
	series_load_end_phase(baton);
    } else {
	baton->slots = data->slots =
	    redisSlotsConnect(
		data->config, 1, baton->info,
		series_load_end_phase, baton->userdata,
		data->events, (void *)baton);
    }
}

static void
setup_source_services(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "setup_source_services");
    seriesBatonReferences(baton, 2, "setup_source_services");

    connect_pmapi_source_service(baton);
    connect_redis_source_service(baton);
}

void
initSeriesLoadBaton(seriesLoadBaton *baton, void *module, pmSeriesFlags flags, 
	pmLogInfoCallBack info, pmSeriesDoneCallBack done, redisSlots *slots,
	void *userdata)
{
    initSeriesBatonMagic(baton, MAGIC_LOAD);
    baton->info = info;
    baton->done = done;
    baton->slots = slots;
    baton->module = module;
    baton->userdata = userdata;
    baton->flags = flags;

    baton->errors = dictCreate(&intKeyDictCallBacks, baton);
    baton->wanted = dictCreate(&intKeyDictCallBacks, baton);
}

void
freeSeriesLoadBaton(seriesLoadBaton *baton)
{
    seriesBatonCheckMagic(baton, MAGIC_LOAD, "freeSeriesLoadBaton");

    if (baton->done)
	baton->done(baton->error, baton->userdata);

    freeSeriesGetContext(&baton->pmapi, 0);
    dictRelease(baton->errors);
    dictRelease(baton->wanted);
    free(baton->metrics);

    memset(baton, 0, sizeof(*baton));
    free(baton);
}

void
doneSeriesLoadBaton(seriesLoadBaton *baton, const char *caller)
{
    seriesPassBaton(&baton->current, baton, caller);
}

context_t *
seriesLoadBatonContext(seriesLoadBaton *baton)
{
    return &baton->pmapi.context;
}

int
series_load(pmSeriesSettings *settings,
	node_t *root, timing_t *timing, pmSeriesFlags flags, void *arg)
{
    seriesLoadBaton	*baton;
    seriesModuleData	*data = getSeriesModuleData(&settings->module);
    sds			msg;
    int			i;

    if (data == NULL)
	return -ENOMEM;
    if ((baton = (seriesLoadBaton *)calloc(1, sizeof(seriesLoadBaton))) == NULL)
	return -ENOMEM;
    initSeriesLoadBaton(baton, &settings->module, flags,
			settings->module.on_info, settings->callbacks.on_done,
			data->slots, arg);
    initSeriesGetContext(&baton->pmapi, baton);
    baton->timing = *timing;

    /* initial setup (non-blocking) */
    load_prepare_source(baton, root, 0);
    if (baton->pmapi.context.type) {
	set_source_origin(&baton->pmapi.context);
    } else {
	infofmt(msg, "found no context to load");
	batoninfo(baton, PMLOG_ERROR, msg);
	freeSeriesLoadBaton(baton);
	return -EINVAL;
    }

    /* ordering of async operations */
    i = 0;
    baton->current = &baton->phases[i];
    baton->phases[i++].func = setup_source_services;
    /* assign source/host string map (series_source_mapping) */
    baton->phases[i++].func = series_source_mapping;
    /* write source info into schema (series_cache_source) */
    baton->phases[i++].func = series_cache_source;
    /* write time series into schema (series_cache_metrics) */
    baton->phases[i++].func = series_cache_metrics;
    baton->phases[i++].func = series_load_finished;
    assert(i <= LOAD_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static void
series_source_persist(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_source_persist");
    /* take a reference to keep this load baton until closed */
    seriesBatonReference(baton, "series_source_persist");
}

static void
series_discover_done(int status, void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_discover_done");
    /* archive no longer active, remove from discovery set */
    freeSeriesLoadBaton(baton);
    (void)status;
}

void
pmSeriesDiscoverSource(pmDiscoverEvent *event, void *arg)
{
    pmDiscoverModule	*module = event->module;
    pmDiscover		*p = (pmDiscover *)event->data;
    pmLabelSet		*set;
    discoverModuleData	*data = getDiscoverModuleData(module);
    seriesLoadBaton	*baton;
    context_t		*cp;
    sds			msg;
    int			i;

    if (data == NULL || data->slots == NULL)
	return;

    baton = (seriesLoadBaton *)calloc(1, sizeof(seriesLoadBaton));
    if (baton == NULL) {
	infofmt(msg, "%s: out of memory for baton", "pmSeriesDiscoverSource");
	moduleinfo(module, PMLOG_ERROR, msg, arg);
	return;
    }
    initSeriesLoadBaton(baton, module, 0 /*flags*/,
			module->on_info, series_discover_done,
			data->slots, arg);
    initSeriesGetContext(&baton->pmapi, baton);
    p->baton = baton;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "%s: new source %s context=%d\n",
			"pmSeriesDiscoverSource", p->context.name, p->ctx);

    cp = &baton->pmapi.context;
    cp->context = p->ctx;
    cp->type = p->context.type;
    cp->name.sds = sdsdup(p->context.name);
    cp->host = p->context.hostname;
    cp->labelset = set = p->context.labelset;
    pmwebapi_source_hash(cp->name.hash, set->json, set->jsonlen);
    pmwebapi_setup_context(cp);
    set_source_origin(cp);

    /* ordering of async operations */
    i = 0;
    baton->current = &baton->phases[i];
    /* assign source/host string map (series_source_mapping) */
    baton->phases[i++].func = series_source_mapping;
    /* write source info into schema (series_cache_source) */
    baton->phases[i++].func = series_cache_source;
    /* batons finally released in pmSeriesDiscoverClosed */
    baton->phases[i++].func = series_source_persist;
    assert(i <= LOAD_PHASES);

    seriesBatonPhases(baton->current, i, baton);
}

void
pmSeriesDiscoverClosed(pmDiscoverEvent *event, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;

    (void)arg;

    /* release pmSeriesDiscoverSource reference on load and context batons */
    doneSeriesLoadBaton(baton, "pmSeriesDiscoverSource");
}

void
pmSeriesDiscoverLabels(pmDiscoverEvent *event,
		int ident, int type, pmLabelSet *sets, int nsets, void *arg)
{
    pmLabelSet		*labels;
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    struct context	*cp = &baton->pmapi.context;
    struct domain	*domain;
    struct cluster	*cluster;
    struct metric	*metric;
    struct indom	*indom;
    struct instance	*instance;
    char		errmsg[PM_MAXERRMSGLEN], idbuf[64];
    sds			msg;
    int			i, id;

    switch (type) {
    case PM_LABEL_CONTEXT:
	if ((labels = pmwebapi_labelsetdup(sets)) != NULL) {
	    if (cp->labelset)
		pmFreeLabelSets(cp->labelset, 1);
	    cp->labelset = labels;
	    pmwebapi_locate_context(cp);
	    cp->updated = 1;
	} else {
	    infofmt(msg, "failed to duplicate label set");
	    moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	}
	break;

    case PM_LABEL_DOMAIN:
	domain = pmwebapi_add_domain(cp, ident);
	if (domain && (labels = pmwebapi_labelsetdup(sets)) != NULL) {
	    if (domain->labelset)
		pmFreeLabelSets(domain->labelset, 1);
	    domain->labelset = labels;
	    domain->updated = 1;
	} else {
	    infofmt(msg, "failed to duplicate label set");
	    moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	}
	break;

    case PM_LABEL_CLUSTER:
	domain = pmwebapi_add_domain(cp, pmID_domain(ident));
	cluster = pmwebapi_add_cluster(cp, domain, pmID_cluster(ident));
	if (cluster && (labels = pmwebapi_labelsetdup(sets)) != NULL) {
	    if (cluster->labelset)
		pmFreeLabelSets(cluster->labelset, 1);
	    cluster->labelset = labels;
	    cluster->updated = 1;
	} else {
	    infofmt(msg, "failed to duplicate label set");
	    moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	}
	break;

    case PM_LABEL_ITEM:
	metric = pmwebapi_new_pmid(cp, ident, event->module->on_info, arg);
	if (metric && (labels = pmwebapi_labelsetdup(sets)) != NULL) {
	    if (metric->labelset)
		pmFreeLabelSets(metric->labelset, 1);
	    metric->labelset = labels;
	    metric->updated = 1;
	} else {
	    infofmt(msg, "failed to duplicate label set");
	    moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	}
	break;

    case PM_LABEL_INDOM:
	domain = pmwebapi_add_domain(cp, pmInDom_domain(ident));
	indom = pmwebapi_add_indom(cp, domain, ident);
	if (indom && (labels = pmwebapi_labelsetdup(sets)) != NULL) {
	    if (indom->labelset)
		pmFreeLabelSets(indom->labelset, 1);
		    indom->labelset = labels;
	    indom->updated = 1;
	} else {
	    infofmt(msg, "failed to duplicate label set");
	    moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	}
	break;

    case PM_LABEL_INSTANCES:
	domain = pmwebapi_add_domain(cp, pmInDom_domain(ident));
	indom = pmwebapi_add_indom(cp, domain, ident);
	for (i = 0; indom && i < nsets; i++) {
	    id = sets[i].inst;
	    if ((instance = dictFetchValue(indom->insts, &id)) == NULL)
		continue;
	    if ((labels = pmwebapi_labelsetdup(&sets[i])) == NULL) {
		infofmt(msg, "failed to dup %s instance labels: %s",
			pmInDomStr_r(indom->indom, idbuf, sizeof(idbuf)),
			pmErrStr_r(-ENOMEM, errmsg, sizeof(errmsg)));
		moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	    }
	    if (instance->labelset)
		pmFreeLabelSets(instance->labelset, 1);
	    instance->labelset = labels;
	    pmwebapi_instance_hash(indom, instance);
	    indom->updated = 1;
	}
	break;

    default:
	if (pmDebugOptions.discovery)
	    fprintf(stderr, "%s: unexpected label type %u\n",
			    "pmSeriesDiscoverLabels", type);
	break;
    }
}

void
pmSeriesDiscoverMetric(pmDiscoverEvent *event,
		pmDesc *desc, int numnames, char **names, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    struct metric	*metric;
    sds			msg;
    int			i;

    if (pmDebugOptions.discovery) {
	for (i = 0; i < numnames; i++)
	    fprintf(stderr, "pmSeriesDiscoverMetric [%d/%d]: %s - %s\n",
			i + 1, numnames, pmIDStr(desc->pmid), names[i]);
    }

    if ((metric = pmwebapi_add_metric(&baton->pmapi.context,
				desc, numnames, names)) == NULL) {
	infofmt(msg, "%s: failed metric discovery", "pmSeriesDiscoverMetric");
	moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	return;
    }
}

void
pmSeriesDiscoverValues(pmDiscoverEvent *event, pmResult *result, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    seriesGetContext	*context = &baton->pmapi;

    seriesBatonReference(context, "pmSeriesDiscoverValues");
    baton->arg = arg;
    context->result = result;

    series_cache_update(baton);
}

void
pmSeriesDiscoverInDom(pmDiscoverEvent *event, pmInResult *in, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;
    struct context	*context = &baton->pmapi.context;
    struct domain	*domain;
    struct indom	*indom;
    pmInDom		id = in->indom;
    sds			msg;
    int			i;

    if (pmDebugOptions.discovery)
	fprintf(stderr, "pmSeriesDiscoverInDom: %s\n", pmInDomStr(id));

    if ((domain = pmwebapi_add_domain(context, pmInDom_domain(id))) == NULL) {
	infofmt(msg, "%s: failed indom discovery (domain %u)",
			"pmSeriesDiscoverInDom", pmInDom_domain(id));
	moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	return;
    }
    if ((indom = pmwebapi_add_indom(context, domain, id)) == NULL) {
	infofmt(msg, "%s: failed indom discovery (indom %s)",
			"pmSeriesDiscoverInDom", pmInDomStr(id));
	moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	return;
    }
    for (i = 0; i < in->numinst; i++) {
	if (pmwebapi_add_instance(indom, in->instlist[i], in->namelist[i]))
	    continue;
	infofmt(msg, "%s: failed indom discovery (indom %s, instance %d: %s)",
			"pmSeriesDiscoverInDom", pmInDomStr(id),
			in->instlist[i], in->namelist[i]);
	moduleinfo(event->module, PMLOG_ERROR, msg, arg);
	return;
    }
}

void
pmSeriesDiscoverText(pmDiscoverEvent *event,
		int ident, int type, char *text, void *arg)
{
    pmDiscover		*p = (pmDiscover *)event->data;
    seriesLoadBaton	*baton = p->baton;

    (void)baton;
    (void)ident;
    (void)type;
    (void)text;
    (void)arg;

    /* for Redis, help text will need special handling (RediSearch) */
}
