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

#include <assert.h>
#include <ctype.h>
#include "util.h"
#include "query.h"
#include "schema.h"
#include "libpcp.h"
#include "batons.h"
#include "slots.h"
#include "maps.h"
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <fnmatch.h>

#define SHA1SZ		20	/* internal sha1 hash buffer size in bytes */
#define QUERY_PHASES	6

typedef struct seriesGetSID {
    seriesBatonMagic	header;		/* MAGIC_SID */
    sds			name;		/* series or source SID */
    sds			metric;		/* back-pointer for instance series */
    int			freed;		/* freed individually on completion */
    void		*baton;
} seriesGetSID;

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
static void series_lookup_services(void *);
static void series_lookup_mapping(void *);
static void series_lookup_finished(void *);

sds	cursorcount;	/* number of elements in each SCAN call */

static void
initSeriesGetQuery(seriesQueryBaton *baton, node_t *root, timing_t *timing)
{
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "initSeriesGetQuery");
    baton->u.query.root = *root;
    baton->u.query.timing = *timing;
}

static void
freeSeriesGetQuery(seriesQueryBaton *baton)
{
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "freeSeriesGetQuery");
    seriesBatonCheckCount(baton, "freeSeriesGetQuery");
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
}

static void
initSeriesGetSID(seriesGetSID *sid, const char *name, int needfree, void *baton)
{
    initSeriesBatonMagic(sid, MAGIC_SID);
    sid->name = sdsnew(name);
    sid->freed = needfree;
    sid->baton = baton;
}

