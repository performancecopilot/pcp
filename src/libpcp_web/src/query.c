/*
 * Copyright (c) 2017-2022 Red Hat.
 * Copyright (c) 2020 Yushan ZHANG.
 * Copyright (c) 2022 Shiyao CHEN.
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

#include <assert.h>
#include <ctype.h>
#include "pmapi.h"
#include "libpcp.h"
#include "util.h"
#include "sha1.h"
#include "query.h"
#include "schema.h"
#include "slots.h"
#include "maps.h"
#include <math.h>
#include <fnmatch.h>

#define SHA1SZ		20	/* internal sha1 hash buffer size in bytes */
#define QUERY_PHASES	8


typedef struct seriesGetLabelMap {
    seriesBatonMagic	header;		/* MAGIC_LABELMAP */
    redisMap		*map;
    sds			series;
    sds			name;
    sds			mapID;
    sds			mapKey;
    void		*baton;
} seriesGetLabelMap;

typedef struct seriesGetLookup {
    redisMap		*map;
    pmSeriesStringCallBack func;
    sds			pattern;
    unsigned int	nseries;
    seriesGetSID	series[0];
} seriesGetLookup;

typedef struct seriesGetQuery {
    node_t		root;
    timing_t		timing;
} seriesGetQuery;

typedef struct seriesQueryBaton {
    seriesBatonMagic	header;		/* MAGIC_QUERY */
    seriesBatonPhase	*current;
    seriesBatonPhase	phases[QUERY_PHASES];
    pmSeriesModule	*module;
    pmSeriesCallBacks	*callbacks;
    pmLogInfoCallBack	info;
    void		*userdata;
    redisSlots          *slots;
    int			error;
    union {
	seriesGetLookup	lookup;
	seriesGetQuery	query;
    } u;
} seriesQueryBaton;

static void series_pattern_match(seriesQueryBaton *, node_t *);
static int series_union(series_set_t *, series_set_t *);
static int series_intersect(series_set_t *, series_set_t *);
static int series_calculate(seriesQueryBaton *, node_t *, int);
static void series_redis_hash_expression(seriesQueryBaton *, char *, int);
static void series_node_get_metric_name(seriesQueryBaton *, seriesGetSID *, series_sample_set_t *);
static void series_node_get_desc(seriesQueryBaton *, sds, series_sample_set_t *);
static void series_lookup_services(void *);
static void series_lookup_mapping(void *);
static void series_lookup_finished(void *);
static void series_query_mapping(void *arg);
static void series_instances_reply_callback(redisClusterAsyncContext *, void *, void *);

sds	cursorcount;	/* number of elements in each SCAN call */

static void
initSeriesGetQuery(seriesQueryBaton *baton, node_t *root, timing_t *timing)
{
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "initSeriesGetQuery");
    baton->u.query.root = *root;
    baton->u.query.timing = *timing;
}

static int
skip_free_value_set(node_t *np)
{
    /* return 0 stands for skipping free this node's value_set. */
    if (np->type == N_RATE || np->type == N_RESCALE || np->type == N_ABS 
	|| np->type == N_SQRT || np->type == N_FLOOR || np->type == N_ROUND
	|| np->type == N_LOG || np->type == N_PLUS || np->type == N_MINUS
	|| np->type == N_STAR || np->type == N_SLASH)
	return 0;
    return 1;
}

static void
freeSeriesQueryNode(node_t *np, int level)
{
    int		n_samples;
    if (np == NULL)
	return;
    if (skip_free_value_set(np) != 0) {
	int i, j, k;
	for (i = 0; i < np->value_set.num_series; i++) {
	    n_samples = np->value_set.series_values[i].num_samples;
	    if (n_samples < 0) n_samples = -n_samples;
	    for (j = 0; j < n_samples; j++) {
		for (k=0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		    sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].timestamp);
		    sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].series);
		    sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		}
		free(np->value_set.series_values[i].series_sample[j].series_instance);
	    }
	    sdsfree(np->value_set.series_values[i].sid->name);
	    free(np->value_set.series_values[i].sid);
	    free(np->value_set.series_values[i].series_sample);
	    sdsfree(np->value_set.series_values[i].series_desc.indom);
	    sdsfree(np->value_set.series_values[i].series_desc.pmid);
	    sdsfree(np->value_set.series_values[i].series_desc.semantics);
	    sdsfree(np->value_set.series_values[i].series_desc.source);
	    sdsfree(np->value_set.series_values[i].series_desc.type);
	    sdsfree(np->value_set.series_values[i].series_desc.units);
	}
	free(np->value_set.series_values);
    }
    freeSeriesQueryNode(np->left, level+1);
    freeSeriesQueryNode(np->right, level+1);
    if (level != 0)
        free(np);
}

static void
freeSeriesGetQuery(seriesQueryBaton *baton)
{
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "freeSeriesGetQuery");
    seriesBatonCheckCount(baton, "freeSeriesGetQuery");
    if (baton->error == 0) {
    	freeSeriesQueryNode(&baton->u.query.root, 0);
    }
    memset(baton, 0, sizeof(seriesQueryBaton));
    free(baton);
}

static void
initSeriesGetLabelMap(seriesGetLabelMap *value, sds series, sds name,
		redisMap *map, sds mapID, sds mapKey, void *baton)
{
    initSeriesBatonMagic(value, MAGIC_LABELMAP);
    value->map = map;
    value->series = sdsdup(series);
    value->name = sdsdup(name);
    value->mapID = sdsdup(mapID);
    value->mapKey = sdsdup(mapKey);
    value->baton = baton;
}

static void
freeSeriesGetLabelMap(seriesGetLabelMap *value)
{
    seriesBatonCheckMagic(value, MAGIC_LABELMAP, "freeSeriesGetLabelMap");

    redisMapRelease(value->map);
    sdsfree(value->series);
    sdsfree(value->name);
    sdsfree(value->mapID);
    sdsfree(value->mapKey);
    memset(value, 0, sizeof(seriesGetLabelMap));
    free(value);
}

static void
initSeriesGetSID(seriesGetSID *sid, const char *name, int needfree, void *baton)
{
    initSeriesBatonMagic(sid, MAGIC_SID);
    sid->name = sdsnew(name);
    sid->freed = needfree ? 1 : 0;
    sid->baton = baton;
}

static void
freeSeriesGetSID(seriesGetSID *sid)
{
    int			needfree;

    seriesBatonCheckMagic(sid, MAGIC_SID, "freeSeriesGetSID");
    sdsfree(sid->name);
    sdsfree(sid->metric);
    needfree = sid->freed;
    memset(sid, 0, sizeof(seriesGetSID));
    if (needfree)
	free(sid);
}

static void
initSeriesQueryBaton(seriesQueryBaton *baton,
		pmSeriesSettings *settings, void *userdata)
{
    seriesModuleData	*data = getSeriesModuleData(&settings->module);

    if (data == NULL) {
	/* calloc failed in getSeriesModuleData */
    	baton->error = -ENOMEM;
	return;
    }
    initSeriesBatonMagic(baton, MAGIC_QUERY);
    baton->callbacks = &settings->callbacks;
    baton->info = settings->module.on_info;
    baton->slots = data->slots;
    baton->module = &settings->module;
    baton->userdata = userdata;
}

static void
initSeriesGetLookup(seriesQueryBaton *baton, int nseries, sds *series,
		pmSeriesStringCallBack func, redisMap *map)
{
    seriesGetSID	*sid;
    unsigned int	i;

    /* pattern matching parameter, optional */
    if (nseries == 0 && series != NULL)
	baton->u.lookup.pattern = *series;
    /* else lookup array of individual sids */
    for (i = 0; i < nseries; i++) {
	sid = &baton->u.lookup.series[i];
	initSeriesGetSID(sid, series[i], 0, baton);
    }
    baton->u.lookup.nseries = nseries;
    baton->u.lookup.func = func;
    baton->u.lookup.map = map;
}

static void
freeSeriesGetLookup(seriesQueryBaton *baton)
{
    seriesGetSID	*sid;
    size_t		bytes;
    unsigned int	i, nseries;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "freeSeriesGetLookup");
    seriesBatonCheckCount(baton, "freeSeriesGetLookup");

    nseries = baton->u.lookup.nseries;
    for (i = 0; i < nseries; i++) {
	sid = &baton->u.lookup.series[i];
	sdsfree(sid->name);
    }
    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    memset(baton, 0, bytes);
    free(baton);
}

void
series_stats_inc(pmSeriesSettings *settings, unsigned int metric)
{   
    seriesModuleData	*data = getSeriesModuleData(&settings->module);

    if (data)
	mmv_inc(data->map, data->metrics[metric]);
}   

static void
series_query_finished(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    baton->callbacks->on_done(baton->error, baton->userdata);
    freeSeriesGetQuery(baton);
}

static void
series_query_end_phase(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    char                error[PM_MAXERRMSGLEN];

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_end_phase");

    if (baton->error == 0) {
	seriesPassBaton(&baton->current, baton, "series_query_end_phase");
    } else {	/* fail after waiting on outstanding I/O */
	if (pmDebugOptions.series || pmDebugOptions.query) {
	    fprintf(stderr, "%s: ERROR: %d %s\n", "series_query_end_phase",
		baton->error, pmErrStr_r(baton->error, error, sizeof(error)));
	}
	if (seriesBatonDereference(baton, "series_query_end_phase"))
	    series_query_finished(baton);
    }
}

const char *
series_instance_name(sds key)
{
    size_t	length = sdslen(key);

    if (length >= sizeof("instance.") &&
	strncmp(key, "instance.", sizeof("instance.") - 1) == 0)
	return key + sizeof("instance.") - 1;
    if (length >= sizeof("inst.") &&
	strncmp(key, "inst.", sizeof("inst.") - 1) == 0)
	return key + sizeof("inst.") - 1;
    if (length >= sizeof("i.") &&
	strncmp(key, "i.", sizeof("i.") - 1) == 0)
	return key + sizeof("i.") - 1;
    return NULL;
}

const char *
series_context_name(sds key)
{
    size_t	length = sdslen(key);

    if (length >= sizeof("context.") &&
	strncmp(key, "context.", sizeof("context.") - 1) == 0)
	return key + sizeof("context.") - 1;
    if (length >= sizeof("source.") &&
	strncmp(key, "source.", sizeof("source.") - 1) == 0)
	return key + sizeof("source.") - 1;
    if (length >= sizeof("c.") &&
	strncmp(key, "c.", sizeof("c.") - 1) == 0)
	return key + sizeof("c.") - 1;
    if (length >= sizeof("s.") &&
	strncmp(key, "s.", sizeof("s.") - 1) == 0)
	return key + sizeof("s.") - 1;
    return NULL;
}

const char *
series_metric_name(sds key)
{
    size_t	length = sdslen(key);

    if (length >= sizeof("metric.") &&
	strncmp(key, "metric.", sizeof("metric.") - 1) == 0)
	return key + sizeof("metric.") - 1;
    if (length >= sizeof("m.") &&
	strncmp(key, "m.", sizeof("m.") - 1) == 0)
	return key + sizeof("m.") - 1;
    return NULL;
}

const char *
series_label_name(sds key)
{
    size_t	length = sdslen(key);

    if (length >= sizeof("label.") &&
	strncmp(key, "label.", sizeof("label.") - 1) == 0)
	return key + sizeof("label.") - 1;
    if (length >= sizeof("l.") &&
	strncmp(key, "l.", sizeof("l.") - 1) == 0)
	return key + sizeof("l.") - 1;
    return NULL;
}

const char *
node_subtype(node_t *np)
{
    switch (np->subtype) {
    case N_QUERY: return "query";
    case N_LABEL: return "label";
    case N_METRIC: return "metric";
    case N_CONTEXT: return "context";
    case N_INSTANCE: return "instance";
    default:
        break;
    }
    return NULL;
}

static int
extract_string(seriesQueryBaton *baton, pmSID series,
		redisReply *reply, sds *string, const char *message)
{
    sds			msg;

    if (reply->type == REDIS_REPLY_STRING) {
	*string = sdscpylen(*string, reply->str, reply->len);
	return 0;
    }
    infofmt(msg, "expected string result for %s of series %s (got %s)",
			message, series, redis_reply_type(reply));
    batoninfo(baton, PMLOG_RESPONSE, msg);
    return -EINVAL;
}

static int
extract_mapping(seriesQueryBaton *baton, pmSID series,
		redisReply *reply, sds *string, const char *message)
{
    redisMapEntry	*entry;
    sds			msg, key;

    if (reply->type == REDIS_REPLY_STRING) {
	key = sdsnewlen(reply->str, reply->len);
	entry = redisMapLookup(baton->u.lookup.map, key);
	sdsfree(key);
	if (entry != NULL) {
	    key = redisMapValue(entry);
	    *string = sdscpylen(*string, key, sdslen(key));
	    return 0;
	}
	infofmt(msg, "bad mapping for %s of series %s", message, series);
	batoninfo(baton, PMLOG_CORRUPT, msg);
	return -EINVAL;
    }
    infofmt(msg, "expected string for %s of series %s", message, series);
    batoninfo(baton, PMLOG_RESPONSE, msg);
    return -EPROTO;
}

static int
extract_sha1(seriesQueryBaton *baton, pmSID series,
		redisReply *reply, sds *sha, const char *message)
{
    sds			msg;
    char		hashbuf[42];

    if (reply->type != REDIS_REPLY_STRING) {
	infofmt(msg, "expected string result for \"%s\" of series %s got %s",
			message, series, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EINVAL;
    }
    if (reply->len != 20) {
	infofmt(msg, "expected sha1 for \"%s\" of series %s, got %ld bytes",
			message, series, (long)reply->len);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EINVAL;
    }
    pmwebapi_hash_str((unsigned char *)reply->str, hashbuf, sizeof(hashbuf));
    *sha = sdscpylen(*sha, hashbuf, 40);
    return 0;
}

/*
 * Report a timeseries result - timestamps and (instance) values
 */
static int
series_instance_reply(seriesQueryBaton *baton, sds series,
	pmSeriesValue *value, int nelements, redisReply **elements)
{
    char		hashbuf[42];
    sds			inst;
    int			i, sts = 0;

    for (i = 0; i < nelements; i += 2) {
	inst = value->series;
	if (extract_string(baton, series, elements[i], &inst, "series") < 0) {
	    sts = -EPROTO;
	    continue;
	}
	if (sdslen(inst) == 0) {	/* no InDom, use series */
	    inst = sdscpylen(inst, series, 40);
	} else if (sdslen(inst) == 20) {
	    pmwebapi_hash_str((const unsigned char *)inst, hashbuf, sizeof(hashbuf));
	    inst = sdscpylen(inst, hashbuf, 40);
	} else {
	    /* TODO: propagate errors and mark records - separate callbacks? */
	    continue;
	}
	value->series = inst;

	if (extract_string(baton, series, elements[i+1], &value->data, "value") < 0)
	    sts = -EPROTO;
	else
	    baton->callbacks->on_value(series, value, baton->userdata);
    }
    return sts;
}

static int
extract_time(seriesQueryBaton *baton, pmSID series,
		redisReply *reply, sds *stamp, pmTimespec *ts)
{
    sds			msg, val;
    char		*point = NULL;
    __uint64_t		milliseconds, fractions, crossover;

    if (reply->type == REDIS_REPLY_STRING) {
	val = sdscpylen(*stamp, reply->str, reply->len);
	milliseconds = strtoull(val, &point, 0);
	if (point && *point == '-') {
	    *point = '.';
	    fractions = strtoull(point + 1, NULL, 0);
	} else {
	    fractions = 0;
	}
	crossover = milliseconds % 1000;
	ts->tv_sec = milliseconds / 1000;
	ts->tv_nsec = (fractions * 1000) + (crossover * 1000000);
	*stamp = val;
	return 0;
    }
    infofmt(msg, "expected string timestamp in series %s", series);
    batoninfo(baton, PMLOG_RESPONSE, msg);
    return -EPROTO;
}

static inline int
pmTimespec_cmp(pmTimespec *a, pmTimespec *b)
{
    if (a->tv_sec != b->tv_sec)
	return (a->tv_sec > b->tv_sec) ? 1 : -1;
    if (a->tv_nsec > b->tv_nsec)
	return 1;
    if (a->tv_nsec < b->tv_nsec)
	return -1;
    return 0;
}

static inline void
pmTimespec_add(pmTimespec *t1, pmTimespec *t2)
{
    __int64_t		sec = t1->tv_sec + t2->tv_sec;
    __int64_t		nsec = t1->tv_nsec + t2->tv_nsec;

    if (nsec >= 1000000000) {
        nsec -= 1000000000;
        sec++;
    }
    t1->tv_sec = sec;
    t1->tv_nsec = nsec;
}

/* t1 - t2 as a double */
static inline double
pmTimespec_delta(pmTimespec *t1, pmTimespec *t2)
{
    return (double)(t1->tv_sec - t2->tv_sec) +
    	(long double)(t1->tv_nsec - t2->tv_nsec) / (long double)1000000000;
}

typedef struct seriesSampling {
    unsigned int	setup;		/* one-pass calculation flag */
    unsigned int	subsampling;	/* sample interval requested */
    unsigned int	count;		/* N sampled values, so far */
    pmSeriesValue	value;		/* current sample being built */
    pmTimespec		goal;		/* 'ideal' sample timestamp */
    pmTimespec		delta;		/* sampling interval (step) */
    pmTimespec		next_timespec;	/* from the following sample */
    sds			next_timestamp;	/* from the following sample */
} seriesSampling;

static int
use_next_sample(seriesSampling *sampling)
{
    /* if the next timestamp is past our goal, use the current value */
    if (pmTimespec_cmp(&sampling->next_timespec, &sampling->goal) > 0) {
	/* selected a value for this interval so move the goal posts */
	pmTimespec_add(&sampling->goal, &sampling->delta);
	return 0;
    }
    return 1;
}

static void
series_values_reply(seriesQueryBaton *baton, sds series,
		int nsamples, redisReply **samples, void *arg)
{
    seriesSampling	sampling = {0};
    redisReply		*reply, *sample, **elements;
    timing_t		*tp = &baton->u.query.timing;
    int			n, sts, next, nelements;
    sds			msg, save_timestamp;

    sampling.value.timestamp = sdsempty();
    sampling.value.series = sdsempty();
    sampling.value.data = sdsempty();

    /* iterate over the 'samples' array */
    for (n = 0; n < nsamples; n++) {
	sample = samples[n];
	if ((nelements = sample->elements) == 0)
	    continue;
	elements = sample->element;

	/* expecting timestamp:valueset pairs, then instance:value pairs */
	if (nelements % 2) {
	    infofmt(msg, "expected time:valueset pairs in %s XRANGE", series);
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	    break;
	}

	/* verify the instance:value pairs array before proceeding */
	reply = elements[1];
	if (reply->type != REDIS_REPLY_ARRAY) {
	    infofmt(msg, "expected value array for series %s %s (type=%s)",
			series, XRANGE, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    baton->error = -EPROTO;
	    break;
	}

	/* setup state variables used internally during selection process */
	if (sampling.setup == 0 && (tp->delta.tv_sec || tp->delta.tv_usec)) {
	    /* 'next' is a nanosecond precision time interval to step with */
	    sampling.delta.tv_sec = tp->delta.tv_sec;
	    sampling.delta.tv_nsec = tp->delta.tv_usec * 1000;

	    /* extract the first timestamp to kickstart the comparison process */
	    if ((sts = extract_time(baton, series, elements[0],
					&sampling.value.timestamp,
					&sampling.value.ts)) < 0) {
		baton->error = sts;
		break;
	    }
	    /* 'goal' is the first target interval as an absolute timestamp */
	    if (tp->start.tv_sec || tp->start.tv_usec) {
		sampling.goal.tv_sec = tp->start.tv_sec;
		sampling.goal.tv_nsec = tp->start.tv_usec * 1000;
	    } else {
		sampling.goal = sampling.value.ts;
	    }
	    sampling.goal.tv_nsec++;	/* ensure we use first sample */

	    sampling.next_timestamp = sdsempty();
	    sampling.subsampling = 1;
	}
	sampling.setup = 1;

	if (sampling.subsampling == 0) {
	    if ((sts = extract_time(baton, series, elements[0],
					&sampling.value.timestamp,
					&sampling.value.ts)) < 0) {
		baton->error = sts;
		continue;
	    }
	} else if ((next = n + 1) < nsamples) {
	    /*
	     * Compare this point and the next to the ideal based on delta;
	     * skip over returning this value if the next one looks better.
	     */
	    elements = samples[next]->element;
	    if ((sts = extract_time(baton, series, elements[0],
					&sampling.next_timestamp,
					&sampling.next_timespec)) < 0) {
		baton->error = sts;
		continue;
	    } else if ((sts = use_next_sample(&sampling)) == 1) {
		goto next_sample;
	    } else if (sts == -1) {		/* sampling reached the end */
		goto last_sample;
	    } /* else falls through and may call user-supplied callback */
	}

	/* check whether a user-requested sample count has been reached */
	if (tp->count && sampling.count++ >= tp->count)
	    break;

	if ((sts = series_instance_reply(baton, series, &sampling.value,
				reply->elements, reply->element)) < 0) {
	    baton->error = sts;
	    goto last_sample;
	}

	if (sampling.subsampling == 0)
	    continue;
next_sample:
	/* carefully swap time strings to avoid leaking memory */
	save_timestamp = sampling.next_timestamp;
	sampling.next_timestamp = sampling.value.timestamp;
	sampling.value.timestamp = save_timestamp;
	sampling.value.ts = sampling.next_timespec;
    }

last_sample:
    if (sampling.setup)
	sdsfree(sampling.next_timestamp);
    sdsfree(sampling.value.timestamp);
    sdsfree(sampling.value.series);
    sdsfree(sampling.value.data);
}

/*
 * Save the series hash identifiers contained in a Redis response
 * for all series that are not already in this nodes set (union).
 * Used at the leaves of the query tree, then merged result sets
 * are propagated upward.
 */
static int
node_series_reply(seriesQueryBaton *baton, node_t *np, int nelements, redisReply **elements)
{
    series_set_t	set;
    unsigned char	*series;
    redisReply		*reply;
    char		hashbuf[42];
    sds			msg;
    int			i, sts = 0;

    if (nelements <= 0)
	return nelements;

    if ((series = (unsigned char *)calloc(nelements, SHA1SZ)) == NULL) {
	infofmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"series reply", (__int64_t)nelements * SHA1SZ);
	batoninfo(baton, PMLOG_REQUEST, msg);
	return -ENOMEM;
    }
    set.series = series;
    set.nseries = nelements;

    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if (reply->type == REDIS_REPLY_STRING) {
	    memcpy(series, reply->str, SHA1SZ);
	    if (pmDebugOptions.series) {
		pmwebapi_hash_str(series, hashbuf, sizeof(hashbuf));
		fprintf(stderr, "    %s\n", hashbuf);
	    }
	    series += SHA1SZ;
	} else {
	    infofmt(msg, "expected string in %s set \"%s\" (type=%s)",
		    node_subtype(np->left), np->left->key,
		    redis_reply_type(reply));
	    batoninfo(baton, PMLOG_REQUEST, msg);
	    sts = -EPROTO;
	}
    }
    if (sts < 0) {
	free(set.series);
	return sts;
    }

    return series_union(&np->result, &set);
}

