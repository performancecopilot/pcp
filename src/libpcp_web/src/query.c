/*
 * Copyright (c) 2017 Red Hat.
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
#include "redis.h"
#include "query.h"
#include "series.h"
#include "libpcp.h"

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

#define MSGSIZE		(PMSIDSZ+64)
#define queryinfo(set, level, msg, arg)	(set)->on_info(level, msg, arg)
#define querydesc(set,id,p,i,s,t,u,arg)	(set)->on_desc(id, p,i,s,t,u, arg)
#define queryinst(set, sid, i,n, arg)	(set)->on_instance(sid, i, n, arg)
#define querylabels(set, sid,label,arg)	(set)->on_labels(sid, label, arg)
#define querymetric(set, sid,name, arg)	(set)->on_metric(sid, name, arg)
#define querydone(set, sts, arg)	(set)->on_done(sts, arg)

#define solvermsg(SP, level, message)		\
	((SP)->settings->on_info((level), (message), (SP)->arg))
#define solvermatch(SP, sid)			\
	((SP)->settings->on_match((sid), (SP)->arg))
#define solvervalue(SP, sid, time, value)	\
	((SP)->settings->on_value((sid), (time), (value), (SP)->arg))

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

char *
series_instance_name(char *key, size_t length)
{
    if (length > sizeof("instance.") &&
	strncmp(key, "instance.", sizeof("instance.") - 1) == 0)
	return key + sizeof("instance.") - 1;
    if (length > sizeof("inst.") &&
	strncmp(key, "inst.", sizeof("inst.") - 1) == 0)
	return key + sizeof("inst.") - 1;
    if (length > sizeof("i.") &&
	strncmp(key, "i.", sizeof("i.") - 1) == 0)
	return key + sizeof("i.") - 1;
    return NULL;
}

char *
series_metric_name(char *key, size_t length)
{
    if (length > sizeof("metric.") &&
	strncmp(key, "metric.", sizeof("metric.") - 1) == 0)
	return key + sizeof("metric.") - 1;
    if (length > sizeof("m.") &&
	strncmp(key, "m.", sizeof("m.") - 1) == 0)
	return key + sizeof("m.") - 1;
    return NULL;
}

char *
series_label_name(char *key, size_t length)
{
    if (length > sizeof("label.") &&
	strncmp(key, "label.", sizeof("label.") - 1) == 0)
	return key + sizeof("label.") - 1;
    if (length > sizeof("l.") &&
	strncmp(key, "l.", sizeof("l.") - 1) == 0)
	return key + sizeof("l.") - 1;
    return NULL;
}

const char *
node_subtype(node_t *np)
{
    switch (np->subtype) {
	case N_METRIC:	return "metric";
	case N_INSTANCE: return "inst";
	case N_LABEL: return "label";
	default: break;
    }
    return NULL;
}

/*
 * Report a timeseries result - value + timestamp (score) pairs
 */