static void
freeSeriesGetSID(seriesGetSID *sid)
{
    int			needfree;

    seriesBatonCheckMagic(sid, MAGIC_SID, "freeSeriesGetSID");
    sdsfree(sid->name);
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

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_end_phase");

    if (baton->error == 0) {
	seriesPassBaton(&baton->current, baton, "series_query_end_phase");
    } else {	/* fail after waiting on outstanding I/O */
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
	default: break;
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

static int
extract_time(seriesQueryBaton *baton, pmSID series,
		redisReply *reply, sds *stamp)
{
    sds			msg, val;
    char		*point;

    if (reply->type == REDIS_REPLY_STRING) {
	val = sdscpylen(*stamp, reply->str, reply->len);
	if ((point = strchr(val, '-')) != NULL)
	    *point = '.';
	*stamp = val;
	return 0;
    }
    infofmt(msg, "expected string timestamp in series %s", series);
    batoninfo(baton, PMLOG_RESPONSE, msg);
    return -EPROTO;
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
	    /* TODO: propogate errors and mark records - separate callbacks? */
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
series_result_reply(seriesQueryBaton *baton, sds series, pmSeriesValue *value,
		int nelements, redisReply **elements)
{
    redisReply		*reply;
    sds			msg;
    int			i, sts;

    /* expecting timestamp:valueset pairs, then instance:value pairs */
    if (nelements % 2) {
	infofmt(msg, "expected time:valueset pairs in %s XRANGE", series);
	batoninfo(baton, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    for (i = 0; i < nelements; i += 2) {
	reply = elements[i+1];
	if ((sts = extract_time(baton, series, elements[i],
				&value->timestamp)) < 0) {
	    baton->error = sts;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    infofmt(msg, "expected value array for series %s %s (type=%s)",
			series, XRANGE, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	    baton->error = -EPROTO;
	} else if ((sts = series_instance_reply(baton, series, value,
				reply->elements, reply->element)) < 0) {
	    baton->error = sts;
	}
    }
    return 0;
}

static void
series_values_reply(seriesQueryBaton *baton, sds series,
		int nelements, redisReply **elements, void *arg)
{
    pmSeriesValue	value;
    redisReply		*reply;
    int			i, sts;

    value.timestamp = sdsempty();
    value.series = sdsempty();
    value.data = sdsempty();

    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if ((sts = series_result_reply(baton, series, &value,
				reply->elements, reply->element)) < 0)
	    baton->error = sts;
    }

    sdsfree(value.timestamp);
    sdsfree(value.series);
    sdsfree(value.data);
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
		printf("    %s\n", hashbuf);
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

    for (i = 0, cp = saved = small; i < nsmall; i++, cp ++) {
	if (!bsearch(cp, large, nlarge, SHA1SZ, series_compare))
	    continue;		/* no match, continue advancing cp only */
	if (saved != cp)
	    memcpy(saved, cp, SHA1SZ);
	saved++;		/* stashed, advance cp & saved pointers */
    }

    if ((total = (saved - small)) < nsmall) {
	/* shrink the smaller set down further */
	if ((small = realloc(small, total * SHA1SZ)) == NULL)
	    return -ENOMEM;
    }

    if (pmDebugOptions.series && pmDebugOptions.desperate) {
	char		hashbuf[42];

	printf("Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp++, i++) {
	    pmwebapi_hash_str(cp, hashbuf, sizeof(hashbuf));
	    printf("    %s\n", hashbuf);
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
	printf("Union of large(%d) and small(%d) series\n", nlarge, nsmall);

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

	printf("Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += SHA1SZ, i++) {
	    pmwebapi_hash_str(cp, hashbuf, sizeof(hashbuf));
	    printf("    %s\n", hashbuf);
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
    sts = regexec((const regex_t *)np->regex, string, 0, NULL, 0);
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

    /* result array sanity checking */
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
	    return -ENOMEM;
	}
	matches[np->nmatches++] = key;
	np->matches = matches;
    }

out:
    if (np->cursor > 0)	/* still more to retrieve - kick off the next batch */
	series_pattern_match(baton, np);

    return nelements;
}

static void
series_prepare_maps_pattern_reply(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    node_t		*np = (node_t *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    const char		*name;
    node_t		*left;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_maps_pattern_reply");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_prepare_maps_pattern_reply, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    assert(np->type == N_GLOB || np->type == N_REQ || np->type == N_RNE);

    left = np->left;

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array for %s key \"%s\" (type=%s)",
		    node_subtype(left), left->key, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
	baton->error = -EPROTO;
    } else {
	name = left->key + sizeof("pcp:map:") - 1;
	if (pmDebugOptions.series)
	    printf("%s %s\n", node_subtype(np->left), np->key);
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
    redisSlotsRequest(baton->slots, HSCAN, key, cmd,
				series_prepare_maps_pattern_reply, np);
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
    sds			val = sdsnewlen(SDS_NOINIT, 40);

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
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    node_t		*np = (node_t *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)np->baton;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_smembers_reply");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_prepare_smembers_reply, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array for %s set \"%s\" (type=%s)",
			node_subtype(np->left), np->right->value,
			redis_reply_type(reply));
	    batoninfo(baton, PMLOG_CORRUPT, msg);
	}
	baton->error = -EPROTO;
    } else {
	if (pmDebugOptions.series)
	    printf("%s %s\n", node_subtype(np->left), np->key);
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
    sds                 cmd, key = sdsdup(kp);

    cmd = redis_command(2);
    cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
    cmd = redis_param_sds(cmd, key);
    redisSlotsRequest(baton->slots, SMEMBERS, key, cmd,
			series_prepare_smembers_reply, np);
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
	/* TODO */
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
series_prepare_time_reply(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_prepare_time_reply");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_prepare_time_reply");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_prepare_time_reply, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s XSTREAM values (type=%s)",
			sid->name, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
	baton->error = -EPROTO;
    } else {
	series_values_reply(baton, sid->name, reply->elements, reply->element, arg);
    }
    freeSeriesGetSID(sid);

    series_query_end_phase(baton);
}

#define DEFAULT_VALUE_COUNT 10

static void
series_prepare_time(seriesQueryBaton *baton, series_set_t *result)
{
    timing_t		*tp = &baton->u.query.timing;
    unsigned char	*series = result->series;
    seriesGetSID	*sid;
    char		buffer[64];
    sds			count, start, end, key, cmd;
    unsigned int	i;

    start = sdsnew(timeval_stream_str(&tp->start, buffer, sizeof(buffer)));
    if (pmDebugOptions.series)
	fprintf(stderr, "START: %s\n", start);

    if (tp->end.tv_sec)
	end = sdsnew(timeval_stream_str(&tp->end, buffer, sizeof(buffer)));
    else
	end = sdsnew("+");	/* "+" means "no end" - to the most recent */
    if (pmDebugOptions.series)
	fprintf(stderr, "END: %s\n", end);

    if (tp->count == 0)
	tp->count = DEFAULT_VALUE_COUNT;
    count = sdscatfmt(sdsempty(), "%u", tp->count);
    if (pmDebugOptions.series)
	fprintf(stderr, "COUNT: %u\n", tp->count);

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

	/* XREAD key t1 t2 [COUNT count] */
	cmd = redis_command(6);
	cmd = redis_param_str(cmd, XRANGE, XRANGE_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, start);
	cmd = redis_param_sds(cmd, end);
	cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
	cmd = redis_param_sds(cmd, count);
	redisSlotsRequest(baton->slots, XRANGE, key, cmd,
				series_prepare_time_reply, sid);
    }
    sdsfree(count);
    sdsfree(start);
    sdsfree(end);
}

static void
series_report_set(seriesQueryBaton *baton, series_set_t *set)
{
    unsigned char	*series = set->series;
    char		hashbuf[42];
    sds			sid = NULL;
    int			i;

    if (set->nseries)
	sid = sdsempty();
    for (i = 0; i < set->nseries; series += SHA1SZ, i++) {
	pmwebapi_hash_str(series, hashbuf, sizeof(hashbuf));
	sid = sdscpylen(sid, hashbuf, 40);
	baton->callbacks->on_match(sid, baton->userdata);
    }
    if (sid)
	sdsfree(sid);
}

static void
series_query_report_matches(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_query_report_matches");
    seriesBatonCheckCount(baton, "series_query_report_matches");

    seriesBatonReference(baton, "series_query_report_matches");
    series_report_set(baton, &baton->u.query.root.result);
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

static int
series_time_window(timing_t *tp)
{
    if (tp->ranges || tp->starts || tp->ends || tp->counts)
	return 1;
    return 0;
}

static void
series_query_services(void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    pmSeriesModule	*module = baton->module;
    seriesModuleData	*data = getSeriesModuleData(module);

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

    if ((flags & PM_SERIES_FLAG_METADATA) || !series_time_window(timing))
	/* Report matching series IDs, unless time windowing */
	baton->phases[i++].func = series_query_report_matches;
    else
	/* Report actual values within the given time window */
	baton->phases[i++].func = series_query_report_values;

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

    key = sdsnewlen(SDS_NOINIT, 20);
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
    sdsfree(key);

    return sts;
}

static void
series_map_keys_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    redisReply		*child;
    sds			val, msg;
    int			sts;
    unsigned int	i;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_map_keys_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_map_keys_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

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
	if (sts < 0) {
	    infofmt(msg, "expected array from string map %s (reply=%s)",
		HVALS, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
    redisSlotsRequest(baton->slots, HVALS, key, cmd,
		   	 series_map_keys_callback, baton);
    return 0;
}

static void
series_label_value_reply(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetLabelMap	*value = (seriesGetLabelMap *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)value->baton;
    redisMapEntry	*entry;
    pmSeriesLabel	label;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(value, MAGIC_LABELMAP, "series_label_value_reply");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_label_value_reply, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    /* unpack - produce reverse map of ids-to-values for each entry */
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	reverse_map(baton, value->map, reply->elements, reply->element);
    } else {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s %s.%s.value (type=%s)", HGETALL,
			 "pcp:map:label", value->mapID, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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

    nmapID = sdsnewlen(SDS_NOINIT, 20);
    vmapID = sdsnewlen(SDS_NOINIT, 20);
    vkey = sdsempty();

    /* perform the label value reverse lookup */
    nelements /= 2;
    for (i = 0; i < nelements; i++) {
	index = i * 2;

	sdsclear(nmapID);
	nmapID = sdscatlen(nmapID, elements[index]->str, elements[index]->len);
	sdsclear(vmapID);
	vmapID = sdscatlen(vmapID, elements[index+1]->str, elements[index+1]->len);

	if ((entry = redisMapLookup(baton->u.lookup.map, nmapID)) != NULL) {
	    sdsclear(vkey);
	    pmwebapi_hash_str((unsigned char *)nmapID, hashbuf, sizeof(hashbuf));
	    vkey = sdscatfmt(vkey, "label.%s.value", hashbuf);
	    vmap = redisMapCreate(vkey);
	    name = redisMapValue(entry);

	    baton->callbacks->on_label(series, name, baton->userdata);

	    if ((labelmap = calloc(1, sizeof(seriesGetLabelMap))) == NULL) {
		infofmt(msg, "%s - label value lookup OOM", series);
		batoninfo(baton, PMLOG_ERROR, msg);
		sts = -ENOMEM;
		continue;
	    }
	    initSeriesGetLabelMap(labelmap, series, name, vmap, vmapID, vkey, baton);

	    seriesBatonReference(baton, "series_label_reply");

	    pmwebapi_hash_str((unsigned char *)nmapID, hashbuf, sizeof(hashbuf));
	    key = sdscatfmt(sdsempty(), "pcp:map:label.%s.value", hashbuf);
	    cmd = redis_command(2);
	    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
	    cmd = redis_param_sds(cmd, key);
	    redisSlotsRequest(baton->slots, HGETALL, key, cmd,
				series_label_value_reply, labelmap);
	} else {
	    infofmt(msg, "%s - timeseries label map", series);
	    batoninfo(baton, PMLOG_CORRUPT, msg);
	    sts = -EINVAL;
	}
    }

    sdsfree(nmapID);
    sdsfree(vmapID);
    sdsfree(vkey);
    return sts;
}

static void
series_lookup_labels_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton    *baton = (seriesQueryBaton *)sid->baton;
    sds			msg;
    int                 sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_lookup_labels_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_labels_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			    cmd, series_lookup_labels_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s %s:%s (type=%s)",
			HGETALL, "pcp:labelvalue:series", sid->name,
			redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
	redisSlotsRequest(baton->slots, HGETALL, key, cmd,
			series_lookup_labels_callback, sid);
    }
}

int
pmSeriesLabels(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

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

    /* sanity check - were we given an invalid series identifier? */
    if (elements[0]->type == REDIS_REPLY_NIL) {
	infofmt(msg, "no descriptor for series identifier %s", series);
	batoninfo(baton, PMLOG_ERROR, msg);
	return -EINVAL;
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

static void
redis_series_desc_reply(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    pmSeriesDesc	desc;
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    sds			msg;
    int			sts;

    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_series_desc_reply, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    desc.indom = sdsempty();
    desc.pmid = sdsempty();
    desc.semantics = sdsempty();
    desc.source = sdsempty();
    desc.type = sdsempty();
    desc.units = sdsempty();

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array type from series %s %s (type=%s)",
			sid->name, HMGET, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
	redisSlotsRequest(baton->slots, HMGET, key, cmd, redis_series_desc_reply, sid);
    }
}

int
pmSeriesDescs(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

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
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    pmSeriesInst	inst;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_instances_reply_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_instances_reply_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_instances_reply_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    inst.instid = sdsempty();
    inst.name = sdsempty();
    inst.source = sdsempty();
    inst.series = sdsempty();

    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array from series %s %s (type=%s)",
			HMGET, sid->name, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
	redisSlotsRequest(baton->slots, HMGET, key, cmd,
				series_instances_reply_callback, sid);
    }
    sdsfree(name);
}

void
series_lookup_instances_callback(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "series_lookup_instances_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_instances_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, series_lookup_instances_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	series_instances_reply(baton, sid->name, reply->elements, reply->element);
    } else {
	if (sts < 0) {
	    infofmt(msg, "expected array from series %s %s (type=%s)",
			SMEMBERS, sid->name, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
	redisSlotsRequest(baton->slots, SMEMBERS, key, cmd,
			series_lookup_instances_callback, sid);
    }
}

int
pmSeriesInstances(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

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
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    seriesQueryBaton	*baton = (seriesQueryBaton *)arg;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "redis_lookup_mapping_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_lookup_mapping_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    /* unpack - produce reverse map of ids-to-names for each context */
    if (LIKELY(reply && reply->type == REDIS_REPLY_ARRAY)) {
	reverse_map(baton, baton->u.lookup.map, reply->elements, reply->element);
    } else {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s %s (type=%s)",
		    HGETALL, "pcp:map:context.name", redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
    redisSlotsRequest(baton->slots, HGETALL, key, cmd,
			redis_lookup_mapping_callback, baton);
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

    seriesBatonCheckMagic(baton, MAGIC_QUERY, "series_lookup_services");
    seriesBatonReference(baton, "series_lookup_services");

    /* attempt to re-use existing slots connections */
    if (data == NULL) {
	baton->error = -ENOMEM;
    } else if (data->slots) {
	baton->slots = data->slots;
	series_query_end_phase(baton);
    } else {
	baton->slots = data->slots =
	    redisSlotsConnect(
		data->config, 1, baton->info,
		series_query_end_phase, baton->userdata,
		data->events, (void *)baton);
    }
}

static void
redis_get_sid_callback(
	redisAsyncContext *redis, redisReply *reply, const sds cmd, void *arg)
{
    seriesGetSID	*sid = (seriesGetSID *)arg;
    seriesQueryBaton	*baton = (seriesQueryBaton *)sid->baton;
    sds			msg;
    int			sts;

    seriesBatonCheckMagic(sid, MAGIC_SID, "redis_get_sid_callback");
    seriesBatonCheckMagic(baton, MAGIC_QUERY, "redis_get_sid_callback");
    sts = redisSlotsRedirect(baton->slots, reply, baton->info, baton->userdata,
			     cmd, redis_get_sid_callback, arg);
    if (sts > 0)
	return;	/* short-circuit as command was re-submitted */

    /* unpack - extract names for this source via context name map */
    if (UNLIKELY(reply == NULL || reply->type != REDIS_REPLY_ARRAY)) {
	if (sts < 0) {
	    infofmt(msg, "expected array from %s %s (type=%s)",
			SMEMBERS, sid->name, redis_reply_type(reply));
	    batoninfo(baton, PMLOG_RESPONSE, msg);
	}
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
	redisSlotsRequest(baton->slots, SMEMBERS, key, cmd,
			redis_get_sid_callback, sid);
    }
    series_query_end_phase(baton);
}

int
pmSeriesSources(pmSeriesSettings *settings, int nsources, pmSID *sources, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

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
	redisSlotsRequest(baton->slots, SMEMBERS, key, cmd,
			redis_get_sid_callback, sid);
    }
}

int
pmSeriesMetrics(pmSeriesSettings *settings, int nseries, sds *series, void *arg)
{
    seriesQueryBaton	*baton;
    size_t		bytes;
    unsigned int	i = 0;

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