static int
series_compare(const void *a, const void *b)
{
    return memcmp(a, b, SHA1SZ);
}

/*
 * Form resulting set via intersection of two child sets.
 * Algorithm:
 * - sort the larger set
 * - for each identifier in the smaller set
 *   o bisect to find match in sorted set
 *   o if matching, add it to the current saved set
 *
 * Memory from the smaller set is re-used to hold the result,
 * its memory is trimmed (via realloc) if the final resulting
 * set is smaller, and the larger set is freed on completion.
 */
static int
series_intersect(series_set_t *a, series_set_t *b)
{
    unsigned char	*small, *large, *saved, *cp;
    int			nsmall, nlarge, total, i;

    if (a->nseries >= b->nseries) {
	large = a->series;	nlarge = a->nseries;
	small = b->series;	nsmall = b->nseries;
    } else {
	small = a->series;	nsmall = a->nseries;
	large = b->series;	nlarge = b->nseries;
    }

    if (pmDebugOptions.series)
	printf("Intersect large(%d) and small(%d) series\n", nlarge, nsmall);

    qsort(large, nlarge, SHA1SZ, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += SHA1SZ) {
	if (!bsearch(cp, large, nlarge, SHA1SZ, series_compare))
	    continue;		/* no match, continue advancing cp only */
	if (saved != cp)
	    memcpy(saved, cp, SHA1SZ);
	saved += SHA1SZ;		/* stashed, advance cp & saved pointers */
    }

    if ((total = (saved - small)/SHA1SZ) < nsmall) {
	/* shrink the smaller set down further */
	if ((small = realloc(small, total * SHA1SZ)) == NULL)
	    return -ENOMEM;
    }

    if (pmDebugOptions.series && pmDebugOptions.desperate) {
	char		hashbuf[42];

	fprintf(stderr, "Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp += SHA1SZ, i++) {
	    pmwebapi_hash_str(cp, hashbuf, sizeof(hashbuf));
	    fprintf(stderr, "    %s\n", hashbuf);
	}
    }

    a->nseries = total;
    a->series = small;
    b->series = NULL;
    b->nseries = 0;
    free(large);
    return 0;
}

static int
node_series_intersect(node_t *np, node_t *left, node_t *right)
{
    int			sts;

    if ((sts = series_intersect(&left->result, &right->result)) >= 0)
	np->result = left->result;

    /* finished with child leaves now, results percolated up */
    right->result.nseries = left->result.nseries = 0;
    return sts;
}

/*
 * Form the resulting set from union of two child sets.
 * The larger set is realloc-ated to form the result, if we
 * need to (i.e. if there are entries in the smaller set not
 * in the larger).
 *
 * Iterates over the smaller set doing a binary search of
 * each series identifier, and tracks which ones in the small
 * need to be added to the large set.
 * At the end, add more space to the larger set if needed and
 * append to it.  As a courtesy, since all callers need this,
 * we free the smaller set as well.
 */
static int
series_union(series_set_t *a, series_set_t *b)
{
    unsigned char	*cp, *saved, *large, *small;
    int			nlarge, nsmall, total, need, i;

    if (a->nseries >= b->nseries) {
	large = a->series;	nlarge = a->nseries;
	small = b->series;	nsmall = b->nseries;
    } else {
	small = a->series;	nsmall = a->nseries;
	large = b->series;	nlarge = b->nseries;
    }

    if (pmDebugOptions.series)
	fprintf(stderr, "Union of large(%d) and small(%d) series\n", nlarge, nsmall);

    qsort(large, nlarge, SHA1SZ, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += SHA1SZ) {
	if (bsearch(cp, large, nlarge, SHA1SZ, series_compare) != NULL)
	    continue;		/* already present, no need to save */
	if (saved != cp)
	    memcpy(saved, cp, SHA1SZ);
	saved += SHA1SZ;	/* stashed, advance both cp & saved */
    }

    if ((need = (saved - small) / SHA1SZ) > 0) {
	/* grow the larger set to cater for new entries, then add 'em */
	if ((cp = realloc(large, (nlarge + need) * SHA1SZ)) == NULL)
	    return -ENOMEM;
	large = cp;
	cp += (nlarge * SHA1SZ);
	memcpy(cp, small, need * SHA1SZ);
	total = nlarge + need;
    } else {
	total = nlarge;
    }

    if (pmDebugOptions.series && pmDebugOptions.desperate) {
	char		hashbuf[42];

	fprintf(stderr, "Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += SHA1SZ, i++) {
	    pmwebapi_hash_str(cp, hashbuf, sizeof(hashbuf));
	    fprintf(stderr, "    %s\n", hashbuf);
	}
    }

    a->nseries = total;
    a->series = large;
    b->series = NULL;
    b->nseries = 0;
    free(small);
    return 0;
}

static int
node_series_union(node_t *np, node_t *left, node_t *right)
{
    int			sts;

    if ((sts = series_union(&left->result, &right->result)) >= 0)
	np->result = left->result;

    /* finished with child leaves now, results percolated up */
    right->result.nseries = left->result.nseries = 0;
    return sts;
}

static int
string_pattern_match(node_t *np, sds pattern, char *string, int length)
{
    int		sts;

    /* if the string is in double quotes, we want to pattern match */
    if (length > 1 && string[0] == '\"' && string[length-1] == '\"') {
	string[length-1] = '\0';
	string++;
    }

    if (np->type == N_GLOB)	/* match via globbing */
	return fnmatch(pattern, string, 0) == 0;

    /* use either regular expression match or negation */
    sts = regexec((const regex_t *)&np->regex, string, 0, NULL, 0);
    if (np->type == N_REQ)
	return sts == 0;
    if (np->type == N_RNE)
	return sts != 0;
    return 0;
}

/*
 * Add a node subtree representing glob (N_GLOB) pattern matches.
 * Each of these matches are then further evaluated (as if N_EQ).
 * Response format is described at https://redis.io/commands/scan
 */
static int
node_pattern_reply(seriesQueryBaton *baton, node_t *np, const char *name, int nelements,
		redisReply **elements)
{
    redisReply		*reply, *r;
    sds			msg, key, pattern, *matches;
    char		buffer[42];
    size_t		bytes;
    unsigned int	i;

    if (nelements != 2) {
	infofmt(msg, "expected cursor and results from %s (got %d elements)",
			HSCAN, nelements);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    /* update the cursor in case subsequent calls are needed */
    reply = elements[0];
    if (!reply || reply->type != REDIS_REPLY_STRING) {
	infofmt(msg, "expected integer cursor result from %s (got %s)",
			HSCAN, reply ? redis_reply_type(reply) : "null");
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }
    np->cursor = strtoull(reply->str, NULL, 10);

    reply = elements[1];
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
	infofmt(msg, "expected array of results from %s (got %s)",
			HSCAN, reply ? redis_reply_type(reply) : "null");
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    /* result array checking */
    if ((nelements = reply->elements) % 2) {
	infofmt(msg, "expected even number of results from %s (not %d)",
		    HSCAN, nelements);
	batoninfo(baton, PMLOG_REQUEST, msg);
	return -EPROTO;
    }
    if (nelements == 0)
	goto out;

    for (i = 0; i < nelements; i += 2) {
	r = reply->element[i];
	if (r->type != REDIS_REPLY_STRING) {
	    infofmt(msg, "expected only string results from %s (type=%s)",
		    HSCAN, redis_reply_type(r));
	    batoninfo(baton, PMLOG_REQUEST, msg);
	    return -EPROTO;
	}
    }

    /* response is key:value pairs from the scanned hash */
    nelements /= 2;

    /* matching string - either glob or regex */
    pattern = np->right->value;
    if (np->type != N_GLOB &&
	/* TODO: move back to initial tree parsing for error handling */
	regcomp((regex_t *)&np->regex, pattern, REG_EXTENDED|REG_NOSUB) != 0) {
	infofmt(msg, "invalid regular expression \"%s\"", pattern);
	batoninfo(baton, PMLOG_REQUEST, msg);
	return -EINVAL;
    }

    for (i = 0; i < nelements; i++) {
	r = reply->element[i*2+1];	/* string value */
	if (!string_pattern_match(np, pattern, r->str, r->len))
	    continue;

	r = reply->element[i*2];	/* SHA1 hash */
	pmwebapi_hash_str((const unsigned char *)r->str, buffer, sizeof(buffer));
	key = sdsnew("pcp:series:");
	key = sdscatfmt(key, "%s:%s", name, buffer);

	if (pmDebugOptions.series)
	    fprintf(stderr, "adding pattern-matched result key: %s\n", key);

	bytes = (np->nmatches + 1) * sizeof(sds);
	if ((matches = (sds *)realloc(np->matches, bytes)) == NULL) {
	    infofmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"pattern reply", (__int64_t)bytes);
	    batoninfo(baton, PMLOG_REQUEST, msg);
	    sdsfree(key); /* Coverity CID328038 */
	    return -ENOMEM;
	}
	matches[np->nmatches++] = key;
	np->matches = matches;
    }

out:
    if (np->cursor > 0)	/* still more to retrieve - kick off the next batch */
	series_pattern_match(baton, np);
    else
	regfree((regex_t *)&np->regex);

    return nelements;
}

static void
series_prepare_maps_pattern_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    node_t		*np = (node_t *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    redisReply		*reply = r;
    const char		*name;
    node_t		*left;
    sds			msg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_maps_pattern_reply");
    assert(np->type == N_GLOB || np->type == N_REQ || np->type == N_RNE);

    left = np->left;

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array for %s key \"%s\" (type=%s)",
	    node_subtype(left), left->key, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else {
	name = left->key + sizeof("pcp:map:") - 1;
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s %s\n", node_subtype(np->left), np->key);
	if (node_pattern_reply(baton, np, name, reply->elements, reply->element) < 0)
	    baton->error = -EPROTO;
    }

    series_query_end_phase(baton);
}

static void
series_pattern_match(seriesQueryBaton *baton, node_t *np)
{
    sds			cmd, cur, key;

    seriesBatonReference(baton, "series_pattern_match");
    cur = sdscatfmt(sdsempty(), "%U", np->cursor);
    key = sdsdup(np->left->key);
    cmd = redis_command(5);
    cmd = redis_param_str(cmd, HSCAN, HSCAN_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_sds(cmd, cur);	/* cursor */
    cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
    cmd = redis_param_sds(cmd, cursorcount);
    sdsfree(cur);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
				series_prepare_maps_pattern_reply, np);
    sdsfree(cmd);
}

/*
 * Map human names to internal Redis identifiers.
 */
static int
series_prepare_maps(seriesQueryBaton *baton, node_t *np, int level)
{
    unsigned char	hash[20];
    const char		*name;
    char		buffer[42];
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_maps(baton, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_NAME:
	/* setup any label name map identifiers needed by direct children */
	if ((name = series_instance_name(np->value)) != NULL) {
	    np->subtype = N_INSTANCE;
	    np->key = sdsnew("pcp:map:inst.name");
	} else if ((name = series_metric_name(np->value)) != NULL) {
	    np->subtype = N_METRIC;
	    np->key = sdsnew("pcp:map:metric.name");
	} else if ((name = series_context_name(np->value)) != NULL) {
	    np->subtype = N_CONTEXT;
	    np->key = sdsnew("pcp:map:context.name");
	} else {
	    np->subtype = N_LABEL;
	    if ((name = series_label_name(np->value)) == NULL)
		name = np->value;
	    pmwebapi_string_hash(hash, name, strlen(name));
	    np->key = sdscatfmt(sdsempty(), "pcp:map:label.%s.value",
			    pmwebapi_hash_str(hash, buffer, sizeof(buffer)));
	}
	break;

    case N_GLOB:	/* globbing or regular expression lookups */
    case N_REQ:
    case N_RNE:
	np->baton = baton;
	series_pattern_match(baton, np);
	break;

    default:
	break;
    }

    return series_prepare_maps(baton, np->right, level+1);
}

static sds
series_node_value(node_t *np)
{
    unsigned char	hash[20];
    sds			val = sdsnewlen(NULL, 40);

    /* special JSON cases still to do: null, true, false */
    if (np->left->type == N_NAME &&
	np->left->subtype == N_LABEL &&
	np->right->type == N_STRING) {
	np->right->subtype = N_LABEL;
	sdsclear(val);
	val = sdscatfmt(val, "\"%S\"", np->right->value);
	pmwebapi_string_hash(hash, val, sdslen(val));
    } else {
	pmwebapi_string_hash(hash, np->right->value, strlen(np->right->value));
    }
    sdsclear(val);
    return pmwebapi_hash_sds(val, hash);
}

static void
series_prepare_smembers_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    node_t		*np = (node_t *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    redisReply		*reply = r;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_smembers_reply");

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array for %s set \"%s\" (type=%s)",
		node_subtype(np->left), np->right->value,
		redis_reply_type(reply));
	batoninfo(baton, PMLOG_CORRUPT, msg);
	baton->error = -EPROTO;
    } else {
	if (pmDebugOptions.series)
	    fprintf(stderr, "%s %s\n", node_subtype(np->left), np->key);
	sts = node_series_reply(baton, np, reply->elements, reply->element);
	if (sts < 0)
	    baton->error = sts;
    }

    if (np->nmatches)
	np->nmatches--;	/* processed one more from this batch */

    series_query_end_phase(baton);
}

static void
series_prepare_smembers(seriesQueryBaton *baton, sds kp, node_t *np)
{
    sds                 cmd;

    cmd = redis_command(2);
    cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
    cmd = redis_param_sds(cmd, kp);
    redisSlotsRequest(baton->slots, cmd,
			series_prepare_smembers_reply, np);
    sdsfree(cmd);
}

static void
series_hmset_function_desc_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    redisReply		*reply = r;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_hmset_function_desc_callback");
    checkStatusReplyOK(baton->info, baton->userdata, c, reply,
			"%s", "pcp:desc");
    series_query_end_phase(baton);
}

static void
series_hmset_function_desc(seriesQueryBaton *baton, sds key, pmSeriesDesc *desc)
{
    sds			cmd;

    seriesBatonReference(baton, "series_hmset_function_desc");

    cmd = redis_command(14);
    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "indom", sizeof("indom")-1);
    cmd = redis_param_sds(cmd, desc->indom);
    cmd = redis_param_str(cmd, "pmid", sizeof("pmid")-1);
    cmd = redis_param_sds(cmd, desc->pmid);
    cmd = redis_param_str(cmd, "semantics", sizeof("semantics")-1);
    cmd = redis_param_sds(cmd, desc->semantics);
    cmd = redis_param_str(cmd, "source", sizeof("source")-1);
    /* for fabricated SIDs, this is a binary string of 20 zero (NULL) bytes */
    cmd = redis_param_sds(cmd, desc->source);
    cmd = redis_param_str(cmd, "type", sizeof("type")-1);
    cmd = redis_param_sds(cmd, desc->type);
    cmd = redis_param_str(cmd, "units", sizeof("units")-1);
    cmd = redis_param_sds(cmd, desc->units);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
			series_hmset_function_desc_callback, baton);
    sdsfree(cmd);
}

static void
series_hmset_function_expr_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    redisSlotsBaton	*baton = (redisSlotsBaton *)arg;
    redisReply		*reply = r;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_hmset_function_expr_callback");
    checkStatusReplyOK(baton->info, baton->userdata, c, reply,
			"%s", "pcp:expr");
    series_query_end_phase(baton);
}

static void
series_hmset_function_expr(seriesQueryBaton *baton, sds key, sds expr)
{
    sds			cmd;

    seriesBatonReference(baton, "series_hmset_function_expr");

    cmd = redis_command(4);
    cmd = redis_param_str(cmd, HMSET, HMSET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "query", sizeof("query")-1);
    cmd = redis_param_sds(cmd, expr);
    sdsfree(key);

    redisSlotsRequest(baton->slots, cmd, series_hmset_function_expr_callback, baton);
    sdsfree(cmd);
}

/*
 * Prepare evaluation of leaf nodes.
 */
static int
series_prepare_eval(seriesQueryBaton *baton, node_t *np, int level)
{
    sds 		val;
    int			sts, i;
    node_t		*left;
    const char		*name;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_eval(baton, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:		/* direct hash lookup */
	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;
	assert(np->key == NULL);
	val = series_node_value(np);
	np->key = sdsnew("pcp:series:");
	np->key = sdscatfmt(np->key, "%s:%S", name, val);
	sdsfree(val);
	np->baton = baton;
	seriesBatonReference(baton, "series_prepare_expr[direct]");
	series_prepare_smembers(baton, np->key, np);
	break;

    case N_GLOB:	/* globbing or regular expression lookups */
    case N_REQ:
    case N_RNE:
	np->baton = baton;
	if (np->nmatches > 0)
	    seriesBatonReferences(baton, np->nmatches, "series_prepare_eval[pattern]");
	for (i = 0; i < np->nmatches; i++)
	    series_prepare_smembers(baton, np->matches[i], np);
	break;

    default:
	break;
    }

    return series_prepare_eval(baton, np->right, level+1);
}

/*
 * Prepare evaluation of internal nodes.
 */
static int
series_prepare_expr(seriesQueryBaton *baton, node_t *np, int level)
{
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_expr(baton, np->left, level+1)) < 0)
	return sts;
    if ((sts = series_prepare_expr(baton, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_LT: case N_LEQ: case N_GEQ: case N_GT: case N_NEQ: case N_NEG:
	/* TODO - relational operators */
	break;

    case N_AND:
	sts = node_series_intersect(np, np->left, np->right);
	break;

    case N_OR:
	sts = node_series_union(np, np->left, np->right);
	break;

    default:
	break;
    }
    return sts;
}

static void
on_series_solve_setup(void *arg)
{
    if (pmDebugOptions.query)
	fprintf(stderr, "on_series_solve_setup\n");
}

static void
on_series_solve_log(pmLogLevel level, sds message, void *arg)
{
    if (pmDebugOptions.query)
	fprintf(stderr, "on_series_solve_log: %s\n", message);
}

static void
on_series_solve_done(int status, void *arg)
{
    seriesQueryBaton	*baton = arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "on_series_solve_done");
    if (pmDebugOptions.query && pmDebugOptions.desperate)
	fprintf(stderr, "on_series_solve_done: arg=%p status=%d\n", arg, status);
    baton->callbacks->on_done(status, baton->userdata);
}

static int
on_series_solve_value(pmSID sid, pmSeriesValue *value, void *arg)
{
    seriesQueryBaton	*baton = arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "on_series_solve_value");
    if (pmDebugOptions.query && pmDebugOptions.desperate)
	fprintf(stderr, "on_series_solve_value: arg=%p %s %s %s\n",
		arg, value->timestamp, value->data, value->series);
    return baton->callbacks->on_value(sid, value, baton->userdata);
}


static void
on_series_solve_inst_done(int status, void *arg)
{
    seriesQueryBaton	*baton = arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "on_series_solve_done");
    if (pmDebugOptions.query && pmDebugOptions.desperate)
	fprintf(stderr, "on_series_solve_done: arg=%p status=%d\n", arg, status);
    /* on_done is called by series_query_finished */
    seriesBatonDereference(baton, "on_series_solve_inst_done");
}

/*
 * HMGETALL pcp:inst:series:(value->series)
 * re-using series_instances_reply_callback as the callback.
 */
static int
on_series_solve_inst_value(pmSID sid, pmSeriesValue *value, void *arg)
{
    seriesQueryBaton	*baton = arg;
    seriesGetSID	*sidbat;
    sds			key, cmd;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "on_series_solve_inst_value");
    if (pmDebugOptions.query) {
	fprintf(stderr, "on_series_solve_inst_value: arg=%p %s %s %s\n",
		arg, value->timestamp, value->data, value->series);
    }

    sidbat = calloc(1, sizeof(seriesGetSID));
    initSeriesGetSID(sidbat, value->series, 1, baton);
    sidbat->metric = sdsdup(sid);

    seriesBatonReference(baton, "on_series_solve_inst_value");
    seriesBatonReference(sidbat, "on_series_solve_inst_value");

    key = sdscatfmt(sdsempty(), "pcp:inst:series:%S", value->series);
    cmd = redis_command(5);
    cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
    cmd = redis_param_str(cmd, "name", sizeof("name")-1);
    cmd = redis_param_str(cmd, "source", sizeof("source")-1);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
			series_instances_reply_callback, sidbat);
    sdsfree(cmd);
    return 0;
}

/* settings and callbacks for /series/values with fabricated SID */
static pmSeriesSettings	series_solve_values_settings = {
    .callbacks.on_value		= on_series_solve_value,
    .callbacks.on_done		= on_series_solve_done,
    .module.on_setup		= on_series_solve_setup,
    .module.on_info		= on_series_solve_log,
};

/* settings and callbacks for /series/instances with fabricated SID */
static pmSeriesSettings	series_solve_inst_settings = {
    .callbacks.on_value		= on_series_solve_inst_value,
    .callbacks.on_done		= on_series_solve_inst_done,
    .module.on_setup		= on_series_solve_setup,
    .module.on_info		= on_series_solve_log,
};

/*
 * Called from /series/values?series=SID[, ...] for a fabricated
 * SID expression. Parse and series_solve the expression with
 * samples/timing then via callbacks, add the resulting reply
 * elements to the response series for original pmSeriesBaton.
 */
static int
series_solve_sid_expr(pmSeriesSettings *settings, pmSeriesExpr *expr, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    series_t		sp = {0}; /* root of parsed expression tree, with timing */
    char		*errstr;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_query_expr_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_expr_reply");

    if (pmDebugOptions.query) {
	fprintf(stderr, "series_solve_sid_expr: SID %s, "
		"seriesQueryBaton=%p, pmSeriesBaton=userdata=%p expr=\"%s\"\n",
		sid->name, baton, baton->userdata, expr->query);
    }

    /* ref baton until on_series_solve_done */
    seriesBatonReference(baton, "series_solve_sid_expr");

    if ((sts = series_parse(expr->query, &sp, &errstr, arg)) == 0) {
	pmSeriesSetSlots(&settings->module, baton->slots);
	settings->module = *baton->module; /* struct cpy */

	sts = series_solve(settings, sp.expr, &baton->u.query.timing,
	    PM_SERIES_FLAG_NONE, baton);
    }

    return sts;
}

