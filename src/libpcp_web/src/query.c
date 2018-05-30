/*
 * Copyright (c) 2017-2018 Red Hat.
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

#include <ctype.h>
#include "util.h"
#include "query.h"
#include "schema.h"
#include "series.h"
#include "libpcp.h"
#include "slots.h"
#include "maps.h"

#define SHA1SZ	20	/* internal sha1 hash buffer size in bytes */

typedef struct {
    redisSlots          *redis;

    settings_t		*settings;
    void		*arg;

    unsigned int	index;
    unsigned int	count;
    redisReply		**replies;
} SOLVER;

#define queryfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define queryinfo(set, level, msg, arg)	\
	((set)->on_info(level, msg, arg), sdsfree(msg))
#define querydesc(set, sid,desc, arg)	(set)->on_desc(sid, desc, arg)
#define queryinst(set, sid,inst, arg)	(set)->on_inst(sid, inst, arg)
#define querylabelmap(set, sid,l,arg)	(set)->on_labelmap(sid, l, arg)
#define queryinstance(set, sid,in, arg)	(set)->on_instance(sid, in, arg)
#define querylabel(set, sid, name, arg)	(set)->on_label(sid, name, arg)
#define queryvalue(set, sid,val, arg)	(set)->on_value(sid, val, arg)
#define querydone(set, sts, arg)	(set)->on_done(sts, arg)

#define solverfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define solvermsg(SP, level, message)		\
	((SP)->settings->on_info((level), (msg), (SP)->arg), sdsfree(msg))
#define solvermatch(SP, sid)			\
	((SP)->settings->on_match((sid), (SP)->arg))
#define solvervalue(SP, sid, n, value)	\
	((SP)->settings->on_value((sid), (n), (value), (SP)->arg))

static int series_union(series_set_t *, series_set_t *);
static int series_intersect(series_set_t *, series_set_t *);

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
extract_string(pmSeriesSettings *settings, pmSID series,
	redisReply *element, sds *string, const char *message, void *arg)
{
    sds			msg;

    if (element->type == REDIS_REPLY_STRING) {
	*string = sdscpylen(*string, element->str, element->len);
	return 0;
    }
    queryfmt(msg, "expected string result for %s of series %s (got %s)",
			message, series, redis_reply(element->type));
    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
    return -EINVAL;
}

static int
extract_mapping(pmSeriesSettings *settings, pmSID series, redisMap *map,
	redisReply *element, sds *string, const char *message, void *arg)
{
    redisMapEntry	*entry;
    sds			msg;

    if (element->type == REDIS_REPLY_STRING) {
	if ((entry = redisRMapLookup(map, element->str)) != NULL) {
	    *string = redisRMapValue(entry);
	    return 0;
	}
	queryfmt(msg, "bad mapping for %s of series %s", message, series);
	queryinfo(settings, PMLOG_CORRUPT, msg, arg);
	return -EINVAL;
    }
    queryfmt(msg, "expected string for %s of series %s", message, series);
    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
    return -EPROTO;
}

static int
extract_sha1(pmSeriesSettings *settings, pmSID series,
	redisReply *element, sds *sha, const char *message, void *arg)
{
    sds			msg;
    char		*hash;

    if (element->type != REDIS_REPLY_STRING) {
	queryfmt(msg, "expected string result for %s of series %s",
			message, series);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EINVAL;
    }
    if (element->len != 20) {
	queryfmt(msg, "expected sha1 for %s of series %s, got %ld bytes",
			message, series, (long)element->len);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EINVAL;
    }
    hash = pmwebapi_hash_str((unsigned char *)element->str);
    *sha = sdscpylen(*sha, hash, 40);
    return 0;
}

static int
extract_time(pmSeriesSettings *settings, pmSID series,
	redisReply *element, sds *stamp, void *arg)
{
    sds			msg, val;
    char		*point;

    if (element->type == REDIS_REPLY_STATUS) {
	val = sdscpylen(*stamp, element->str, element->len);
	if ((point = strchr(val, '-')) != NULL)
	    *point = '.';
	*stamp = val;
	return 0;
    }
    queryfmt(msg, "expected string timestamp in series %s", series);
    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
    return -EPROTO;
}

/*
 * Report a timeseries result - timestamps and (instance) values
 */
static int
series_instance_reply(pmSeriesSettings *settings, sds series,
	pmSeriesValue *value, int nelements, redisReply **elements, void *arg)
{
    char		*hash;
    sds			inst;
    int			i, sts = 0;

    for (i = 0; i < nelements; i += 2) {
	inst = value->series;
	if (extract_string(settings, series, elements[i],
				&inst, "series", arg) < 0) {
	    sts = -EPROTO;
	    continue;
	}
	if (sdslen(inst) == 0) {	/* no InDom, use series */
	    inst = sdscpylen(inst, series, 40);
	} else if (sdslen(inst) == 20) {
	    hash = pmwebapi_hash_str((const unsigned char *)inst);
	    inst = sdscpylen(inst, hash, 40);
	} else {
	    /* TODO: errors and mark records - new callback(s)? */
	    continue;
	}
	value->series = inst;

	if (extract_string(settings, series, elements[i+1],
				&value->data, "value", arg) < 0)
	    sts = -EPROTO;
	else
	    queryvalue(settings, series, value, arg);
    }
    return sts;
}

