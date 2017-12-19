/*
 * Copyright (c) 2017 Red Hat.
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
#include "redis.h"
#include "load.h"
#include "util.h"
#include "sha1.h"

#include "libpcp.h"

typedef struct {
    redisContext	*redis;

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

#define ERRSIZE		PM_MAXERRMSGLEN
#define MSGSIZE		(ERRSIZE + 128)
#define loadmsg(SP, level, message)		\
	((SP)->settings->on_info((level), (message), (SP)->arg))

static void
series_cache_addvalue(SOURCE *sp, metric_t *metric, value_t *value)
{
    redis_series_addvalue(sp->redis, metric, value);
}

static void
series_cache_metadata(SOURCE *sp, metric_t *metric, value_t *value)
{
    redis_series_metadata(sp->redis, metric, value);
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
    char		*hname;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    pmID		pmid;
    int			sts;

    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0) {
	pmsprintf(msg, sizeof(msg), 
		"failed to lookup metric name (pmid=%s): %s",
		name, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
    } else if ((hname = strdup(name)) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
		"cache metric name", (long long)strlen(name)+1);
	loadmsg(sp, PMSERIES_ERROR, msg);
    } else {
	if (sp->verbose || pmDebugOptions.series)
	    fprintf(stderr, "cache_prepare: caching PMID=%s name=%s\n",
			pmIDStr(pmid), hname);
	__pmHashAdd(pmid, hname, &sp->wanthash);
    }
}

static void
cache_value_metadata(SOURCE *sp, metric_t *metric, value_t *value)
{
    pmDesc		*desc = &metric->desc;
    pmID		pmid = desc->pmid;
    char		identifier[BUFSIZ+PM_MAXLABELJSONLEN];
    unsigned char	hash[20];
    SHA1_CTX		shactx;
    int			nbytes, off;

    nbytes = (desc->indom == PM_INDOM_NULL) ?
	pmsprintf(identifier, sizeof(identifier), "{"
		"\"desc\":{\"domain\":%u,\"cluster\":%u,\"item\":%u,"
		    "\"semantics\":%u,\"type\":%u,\"units\":%u},"
		"\"inst\":{\"id\":0,\"name\":null},"
		"\"label\":%s}",
		pmID_domain(pmid), pmID_cluster(pmid), pmID_item(pmid),
		desc->sem, desc->type, *(unsigned int *)&desc->units,
		value_labels(metric, value))
	:
	pmsprintf(identifier, sizeof(identifier), "{"
		"\"desc\":{\"domain\":%u,\"cluster\":%u,\"item\":%u,"
		    "\"semantics\":%u,\"serial\":%u,\"type\":%u,\"units\":%u},"
		"\"inst\":{\"id\":%u,\"name\":%s},"
		"\"label\":%s}",
		pmID_domain(pmid), pmID_cluster(pmid), pmID_item(pmid),
		desc->sem, pmInDom_serial(desc->indom), desc->type,
		*(unsigned int *)&desc->units, value_instid(value),
		value_instname(value), value_labels(metric, value));

    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)identifier, nbytes);
    SHA1Final(hash, &shactx);

    for (nbytes = off = 0; nbytes < sizeof(hash); nbytes++)
	off += pmsprintf(value->hash+off, sizeof(value->hash)-off, "%02x",
			hash[nbytes]);
    value->hash[PMSIDSZ] = '\0';

    if (sp->verbose || pmDebugOptions.series) {
	fprintf(stderr, "Cache insert:\nNAME(S): ");
	__pmPrintMetricNames(stderr, metric->numnames, metric->names, " or ");
	fprintf(stderr, "\nSHA1=%.*s\n%s\n", PMSIDSZ, value->hash, identifier);
    }

    series_cache_metadata(sp, metric, value);
}

static double
unwrap(double current, double previous, int pmtype)
{
    double	outval = current;
    static int	dowrap = -1;

    if ((current - previous) < 0.0) {
	if (dowrap == -1) {
	    /* PCP_COUNTER_WRAP in environment enables "counter wrap" logic */
	    if (getenv("PCP_COUNTER_WRAP") == NULL)
		dowrap = 0;
	    else
		dowrap = 1;
	}
	if (dowrap) {
	    switch (pmtype) {
		case PM_TYPE_32:
		case PM_TYPE_U32:
		    outval += (double)UINT_MAX+1;
		    break;
		case PM_TYPE_64:
		case PM_TYPE_U64:
		    outval += (double)ULONGLONG_MAX+1;
		    break;
	    }
	}
    }

    return outval;
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
    pmLabelSet	*dup;
    char	*json;

    if ((dup = calloc(1, sizeof(pmLabelSet))) == NULL)
	return NULL;
    *dup = *lp;
    if ((json = strdup(lp->json)) == NULL) {
	free(dup);
	return NULL;
    }
    if (lp->nlabels <= 0)
	return dup;

    if ((dup->labels = calloc(lp->nlabels, sizeof(pmLabel))) == NULL) {
	free(dup);
	free(json);
	return NULL;
    }
    memcpy(dup->labels, lp->labels, sizeof(pmLabel) * lp->nlabels);
    dup->json = json;
    return dup;
}