static void
series_query_expr_reply(redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    pmSeriesExpr	expr = {0};
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_query_expr_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_expr_reply");
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array of one string element (got %zu) from series %s %s (type=%s)",
		reply->elements, sid->name, HMGET, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
    } else if (reply->element[0]->type == REDIS_REPLY_STRING) {
	expr.query = sdsempty();
    	if ((sts = extract_string(baton, sid->name, reply->element[0], &expr.query, "query")) < 0) {
	    baton->error = sts;
	} else {
	    /* Parse the expr (with timing) and series solve the resulting expr tree */
	    baton->error = series_solve_sid_expr(&series_solve_values_settings, &expr, arg);
	}
    }
    series_query_end_phase(baton);
}

static void
series_prepare_time_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    seriesGetSID	*expr;
    sds			key, exprcmd;
    sds			msg;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_prepare_time_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_time_reply");
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from %s XSTREAM values (type=%s)",
			sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else {
	if (reply->elements > 0) {
	    /* reply is a normal time series */
	    series_values_reply(baton, sid->name, reply->elements, reply->element, arg);
	} else {
	    /* Handle fabricated/expression SID in /series/values :
	     * - get the expr for sid->name from redis. In the callback for that,
	     *   parse the expr and then solve the expression tree with timing from
	     *   this series query baton. Then merge the values in the reply elements.
	     *   TODO (maybe) - also get the desc and check source hash is zero.
	     */
	    if (pmDebugOptions.series)
		fprintf(stderr, "series_prepare_time_reply: sid %s is fabricated\n", sid->name);
	    expr = calloc(1, sizeof(seriesGetSID));
	    initSeriesGetSID(expr, sid->name, 1, baton);
	    seriesBatonReference(baton, "series_query_expr_reply");

	    key = sdscatfmt(sdsempty(), "pcp:expr:series:%S", expr->name);
	    exprcmd = redis_command(3);
	    exprcmd = redis_param_str(exprcmd, HMGET, HMGET_LEN);
	    exprcmd = redis_param_sds(exprcmd, key);
	    exprcmd = redis_param_str(exprcmd, "query", sizeof("query")-1);
	    sdsfree(key);
	    redisSlotsRequest(baton->slots, exprcmd, series_query_expr_reply, expr);
	    sdsfree(exprcmd);
	}
    }
    freeSeriesGetSID(sid);
    series_query_end_phase(baton);
}

unsigned int
series_value_count_only(timing_t *tp)
{
    if (tp->window.range || tp->window.delta ||
	tp->window.start || tp->window.end)
	return 0;
    return tp->count;
}

static void
series_prepare_time(seriesQueryBaton *baton, series_set_t *result)
{
    timing_t		*tp = &baton->u.query.timing;
    unsigned char	*series = result->series;
    seriesGetSID	*sid;
    char		buffer[64], revbuf[64];
    sds			start, end, key, cmd;
    unsigned int	i, revlen = 0, reverse = 0;

    /* if only 'count' is requested, work back from most recent value */
    if ((reverse = series_value_count_only(tp)) != 0) {
	revlen = pmsprintf(revbuf, sizeof(revbuf), "%u", reverse);
	start = sdsnew("+");
    } else {
	start = sdsnew(timeval_stream_str(&tp->start, buffer, sizeof(buffer)));
    }

    if (pmDebugOptions.series)
	fprintf(stderr, "START: %s\n", start);

    if (reverse)
	end = sdsnew("-");
    else if (tp->end.tv_sec)
	end = sdsnew(timeval_stream_str(&tp->end, buffer, sizeof(buffer)));
    else
	end = sdsnew("+");	/* "+" means "no end" - to the most recent */

    if (pmDebugOptions.series)
	fprintf(stderr, "END: %s\n", end);

    /*
     * Query cache for the time series range (groups of instance:value
     * pairs, with an associated timestamp).
     */
    for (i = 0; i < result->nseries; i++, series += SHA1SZ) {
	sid = calloc(1, sizeof(seriesGetSID));
	pmwebapi_hash_str(series, buffer, sizeof(buffer));

	initSeriesGetSID(sid, buffer, 1, baton);
	seriesBatonReference(baton, "series_prepare_time");

	key = sdscatfmt(sdsempty(), "pcp:values:series:%S", sid->name);

	/* X[REV]RANGE key t1 t2 [count N] */
	if (reverse) {
	    cmd = redis_command(6);
	    cmd = redis_param_str(cmd, XREVRANGE, XREVRANGE_LEN);
	} else {
	    cmd = redis_command(4);
	    cmd = redis_param_str(cmd, XRANGE, XRANGE_LEN);
	}
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, start);
	cmd = redis_param_sds(cmd, end);
	if (reverse) {
	    cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
	    cmd = redis_param_str(cmd, revbuf, revlen);
	}
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
				series_prepare_time_reply, sid);
	sdsfree(cmd);
    }
    sdsfree(start);
    sdsfree(end);
}

static void
series_expr_query_desc(seriesQueryBaton *baton, series_set_t *query_series_set, node_t *np)
{
    unsigned int		i;
    int				nseries = query_series_set->nseries;
    unsigned char		*series = query_series_set->series;
    char			hashbuf[42];
    seriesGetSID		*sid;

    /* calloc nseries samples store space */
    if ((np->value_set.series_values = (series_sample_set_t *)calloc(nseries, sizeof(series_sample_set_t))) == NULL) {
	baton->error = -ENOMEM;
	return;
    }
    for (i = 0; i < nseries; i++, series += SHA1SZ) {
	if ((sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID))) == NULL) {
	    baton->error = -ENOMEM;
	    return;
	}
	pmwebapi_hash_str(series, hashbuf, sizeof(hashbuf));
	initSeriesGetSID(sid, hashbuf, 1, baton);
	np->value_set.series_values[i].baton = baton;
	np->value_set.series_values[i].sid = sid;
	np->value_set.series_values[i].num_samples = 0;
	series_node_get_desc(baton, sid->name, &np->value_set.series_values[i]);
	series_node_get_metric_name(baton, sid, &np->value_set.series_values[i]);
    }
}

static int
series_expr_node_desc(seriesQueryBaton *baton, node_t *np)
{
    int		sts, nelements;

    if (np == NULL)
	return 0;

    if ((nelements = np->result.nseries) != 0) {
	np->value_set.num_series = nelements;
	np->baton = baton;
	series_expr_query_desc(baton, &np->result, np);
	return baton->error;
    }

    if ((sts = series_expr_node_desc(baton, np->left)) < 0)
	return sts;
    return series_expr_node_desc(baton, np->right);
}

static void
series_report_set(seriesQueryBaton *baton, node_t *np)
{
    int		i, j;
    sds		series;

    for (i = 0; i < np->value_set.num_series; i++) {
	series = np->value_set.series_values[i].sid->name;
	for (j=0; j < i; j++) {
	    if (strncmp(series, np->value_set.series_values[j].sid->name, SHA1SZ) == 0)
	    	break;
	}
	if (i == j && baton->callbacks->on_match)
	    baton->callbacks->on_match(series, baton->userdata);
    }
}

static void
series_query_report_matches(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    int			has_function = 0;
    char		hashbuf[42];

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_report_matches");
    seriesBatonCheckCount(baton, "series_query_report_matches");

    seriesBatonReference(baton, "series_query_report_matches");

    has_function = series_calculate(baton, &baton->u.query.root, 0);
    
    if (has_function != 0)
	series_redis_hash_expression(baton, hashbuf, sizeof(hashbuf));
    series_report_set(baton, &baton->u.query.root);
    series_query_end_phase(baton);
}

static void
series_query_maps(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_maps");
    seriesBatonCheckCount(baton, "series_query_maps");

    seriesBatonReference(baton, "series_query_maps");
    series_prepare_maps(baton, &baton->u.query.root, 0);
    series_query_end_phase(baton);
}

static void
series_query_eval(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_eval");
    seriesBatonCheckCount(baton, "series_query_eval");

    seriesBatonReference(baton, "series_query_eval");
    series_prepare_eval(baton, &baton->u.query.root, 0);
    series_query_end_phase(baton);
}

static void
series_query_expr(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_expr");
    seriesBatonCheckCount(baton, "series_query_expr");

    seriesBatonReference(baton, "series_query_expr");
    series_prepare_expr(baton, &baton->u.query.root, 0);
    series_query_end_phase(baton);
}

static int
series_instance_store_to_node(seriesQueryBaton *baton, sds series,
	pmSeriesValue *value, int nelements, redisReply **elements, node_t *np, int idx_sample)
{
    char		hashbuf[42];
    sds			inst;
    int			i, sts = 0;
    int			idx_instance = 0;
    int			idx_series = np->value_set.num_series;

    for (i = 0; i < nelements; i += 2) {
	inst = value->series;
	if (extract_string(baton, series, elements[i], &inst, "series") < 0) {
	    sts = -EPROTO;
	    continue;
	}
	if (sdslen(inst) == 0) {	/* no InDom, use series */
	    inst = sdscpylen(inst, series, 40);
	} else if (sdslen(inst) == 20) {
	    pmwebapi_hash_str((const unsigned char *)inst, hashbuf, sizeof(hashbuf));
	    inst = sdscpylen(inst, hashbuf, 40);
	} else {
	    /* TODO: propagate errors and mark records - separate callbacks? */
	    continue;
	}
	value->series = inst;

	if (extract_string(baton, series, elements[i+1], &value->data, "value") < 0)
	    sts = -EPROTO;
	else {
	    /* update value instance */
	    pmSeriesValue *valinst = &np->value_set.series_values[idx_series].series_sample[idx_sample].series_instance[idx_instance];

	    valinst->ts = value->ts; /* struct pmTimespec assign */
	    valinst->timestamp = sdsnew(value->timestamp);
	    valinst->series = sdsnew(value->series);
	    valinst->data = sdsnew(value->data);
	    ++idx_instance;
	}
    }
    return sts;
}

/* Do something like memcpy */
static void
series_values_store_to_node(seriesQueryBaton *baton, sds series,
		int nsamples, redisReply **samples, node_t *np)
{
    seriesSampling	sampling = {0};
    redisReply		*reply, *sample, **elements;
    timing_t		*tp = &baton->u.query.timing;
    int			i, sts, next, nelements;
    int			idx_series = np->value_set.num_series;
    int			idx_sample = 0;
    sds			msg, save_timestamp;

    sampling.value.timestamp = sdsempty();
    sampling.value.series = sdsempty();
    sampling.value.data = sdsempty();

    /* iterate over the 'samples' array */
    for (i = 0; i < nsamples; i++) {
	sample = samples[i];
	if ((nelements = sample->elements) == 0)
	    continue;
	elements = sample->element;

	/* expecting timestamp:valueset pairs, then instance:value pairs */
	if (nelements % 2) {
	    infofmt(msg, "expected time:valueset pairs in %s XRANGE", series);
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	    break;
	}

	/* verify the instance:value pairs array before proceeding */
	reply = elements[1];
	if (reply->type != REDIS_REPLY_ARRAY) {
	    infofmt(msg, "expected value array for series %s %s (type=%s)",
			series, XRANGE, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    baton->error = -EPROTO;
	    break;
	}

	/* setup state variables used internally during selection process */
	if (sampling.setup == 0 && (tp->delta.tv_sec || tp->delta.tv_usec)) {
	    /* 'next' is a nanosecond precision time interval to step with */
	    sampling.delta.tv_sec = tp->delta.tv_sec;
	    sampling.delta.tv_nsec = tp->delta.tv_usec * 1000;

	    /* extract the first timestamp to kickstart the comparison process */
	    if ((sts = extract_time(baton, series, elements[0],
	    				&sampling.value.timestamp,
					&sampling.value.ts)) < 0) {
		baton->error = sts;
		break;
	    }
	    /* 'goal' is the first target interval as an absolute timestamp */
	    if (tp->start.tv_sec || tp->start.tv_usec) {
		sampling.goal.tv_sec = tp->start.tv_sec;
		sampling.goal.tv_nsec = tp->start.tv_usec * 1000;
	    } else {
		sampling.goal = sampling.value.ts;
	    }
	    sampling.next_timestamp = sdsempty();
	    sampling.subsampling = 1;
	}
	sampling.setup = 1;

	if (sampling.subsampling == 0) {
	    if ((sts = extract_time(baton, series, elements[0],
					&sampling.value.timestamp,
					&sampling.value.ts)) < 0) {
		baton->error = sts;
		continue;
	    }
	} else if ((next = i + 1) < nsamples) {
	    /*
	     * Compare this point and the next to the ideal based on delta;
	     * skip over returning this value if the next one looks better.
	     */
	    elements = samples[next]->element;
	    if ((sts = extract_time(baton, series, elements[0],
	    				&sampling.next_timestamp,
					&sampling.next_timespec)) < 0) {
		baton->error = sts;
		continue;
	    } else if ((sts = use_next_sample(&sampling)) == 1) {
		goto next_sample;
	    } else if (sts == -1) {		/* sampling reached the end */
		goto last_sample;
	    }
	} /* else falls through and may call user-supplied callback */

	/* check whether a user-requested sample count has been reached
	 * Here copy the sample results(instance values) to the node.
	 */
	if (tp->count && sampling.count++ >= tp->count)
	    break;
	
	idx_sample = i;
	np->value_set.series_values[idx_series].series_sample[idx_sample].num_instances = reply->elements/2;
	if ((np->value_set.series_values[idx_series].series_sample[idx_sample].series_instance =
		(pmSeriesValue *)calloc(reply->elements/2, sizeof(pmSeriesValue))) == NULL) {
	    /* TODO: error report here */
	    baton->error = -ENOMEM;
	}
	if ((sts = series_instance_store_to_node(baton, series, &sampling.value,
				reply->elements, reply->element, np, idx_sample)) < 0) {
	    baton->error = sts;
	    goto last_sample;
	}
	
	if (sampling.subsampling == 0)
	    continue;

next_sample:
	/* carefully swap time strings to avoid leaking memory */
	save_timestamp = sampling.next_timestamp;
	sampling.next_timestamp = sampling.value.timestamp;
	sampling.value.timestamp = save_timestamp;
	sampling.value.ts = sampling.next_timespec;
    }

last_sample:
    if (sampling.setup)
	sdsfree(sampling.next_timestamp);
    sdsfree(sampling.value.timestamp);
    sdsfree(sampling.value.series);
    sdsfree(sampling.value.data);
}

static int
extract_series_desc(seriesQueryBaton *baton, pmSID series,
		int nelements, redisReply **elements, pmSeriesDesc *desc)
{
    sds			msg;

    if (nelements < 6) {
	infofmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    /* were we given a non-metric series identifier? (e.g. an instance) */
    if (elements[0]->type == REDIS_REPLY_NIL) {
	desc->indom = sdscpylen(desc->indom, "unknown", 7);
	desc->pmid = sdscpylen(desc->pmid, "PM_ID_NULL", 10);
	desc->semantics = sdscpylen(desc->semantics, "unknown", 7);
	desc->source = sdscpylen(desc->source, "unknown", 7);
	desc->type = sdscpylen(desc->type, "unknown", 7);
	desc->units = sdscpylen(desc->units, "unknown", 7);
	return 0;
    }

    if (extract_string(baton, series, elements[0], &desc->indom, "indom") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[1], &desc->pmid, "pmid") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[2], &desc->semantics, "semantics") < 0)
	return -EPROTO;
    if (extract_sha1(baton, series, elements[3], &desc->source, "source") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[4], &desc->type, "type") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[5], &desc->units, "units") < 0)
	return -EPROTO;

    return 0;
}

static int
extract_series_node_desc(seriesQueryBaton *baton, pmSID series,
		int nelements, redisReply **elements, pmSeriesDesc *desc)
{
    sds			msg;

    if (nelements < 4 || elements[0]->type == REDIS_REPLY_NIL) {
	infofmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    desc->pmid = sdsnew("511.0.0");
    if (extract_string(baton, series, elements[0], &desc->indom, "indom") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[1], &desc->semantics, "semantics") < 0)
	return -EPROTO;
    desc->source = sdsnew(elements[2]->str);
    if (extract_string(baton, series, elements[3], &desc->type, "type") < 0)
	return -EPROTO;
    if (extract_string(baton, series, elements[4], &desc->units, "units") < 0)
	return -EPROTO;
    return 0;
}

static void
series_node_get_desc_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    series_sample_set_t		*sample_set = (series_sample_set_t *) arg;
    redisReply			*reply = r;
    int				sts;
    pmSeriesDesc		*desc = &sample_set->series_desc;
    seriesQueryBaton		*baton = (seriesQueryBaton *)sample_set->baton;
    sds				msg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_node_get_desc_reply");

    desc->indom = sdsempty();
    desc->pmid = sdsempty();
    desc->semantics = sdsempty();
    desc->source = sdsempty();
    desc->type = sdsempty();
    desc->units = sdsempty();

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array type from series %s %s (type=%s)",
		sample_set->sid->name, HMGET, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else if ((sts = extract_series_node_desc(baton, sample_set->sid->name,
			reply->elements, reply->element, desc)) < 0)
	baton->error = sts;

    series_query_end_phase(baton);
}

static void
series_node_get_desc(seriesQueryBaton *baton, sds sid_name, series_sample_set_t *sample_set)
{
    sds			cmd, key;

    seriesBatonReference(baton, "series_node_get_desc");

    key = sdscatfmt(sdsempty(), "pcp:desc:series:%S", sid_name);
    cmd = redis_command(7);
    cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
    cmd = redis_param_sds(cmd, key);
    cmd = redis_param_str(cmd, "indom", sizeof("indom")-1);
    cmd = redis_param_str(cmd, "semantics", sizeof("semantics")-1);
    cmd = redis_param_str(cmd, "source", sizeof("source")-1);
    cmd = redis_param_str(cmd, "type", sizeof("type")-1);
    cmd = redis_param_str(cmd, "units", sizeof("units")-1);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd, series_node_get_desc_reply, sample_set);
    sdsfree(cmd);
}

static int
series_store_metric_name(seriesQueryBaton *baton, series_sample_set_t *sample_set,
		sds series, int nelements, redisReply **elements)
{
    redisMapEntry	*entry;
    redisReply		*reply;
    sds			msg, key;
    unsigned int	i;
    int			sts = 0;

    key = sdsnewlen(NULL, 20);
    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if (reply->type == REDIS_REPLY_STRING) {
	    sdsclear(key);
	    key = sdscatlen(key, reply->str, reply->len);
	    if ((entry = redisMapLookup(namesmap, key)) != NULL) {
		sample_set->metric_name = redisMapValue(entry);
	    } else {
		infofmt(msg, "%s - timeseries string map", series);
		batoninfo(baton, PMLOG_CORRUPT, msg);
		sts = -EINVAL;
	    }
	} else {
	    infofmt(msg, "expected string in %s set (type=%s)",
			series, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	}
    }
    sdsfree(key);

    return sts;
}

static void
series_node_get_metric_name_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    series_sample_set_t		*sample_set = (series_sample_set_t *) arg;
    seriesQueryBaton		*baton = (seriesQueryBaton *)sample_set->baton;
    redisReply			*reply = r;
    int				sts;
    sds				msg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_node_get_metric_name_reply");

    /* unpack - extract names for this source via context name map */
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from %s %s (type=%s)",
		SMEMBERS, sample_set->sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else if ((sts = series_store_metric_name(baton, sample_set, sample_set->sid->name,
			reply->elements, reply->element)) < 0) {
	baton->error = sts;
    }
    series_query_end_phase(baton);
}

static void
series_node_get_metric_name(
	seriesQueryBaton *baton, seriesGetSID *sid, series_sample_set_t *sample_set)
{
    sds cmd, key;

    seriesBatonReference(baton, "series_node_get_metric_name");
    key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%S", sid->name);
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
    cmd = redis_param_sds(cmd, key);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
		    series_node_get_metric_name_reply, sample_set);
    sdsfree(cmd);
}

/* 
 * Redis has returned replies about samples of series, save them into the corresponding node.
 */
static void
series_node_prepare_time_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    node_t			*np = (node_t *)arg;
    seriesQueryBaton		*baton = (seriesQueryBaton *)np->baton;
    redisReply			*reply = r;
    sds				msg;
    int				idx = np->value_set.num_series;
    seriesGetSID		*sid = np->value_set.series_values[idx].sid;

    /* 
     * Got an reply contains series values which need to be saved into the corresponding 
     * node, but when this callback function be called, we need the information of SID of the
     * series. Here we can not access SID unless store SIDs in node. That's why I add **SID into
     * struct node_t in query.h
     */
    seriesBatonCheckMagic(sid, MAGIC_SID, "series_node_prepare_time_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_node_prepare_time_reply");

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from %s XSTREAM values (type=%s)",
		sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else {
	/* calloc space to store series samples */
	np->value_set.series_values[idx].num_samples = reply->elements;
	if ((np->value_set.series_values[idx].series_sample =
	    (series_instance_set_t *)calloc(reply->elements, sizeof(series_instance_set_t))) == NULL) {
	    /* TODO: error report here */
	    baton->error = -ENOMEM;
	}
	/* Query for the desc of idx-th series */
	np->value_set.series_values[idx].baton = baton;
	series_node_get_desc(baton, sid->name, &np->value_set.series_values[idx]);
	series_node_get_metric_name(baton, sid, &np->value_set.series_values[idx]);
	
	series_values_store_to_node(baton, sid->name, reply->elements, reply->element, np);
	np->value_set.num_series++;
    }
    series_query_end_phase(baton);
}