static int
series_result_reply(SOLVER *sp, sds series, pmSeriesValue *value,
		int nelements, redisReply **elements, void *arg)
{
    pmSeriesSettings	*settings = sp->settings;
    redisReply		*reply;
    sds			msg;
    int			i, rc, sts = 0;

    /* expecting timestamp:valueset pairs, then instance:value pairs */
    if (nelements % 2) {
	solverfmt(msg, "expected time:valueset pairs in %s XRANGE", series);
	solvermsg(sp, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    for (i = 0; i < nelements; i += 2) {
	if ((rc = extract_time(settings, series, elements[i],
				&value->timestamp, arg)) < 0) {
	    sts = rc;
	    continue;
	}
	reply = elements[i+1];
	if (reply->type != REDIS_REPLY_ARRAY) {
	    solverfmt(msg, "expected value array for series %s %s (type=%s)",
			series, XRANGE, redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	}
	if ((rc = series_instance_reply(settings, series, value,
				reply->elements, reply->element, arg)) < 0) {
	    sts = rc;
	    continue;
	}
    }
    return sts;
}

static int
series_values_reply(SOLVER *sp, sds series,
		int nelements, redisReply **elements, void *arg)
{
    pmSeriesValue	value;
    redisReply		*reply;
    int			i, rc, sts = 0;

    value.timestamp = sdsnewlen(NULL, 32);
    value.series = sdsnewlen(NULL, 40);
    value.data = sdsempty();

    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if ((rc = series_result_reply(sp, series, &value,
				reply->elements, reply->element, arg)) < 0)
	    sts = rc;
    }

    sdsfree(value.timestamp);
    sdsfree(value.series);
    sdsfree(value.data);
    return sts;
}

/*
 * Save the series hash identifiers contained in a Redis response
 * for all series that are not already in this nodes set (union).
 * Used at the leaves of the query tree, then merged result sets
 * are propagated upward.
 */
static int
node_series_reply(SOLVER *sp, node_t *np, int nelements, redisReply **elements)
{
    series_set_t	set;
    unsigned char	*series;
    redisReply		*reply;
    sds			msg;
    int			i, sts = 0;

    if (nelements <= 0)
	return nelements;

    if ((series = (unsigned char *)calloc(nelements, SHA1SZ)) == NULL) {
	solverfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"series reply", (__int64_t)nelements * SHA1SZ);
	solvermsg(sp, PMLOG_REQUEST, msg);
	return -ENOMEM;
    }
    set.series = series;
    set.nseries = nelements;

    for (i = 0; i < nelements; i++) {
	reply = elements[i];
	if (reply->type == REDIS_REPLY_STRING) {
	    memcpy(series, reply->str, SHA1SZ);
	    if (pmDebugOptions.series)
		printf("    %s\n", pmwebapi_hash_str(series));
	    series += SHA1SZ;
	} else {
	    solverfmt(msg, "expected string in %s set \"%s\" (type=%s)",
		    node_subtype(np->left), np->left->key,
		    redis_reply(reply->type));
	    solvermsg(sp, PMLOG_REQUEST, msg);
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

    if (pmDebugOptions.series) {
	printf("Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp++, i++)
	    printf("    %s\n", pmwebapi_hash_str(cp));
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
	if ((large = realloc(large, (nlarge + need) * SHA1SZ)) == NULL)
	    return -ENOMEM;
	cp = large + (nlarge * SHA1SZ);
	memcpy(cp, small, need * SHA1SZ);
	total = nlarge + need;
    } else {
	total = nlarge;
    }

    if (pmDebugOptions.series) {
	printf("Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += SHA1SZ, i++)
	    printf("    %s\n", pmwebapi_hash_str(cp));
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

/*
 * Add a node subtree representing glob (N_GLOB) pattern matches.
 * Each of these matches are then further evaluated (as if N_EQ).
 * Response format is described at https://redis.io/commands/scan
 */
static int
node_glob_reply(SOLVER *sp, node_t *np, const char *name, int nelements,
		redisReply **elements)
{
    redisReply		*reply, *r;
    sds			msg, key, *matches;
    int			i;

    if (nelements != 2) {
	solverfmt(msg, "expected cursor and results from %s (got %d elements)",
			HSCAN, nelements);
	solvermsg(sp, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    /* Update the cursor, in case subsequent calls are needed */
    reply = elements[0];
    if (!reply || reply->type != REDIS_REPLY_STRING) {
	solverfmt(msg, "expected integer cursor result from %s (got %s)",
			HSCAN, reply ? redis_reply(reply->type) : "null");
	solvermsg(sp, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }
    np->cursor = strtoull(reply->str, NULL, 10);

    reply = elements[1];
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
	solverfmt(msg, "expected array of results from %s (got %s)",
			HSCAN, reply ? redis_reply(reply->type) : "null");
	solvermsg(sp, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

    if ((nelements = reply->elements) == 0) {
	if (np->result.series)
	    free(np->result.series);
	np->result.nseries = 0;
	return 0;
    }

    /* result array sanity checking */
    if (nelements % 2) {
	solverfmt(msg, "expected even number of results from %s (not %d)",
		    HSCAN, nelements);
	solvermsg(sp, PMLOG_REQUEST, msg);
	return -EPROTO;
    }
    for (i = 0; i < nelements; i += 2) {
	r = reply->element[i];
	if (r->type != REDIS_REPLY_STRING) {
	    solverfmt(msg, "expected only string results from %s (type=%s)",
		    HSCAN, redis_reply(r->type));
	    solvermsg(sp, PMLOG_REQUEST, msg);
	    return -EPROTO;
	}
    }

    /* response is matching key:value pairs from the scanned hash */
    nelements /= 2;
    if ((matches = (sds *)calloc(nelements, sizeof(sds))) == NULL) {
	solverfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)",
			"glob reply", (__int64_t)nelements * sizeof(sds));
	solvermsg(sp, PMLOG_REQUEST, msg);
	return -ENOMEM;
    }
    for (i = 0; i < nelements; i++) {
	r = reply->element[i*2+1];
	key = sdsnew("pcp:series:");
	key = sdscatfmt(key, "%s:%s", name, r->str);
	if (pmDebugOptions.series)
	    printf("adding glob result key: %s\n", key);
	matches[i] = key;
    }
    np->nmatches = nelements;
    np->matches = matches;
    return nelements;
}

/*
 * Map human names to internal Redis identifiers.
 */
static int
series_prepare_maps(SOLVER *sp, node_t *np, int level)
{
    redisSlots	        *redis = sp->redis;
    const char		*name;
    sds			cmd, cur, key, val;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_maps(sp, np->left, level+1)) < 0)
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
	    key = sdsnew("pcp:map:context.name");
	    np->key = sdsdup(key);
	    np->subtype = N_CONTEXT;
	    cmd = redis_command(3);
	    cmd = redis_param_str(cmd, HGET, HGET_LEN);
	    cmd = redis_param_sds(cmd, key);
	    cmd = redis_param_str(cmd, name, strlen(name));
	    redis_submit(redis, HGET, key, cmd);
	    sp->count++;
	} else {
	    if ((name = series_label_name(np->value)) == NULL)
		name = np->value;
	    key = sdsnew("pcp:map:label.name");
	    np->key = sdsdup(key);
	    np->subtype = N_LABEL;
	    cmd = redis_command(3);
	    cmd = redis_param_str(cmd, HGET, HGET_LEN);
	    cmd = redis_param_sds(cmd, key);
	    cmd = redis_param_str(cmd, name, strlen(name));
	    redis_submit(redis, HGET, key, cmd);
	    sp->count++;
	}
	break;

    case N_GLOB:	/* indirect hash lookup with key globbing */
	cur = sdscatfmt(sdsempty(), "%U", np->cursor);
	val = np->right->value;
	key = sdsdup(np->left->key);
	cmd = redis_command(7);
	cmd = redis_param_str(cmd, HSCAN, HSCAN_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, cur);	/* cursor */
	cmd = redis_param_str(cmd, "MATCH", sizeof("MATCH")-1);
	cmd = redis_param_sds(cmd, val);	/* pattern */
	cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
	cmd = redis_param_str(cmd, "256", sizeof("256")-1);
	redis_submit(redis, HSCAN, key, cmd);
	sdsfree(cur);
	sp->count++;
	break;

    default:
	break;
    }

    return series_prepare_maps(sp, np->right, level+1);
}

/*
 * Find the sets of series corresponding to leaf search parameters.
 */
static int
series_resolve_maps(SOLVER *sp, node_t *np, int level)
{
    redisSlots	        *redis = sp->redis;
    redisReply		*reply;
    node_t		*left;
    const char		*name;
    sds			msg;
    int			sts = 0;

    if (np == NULL)
	return 0;

    series_resolve_maps(sp, np->left, level+1);

    switch (np->type) {
    case N_NAME:
	/* TODO: need to handle JSONB label name nesting. */
	/* TODO: need lookup via source:context.name set. */

	/* setup any label and context name map identifiers needed */
	if (np->subtype == N_LABEL || np->subtype == N_CONTEXT) {
	    if (redisGetReply(sp->redis->control, (void **)&reply) != REDIS_OK) {
		solverfmt(msg, "no %s named \"%s\" found (error=%d)",
			node_subtype(np), np->value, sp->redis->control->err);
		solvermsg(sp, PMLOG_RESPONSE, msg);
		sts = -EPROTO;
	    } else if (reply->type != REDIS_REPLY_STRING) {
		solverfmt(msg, "expected string for %s map \"%s\" (type=%s)",
			node_subtype(np), np->value, redis_reply(reply->type));
		solvermsg(sp, PMLOG_RESPONSE, msg);
		sts = -EPROTO;
	    } else {
		sdsfree(np->key);
		sp->replies[sp->index++] = reply;
		if (np->subtype == N_LABEL) {
		    np->key = sdsnew("pcp:map:");
		    np->key = sdscatprintf(np->key, "%s.%s.value",
					node_subtype(np), reply->str);
		}
		if (np->subtype == N_CONTEXT) {
		    np->key = sdsnew("pcp:source:");
		    np->key = sdscatprintf(np->key, "%s.name:%s",
					node_subtype(np), reply->str);
		}
	    }
	}
	break;

    case N_GLOB:	/* indirect hash lookup with key globbing */
	/* TODO: need to handle multiple sets of results. */

	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;

	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "map table %s key %s not found",
		    left->key, np->right->value);
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    solverfmt(msg, "expected array for %s key \"%s\" (type=%s)",
		    node_subtype(left), left->key, redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    sp->replies[sp->index++] = reply;
	    if (pmDebugOptions.series)
		printf("%s %s\n", node_subtype(np->left), np->key);
	    sts = node_glob_reply(sp, np, name, reply->elements, reply->element);
	}
	break;

    default:
	break;
    }
    if (sts < 0)
	return sts;

    return series_resolve_maps(sp, np->right, level+1);
}

static sds
series_node_value(node_t *np)
{
    /* special JSON cases still to do: null, true, false */
    if (np->left->type == N_NAME &&
	np->left->subtype == N_LABEL &&
	np->right->type == N_STRING) {
	np->right->subtype = N_LABEL;
	return sdscatfmt(sdsempty(), "\"%S\"", np->right->value);
    }
    return sdsdup(np->right->value);
}

/*
 * Prepare evaluation of leaf nodes.
 */
static int
series_prepare_eval(SOLVER *sp, node_t *np, int level)
{
    redisSlots	        *redis = sp->redis;
    sds 		key, val, cmd;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_eval(sp, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:		/* direct hash lookup */
	val = series_node_value(np);
	key = sdsdup(np->left->key);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, HGET, HGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, val);
	redis_submit(redis, HGET, key, cmd);
	sdsfree(val);
	sp->count++;
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }

    return series_prepare_eval(sp, np->right, level+1);
}

static int
series_resolve_eval(SOLVER *sp, node_t *np, int level)
{
    redisSlots	        *redis = sp->redis;
    redisReply		*reply;
    const char		*name;
    node_t		*left;
    sds			msg;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_resolve_eval(sp, np->left, level + 1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:		/* direct hash lookup */
	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;

	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "map table %s key %s not found",
		    left->key, np->right->value);
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type == REDIS_REPLY_NIL) {
	    solverfmt(msg, "no match for time series query");
	    solvermsg(sp, PMLOG_ERROR, msg);
	    sts = -EINVAL;
	} else if (reply->type != REDIS_REPLY_STRING) {
	    solverfmt(msg, "expected string for %s key \"%s\" (type=%s)",
		    node_subtype(left), left->key, redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    sp->replies[sp->index++] = reply;
	    sdsfree(np->key);
	    if (np->subtype == N_CONTEXT)
		np->key = sdsnew("pcp:source:");
	    else
		np->key = sdsnew("pcp:series:");
	    np->key = sdscatfmt(np->key, "%s:%s", name, reply->str);
	}
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }
    if (sts < 0)
	return sts;

    return series_resolve_eval(sp, np->right, level+1);
}

static void
series_prepare_smembers(SOLVER *sp, sds key)
{
    redisSlots	        *redis = sp->redis;
    sds			cmd;

    cmd = redis_command(2);
    cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, SMEMBERS, key, cmd);
    sp->count++;
}

static int
series_resolve_smembers(SOLVER *sp, node_t *np)
{
    redisSlots	        *redis = sp->redis;
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	solverfmt(msg, "map table %s key %s not found",
			np->left->key, np->right->value);
	solvermsg(sp, PMLOG_CORRUPT, msg);
	sts = -EPROTO;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	solverfmt(msg, "expected array for %s set \"%s\" (type=%s)",
			node_subtype(np->left), np->right->value,
			redis_reply(reply->type));
	solvermsg(sp, PMLOG_CORRUPT, msg);
	sts = -EPROTO;
    } else {
	sp->replies[sp->index++] = reply;
	if (pmDebugOptions.series)
	    printf("%s %s\n", node_subtype(np->left), np->key);
	sts = node_series_reply(sp, np, reply->elements, reply->element);
    }
    return sts;
}