/*
 * Iterate over each value associated with this metric, and complete
 * the metadata (instance name, labels) associated with each.  Cache
 * values that have not been explicitly restricted via command line.
 */
static void
cache_metric_metadata(SOURCE *sp, metric_t *metric,
	int ninst, int *instlist, char **namelist,
	int nsets, pmLabelSet *labelsets)
{
    pmLabelSet		*labels;
    value_t		*value;
    size_t		length;
    char		msg[MSGSIZE];
    char		*name;
    int			i, j;

    for (i = 0; i < metric->listsize; i++) {
	value = metric->vlist[i];

	for (j = 0; j < ninst; j++) {
	    if (value->inst != instlist[j])
		continue;
	    name = namelist[j];
	    length = strlen(name) + 1;
	    if ((name = strndup(name, length)) == NULL) {
	        pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
			"cache_metric_metadata name", (long long)length);
		loadmsg(sp, PMSERIES_ERROR, msg);
		continue;
	    }
	    if (value->name) free(value->name);
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
		pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
			"cache_metric_metadata labels", (long long)length);
		loadmsg(sp, PMSERIES_ERROR, msg);
		continue;
	    }
	    if (value->labels) pmFreeLabelSets(value->labels, 1);
	    value->labels = labels;
	    break;
	}

	if (value->cached == 0) {
	    value->cached = 1;
	    cache_value_metadata(sp, metric, value);
	}
    }
}

/*
 * Iterate over the set of metric values and extract names and labels
 * for each instance.  Finally cache metadata once its all available.
 */
static void
new_values_names(SOURCE *sp, metric_t *metric)
{
    char		**namelist = NULL;
    int			*instlist = NULL;
    int			sts;
    int			ninst = 0;
    int			nsets = 0;
    char		indommsg[20];
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    pmInDom		indom = metric->desc.indom;
    pmLabelSet		*labelset = NULL;

    if (indom != PM_INDOM_NULL) {
	if ((sts = pmGetInDom(indom, &instlist, &namelist)) < 0) {
	    pmsprintf(msg, sizeof(msg), "failed to get InDom %s instances: %s",
			pmInDomStr_r(indom, indommsg, sizeof(indommsg)),
			pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	    return;
	}
	ninst = sts;
	if ((sts = pmGetInstancesLabels(indom, &labelset)) < 0) {
	    if (sp->verbose)
		fprintf(stderr, "%s: failed to get PMID %s labels: %s\n",
			pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	    /* continue on with no labels for this value */
	    sts = 0;
	}
	nsets = sts;
    }

    /* insert metadata into the cache for all values of this metric */
    cache_metric_metadata(sp, metric, ninst, instlist, namelist, nsets, labelset);

    if (labelset) pmFreeLabelSets(labelset, nsets);
    if (namelist) free(namelist);
    if (instlist) free(instlist);
}

static int
new_value(SOURCE	*sp,
	pmValue		*vp,
	struct metric	*metric,	/* updated by this function */
	int		 valfmt,
	struct timeval	*timestamp,	/* timestamp for this sample */
	int		 pos)		/* position of this inst in vlist */
{
    int			sts;
    int			type = metric->desc.type;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    value_t		*value, **vpp;
    size_t		size;
    pmAtomValue		av;

    if ((sts = pmExtractValue(valfmt, vp, type, &av, metric->outype)) < 0) {
	pmiderr(sp, metric->desc.pmid, "failed to extract value: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	pmsprintf(msg, sizeof(msg), "possibly corrupt archive %s",
		sp->context.source);
	loadmsg(sp, PMSERIES_CORRUPT, msg);
	return -ESRCH;
    }
    size = (pos + 1) * sizeof(value_t *);
    if ((vpp = (value_t **)realloc(metric->vlist, size)) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
			"new value vlist", (long long)size);
	loadmsg(sp, PMSERIES_ERROR, msg);
	free(metric->vlist);
	metric->vlist = NULL;
	return -ENOMEM;
    }
    metric->vlist = vpp;
    size = sizeof(value_t);
    metric->vlist[pos] = value = (value_t *)calloc(1, size);
    if (value == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
			"new value vlist[inst]", (long long)size);
	loadmsg(sp, PMSERIES_ERROR, msg);
	free(metric->vlist);
	metric->vlist = NULL;
	return -ENOMEM;
    }
    value->inst = vp->inst;
    if (metric->desc.sem != PM_SEM_COUNTER)
	value->count = 1;
    value->lastval = av;
    value->firsttime = *timestamp;
    value->lasttime = *timestamp;
    metric->listsize++;
    return 0;
}