static void
series_node_prepare_time(seriesQueryBaton *baton, series_set_t *query_series_set, node_t *np)
{
    timing_t			*tp = &np->time;
    unsigned char		*series = query_series_set->series;
    seriesGetSID		*sid;
    char			buffer[64], revbuf[64];
    sds				start, end, key, cmd;
    unsigned int		i, revlen = 0, reverse = 0;
    int				nseries = query_series_set->nseries;

    /* if only 'count' is requested, work back from most recent value */
    if ((reverse = series_value_count_only(tp)) != 0) {
	revlen = pmsprintf(revbuf, sizeof(revbuf), "%u", reverse);
	start = sdsnew("+");
    } else {
	start = sdsnew(timeval_stream_str(&tp->start, buffer, sizeof(buffer)));
    }

    if (pmDebugOptions.series)
	fprintf(stderr, "START: %s\n", start);

    if (reverse)
	end = sdsnew("-");
    else if (tp->end.tv_sec)
	end = sdsnew(timeval_stream_str(&tp->end, buffer, sizeof(buffer)));
    else
	end = sdsnew("+");	/* "+" means "no end" - to the most recent */

    if (pmDebugOptions.series)
	fprintf(stderr, "END: %s\n", end);
    

    /* calloc nseries samples store space */
    if ((np->value_set.series_values =
    	(series_sample_set_t *)calloc(nseries, sizeof(series_sample_set_t))) == NULL) {
	baton->error = -ENOMEM;
	sdsfree(start);
	sdsfree(end);
	return;
    }

    /*
     * Query cache for the time series range (groups of instance:value
     * pairs, with an associated timestamp).
     */
    for (i = 0; i < nseries; i++, series += SHA1SZ) {
	sid = calloc(1, sizeof(seriesGetSID));
	pmwebapi_hash_str(series, buffer, sizeof(buffer));

	initSeriesGetSID(sid, buffer, 1, baton);
	seriesBatonReference(baton, "series_prepare_time");

	key = sdscatfmt(sdsempty(), "pcp:values:series:%S", sid->name);

	/* X[REV]RANGE key t1 t2 [count N] */
	if (reverse) {
	    cmd = redis_command(6);
	    cmd = redis_param_str(cmd, XREVRANGE, XREVRANGE_LEN);
	} else {
	    cmd = redis_command(4);
	    cmd = redis_param_str(cmd, XRANGE, XRANGE_LEN);
	}
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, start);
	cmd = redis_param_sds(cmd, end);
	if (reverse) {
	    cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
	    cmd = redis_param_str(cmd, revbuf, revlen);
	}
	sdsfree(key);
	np->value_set.series_values[i].baton = baton;
	np->value_set.series_values[i].sid = sid;
	/* Note: np->series_set.num_series is not equal to nseries in this function */
	redisSlotsRequest(baton->slots, cmd,
				series_node_prepare_time_reply, np);
	sdsfree(cmd);
	
    }
    sdsfree(start);
    sdsfree(end);
}

/* 
 * When we encounter a data node (i.e. np->result.nseries!=0),
 * query Redis for actual values and store them into this node
 * because time series identifier will always be described in
 * the top node of a subtree at the parser tree's bottom. 
 */
static int
series_process_func(seriesQueryBaton *baton, node_t *np, int level)
{
    int		sts, nelements = 0;

    if (np == NULL)
	return 0;

    if ((nelements = np->result.nseries) != 0) {
	np->value_set.num_series = 0;
	np->baton = baton;
	series_node_prepare_time(baton, &np->result, np);
	return baton->error;
    }

    if ((sts = series_process_func(baton, np->left, level+1)) < 0)
	return sts;
    return series_process_func(baton, np->right, level+1);
}

static sds
series_expr_canonical(node_t *np, int idx)
{
    sds		left, right, metric = NULL, statement = NULL;

    if (np == NULL)
	return sdsempty();

    /* first find each of the left and right hand sides, if any */
    switch (np->type) {
    case N_INTEGER:
    case N_NAME:
    case N_DOUBLE:
    case N_STRING:
    case N_SCALE:
	statement = sdsdup(np->value);
	left = right = NULL; /* statement is a leaf in the expr tree */
	break;

    case N_EQ:
	if (np->left->type == N_NAME &&
	    strncmp(np->left->value, "metric.name", sdslen(np->left->value)) == 0) {
	    left = sdsempty();
	} else
	    left = series_expr_canonical(np->left, idx);
	right = series_expr_canonical(np->right, idx);
	break;

    case N_LT:
    case N_LEQ:
    case N_GLOB:
    case N_GEQ:
    case N_GT:
    case N_NEQ:
    case N_AND:
    case N_OR:
    case N_REQ:
    case N_RNE:
    case N_PLUS:
    case N_MINUS:
    case N_STAR:
    case N_SLASH:
    case N_RESCALE:
	left = series_expr_canonical(np->left, idx);
	right = series_expr_canonical(np->right, idx);
	break;

    case N_AVG:
    case N_AVG_INST:
    case N_AVG_SAMPLE:
    case N_SUM:
    case N_SUM_INST:
    case N_SUM_SAMPLE:
    case N_STDEV_INST:
    case N_STDEV_SAMPLE:
    case N_TOPK_INST:
    case N_TOPK_SAMPLE:
    case N_NTH_PERCENTILE_INST:
    case N_NTH_PERCENTILE_SAMPLE:
	left = series_expr_canonical(np->left, idx);
	right = series_expr_canonical(np->right, idx);
	break;
    case N_MAX:
    case N_MAX_INST:
    case N_MAX_SAMPLE:
    case N_MIN:
    case N_MIN_INST:
    case N_MIN_SAMPLE:
    case N_RATE:
    case N_ABS:
    case N_FLOOR:
    case N_SQRT:
    case N_ROUND:
	left = series_expr_canonical(np->left, idx);
	right = NULL;
	break;

    case N_LOG:
	left = series_expr_canonical(np->left, idx);
	right = np->right ? series_expr_canonical(np->right, idx) : NULL;
	break;
    default: 
	left = right = NULL;
	break;
    }

    /* form a merged canonical expression from component parts */
    switch (np->type) {
    case N_PLUS:
	statement = sdscatfmt(sdsempty(), "%S+%S", left, right);
	break;
    case N_MINUS:
	statement = sdscatfmt(sdsempty(), "%S-%S", left, right);
	break;
    case N_STAR:
	statement = sdscatfmt(sdsempty(), "%S*%S", left, right);
	break;
    case N_SLASH:
	statement = sdscatfmt(sdsempty(), "%S/%S", left, right);
	break;
    case N_AVG:
	statement = sdscatfmt(sdsempty(), "avg(%S)", left);
	break;
    case N_AVG_INST:
	statement = sdscatfmt(sdsempty(), "avg_inst(%S)", left);
	break;
    case N_AVG_SAMPLE:
	statement = sdscatfmt(sdsempty(), "avg_sample(%S)", left);
	break;
    case N_COUNT:
	statement = sdscatfmt(sdsempty(), "count(%S)", left);
	break;
    case N_DELTA:
	break;
    case N_MAX:
	statement = sdscatfmt(sdsempty(), "max(%S)", left);
	break;
    case N_MAX_INST:
	statement = sdscatfmt(sdsempty(), "max_inst(%S)", left);
	break;
    case N_MAX_SAMPLE:
	statement = sdscatfmt(sdsempty(), "max_sample(%S)", left);
	break;
    case N_MIN:
	statement = sdscatfmt(sdsempty(), "min(%S)", left);
	break;
    case N_MIN_INST:
	statement = sdscatfmt(sdsempty(), "min_inst(%S)", left);
	break;
    case N_MIN_SAMPLE:
	statement = sdscatfmt(sdsempty(), "min_sample(%S)", left);
	break;
    case N_SUM:
	statement = sdscatfmt(sdsempty(), "sum(%S)", left);
	break;
    case N_SUM_INST:
	statement = sdscatfmt(sdsempty(), "sum_inst(%S)", left);
	break;
    case N_SUM_SAMPLE:
	statement = sdscatfmt(sdsempty(), "sum_sample(%S)", left);
	break;
    case N_STDEV_INST:
	statement = sdscatfmt(sdsempty(), "stdev_inst(%S)", left);
	break;
    case N_STDEV_SAMPLE:
	statement = sdscatfmt(sdsempty(), "stdev_sample(%S)", left);
	break;
    case N_TOPK_INST:
	statement = sdscatfmt(sdsempty(), "topk_inst(%S, %S)", left, right);
	break;
    case N_TOPK_SAMPLE:
	statement = sdscatfmt(sdsempty(), "topk_sample(%S, %S)", left, right);
        break;
    case N_NTH_PERCENTILE_INST:
	statement = sdscatfmt(sdsempty(), "nth_percentile_inst(%S, %S)", left, right);
	break;
    case N_NTH_PERCENTILE_SAMPLE:
	statement = sdscatfmt(sdsempty(), "nth_percentile_inst(%S, %S)", left, right);
	break;
    case N_ANON:
	break;
    case N_RATE:
	statement = sdscatfmt(sdsempty(), "rate(%S)", left);
	break;
    case N_INSTANT:
	break;
    case N_LT:
	statement = sdscatfmt(sdsempty(), "%S<%S", left, right);
	break;
    case N_LEQ:
	statement = sdscatfmt(sdsempty(), "%S<=%S", left, right);
	break;
    case N_EQ:
	metric = sdsnew("metric.name");
	if (np->left->type == N_NAME && sdscmp(np->left->value, metric) == 0)
	    statement = sdsdup(right);
	else
	    statement = sdscatfmt(sdsempty(), "%S==\"%S\"", left, right);
	break;
    case N_GLOB:
	metric = sdsnew("metric.name");
	if (np->left->type == N_NAME && sdscmp(np->left->value, metric) == 0)
	    statement = sdscatfmt(sdsempty(), "%S", np->value_set.series_values[idx].metric_name);
	else
	    statement = sdscatfmt(sdsempty(), "%S~~\"%S\"", left, right);
	break;
    case N_GEQ:
	statement = sdscatfmt(sdsempty(), "%S>=%S", left, right);
	break;
    case N_GT:
	statement = sdscatfmt(sdsempty(), "%S>%S", left, right);
	break;
    case N_NEQ:
	statement = sdscatfmt(sdsempty(), "%S!=\"%S\"", left, right);
	break;
    case N_AND:
	metric = sdsnew("metric.name");
	if ((np->left->type == N_EQ || np->left->type == N_GLOB)
		&& sdscmp(np->left->left->value, metric) == 0)
	    statement = sdscatfmt(sdsempty(), "%S{%S}", np->value_set.series_values[idx].metric_name, right);
	else
	    statement = sdscatfmt(sdsempty(), "%S&&%S", left, right);
	break;
    case N_OR:
	statement = sdscatfmt(sdsempty(), "%S||%S", left, right);
	break;
    case N_REQ:
	statement = sdscatfmt(sdsempty(), "%S=~%S", left, right);
	break;
    case N_RNE:
	statement = sdscatfmt(sdsempty(), "%S!~%S", left, right);
	break;
    case N_NEG:
	break;
    case N_RESCALE:
	statement = sdscatfmt(sdsempty(), "rescale(%S,\"%S\")", left, right);
	break;
    case N_DEFINED:
	break;
    case N_ABS:
	statement = sdscatfmt(sdsempty(), "abs(%S)", left);
	break;
    case N_FLOOR:
	statement = sdscatfmt(sdsempty(), "floor(%S)", left);
	break;
    case N_LOG:
	if (np->right == NULL)
	    statement = sdscatfmt(sdsempty(), "log(%S)", left);
	else
	    statement = sdscatfmt(sdsempty(), "log(%S,%S)", left, right);
	break;
    case N_SQRT:
	statement = sdscatfmt(sdsempty(), "sqrt(%S)", left);
	break;
    case N_ROUND:
	statement = sdscatfmt(sdsempty(), "round(%S)", left);
	break;
    default:
	break;
    }
    sdsfree(left);
    sdsfree(right);
    sdsfree(metric);
    return statement ? statement : sdsempty();
}

static sds
series_function_hash(unsigned char *hash, node_t *np, int idx)
{
    sds		identifier = series_expr_canonical(np, idx);
    SHA1_CTX	shactx;
    const char	prefix[] = "{\"series\":\"expr\",\"expr\":\"";
    const char	suffix[] = "\"}";

    if (pmDebugOptions.query)
	fprintf(stderr, "%s: canonical expr: \"%s\"\n", __FUNCTION__, identifier);
    SHA1Init(&shactx);
    SHA1Update(&shactx, (unsigned char *)prefix, sizeof(prefix)-1);
    SHA1Update(&shactx, (unsigned char *)identifier, sdslen(identifier));
    SHA1Update(&shactx, (unsigned char *)suffix, sizeof(suffix)-1);
    SHA1Final(hash, &shactx);

    return identifier;
}

/*
 * Report a timeseries result - timestamps and (instance) values from a node
 */
static void
series_node_values_report(seriesQueryBaton *baton, node_t *np)
{
    sds		series;
    int		i, j, k;

    for (i = 0; i < np->value_set.num_series; i++) {
	series = np->value_set.series_values[i].sid->name;
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		pmSeriesValue value = np->value_set.series_values[i].series_sample[j].series_instance[k];
		baton->callbacks->on_value(series, &value, baton->userdata);
	    }
	}
    }
}

static int
series_rate_check(pmSeriesDesc desc)
{
    /* TODO: Do type check for rate function. return 0 when success. */
    if (strncmp(desc.semantics, "counter", sizeof("counter")-1) != 0)
	return 1;
    return 0;
}

/*
 * Compute rate between samples for each metric.
 * The number of samples in result is one less than the original samples. 
 */
static void
series_calculate_rate(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    pmSeriesValue	s_pmval, t_pmval;
    unsigned int	n_instances, n_samples, i, j, k;
    double		s_data, t_data, mult;
    char		str[256];
    sds			msg, expr;
    int			sts;
    pmUnits		units = {0};

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	n_samples = np->value_set.series_values[i].num_samples;
	if (series_rate_check(np->value_set.series_values[i].series_desc) == 0) {
	    if (n_samples > 0) {
		n_instances = np->value_set.series_values[i].series_sample[0].num_instances;
	    }
	    for (j = 1; j < n_samples; j++) {
		if (np->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
		    if (pmDebugOptions.query && pmDebugOptions.desperate)
			fprintf(stderr, "Error: number of instances in each sample are not equal %d != %d.\n",
				np->value_set.series_values[i].series_sample[j].num_instances, n_instances);
		    continue;
		}
		for (k = 0; k < n_instances; k++) {
		    t_pmval = np->value_set.series_values[i].series_sample[j-1].series_instance[k];
		    s_pmval = np->value_set.series_values[i].series_sample[j].series_instance[k];
		    if (strcmp(s_pmval.series, t_pmval.series) != 0) {
			/* TODO: two SIDs of the instances' names between samples are different, report error. */
			if (pmDebugOptions.query) {
			    fprintf(stderr, "TODO: two SIDs of the instances' names between samples are different, report error.");
			    fprintf(stderr, "%s %s\n", s_pmval.series, t_pmval.series);
			}
		    }

		    /* compute rate/sec from delta value and delta timestamp */
		    s_data = strtod(s_pmval.data, NULL);
		    t_data = strtod(t_pmval.data, NULL);
		    pmsprintf(str, sizeof(str), "%.6lf", (t_data - s_data) / pmTimespec_delta(&t_pmval.ts, &s_pmval.ts));

		    sdsfree(np->value_set.series_values[i].series_sample[j-1].series_instance[k].data);
		    sdsfree(np->value_set.series_values[i].series_sample[j-1].series_instance[k].timestamp);
		    np->value_set.series_values[i].series_sample[j-1].series_instance[k].data = sdsnew(str);
		    np->value_set.series_values[i].series_sample[j-1].series_instance[k].timestamp =
		    	sdsnew(np->value_set.series_values[i].series_sample[j].series_instance[k].timestamp);
		    np->value_set.series_values[i].series_sample[j-1].series_instance[k].ts =
		    	np->value_set.series_values[i].series_sample[j].series_instance[k].ts;
		}
		if (j == n_samples-1) {
		    /* Free the last sample */
		    for (k = 0; k < n_instances; k++) {
			sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].timestamp);
			sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].series);
			sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    }
		    np->value_set.series_values[i].num_samples -= 1;
		}
	    }
	} else {
	    expr = series_expr_canonical(np->left, i);
	    infofmt(msg, "Can't rate convert '%s', counter semantics required\n", expr);
	    sdsfree(expr);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -n_samples;
	}
	sdsfree(np->value_set.series_values[i].series_desc.type);
	sdsfree(np->value_set.series_values[i].series_desc.semantics);
	if ((sts = pmParseUnitsStr(
			np->value_set.series_values[i].series_desc.units,
			&units, &mult, &msg)) < 0) {
	    free(msg);
	}
	sdsfree(np->value_set.series_values[i].series_desc.units);
	units.dimTime -= 1;
	units.scaleTime = PM_TIME_SEC;
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.semantics = sdsnew("instant");
	np->value_set.series_values[i].series_desc.units = sdsnew(pmUnitsStr(&units));
    }
}

/*
 * Compare and pick the max instance value(s) among samples.
 */
static void
series_calculate_time_domain_max(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		max_data, data;
    int			max_pointer;
    sds			msg;
    pmSeriesValue	inst;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;

	    for (j = 0; j < n_samples; j++) {
		np->value_set.series_values[i].series_sample[j].num_instances = 1;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(1, sizeof(pmSeriesValue));

		max_pointer = 0;
		max_data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[0].data);
		for (k = 1; k < n_instances; k++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }                
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    if (max_data < data) {
			max_data = data;
			max_pointer = k;
		    }
		}
		inst = np->left->value_set.series_values[i].series_sample[j].series_instance[max_pointer];

		np->value_set.series_values[i].series_sample[j].series_instance[0].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[j].series_instance[0].series = sdsnew(inst.series);
		np->value_set.series_values[i].series_sample[j].series_instance[0].data = sdsnew(inst.data);
		np->value_set.series_values[i].series_sample[j].series_instance[0].ts = inst.ts;
	    }
        } else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * Compare and pick the maximal instance value(s) among samples for each metric.
 */
static void
series_calculate_max(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		max_data, data;
    int			max_pointer;
    sds			msg;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = 1;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(1, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].series_sample[0].num_instances = n_instances;
	    np->value_set.series_values[i].series_sample[0].series_instance = (pmSeriesValue *)calloc(n_instances, sizeof(pmSeriesValue));
	    for (k = 0; k < n_instances; k++) {
		max_pointer = 0;
		max_data = atof(np->left->value_set.series_values[i].series_sample[0].series_instance[k].data);
		for (j = 1; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    if (max_data < data) {
			max_data = data;
			max_pointer = j;
		    }
		}
		np->value_set.series_values[i].series_sample[0].series_instance[k].timestamp = 
			sdsnew(np->left->value_set.series_values[i].series_sample[max_pointer].series_instance[k].timestamp);
		np->value_set.series_values[i].series_sample[0].series_instance[k].series = 
			sdsnew(np->left->value_set.series_values[i].series_sample[max_pointer].series_instance[k].series);
		np->value_set.series_values[i].series_sample[0].series_instance[k].data = 
			sdsnew(np->left->value_set.series_values[i].series_sample[max_pointer].series_instance[k].data);
		np->value_set.series_values[i].series_sample[0].series_instance[k].ts = 
			np->left->value_set.series_values[i].series_sample[max_pointer].series_instance[k].ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * Compare and pick the minimal value(s) among samples for each metric across time.
 */
static void
series_calculate_time_domain_min(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		min_data, data;
    int			min_pointer;
    sds			msg;
    pmSeriesValue	inst;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;

	    for (j = 0; j < n_samples; j++) {
		np->value_set.series_values[i].series_sample[j].num_instances = 1;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(1, sizeof(pmSeriesValue));

		min_pointer = 0;
		min_data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[0].data);
		for (k = 1; k < n_instances; k++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }                
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    if (min_data > data) {
			min_data = data;
			min_pointer = k;
		    }
		}
		inst = np->left->value_set.series_values[i].series_sample[j].series_instance[min_pointer];

		np->value_set.series_values[i].series_sample[j].series_instance[0].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[j].series_instance[0].series = sdsnew(inst.series);
		np->value_set.series_values[i].series_sample[j].series_instance[0].data = sdsnew(inst.data);
		np->value_set.series_values[i].series_sample[j].series_instance[0].ts = inst.ts;
	    }
        } else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * Compare and pick the minimal instance value(s) among samples for each metric.
 */
