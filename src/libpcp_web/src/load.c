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
#include <limits.h>
#include <assert.h>
#include <ctype.h>
#include "schema.h"
#include "util.h"

void initSeriesLoadBaton(seriesLoadBaton *, pmSeriesSettings *, pmflags, void *);
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

static uint64_t
idHash(const void *key)
{
    const unsigned int	*i = (const unsigned int *)key;

    return dictGenHashFunction(i, sizeof(unsigned int));
}

static int
idCmp(void *privdata, const void *a, const void *b)
{
    const unsigned int	*ia = (const unsigned int *)a;
    const unsigned int	*ib = (const unsigned int *)b;

    (void)privdata;
    return (*ia == *ib);
}

static void *
idDup(void *privdata, const void *key)
{
    unsigned int	*i = (unsigned int *)key;
    unsigned int	*k = (unsigned int *)malloc(sizeof(*i));

    (void)privdata;
    if (k)
	*k = *i;
    return k;
}

static void
idFree(void *privdata, void *value)
{
    (void)privdata;
    if (value) free(value);
}

static dictType idDict = {
    .hashFunction	= idHash,
    .keyCompare		= idCmp,
    .keyDup		= idDup,
    .keyDestructor	= idFree,
    .valDestructor      = idFree,
};

void *
findID(dict *map, void *id)
{
    dictEntry		*entry;

    entry = dictFind(map, id);
    return entry ? dictGetVal(entry) : NULL;
}