static domain_t *
new_domain(SOURCE *sp, int domain, context_t *context)
{
    domain_t		*domainp;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    int			sts;

    if ((domainp = calloc(1, sizeof(domain_t))) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
		"new domain", (long long)sizeof(domain_t));
	loadmsg(sp, PMSERIES_ERROR, msg);
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
	pmsprintf(msg, sizeof(msg), 
		"failed to store domain labels (domain=%d): %s",
		domain, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
    }
    return domainp;
}

static cluster_t *
new_cluster(SOURCE *sp, int cluster, domain_t *domain)
{
    cluster_t		*clusterp;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    int			sts;

    if ((clusterp = calloc(1, sizeof(cluster_t))) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
		"new cluster", (long long)sizeof(cluster_t));
	loadmsg(sp, PMSERIES_ERROR, msg);
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
	pmsprintf(msg, sizeof(msg), 
		"failed to store cluster labels (cluster=%u.%u): %s",
		pmID_domain(cluster), pmID_cluster(cluster),
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
    }
    return clusterp;
}

static indom_t *
new_indom(SOURCE *sp, pmInDom indom, domain_t *domain)
{
    indom_t		*indomp;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    int			sts;

    if ((indomp = calloc(1, sizeof(indom_t))) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
		"new indom", (long long)sizeof(indom_t));
	loadmsg(sp, PMSERIES_ERROR, msg);
	return NULL;
    }
    indomp->indom = indom;
    indomp->domain = domain;
    if ((sts = pmGetInDomLabels(indom, &indomp->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr, "%s: failed to get indom (%s) labels: %s\n",
		    pmGetProgname(), pmInDomStr(indom), pmErrStr(sts));
	/* continue on with no labels for this indom */
    }
    if (__pmHashAdd(indom, (void *)indomp, &sp->indomhash) < 0) {
	pmsprintf(msg, sizeof(msg), "failed to store indom (%s) labels: %s",
		pmInDomStr(indom), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
    }
    return indomp;
}

static metric_t *
new_metric(SOURCE	*sp,
	pmValueSet	*vsp,
	pmDesc		*desc,
	struct timeval	*timestamp)	/* timestamp for this sample */
{
    __pmHashNode	*hptr;
    cluster_t		*cp;
    domain_t		*dp;
    indom_t		*ip;
    metric_t		*metric;
    double		powerof;
    pmID		pmid = desc->pmid;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    char		**names;
    int			*mapids;
    int			cluster, domain, sts, i;

    if ((metric = (metric_t *)calloc(1, sizeof(metric_t))) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
			"new metric", (long long)sizeof(metric_t));
	loadmsg(sp, PMSERIES_ERROR, msg);
	return NULL;
    }
    metric->desc = *desc;
    if (desc->sem != PM_SEM_COUNTER)
	metric->outype = desc->type;
    else
	metric->outype = PM_TYPE_DOUBLE;

    if ((sts = pmNameAll(pmid, &names)) < 0) {
	pmsprintf(msg, sizeof(msg), "failed to lookup metric %s names: %s",
		pmIDStr(pmid), pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
    } else if ((mapids = calloc(sts, sizeof(int))) == NULL) {
	pmsprintf(msg, sizeof(msg), "out of memory (%s, %lld bytes)",
		"mapids", (long long)sts * sizeof(int));
	loadmsg(sp, PMSERIES_ERROR, msg);
	sts = -ENOMEM;
    }
    if (sts <= 0) {
	free(metric);
	return NULL;
    }
    metric->names = names;
    metric->mapids = mapids;
    metric->numnames = sts;

    if (pmDebugOptions.appl0) {
	fprintf(stderr, "Metric [%s] ", pmIDStr(pmid));
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
    if (desc->indom == PM_INDOM_NULL)
	ip = NULL;
    else if ((hptr = __pmHashSearch(desc->indom, &sp->indomhash)) != NULL)
	ip = (indom_t *)hptr->data;
    else
	ip = new_indom(sp, desc->indom, dp);

    metric->cluster = cp;
    metric->indom = ip;

    if ((sts = pmGetItemLabels(pmid, &metric->labels)) < 0) {
	if (sp->verbose)
	    fprintf(stderr, "%s: failed to get metric %s labels: %s\n",
		    pmGetProgname(), pmIDStr(pmid), pmErrStr(sts));
	/* continue on with no labels for this PMID */
    }

    /* convert counter metric units to rate units & get time scale */
    if (desc->sem == PM_SEM_COUNTER) {
	if (desc->units.dimTime == 0)
	    metric->scale = 1.0;
	else {
	    if (desc->units.scaleTime > PM_TIME_SEC) {
		powerof = (double)(PM_TIME_SEC - desc->units.scaleTime);
		metric->scale = pow(60.0, powerof);
	    } else {
		powerof = (double)(PM_TIME_SEC - desc->units.scaleTime);
		metric->scale = pow(1000.0, powerof);
	    }
	}
	if (desc->units.dimTime == 0)
	    metric->desc.units.scaleTime = PM_TIME_SEC;
	metric->desc.units.dimTime--;
    }
    for (i = 0; i < vsp->numval; i++)
	new_value(sp, &vsp->vlist[i], metric, vsp->valfmt, timestamp, i);
    new_values_names(sp, metric);

    return metric;
}

/*
 * must keep a note for every value for every metric whenever a mark
 * record has been seen between now & the last fetch for that value
 */
static void
markrecord(SOURCE *sp, pmResult *result)
{
    int			i, j;
    __pmHashNode	*hptr;
    value_t		*value;
    metric_t		*metric;
    struct timeval	timediff;

    if (pmDebugOptions.appl0) {
	fputstamp(&result->timestamp, '\n', stderr);
	fprintf(stderr, " - mark record\n\n");
    }
    for (i = 0; i < sp->pmidhash.hsize; i++) {
	for (hptr = sp->pmidhash.hash[i]; hptr != NULL; hptr = hptr->next) {
	    metric = (metric_t *)hptr->data;
	    for (j = 0; j < metric->listsize; j++) {
		value = metric->vlist[j];
		if (metric->desc.sem == PM_SEM_DISCRETE) {
		    /* extend discrete metrics to the mark point */
		    timediff = result->timestamp;
		    tsub(&timediff, &value->lasttime);
		    value->lasttime = result->timestamp;
		    value->count++;
		}
		value->marked = 1;
		value->markcount++;
	    }
	}
    }
}

static void
free_metric(metric_t *metric)
{
    value_t		*value;
    int			i;

    for (i = 0; i < metric->listsize; i++) {
	value = metric->vlist[i];
	if (value->name) free(value->name);
	if (value) free(value);
    }
    if (metric->vlist) free(metric->vlist);
    if (metric->names) free(metric->names);
    free(metric);
}

static void
series_cache_update(SOURCE *sp, pmResult *result, pmseries_flags flags)
{
    int			i, j, k;
    int			sts;
    int			wrap;
    int			refresh;
    double		val;
    pmDesc		desc;
    pmAtomValue 	av;
    pmValue		*vp;
    pmValueSet		*vsp;
    __pmHashNode	*hptr = NULL;
    metric_t		*metric = NULL;
    value_t		*value;
    double		diff;
    double		rate = 0;
    struct timeval	timediff;

    if (result->numpmid == 0)	/* mark record */
	markrecord(sp, result);
    else
	pmSortInstances(result);

    for (i = 0; i < result->numpmid; i++) {
	vsp = result->vset[i];
	if (vsp->numval == 0)
	    continue;
	if (vsp->numval < 0) {
	    pmiderr(sp, vsp->pmid, "failed in archive value fetch: %s\n",
		    pmErrStr(vsp->numval));
	    continue;
	}

	/* check if in the restricted group (command line optional) */
	if (sp->wanthash.nodes &&
	    __pmHashSearch(vsp->pmid, &sp->wanthash) == NULL)
	    continue;

	/* check if pmid already in hash list */
	if ((hptr = __pmHashSearch(vsp->pmid, &sp->pmidhash)) == NULL) {
	    if ((sts = pmLookupDesc(vsp->pmid, &desc)) < 0) {
		pmiderr(sp, vsp->pmid, "cannot find descriptor: %s\n",
			pmErrStr(sts));
		continue;
	    }

	    // TODO: support non-numeric types
	    if (desc.type != PM_TYPE_32 && desc.type != PM_TYPE_U32 &&
		desc.type != PM_TYPE_64 && desc.type != PM_TYPE_U64 &&
		desc.type != PM_TYPE_FLOAT && desc.type != PM_TYPE_DOUBLE) {
		pmiderr(sp, vsp->pmid, "non-numeric metrics ignored for now\n");
		continue;
	    }

	    /* create a new one & add to list */
	    if ((metric = new_metric(sp, vsp, &desc, &result->timestamp)) == NULL)
		continue;	/* out of memory */

	    if (__pmHashAdd(metric->desc.pmid, (void *)metric, &sp->pmidhash) < 0) {
		pmiderr(sp, metric->desc.pmid, "failed hash table insertion\n",
			pmGetProgname());
		/* free memory allocated above on insert failure */
		free_metric(metric);
		continue;
	    }
	} else {	/* pmid exists */
	    metric = (metric_t *)hptr->data;
	}

	/* iterate through result instances and ensure metric_t is complete */
	refresh = 0;
	for (j = 0; j < vsp->numval; j++) {
	    vp = &vsp->vlist[j];
	    k = j;	/* index into stored inst list, result may differ */
	    if ((vsp->numval > 1) || (metric->desc.indom != PM_INDOM_NULL)) {
		if ((k < metric->listsize) &&
		    (metric->vlist[k]->inst != vp->inst)) {
		    for (k = 0; k < metric->listsize; k++) {
			if (vp->inst == metric->vlist[k]->inst)
			    break;	/* k now correct */
		    }
		    if (k == metric->listsize) {    /* no matching inst found */
			new_value(sp, vp, metric, vsp->valfmt, &result->timestamp, k);
			refresh++;
			continue;
		    }
		}
		else if (k >= metric->listsize) {
		    k = metric->listsize;
		    new_value(sp, vp, metric, vsp->valfmt, &result->timestamp, k);
		    refresh++;
		    continue;
		}
	    }
	}
	if (refresh)
	    new_values_names(sp, metric);

	/* iterate through result values now and insert into the cache */
	for (j = 0; j < vsp->numval; j++) {
	    vp = &vsp->vlist[j];
	    k = j;	/* index into stored inst list, result may differ */
	    if ((vsp->numval > 1) || (metric->desc.indom != PM_INDOM_NULL)) {
		assert(k < metric->listsize);
		if (metric->vlist[k]->inst != vp->inst) {
		    for (k = 0; k < metric->listsize; k++) {
			if (vp->inst == metric->vlist[k]->inst)
			    break;	/* k now correct */
		    }
		    assert(k != metric->listsize);    /* no matching inst found */
		}
	    }
	    value = metric->vlist[k];

	    if ((sts = pmExtractValue(vsp->valfmt, vp, metric->desc.type,
					&av, metric->outype)) < 0)
		continue;

	    timediff = result->timestamp;
	    tsub(&timediff, &value->lasttime);
	    diff = pmtimevalToReal(&timediff);
	    wrap = 0;

	    if (metric->desc.sem == PM_SEM_COUNTER) {
		diff *= metric->scale;
		if (diff == 0.0)
		    continue;
		if (value->marked)
		    val = av.d;
		else
		    val = unwrap(av.d, value->lastval.d, metric->desc.type);
		if (pmDebugOptions.appl0) {
		    __pmPrintMetricNames(stderr,
				metric->numnames, metric->names, " or ");
		    fprintf(stderr, " base value is %f, count %d\n",
				val, value->count + 1);
		}
		if (value->marked || val < value->lastval.d) {
		    /* either previous record was a "mark", or this is not */
		    /* the first one, and counter not monotonic increasing */
		    if (pmDebugOptions.appl1) {
			__pmPrintMetricNames(stderr,
				metric->numnames, metric->names, " or ");
			fprintf(stderr, " counter wrapped or <mark>\n");
		    }
		    wrap = 1;
		    value->marked = 0;
		    tadd(&value->firsttime, &result->timestamp);
		    tsub(&value->firsttime, &value->lasttime);
		}
		else {
		    rate = (val - value->lastval.d) / diff;
		    if (value->marked) {
			value->marked = 0;
			/* remove timeslice in question from time-based calc */
			tadd(&value->firsttime, &result->timestamp);
			tsub(&value->firsttime, &value->lasttime);
		    }
		    
		}
	    }
	    else {	/* for the other semantics - discrete & instantaneous */
		if (value->marked) {
		    value->marked = 0;
		    /* remove the timeslice in question from time-based calc */
		    tadd(&value->firsttime, &result->timestamp);
		    tsub(&value->firsttime, &value->lasttime);
		}
	    }
	    if (!wrap) {
		value->count++;
		if ((pmDebugOptions.appl1) &&
		    ((metric->desc.sem != PM_SEM_COUNTER) ||
		     (value->count > 0))) {
		    struct timeval	metrictimespan;

		    metrictimespan = result->timestamp;
		    tsub(&metrictimespan, &value->firsttime);
		    fprintf(stderr, "++ ");
		    __pmPrintMetricNames(stderr,
				metric->numnames, metric->names, " or ");
		    if (metric->desc.sem == PM_SEM_COUNTER) {
			fprintf(stderr, " timedelta=%f count=%d rate=%f\n",
				diff, value->count, rate);
		    }
		    else {	/* non-counters */
			fprintf(stderr, " timedelta=%f count=%d val=?\n",
				diff, value->count);
		    }
		}
	    }
	    value->lastval = av;
	    value->lasttime = result->timestamp;

	    if (!(flags & PMSERIES_METADATA))
		series_cache_addvalue(sp, metric, value);
	}
    }
}

static int
series_cache_load(SOURCE *sp, timing_t *tp, pmseries_flags flags)
{
    struct timeval	*finish = &tp->end;
    pmResult		*result;
    char		msg[MSGSIZE];
    char		pmmsg[ERRSIZE];
    int			sts, count = 0;

    if ((sts = pmSetMode(PM_MODE_FORW, &tp->start, 0)) < 0) {
	pmsprintf(msg, sizeof(msg), "pmSetMode failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_ERROR, msg);
	return sts;
    }

    /* TODO: support the other context types */
    if (sp->context.type != PM_CONTEXT_ARCHIVE)
	return -ENOTSUP;

    /* TODO: support fetch interpolation (if tp->delta) */
    /* TODO: support a tail-f-mode of operation as well */

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

    pmsprintf(msg, sizeof(msg), "processed %d archive records from %s",
		count, sp->context.source);
    loadmsg(sp, PMSERIES_INFO, msg);

    if (sts == PM_ERR_EOL)
	sts = 0;
    else {
	pmsprintf(msg, sizeof(msg), "fetch failed: %s",
		pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_ERROR, msg);
	sts = 1;
    }
    return sts;
}

static int
default_labelset(int ctx, pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    char	host[MAXHOSTNAMELEN];
    int		sts;

    if ((pmGetContextHostName_r(ctx, host, sizeof(host))) == NULL)
	return PM_ERR_GENERIC;
    pmsprintf(buf, sizeof(buf), "{\"hostname\":\"%s\"}", host);
    if ((sts = __pmAddLabels(&lp, buf, PM_LABEL_CONTEXT)) > 0) {
	*sets = lp;
	return 0;
    }
    return sts;
}

static void
set_context_source(SOURCE *sp, const char *source)
{
    sp->context.source = source;
}

static void
set_context_type(SOURCE *sp, const char *name)
{
    if (strcmp(name, "source.local") == 0)
	sp->context.type = PM_CONTEXT_LOCAL;
    else if (strcmp(name, "source.archive") == 0)
	sp->context.type = PM_CONTEXT_ARCHIVE;
    else if (strcmp(name, "source.hostspec") == 0)
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
    char	msg[MSGSIZE];
    char	pmmsg[ERRSIZE];
    int		i, sts, errors = 0;

    for (i = 0; i < sp->context.nmetrics; i++) {
	if ((sts = pmTraversePMNS_r(metrics[i], cache_prepare, sp)) >= 0)
	    continue;
	pmsprintf(msg, sizeof(msg), "PMNS traversal failed for %s: %s",
			metrics[i], pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_WARNING, msg);
	errors++;
    }
    return (errors && errors == sp->context.nmetrics) ? -ESRCH : 0;
}

static int
load_prepare_timing(SOURCE *sp, timing_t *tp)
{
    /* TODO - handle timezones and so on correctly */
    return 0;
}

static void
load_prepare_source(SOURCE *sp, node_t *np, int level)
{
    int		length, subtype;
    char	*name;

    if (np == NULL)
	return;

    /* descend to the leaves first */
    load_prepare_source(sp, np->left, level+1);
    load_prepare_source(sp, np->right, level+1);

    switch (np->type) {
    case N_NAME:
	length = strlen(np->value);
	if ((name = series_instance_name(np->value, length)) != NULL)
	    np->subtype = N_INSTANCE;
	else if ((name = series_metric_name(np->value, length)) != NULL)
	    np->subtype = N_METRIC;
	else {
	    if ((name = series_label_name(np->value, length)) == NULL)
		name = np->value;
	    np->subtype = N_LABEL;
	}
	set_context_type(sp, name);
	break;

    case N_EQ:
	if (np->right->type != N_STRING)
	    break;
	if (np->left->type == N_NAME) {
	    subtype = np->left->subtype;
	    if (subtype == N_LABEL)
		 set_context_source(sp, np->right->value);
	}
	if (np->left->type == N_METRIC)
	    add_source_metric(sp, np->right->value);
	break;
    }
}

static int
load_resolve_source(SOURCE *sp)
{
    context_t	*cp = &sp->context;
    char	pmmsg[ERRSIZE];
    char	msg[MSGSIZE];
    int		sts;

    if ((sts = pmNewContext(cp->type, cp->source)) < 0) {
	if (cp->type == PM_CONTEXT_HOST)
	    pmsprintf(msg, sizeof(msg), 
		    "cannot connect to PMCD on host \"%s\": %s",
		    cp->source, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else if (sp->context.type == PM_CONTEXT_LOCAL)
	    pmsprintf(msg, sizeof(msg), 
		    "cannot make standalone connection on localhost: %s",
		    pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	else
	    pmsprintf(msg, sizeof(msg), "cannot open archive \"%s\": %s",
		    cp->source, pmErrStr_r(sts, pmmsg, sizeof(pmmsg)));
	loadmsg(sp, PMSERIES_ERROR, msg);
	sts = -ESRCH;
    } else {
	cp->context = sts;

	if ((sts = pmGetContextLabels(&cp->labels)) < 0 &&
	    (default_labelset(cp->context, &cp->labels) < 0)) {
	    pmsprintf(msg, sizeof(msg), "failed to get context labels: %s",
		    pmErrStr(sts));
	    loadmsg(sp, PMSERIES_ERROR, msg);
	    sts = -ESRCH;
	}
    }
    return sts;
}

void
load_destroy_source(SOURCE *sp)
{
    context_t	*cp = &sp->context;

    pmDestroyContext(cp->context);
    cp->context = -1;
}

int
series_source(pmSeriesSettings *settings,
	node_t *root, timing_t *timing, pmseries_flags flags, void *arg)
{
    SOURCE	source = { .settings = settings, .arg = arg };
    char	msg[MSGSIZE];
    int		sts;

    source.redis = redis_init();

    load_prepare_source(&source, root, 0);
    if (!source.context.type) {
	pmsprintf(msg, sizeof(msg), "found no context to load");
	loadmsg(&source, PMSERIES_ERROR, msg);
	return -ESRCH;
    }
    if ((sts = load_resolve_source(&source)) < 0)
	return sts;

    /* metric and time-based filtering */
    if ((sts = load_prepare_metrics(&source)) < 0 ||
	(sts = load_prepare_timing(&source, timing)) < 0) {
	load_destroy_source(&source);
	return sts;
    }

    series_cache_load(&source, timing, flags);
    return 0;
}