static void
series_calculate_min(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		min_data, data;
    int			min_pointer;
    sds			msg;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = 1;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(1, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].series_sample[0].num_instances = n_instances;
	    np->value_set.series_values[i].series_sample[0].series_instance = (pmSeriesValue *)calloc(n_instances, sizeof(pmSeriesValue));
	    for (k = 0; k < n_instances; k++) {
		min_pointer = 0;
		min_data = atof(np->left->value_set.series_values[i].series_sample[0].series_instance[k].data);
		for (j = 1; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    if (min_data > data) {
			min_data = data;
			min_pointer = j;
		    }
		}
		np->value_set.series_values[i].series_sample[0].series_instance[k].timestamp = 
			sdsnew(np->left->value_set.series_values[i].series_sample[min_pointer].series_instance[k].timestamp);
		np->value_set.series_values[i].series_sample[0].series_instance[k].series = 
			sdsnew(np->left->value_set.series_values[i].series_sample[min_pointer].series_instance[k].series);
		np->value_set.series_values[i].series_sample[0].series_instance[k].data = 
			sdsnew(np->left->value_set.series_values[i].series_sample[min_pointer].series_instance[k].data);
		np->value_set.series_values[i].series_sample[0].series_instance[k].ts = 
			np->left->value_set.series_values[i].series_sample[min_pointer].series_instance[k].ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

static int
compare_pmUnits_dim(pmUnits *a, pmUnits *b)
{
    if (a->dimCount == b->dimCount &&
	a->dimTime == b->dimTime &&
	a->dimSpace == b->dimSpace)
	return 0;
    return -1;
}

static int
series_extract_type(char *typeStr) 
{
    if (strncmp("32", typeStr, sizeof("32")-1) == 0) {
	return PM_TYPE_32;
    } else if ((strncmp("U32", typeStr, sizeof("U32")-1) == 0) || (strncmp("u32", typeStr, sizeof("u32")-1) == 0)) {
	return PM_TYPE_U32;
    } else if (strncmp("64", typeStr, sizeof("64")-1) == 0) {
	return PM_TYPE_64;
    } else if ((strncmp("U64", typeStr, sizeof("U64")-1) == 0) || (strncmp("u64", typeStr, sizeof("u64")-1) == 0)) {
	return PM_TYPE_U64;
    } else if ((strncmp("FLOAT", typeStr, sizeof("FLOAT")-1) == 0) || (strncmp("float", typeStr, sizeof("float")-1) == 0)) {
	return PM_TYPE_FLOAT;
    } else if ((strncmp("DOUBLE", typeStr, sizeof("DOUBLE")-1) == 0) || (strncmp("double", typeStr, sizeof("double")-1) == 0)) {
	return PM_TYPE_DOUBLE;
    } else {
	return PM_TYPE_UNKNOWN;
    }
}

static int
series_extract_value(int type, sds str, pmAtomValue *oval) 
{
    int		sts;

    switch (type) {
    case PM_TYPE_32:
	sts = sscanf(str, "%d", &oval->l);
	break;
    case PM_TYPE_U32:
	sts = sscanf(str, "%u", &oval->ul);
	break;
    case PM_TYPE_64:
	sts = sscanf(str, "%" PRId64, &oval->ll);
	break;
    case PM_TYPE_U64:
	sts = sscanf(str, "%" PRIu64, &oval->ull);
	break;
    case PM_TYPE_FLOAT:
	sts = sscanf(str, "%f", &oval->f);
	break;
    case PM_TYPE_DOUBLE:
	sts = sscanf(str, "%lf", &oval->d);
	break;
    default:
	sts = 0;
	break;
    }
    return (sts == 1) ? 0 : PM_ERR_CONV;
}

static int
series_pmAtomValue_conv_str(int type, char *str, pmAtomValue *val, int max_len)
{
    char	*s;

    switch (type) {
    case PM_TYPE_32:
    case PM_TYPE_U32:
    case PM_TYPE_64:
    case PM_TYPE_U64:
    case PM_TYPE_FLOAT:
    case PM_TYPE_DOUBLE:
        s = pmAtomStr_r(val, type, str, max_len);
	if (s && (isdigit(*s) || *s == '-' || *s == '+'))
            return strlen(str);
        break;

    default:
        s = NULL;
        break;
    }

    pmNotifyErr(LOG_ERR, "series_pmAtomValue_conv_str: type=%s failed: %s\n",
            pmTypeStr(type), s ? s : "only numeric types supported");
    return 0;
}

/* 
 * The left child node of L_RESCALE should contains a set of time
 * series values.  And the right child node should be L_SCALE, which
 * contains the target units information.  This rescale() should only
 * accept metrics with semantics instant.  Compare the consistencies
 * of 3 time/space/count dimensions between the pmUnits of input and
 * metrics to be modified. 
 */
static void
series_calculate_rescale(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    double		mult;
    pmUnits		iunit;
    char		*errmsg, str_val[256];
    pmAtomValue		ival, oval;
    int			type, sts, str_len, i, j, k;
    sds			msg;

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if (pmParseUnitsStr(np->value_set.series_values[i].series_desc.units, &iunit, &mult, &errmsg) < 0) {
	    infofmt(msg, "Units string of %s parse error, %s\n", np->value_set.series_values[i].sid->name, errmsg);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    free(errmsg);
	    return;
	}
	if (compare_pmUnits_dim(&iunit, &np->right->meta.units) != 0) {
	    infofmt(msg, "Dimensions of units mismatch, for series %s the units is %s\n", 
		np->value_set.series_values[i].sid->name, np->value_set.series_values[i].series_desc.units);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}
	if ((type = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}
	type = PM_TYPE_DOUBLE;
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(type, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &ival) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = pmConvScale(type, &ival, &iunit, &oval, &np->right->meta.units)) != 0) {
		    /* TODO: rescale error report */
		    fprintf(stderr, "rescale error\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(type, str_val, &oval, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	sdsfree(np->value_set.series_values[i].series_desc.units);
	np->value_set.series_values[i].series_desc.units = sdsnew(pmUnitsStr(&np->right->meta.units));
    }
}

static int
series_abs_pmAtomValue(int type, pmAtomValue *val)
{
    int		sts = 0;

    switch (type) {
    case PM_TYPE_32:
	if (val->l < 0)
	    val->l = -val->l;
	break;
    case PM_TYPE_64:
	if (val->ll < 0)
	    val->ll = -val->ll;
	break;
    case PM_TYPE_U32:
    case PM_TYPE_U64:
	/* No need to change value */
	break;
    case PM_TYPE_FLOAT:
	if (val->f < 0)
	    val->f = -val->f;
	break;
    case PM_TYPE_DOUBLE:
	if (val->d < 0)
	    val->d = -val->d;
	break;
    default:
	/* Unsupported type */
	sts = -1;
	break;
    }
    return sts;
}

static void
series_calculate_abs(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    pmAtomValue		val;
    int			type, sts, str_len, i, j, k;
    char		str_val[256];
    sds			msg;

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if ((type = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}	
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(type, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &val) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = series_abs_pmAtomValue(type, &val)) != 0) {
		    /* TODO: unsupported type */
		    fprintf(stderr, "Unsupport type to take abs()\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(type, str_val, &val, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
    }
}

/*
 * calculate top k instances among samples
 */
static void
series_calculate_time_domain_topk(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k, l;
    sds			msg;
    pmSeriesValue	inst;
    int			n, ind;
    double		data;
    double		*topk_data;
    int			*topk_pointer;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0){
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;

	    for (j = 0; j < n_samples; j++){
		sscanf(np->right->value, "%d", &n);
		if (n > n_instances){
		    n = n_instances;
		}
		topk_data = (double*) calloc(n, sizeof(double));
		topk_pointer = (int*) calloc(n, sizeof(int));
		np->value_set.series_values[i].series_sample[j].num_instances = n;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(n, sizeof(pmSeriesValue));

		for (k = 0; k < n_instances; k++){
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
		    continue;
		    }                
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    if (data > topk_data[n-1]){
			for (l = 0; l < n; ++l){
			    if (data > topk_data[l]){
				// insert in to position l
				for (ind = n - 1; ind > l; --ind){
				    topk_data[ind] = topk_data[ind-1];
				    topk_pointer[ind] = topk_pointer[ind-1];
				}
				topk_data[l] = data;
				topk_pointer[l] = k;
			    	break;
			    }
			}
		    }
		}

		for (l = 0; l < n; ++l){
		    inst = np->left->value_set.series_values[i].series_sample[j].series_instance[topk_pointer[l]];
		    np->value_set.series_values[i].series_sample[j].series_instance[l].timestamp = sdsnew(inst.timestamp);
		    np->value_set.series_values[i].series_sample[j].series_instance[l].series = sdsnew(inst.series);
		    np->value_set.series_values[i].series_sample[j].series_instance[l].data = sdsnew(inst.data);
		    np->value_set.series_values[i].series_sample[j].series_instance[l].ts = inst.ts;       
		}
		free(topk_data);
		free(topk_pointer);
	    }
	}
	else{
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }

}
/*
 * calculate top k series per-instance over time samples
 */
static void
series_calculate_topk(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k, l;
    double		data;
    int 		n, ind;
    double		*topk_data;
    int			*topk_pointer;
    sds			msg;
    pmSeriesValue	inst;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	sscanf(np->right->value, "%d", &n);
	if (n > n_samples){
	    n = n_samples;
	}
	if (n_samples > 0) {
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].num_samples = n_instances;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_instances, sizeof(series_instance_set_t));
	    topk_data = (double*) calloc(n, sizeof(double));
	    topk_pointer = (int*) calloc(n, sizeof(int));
	    for (j = 0; j < n_instances; j++){
		np->value_set.series_values[i].series_sample[j].num_instances = n;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(n, sizeof(pmSeriesValue));
	    }
	    for (k = 0; k < n_instances; k++) {
		memset(topk_data, 0, sizeof(*topk_data));
		for (j = 0; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    if (data > topk_data[n-1]){
			for (l = 0; l < n; ++l){
			    if (data > topk_data[l]){
				// insert in to position l
				for (ind = n - 1; ind > l; --ind){
				    topk_data[ind] = topk_data[ind-1];
				    topk_pointer[ind] = topk_pointer[ind-1];
				}
				topk_data[l] = data;
				topk_pointer[l] = j;
			    	break;
			    }
			}
		    }
		}		
		for (l = 0; l < n; ++l){
		    inst = np->left->value_set.series_values[i].series_sample[topk_pointer[l]].series_instance[k];
		    np->value_set.series_values[i].series_sample[k].series_instance[l].timestamp = sdsnew(inst.timestamp);
		    np->value_set.series_values[i].series_sample[k].series_instance[l].series = sdsnew(inst.series);
		    np->value_set.series_values[i].series_sample[k].series_instance[l].data = sdsnew(inst.data);
		    np->value_set.series_values[i].series_sample[k].series_instance[l].ts = inst.ts;
		}
	    }
	    free(topk_data);
	    free(topk_pointer);
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * calculate standard deviation series per-instance over time samples
 */
