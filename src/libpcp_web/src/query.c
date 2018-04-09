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

#define SHA1SZ	20	/* internal sha1 hash buffer size in bytes */

typedef struct {
    redisContext	*redis;

    settings_t		*settings;
    void		*arg;

    unsigned int	index;
    unsigned int	count;
    redisReply		**replies;
} SOLVER;

typedef __pmHashCtl	reverseMap;
typedef __pmHashNode	*reverseMapNode;

#define queryfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define queryinfo(set, level, msg, arg)	\
	((set)->on_info(level, msg, arg), sdsfree(msg))
#define querydesc(set, sid,n,desc, arg)	(set)->on_desc(sid, n,desc, arg)
#define queryinst(set, sid,n,inst, arg)	(set)->on_inst(sid, n,inst, arg)
#define queryinstname(set, sid,in, arg)	(set)->on_instname(sid, in, arg)
#define querymetric(set, sid,name, arg)	(set)->on_metric(sid, name, arg)
#define querylabel(set, sid,label, arg)	(set)->on_label(sid, label, arg)
#define querysource(set, sid,src, arg)	(set)->on_source(sid, src, arg)
#define querydone(set, sts, arg)	(set)->on_done(sts, arg)

#define solverfmt(msg, fmt, ...)		\
	((msg) = sdscatprintf(sdsempty(), fmt, ##__VA_ARGS__))
#define solvermsg(SP, level, message)		\
	((SP)->settings->on_info((level), (msg), (SP)->arg), sdsfree(msg))
#define solvermatch(SP, sid)			\
	((SP)->settings->on_match((sid), (SP)->arg))
#define solvervalue(SP, sid, n, value)	\
	((SP)->settings->on_value((sid), (n), (value), (SP)->arg))

static const char *
redis_reply(int reply)
{
    switch (reply) {
    case REDIS_REPLY_STRING:
	return "string";
    case REDIS_REPLY_ARRAY:
	return "array";
    case REDIS_REPLY_INTEGER:
	return "integer";
    case REDIS_REPLY_NIL:
	return "nil";
    case REDIS_REPLY_STATUS:
	return "status";
    case REDIS_REPLY_ERROR:
	return "error";
    default:
	break;
    }
    return "unknown";
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
series_source_name(sds key)
{
    size_t	length = sdslen(key);

    if (length >= sizeof("source.") &&
	strncmp(key, "source.", sizeof("source.") - 1) == 0)
	return key + sizeof("source.") - 1;
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
	case N_SOURCE: return "source";
	case N_LABEL: return "label";
	case N_METRIC: return "metric";
	case N_INSTANCE: return "instance";
	default: break;
    }
    return NULL;
}

/*
 * Report a timeseries result - timestamps and (instance) values
 */
static int
series_values_reply(SOLVER *sp, unsigned char *series,
	int nelements, redisReply **elements)
{
#if 0
    redisReply		*nameReply, *valueReply;
    const char		*hash;
    sds			msg, id, value, seriesid, timestamp;
    int			i, sts = 0;

    if (nelements <= 0)
	return nelements;
    hash = pmwebapi_hash_str(series);

    /* expecting timestamp:valueset pairs, then name:value pairs */
    if (nelements % 2) {
	solverfmt(msg, "expected time:value pairs from %s series XRANGE", hash);
	solvermsg(sp, PMLOG_RESPONSE, msg);
	return -EPROTO;
    }

/*--printf("    \"%.*s\" : {\n", pmwebapi_hash_str(series);--*TODO*/

    id = sdsempty();
    value = sdsempty();

    seriesid = sdsnew(hash);
    timestamp = sdsempty();
    /* TODO: must extract the timestamp field and construct a timespec */

    for (i = 0; i < nelements; i += 2) {
	nameReply = elements[i];
	valueReply = elements[i+1];
	if (nameReply->type == REDIS_REPLY_STRING) {
	    if (valueReply->type == REDIS_REPLY_STRING) {
/*--		printf("      \"%.*s\": \"%s\"",
				15, scores, valueReply->str);--*TODO*/
		/* TODO: must map internal id to external inst identifier */
		/* TODO: handle odd cases for instids (indom_null, marks) */
		id = sdscat(id, nameReply->str);
		value = sdscat(value, valueReply->str);
		solvervalue(sp, seriesid, timestamp, id, value);
	    } else {
		solverfmt(msg, "expected string value for series %s (type=%s)",
			seriesid, redis_reply(valueReply->type));
		solvermsg(sp, PMLOG_RESPONSE, msg);
		sts = -EPROTO;
	    }
	} else {
	    solverfmt(msg, "expected string name ID for series %s (type=%s)",
			seriesid, redis_reply(nameReply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	}
/*--	if (i + 2 < nelements)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("    }", stdout);--*TODO*/

    sdsfree(timestamp);
    sdsfree(seriesid);
    sdsfree(value);
    sdsfree(id);

    return sts;
#endif
    return -1;
}

/*
 * Save the series hash identifiers contained in a Redis response.
 * Used at the leaves of the query tree, then merged result sets
 * are propagated upward.
 */
static int
node_series_reply(SOLVER *sp, node_t *np, int nelements, redisReply **elements)
{
    unsigned char	*series;
    redisReply		*reply;
    sds			msg;
    int			i, sts = 0;

    if ((np->nseries = nelements) <= 0)
	return nelements;

    if ((series = (unsigned char *)calloc(nelements, SHA1SZ)) == NULL) {
	solverfmt(msg, "out of memory (%s, %lld bytes)",
			"series reply", (long long)nelements * SHA1SZ);
	solvermsg(sp, PMLOG_REQUEST, msg);
	return -ENOMEM;
    }
    np->series = series;

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
	    free(np->series);
	    np->series = NULL;
	    np->nseries = 0;
	    sts = -EPROTO;
	}
    }
    if (sts < 0)
	return sts;
    return nelements;
}

static void
free_child_series(node_t *left, node_t *right)
{
    if (left->series) {
	free(left->series);
	left->series = NULL;
    }
    if (right->series) {
	free(right->series);
	right->series = NULL;
    }
    right->nseries = left->nseries = 0;
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
node_series_intersect(node_t *np, node_t *left, node_t *right)
{
    unsigned char	*small, *large, *cp, *saved;
    int			i, nsmall, nlarge, total;

    if (pmDebugOptions.series)
	printf("Intersect left(%d) and right(%d) series\n",
		left->nseries, right->nseries);

    if (left->nseries >= right->nseries) {
	small = right->series;	nsmall = right->nseries;
	large = left->series;	nlarge = left->nseries;
	right->series = NULL;	/* do-not-free marker for small set */
    } else {
	small = left->series;	nsmall = left->nseries;
	large = right->series;	nlarge = right->nseries;
	left->series = NULL;	/* do-not-free marker for small set */
    }

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

    np->nseries = total;
    np->series = small;

    if (pmDebugOptions.series) {
	printf("Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp++, i++)
	    printf("    %s\n", pmwebapi_hash_str(cp));
    }

    free_child_series(left, right);
    return total;
}

/*
 * Form the resulting set from union of two child sets.
 * The larger set is realloc-ated to form the result, if we
 * need to (i.e. if there are entries in the smaller set not
 * in the larger).
 *
 * Iterates over the smaller set doing a binary search of
 * each series identifier, and tracks which ones in the small
 * need to be added to the large set.  Then, at end, add more
 * space to the larger set if needed and append.
 */
static int
node_series_union(node_t *np, node_t *left, node_t *right)
{
    unsigned char	*small, *large, *cp, *saved;
    int			i, nsmall, nlarge, total, need;

    if (pmDebugOptions.series)
	printf("Union of left(%d) and right(%d) series\n",
		left->nseries, right->nseries);

    if (left->nseries >= right->nseries) {
	small = right->series;	nsmall = right->nseries;
	large = left->series;	nlarge = left->nseries;
	left->series = NULL;	/* do-not-free marker for large set */
    } else {
	small = left->series;	nsmall = left->nseries;
	large = right->series;	nlarge = right->nseries;
	right->series = NULL;	/* do-not-free marker for large set */
    }

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

    np->nseries = total;
    np->series = large;

    if (pmDebugOptions.series) {
	printf("Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += SHA1SZ, i++)
	    printf("    %s\n", pmwebapi_hash_str(cp));
    }

    free_child_series(left, right);
    return total;
}

/*
 * Map human names to internal redis identifiers.
 */
static int
series_prepare_maps(SOLVER *sp, node_t *np, int level)
{
    redisContext	*redis = sp->redis;
    const char		*name;
    sds			cmd;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_maps(sp, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_STRING:
	// may need string quoting with regex escapes? (for MATCH)
	// - or should we rewrite this earlier perhaps?  (see also KEYS docs)
	break;

    case N_NAME:
	// setup any label name map identifiers needed by direct children
	if ((name = series_instance_name(np->value)) != NULL) {
	    np->subtype = N_INSTANCE;
	    np->key = sdsnew("pcp:map:inst.name");
	} else if ((name = series_metric_name(np->value)) != NULL) {
	    np->subtype = N_METRIC;
	    np->key = sdsnew("pcp:map:metric.name");
	} else {
	    np->subtype = N_LABEL;
	    np->key = sdsnew("pcp:map:label.name");
	    if ((name = series_label_name(np->value)) == NULL)
		name = np->value;
	    cmd = redis_command(3);
	    cmd = redis_param_str(cmd, HGET, HGET_LEN);
	    cmd = redis_param_sds(cmd, np->key);
	    cmd = redis_param_str(cmd, name, strlen(name));
	    redis_submit(redis, HGET, np->key, cmd);
	    np->key = NULL;	/* freed above */
	    sp->count++;
	}
	break;

    case N_LT:  case N_LEQ: case N_EQ:  case N_GEQ: case N_GT:  case N_NEQ:
    case N_AND: case N_OR:  case N_RNE: case N_REQ: case N_NEG:
    default:
	break;
    }
    if (sts < 0)
	return sts;

    return series_prepare_maps(sp, np->right, level+1);
}

/*
 * Find the sets of series corresponding to leaf search parameters.
 */
static int
series_resolve_maps(SOLVER *sp, node_t *np, int level)
{
    redisReply		*reply;
    sds			msg;
    int			sts = 0;

    if (np == NULL)
	return 0;

    series_resolve_maps(sp, np->left, level+1);

    switch (np->type) {
    case N_NAME:
	/* TODO: need to handle JSONB label name nesting. */

	/* setup any label name map identifiers needed */
	if (np->subtype == N_LABEL) {
	    if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
		solverfmt(msg, "no %s named \"%s\" found (error=%d)",
			node_subtype(np), np->value, sp->redis->err);
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
		np->key = sdsnew("pcp:map:");
		np->key = sdscatprintf(np->key, "%s.%s.value",
				node_subtype(np), reply->str);
	    }
	}
	break;

    case N_LT:  case N_LEQ: case N_EQ:  case N_GEQ: case N_GT:  case N_NEQ:
    case N_AND: case N_OR:  case N_RNE: case N_REQ: case N_NEG:
    default:
	break;
    }
    if (sts < 0)
	return sts;

    return series_resolve_maps(sp, np->right, level+1);
}

/*
 * Prepare evaluation of leaf nodes.
 */
static int
series_prepare_eval(SOLVER *sp, node_t *np, int level)
{
    redisContext	*redis = sp->redis;
    sds 		key, val, cmd;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_eval(sp, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	val = np->right->value;
	key = sdsdup(np->left->key);
	cmd = redis_command(3);
	cmd = redis_param_str(cmd, HGET, HGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, val);
	redis_submit(redis, HGET, key, cmd);
	sp->count++;
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

    return series_prepare_eval(sp, np->right, level+1);
}

static int
series_resolve_eval(SOLVER *sp, node_t *np, int level)
{
    redisContext	*redis = sp->redis;
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
    case N_EQ:	/* direct hash lookup */
	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;

	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "map table %s key %s not found",
		    left->key, np->right->value);
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_STRING) {
	    solverfmt(msg, "expected string for %s key \"%s\" (type=%s)",
		    node_subtype(left), left->key, redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    sp->replies[sp->index++] = reply;
	    sdsfree(np->key);
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

/*
 * Prepare evaluation of internal nodes.
 */
static int
series_prepare_expr(SOLVER *sp, node_t *np, int level)
{
    redisContext	*redis = sp->redis;
    sds			cmd, key;
    int			sts;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((series_prepare_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	key = sdsdup(np->key);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);
	sp->count++;
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }
    return sts;
}

static int
series_resolve_expr(SOLVER *sp, node_t *np, int level)
{
    redisContext	*redis = sp->redis;
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (np == NULL)
	return 0;

    /* evaluate leaves first then interior nodes */
    if ((sts = series_resolve_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((sts = series_resolve_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "map table %s key %s not found",
			np->left->key, np->right->value);
	    solvermsg(sp, PMLOG_CORRUPTED, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    solverfmt(msg, "expected array for %s set \"%s\" (type=%s)",
			node_subtype(np->left), np->right->value,
			redis_reply(reply->type));
	    solvermsg(sp, PMLOG_CORRUPTED, msg);
	    sts = -EPROTO;
	} else {
	    sp->replies[sp->index++] = reply;
	    if (pmDebugOptions.series)
		printf("%s %s\n", node_subtype(np->left), np->key);
	    sts = node_series_reply(sp, np, reply->elements, reply->element);
	}
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
	/* TODO */
	break;

    case N_AND:
	node_series_intersect(np, np->left, np->right);
	/* TODO: error handling */
	break;

    case N_OR:
	node_series_union(np, np->left, np->right);
	/* TODO: error handling */
	break;

    default:
	break;
    }
    return sts;
}

#define DEFAULT_VALUE_COUNT 10

static int
series_prepare_time(SOLVER *sp, timing_t *tp, int nseries, unsigned char *series)
{
    redisContext	*redis = sp->redis;
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
    for (i = 0; i < nseries; i++, series += SHA1SZ) {
	key = sdscatfmt(sdsempty(), "pcp:values:series:%s", pmwebapi_hash_str(series));

	/* XREAD key t1 t2 [COUNT count] */
	cmd = redis_command(6);
	cmd = redis_param_str(cmd, XRANGE, XRANGE_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_sds(cmd, end);
	cmd = redis_param_sds(cmd, start);
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

#if 0	/* TODO */
static const char *
series_result(int nseries)
{
    if (nseries == 0)
	return "empty";
    if (nseries == 1)
	return "single";
    return "vector";
}
#endif

static int
series_resolve_time(SOLVER *sp, int nseries, unsigned char *series, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			i, rc, sts = 0;

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    for (i = 0; i < nseries; i++, series += SHA1SZ) {
	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    solverfmt(msg, "failed series %s XRANGE command",
			    pmwebapi_hash_str(series));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    solverfmt(msg, "expected array from %s XSTREAM values (type=%s)",
			pmwebapi_hash_str(series), redis_reply(reply->type));
	    solvermsg(sp, PMLOG_RESPONSE, msg);
	    sts = -EPROTO;
	} else if ((rc = series_values_reply(sp, series,
				reply->elements, reply->element)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);

/*--	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    return sts;
}

void
new_solver_replies(SOLVER *sp)
{
    sds			msg;

    if ((sp->replies = calloc(sp->count, sizeof(redisReply *))) == NULL) {
	solverfmt(msg, "out of memory (%s, %lld bytes)", "solver replies",
			(long long)sp->count * sizeof(redisReply *));
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
series_report_set(SOLVER *sp, int nseries, unsigned char *series)
{
    sds			sid;
    int			i;

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": [\n");--*TODO*/

    if (nseries)
	sid = sdsnewlen("", 40+1);
    for (i = 0; i < nseries; series += SHA1SZ, i++) {
	sid = sdscpy(sid, pmwebapi_hash_str(series));
	solvermatch(sp, sid);

/*--	printf("    \"%s\"", sid);
	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }

/*--fputs("  ]\n}\n", stdout);--*TODO*/
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

    solver.redis = redis_init();

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
	series_report_set(sp, root->nseries, root->series);
	return 0;
    }

    /* Extract values within the given time window */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_time\n");
    series_prepare_time(sp, timing, root->nseries, root->series);
    new_solver_replies(sp);
    series_resolve_time(sp, root->nseries, root->series, arg);
    free_solver_replies(sp);

    return 0;
}

/* build a reverse hash mapping */
static int
reverse_map(pmSeriesSettings *settings, int nkeys,
	redisReply **elements, reverseMap *map, void *arg)
{
    redisReply		*name, *key;
    sds			msg;
    int			i;

    /* TODO:leak - rework this map/hash management - see also schema.c */
    __pmHashInit(map);

    for (i = 0; i < nkeys; i += 2) {
	name = elements[i];
	key = elements[i+1];
	if (name->type == REDIS_REPLY_STRING) {
	    if (key->type == REDIS_REPLY_STRING) {
		/* TODO:leak - rework reply string management and hash */
		__pmHashAdd(atoi(key->str), sdsnewlen(name->str, name->len), map);
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
	int nelements, redisReply **elements, reverseMap *map)
{
    redisReply		*id;
    reverseMapNode	node;
    sds			msg;
    int			i, sts = 0;

/*--printf("    \"%s\" : [", series);--*TODO*/

    for (i = 0; i < nelements; i++) {
	id = elements[i];
	if (id->type == REDIS_REPLY_STRING) {
	    if ((node = __pmHashSearch(atoi(id->str), map)) == NULL) {
		queryfmt(msg, "%s - timeseries string map", series);
		queryinfo(settings, PMLOG_CORRUPTED, msg, arg);
		sts = -EINVAL;
	    } else {
		series_string_callback(series, node->data, arg);
	    }
	} else {
	    queryfmt(msg, "expected string in %s set (type=%s)",
			series, redis_reply(id->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	}
/*--	if (i < nelements - 1)
	    fputc(',', stdout);--*TODO*/
    }
/*--fputs(" ]", stdout);--*TODO*/

    return sts;
}

static int
series_map_keys(pmSeriesSettings *settings, redisContext *redis,
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

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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

/*--printf("[");--*TODO*/
    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
/*--	    printf(" \"%s\"", rp->str);--*TODO*/
	    val = sdscpylen(val, rp->str, rp->len);
	    series_string_callback(NULL, val, arg);
	} else {
	    queryfmt(msg, "bad response for string map %s (%s)",
			HKEYS, redis_reply(rp->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    return -EINVAL;
	}
/*--	if (i + 1 < reply->elements)
	    fputc(',', stdout);--*TODO*/
	freeReplyObject(rp);
    }
/*--printf(" ]\n");--*TODO*/
//  freeReplyObject(reply);

    sdsfree(val);
    return 0;
}

void
pmSeriesLabels(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    sds			msg, key, cmd;
    int			i, rc, sts = 0;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
		series_map_keys(settings, redis,
				settings->on_label, arg, "label.name");
	goto done;
    }

    /* prepare command batch */
    key = sdsnew("pcp:map:label.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);
    for (i = 0; i < nseries; i++) {
	key = sdscatfmt(sdsempty(), "pcp:label.name:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);
    }

    /* TODO: async response handling */

    if (redisGetReply(redis, (void **)&rp) != REDIS_OK) {
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
	sts = reverse_map(settings, rp->elements, rp->element, &map, arg);
    }
    if (sts < 0)
	goto done;

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));--*TODO*/
/*--printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract label names for each set */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s %s:%s",
			SMEMBERS, "pcp:label.name:series", series[i]);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from %s %s (type=%s)",
			SMEMBERS, series[i], redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_map_reply(settings, series[i],
				settings->on_label, arg,
				reply->elements, reply->element, &map)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);

/*--	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}

static int
series_metric_name_prepare(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
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
	redisContext *redis, redisReply **rp, reverseMap *mp, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
	/* unpack - produce reverse map of ids -> names for each label */
	sts = reverse_map(settings, reply->elements, reply->element, mp, arg);
    }
    return sts;
}

void
pmSeriesMetrics(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    sds			cmd, key, msg;
    int			i, rc, sts;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
		series_map_keys(settings, redis,
				settings->on_metric, arg, "metric.name");
	goto done;
    }

    if ((sts = series_metric_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_metric_name_execute(settings, redis, &rp, &map, arg)) < 0)
	goto done;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	key = sdscatfmt(sdsempty(), "pcp:metric.name:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);
    }
    if (sts < 0)
	goto done;

    /* TODO: async response handling */

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));--*TODO*/
/*--printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
				reply->elements, reply->element, &map)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);

/*--	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}

static int
extract_string(pmSeriesSettings *settings, pmSeriesID series,
	redisReply *element, sds *string, const char *message, void *arg)
{
    sds			msg;

    if (element->type == REDIS_REPLY_STRING) {
	*string = sdscpylen(*string, element->str, element->len);
	return 0;
    }
    queryfmt(msg, "expected string result for %s of series %s",
			message, series);
    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
    return -EINVAL;
}

static int
extract_mapping(pmSeriesSettings *settings, pmSeriesID series, reverseMap *map,
	redisReply *element, sds *string, const char *message, void *arg)
{
    reverseMapNode	node;
    sds			msg;

    if (element->type == REDIS_REPLY_STRING) {
	if ((node = __pmHashSearch(atoi(element->str), map)) != NULL) {
	    *string = node->data;
	    return 0;
	}
	queryfmt(msg, "bad mapping for %s of series %s", message, series);
	queryinfo(settings, PMLOG_CORRUPTED, msg, arg);
	return -EINVAL;
    }
    queryfmt(msg, "expected string for %s of series %s", message, series);
    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
    return -EPROTO;
}

static int
extract_sha1(pmSeriesSettings *settings, pmSeriesID series,
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
			message, series, element->len);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EINVAL;
    }
    hash = pmwebapi_hash_str((unsigned char *)element->str);
    *sha = sdscpylen(*sha, hash, 40);
    return 0;
}

static int
series_desc_reply(pmSeriesSettings *settings, pmSeriesID series,
	int nelements, redisReply **elements, sds *desc, void *arg)
{
    sds			msg;

    if (nelements < PMDESC_MAXFIELD) {
	queryfmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }

/*--printf("    \"%s\" : {\n", series);--TODO*/

    if (extract_string(settings, series, elements[PMDESC_INDOM],
			    &desc[PMDESC_INDOM], "indom", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"indom", desc[PMDESC_INDOM]);--*TODO*/

    if (extract_string(settings, series, elements[PMDESC_PMID],
			    &desc[PMDESC_PMID], "pmid", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"pmid", desc[PMDESC_PMID]);--*TODO*/

    if (extract_string(settings, series, elements[PMDESC_SEMANTICS],
			    &desc[PMDESC_SEMANTICS], "semantics", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"semantics", desc[PMDESC_SEMANTICS]);--*TODO*/

    if (extract_sha1(settings, series, elements[PMDESC_SOURCE],
			    &desc[PMDESC_SOURCE], "source", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"source", &desc[PMDESC_SOURCE]);--*TODO*/

    if (extract_string(settings, series, elements[PMDESC_TYPE],
			    &desc[PMDESC_TYPE], "type", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"type", desc[PMDESC_TYPE]);--*TODO*/

    if (extract_string(settings, series, elements[PMDESC_UNITS],
			    &desc[PMDESC_UNITS], "units", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\"\n",
		"units", desc[PMDESC_UNITS]);--*TODO*/

/*--fputs("    }", stdout);--*TODO*/
    return 0;
}

void
pmSeriesDescs(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply;
    sds			cmd, key, msg, desc[PMDESC_MAXFIELD];
    int			i, n, rc, sts = 0;

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

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    desc[PMDESC_INDOM] = sdsnewlen("", 16);
    desc[PMDESC_PMID] = sdsnewlen("", 16);
    desc[PMDESC_SEMANTICS] = sdsnewlen("", 16);
    desc[PMDESC_SOURCE] = sdsnewlen("", 40);
    desc[PMDESC_TYPE] = sdsnewlen("", 16);
    desc[PMDESC_UNITS] = sdsnewlen("", 16);

    /* unpack - iterate over series and extract descriptor for each */
    for (n = 0; n < nseries; n++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s on series %s", HMGET, series[n]);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array type from series %s %s (type=%s)",
			series[n], HMGET, redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_desc_reply(settings, series[n],
				reply->elements, reply->element, desc, arg)) < 0) {
	    sts = rc;
	} else if ((rc = settings->on_desc(series[n],
				PMDESC_MAXFIELD, desc, arg)) < 0) {
	    sts = rc;
	}

	freeReplyObject(reply);

/*--	if (n + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    for (i = 0; i < PMDESC_MAXFIELD; i++)
	sdsfree(desc[i]);

done:
    querydone(settings, sts, arg);
}

static int
series_inst_name_prepare(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
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
	redisContext *redis, redisReply **rp, reverseMap *mp, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
	/* unpack - produce reverse map of ids-to-names for each entry */
	sts = reverse_map(settings, reply->elements, reply->element, mp, arg);
    }
    return sts;
}

static int
series_inst_reply(pmSeriesSettings *settings, pmSeriesID series, reverseMap *map,
	int nelements, redisReply **elements, sds *inst, void *arg)
{
    sds			msg;

    if (nelements < PMINST_MAXFIELD) {
	queryfmt(msg, "bad reply from %s %s (%d)", series, HMGET, nelements);
	queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	return -EPROTO;
    }

/*--printf("    \"%s\" : {\n", series);--TODO*/

    if (extract_string(settings, series, elements[PMINST_INSTID],
			    &inst[PMINST_INSTID], "inst", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"inst", inst[PMINST_INSTID]);--*TODO*/

    if (extract_mapping(settings, series, map, elements[PMINST_NAME],
			    &inst[PMINST_NAME], "name", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"name", inst[PMINST_NAME]);--*TODO*/

    if (extract_sha1(settings, series, elements[PMINST_SERIES],
			    &inst[PMINST_SERIES], "series", arg) < 0)
	return -EPROTO;
/*--printf("      \"%s\": \"%s\",\n",
		"series", &inst[PMINST_SERIES]);--*TODO*/

/*--fputs("    }", stdout);--*TODO*/
    return 0;
}

static int
series_instances_reply(pmSeriesSettings *settings, redisContext *redis,
	pmSeriesID series, int nelements, redisReply **elements, sds *inst,
	reverseMap *map, void *arg)
{
    redisReply		*reply;
    pmSeriesID		sid;
    sds			key, cmd, msg;
    int			i, rc, sts = 0;

    /*
     * iterate over the instance series identifiers, looking up
     * the hash for each.
     */
    for (i = 0; i < nelements; i++) {
	if (extract_sha1(settings, sid, elements[i], &sid, "series", arg) < 0)
	    return -EPROTO;

	key = sdscatfmt(sdsempty(), "pcp:inst:series:%S", sid);
	cmd = redis_command(5);
	cmd = redis_param_str(cmd, HMGET, HMGET_LEN);
	cmd = redis_param_sds(cmd, key);
	cmd = redis_param_str(cmd, "inst", sizeof("inst")-1);
	cmd = redis_param_str(cmd, "name", sizeof("name")-1);
	cmd = redis_param_str(cmd, "series", sizeof("series")-1);
	redis_submit(redis, HMGET, key, cmd);
    }

    /* TODO: async response handling */

    for (i = 0; i < nelements; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    queryfmt(msg, "failed %s %s:%s",
			HMGET, "pcp:inst:series", series);
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    queryfmt(msg, "expected array from series %s %s (type=%s)",
			HMGET, series, redis_reply(reply->type));
	    queryinfo(settings, PMLOG_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if ((rc = series_inst_reply(settings, series, map,
				reply->elements, reply->element, inst, arg)) < 0) {
	    sts = rc;
	} else if ((rc = settings->on_inst(series,
				PMINST_MAXFIELD, inst, arg)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);
    }
    return sts;
}

void
pmSeriesInstances(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    sds			cmd, key, msg, inst[PMINST_MAXFIELD];
    int			i, rc, sts = 0;

    if (nseries <= 0) {
	sts = (nseries < 0) ? -EINVAL :
		series_map_keys(settings, redis,
				settings->on_instname, arg, "inst.name");
	goto done;
    }

    if ((sts = series_inst_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_inst_name_execute(settings, redis, &rp, &map, arg)) < 0)
	goto done;

    inst[PMINST_INSTID] = sdsnewlen("", 16);
    inst[PMINST_NAME] = sdsnewlen("", 16);
    inst[PMINST_SERIES] = sdsnewlen("", 40);

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    for (i = 0; i < nseries; i++) {
	/* prepare command series */
	key = sdscatfmt(sdsempty(), "pcp:instances:series:%S", series[i]);
	cmd = redis_command(2);
	cmd = redis_param_str(cmd, SMEMBERS, SMEMBERS_LEN);
	cmd = redis_param_sds(cmd, key);
	redis_submit(redis, SMEMBERS, key, cmd);

	/* TODO: async response handling */

	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
			reply->elements, reply->element, inst, &map, arg)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);

/*--	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    freeReplyObject(rp);

    for (i = 0; i < PMINST_MAXFIELD; i++)
	sdsfree(inst[i]);
done:
    querydone(settings, sts, arg);
}


static int
series_source_name_prepare(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
{
    sds			cmd, key;

    key = sdscatfmt(sdsempty(), "pcp:map:context.name");
    cmd = redis_command(2);
    cmd = redis_param_str(cmd, HGETALL, HGETALL_LEN);
    cmd = redis_param_sds(cmd, key);
    redis_submit(redis, HGETALL, key, cmd);
    return 0;
}

static int
series_source_name_execute(pmSeriesSettings *settings, 
	redisContext *redis, redisReply **rp, reverseMap *mp, void *arg)
{
    redisReply		*reply;
    sds			msg;
    int			sts;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
	/* unpack - produce reverse map of ids-to-names for each entry */
	sts = reverse_map(settings, reply->elements, reply->element, mp, arg);
    }
    return sts;
}

void
pmSeriesSources(pmSeriesSettings *settings,
	int nsources, pmSourceID *sources, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    sds			cmd, key, msg;
    int			i, rc, sts;

    if (nsources <= 0) {
	sts = (nsources < 0) ? -EINVAL :
		series_map_keys(settings, redis,
				settings->on_instname, arg, "context.name");
	goto done;
    }
    if ((sts = series_source_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_source_name_execute(settings, redis, &rp, &map, arg)) < 0)
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

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    /* TODO: async response handling */

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));--*TODO*/
/*--printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nsources; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
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
				reply->elements, reply->element, &map)) < 0) {
	    sts = rc;
	}
	freeReplyObject(reply);

/*--	if (i + 1 < nsources)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

    freeReplyObject(rp);

done:
    querydone(settings, sts, arg);
}