static void
pmidErr(seriesLoadBaton *baton, pmID pmid, const char *fmt, ...)
{
    va_list		arg;
    int			i, numnames;
    char		**names;
    sds			msg;

    if (findID(baton->errors, &pmid) == NULL) {
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
	webapimsg(baton, PMLOG_WARNING, msg);
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
	webapimsg(baton, PMLOG_WARNING, msg);
    } else if ((hname = strdup(name)) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
		"cache metric name", (__int64_t)strlen(name)+1);
	webapimsg(baton, PMLOG_ERROR, msg);
    } else {
	if (pmDebugOptions.series)
	    fprintf(stderr, "load_prepare_metric: caching PMID=%s name=%s\n",
			pmIDStr(pmid), hname);
	dictAdd(baton->wanted, &pmid, hname);
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
new_value(seriesLoadBaton *baton, metric_t *metric, int inst, int index)
{
    valuelist_t		*vlist;
    size_t		size;
    sds			msg;
    unsigned int	i;

    if (metric->u.vlist == NULL) {
	assert(index == 0);
	size = sizeof(valuelist_t) + sizeof(value_t);
	if ((vlist = calloc(1, size)) == NULL) {
	    seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new instlist", (__int64_t)size);
	    webapimsg(baton, PMLOG_ERROR, msg);
	    return -ENOMEM;
	}
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
	if ((vlist = (valuelist_t *)realloc(vlist, size)) == NULL) {
	    seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"grew instlist", (__int64_t)size);
	    webapimsg(baton, PMLOG_ERROR, msg);
	    return -ENOMEM;
	}
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

/*
 * Iterate over each instance associated with this indom, and complete
 * the metadata (instance name, labels) associated with each.
 */
static void
update_instance_metadata(seriesLoadBaton *baton, indom_t *indom, int ninst,
	int *instlist, char **namelist, int nsets, pmLabelSet *labelsets)
{
    instance_t		*instance;
    pmLabelSet		*labels;
    dictEntry		*entry;
    size_t		length;
    sds			msg;
    int			i, j;

    for (i = 0; i < ninst; i++) {
	unsigned int	key = instlist[i];

	if ((entry = dictFind(indom->insts, &key)) == NULL) {
	    instance = calloc(1, sizeof(instance_t));
	    instance->inst = key;
	    entry = dictAddRaw(indom->insts, &instance->inst, NULL);
	    if (entry)
		dictSetVal(indom->insts, entry, instance);
	} else {
	    instance = dictGetVal(entry);
	}
	instance->name.mapid = 0;
	if (instance->name.sds)
	    sdsfree(instance->name.sds);
	if ((instance->name.sds = sdsnew(namelist[i])) == NULL) {
	    length = strlen(namelist[i]);
	    seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata labels", (__int64_t)length);
	    webapimsg(baton, PMLOG_ERROR, msg);
	    continue;
	}

	for (j = 0; j < nsets; j++) {
	    if (key != labelsets[j].inst)
		continue;
	    labels = &labelsets[j];
	    length = labelsetlen(labels);
	    if (length == 0)
		continue;
	    if ((labels = labelsetdup(labels)) == NULL) {
		seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"update_instance_metadata labels", (__int64_t)length);
		webapimsg(baton, PMLOG_ERROR, msg);
		continue;
	    }
	    if (instance->labels)
		pmFreeLabelSets(instance->labels, 1);
	    instance->labels = labels;
	    break;
	}

	instance_hash(indom, instance);

	if (pmDebugOptions.series) {
	    fprintf(stderr, "Cache insert - instance: %s", instance->name.sds);
	    fprintf(stderr, "\nSHA1=%s\n", pmwebapi_hash_str(instance->name.hash));
	}
    }
}

static domain_t *new_domain(seriesLoadBaton *, int, context_t *);
static indom_t *new_indom(seriesLoadBaton *, pmInDom, domain_t *);

/*
 * Iterate over the set of metric values and extract names and labels
 * for each instance.
 */
static int
get_instance_metadata(seriesLoadBaton *baton, pmInDom indom)
{
    pmLabelSet		*labelset = NULL;
    dictEntry		*entry;
    domain_t		*dp;
    indom_t		*ip;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		indommsg[20];
    char		**namelist = NULL;
    int			*instlist = NULL;
    sds			msg;
    int			domain;
    int			sts = 0;
    int			ninst = 0;
    int			nsets = 0;

    if (indom != PM_INDOM_NULL) {
	if ((sts = pmGetInDom(indom, &instlist, &namelist)) < 0) {
	    seriesfmt(msg, "failed to get InDom %s instances: %s",
			pmInDomStr_r(indom, indommsg, sizeof(indommsg)),
			pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	    webapimsg(baton, PMLOG_ERROR, msg);
	    sts = -1;
	    goto done;
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

	domain = pmInDom_domain(indom);
	if ((entry = dictFind(baton->domains, &domain)) != NULL)
	    dp = (domain_t *)dictGetVal(entry);
	else
	    dp = new_domain(baton, domain, &baton->pmapi.context);
	if ((entry = dictFind(baton->indoms, &indom)) != NULL)
	    ip = (indom_t *)dictGetVal(entry);
	else
	    ip = new_indom(baton, indom, dp);

	update_instance_metadata(baton, ip,
				 ninst, instlist, namelist, nsets, labelset);
    }
done:
    if (labelset) pmFreeLabelSets(labelset, nsets);
    if (namelist) free(namelist);
    if (instlist) free(instlist);
    return sts;
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
	webapimsg(baton, PMLOG_ERROR, msg);
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
    if (dictAdd(baton->domains, &domain, (void *)domainp) != DICT_OK) {
	seriesfmt(msg, "failed to store domain labels (domain=%d): %s",
		domain, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	webapimsg(baton, PMLOG_WARNING, msg);
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
	webapimsg(baton, PMLOG_ERROR, msg);
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
    if (dictAdd(baton->clusters, &cluster, (void *)clusterp) != DICT_OK) {
	seriesfmt(msg, "failed to store cluster labels (cluster=%u.%u): %s",
		pmID_domain(cluster), pmID_cluster(cluster),
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	webapimsg(baton, PMLOG_WARNING, msg);
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
	webapimsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }
    indomp->indom = indom;
    indomp->domain = domain;
    indomp->insts = dictCreate(&idDict, indomp);
    if ((sts = pmGetInDomLabels(indom, &indomp->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to get indom (%s) labels: %s\n",
		    pmGetProgname(), pmInDomStr(indom),
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	/* continue on with no labels for this indom */
    }
    if (dictAdd(baton->indoms, &indom, (void *)indomp) != DICT_OK) {
	seriesfmt(msg, "failed to store indom (%s) labels", pmInDomStr(indom));
	webapimsg(baton, PMLOG_WARNING, msg);
    }
    return indomp;
}

static int
new_metric_names(seriesLoadBaton *baton, int nnames, char **allnames,
		seriesname_t **namesp)
{
    seriesname_t	*names = NULL;
    sds			msg;
    int			i;

    if (nnames == 0)
	return -ESRCH;

    if ((names = calloc(nnames, sizeof(seriesname_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " names)",
		"mapIDs", (__int64_t)nnames * sizeof(seriesname_t));
	webapimsg(baton, PMLOG_ERROR, msg);
	goto nomem;
    }
    for (i = 0; i < nnames; i++) {
	if ((names[i].sds = sdsnew(allnames[i])) == NULL)
	    goto nomem;
    }
    free(allnames);
    *namesp = names;
    return nnames;

nomem:
    if (names) {
	for (i = 0; i < nnames; i++)
	    if (names[i].sds) sdsfree(names[i].sds);
	free(names);
    }
    free(allnames);
    return -ENOMEM;
}

static metric_t *
new_metric(seriesLoadBaton *baton, pmValueSet *vsp)
{
    seriesname_t	*names;
    dictEntry		*entry;
    cluster_t		*cp;
    domain_t		*dp;
    indom_t		*ip;
    metric_t		*metric;
    pmDesc		desc;
    pmID		pmid = vsp->pmid;
    char		pmmsg[PM_MAXERRMSGLEN];
    char		**nameall = NULL;
    sds			msg;
    int			cluster, domain, count, sts, i;

    if ((metric = (metric_t *)calloc(1, sizeof(metric_t))) == NULL) {
	seriesfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"new metric", (__int64_t)sizeof(metric_t));
	webapimsg(baton, PMLOG_ERROR, msg);
	return NULL;
    }

    if ((sts = pmLookupDesc(pmid, &desc)) < 0) {
	seriesfmt(msg, "failed to lookup metric %s descriptor: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	webapimsg(baton, PMLOG_WARNING, msg);
    } else if ((sts = count = pmNameAll(pmid, &nameall)) < 0) {
	seriesfmt(msg, "failed to lookup metric %s names: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	webapimsg(baton, PMLOG_WARNING, msg);
    } else {
	/* create space needed in metric_t for maps and sds names */
	sts = new_metric_names(baton, sts, nameall, &names);
    }
    if (sts < 0) {
	free(metric);
	return NULL;
    }

    /* pick out domain#, indom# and cluster# and update label caches */
    domain = pmID_domain(pmid);
    if ((entry = dictFind(baton->domains, &domain)) != NULL)
	dp = (domain_t *)dictGetVal(entry);
    else
	dp = new_domain(baton, domain, &baton->pmapi.context);
    cluster = pmID_build(domain, pmID_cluster(pmid), 0);
    if ((entry = dictFind(baton->clusters, &cluster)) != NULL)
	cp = (cluster_t *)dictGetVal(entry);
    else
	cp = new_cluster(baton, cluster, dp);
    if (desc.indom == PM_INDOM_NULL)
	ip = NULL;
    else if ((entry = dictFind(baton->indoms, &desc.indom)) != NULL)
	ip = (indom_t *)dictGetVal(entry);
    else
	ip = new_indom(baton, desc.indom, dp);

    metric->cluster = cp;
    metric->indom = ip;
    metric->desc = desc;
    metric->names = names;
    metric->numnames = count;

    if ((sts = pmGetItemLabels(pmid, &metric->labels)) < 0) {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s: failed to get metric %s labels: %s\n",
		    pmGetProgname(), pmIDStr(pmid), pmErrStr(sts));
	/* continue on without item labels for this PMID */
    }

    metric_hash(metric);

    if (pmDebugOptions.series) {
	fprintf(stderr, "new_metric [%s] ip=%p names:",
		pmIDStr_r(pmid, pmmsg, sizeof(pmmsg)), ip);
	for (i = 0; i < count; i++)
	    fprintf(stderr, "SHA1=%s [%s]\n",
		    pmwebapi_hash_str(metric->names[i].hash),
		    metric->names[i].sds);
	if (!count)
	    fputc('\n', stderr);
    }

    return metric;
}

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

static void
clear_metric_updated(metric_t *metric)
{
    int			i, count;

    metric->updated = 0;
    if (metric->desc.indom == PM_INDOM_NULL || metric->u.vlist == NULL)
	return;
    count = metric->u.vlist->listcount;
    for (i = 0; i < count; i++)
	metric->u.vlist->value[i].updated = 0;
}

static int
new_valueset(seriesLoadBaton *baton, metric_t *metric, pmValueSet *vsp)
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

	if (metric->u.vlist == NULL) {
	    new_value(baton, metric, vp->inst, j);
	    count++;
	    k = 0;
	}
	else if (j >= metric->u.vlist->listcount) {
	    new_value(baton, metric, vp->inst, j);
	    count++;
	    k = j;
	}
	else if (vp->inst != metric->u.vlist->value[j].inst) {
	    for (k = 0; k < metric->u.vlist->listcount; k++) {
		if (vp->inst == metric->u.vlist->value[k].inst)
		    break;	/* k is now the correct offset */
	    }
	    if (k == metric->u.vlist->listcount) {    /* no matching instance */
		new_value(baton, metric, vp->inst, k);
		count++;
	    }
	} else {
	    k = j;		/* successful direct mapping */
	}
	if (baton->flags & PMFLAG_METADATA)	/* metadata only */
	    continue;
	avp = &metric->u.vlist->value[k].atom;
	pmExtractValue(vsp->valfmt, vp, type, avp, type);
    }

    if (metric->u.vlist)
	metric->u.vlist->listcount = vsp->numval;
    return count;
}

static void
series_cache_update(seriesLoadBaton *baton)
{
    seriesGetContext	*context = &baton->pmapi;
    pmResult		*result = context->result;
    pmValueSet		*vsp;
    dictEntry		*entry = NULL;
    metric_t		*metric = NULL;
    sds			timestamp;
    int			i, write_meta, write_data;

    timestamp = sdsnew(timeval_str(&result->timestamp));
    write_data = (!(baton->flags & PMFLAG_METADATA));

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
	if (dictSize(baton->wanted) && findID(baton->wanted, &vsp->pmid) == NULL)
	    continue;

	/* check if pmid already in hash list */
	if ((entry = dictFind(baton->pmids, &vsp->pmid)) == NULL) {

	    /* create a new one & add to hash */
	    if ((metric = new_metric(baton, vsp)) == NULL)
		continue;

	    if (dictAdd(baton->pmids, &vsp->pmid, (void *)metric) != DICT_OK) {
		pmidErr(baton, vsp->pmid, "failed hash table insertion\n");
		/* free memory allocated above on insert failure */
		free_metric(metric);
		continue;
	    }

	    write_meta = 1;
	} else {	/* pmid exists */
	    metric = (metric_t *)dictGetVal(entry);
	    clear_metric_updated(metric);
	    write_meta = 0;
	}

	/* iterate through result instances and ensure metric_t is complete */
	if (metric->error == 0 && vsp->numval < 0)
	    write_meta = 1;
	if (new_valueset(baton, metric, vsp) != 0)
	    write_meta = 1;

	/* record the error code in the cache */
	metric->error = (vsp->numval < 0) ? vsp->numval : 0;

	/* make PMAPI calls to cache metadata */
	if (write_meta && get_instance_metadata(baton, metric->desc.indom) != 0)
	    continue;

	/* initiate writes to backend caching servers (Redis) */
	seriesBatonReference(context, "series_cache_update[metric]");
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
	seriesfmt(msg, "pmSetMode failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	webapimsg(baton, PMLOG_ERROR, msg);
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

    /* drop context reference taken in server_cache_window */
    doneSeriesGetContext(context, "server_cache_series_finished");

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

    /* drop reference taken in server_cache_window */
    doneSeriesGetContext(context, "server_cache_update_done");

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
	    seriesBatonReference(context, "server_cache_window");
	    context->done = server_cache_update_done;
	    series_cache_update(baton);
	}
	else {
	    sts = PM_ERR_EOL;
	    pmFreeResult(result);
	    context->result = NULL;
	}
    }

    if (sts < 0) {
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
	webapimsg(baton, PMLOG_WARNING, msg);
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
new_pmapi_context(seriesLoadBaton *baton)
{
    context_t		*cp = &baton->pmapi.context;
    char		labels[PM_MAXLABELJSONLEN];
    char		pmmsg[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    /* establish PMAPI context */
    if ((sts = cp->context = pmNewContext(cp->type, cp->name.sds)) < 0) {
	if (cp->type == PM_CONTEXT_HOST)
	    seriesfmt(msg, "cannot connect to PMCD on host \"%s\": %s",
		    cp->name.sds, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else if (cp->type == PM_CONTEXT_LOCAL)
	    seriesfmt(msg, "cannot make standalone connection on localhost: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else
	    seriesfmt(msg, "cannot open archive \"%s\": %s",
		    cp->name.sds, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_meta(cp, labels, sizeof(labels))) < 0) {
	seriesfmt(msg, "failed to get context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else if ((sts = pmwebapi_source_hash(cp->name.hash, labels, sts)) < 0) {
	seriesfmt(msg, "failed to merge context labels: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
    } else {
	if (pmDebugOptions.series) {
	    fprintf(stderr, "new_pmapi_context: SHA1=%s [%s]\n",
			    pmwebapi_hash_str(cp->name.hash), cp->name.sds);
	}
	return /*success*/;
    }

    webapimsg(baton, PMLOG_ERROR, msg);
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

void
source_mapping_callback(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "source_mapping_callback");
    seriesPassBaton(&baton->current, baton, "source_mapping_callback");
}

void
series_source_mapping(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;
    context_t		*context = &baton->pmapi.context;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "series_source_mapping");
    seriesBatonCheckCount(baton, "series_source_mapping");
    seriesBatonReference(baton, "series_source_mapping");

    if (context->name.mapid > 0 && context->hostid > 0) {
	/* fast path - string maps are already resolved */
	seriesPassBaton(&baton->current, baton, "series_source_mapping");
    } else {
	if (context->name.mapid <= 0) {
	    seriesBatonReference(baton, "series_source_mapping mapid");
	    redisGetMap(baton->slots, contextmap, context->name.sds,
			&context->name.mapid, source_mapping_callback,
			baton->settings->command.on_info, baton->arg,
			(void *)baton);
	}
	if (context->hostid <= 0) {
	    seriesBatonReference(baton, "series_source_mapping hostid");
	    redisGetMap(baton->slots, contextmap, context->host,
			&context->hostid, source_mapping_callback,
			baton->settings->command.on_info, baton->arg,
			(void *)baton);
	}
	seriesBatonDereference(baton, "series_source_mapping");
    }
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

    pmDestroyContext(cp->context);
    if (cp->name.sds)
	sdsfree(cp->name.sds);
    if (cp->origin)
	sdsfree(cp->origin);
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

    if (seriesBatonDereference(context, caller))
	context->done(baton);

    if (context->error) {
	char		pmmsg[PM_MAXERRMSGLEN];
	sds		msg;

	if (context->error == PM_ERR_EOF) {
	    seriesfmt(msg, "processed %llu archive records from %s",
			context->count, context->context.name.sds);
	    webapimsg(baton, PMLOG_INFO, msg);
	} else {
	    seriesfmt(msg, "fetch failed: %s",
			pmErrStr_r(context->error, pmmsg, sizeof(pmmsg)));
	    webapimsg(baton, PMLOG_ERROR, msg);
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

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "connect_pmapi_source_service");

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
    pmSeriesCommand	*command = &baton->settings->command;

    /* attempt to re-use existing slots connections */
    if (command->slots) {
	baton->slots = command->slots;
	series_load_end_phase(baton);
    } else {
	baton->slots = command->slots =
	    redisSlotsConnect(
		command->hostspec, 1, command->on_info,
		series_load_end_phase, baton->userdata,
		command->events, (void *)baton);
    }
}

static void
setup_source_services(void *arg)
{
    seriesLoadBaton	*baton = (seriesLoadBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "setup_source_services");
    seriesBatonReferences(baton, 2, "setup_source_services");
    initSeriesGetContext(&baton->pmapi, baton);

    connect_pmapi_source_service(baton);
    connect_redis_source_service(baton);
}

void
initSeriesLoadBaton(seriesLoadBaton *baton,
		pmSeriesSettings *settings, pmflags flags, void *userdata)
{
    initSeriesBatonMagic(baton, MAGIC_LOAD);
    baton->settings = settings;
    baton->userdata = userdata;
    baton->flags = flags;

    baton->clusters = dictCreate(&idDict, baton);
    baton->domains = dictCreate(&idDict, baton);
    baton->indoms = dictCreate(&idDict, baton);
    baton->pmids = dictCreate(&idDict, baton);
    baton->errors = dictCreate(&idDict, baton);
    baton->wanted = dictCreate(&idDict, baton);
}

void
freeSeriesLoadBaton(seriesLoadBaton *baton)
{
    pmSeriesSettings	*settings;

    seriesBatonCheckMagic(baton, MAGIC_LOAD, "freeSeriesLoadBaton");
    freeSeriesGetContext(&baton->pmapi, 0);
    settings = baton->settings;
    if (settings->on_done)
	settings->on_done(baton->error, baton->userdata);

    dictRelease(baton->clusters);
    dictRelease(baton->domains);
    dictRelease(baton->indoms);
    dictRelease(baton->pmids);
    dictRelease(baton->errors);
    dictRelease(baton->wanted);

    memset(baton, 0, sizeof(*baton));
    free(baton);
}

void
doneSeriesLoadBaton(seriesLoadBaton *baton, const char *caller)
{
    seriesPassBaton(&baton->current, baton, caller);
}

redisInfoCallBack
seriesLoadBatonInfo(seriesLoadBaton *baton)
{
    return baton->settings->command.on_info;
}

context_t *
seriesLoadBatonContext(seriesLoadBaton *baton)
{
    return &baton->pmapi.context;
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
	webapimsg(baton, PMLOG_ERROR, msg);
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