static void
series_calculate_time_domain_standard_deviation(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		sum_data, mean, sd, data;
    sds			msg;
    pmSeriesValue	inst;
    char		stdev[64];

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;

	    for (j = 0; j < n_samples; j++) {
		np->value_set.series_values[i].series_sample[j].num_instances = 1;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(1, sizeof(pmSeriesValue));
		sum_data = 0.0;
		for (k = 0; k < n_instances; k++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
		    continue;
		    }
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sum_data += data;
		}

		mean = sum_data/n_instances;
		sd = 0.0;
		for (k = 0; k < n_instances; k++) {
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sd += pow(data - mean, 2);
		}

		pmsprintf(stdev, sizeof(stdev), "%le", sqrt(sd / n_instances));
		inst = np->left->value_set.series_values[i].series_sample[j].series_instance[0];
		np->value_set.series_values[i].series_sample[j].series_instance[0].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[j].series_instance[0].series = sdsnew(0);
		np->value_set.series_values[i].series_sample[j].series_instance[0].data = sdsnew(stdev);
		np->value_set.series_values[i].series_sample[j].series_instance[0].ts = inst.ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * calculate standard deviation series per-instance over time samples
 */
static void
series_calculate_standard_deviation(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		sum_data, data, sd, mean;
    char		stdev[64];
    sds			msg;
    pmSeriesValue       inst;

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = 1;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(1, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].series_sample[0].num_instances = n_instances;
	    np->value_set.series_values[i].series_sample[0].series_instance = (pmSeriesValue *)calloc(n_instances, sizeof(pmSeriesValue));
	    for (k = 0; k < n_instances; k++) {
		sum_data = 0.0;
		for (j = 0; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sum_data += data;
		}
		mean = sum_data/n_samples;
		sd = 0.0;
		for (j = 0; j < n_samples; j++) {
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sd += pow(data - mean, 2);
		}
		pmsprintf(stdev, sizeof(stdev), "%le", sqrt(sd / n_samples));
		inst = np->left->value_set.series_values[i].series_sample[0].series_instance[k];
		np->value_set.series_values[i].series_sample[0].series_instance[k].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[0].series_instance[k].series = sdsnew(inst.series);
		np->value_set.series_values[i].series_sample[0].series_instance[k].data = sdsnew(stdev);
		np->value_set.series_values[i].series_sample[0].series_instance[k].ts = inst.ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * calculate the nth percentile in the time series for each sample across time
 */
static void
series_calculate_time_domain_nth_percentile(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k, l, m;
    int			n, instance_idx, rank, *n_pointer;
    double              *n_data, data, rank_d;
    sds			msg;
    pmSeriesValue       inst;

    sscanf(np->right->value, "%d", &n);
    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    rank_d = ((double)n/100 * n_instances);
	    rank = (int) rank_d;
	    for (j = 0; j < n_samples; j++) {
		np->value_set.series_values[i].series_sample[j].num_instances = 1;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(1, sizeof(pmSeriesValue));
		n_data = (double*) calloc(n_instances, sizeof(double));
		n_pointer = (int*) calloc(n_instances, sizeof(int)); 

		for (k = 0; k < n_instances; k++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    for (l = 0; l < n_instances; ++l){
			if (data > n_data[l]){
			    for (m = n_instances - 1; m > l; --m){
				n_data[m] = n_data[m-1];
				n_pointer[m] = n_pointer[m-1];
			    }
			    n_data[l] = data;
			    n_pointer[l] = k;
			    break;
			}
		    }
		}

		if (rank == n_instances) {
		    instance_idx = n_pointer[0];
		} else {
		    instance_idx = n_pointer[n_instances-1-rank];
		}
		inst = np->left->value_set.series_values[i].series_sample[j].series_instance[instance_idx];
		np->value_set.series_values[i].series_sample[j].series_instance[0].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[j].series_instance[0].series = sdsnew(inst.series);
		np->value_set.series_values[i].series_sample[j].series_instance[0].data = sdsnew(inst.data);
		np->value_set.series_values[i].series_sample[j].series_instance[0].ts = inst.ts;
		free(n_data);
		free(n_pointer);
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * calculate the nth percentile series per-instance over time samples
 */
static void
series_calculate_nth_percentile(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k, l, m;
    int			n, instance_idx, rank, *n_pointer;
    double              *n_data, data, rank_d;
    sds			msg;
    pmSeriesValue       inst;

    sscanf(np->right->value, "%d", &n);

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = 1;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(1, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].series_sample[0].num_instances = n_instances;
	    np->value_set.series_values[i].series_sample[0].series_instance = (pmSeriesValue *)calloc(n_instances, sizeof(pmSeriesValue));
	    rank_d = ((double)n/100 * n_samples);
	    rank = (int) rank_d;
	    for (k = 0; k < n_instances; k++) {
		n_data = (double*) calloc(n_samples, sizeof(double));
		n_pointer = (int*) calloc(n_samples, sizeof(int)); 

		for (j = 1; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = atof(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data);
		    for (l = 0; l < n_samples; ++l){
			if (data > n_data[l]) {
			    for (m = n_samples - 1; m > l; --m){
				n_data[m] = n_data[m-1];
				n_pointer[m] = n_pointer[m-1];
			    }
			    n_data[l] = data;
			    n_pointer[l] = j;
			    break;
			}
		    }
		}
		if (rank == n_samples) {
		    instance_idx = n_pointer[0];
		} else {
		    instance_idx = n_pointer[n_samples-1-rank];
		}
		inst = np->left->value_set.series_values[i].series_sample[instance_idx].series_instance[k];
		np->value_set.series_values[i].series_sample[0].series_instance[k].timestamp = sdsnew(inst.timestamp);
		np->value_set.series_values[i].series_sample[0].series_instance[k].series = sdsnew(inst.series);
		np->value_set.series_values[i].series_sample[0].series_instance[k].data = sdsnew(inst.data);
		np->value_set.series_values[i].series_sample[0].series_instance[k].ts = inst.ts;
		free(n_data);
		free(n_pointer);
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew(np->left->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
    }
}

/*
 * calculate sum or avg in the time series for each sample across time
 */
static void
series_calculate_time_domain_statistical(node_t *np, nodetype_t func)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		sum_data, data;
    char		sum_data_str[64];
    sds			msg;

    assert(func == N_SUM_SAMPLE || func == N_AVG_SAMPLE);

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = n_samples;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(n_samples, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    for (j = 0; j < n_samples; j++) {
		np->value_set.series_values[i].series_sample[j].num_instances = 1;
		np->value_set.series_values[i].series_sample[j].series_instance = (pmSeriesValue *)calloc(1, sizeof(pmSeriesValue));
		
		sum_data = 0.0;
		for (k = 0; k < n_instances; k++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sum_data += data;
		}
		np->value_set.series_values[i].series_sample[j].series_instance[0].timestamp = 
			sdsnew(np->left->value_set.series_values[i].series_sample[j].series_instance[0].timestamp);
		np->value_set.series_values[i].series_sample[j].series_instance[0].series = 
			sdsnew(0);
		switch (func) {
		case N_SUM_SAMPLE:
		    pmsprintf(sum_data_str, sizeof(sum_data_str), "%le", sum_data);
		    break;
		case N_AVG_SAMPLE:
		    pmsprintf(sum_data_str, sizeof(sum_data_str), "%le", sum_data / n_instances);
		    break;
		default:
		    /* .. TODO: standard deviation, variance, mode, median, etc */
		    sum_data_str[0] = '\0';	/* for coverity */
		    assert(0);
		    break;
		}

		np->value_set.series_values[i].series_sample[j].series_instance[0].data = sdsnew(sum_data_str);
		np->value_set.series_values[i].series_sample[j].series_instance[0].ts = 
		np->left->value_set.series_values[i].series_sample[j].series_instance[0].ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;
	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);

	if (func == N_AVG_SAMPLE) {
	    np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	} else {
	    np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	}
    }
}

/*
 * calculate sum or avg series per-instance over time samples
 */
static void
series_calculate_statistical(node_t *np, nodetype_t func)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    unsigned int	n_series, n_samples, n_instances, i, j, k;
    double		sum_data, data;
    char		sum_data_str[64];
    sds			msg;

    assert(func == N_SUM || func == N_AVG || func == N_SUM_INST || func == N_AVG_INST);

    n_series = np->left->value_set.num_series;
    np->value_set.num_series = n_series;
    np->value_set.series_values = (series_sample_set_t *)calloc(n_series, sizeof(series_sample_set_t));
    for (i = 0; i < n_series; i++) {
	n_samples = np->left->value_set.series_values[i].num_samples;
	if (n_samples > 0) {
	    np->value_set.series_values[i].num_samples = 1;
	    np->value_set.series_values[i].series_sample = (series_instance_set_t *)calloc(1, sizeof(series_instance_set_t));
	    n_instances = np->left->value_set.series_values[i].series_sample[0].num_instances;
	    np->value_set.series_values[i].series_sample[0].num_instances = n_instances;
	    np->value_set.series_values[i].series_sample[0].series_instance = (pmSeriesValue *)calloc(n_instances, sizeof(pmSeriesValue));
	    for (k = 0; k < n_instances; k++) {
		sum_data = 0.0;
		for (j = 0; j < n_samples; j++) {
		    if (np->left->value_set.series_values[i].series_sample[j].num_instances != n_instances) {
			if (pmDebugOptions.query && pmDebugOptions.desperate) {
			    infofmt(msg, "number of instances in each sample are not equal\n");
			    batoninfo(baton, PMLOG_ERROR, msg);
			}
			continue;
		    }
		    data = strtod(np->left->value_set.series_values[i].series_sample[j].series_instance[k].data, NULL);
		    sum_data += data;
		}
		np->value_set.series_values[i].series_sample[0].series_instance[k].timestamp = 
			sdsnew(np->left->value_set.series_values[i].series_sample[0].series_instance[k].timestamp);
		np->value_set.series_values[i].series_sample[0].series_instance[k].series = 
			sdsnew(np->left->value_set.series_values[i].series_sample[0].series_instance[k].series);
		switch (func) {
		case N_SUM:
		case N_SUM_INST:
		    pmsprintf(sum_data_str, sizeof(sum_data_str), "%le", sum_data);
		    break;
		case N_AVG:
		case N_AVG_INST:
		    pmsprintf(sum_data_str, sizeof(sum_data_str), "%le", sum_data / n_samples);
		    break;
		default:
		    /* .. TODO: standard deviation, variance, mode, median, etc */
		    sum_data_str[0] = '\0';	/* for coverity */
		    assert(0);
		    break;
		}

		np->value_set.series_values[i].series_sample[0].series_instance[k].data = sdsnew(sum_data_str);
		np->value_set.series_values[i].series_sample[0].series_instance[k].ts = 
			np->left->value_set.series_values[i].series_sample[0].series_instance[k].ts;
	    }
	} else {
	    np->value_set.series_values[i].num_samples = 0;
	}
	np->value_set.series_values[i].sid = (seriesGetSID *)calloc(1, sizeof(seriesGetSID));
	np->value_set.series_values[i].sid->name = sdsnew(np->left->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].baton = np->left->value_set.series_values[i].baton;

	np->value_set.series_values[i].series_desc.indom = sdsnew(np->left->value_set.series_values[i].series_desc.indom);
	np->value_set.series_values[i].series_desc.pmid = sdsnew(np->left->value_set.series_values[i].series_desc.pmid);
	np->value_set.series_values[i].series_desc.source = sdsnew(np->left->value_set.series_values[i].series_desc.source);
	np->value_set.series_values[i].series_desc.type = sdsnew("double");
	np->value_set.series_values[i].series_desc.units = sdsnew(np->left->value_set.series_values[i].series_desc.units);
	
	if (func == N_AVG || func == N_AVG_INST) {
	    np->value_set.series_values[i].series_desc.semantics = sdsnew("instance");
	} else {
	    np->value_set.series_values[i].series_desc.semantics = sdsnew(np->left->value_set.series_values[i].series_desc.semantics);
	}
    }
}

static int
series_floor_pmAtomValue(int type, pmAtomValue *val)
{
    int			sts = 0;

    switch (type) {
    case PM_TYPE_32:
    case PM_TYPE_U32:
    case PM_TYPE_64:
    case PM_TYPE_U64:
	/* No change */
	break;
    case PM_TYPE_FLOAT:
	val->f = floorf(val->f);
	break;
    case PM_TYPE_DOUBLE:
	val->d = floor(val->d);
	break;
    default:
	/* Unsupported type */
	sts = -1;
	break;
    }

    return sts;
}

static void
series_calculate_floor(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    pmAtomValue		val;
    int			type, sts, str_len, i, j, k;
    char		str_val[256];
    sds			msg;

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if ((type = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}	
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(type, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &val) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = series_floor_pmAtomValue(type, &val)) != 0) {
		    /* TODO: unsupported type */
		    fprintf(stderr, "Unsupport type to take abs()\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(type, str_val, &val, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
    }
}

static int
series_log_pmAtomValue(int itype, int *otype, pmAtomValue *val, int is_natural_log, double base)
{
    int			sts = 0;
    double		res;

    switch (itype) {
    case PM_TYPE_32:
	res = val->l;
	break;
    case PM_TYPE_U32:
	res = val->ul;
	break;
    case PM_TYPE_64:
	res = val->ll;
	break;
    case PM_TYPE_U64:
	res = val->ull;
	break;
    case PM_TYPE_FLOAT:
	res = val->f;
	break;
    case PM_TYPE_DOUBLE:
	res = val->d;
	break;
    default:
	/* Unsupported type */
	sts = -1;
	break;
    }

    if (sts == 0) {
	*otype = PM_TYPE_DOUBLE;
	if (is_natural_log == 1)
	    val->d = log(res);
	else
	    val->d = log(res)/log(base);
    }

    return sts;
}

/*
 * Return the logarithm of x to base b (log_b^x).
 */
static void
series_calculate_log(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    double		base;
    pmAtomValue		val;
    int			i, j, k, itype, otype=PM_TYPE_UNKNOWN;
    int			sts, str_len, is_natural_log;
    char		str_val[256];
    sds			msg;


    if (np->right != NULL) {
	sscanf(np->right->value, "%lf", &base);
	is_natural_log = 0;
    } else {
	is_natural_log = 1;
    }
    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if ((itype = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(itype, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &val) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = series_log_pmAtomValue(itype, &otype, &val, is_natural_log, base)) != 0) {
		    /* TODO: unsupported type */
		    fprintf(stderr, "Unsupport type to take log()\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(otype, str_val, &val, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	sdsfree(np->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.type = sdsnew(pmTypeStr(otype));
    }
}

static int
series_sqrt_pmAtomValue(int itype, int *otype, pmAtomValue *val)
{
    int			sts = 0;
    double		res;

    switch (itype) {
    case PM_TYPE_32:
	*otype = PM_TYPE_DOUBLE;
	res = val->l;
	val->d = sqrt(res);
	break;
    case PM_TYPE_U32:
	*otype = PM_TYPE_DOUBLE;
	res = val->ul;
	val->d = sqrt(res);
	break;
    case PM_TYPE_64:
	*otype = PM_TYPE_DOUBLE;
	res = val->ll;
	val->d = sqrt(res);
	break;
    case PM_TYPE_U64:
	*otype = PM_TYPE_DOUBLE;
	res = val->ull;
	val->d = sqrt(res);
	break;
    case PM_TYPE_FLOAT:
	*otype = PM_TYPE_DOUBLE;
	res = val->f;
	val->d = sqrt(res);
	break;
    case PM_TYPE_DOUBLE:
	val->d = sqrt(val->d);
	break;
    default:
	/* Unsupported type */
	sts = -1;
	break;
    }
    return sts;
}

static void
series_calculate_sqrt(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    pmAtomValue		val;
    int			i, j, k, itype, otype=PM_TYPE_UNKNOWN;
    int			sts, str_len;
    char		str_val[256];
    sds			msg;

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if ((itype = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}	
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(itype, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &val) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = series_sqrt_pmAtomValue(itype, &otype, &val)) != 0) {
		    /* TODO: unsupported type */
		    fprintf(stderr, "Unsupport type to take sqrt()\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(otype, str_val, &val, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	sdsfree(np->value_set.series_values[i].series_desc.type);
	np->value_set.series_values[i].series_desc.type = sdsnew(pmTypeStr(otype));
    }
}

static int
series_round_pmAtomValue(int type, pmAtomValue *val)
{
    int			sts = 0;

    switch (type) {
    case PM_TYPE_32:
    case PM_TYPE_U32:
    case PM_TYPE_64:
    case PM_TYPE_U64:
	/* No change */
	break;
    case PM_TYPE_FLOAT:
	val->f = roundf(val->f);
	break;
    case PM_TYPE_DOUBLE:
	val->d = round(val->f);
	break;
    default:
	/* Unsupported type */
	sts = -1;
	break;
    }
    return sts;
}

static void
series_calculate_round(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    pmAtomValue		val;
    int			i, j, k, type, sts, str_len;
    char		str_val[256];
    sds			msg;

    np->value_set = np->left->value_set;
    for (i = 0; i < np->value_set.num_series; i++) {
	if ((type = series_extract_type(np->value_set.series_values[i].series_desc.type)) == PM_TYPE_UNKNOWN) {
	    infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    np->value_set.series_values[i].num_samples = -np->value_set.series_values[i].num_samples;
	    return;
	}
	for (j = 0; j < np->value_set.series_values[i].num_samples; j++) {
	    for (k = 0; k < np->value_set.series_values[i].series_sample[j].num_instances; k++) {
		if (series_extract_value(type, 
			np->value_set.series_values[i].series_sample[j].series_instance[k].data, &val) != 0 ) {
		    /* TODO: error report for extracting values from string fail */
		    fprintf(stderr, "Extract values from string fail\n");
		    return;
		}
		if ((sts = series_round_pmAtomValue(type, &val)) != 0) {
		    /* TODO: unsupported type */
		    fprintf(stderr, "Unsupport type to take abs()\n");
		    return;
		}
		if ((str_len = series_pmAtomValue_conv_str(type, str_val, &val, sizeof(str_val))) == 0)
		    return;
		sdsfree(np->value_set.series_values[i].series_sample[j].series_instance[k].data);
		np->value_set.series_values[i].series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	
    }
}

static int
pmStrSem(sds sem_str)
{
    if (strncmp("counter", sem_str, sizeof("counter")-1) == 0)
	return PM_SEM_COUNTER;
    if (strncmp("instant", sem_str, sizeof("instant")-1) == 0)
	return PM_SEM_INSTANT;
    if (strncmp("discrete", sem_str, sizeof("discrete")-1) == 0)
	return PM_SEM_DISCRETE;
    return -1;
}

static int
series_calculate_binary_check(int ope_type, seriesQueryBaton *baton,
	node_t *left, node_t *right, int *l_type, int *r_type,
	int *l_sem, int *r_sem, pmUnits *l_units, pmUnits *r_units,
	pmUnits *large_units, sds l_indom, sds r_indom)
{
    sds			msg;
    int			num_samples;
    double		mult;
    char		*errmsg = NULL;

    /*
     * Operands should have the same instance domain for all of
     * the binary operators.
     */
    if (sdscmp(l_indom, r_indom) != 0) {
	infofmt(msg, "Operands should have the same instance domain for all of the binary operators.\n");
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	return -1;
    }
    /*
     * For addition and subtraction all dimensions for each of
     * the operands and result are identical.
     */
    if ((ope_type == N_PLUS || ope_type == N_MINUS) &&
		compare_pmUnits_dim(&left->meta.units, &right->meta.units) != 0) {
	infofmt(msg, "Dimensions of two operands mismatch\n");
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	return -1;
    }

    /* Number of samples of both operands should be identical. */
    num_samples = left->value_set.series_values[0].num_samples;
    if (num_samples != right->value_set.series_values[0].num_samples) {
	infofmt(msg, "Number of samples of two metrics are not identical, %s has %d but %s has %d\n",
		left->value_set.series_values[0].sid->name, num_samples, 
		right->value_set.series_values[0].sid->name, right->value_set.series_values[0].num_samples);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	return -1;
    }

    /*
     * For an arithmetic expression:
     * - if both operands have the semantics of a counter, then only
     *   addition or subtraction is allowed
     * - if the left operand is a counter and the right operand is not,
     *   then only multiplication or division are allowed
     * - if the left operand is not a counter and the right operand is
     *   a counter, then only multiplication is allowed.
     */
    *l_sem = pmStrSem(left->value_set.series_values[0].series_desc.semantics);
    *r_sem = pmStrSem(right->value_set.series_values[0].series_desc.semantics);
    if (*l_sem == PM_SEM_COUNTER && *r_sem == PM_SEM_COUNTER) {
	if (ope_type != N_PLUS && ope_type != N_MINUS) {
	    infofmt(msg, "Both operands have the semantics of counter, only addition or subtraction is allowed.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return -1;
	}
    }
    if ((*l_sem == PM_SEM_COUNTER && *r_sem !=PM_SEM_COUNTER)) {
	if (ope_type != N_STAR && ope_type != N_SLASH) {
	    infofmt(msg, "Left operand is a counter and the right one is not, only multiplication or division is allowed.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return -1;
	}
    }
    if (*l_sem != PM_SEM_COUNTER && *r_sem ==PM_SEM_COUNTER) {
	if (ope_type != N_STAR) {
	    infofmt(msg, "Left operand is not a counter and the right one is, only multiplication is allowed.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return -1;
	}
    }

    /* Extract data tpyes of two operands */
    if ((*l_type = series_extract_type(left->value_set.series_values[0].series_desc.type)) == PM_TYPE_UNKNOWN) {
	infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	return -1;
    }
    if ((*r_type = series_extract_type(right->value_set.series_values[0].series_desc.type)) == PM_TYPE_UNKNOWN) {
	infofmt(msg, "Series values' Type extract fail, unsupported type\n");
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	return -1;
    }
    /* Extract units of both operands */
    if (pmParseUnitsStr(left->value_set.series_values[0].series_desc.units, l_units, &mult, &errmsg) < 0 &&
		strncmp(left->value_set.series_values[0].series_desc.units, "none", sizeof("none")-1) != 0) {
	infofmt(msg, "Units string of %s parse error, %s\n", left->value_set.series_values[0].sid->name, errmsg);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	free(errmsg);
	return -1;
    } else if (errmsg) {
	free(errmsg);
	errmsg = NULL;
    }
    if (pmParseUnitsStr(right->value_set.series_values[0].series_desc.units, r_units, &mult, &errmsg) < 0 &&
		strncmp(right->value_set.series_values[0].series_desc.units, "none", sizeof("none")-1) != 0) {
	infofmt(msg, "Units string of %s parse error, %s\n", right->value_set.series_values[0].sid->name, errmsg);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EPROTO;
	free(errmsg);
	errmsg = NULL;
	return -1;
    } else if (errmsg) {
	free(errmsg);
	errmsg = NULL;
    }
    /*
     * If both operands have a dimension of Count/Time/Space and the scales
     * are not the same, use the larger scale and convert the values of the
     * operand with the smaller scale.
     * The result is promoted to type PM_TYPE_DOUBLE.
     */
    large_units->scaleCount = l_units->scaleCount > r_units->scaleCount ? l_units->scaleCount : r_units->scaleCount;
    large_units->scaleSpace = l_units->scaleSpace > r_units->scaleSpace ? l_units->scaleSpace : r_units->scaleSpace;
    large_units->scaleTime = l_units->scaleTime > r_units->scaleTime ? l_units->scaleTime : r_units->scaleTime;
    if (large_units->scaleCount != l_units->scaleCount ||
	large_units->scaleSpace != l_units->scaleSpace ||
	large_units->scaleTime != l_units->scaleTime) {
	*l_type = PM_TYPE_DOUBLE;
    }
    if (large_units->scaleCount != r_units->scaleCount ||
	large_units->scaleSpace != r_units->scaleSpace ||
	large_units->scaleTime != r_units->scaleTime) {
	*r_type = PM_TYPE_DOUBLE;
    }

    return 0;
}

int
calculate_plus(int *type, pmAtomValue *l_val, pmAtomValue *r_val, pmAtomValue *res)
{
    switch (*type) {
    case PM_TYPE_32:
	res->l = l_val->l + r_val->l;
	break;
    case PM_TYPE_U32:
	res->ul = l_val->ul + r_val->ul;
	break;
    case PM_TYPE_64:
	res->ll = l_val->ll + r_val->ll;
	break;
    case PM_TYPE_U64:
	res->ull = l_val->ull + r_val->ull;
	break;
    case PM_TYPE_FLOAT:
	res->f = l_val->f + r_val->f;
	break;
    case PM_TYPE_DOUBLE:
	res->d = l_val->d + r_val->d;
	break;
    default:
	break;
    }
    return 0;
}

int
calculate_minus(int *type, pmAtomValue *l_val, pmAtomValue *r_val, pmAtomValue *res)
{
    switch (*type) {
    case PM_TYPE_32:
	res->l = l_val->l - r_val->l;
	break;
    case PM_TYPE_U32:
	if (l_val->ul < r_val->ul)
	    return -1;
	res->ul = l_val->ul - r_val->ul;
	break;
    case PM_TYPE_64:
	res->ll = l_val->ll - r_val->ll;
	break;
    case PM_TYPE_U64:
	if (l_val->ull < r_val->ull)
	    return -1;
	res->ull = l_val->ull - r_val->ull;
	break;
    case PM_TYPE_FLOAT:
	res->f = l_val->f - r_val->f;
	break;
    case PM_TYPE_DOUBLE:
	res->d = l_val->d - r_val->d;
	break;
    default:
	break;
    }
    return 0;
}

int
calculate_star(int *type, pmAtomValue *l_val, pmAtomValue *r_val, pmAtomValue *res)
{
    switch (*type) {
    case PM_TYPE_32:
	res->l = l_val->l * r_val->l;
	break;
    case PM_TYPE_U32:
	res->ul = l_val->ul * r_val->ul;
	break;
    case PM_TYPE_64:
	res->ll = l_val->ll * r_val->ll;
	break;
    case PM_TYPE_U64:
	res->ull = l_val->ull * r_val->ull;
	break;
    case PM_TYPE_FLOAT:
	res->f = l_val->f * r_val->f;
	break;
    case PM_TYPE_DOUBLE:
	res->d = l_val->d * r_val->d;
	break;
    default:
	break;
    }
    return 0;
}

int
calculate_slash(int *type, pmAtomValue *l_val, pmAtomValue *r_val, pmAtomValue *res)
{
    switch (*type) {
    case PM_TYPE_32:
	res->l = l_val->l / r_val->l;
	break;
    case PM_TYPE_U32:
	res->ul = l_val->ul / r_val->ul;
	break;
    case PM_TYPE_64:
	res->ll = l_val->ll / r_val->ll;
	break;
    case PM_TYPE_U64:
	res->ull = l_val->ull / r_val->ull;
	break;
    case PM_TYPE_FLOAT:
	res->f = l_val->f / r_val->f;
	break;
    case PM_TYPE_DOUBLE:
	res->d = l_val->d / r_val->d;
	break;
    default:
	break;
    }
    return 0;
}

static void
series_calculate_order_binary(int ope_type, int l_type, int r_type, int *otype,
	pmAtomValue *l_val, pmAtomValue *r_val,
	pmSeriesValue *l_data, pmSeriesValue *r_data,
	pmUnits *l_units, pmUnits *r_units, pmUnits *large_units,
	int (*operator)(int*, pmAtomValue*, pmAtomValue*, pmAtomValue*))
{
    pmAtomValue		res;
    int			str_len;
    char		str_val[256];

    if (l_type == PM_TYPE_DOUBLE || r_type == PM_TYPE_DOUBLE) {
	*otype = PM_TYPE_DOUBLE;
    } else if (ope_type == N_SLASH) {
	*otype = PM_TYPE_DOUBLE;
    } else if (l_type == PM_TYPE_FLOAT || r_type == PM_TYPE_FLOAT) {
	*otype = PM_TYPE_FLOAT;
    } else if (l_type == PM_TYPE_U64 || r_type == PM_TYPE_U64) {
	*otype = PM_TYPE_U64;
    } else if (l_type == PM_TYPE_64 || r_type == PM_TYPE_64) {
	*otype = PM_TYPE_64;
    } else if (l_type == PM_TYPE_U32 || r_type == PM_TYPE_U32) {
	*otype = PM_TYPE_U32;
    } else { /* both are PM_TYPE_32 */
	*otype = PM_TYPE_32;
    }

    /* Extract series values */
    series_extract_value(*otype, r_data->data, r_val);
    series_extract_value(*otype, l_data->data, l_val);

    /* Convert scale to larger one */
    if (pmConvScale(*otype, l_val, l_units, l_val, large_units) < 0)
    	memset(large_units, 0, sizeof(*large_units));
    if (pmConvScale(*otype, r_val, r_units, r_val, large_units) < 0)
    	memset(large_units, 0, sizeof(*large_units));

    if ((*operator)(otype, l_val, r_val, &res) != 0) {
	sdsfree(l_data->data);
	l_data->data = sdsnew("no value"); /* TODO - error handling */
    } else {
	sdsfree(l_data->data);
	str_len = series_pmAtomValue_conv_str(*otype, str_val, &res, sizeof(str_val));
	l_data->data = sdsnewlen(str_val, str_len);
    }
}

static void
series_binary_meta_update(node_t *left, pmUnits *large_units, int *l_sem, int *r_sem, int *otype)
{
    int		o_sem;

    /* Update units */
    sdsfree(left->value_set.series_values[0].series_desc.units);
    left->value_set.series_values[0].series_desc.units = sdsnew(pmUnitsStr(large_units));

    /*
     * If the semantics of both operands is not a counter
     * (i.e. PM_SEM_INSTANT or PM_SEM_DISCRETE) then the
     * result will have semantics PM_SEM_INSTANT unless both
     * operands are PM_SEM_DISCRETE in which case the result
     * is also PM_SEM_DISCRETE.
     */
    if (*l_sem == PM_SEM_DISCRETE && *r_sem == PM_SEM_DISCRETE) {
	o_sem = PM_SEM_DISCRETE;
    } else if (*l_sem != PM_SEM_COUNTER || *r_sem != PM_SEM_COUNTER) {
	o_sem = PM_SEM_INSTANT;
    } else {
	o_sem = PM_SEM_COUNTER;
    }

    /* override type of result value (if it's been set) */
    if (*otype != PM_TYPE_UNKNOWN) {
	sdsfree(left->value_set.series_values[0].series_desc.type);
	left->value_set.series_values[0].series_desc.type = sdsnew(pmTypeStr(*otype));
    }

    /* Update semantics */
    sdsfree(left->value_set.series_values[0].series_desc.semantics);
    left->value_set.series_values[0].series_desc.semantics = sdsnew(pmSemStr(o_sem));
}

static void
series_calculate_plus(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    node_t		*left = np->left, *right = np->right;
    int			l_type, r_type, otype=PM_TYPE_UNKNOWN;
    int			l_sem, r_sem, j, k;
    unsigned int	num_samples, num_instances;
    pmAtomValue		l_val, r_val;
    pmUnits		l_units = {0}, r_units = {0}, large_units = {0};
    sds			msg;

    if (left->value_set.num_series == 0 || right->value_set.num_series == 0)
	return;

    if (series_calculate_binary_check(N_PLUS, baton, left, right,
		&l_type, &r_type, &l_sem, &r_sem,
		&l_units, &r_units, &large_units,
		left->value_set.series_values[0].series_desc.indom,
		right->value_set.series_values[0].series_desc.indom) != 0)
	return;

    num_samples = left->value_set.series_values[0].num_samples;

    for (j = 0; j < num_samples; j++) {
	num_instances = left->value_set.series_values[0].series_sample[j].num_instances;
	if (num_instances != right->value_set.series_values[0].series_sample[j].num_instances) {
	    infofmt(msg, "Number of instances of two metrics are inconsistent.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return;
	}
	for (k = 0; k < num_instances; k++) {
	    series_calculate_order_binary(N_PLUS, l_type, r_type, &otype, 
		&l_val, &r_val, 
		left->value_set.series_values[0].series_sample[j].series_instance + k,
		right->value_set.series_values[0].series_sample[j].series_instance + k,
		&l_units, &r_units, &large_units, calculate_plus);
	}
    }
    /*
     * For addition and subtraction all dimensions for
     * each of the operands and result are identical.
     */
    large_units.dimCount = l_units.dimCount;
    large_units.dimSpace = l_units.dimSpace;
    large_units.dimTime = l_units.dimTime;

    series_binary_meta_update(left, &large_units, &l_sem, &r_sem, &otype);
    np->value_set = left->value_set;
}

static void
series_calculate_minus(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    node_t		*left = np->left, *right = np->right;
    unsigned int	num_samples, num_instances, j, k;
    pmAtomValue		l_val, r_val;
    pmUnits		l_units = {0}, r_units = {0}, large_units = {0};
    int			l_type, r_type, otype=PM_TYPE_UNKNOWN;
    int			l_sem, r_sem;
    sds			msg;

    if (left->value_set.num_series == 0 || right->value_set.num_series == 0)
	return;

    if (series_calculate_binary_check(N_MINUS, baton, left, right,
		&l_type, &r_type, &l_sem, &r_sem,
		&l_units, &r_units, &large_units,
		left->value_set.series_values[0].series_desc.indom,
		right->value_set.series_values[0].series_desc.indom) != 0)
	return;

    num_samples = left->value_set.series_values[0].num_samples;

    for (j = 0; j < num_samples; j++) {
	num_instances = left->value_set.series_values[0].series_sample[j].num_instances;
	if (num_instances != right->value_set.series_values[0].series_sample[j].num_instances) {
	    infofmt(msg, "Number of instances of two metrics are inconsistent.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return;
	}
	for (k = 0; k < num_instances; k++) {
	    series_calculate_order_binary(N_MINUS, l_type, r_type, &otype, 
		&l_val, &r_val, 
		left->value_set.series_values[0].series_sample[j].series_instance + k,
		right->value_set.series_values[0].series_sample[j].series_instance + k,
		&l_units, &r_units, &large_units, calculate_minus);
	}
    }
    /*
     * For addition and subtraction all dimensions for each of
     * the operands and result are identical.
     */
    large_units.dimCount = l_units.dimCount;
    large_units.dimSpace = l_units.dimSpace;
    large_units.dimTime = l_units.dimTime;

    series_binary_meta_update(left, &large_units, &l_sem, &r_sem, &otype);
    np->value_set = left->value_set;
}

static void
series_calculate_star(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    node_t		*left = np->left, *right = np->right;
    unsigned int	num_samples, num_instances, j, k;
    pmAtomValue		l_val, r_val;
    pmUnits		l_units = {0}, r_units = {0}, large_units = {0};
    int			l_type, r_type, otype=PM_TYPE_UNKNOWN;
    int			l_sem, r_sem;
    sds			msg;

    if (left->value_set.num_series == 0 || right->value_set.num_series == 0)
	return;

    if (series_calculate_binary_check(N_STAR, baton, left, right,
		&l_type, &r_type, &l_sem, &r_sem,
		&l_units, &r_units, &large_units,
		left->value_set.series_values[0].series_desc.indom,
		right->value_set.series_values[0].series_desc.indom) != 0)
	return;

    num_samples = left->value_set.series_values[0].num_samples;

    for (j = 0; j < num_samples; j++) {
	num_instances = left->value_set.series_values[0].series_sample[j].num_instances;
	if (num_instances != right->value_set.series_values[0].series_sample[j].num_instances) {
	    infofmt(msg, "Number of instances of two metrics are inconsistent.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return;
	}
	for (k = 0; k < num_instances; k++) {
	    series_calculate_order_binary(N_STAR, l_type, r_type, &otype, 
		&l_val, &r_val, 
		left->value_set.series_values[0].series_sample[j].series_instance + k,
		right->value_set.series_values[0].series_sample[j].series_instance + k,
		&l_units, &r_units, &large_units, calculate_star);
	}
    }
    /*
     * For multiplication, the dimensions of the result are the
     * sum of the dimensions of the operands.
     */
    large_units.dimCount = l_units.dimCount + r_units.dimCount;
    large_units.dimSpace = l_units.dimSpace + r_units.dimSpace;
    large_units.dimTime = l_units.dimTime + r_units.dimTime;

    series_binary_meta_update(left, &large_units, &l_sem, &r_sem, &otype);
    np->value_set = left->value_set;
}

static void
series_calculate_slash(node_t *np)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    node_t		*left = np->left, *right = np->right;
    unsigned int	num_samples, num_instances, j, k;
    pmAtomValue		l_val, r_val;
    pmUnits		l_units = {0}, r_units = {0}, large_units = {0};
    int			l_type, r_type, otype=PM_TYPE_UNKNOWN;
    int			l_sem, r_sem;
    sds			msg;

    if (left->value_set.num_series == 0 || right->value_set.num_series == 0) return;
    if (series_calculate_binary_check(N_SLASH, baton, left, right, &l_type, &r_type, &l_sem, &r_sem,
		 &l_units, &r_units, &large_units, left->value_set.series_values[0].series_desc.indom,
		 right->value_set.series_values[0].series_desc.indom) != 0) {
	return;
    }
    num_samples = left->value_set.series_values[0].num_samples;

    for (j = 0; j < num_samples; j++) {
	num_instances = left->value_set.series_values[0].series_sample[j].num_instances;
	if (num_instances != right->value_set.series_values[0].series_sample[j].num_instances) {
	    infofmt(msg, "Number of instances of two metrics are inconsistent.\n");
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    return;
	}
	for (k = 0; k < num_instances; k++) {
	    series_calculate_order_binary(N_SLASH, l_type, r_type, &otype, 
		&l_val, &r_val, 
		left->value_set.series_values[0].series_sample[j].series_instance + k,
		right->value_set.series_values[0].series_sample[j].series_instance + k,
		&l_units, &r_units, &large_units, calculate_slash);
	}
    }
    /*
     * For division, the dimensions of the result are the
     * difference of the dimensions of the operands.
     */
    large_units.dimCount = l_units.dimCount - r_units.dimCount;
    large_units.dimSpace = l_units.dimSpace - r_units.dimSpace;
    large_units.dimTime = l_units.dimTime - r_units.dimTime;

    series_binary_meta_update(left, &large_units, &l_sem, &r_sem, &otype);
    np->value_set = left->value_set;

}

/* 
 * In this phase all time series values have been stored into nodes.
 * Therefore we can directly calculate values of a node according to
 * the semantics of this node.  Do dfs here.
 * In the process of unstacking from bottom of the parser tree, each
 * time we encounter a function-type node, calculate the results and
 * store them into this node.
 */
static int
series_calculate(seriesQueryBaton *baton, node_t *np, int level)
{
    int		sts;

    if (np == NULL)
	return 0;
    if ((sts = series_calculate(baton, np->left, level+1)) < 0)
	return sts;
    if ((sts = series_calculate(baton, np->right, level+1)) < 0)
	return sts;

    np->baton = baton;
    switch ((sts = np->type)) {
	case N_RATE:
	    series_calculate_rate(np);
	    break;
	case N_MAX:
	case N_MAX_INST:
	    series_calculate_max(np);
	    break;
	case N_MAX_SAMPLE:
	    series_calculate_time_domain_max(np);
	    break;
	case N_MIN:
	case N_MIN_INST:
	    series_calculate_min(np);
	    break;
	case N_MIN_SAMPLE:
	    series_calculate_time_domain_min(np);
	    break;
	case N_RESCALE:
	    series_calculate_rescale(np);
	    break;
	case N_ABS:
	    series_calculate_abs(np);
	    break;
	case N_FLOOR:
	    series_calculate_floor(np);
	    break;
	case N_LOG:
	    series_calculate_log(np);
	    break;
	case N_SQRT:
	    series_calculate_sqrt(np);
	    break;
	case N_ROUND:
	    series_calculate_round(np);
	    break;
	case N_PLUS:
	    series_calculate_plus(np);
	    break;
	case N_MINUS:
	    series_calculate_minus(np);
	    break;
	case N_STAR:
	    series_calculate_star(np);
	    break;
	case N_SLASH:
	    series_calculate_slash(np);
	    break;
	case N_AVG:
	    series_calculate_statistical(np, N_AVG);
	    break;
	case N_AVG_INST:
	    series_calculate_statistical(np, N_AVG_INST);
		sts = N_AVG_INST;
	    break;
	case N_AVG_SAMPLE:
	    series_calculate_time_domain_statistical(np, N_AVG_SAMPLE);
	    sts = N_AVG_SAMPLE;
	    break;
	case N_SUM:
	    series_calculate_statistical(np, N_SUM);
	    break;
	case N_SUM_INST:
	    series_calculate_statistical(np, N_SUM_INST);
	    break;
	case N_SUM_SAMPLE:
	    series_calculate_time_domain_statistical(np, N_SUM_SAMPLE);
	    break;
	case N_STDEV_INST:
	    series_calculate_standard_deviation(np);
	    break;
	case N_STDEV_SAMPLE:
	    series_calculate_time_domain_standard_deviation(np);
	    break;
	case N_TOPK_INST:
	    series_calculate_topk(np);
	    break;
	case N_TOPK_SAMPLE:
	    series_calculate_time_domain_topk(np);
            break;
	case N_NTH_PERCENTILE_INST:
	    series_calculate_nth_percentile(np);
	    break;
	case N_NTH_PERCENTILE_SAMPLE:
	    series_calculate_time_domain_nth_percentile(np);
	    break;
	default:
	    sts = 0;	/* no function */
	    break;
    }
    return sts;
}

static int
check_compatibility(pmUnits *units_a, pmUnits *units_b)
{
    if (compare_pmUnits_dim(units_a, units_b) == 0)
	return 0;
    return -1;
}

static void
series_compatibility_convert(
	series_sample_set_t *set0, series_sample_set_t *set1, pmUnits *units0, pmUnits *units1, pmUnits *large_units)
{
    unsigned int	j, k;
    int			type0, type1, str_len;
    char		str_val[256];
    pmAtomValue		val0, val1;

    large_units->scaleCount = units0->scaleCount > units1->scaleCount ? units0->scaleCount : units1->scaleCount;
    large_units->scaleSpace = units0->scaleSpace > units1->scaleSpace ? units0->scaleSpace : units1->scaleSpace;
    large_units->scaleTime = units0->scaleTime > units1->scaleTime ? units0->scaleTime : units1->scaleTime;

    type0 = PM_TYPE_NOSUPPORT;
    type1 = PM_TYPE_NOSUPPORT;
    if (large_units->scaleCount != units0->scaleCount ||
	large_units->scaleSpace != units0->scaleSpace ||
	large_units->scaleTime != units0->scaleTime) {
	type0 = PM_TYPE_DOUBLE;
	for (j = 0; j < set0->num_samples; j++) {
	    for (k = 0; k < set0->series_sample[j].num_instances; k++) {
		series_extract_value(type0, set0->series_sample[j].series_instance[k].data, &val0);
		if (pmConvScale(type0, &val0, units0, &val0, large_units) < 0)
		    memset(large_units, 0, sizeof(*large_units));
		sdsfree(set0->series_sample[j].series_instance[k].data);
		str_len = series_pmAtomValue_conv_str(type0, str_val, &val0, sizeof(str_val));
		set0->series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	sdsfree(set0->series_desc.type);
	sdsfree(set0->series_desc.units);
	set0->series_desc.type = sdsnew(pmTypeStr(type0));
	set0->series_desc.units = sdsnew(pmUnitsStr(large_units));
    }
    if (large_units->scaleCount != units1->scaleCount ||
	large_units->scaleSpace != units1->scaleSpace ||
	large_units->scaleTime != units1->scaleTime) {
	type1 = PM_TYPE_DOUBLE;
	for (j = 0; j < set1->num_samples; j++) {
	    for (k = 0; k < set1->series_sample[j].num_instances; k++) {
		series_extract_value(type1, set1->series_sample[j].series_instance[k].data, &val1);
		if (pmConvScale(type1, &val1, units1, &val1, large_units) < 0)
		    memset(large_units, 0, sizeof(*large_units));
		sdsfree(set1->series_sample[j].series_instance[k].data);
		str_len = series_pmAtomValue_conv_str(type1, str_val, &val1, sizeof(str_val));
		set1->series_sample[j].series_instance[k].data = sdsnewlen(str_val, str_len);
	    }
	}
	sdsfree(set1->series_desc.type);
	sdsfree(set1->series_desc.units);
	set1->series_desc.type = sdsnew(pmTypeStr(type1));
	set1->series_desc.units = sdsnew(pmUnitsStr(large_units));
    }
}

static void
series_redis_hash_expression(seriesQueryBaton *baton, char *hashbuf, int len_hashbuf)
{
    unsigned char	hash[20];
    sds			key, msg;
    char		*errmsg;
    node_t		*np = &baton->u.query.root;
    int			i, j, num_series = np->value_set.num_series;
    pmUnits		units0, units1, large_units;
    double		mult;
    pmSeriesExpr	expr;

    for (i = 0; i < num_series; i++)
	np->value_set.series_values[i].compatibility = 1;

    if (num_series > 0) {
	expr.query = series_function_hash(hash, np, 0);
	pmwebapi_hash_str(hash, hashbuf, len_hashbuf);
    }

    for (i = 0; i < num_series; i++) {
	if (!np->value_set.series_values[i].compatibility) {
	    infofmt(msg, "Descriptors for metric '%s' do not satisfy compatibility between different hosts/sources.\n",
		np->value_set.series_values[i].metric_name);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    continue;
	}

	if (strncmp(np->value_set.series_values[i].series_desc.units, "none", 4) == 0)
	    memset(&units0, 0, sizeof(units0));
	else if (pmParseUnitsStr(np->value_set.series_values[i].series_desc.units,
	    &units0, &mult, &errmsg) != 0) {
	    np->value_set.series_values[i].compatibility = 0;
	    infofmt(msg, "Invalid units string: %s\n",
		    np->value_set.series_values[i].series_desc.units);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EPROTO;
	    free(errmsg);
	    break;
	}

	for (j = 0; j < num_series; j++) {
	    if (!np->value_set.series_values[j].compatibility || i == j)
	    	continue;

	    if (strncmp(np->value_set.series_values[j].series_desc.units, "none", 4) == 0)
		memset(&units1, 0, sizeof(units1));
	    else if (pmParseUnitsStr(np->value_set.series_values[j].series_desc.units,
	    	&units1, &mult, &errmsg) != 0) {
		np->value_set.series_values[j].compatibility = 0;
		infofmt(msg, "Invalid units string: %s\n",
			np->value_set.series_values[j].series_desc.units);
		batoninfo(baton, PMLOG_ERROR, msg);
		baton->error = -EPROTO;
		free(errmsg);
		break;
	    }

	    if (check_compatibility(&units0, &units1) != 0) {
		np->value_set.series_values[j].compatibility = 0;
		infofmt(msg, "Incompatible units between operand metrics or expressions\n");
		batoninfo(baton, PMLOG_ERROR, msg);
		baton->error = -EPROTO;
		break;
	    } else {
		/* 
		 * For series with the same metric names, if they have
		 * same dimensions but different scales, use the larger
		 * scale and convert the values with the smaller scale.
		 * The result is promoted to type PM_TYPE_DOUBLE.
		 */
		series_compatibility_convert(&np->value_set.series_values[i],
			    &np->value_set.series_values[j],
			    &units0, &units1, &large_units);
	    }
	}

	if (baton->error != 0)
	    break;

	sdsfree(np->value_set.series_values[i].sid->name);
	np->value_set.series_values[i].sid->name = sdsnew(hashbuf);
    }

    if (baton->error == 0 && num_series > 0 && np->value_set.series_values[0].compatibility) {
	/* descriptor, after the O(N^2) checking the descriptor of 1st series has been 
	 * converted to the largest one.
	 */
	key = sdscatfmt(sdsempty(), "pcp:desc:series:%s", hashbuf);
	sdsfree(np->value_set.series_values[0].series_desc.source);
	np->value_set.series_values[0].series_desc.source = sdsnewlen(NULL, SHA1SZ);
	series_hmset_function_desc(baton, key, &np->value_set.series_values[0].series_desc);

	/* expression */
	key = sdscatfmt(sdsempty(), "pcp:expr:series:%s", hashbuf);
	series_hmset_function_expr(baton, key, expr.query);
    }
}

static void
series_query_report_values(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_report_values");
    seriesBatonCheckCount(baton, "series_query_report_values");

    seriesBatonReference(baton, "series_query_report_values");
    series_prepare_time(baton, &baton->u.query.root.result);
    series_query_end_phase(baton);
}

static void
series_query_funcs_report_values(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    int			has_function = 0;
    char		hashbuf[42];

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_funcs_report_values");
    seriesBatonCheckCount(baton, "series_query_funcs_report_values");

    seriesBatonReference(baton, "series_query_funcs_report_values");

    /* For function-type nodes, calculate actual values */
    has_function = series_calculate(baton, &baton->u.query.root, 0);

    /*
     * Store the canonical query to Redis if this query statement has
     * function operation.
     */
    if (has_function)
	series_redis_hash_expression(baton, hashbuf, sizeof(hashbuf));

    /* time series values saved in root node so report them directly. */
    series_node_values_report(baton, &baton->u.query.root);
    
    series_query_end_phase(baton);
}

static void
series_query_funcs(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_funcs");
    seriesBatonCheckCount(baton, "series_query_funcs");

    seriesBatonReference(baton, "series_query_funcs");
    /* Process function-type node */
    series_process_func(baton, &baton->u.query.root, 0);
    series_query_end_phase(baton);
}

static void
series_query_desc(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_desc");
    seriesBatonCheckCount(baton, "series_query_desc");

    seriesBatonReference(baton, "series_query_desc");
    series_expr_node_desc(baton, &baton->u.query.root);
    series_query_end_phase(baton);
}

static int
series_time_window(timing_t *tp)
{
    if (tp->count || tp->window.range ||
	tp->window.start || tp->window.end ||
	tp->window.count || tp->window.delta)
	return 1;
    return 0;
}

static void
series_query_services(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    pmSeriesModule	*module = baton->module;
    seriesModuleData	*data = getSeriesModuleData(module);
    sds			option;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_services");
    seriesBatonCheckCount(baton, "series_query_services");

    seriesBatonReference(baton, "series_query_services");

    /* attempt to re-use existing slots connections */
    if (data == NULL) {
	baton->error = -ENOMEM;
    } else if (data->slots) {
	baton->slots = data->slots;
	series_query_end_phase(baton);
    } else {
	option = pmIniFileLookup(data->config, "redis", "enabled");
	if (option && strcmp(option, "false") == 0)
	    baton->error = -ENOTSUP;
	else
	    baton->slots = data->slots =
		redisSlotsConnect(
		    data->config, 1, baton->info,
		    series_query_end_phase, baton->userdata,
		    data->events, (void *)baton);
    }
}

int
series_solve(pmSeriesSettings *settings,
	node_t *root, timing_t *timing, pmSeriesFlags flags, void *arg)
{
    seriesQueryBaton	*baton;
    unsigned int	i = 0;

    if (root == NULL) {
	/* Coverity CID366052 */
    	return -ENOMEM;
    }

    if ((baton = calloc(1, sizeof(seriesQueryBaton))) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetQuery(baton, root, timing);

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_query_services;

    /* Resolve label key names (via their map keys) */
    baton->phases[i++].func = series_query_maps;

    /* Resolve sets of series identifiers for leaf nodes */
    baton->phases[i++].func = series_query_eval;

    /* Perform final matching (set of) series solving */
    baton->phases[i++].func = series_query_expr;

    baton->phases[i++].func = series_query_mapping;
    if ((flags & PM_SERIES_FLAG_METADATA) || !series_time_window(timing)) {
	/* Store series descriptors into nodes */
	baton->phases[i++].func = series_query_desc;
	/* Report matching series IDs, unless time windowing */
	baton->phases[i++].func = series_query_report_matches;
    } else {
	/* Store time series values into nodes */
	baton->phases[i++].func = series_query_funcs;
	/* Report actual values */
	baton->phases[i++].func = series_query_funcs_report_values;
    }

    /* final callback once everything is finished, free baton */
    baton->phases[i++].func = series_query_finished;

    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

/* build a reverse hash mapping */
static void
reverse_map(seriesQueryBaton *baton, redisMap *map, int nkeys, redisReply **elements)
{
    redisReply		*name, *hash;
    sds			msg, key, val;
    unsigned int	i;

    for (i = 0; i < nkeys; i += 2) {
	hash = elements[i];
	name = elements[i+1];
	if (name->type == REDIS_REPLY_STRING) {
	    if (hash->type == REDIS_REPLY_STRING) {
		key = sdsnewlen(hash->str, hash->len);
		val = sdsnewlen(name->str, name->len);
		redisMapInsert(map, key, val);
		sdsfree(key); // map has keyDup set
	    } else {
		infofmt(msg, "expected string key for hashmap (type=%s)",
			redis_reply_type(hash));
		batoninfo(baton, PMLOG_RESPONSE, msg);
		baton->error = -EINVAL;
	    }
	} else {
	    infofmt(msg, "expected string name for hashmap (type=%s)",
		    redis_reply_type(name));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    baton->error = -EINVAL;
	}
    }
}

static void
series_map_lookup_expr_reply(redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    sds			query;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_map_lookup_expr_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_map_lookup_expr_reply");
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY || reply->elements == 0)) {
	infofmt(msg, "expected array of one string element (got %zu) from series %s %s (type=%s)",
		reply->elements, sid->name, HMGET, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
    } else if (reply->element[0]->type == REDIS_REPLY_STRING) {
	query = sdsempty();
    	if ((sts = extract_string(baton, sid->name, reply->element[0], &query, "query")) < 0) {
	    baton->error = sts;
	} else {
	    /* call the on_metric callback, whatever it's set to by the caller */
	    baton->u.lookup.func(sid->name, query, baton->userdata);
	}
    }
    series_query_end_phase(baton);
}

static void
series_map_lookup_expr(seriesQueryBaton *baton, sds series, void *arg)
{
    seriesGetSID	*sidexpr;
    sds			key;
    sds			exprcmd;

    sidexpr = calloc(1, sizeof(seriesGetSID));
    initSeriesGetSID(sidexpr, series, 1, baton);

    seriesBatonReference(baton, "series_map_lookup_expr");

    key = sdscatfmt(sdsempty(), "pcp:expr:series:%S", sidexpr->name);
    exprcmd = redis_command(3);
    exprcmd = redis_param_str(exprcmd, HMGET, HMGET_LEN);
    exprcmd = redis_param_sds(exprcmd, key);
    exprcmd = redis_param_str(exprcmd, "query", sizeof("query")-1);
    sdsfree(key);
    redisSlotsRequest(baton->slots, exprcmd, series_map_lookup_expr_reply, sidexpr);
    sdsfree(exprcmd);
}

/*
 * Produce the list of mapped names (requires reverse mapping from IDs)
 */
static int
series_map_reply(seriesQueryBaton *baton, sds series,
		int nelements, redisReply **elements)
{
    redisMapEntry	*entry;
    redisReply		*reply;
    sds			msg, key;
    unsigned int	i;
    int			sts = 0;

    key = sdsnewlen(NULL, 20);
    if (nelements == 0) {
	/* expression - not mapped */
	if (pmDebugOptions.series || pmDebugOptions.query)
	    fprintf(stderr, "series_map_reply: fabricated SID %s\n", series);
	series_map_lookup_expr(baton, series, baton->userdata);
    } else {
	/* name - get the mapped value */
	for (i = 0; i < nelements; i++) {
	    reply = elements[i];
	    if (reply->type == REDIS_REPLY_STRING) {
		sdsclear(key);
		key = sdscatlen(key, reply->str, reply->len);
		if ((entry = redisMapLookup(baton->u.lookup.map, key)) != NULL)
		    baton->u.lookup.func(series, redisMapValue(entry), baton->userdata);
		else {
		    infofmt(msg, "%s - timeseries string map", series);
		    batoninfo(baton, PMLOG_CORRUPT, msg);
		    sts = -EINVAL;
		}
	    } else {
		infofmt(msg, "expected string in %s set (type=%s)",
			    series, redis_reply_type(reply));
		batoninfo(baton, PMLOG_RESPONSE, msg);
		sts = -EPROTO;
	    }
	}
    }
    sdsfree(key);

    return sts;
}

static void
series_map_keys_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    redisReply		*reply = r;
    redisReply		*child;
    sds			val, msg;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_map_keys_callback");
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	val = sdsempty();
	for (i = 0; i < reply->elements; i++) {
	    child = reply->element[i];
	    if (child->type == REDIS_REPLY_STRING) {
		if (baton->u.lookup.pattern != NULL &&
		    fnmatch(baton->u.lookup.pattern, child->str, 0) != 0)
		    continue;
		val = sdscpylen(val, child->str, child->len);
		baton->u.lookup.func(NULL, val, baton->userdata);
	    } else {
		infofmt(msg, "bad response for string map %s (%s)",
			HVALS, redis_reply_type(child));
		batoninfo(baton, PMLOG_RESPONSE, msg);
		sdsfree(val);
		baton->error = -EINVAL;
	    }
	}
	sdsfree(val);
    } else {
	infofmt(msg, "expected array from string map %s (reply=%s)",
	    HVALS, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }

    baton->callbacks->on_done(baton->error, baton->userdata);
    freeSeriesGetLookup(baton);
}

static int
series_map_keys(seriesQueryBaton *baton, const char *name)
{
    sds			cmd, key;

    key = sdscatfmt(sdsempty(), "pcp:map:%s", name);
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HVALS, HVALS_LEN);
    cmd = redis_param_sds(cmd, key);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
		   	 series_map_keys_callback, baton);
    sdsfree(cmd);
    return 0;
}

static void
series_label_value_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetLabelMap	*value = (seriesGetLabelMap *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)value->baton;
    redisReply		*reply = r;
    redisMapEntry	*entry;
    pmSeriesLabel	label;
    sds			msg;

    seriesBatonCheckMagic(value, MAGIC_LABELMAP, "series_label_value_reply");

    /* unpack - produce reverse map of ids-to-values for each entry */
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	reverse_map(baton, value->map, reply->elements, reply->element);
    } else {
	infofmt(msg, "expected array from %s %s.%s.value (type=%s)", HGETALL,
		 "pcp:map:label", value->mapID, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }
    if (baton->error == 0) {
	label.name = value->name;
	if ((entry = redisMapLookup(value->map, value->mapID)) == NULL)
	    label.value = sdsnew("null");
	else
	    label.value = redisMapValue(entry);

	baton->callbacks->on_labelmap(value->series, &label, baton->userdata);

	if (entry == NULL)
	    sdsfree(label.value);
    } else {
	infofmt(msg, "%s - timeseries name map", value->series);
	batoninfo(baton, PMLOG_CORRUPT, msg);
    }

    freeSeriesGetLabelMap(value);

    series_query_end_phase(baton);
}

static int
series_label_reply(seriesQueryBaton *baton, sds series,
		int nelements, redisReply **elements)
{
    seriesGetLabelMap	*labelmap;
    redisMapEntry 	*entry;
    redisReply		*reply;
    redisMap		*vmap;
    char		hashbuf[42];
    sds			msg, key, cmd, name, vkey, nmapID, vmapID;
    unsigned int	i, index;
    int			sts = 0;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_label_reply");

    /* result verification first */
    if (nelements % 2) {
	infofmt(msg, "expected even number of results from %s (not %d)",
		    HGETALL, nelements);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }
    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if (reply->type != REDIS_REPLY_STRING) {
	    infofmt(msg, "expected only string results from %s (type=%s)",
			HGETALL, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    return -EPROTO;
	}
    }

    nmapID = sdsnewlen(NULL, 20);
    vmapID = sdsnewlen(NULL, 20);

    /* perform the label value reverse lookup */
    nelements /= 2;
    for (i = 0; i < nelements; i++) {
	index = i * 2;

	sdsclear(nmapID);
	nmapID = sdscatlen(nmapID, elements[index]->str, elements[index]->len);
	sdsclear(vmapID);
	vmapID = sdscatlen(vmapID, elements[index+1]->str, elements[index+1]->len);

	if ((entry = redisMapLookup(baton->u.lookup.map, nmapID)) != NULL) {
	    pmwebapi_hash_str((unsigned char *)nmapID, hashbuf, sizeof(hashbuf));
	    vkey = sdscatfmt(sdsempty(), "label.%s.value", hashbuf);
	    vmap = redisMapCreate(vkey);
	    name = redisMapValue(entry);

	    baton->callbacks->on_label(series, name, baton->userdata);

	    if ((labelmap = calloc(1, sizeof(seriesGetLabelMap))) == NULL) {
		infofmt(msg, "%s - label value lookup OOM", series);
		batoninfo(baton, PMLOG_ERROR, msg);
		sts = -ENOMEM;
		if (vmap) /* Coverity CID308763 */
		    redisMapRelease(vmap);
		continue;
	    }
	    initSeriesGetLabelMap(labelmap, series, name, vmap, vmapID, vkey, baton);

	    seriesBatonReference(baton, "series_label_reply");

	    pmwebapi_hash_str((unsigned char *)nmapID, hashbuf, sizeof(hashbuf));
	    key = sdscatfmt(sdsempty(), "pcp:map:label.%s.value", hashbuf);
	    cmd = redis_command(2);
	    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
	    cmd = redis_param_sds(cmd, key);
	    sdsfree(key);
	    redisSlotsRequest(baton->slots, cmd,
				series_label_value_reply, labelmap);
	    sdsfree(cmd);
	} else {
	    infofmt(msg, "%s - timeseries label map", series);
	    batoninfo(baton, PMLOG_CORRUPT, msg);
	    sts = -EINVAL;
	}
    }

    sdsfree(nmapID);
    sdsfree(vmapID);
    return sts;
}

static void
series_lookup_labels_callback(
	redisClusterAsyncContext *c, void *r,void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton    *baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    sds			msg;
    int                 sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_lookup_labels_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_labels_callback");

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from %s %s:%s (type=%s)",
		HGETALL, "pcp:labelvalue:series", sid->name,
		redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else if ((sts = series_label_reply(baton, sid->name,
				reply->elements, reply->element)) < 0) {
	baton->error = sts;
    }
    freeSeriesGetSID(sid);

    series_query_end_phase(baton);
}

static void
series_lookup_labels(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_labels");
    seriesBatonCheckCount(baton, "series_lookup_labels");

    /* unpack - iterate over series and extract labels names and values */
    for (i = 0; i < baton->u.lookup.nseries; i++) {
	seriesBatonReference(baton, "series_lookup_labels");
	sid = &baton->u.lookup.series[i];
	key = sdscatfmt(sdsempty(), "pcp:labelvalue:series:%S", sid->name);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
	cmd = redis_param_sds(cmd, key);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
			series_lookup_labels_callback, sid);
	sdsfree(cmd);
    }
}

int
pmSeriesLabels(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_LABELS_CALLS);

    if (nseries < 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nseries, series, settings->callbacks.on_label, labelsmap);

    if (nseries == 0)
	return series_map_keys(baton, redisMapName(baton->u.lookup.map));

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_mapping;
    baton->phases[i++].func = series_lookup_labels;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static void
series_lookup_labelvalues_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton    *baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    redisReply		*child;
    pmSeriesLabel	label;
    unsigned int	i;
    sds			msg;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_lookup_labelvalues_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_labelvalues_callback");

    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	label.name = sid->name;
	label.value = sdsempty();

	for (i = 0; i < reply->elements; i++) {
	    child = reply->element[i];
	    if (child->type == REDIS_REPLY_STRING) {
		label.value = sdscpylen(label.value, child->str, child->len);
		baton->callbacks->on_labelmap(NULL, &label, baton->userdata);
	    } else {
		infofmt(msg, "bad response for string map %s (%s)",
			HVALS, redis_reply_type(child));
		batoninfo(baton, PMLOG_RESPONSE, msg);
		baton->error = -EINVAL;
	    }
	}
	sdsfree(label.value);
    } else {
	infofmt(msg, "expected array from string map %s (reply=%s)",
	    HVALS, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }
    freeSeriesGetSID(sid);

    series_query_end_phase(baton);
}

static void
series_lookup_labelvalues(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    unsigned char	hash[20];
    char		buffer[42];
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_labelvalues");
    seriesBatonCheckCount(baton, "series_lookup_labelvalues");

    for (i = 0; i < baton->u.lookup.nseries; i++) {
	sid = &baton->u.lookup.series[i];
	seriesBatonReference(baton, "series_lookup_labelvalues");

	pmwebapi_string_hash(hash, sid->name, sdslen(sid->name));
	key = sdscatfmt(sdsempty(), "pcp:map:label.%s.value",
			    pmwebapi_hash_str(hash, buffer, sizeof(buffer)));
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, HVALS, HVALS_LEN);
	cmd = redis_param_sds(cmd, key);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
			series_lookup_labelvalues_callback, sid);
	sdsfree(cmd);
    }
}

int
pmSeriesLabelValues(pmSeriesSettings *settings, int nlabels, pmSID *labels, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    if (nlabels <= 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nlabels * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nlabels, labels, NULL, NULL);

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_labelvalues;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    series_stats_inc(settings, SERIES_LABELVALUES_CALLS);
    return 0;
}


static void
redis_series_desc_reply(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    pmSeriesDesc	desc;
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    sds			msg;
    int			sts;

    desc.indom = sdsempty();
    desc.pmid = sdsempty();
    desc.semantics = sdsempty();
    desc.source = sdsempty();
    desc.type = sdsempty();
    desc.units = sdsempty();

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array type from series %s %s (type=%s)",
		sid->name, HMGET, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }
    else if ((sts = extract_series_desc(baton, sid->name,
			reply->elements, reply->element, &desc)) < 0)
	baton->error = sts;
    else if ((sts = baton->callbacks->on_desc(sid->name, &desc, baton->userdata)) < 0)
	baton->error = sts;

    sdsfree(desc.indom);
    sdsfree(desc.pmid);
    sdsfree(desc.semantics);
    sdsfree(desc.source);
    sdsfree(desc.type);
    sdsfree(desc.units);

    series_query_end_phase(baton);
}

static void
series_lookup_desc(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_desc");
    seriesBatonCheckCount(baton, "series_lookup_desc");

    for (i = 0; i < baton->u.lookup.nseries; i++) {
	sid = &baton->u.lookup.series[i];
	seriesBatonReference(baton, "series_lookup_desc");

	key = sdscatfmt(sdsempty(), "pcp:desc:series:%S", sid->name);
	cmd = redis_command(8);
	cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, "indom", sizeof("indom")-1);
	cmd = redis_param_str(cmd, "pmid", sizeof("pmid")-1);
	cmd = redis_param_str(cmd, "semantics", sizeof("semantics")-1);
	cmd = redis_param_str(cmd, "source", sizeof("source")-1);
	cmd = redis_param_str(cmd, "type", sizeof("type")-1);
	cmd = redis_param_str(cmd, "units", sizeof("units")-1);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd, redis_series_desc_reply, sid);
	sdsfree(cmd);
    }
}

int
pmSeriesDescs(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_DESCS_CALLS);

    if (nseries <= 0)
	return -EINVAL;

    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nseries, series, NULL, NULL);

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_desc;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static int
extract_series_inst(seriesQueryBaton *baton, seriesGetSID *sid,
		pmSeriesInst *inst, int nelements, redisReply **elements)
{
    sds			msg, series = sid->metric;

    if (nelements < 3) {
	infofmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    if (extract_string(baton, series, elements[0], &inst->instid, "inst") < 0)
	return -EPROTO;
    if (extract_mapping(baton, series, elements[1], &inst->name, "name") < 0)
	return -EPROTO;
    if (extract_sha1(baton, series, elements[2], &inst->source, "source") < 0)
	return -EPROTO;

    /* return instance series identifiers, not the metric series */
    inst->series = sdscpylen(inst->series, sid->name, sdslen(sid->name));
    return 0;
}

static void
series_instances_reply_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    pmSeriesInst	inst;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_instances_reply_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_instances_reply_callback");

    inst.instid = sdsempty();
    inst.name = sdsempty();
    inst.source = sdsempty();
    inst.series = sdsempty();

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from series %s %s (type=%s)",
		HMGET, sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }
    else if ((sts = extract_series_inst(baton, sid, &inst,
				reply->elements, reply->element)) < 0)
	baton->error = sts;
    else if ((sts = baton->callbacks->on_inst(sid->metric, &inst, baton->userdata)) < 0)
	baton->error = sts;
    freeSeriesGetSID(sid);

    sdsfree(inst.instid);
    sdsfree(inst.name);
    sdsfree(inst.source);
    sdsfree(inst.series);

    series_query_end_phase(baton);
}

static void
series_instances_reply(seriesQueryBaton *baton,
		pmSID series, int nelements, redisReply **elements)
{
    seriesGetSID	*sid;
    pmSID		name = sdsempty();
    sds			key, cmd;
    int			i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_instances_reply");

    /*
     * Iterate over the instance series identifiers, looking up
     * the instance hash contents for each.
     */
    for (i = 0; i < nelements; i++) {
	if ((sid = calloc(1, sizeof(seriesGetSID))) == NULL) {
	    /* TODO: report error */
	    continue;
	}
	initSeriesGetSID(sid, series, 1, baton);
	sid->metric = sdsdup(series);

	if (extract_sha1(baton, series, elements[i], &sid->name, "series") < 0) {
	    /* TODO: report error */
	    freeSeriesGetSID(sid); /* Coverity CID308764 */
	    continue;
	}
	seriesBatonReference(sid, "series_instances_reply");
	seriesBatonReference(baton, "series_instances_reply");

	key = sdscatfmt(sdsempty(), "pcp:inst:series:%S", sid->name);
	cmd = redis_command(5);
	cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
	cmd = redis_param_str(cmd, "name", sizeof("name")-1);
	cmd = redis_param_str(cmd, "source", sizeof("source")-1);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
				series_instances_reply_callback, sid);
	sdsfree(cmd);
    }
    sdsfree(name);
}

static void
series_inst_expr_reply(redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    pmSeriesExpr	expr = {0};
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_inst_expr_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_inst_expr_reply");

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array of one string element (got %zu) from series %s %s (type=%s)",
	    reply->elements, sid->name, HMGET, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
    } else if (reply->element[0]->type == REDIS_REPLY_STRING) {
	expr.query = sdsempty();
    	if ((sts = extract_string(baton, sid->name, reply->element[0], &expr.query, "query")) < 0) {
	    baton->error = sts;
	} else {
	    /* Parse the expr (with timing) and series solve the resulting expr tree */
	    baton->u.query.timing.count = 1;
	    baton->error = series_solve_sid_expr(&series_solve_inst_settings, &expr, arg);
	}
    }
    series_query_end_phase(baton);
}