/*
 * Prepare evaluation of internal nodes.
 */
static int
series_prepare_expr(SOLVER *sp, node_t *np, int level)
{
    int			sts, i;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((series_prepare_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:		/* direct hash lookup */
	if (np->key)
	    series_prepare_smembers(sp, sdsdup(np->key));
	break;

    case N_GLOB:	/* globbing or regular expression lookups */
    case N_REQ:
    case N_RNE:
	for (i = 0; i < np->nmatches; i++)
	    series_prepare_smembers(sp, np->matches[i]);
	if (np->matches) {
	    free(np->matches);
	    np->matches = NULL;
	}
	break;

    case N_LT: case N_LEQ: case N_GEQ: case N_GT: case N_NEQ: case N_NEG:
	/* TODO */
	break;

    default:
	break;
    }
    return sts;
}

static int
series_resolve_expr(SOLVER *sp, node_t *np, int level)
{
    int			sts, rc, i;

    if (np == NULL)
	return 0;

    /* evaluate leaves first then interior nodes */
    if ((sts = series_resolve_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((sts = series_resolve_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:
	if (np->key)
	    sts = series_resolve_smembers(sp, np);
	break;

    case N_GLOB:
    case N_REQ:
    case N_RNE:
	for (i = 0; i < np->nmatches; i++) {
	    if ((rc = series_resolve_smembers(sp, np)) < 0)
		sts = rc;
	}
	np->nmatches = 0;	/* processed this batch */
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

#define DEFAULT_VALUE_COUNT 10

static int
series_prepare_time(SOLVER *sp, timing_t *tp, series_set_t *result)
{
    unsigned char	*series = result->series;
    redisSlots	        *redis = sp->redis;
    sds			count, start, end, key, cmd;
    int			i, sts = 0;

    start = sdsnew(timeval_str(&tp->start));
    if (pmDebugOptions.series)
	fprintf(stderr, "START: %s\n", start);

    if (tp->end.tv_sec)
	end = sdsnew(timeval_str(&tp->end));
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
	key = sdsempty();
	key = sdscatfmt(key, "pcp:values:series:%s", pmwebapi_hash_str(series));

	/* XREAD key t1 t2 [COUNT count] */
	cmd = redis_command(6);
	cmd = redis_param_str(cmd, XRANGE, XRANGE_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, start);
	cmd = redis_param_sds(cmd, end);
	cmd = redis_param_str(cmd, "COUNT", sizeof("COUNT")-1);
	cmd = redis_param_sds(cmd, count);
	redis_submit(redis, XRANGE, key, cmd);
	sp->count++;
    }
    sdsfree(count);
    sdsfree(start);
    sdsfree(end);

    return sts;
}

static int
series_resolve_time(SOLVER *sp, series_set_t *result, void *arg)
{
    unsigned char	*series = result->series;
    redisReply		*reply;
    sds			msg, sha;
    int			i, rc, sts = 0;

    if (result->nseries == 0)
	return sts;

    sha = sdsnewlen(NULL, 40);
    for (i = 0; i < result->nseries; i++, series += SHA1SZ) {
	if (redisGetReply(sp->redis->control, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "failed series %s XRANGE command",
				pmwebapi_hash_str(series));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    solverfmt(msg, "expected array from %s XSTREAM values (type=%s)",
			pmwebapi_hash_str(series), redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    sha = sdscpylen(sha, pmwebapi_hash_str(series), 40);
	    if ((rc = series_values_reply(sp, sha,
				reply->elements, reply->element, arg)) < 0)
		sts = rc;
	}
	freeReplyObject(reply);
    }
    sdsfree(sha);

    return sts;
}

void
new_solver_replies(SOLVER *sp)
{
    sds			msg;

    if ((sp->replies = calloc(sp->count, sizeof(redisReply *))) == NULL) {
	solverfmt(msg, "out of memory (%s, %" FMT_INT64 " bytes)", "solver replies",
			(__int64_t)sp->count * sizeof(redisReply *));
	solvermsg(sp, PMLOG_REQUEST, msg);
    }
    sp->index = 0;
}

static void
free_solver_replies(SOLVER *sp)
{
#if 0	/* TODO */
    for (sp->index = 0; sp->index < sp->count; sp->index++)
	freeReplyObject(&sp->replies[sp->index]);
    free(sp->replies);
#endif
    sp->index = sp->count = 0;
}

static int
series_report_set(SOLVER *sp, series_set_t *set)
{
    unsigned char	*series = set->series;
    sds			sid;
    int			i;

    if (set->nseries)
	sid = sdsnewlen(NULL, 40+1);
    for (i = 0; i < set->nseries; series += SHA1SZ, i++) {
	sid = sdscpylen(sid, pmwebapi_hash_str(series), 40);
	solvermatch(sp, sid);
    }
    return 0;
}

static int
series_time_window(timing_t *tp)
{
    if (tp->ranges || tp->starts || tp->ends || tp->counts)
	return 1;
    return 0;
}

int
series_solve(settings_t *settings,
	node_t *root, timing_t *timing, pmflags flags, void *arg)
{
    SOLVER	solver = { .settings = settings, .arg = arg };
    SOLVER	*sp = &solver;

    solver.redis = redis_init(sp->settings->hostspec);

    /* Resolve label key names (via their map keys) */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_maps\n");
    series_prepare_maps(sp, root, 0);
    new_solver_replies(sp);
    series_resolve_maps(sp, root, 0);
    free_solver_replies(sp);

    /* Resolve sets of series identifiers for leaf nodes */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_eval\n");
    series_prepare_eval(sp, root, 0);
    new_solver_replies(sp);
    series_resolve_eval(sp, root, 0);
    free_solver_replies(sp);

    /* Perform final matching (set of) series solving */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_expr\n");
    series_prepare_expr(sp, root, 0);
    new_solver_replies(sp);
    series_resolve_expr(sp, root, 0);
    free_solver_replies(sp);

    /* Report the matching series ids, unless time window given */
    if ((flags & PMFLAG_METADATA) || !series_time_window(timing)) {
	series_report_set(sp, &root->result);
	return 0;
    }

    /* Extract values within the given time window */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_time\n");
    series_prepare_time(sp, timing, &root->result);
    new_solver_replies(sp);
    series_resolve_time(sp, &root->result, arg);
    free_solver_replies(sp);

    return 0;
}

/* build a reverse hash mapping */
static int
reverse_map(pmSeriesSettings *settings, int nkeys,
	redisReply **elements, redisMap *map, void *arg)
{
    redisReply		*name, *key;
    sds			msg, val;
    int			i;

    for (i = 0; i < nkeys; i += 2) {
	name = elements[i];
	key = elements[i+1];
	if (name->type == REDIS_REPLY_STRING) {
	    if (key->type == REDIS_REPLY_STRING) {
		val = sdsnewlen(name->str, name->len);
		redisRMapInsert(map, key->str, val);
		sdsfree(val);
	    } else {
		queryfmt(msg, "expected string key for hashmap (type=%s)",
			redis_reply(key->type));
		queryinfo(settings, PMLOG_RESPONSE, msg, arg);
		return -EINVAL;
	    }
	} else {
	    queryfmt(msg, "expected string name for hashmap (type=%s)",
		    redis_reply(name->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    return -EINVAL;
	}
    }
    return 0;
}

/*
 * Produce the list of mapped names (requires reverse mapping from IDs)
 */
static int
series_map_reply(pmSeriesSettings *settings, sds series,
	pmSeriesStringCallBack series_string_callback, void *arg,
	int nelements, redisReply **elements, redisMap *map)
{
    redisMapEntry	*entry;
    redisReply		*id;
    sds			msg;
    int			i, sts = 0;

    for (i = 0; i < nelements; i++) {
	id = elements[i];
	if (id->type == REDIS_REPLY_STRING) {
	    if ((entry = redisRMapLookup(map, id->str)) != NULL)
		series_string_callback(series, redisRMapValue(entry), arg);
	    else {
		queryfmt(msg, "%s - timeseries string map", series);
		queryinfo(settings, PMLOG_CORRUPT, msg, arg);
		sts = -EINVAL;
	    }
	} else {
	    queryfmt(msg, "expected string in %s set (type=%s)",
			series, redis_reply(id->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	}
    }

    return sts;
}

static int
series_map_keys(pmSeriesSettings *settings, redisSlots *redis,
		pmSeriesStringCallBack series_string_callback, void *arg,
		const char *name)
{
    redisReply		*reply, *rp;
    sds			cmd, key, val, msg;
    int			i;

    key = sdscatfmt(sdsempty(), "pcp:map:%s", name);
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HKEYS, HKEYS_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HKEYS, key, cmd);

    /* TODO: async response handling */

    if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	queryfmt(msg, "failed %s string mapping", HKEYS);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from string map %s (reply=%s)",
		HKEYS, redis_reply(reply->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	freeReplyObject(reply);
	return -EPROTO;
    }

    val = sdsempty();

    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
	    val = sdscpylen(val, rp->str, rp->len);
	    series_string_callback(NULL, val, arg);
	} else {
	    queryfmt(msg, "bad response for string map %s (%s)",
			HKEYS, redis_reply(rp->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sdsfree(val);
	    return -EINVAL;
	}
	freeReplyObject(rp);
    }

    sdsfree(val);
    return 0;
}

void
series_label_value_prepare(redisSlots *redis, const char *mapid, void *arg)
{
    sds			cmd, key;

    key = sdscatfmt(sdsempty(), "pcp:map:label.%s.value", mapid);
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);
}

static int
series_label_value_execute(pmSeriesSettings *settings,
	redisSlots *redis, const char *mapid, redisMap *map, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	queryfmt(msg, "failed %s %s.%s.value", HGETALL, "pcp:map:label", mapid);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from %s %s.%s.value (type=%s)",
		HGETALL, "pcp:map:label", mapid, redis_reply(reply->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	/* unpack - produce reverse map of ids-to-names for each entry */
	sts = reverse_map(settings, reply->elements, reply->element, map, arg);
    }
    freeReplyObject(reply);
    return sts;
}

static int
series_label_reply(pmSeriesSettings *settings, redisSlots *redis, sds series,
	redisMap *map, int nelements, redisReply **elements, void *arg)
{
    pmSeriesLabel	label;
    redisReply		*r;
    redisMap		*vmap;
    redisMapEntry 	*entry;
    sds			msg, name, value, vkey;
    char		*nmapid, *vmapid;
    int			i, rc, index, sts = 0;

    if (nelements % 2) {
	queryfmt(msg, "expected even number of results from %s (not %d)",
		    HGETALL, nelements);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }
    for (i = 0; i < nelements; i++) {
	r = elements[i];
	if (r->type != REDIS_REPLY_STRING) {
	    queryfmt(msg, "expected only string results from %s (type=%s)",
			HGETALL, redis_reply(r->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    return -EPROTO;
	}
    }

    nelements /= 2;
    for (i = 0; i < nelements; i++) {
	index = i * 2;
	nmapid = elements[index]->str;
	vmapid = elements[index+1]->str;

	if ((entry = redisRMapLookup(map, nmapid)) != NULL) {
	    vkey = sdscatfmt(sdsempty(), "label.%s.value", nmapid);
	    vmap = redisRMapCreate(vkey);
	    name = redisRMapValue(entry);
	    querylabel(settings, series, name, arg);

	    /* perform the label value reverse lookup */
	    series_label_value_prepare(redis, vmapid, arg);

	    /* TODO: async response handling */
	    rc = series_label_value_execute(settings, redis, vmapid, vmap, arg);
	    if (rc < 0) {
		redisMapRelease(vmap);
		sdsfree(vkey);
		sts = rc;
		continue;
	    }

	    if ((entry = redisRMapLookup(vmap, vmapid)) == NULL)
		value = sdsempty();
	    else
		value = redisRMapValue(entry);

	    label.name = name;
	    label.value = value;
	    querylabelmap(settings, series, &label, arg);
	    if (entry == NULL)
		sdsfree(value);
	    redisMapRelease(vmap);
	    sdsfree(vkey);
	} else {
	    queryfmt(msg, "%s - timeseries name map", series);
	    queryinfo(settings, PMLOG_CORRUPT, msg, arg);
	    sts = -EINVAL;
	}
    }

    return sts;
}

void
pmSeriesLabels(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    redisSlots		*redis = redis_init(settings->hostspec);
    redisReply		*reply, *rp;
    const char		*name = redisMapName(labelsrmap);
    redisMap		*map = labelsrmap;
    sds			msg, key, cmd;
    int			i, rc, sts = 0;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
            series_map_keys(settings, redis, settings->on_label, arg, name);
	goto done;
    }

    /* prepare command batch */
    key = sdsnew("pcp:map:label.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);

    /* TODO: async response handling */

    if (redisGetReply(redis->control, (void **)&rp) != REDIS_OK) {
	queryfmt(msg, "failed %s %s", HGETALL, "pcp:map:label.name");
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else if (rp->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from %s %s HGETALL (type=%s)",
		HGETALL, "pcp:map:label.name", redis_reply(rp->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	/* unpack - produce reverse hash map of ids -> names for each label */
	sts = reverse_map(settings, rp->elements, rp->element, map, arg);
    }
    if (sts < 0)
	goto done;

    /* unpack - iterate over series and extract labels names and values */
    for (i = 0; i < nseries; i++) {
	key = sdscatfmt(sdsempty(), "pcp:labels:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, HGETALL, key, cmd);

	/* TODO: async response handling */

	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s %s:%s",
			HGETALL, "pcp:labels:series", series[i]);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from %s %s:%s (type=%s)",
			HGETALL, "pcp:labels:series", series[i],
			redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_label_reply(settings, redis, series[i], map,
				reply->elements, reply->element, arg)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }
    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}

static int
series_metric_name_prepare(pmSeriesSettings *settings,
	redisSlots *redis, void *arg)
{
    sds			cmd, key;

    key = sdsnew("pcp:map:metric.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);
    return 0;
}

static int
series_metric_name_execute(pmSeriesSettings *settings,
	redisSlots *redis, redisReply **rp, redisMap *map, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	queryfmt(msg, "failed %s %s", HGETALL, "pcp:map:metric.name");
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from %s %s (type=%s)",
		HGETALL, "pcp:map:metric.name", redis_reply(reply->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	*rp = reply;
	/* unpack - produce reverse map of ids -> names for each metric */
	sts = reverse_map(settings, reply->elements, reply->element, map, arg);
    }
    return sts;
}

void
pmSeriesMetrics(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    redisSlots		*redis = redis_init(settings->hostspec);
    redisReply		*reply, *rp;
    const char		*name = redisMapName(namesrmap);
    redisMap		*map = namesrmap;
    sds			cmd, key, msg;
    int			i, rc, sts;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
		series_map_keys(settings, redis, settings->on_metric, arg, name);
	goto done;
    }

    if ((sts = series_metric_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_metric_name_execute(settings, redis, &rp, map, arg)) < 0)
	goto done;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);
    }

    /* TODO: async response handling */

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "%s series %s query failed", SMEMBERS, series[i]);
	    queryinfo(settings, PMLOG_REQUEST, msg, arg);
	    sts = -EAGAIN;
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from %s %s (type=%s)",
			SMEMBERS, series[i], redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_map_reply(settings, series[i],
				settings->on_metric, arg,
				reply->elements, reply->element, map)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }

    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}

static int
series_desc_reply(pmSeriesSettings *settings, pmSID series,
	int nelements, redisReply **elements, pmSeriesDesc *desc, void *arg)
{
    sds			msg;

    if (nelements < 6) {
	queryfmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }

    /* sanity check - were we given an invalid series identifier? */
    if (elements[0]->type == REDIS_REPLY_NIL) {
	queryfmt(msg, "no descriptor for series identifier %s", series);
	queryinfo(settings, PMLOG_ERROR, msg, arg);
	return -EINVAL;
    }

    if (extract_string(settings, series, elements[0],
			    &desc->indom, "indom", arg) < 0)
	return -EPROTO;
    if (extract_string(settings, series, elements[1],
			    &desc->pmid, "pmid", arg) < 0)
	return -EPROTO;
    if (extract_string(settings, series, elements[2],
			    &desc->semantics, "semantics", arg) < 0)
	return -EPROTO;
    if (extract_sha1(settings, series, elements[3],
			    &desc->source, "source", arg) < 0)
	return -EPROTO;
    if (extract_string(settings, series, elements[4],
			    &desc->type, "type", arg) < 0)
	return -EPROTO;
    if (extract_string(settings, series, elements[5],
			    &desc->units, "units", arg) < 0)
	return -EPROTO;

    return 0;
}

void
pmSeriesDescs(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    pmSeriesDesc	desc;
    redisSlots		*redis = redis_init(settings->hostspec);
    redisReply		*reply;
    sds			cmd, key, msg;
    int			n, rc, sts = 0;

    if (nseries <= 0) {
	sts = -EINVAL;
	goto done;
    }

    /* prepare command series */

    for (n = 0; n < nseries; n++) {
	key = sdscatfmt(sdsempty(), "pcp:desc:series:%S", series[n]);
	cmd = redis_command(8);
	cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, "indom", sizeof("indom")-1);
	cmd = redis_param_str(cmd, "pmid", sizeof("pmid")-1);
	cmd = redis_param_str(cmd, "semantics", sizeof("semantics")-1);
	cmd = redis_param_str(cmd, "source", sizeof("source")-1);
	cmd = redis_param_str(cmd, "type", sizeof("type")-1);
	cmd = redis_param_str(cmd, "units", sizeof("units")-1);
	redis_submit(redis, HMGET, key, cmd);
    }

    /* TODO: async response handling */

    desc.indom = sdsnewlen(NULL, 16);
    desc.pmid = sdsnewlen(NULL, 16);
    desc.semantics = sdsnewlen(NULL, 16);
    desc.source = sdsnewlen(NULL, 40);
    desc.type = sdsnewlen(NULL, 16);
    desc.units = sdsnewlen(NULL, 16);

    /* unpack - iterate over series and extract descriptor for each */
    for (n = 0; n < nseries; n++) {
	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s on series %s", HMGET, series[n]);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array type from series %s %s (type=%s)",
			series[n], HMGET, redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_desc_reply(settings, series[n],
			reply->elements, reply->element, &desc, arg)) < 0) {
	    sts = rc;
	} else if ((rc = querydesc(settings, series[n], &desc, arg)) < 0) {
	    sts = rc;
	}

	freeReplyObject(reply);
    }

    sdsfree(desc.indom);
    sdsfree(desc.pmid);
    sdsfree(desc.semantics);
    sdsfree(desc.source);
    sdsfree(desc.type);
    sdsfree(desc.units);

done:
    querydone(settings, sts, arg);
}

static int
series_inst_name_prepare(pmSeriesSettings *settings,
	redisSlots *redis, void *arg)
{
    sds			cmd, key;

    key = sdsnew("pcp:map:inst.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);
    return 0;
}

static int
series_inst_name_execute(pmSeriesSettings *settings,
	redisSlots *redis, redisReply **rp, redisMap *map, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	queryfmt(msg, "failed %s %s", HGETALL, "pcp:map:inst.name");
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from %s %s (type=%s)",
		HGETALL, "pcp:map:inst.name", redis_reply(reply->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	*rp = reply;
	/* unpack - produce reverse map of ids-to-names for each instance */
	sts = reverse_map(settings, reply->elements, reply->element, map, arg);
    }
    return sts;
}

static int
series_inst_reply(pmSeriesSettings *settings, pmSID series,
		redisMap *map, pmSID instsid, pmSeriesInst *inst,
		int nelements, redisReply **elements, void *arg)
{
    sds			msg;

    if (nelements < 3) {
	queryfmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }

    if (extract_string(settings, series, elements[0],
			    &inst->instid, "inst", arg) < 0)
	return -EPROTO;
    if (extract_mapping(settings, series, map, elements[1],
			    &inst->name, "name", arg) < 0)
	return -EPROTO;
    if (extract_sha1(settings, series, elements[2],
			    &inst->series, "series", arg) < 0)
	return -EPROTO;

    /* verify that this instance series matches the given series */
    if (sdscmp(series, inst->series) != 0) {
	queryfmt(msg, "mismatched series for instance %s of series %s (got %s)",
			instsid, series, inst->series);
	queryinfo(settings, PMLOG_CORRUPT, msg, arg);
	return -EINVAL;
    }
    /* return instance series identifiers, not the metric series */
    inst->series = sdscpy(inst->series, instsid);
    return 0;
}

static int
series_instances_reply(pmSeriesSettings *settings, redisSlots *redis,
	pmSID series, int nelements, redisReply **elements, pmSeriesInst *inst,
	redisMap *map, void *arg)
{
    redisReply		*reply;
    pmSID		sid = sdsempty();
    sds			key, cmd, msg;
    int			i, rc, sts = 0;

    /*
     * Iterate over the instance series identifiers, looking up
     * the instance hash contents for each.
     */
    for (i = 0; i < nelements; i++) {
	sts = extract_sha1(settings, series, elements[i], &sid, "series", arg);
	if (sts < 0) {
	    sdsfree(sid);
	    return -EPROTO;
	}

	key = sdscatfmt(sdsempty(), "pcp:inst:series:%S", sid);
	cmd = redis_command(5);
	cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
	cmd = redis_param_str(cmd, "name", sizeof("name")-1);
	cmd = redis_param_str(cmd, "series", sizeof("series")-1);
	redis_submit(redis, HMGET, key, cmd);
    }
    sdsfree(sid);

    /* TODO: async response handling */

    sid = sdsempty();
    for (i = 0; i < nelements; i++) {
	extract_sha1(settings, series, elements[i], &sid, "series", arg);
	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s %s:%s",
			HMGET, "pcp:inst:series", series);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from series %s %s (type=%s)",
			HMGET, series, redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_inst_reply(settings, series, map, sid, inst,
				reply->elements, reply->element, arg)) < 0) {
	    sts = rc;
	} else if ((rc = queryinst(settings, series, inst, arg)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }
    sdsfree(sid);
    return sts;
}

void
pmSeriesInstances(pmSeriesSettings *settings, int nseries, pmSID *series, void *arg)
{
    pmSeriesInst	inst;
    redisSlots		*redis = redis_init(settings->hostspec);
    redisReply		*reply, *rp;
    const char		*name = redisMapName(instrmap);
    redisMap		*map = instrmap;
    sds			cmd, key, msg;
    int			i, rc, sts = 0;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
		series_map_keys(settings, redis, settings->on_instance, arg, name);
	goto done;
    }

    if ((sts = series_inst_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_inst_name_execute(settings, redis, &rp, map, arg)) < 0)
	goto done;

    inst.instid = sdsnewlen(NULL, 16);
    inst.name = sdsnewlen(NULL, 16);
    inst.series = sdsnewlen(NULL, 40);

    for (i = 0; i < nseries; i++) {
	/* prepare command series */
	key = sdscatfmt(sdsempty(), "pcp:instances:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);

	/* TODO: async response handling */

	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s %s:%s",
			SMEMBERS, "pcp:instances:series", series[i]);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from series %s %s (type=%s)",
			SMEMBERS, series[i], redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_instances_reply(settings, redis, series[i],
			reply->elements, reply->element, &inst, map, arg)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }

    freeReplyObject(rp);

    sdsfree(inst.instid);
    sdsfree(inst.name);
    sdsfree(inst.series);

done:
    querydone(settings, sts, arg);
}


static int
series_context_name_prepare(pmSeriesSettings *settings,
	redisSlots *slots, void *arg)
{
    sds			cmd, key;

    key = sdscatfmt(sdsempty(), "pcp:map:context.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(slots, HGETALL, key, cmd);
    return 0;
}

static int
series_context_name_execute(pmSeriesSettings *settings,
	redisSlots *slots, redisReply **rp, redisMap *map, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    /* TODO: make this into an async callback and use correct context */
    if (redisGetReply(slots->control, (void **)&reply) != REDIS_OK) {
	queryfmt(msg, "failed %s %s", HGETALL, "pcp:map:context.name");
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	queryfmt(msg, "expected array from %s %s (type=%s)",
		HGETALL, "pcp:map:context.name", redis_reply(reply->type));
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	*rp = reply;
	/* unpack - produce reverse map of ids-to-names for each context */
	sts = reverse_map(settings, reply->elements, reply->element, map, arg);
    }
    return sts;
}

void
pmSeriesSources(pmSeriesSettings *settings, int nsources, pmSID *sources, void *arg)
{
    redisSlots		*redis = redis_init(settings->hostspec);
    redisReply		*reply, *rp;
    const char		*name = redisMapName(contextrmap);
    redisMap		*map = contextrmap;
    sds			cmd, key, msg;
    int			i, rc, sts;

    if (nsources <= 0) {
	sts = (nsources < 0) ? -EINVAL :
            series_map_keys(settings, redis, settings->on_context, arg, name);
	goto done;
    }
    if ((sts = series_context_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_context_name_execute(settings, redis, &rp, map, arg)) < 0)
	goto done;

    /* prepare command series */

    for (i = 0; i < nsources; i++) {
	key = sdscatfmt(sdsempty(), "pcp:context.name:source:%s", sources[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);
    }

    /* TODO: async response handling */

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nsources; i++) {
	if (redisGetReply(redis->control, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "%s sources %s query failed", SMEMBERS, sources[i]);
	    queryinfo(settings, PMLOG_REQUEST, msg, arg);
	    sts = -EAGAIN;
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from %s %s (type=%s)",
			SMEMBERS, sources[i], redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_map_reply(settings, sources[i],
				settings->on_context, arg,
				reply->elements, reply->element, map)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }

    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}