static int
series_values_reply(SOLVER *sp, pmSeriesID *seriesid,
	int nelements, redisReply **elements)
{
    redisReply		*value;
    redisReply		*score;
    char		msg[MSGSIZE];
    int			i, sts = 0;

    if (nelements <= 0)
	return nelements;
    if (nelements % 2) {	/* expecting (value, timestamp) pairs */
	pmsprintf(msg, sizeof(msg),
		"expected time:value pairs from %.*s values ZSET",
		PMSIDSZ, seriesid->name);
	solvermsg(sp, PMSERIES_RESPONSE, msg);
	return -EPROTO;
    }

/*--printf("    \"%.*s\" : {\n", PMSIDSZ, seriesid->name);--*TODO*/

    for (i = 0; i < nelements; i += 2) {
	value = elements[i];
	score = elements[i+1];
	if (value->type == REDIS_REPLY_STRING) {
	    if (score->type == REDIS_REPLY_STRING) {
/*--		printf("      \"%.*s\": \"%s\"",
				15, score->str, value->str);--*TODO*/
		solvervalue(sp, seriesid, score->str, value->str);
		sp->settings->on_value(seriesid, score->str, value->str, sp->arg);
	    } else {
		pmsprintf(msg, sizeof(msg),
			"expected string stamp for series %.*s (type=%s)",
			PMSIDSZ, seriesid->name, redis_reply(value->type));
		solvermsg(sp, PMSERIES_RESPONSE, msg);
		sts = -EPROTO;
	    }
	} else {
	    pmsprintf(msg, sizeof(msg),
			"expected string value for series %.*s (type=%s)",
			PMSIDSZ, seriesid->name, redis_reply(value->type));
	    solvermsg(sp, PMSERIES_RESPONSE, msg);
	    sts = -EPROTO;
	}
/*--	if (i + 2 < nelements)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("    }", stdout);--*TODO*/

    return sts;
}

/*
 * Save the series hash identifiers contained in a Redis response.
 * Used at the leaves of the query tree, then merged result sets
 * are propagated upward.
 */
static int
node_series_reply(SOLVER *sp, node_t *np, int nelements, redisReply **elements)
{
    redisReply	*series;
    char	msg[MSGSIZE];
    char	*seriesp;
    int		i, sts = 0;

    if ((np->nseries = nelements) <= 0)
	return nelements;

    np->series = malloc(nelements * PMSIDSZ);
    seriesp = (char *)np->series;

    for (i = 0; i < nelements; i++) {
	series = elements[i];
	if (series->type == REDIS_REPLY_STRING) {
	    if (pmDebugOptions.series)
	        printf("    %s\n", series->str);
	    memcpy(seriesp, series->str, PMSIDSZ);
	    seriesp += PMSIDSZ;
	} else {
	    pmsprintf(msg, sizeof(msg),
		    "expected string in %s set \"%s\" (type=%s)",
		    node_subtype(np->left), np->left->key,
		    redis_reply(series->type));
	    solvermsg(sp, PMSERIES_REQUEST, msg);
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
    return memcmp(a, b, PMSIDSZ);
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
    char	*small, *large, *cp, *saved;
    int		i, nsmall, nlarge, total;

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

    qsort(large, nlarge, PMSIDSZ, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += PMSIDSZ) {
	if (bsearch(cp, large, nlarge, PMSIDSZ, series_compare) == NULL)
	    continue;		/* no match, continue advancing cp only */
	if (saved != cp)
	    memcpy(saved, cp, PMSIDSZ);
	saved += PMSIDSZ;	/* stashed, advance cp & saved pointers */
    }

    if ((total = (saved - small) / PMSIDSZ) < nsmall) {
	/* shrink the smaller set down further */
	if ((small = realloc(small, total * PMSIDSZ)) == NULL)
	    return -ENOMEM;
    }

    np->nseries = total;
    np->series = small;

    if (pmDebugOptions.series) {
	printf("Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp += PMSIDSZ, i++)
	    printf("    %.*s\n", PMSIDSZ, cp);
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
    char	*small, *large, *cp, *saved;
    int		i, nsmall, nlarge, total, need;

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

    qsort(large, nlarge, PMSIDSZ, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += PMSIDSZ) {
	if (bsearch(cp, large, nlarge, PMSIDSZ, series_compare) != NULL)
	    continue;		/* already present, no need to save */
	if (saved != cp)
	    memcpy(saved, cp, PMSIDSZ);
	saved += PMSIDSZ;	/* stashed, advance both cp & saved */
    }

    if ((need = (saved - small) / PMSIDSZ) > 0) {
	/* grow the larger set to cater for new entries, then add 'em */
	if ((large = realloc(large, (nlarge + need) * PMSIDSZ)) == NULL)
	    return -ENOMEM;
	cp = large + (nlarge * PMSIDSZ);
	memcpy(cp, small, need * PMSIDSZ);
	total = nlarge + need;
    } else {
	total = nlarge;
    }

    np->nseries = total;
    np->series = large;

    if (pmDebugOptions.series) {
	printf("Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += PMSIDSZ, i++)
	    printf("    %.*s\n", PMSIDSZ, cp);
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
    int		length, sts;
    char	*name, *cmd;
    char	msg[MSGSIZE];

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
	length = strlen(np->value);
	if ((name = series_instance_name(np->value, length)) != NULL) {
	    np->subtype = N_INSTANCE;
	    np->key = strdup("pcp:map:inst.name");
	} else if ((name = series_metric_name(np->value, length)) != NULL) {
	    np->subtype = N_METRIC;
	    np->key = strdup("pcp:map:metric.name");
	} else {
	    np->subtype = N_LABEL;
	    if ((name = series_label_name(np->value, length)) == NULL)
		name = np->value;
	    length = redisFormatCommand(&cmd, "HGET pcp:map:label.name %s", name);
	    if (redisAppendFormattedCommand(sp->redis, cmd, length) != REDIS_OK) {
		pmsprintf(msg, sizeof(msg),
			"failed to setup label name lookup command");
		solvermsg(sp, PMSERIES_REQUEST, msg);
		sts = -EAGAIN;
	    }
	    sp->count++;
	    free(cmd);
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
    redisReply	*reply;
    char	msg[MSGSIZE];
    char	id[128];
    int		sts = 0;

    if (np == NULL)
	return 0;

    series_resolve_maps(sp, np->left, level+1);

    switch (np->type) {
    case N_NAME:
	/* TODO: need to handle JSONB label name nesting. */

	/* setup any label name map identifiers needed */
	if (np->subtype == N_LABEL) {
	    if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
		pmsprintf(msg, sizeof(msg),
			"no %s named \"%s\" found (error=%d)",
			node_subtype(np), np->value, sp->redis->err);
		solvermsg(sp, PMSERIES_RESPONSE, msg);
		sts = -EPROTO;
	    } else if (reply->type != REDIS_REPLY_STRING) {
		pmsprintf(msg, sizeof(msg),
			"expected string for %s map \"%s\" (type=%s)",
			node_subtype(np), np->value, redis_reply(reply->type));
		solvermsg(sp, PMSERIES_RESPONSE, msg);
		sts = -EPROTO;
	    } else {
		sp->replies[sp->index++] = reply;
		pmsprintf(id, sizeof(id), "pcp:map:%s.%s.value",
			node_subtype(np), reply->str);
		solvermsg(sp, PMSERIES_RESPONSE, msg);
		np->key = strdup(id);
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
    int		length, sts;
    char	msg[MSGSIZE];
    char	*key, *value, *cmd;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_eval(sp, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	key = np->left->key;
	value = np->right->value;
	length = redisFormatCommand(&cmd, "HGET %s %s", key, value);
	if (redisAppendFormattedCommand(sp->redis, cmd, length) == REDIS_OK) {
	    sp->count++;
	} else {
	    pmsprintf(msg, sizeof(msg),
		    "failed setup equality test on %s:%s (key %s)\n",
		    np->left->value, value, key);
	    solvermsg(sp, PMSERIES_REQUEST, msg);
	    sts = -EPROTO;
	}
	free(cmd);
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
    redisReply	*reply;
    node_t	*left;
    char	*name;
    char	id[128];
    char	msg[MSGSIZE];
    int		sts;

    if (np == NULL)
	return 0;

    if ((sts = series_resolve_eval(sp, np->left, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;

	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg),
		    "map table %s key %s not found",
		    left->key, np->right->value);
	    solvermsg(sp, PMSERIES_RESPONSE, msg);
	    sts = -EPROTO;
	}
	if (reply->type != REDIS_REPLY_STRING) {
	    pmsprintf(msg, sizeof(msg),
		    "expected string for %s key \"%s\" (type=%s)",
		    node_subtype(left), left->key, redis_reply(reply->type));
	    solvermsg(sp, PMSERIES_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    sp->replies[sp->index++] = reply;
	    pmsprintf(id, sizeof(id), "pcp:series:%s:%s", name, reply->str);
	    if (np->key)
		free(np->key);
	    np->key = strdup(id);
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
    int		length, sts;
    char	msg[MSGSIZE];
    char	*cmd;

    if (np == NULL)
	return 0;

    if ((sts = series_prepare_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((series_prepare_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	length = redisFormatCommand(&cmd, "SMEMBERS %s", np->key);
	if (redisAppendFormattedCommand(sp->redis, cmd, length) == REDIS_OK) {
	    sp->count++;
	} else {
	    pmsprintf(msg, sizeof(msg), "failed SMEMBERS (key=%s)", np->key);
	    solvermsg(sp, PMSERIES_REQUEST, msg);
	    sts = -EPROTO;
	}
	free(cmd);
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
    redisReply	*reply;
    char	msg[MSGSIZE];
    int		sts;

    if (np == NULL)
	return 0;

    /* evaluate leaves first then interior nodes */
    if ((sts = series_resolve_expr(sp, np->left, level+1)) < 0)
	return sts;
    if ((sts = series_resolve_expr(sp, np->right, level+1)) < 0)
	return sts;

    switch (np->type) {
    case N_EQ:
	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg),
			"map table %s key %s not found",
			np->left->key, np->right->value);
	    solvermsg(sp, PMSERIES_CORRUPT, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array for %s set \"%s\" (type=%s)",
			node_subtype(np->left), np->right->value,
			redis_reply(reply->type));
	    solvermsg(sp, PMSERIES_CORRUPT, msg);
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

int
seriesid_copy(const char *series, pmSeriesID *seriesid)
{
    char *name = (char *)seriesid->name;
    int	bytes = pmsprintf(name, PMSIDSZ, "%.*s", PMSIDSZ, series);
    name[PMSIDSZ] = '\0';
    return bytes == PMSIDSZ? 0 : -E2BIG;
}

#define DEFAULT_VALUE_COUNT 10

static int
series_prepare_time(SOLVER *sp, timing_t *tp, int nseries, char *series)
{
    char	start[64], end[64];
    char	msg[MSGSIZE], *cmd;
    pmSeriesID	seriesid;
    int		sts = 0, len, i;

    pmsprintf(start, sizeof(start), "%.64g", tv2real(&tp->start));
    if (pmDebugOptions.series)
	fprintf(stderr, "START: %s\n", start);

    if (tp->end.tv_sec)
	pmsprintf(end, sizeof(end), "%.64g", tv2real(&tp->end));
    else
	pmsprintf(end, sizeof(end), "+inf");
    if (pmDebugOptions.series)
	fprintf(stderr, "END: %s\n", end);

    if (tp->count == 0)
	tp->count = DEFAULT_VALUE_COUNT;
    if (tp->offset > tp->count)
	nseries = 0;	/* we're finished */
    if (pmDebugOptions.series)
	fprintf(stderr, "LIMIT: %u %u\n", tp->offset, tp->count);

    /*
     * Query cache for the time series range (time:value pairs)
     * - ZSET values are metric values, score is the timestamp.
     */

    for (i = 0; i < nseries; i++, series += PMSIDSZ) {
	if (seriesid_copy(series, &seriesid) < 0) {
	    sts = -EPROTO;
	    continue;
	}

	/* ZREVRANGEBYSCORE key max min [WITHSCORES] [LIMIT offset count] */
	len = redisFormatCommand(&cmd, "ZREVRANGEBYSCORE"
		" pcp:values:series:%s %s %s WITHSCORES LIMIT %u %u",
		seriesid.name, end, start, tp->offset, tp->count);

	if (redisAppendFormattedCommand(sp->redis, cmd, len) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg),
			"failed pcp:values:series:%.*s ZREVRANGEBYSCORE",
			PMSIDSZ, series);
	    solvermsg(sp, PMSERIES_REQUEST, msg);
	    sts = -EPROTO;
	} else {
	    sp->count++;
	}
	free(cmd);
    }

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
series_resolve_time(SOLVER *sp, int nseries, char *series)
{
    redisReply		*reply;
    pmSeriesID		seriesid;
    char		msg[MSGSIZE];
    int			i, sts = 0;

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    for (i = 0; i < nseries; i++, series += PMSIDSZ) {
	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "failed series %.*s ZSET query",
			PMSIDSZ, series);
	    solvermsg(sp, PMSERIES_RESPONSE, msg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array from %.*s zset values (type=%s)",
			PMSIDSZ, series, redis_reply(reply->type));
	    solvermsg(sp, PMSERIES_RESPONSE, msg);
	    sts = -EPROTO;
	} else {
	    memcpy(seriesid.name, series, PMSIDSZ);
	    seriesid.name[PMSIDSZ] = '\0';
	    sts |= series_values_reply(sp, &seriesid,
				reply->elements, reply->element);
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
    sp->replies = calloc(sp->count, sizeof(redisReply *));
    /* TODO: error handling */
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
series_report_set(SOLVER *sp, int nseries, char *series)
{
    pmSeriesID		seriesid;
    int			i;

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": [\n");--*TODO*/

    for (i = 0; i < nseries; series += PMSIDSZ, i++) {
	memcpy(seriesid.name, series, PMSIDSZ);
	seriesid.name[PMSIDSZ] = '\0';
	solvermatch(sp, &seriesid);

/*--	printf("    \"%.*s\"", PMSIDSZ, series);
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
	node_t *root, timing_t *timing, pmseries_flags flags, void *arg)
{
    SOLVER	solver = { .settings = settings, .arg = arg };
    SOLVER	*sp = &solver;

    solver.redis = redis_init();

    /* Resolve label and note key names (via their map keys) */
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
    if ((flags & PMSERIES_METADATA) || !series_time_window(timing)) {
	series_report_set(sp, root->nseries, root->series);
	return 0;
    }

    /* Extract values within the given time window */
    if (pmDebugOptions.series)
	fprintf(stderr, "series_time\n");
    series_prepare_time(sp, timing, root->nseries, root->series);
    new_solver_replies(sp);
    series_resolve_time(sp, root->nseries, root->series);
    free_solver_replies(sp);

    return 0;
}

/* build a reverse hash mapping */
static int
reverse_map(pmSeriesSettings *settings, int nkeys,
	redisReply **elements, reverseMap *map, void *arg)
{
    redisReply		*name, *key;
    char		msg[MSGSIZE];
    int			i;

    __pmHashInit(map);
    for (i = 0; i < nkeys; i += 2) {
	name = elements[i];
	key = elements[i+1];
	if (name->type == REDIS_REPLY_STRING) {
	    if (key->type == REDIS_REPLY_STRING) {
		__pmHashAdd(atoi(key->str), name->str, map);
	    } else {
		pmsprintf(msg, sizeof(msg),
			"expected string key for hashmap (type=%s)",
			redis_reply(key->type));
		settings->on_info(PMSERIES_RESPONSE, msg, arg);
		return -EINVAL;
	    }
	} else {
	    pmsprintf(msg, sizeof(msg),
		    "expected string name for hashmap (type=%s)",
		    redis_reply(name->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    return -EINVAL;
	}
    }
    return 0;
}

/*
 * Produce the list of label names (requires reverse mapping from IDs)
 */
static int
series_map_reply(pmSeriesSettings *settings, pmSeriesID *series,
	pmSeriesStringCallback series_string_callback, void *arg,
	int nelements, redisReply **elements, reverseMap *map)
{
    redisReply		*id;
    reverseMapNode	node;
    char		*name;
    char		msg[MSGSIZE];
    int			i, sts = 0;

/*--printf("    \"%.*s\" : [", PMSIDSZ, series->name);--*TODO*/

    for (i = 0; i < nelements; i++) {
	id = elements[i];
	if (id->type == REDIS_REPLY_STRING) {
	    if ((node = __pmHashSearch(atoi(id->str), map)) == NULL) {
		pmsprintf(msg, sizeof(msg), "%.*s - timeseries string map",
				PMSIDSZ, series->name);
		settings->on_info(PMSERIES_CORRUPT, msg, arg);
		sts = -EINVAL;
	    } else {
		name = (char *)node->data;
/*--     	printf(" \"%s\"", name);--*TODO*/
		series_string_callback(series, name, arg);
	    }
	} else {
	    pmsprintf(msg, sizeof(msg),
			"expected string in %.*s set (type=%s)",
			PMSIDSZ, series->name, redis_reply(id->type));
	    queryinfo(settings, PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	}
/*--	if (i < nelements - 1)
	    fputc(',', stdout);--*TODO*/
    }
/*--fputs(" ]", stdout);--*TODO*/

    return sts;
}

static int
series_all_labels(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
{
    redisReply		*reply, *rp;
    char		msg[MSGSIZE];
    char		*cmd;
    int			i, len, sts = 0;

    /* prepare command */

    len = redisFormatCommand(&cmd, "HKEYS pcp:map:label.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:label.name HKEYS");
	settings->on_info(PMSERIES_REQUEST, msg, arg);
	return -EAGAIN;
    }
    free(cmd);

    /* response handling */

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:label.name HKEYS");
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	return -EPROTO;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	pmsprintf(msg, sizeof(msg),
		"expected array from pcp:map:label.name HKEYS (type=%s)",
		redis_reply(reply->type));
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	freeReplyObject(reply);
	return -EPROTO;
    }

/*--printf("[");--*TODO*/
    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
/*--	    printf(" \"%s\"", rp->str);--*TODO*/
	    querylabels(settings, NULL, rp->str, arg);
	} else {
	    pmsprintf(msg, sizeof(msg),
			"expected string in label HGET (type=%s)\n",
			redis_reply(rp->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EINVAL;
	}
/*--	if (i + 1 < reply->elements)
	    fputc(',', stdout);--*TODO*/
	freeReplyObject(rp);
    }
/*--printf(" ]\n");--*TODO*/

    return sts;
}

void
pmSeriesLabel(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    pmSeriesID		seriesid;
    reverseMap		map;
    char		*cmd;
    char		msg[MSGSIZE];
    int			i, len, sts = 0;

    if (nseries <= 0) {
	if (nseries == 0)
	    sts = series_all_labels(settings, redis, arg);
	else
	    sts = -EINVAL;
	goto done;
    }

    /* prepare command batch */

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:label.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:label.name HGETALL");
	queryinfo(settings, PMSERIES_REQUEST, msg, arg);
	free(cmd);
	sts = -EINVAL;
	goto done;
    }
    free(cmd);

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "SMEMBERS pcp:label.name:series:%s",
				series[i].name);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg),
			"failed pcp:label.name:series:%.*s SMEMBERS",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_REQUEST, msg, arg);
	    sts = -EINVAL;
	}
	free(cmd);
    }
    if (sts < 0)
	goto done;

    /* response handling */

    if (redisGetReply(redis, (void **)&rp) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed HGETALL pcp:map:label.name");
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else if (rp->type != REDIS_REPLY_ARRAY) {
	pmsprintf(msg, sizeof(msg),
		"expected array from pcp:map:label HGETALL (type=%s)",
		redis_reply(rp->type));
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
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
	    pmsprintf(msg, sizeof(msg),
			"failed pcp:label.name:series:%.*s SMEMBERS",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array from %.*s SMEMBERS (type=%s)",
			PMSIDSZ, series[i].name, redis_reply(reply->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else {
	    memcpy(seriesid.name, series, PMSIDSZ);
	    seriesid.name[PMSIDSZ] = '\0';
	    sts |= series_map_reply(settings, &seriesid,
				settings->on_labels, arg,
				reply->elements, reply->element, &map);
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
series_all_metrics(pmSeriesSettings *settings, redisContext *redis,
	void *arg)
{
    redisReply		*reply, *rp;
    char		msg[MSGSIZE];
    char		*cmd;
    int			i, len;

    /* prepare command */

    len = redisFormatCommand(&cmd, "HKEYS pcp:map:metric.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:metric.name HKEYS");
	settings->on_info(PMSERIES_REQUEST, msg, arg);
	return -EAGAIN;
    }
    free(cmd);

    /* response handling */

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:metric.name HKEYS");
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	return -EPROTO;
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	pmsprintf(msg, sizeof(msg),
		"expected array from pcp:map:metric HKEYS (reply=%s)",
		redis_reply(reply->type));
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	freeReplyObject(reply);
	return -EPROTO;
    }

/*--printf("[");--*TODO*/
    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
/*--	    printf(" \"%s\"", rp->str);--*TODO*/
	    querymetric(settings, NULL, rp->str, arg);
	} else {
	    pmsprintf(msg, sizeof(msg), "bad response in metrics HGET (%s)",
			redis_reply(rp->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    return -EINVAL;
	}
/*--	if (i + 1 < reply->elements)
	    fputc(',', stdout);--*TODO*/
	freeReplyObject(rp);
    }
/*--printf(" ]\n");--*TODO*/

//  freeReplyObject(reply);

    return 0;
}

static int
series_metric_name_prepare(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
{
    char		*cmd;
    char		msg[MSGSIZE];
    int			len, sts = 0;

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:metric.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:metric.name HGETALL");
	settings->on_info(PMSERIES_REQUEST, msg, arg);
	sts = -EAGAIN;
    }
    free(cmd);
    return sts;
}

static int
series_metric_name_execute(pmSeriesSettings *settings,
	redisContext *redis, redisReply **rp, reverseMap *mp, void *arg)
{
    redisReply		*reply;
    char		msg[MSGSIZE];
    int			sts;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed pcp:map:metric.name HGETALL");
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	pmsprintf(msg, sizeof(msg),
		"expected array from pcp:map:metric.name HGETALL (type=%s)",
		redis_reply(reply->type));
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	*rp = reply;
	/* unpack - produce reverse map of ids -> names for each label */
	sts = reverse_map(settings, reply->elements, reply->element, mp, arg);
    }
    return sts;
}

void
pmSeriesMetric(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    char		*cmd;
    char		msg[MSGSIZE];
    int			i, len, sts;

    if (nseries <= 0) {
	if (nseries == 0)
	    sts = series_all_metrics(settings, redis, arg);
	else
	    sts = -EINVAL;
	goto done;
    }

    if ((sts = series_metric_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_metric_name_execute(settings, redis, &rp, &map, arg)) < 0)
	goto done;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "SMEMBERS pcp:metric.name:series:%s",
				series[i].name);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg),
			"failed pcp:metric.name:series:%.*s smembers",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_REQUEST, msg, arg);
	    sts = -EINVAL;
	}
	free(cmd);
    }
    if (sts < 0)
	goto done;

    /* response handling */

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));--*TODO*/
/*--printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "SMEMBERS series %.*s query failed",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_REQUEST, msg, arg);
	    sts = -EAGAIN;
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array from %.*s SMEMBERS (type=%s)",
			PMSIDSZ, series[i].name, redis_reply(reply->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else {
	    sts |= series_map_reply(settings, &series[i],
				settings->on_metric, arg,
				reply->elements, reply->element, &map);
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
extract_number(pmSeriesSettings *settings, pmSeriesID *series,
	redisReply *element, unsigned int *response,
	const char *message, void *arg)
{
    char		msg[MSGSIZE];
    char		*endnum;

    if (element->type != REDIS_REPLY_STRING) {
	pmsprintf(msg, sizeof(msg),
		"expected string for numeric %s of series %.*s (got %s)",
		message, PMSIDSZ, series->name, redis_reply(element->type));
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	return -EINVAL;
    }
    *response = strtoul(element->str, &endnum, 10);
    if (*endnum != '\0') {
	pmsprintf(msg, sizeof(msg),
		"expected a number for %s of series %.*s (got %s)",
		message, PMSIDSZ, series->name, element->str);
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	return -EINVAL;
    }
    return 0;
}

static int
extract_string(pmSeriesSettings *settings, pmSeriesID *series,
	redisReply *element, char **response,
	const char *message, void *arg)
{
    char		msg[MSGSIZE];

    if (element->type != REDIS_REPLY_STRING) {
	pmsprintf(msg, sizeof(msg),
			"expected string result for %s of series %.*s",
			message, PMSIDSZ, series->name);
	queryinfo(settings, PMSERIES_RESPONSE, msg, arg);
	return -EINVAL;
    }
    *response = element->str;
    return 0;
}

/* PM_TYPE_* -> string, max length is 20 bytes */
char *
series_type_string(int type, char *buf, int buflen)
{
    static const char *series_type_names[] = {
	"s32", "u32", "s64", "u64", "float", "double", "string",
	"aggregate", "aggregate_static", "event", "highres_event"
    };

    if (type >= 0 && type < sizeof(series_type_names) / sizeof(const char *))
	pmsprintf(buf, buflen, "%s", series_type_names[type]);
    else if (type == PM_TYPE_NOSUPPORT)
	pmsprintf(buf, buflen, "%s", "not_supported");
    else if (type == PM_TYPE_UNKNOWN)
	pmsprintf(buf, buflen, "%s", "unknown");
    else
	pmsprintf(buf, buflen, "bad_type=%d", type);
    return buf;
}

static int
series_desc_reply(pmSeriesSettings *settings, pmSeriesID *series,
	int nelements, redisReply **elements, void *arg)
{
    unsigned int	domain, cluster, item, serial;
    unsigned int	sem, type;
    char		*units;
    pmID		pmid;
    pmInDom		indom;
    char		msg[MSGSIZE];
    char		pmids[32], indoms[32], sems[16], types[20];

    if (nelements != 7) {
	pmsprintf(msg, sizeof(msg), "bad reply from %.*s HMGET (%d)",
			PMSIDSZ, series->name, nelements);
	settings->on_info(PMSERIES_RESPONSE, msg, arg);
	return -EPROTO;
    }

/*--printf("    \"%.*s\" : {\n", PMSIDSZ, series->name);--TODO*/

    if (extract_number(settings, series, elements[0], &domain,
			    "PMID domain", arg) < 0)
	return -EPROTO;
    if (extract_number(settings, series, elements[1], &cluster,
			    "PMID cluster", arg) < 0)
	return -EPROTO;
    if (extract_number(settings, series, elements[2], &item,
			    "PMID item", arg) < 0)
	return -EPROTO;
    pmid = pmID_build(domain, cluster, item);
    pmIDStr_r(pmid, pmids, sizeof(pmids));
/*--printf("      \"pmid\": \"%s\",\n", pmids);--TODO*/

    if (elements[3]->type != REDIS_REPLY_NIL &&
	extract_number(settings, series, elements[3], &serial,
			    "InDom serial", arg) == 0) {
	indom = pmInDom_build(domain, serial);
	pmInDomStr_r(indom, indoms, sizeof(indoms));
/*--	printf("      \"indom\": \"%s\",\n", indoms);--TODO*/
    } else {
	indoms[0] = '\0';
    }

    if (extract_number(settings, series, elements[4], &sem,
			    "semantics", arg) < 0)
	return -EPROTO;
    pmSemStr_r(sem, sems, sizeof(sems));
/*--printf("      \"semantics\": \"%s\",\n", sems);--*TODO*/
    if (extract_number(settings, series, elements[5], &type,
			    "type", arg) < 0)
	return -EPROTO;
    series_type_string(type, types, sizeof(types));
/*--printf("      \"type\": \"%s\",\n", types);--*TODO*/
    if (extract_string(settings, series, elements[6], &units,
			    "units", arg) != 0 || units[0] == '\0')
	units = "none";
/*--printf("      \"units\": \"%s\"\n", units);--*TODO*/

    querydesc(settings, series, pmids, indoms, sems, types, units, arg);

/*--fputs("    }", stdout);--*TODO*/
    return 0;
}

void
pmSeriesDesc(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply;
    char		msg[MSGSIZE];
    char		*cmd;
    int			i, len, sts = 0;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "HMGET pcp:desc:series:%s "
			"domain cluster item serial semantics type units",
			series[i].name);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "failed pcp:desc:series:%.*s HMGET",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_REQUEST, msg, arg);
	    sts = -EAGAIN;
	}
	free(cmd);
    }
    if (sts < 0)
	goto done;

    /* response handling */

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract descriptor for each */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "failed HMGET on series %.*s",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array type from series %.*s HMGET (type=%s)",
			PMSIDSZ, series[i].name, redis_reply(reply->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	}
	series_desc_reply(settings, &series[i],
			reply->elements, reply->element, arg);
	freeReplyObject(reply);

/*--	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);--*TODO*/
    }
/*--fputs("  }\n}\n", stdout);--*TODO*/

done:
    querydone(settings, sts, arg);
}

static int
series_inst_name_prepare(pmSeriesSettings *settings,
	redisContext *redis, void *arg)
{
    char		*cmd;
    int			len, sts = 0;

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:inst.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	settings->on_info(PMSERIES_REQUEST, "failed pcp:map:inst.name HGETALL", arg);
	sts = -EINVAL;
    }
    free(cmd);
    return sts;
}

static int
series_inst_name_execute(pmSeriesSettings *settings, 
	redisContext *redis, redisReply **rp, reverseMap *mp, void *arg)
{
    redisReply		*reply;
    char		msg[MSGSIZE];
    int			sts;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	pmsprintf(msg, sizeof(msg), "failed HGETALL pcp:map:inst.name");
	queryinfo(settings, PMSERIES_RESPONSE, msg, arg);
	sts = -EAGAIN;
    } else if (reply->type != REDIS_REPLY_ARRAY) {
	pmsprintf(msg, sizeof(msg),
		"expected array from pcp:map:inst.name HGETALL (type=%s)",
		redis_reply(reply->type));
	queryinfo(settings, PMSERIES_RESPONSE, msg, arg);
	sts = -EPROTO;
    } else {
	*rp = reply;
	/* unpack - produce reverse map of ids-to-names for each entry */
	sts = reverse_map(settings, reply->elements, reply->element, mp, arg);
    }
    return sts;
}

static int
series_inst_reply(pmSeriesSettings *settings, pmSeriesID *series,
	void *arg, int nelements, redisReply **elements, reverseMap *map)
{
    reverseMapNode	node;
    unsigned int	id, mapid;
    char		msg[MSGSIZE];
    char		*name;
    int			sts = 0;

/*--printf("    \"%.*s\" : {\n", PMSIDSZ, series->name);--*TODO*/

    if (nelements == 2) {
	if (elements[0]->type == REDIS_REPLY_NIL) {
	    queryinst(settings, series, PM_IN_NULL, NULL, arg);
	} else if (extract_number(settings, series, elements[0], &id,
				"instance ID", arg) < 0) {
	    sts = -EINVAL;
	} else if (extract_number(settings, series, elements[1], &mapid,
				"instance name map", arg) < 0) {
	    sts = -EINVAL;
	} else if ((node = __pmHashSearch(mapid, map)) != NULL) {
	    name = (char *)node->data;
/*--	    printf("      \"inst\": %u,\n", id);
	    printf("      \"name\": \"%s\"\n", name);--*TODO*/
	    queryinst(settings, series, id, name, arg);
	} else {
	    pmsprintf(msg, sizeof(msg), "%.*s - timeseries instance map",
			PMSIDSZ, series->name);
	    queryinfo(settings, PMSERIES_CORRUPT, msg, arg);
	    sts = -EINVAL;
	}
    } else if (nelements == 1) {
	pmsprintf(msg, sizeof(msg), "%.*s - timeseries instance entry",
			PMSIDSZ, series->name);
	queryinfo(settings, PMSERIES_CORRUPT, msg, arg);
	sts = -EINVAL;
    } else if (nelements == 0) {
	queryinst(settings, series, PM_IN_NULL, NULL, arg);
    }

/*--fputs("    }", stdout);--*TODO*/
    return sts;
}

void
pmSeriesInstance(pmSeriesSettings *settings,
	int nseries, pmSeriesID *series, void *arg)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    char		*cmd;
    char		msg[MSGSIZE];
    int			i, len, sts;

    if (nseries <= 0) {
	if (nseries == 0)
	    sts = PM_ERR_NYI;	/* TODO: "all" instances interface */
	else
	    sts = -EINVAL;
	goto done;
    }
    if ((sts = series_inst_name_prepare(settings, redis, arg)) < 0)
	goto done;
    if ((sts = series_inst_name_execute(settings, redis, &rp, &map, arg)) < 0)
	goto done;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "HMGET pcp:inst:series:%s id name",
			series[i].name);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "failed pcp:inst:series:%.*s HMGET",
			PMSIDSZ, series[i].name);
	    queryinfo(settings, PMSERIES_REQUEST, msg, arg);
	    sts = -EINVAL;
	}
	free(cmd);
    }
    if (sts < 0)
	goto done;

    /* response handling */

/*--printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");--*TODO*/

    /* unpack - iterate over series and extract instance (if any) for each */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    pmsprintf(msg, sizeof(msg), "failed pcp:inst:series:%.*s HMGET",
			PMSIDSZ, series[i].name);
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EAGAIN;
	} else if (reply->type != REDIS_REPLY_ARRAY) {
	    pmsprintf(msg, sizeof(msg),
			"expected array from series %.*s HMGET (type=%s)",
			PMSIDSZ, series[i].name, redis_reply(reply->type));
	    settings->on_info(PMSERIES_RESPONSE, msg, arg);
	    sts = -EPROTO;
	} else {
	    sts = series_inst_reply(settings, &series[i], arg,
				reply->elements, reply->element, &map);
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