void
series_lookup_instances_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    seriesGetSID	*expr;
    sds			key, exprcmd;
    sds			msg;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_lookup_instances_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_instances_callback");

    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	if (reply->elements > 0) {
	    /* regular series */
	    series_instances_reply(baton, sid->name, reply->elements, reply->element);
	} else {
	    /* Handle fabricated/expression SID in /series/instances :
	     * - get the expr for sid->name from redis. In the callback for that,
	     *   parse the expr and then solve the expression tree with timing from
	     *   this series query baton, then merge the instances into the reply.
	     */
	    if (pmDebugOptions.query)
		fprintf(stderr, "series_lookup_instances_callback: sid %s is fabricated\n", sid->name);
	    expr = calloc(1, sizeof(seriesGetSID));
	    initSeriesGetSID(expr, sid->name, 1, baton);
	    seriesBatonReference(baton, "series_lookup_instances_callback");

	    key = sdscatfmt(sdsempty(), "pcp:expr:series:%S", expr->name);
	    exprcmd = redis_command(3);
	    exprcmd = redis_param_str(exprcmd, HMGET, HMGET_LEN);
	    exprcmd = redis_param_sds(exprcmd, key);
	    exprcmd = redis_param_str(exprcmd, "query", sizeof("query")-1);
	    sdsfree(key);
	    redisSlotsRequest(baton->slots, exprcmd, series_inst_expr_reply, expr);
	    sdsfree(exprcmd);
	}
    } else {
	infofmt(msg, "expected array from series %s %s (type=%s)",
		SMEMBERS, sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }

    series_query_end_phase(baton);
}

static void
series_lookup_instances(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    int			i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_instances_callback");
    seriesBatonCheckCount(baton, "series_lookup_instances_callback");

    for (i = 0; i < baton->u.lookup.nseries; i++) {
	seriesBatonReference(baton, "series_lookup_instances_callback");
	sid = &baton->u.lookup.series[i];
	key = sdscatfmt(sdsempty(), "pcp:instances:series:%S", sid->name);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
			series_lookup_instances_callback, sid);
	sdsfree(cmd);
    }
}

int
pmSeriesInstances(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_INSTANCES_CALLS);

    if (nseries < 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nseries, series, settings->callbacks.on_instance, instmap);

    if (nseries == 0)
	return series_map_keys(baton, redisMapName(baton->u.lookup.map));

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_mapping;
    baton->phases[i++].func = series_lookup_instances;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static void
redis_lookup_mapping_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    redisReply		*reply = r;
    sds			msg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "redis_lookup_mapping_callback");

    /* unpack - produce reverse map of ids-to-names for each context */
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	reverse_map(baton, baton->u.lookup.map, reply->elements, reply->element);
    } else {
	infofmt(msg, "expected array from %s %s (type=%s)",
	    HGETALL, "pcp:map:context.name", redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }

    series_query_end_phase(baton);
}

static void
series_lookup_mapping(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    sds			cmd, key;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_mapping");
    seriesBatonCheckCount(baton, "series_lookup_mapping");
    seriesBatonReference(baton, "series_lookup_mapping");

    key = sdscatfmt(sdsempty(), "pcp:map:%s", redisMapName(baton->u.lookup.map));
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
			redis_lookup_mapping_callback, baton);
    sdsfree(cmd);
}

static void
series_query_mapping_callback(
	redisClusterAsyncContext *c, void *r, void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    redisReply		*reply = r;
    sds			msg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_mapping_callback");

    /* unpack - produce reverse map of ids-to-names for each context */
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	reverse_map(baton, namesmap, reply->elements, reply->element);
    } else {
	infofmt(msg, "expected array from %s %s (type=%s)",
	    HGETALL, "pcp:map:context.name", redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    }

    series_query_end_phase(baton);
}

static void
series_query_mapping(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    sds			cmd, key;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_mapping");
    seriesBatonCheckCount(baton, "series_query_mapping");
    seriesBatonReference(baton, "series_query_mapping");

    key = sdsnew("pcp:map:metric.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    sdsfree(key);
    redisSlotsRequest(baton->slots, cmd,
			series_query_mapping_callback, baton);
    sdsfree(cmd);
}

static void
series_lookup_finished(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_finished");
    baton->callbacks->on_done(baton->error, baton->userdata);
    freeSeriesGetLookup(baton);
}

static void
series_lookup_services(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    pmSeriesModule	*module = baton->module;
    seriesModuleData	*data = getSeriesModuleData(module);
    sds			option;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_services");
    seriesBatonReference(baton, "series_lookup_services");

    /* attempt to re-use existing slots connections */
    if (data == NULL) {
	baton->error = -ENOMEM;
    } else if (data->slots) {
	baton->slots = data->slots;
	series_query_end_phase(baton);
    } else {
	option = pmIniFileLookup(data->config, "redis", "enabled");
	if (option && strcmp(option, "false") == 0)
	    baton->error = -ENOTSUP;
	else
	    baton->slots = data->slots =
		redisSlotsConnect(
		    data->config, 1, baton->info,
		    series_query_end_phase, baton->userdata,
		    data->events, (void *)baton);
    }
}

static void
redis_get_sid_callback(
	redisClusterAsyncContext *redis, void *r, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    redisReply		*reply = r;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "redis_get_sid_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "redis_get_sid_callback");

    /* unpack - extract names for this source via context name map */
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	infofmt(msg, "expected array from %s %s (type=%s)",
		SMEMBERS, sid->name, redis_reply_type(reply));
	batoninfo(baton, PMLOG_RESPONSE, msg);
	baton->error = -EPROTO;
    } else if ((sts = series_map_reply(baton, sid->name,
			reply->elements, reply->element)) < 0) {
	baton->error = sts;
    }
    series_query_end_phase(baton);
}

static void
series_lookup_sources(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_sources");
    seriesBatonCheckCount(baton, "series_lookup_sources");
    seriesBatonReference(baton, "series_lookup_sources");

    for (i = 0; i < baton->u.lookup.nseries; i++) {
	seriesBatonReference(baton, "series_lookup_sources");
	sid = &baton->u.lookup.series[i];
	key = sdscatfmt(sdsempty(), "pcp:context.name:source:%S", sid->name);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
			redis_get_sid_callback, sid);
	sdsfree(cmd);
    }
    series_query_end_phase(baton);
}

int
pmSeriesSources(pmSeriesSettings *settings, int nsources, pmSID *sources, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_SOURCES_CALLS);

    if (nsources < 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nsources * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nsources, sources, settings->callbacks.on_context, contextmap);

    if (nsources == 0)
	return series_map_keys(baton, redisMapName(baton->u.lookup.map));

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_mapping;
    baton->phases[i++].func = series_lookup_sources;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static void
series_lookup_metrics(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    seriesGetSID	*sid;
    sds			cmd, key;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_metrics");
    seriesBatonCheckCount(baton, "series_lookup_metrics");

    for (i = 0; i < baton->u.lookup.nseries; i++) {
	seriesBatonReference(baton, "series_lookup_metrics");
	sid = &baton->u.lookup.series[i];
	key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%S", sid->name);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	sdsfree(key);
	redisSlotsRequest(baton->slots, cmd,
			redis_get_sid_callback, sid);
	sdsfree(cmd);
    }
}

int
pmSeriesMetrics(pmSeriesSettings *settings, int nseries, sds *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_METRICS_CALLS);

    if (nseries < 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetLookup(baton, nseries, series, settings->callbacks.on_metric, namesmap);

    if (nseries == 0)
	return series_map_keys(baton, redisMapName(baton->u.lookup.map));

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_lookup_mapping;
    baton->phases[i++].func = series_lookup_metrics;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}

static void
parsedelta(seriesQueryBaton *baton, sds string, struct timeval *result, const char *source)
{
    char		*error;
    sds			msg;
    int			sts;

    if ((sts = pmParseInterval(string, result, &error)) < 0) {
	infofmt(msg, "Cannot parse time %s with pmParseInterval:\n%s",
		source, error);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = sts;
	free(error);
    }
}

static void
parsetime(seriesQueryBaton *baton, sds string, struct timeval *result, const char *source)
{
    struct timeval	start = { 0, 0 };
    struct timeval	end = { PM_MAX_TIME_T, 0 };
    char		*error;
    sds			msg;
    int			sts;

    if ((sts = __pmParseTime(string, &start, &end, result, &error)) < 0) {
	infofmt(msg, "Cannot parse %s time with __pmParseTime:\n%s",
		source, error);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = sts;
	free(error);
    }
}

static void
parseuint(seriesQueryBaton *baton, sds string, unsigned int *vp, const char *source)
{
    unsigned int	value;
    char		*endnum;
    sds			msg;

    value = (unsigned int)strtoul(string, &endnum, 10);
    if (*endnum != '\0') {
	infofmt(msg, "Invalid sample %s requested - %s", source, string);
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = -EINVAL;
    } else {
	*vp = value;
    }
}

static void
parsezone(seriesQueryBaton *baton, sds string, int *zone, const char *source)
{
    char		error[PM_MAXERRMSGLEN];
    sds			msg;
    int			sts;

    if ((sts = pmNewZone(string)) < 0) {
	infofmt(msg, "Cannot parse %s with pmNewZone:\n%s - %s",
		source, string, pmErrStr_r(sts, error, sizeof(error)));
	batoninfo(baton, PMLOG_ERROR, msg);
	baton->error = sts;
    } else {
	*zone = sts;
    }
}

static void
parseseries(seriesQueryBaton *baton, sds series, unsigned char *result)
{
    unsigned int	i, off;
    char		*endptr, tuple[3] = {0};
    sds			msg;

    for (i = 0; i < 20; i++) {
	off = i * 2;
	tuple[0] = series[off];
	tuple[1] = series[off+1];
	result[i] = (unsigned char)strtoul(tuple, &endptr, 16);
	if (endptr != &tuple[2]) {
	    infofmt(msg, "Invalid SID %s near offset %u", series, off);
	    batoninfo(baton, PMLOG_ERROR, msg);
	    baton->error = -EINVAL;
	}
    }
}

static void
initSeriesGetValues(seriesQueryBaton *baton, int nseries, sds *series,
		pmSeriesTimeWindow *window)
{
    struct series_set	*result = &baton->u.query.root.result;
    struct timing	*timing = &baton->u.query.timing;
    struct timeval	offset;
    int			i;

    /* validate and convert 40-byte (ASCII) SIDs to internal 20-byte form */
    result->nseries = nseries;
    if ((result->series = calloc(nseries, 20)) == NULL) {
	baton->error = -ENOMEM;
    } else {
	for (i = 0; i < nseries; i++)
	    parseseries(baton, series[i], result->series + (i * 20));
    }
    if (baton->error) {
	if (result->series)
	    free(result->series);
	return;
    }

    /* validate and convert time window specification to internal struct */
    timing->window = *window;
    if (window->delta)
	parsedelta(baton, window->delta, &timing->delta, "delta");
    if (window->align)
	parsetime(baton, window->align, &timing->align, "align");
    if (window->start)
	parsetime(baton, window->start, &timing->start, "start");
    if (window->end)
	parsetime(baton, window->end, &timing->end, "end");
    if (window->range) {
	parsedelta(baton, window->range, &timing->start, "range");
	gettimeofday(&offset, NULL);
	tsub(&offset, &timing->start);
	timing->start = offset;
	timing->end.tv_sec = PM_MAX_TIME_T;
    }
    if (window->count)
	parseuint(baton, window->count, &timing->count, "count");
    if (window->offset)
	parseuint(baton, window->offset, &timing->offset, "offset");
    if (window->zone)
	parsezone(baton, window->zone, &timing->zone, "timezone");

    /* if no time window parameters passed, default to latest value */
    if (!series_time_window(timing))
	timing->count = 1;
}

int
pmSeriesValues(pmSeriesSettings *settings, pmSeriesTimeWindow *timing,
		int nseries, sds *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

    series_stats_inc(settings, SERIES_VALUES_CALLS);

    if (nseries <= 0)
	return -EINVAL;
    bytes = sizeof(seriesQueryBaton) + (nseries * sizeof(seriesGetSID));
    if ((baton = calloc(1, bytes)) == NULL)
	return -ENOMEM;
    initSeriesQueryBaton(baton, settings, arg);
    initSeriesGetValues(baton, nseries, series, timing);

    baton->current = &baton->phases[0];
    baton->phases[i++].func = series_lookup_services;
    baton->phases[i++].func = series_query_report_values;
    baton->phases[i++].func = series_lookup_finished;
    assert(i <= QUERY_PHASES);
    seriesBatonPhases(baton->current, i, baton);
    return 0;
}
