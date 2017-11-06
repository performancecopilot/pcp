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
#include "load.h"
#include "util.h"
#include "redis.h"
#include "series.h"

typedef struct {
    redisContext	*redis;
    unsigned int	index;
    unsigned int	count;
    redisReply		**replies;
} SOLVER;

typedef __pmHashCtl reverseMap;
typedef __pmHashNode *reverseMapNode;

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

char *
series_note_name(char *key, size_t length)
{
    if (length > sizeof("note.") &&
	strncmp(key, "note.", sizeof("note.") - 1) == 0)
	return key + sizeof("note.") - 1;
    if (length > sizeof("n.") &&
	strncmp(key, "n.", sizeof("n.") - 1) == 0)
	return key + sizeof("n.") - 1;
    return NULL;
}

const char *
node_subtype(node_t *np)
{
    switch (np->subtype) {
	case N_METRIC:	return "metric";
	case N_INSTANCE: return "inst";
	case N_LABEL: return "label";
	case N_NOTE: return "note";
	default: break;
    }
    return NULL;
}

/*
 * Report a timeseries result - value + timestamp (score) pairs
 */
static int
series_values_reply(char *name, int nelements, redisReply **elements)
{
    redisReply	*value;
    redisReply	*score;
    int		i;

    if (nelements <= 0)
	return nelements;
    if (nelements % 2)	/* expect (value, timestamp) pairs */
	return -EINVAL;

    printf("    \"%.*s\" : {\n", HASHSIZE, name);

    for (i = 0; i < nelements; i += 2) {
	value = elements[i];
	score = elements[i+1];
	if (value->type == REDIS_REPLY_STRING &&
	    score->type == REDIS_REPLY_STRING) {
	    printf("      \"%.*s\": \"%s\"", 15, score->str, value->str);
	} else {
	    fprintf(stderr, "Bad response for series %s value - "
			    "value=%d or score=%d non-string (%d)\n",
		    name, value->type, score->type, REDIS_REPLY_STRING);
	    return -EINVAL;
	}
	if (i + 2 < nelements)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("    }", stdout);
    return nelements;
}

/*
 * Save the series hash identifiers contained in a Redis response.
 * Used at the leaves of the query tree, then merged result sets
 * are propagated upward.
 */
static int
node_series_reply(node_t *np, int nelements, redisReply **elements)
{
    redisReply	*series;
    char	*sp;
    int		i;

    if ((np->nseries = nelements) <= 0)
	return nelements;

    np->series = malloc(nelements * HASHSIZE);
    sp = (char *)np->series;

    for (i = 0; i < nelements; i++) {
	series = elements[i];
	if (series->type == REDIS_REPLY_STRING) {
	    if (pmDebugOptions.series)
	        printf("    %s\n", series->str);
	    memcpy(sp, series->str, HASHSIZE);
	    sp += HASHSIZE;
	} else {
	    fprintf(stderr, "Bad response in %s set \"%s\" (%d)\n",
		    node_subtype(np->left), np->left->key, series->type);
	    free(np->series);
	    np->series = NULL;
	    np->nseries = 0;
	    return -EINVAL;
	}
    }
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
    return memcmp(a, b, HASHSIZE);
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

    qsort(large, nlarge, HASHSIZE, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += HASHSIZE) {
	if (bsearch(cp, large, nlarge, HASHSIZE, series_compare) == NULL)
	    continue;		/* no match, continue advancing cp only */
	if (saved != cp)
	    memcpy(saved, cp, HASHSIZE);
	saved += HASHSIZE;	/* stashed, advance cp & saved pointers */
    }

    if ((total = (saved - small) / HASHSIZE) < nsmall) {
	/* shrink the smaller set down further */
	if ((small = realloc(small, total * HASHSIZE)) == NULL)
	    return -ENOMEM;
    }

    np->nseries = total;
    np->series = small;

    if (pmDebugOptions.series) {
	printf("Intersect result set contains %d series:\n", total);
	for (i = 0, cp = small; i < total; cp += HASHSIZE, i++)
	    printf("    %.*s\n", HASHSIZE, cp);
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

    qsort(large, nlarge, HASHSIZE, series_compare);

    for (i = 0, cp = saved = small; i < nsmall; i++, cp += HASHSIZE) {
	if (bsearch(cp, large, nlarge, HASHSIZE, series_compare) != NULL)
	    continue;		/* already present, no need to save */
	if (saved != cp)
	    memcpy(saved, cp, HASHSIZE);
	saved += HASHSIZE;	/* stashed, advance both cp & saved */
    }

    if ((need = (saved - small) / HASHSIZE) > 0) {
	/* grow the larger set to cater for new entries, then add 'em */
	if ((large = realloc(large, (nlarge + need) * HASHSIZE)) == NULL)
	    return -ENOMEM;
	cp = large + (nlarge * HASHSIZE);
	memcpy(cp, small, need * HASHSIZE);
	total = nlarge + need;
    } else {
	total = nlarge;
    }

    np->nseries = total;
    np->series = large;

    if (pmDebugOptions.series) {
	printf("Union result set contains %d series:\n", total);
	for (i = 0, cp = large; i < total; cp += HASHSIZE, i++)
	    printf("    %.*s\n", HASHSIZE, cp);
    }

    free_child_series(left, right);
    return total;
}

/*
 * Map human names to internal redis identifiers.
 */
static void
series_prepare_maps(SOLVER *sp, node_t *np, int level)
{
    int		length;
    char	*name, *cmd;

    if (np == NULL)
	return;

    series_prepare_maps(sp, np->left, level+1);

    switch (np->type) {
    case N_STRING:
	// may need string quoting with regex escapes? (for MATCH)
	// - or should we rewrite this earlier perhaps?  (see also KEYS docs)
	break;

    case N_NAME:
	// setup any label/note name map identifiers needed by direct children
	length = strlen(np->value);
	if ((name = series_instance_name(np->value, length)) != NULL) {
	    np->subtype = N_INSTANCE;
	    np->key = strdup("pcp:map:inst.name");
	}
	else if ((name = series_metric_name(np->value, length)) != NULL) {
	    np->subtype = N_METRIC;
	    np->key = strdup("pcp:map:metric.name");
	}
	else if ((name = series_note_name(np->value, length)) != NULL) {
	    np->subtype = N_NOTE;
	    length = redisFormatCommand(&cmd, "HGET pcp:map:note.name %s", name);
	    if (redisAppendFormattedCommand(sp->redis, cmd, length) != REDIS_OK) {
		fprintf(stderr, "Failed to setup note name lookup command\n");
		/* TODO: error handling */
		exit(1);
	    }
	    sp->count++;
	    free(cmd);
	}
	else {
	    np->subtype = N_LABEL;
	    if ((name = series_label_name(np->value, length)) == NULL)
		name = np->value;
	    length = redisFormatCommand(&cmd, "HGET pcp:map:label.name %s", name);
	    if (redisAppendFormattedCommand(sp->redis, cmd, length) != REDIS_OK) {
		fprintf(stderr, "Failed to setup label name lookup command\n");
		/* TODO: error handling */
		exit(1);
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

    series_prepare_maps(sp, np->right, level+1);
}

/*
 * Find the sets of series corresponding to leaf search parameters.
 */
static void
series_resolve_maps(SOLVER *sp, node_t *np, int level)
{
    redisReply	*reply;
    char	id[64];

    if (np == NULL)
	return;

    series_resolve_maps(sp, np->left, level+1);

    switch (np->type) {
    case N_NAME:
	/* setup any label or note name map identifiers needed */
	/* TODO: need to handle JSONB label/note name nesting. */

	if (np->subtype == N_NOTE) {
	    if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
		fprintf(stderr, "No %s named \"%s\" found (%d)\n",
			node_subtype(np), np->value, sp->redis->err);
		/* TODO: improve error handling */
		exit(1);
	    }
	    if (reply->type != REDIS_REPLY_STRING) {
		fprintf(stderr, "Bad response to request for %s map \"%s\" (%d)\n",
			node_subtype(np), np->value, reply->type);
		/* TODO: improve error handling */
		exit(1);
	    }
	    sp->replies[sp->index++] = reply;
	    pmsprintf(id, sizeof(id), "pcp:map:%s.%s.value",
			node_subtype(np), reply->str);
	    np->key = strdup(id);
	}
	else if (np->subtype == N_LABEL) {
	    if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
		fprintf(stderr, "No %s named \"%s\" found (%d)\n",
			node_subtype(np), np->value, sp->redis->err);
		/* TODO: improve error handling */
		exit(1);
	    }
	    if (reply->type != REDIS_REPLY_STRING) {
		fprintf(stderr, "Bad response to request for %s map \"%s\" (%d)\n",
			node_subtype(np), np->value, reply->type);
		/* TODO: improve error handling */
		exit(1);
	    }
	    sp->replies[sp->index++] = reply;
	    pmsprintf(id, sizeof(id), "pcp:map:%s.%s.value",
			node_subtype(np), reply->str);
	    np->key = strdup(id);
	}
	break;

    case N_LT:  case N_LEQ: case N_EQ:  case N_GEQ: case N_GT:  case N_NEQ:
    case N_AND: case N_OR:  case N_RNE: case N_REQ: case N_NEG:
    default:
	break;
    }

    series_resolve_maps(sp, np->right, level+1);
}

/*
 * Prepare evaluation of leaf nodes.
 */
static void
series_prepare_eval(SOLVER *sp, node_t *np, int level)
{
    int		length;
    char	*key, *value, *cmd;

    if (np == NULL)
	return;

    series_prepare_eval(sp, np->left, level+1);

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	key = np->left->key;
	value = np->right->value;
	length = redisFormatCommand(&cmd, "HGET %s %s", key, value);
	if (redisAppendFormattedCommand(sp->redis, cmd, length) != REDIS_OK) {
	    fprintf(stderr, "Failed to setup equality test on %s:%s (key %s)\n",
			np->left->value, value, key);
		/* TODO: error handling */
		exit(1);
	}
	sp->count++;
	free(cmd);
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }

    series_prepare_eval(sp, np->right, level+1);
}

static void
series_resolve_eval(SOLVER *sp, node_t *np, int level)
{
    redisReply	*reply;
    node_t	*left;
    char	*name;
    char	id[64];

    if (np == NULL)
	return;

    series_resolve_eval(sp, np->left, level+1);

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	left = np->left;
	name = left->key + sizeof("pcp:map:") - 1;

	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "Map table %s key %s not found\n",
			left->key, np->right->value);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_STRING) {
	    fprintf(stderr, "Bad response to request for %s key \"%s\" (%d)\n",
			node_subtype(left), left->key, reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	sp->replies[sp->index++] = reply;
	pmsprintf(id, sizeof(id), "pcp:series:%s:%s", name, reply->str);
	if (np->key)	/* TODO: is this ever true? */
	    free(np->key);
	np->key = strdup(id);
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }

    series_resolve_eval(sp, np->right, level+1);
}

/*
 * Prepare evaluation of internal nodes.
 */
static void
series_prepare_expr(SOLVER *sp, node_t *np, int level)
{
    int		length;
    char	*cmd;

    if (np == NULL)
	return;

    series_prepare_expr(sp, np->left, level+1);
    series_prepare_expr(sp, np->right, level+1);

    switch (np->type) {
    case N_EQ:	/* direct hash lookup */
	length = redisFormatCommand(&cmd, "SMEMBERS %s", np->key);
	if (redisAppendFormattedCommand(sp->redis, cmd, length) != REDIS_OK) {
	    fprintf(stderr, "Failed to setup smembers expr on (key %s)\n",
			np->key);
		/* TODO: error handling */
		exit(1);
	}
	sp->count++;
	free(cmd);
	break;

    case N_LT:  case N_LEQ: case N_GEQ: case N_GT:  case N_NEQ:
    case N_RNE: case N_REQ: case N_NEG:
    case N_AND: case N_OR:
	/* TODO */

    default:
	break;
    }
}

static void
series_resolve_expr(SOLVER *sp, node_t *np, int level)
{
    redisReply	*reply;

    if (np == NULL)
	return;

    /* evaluate leaves first then interior nodes */
    series_resolve_expr(sp, np->left, level+1);
    series_resolve_expr(sp, np->right, level+1);

    switch (np->type) {
    case N_EQ:
	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "Map table %s key %s not found\n",
			np->left->key, np->right->value);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Bad response to request for %s set \"%s\" (%d)\n",
			node_subtype(np->left), np->left->key, reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	sp->replies[sp->index++] = reply;
	if (pmDebugOptions.series)
	    printf("%s %s\n", node_subtype(np->left), np->key);
	node_series_reply(np, reply->elements, reply->element);
	/* TODO: improve error handling */
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
}

#define DEFAULT_VALUE_COUNT 10

static void
series_prepare_time(SOLVER *sp, timing_t *tp, int nseries, char *series)
{
    char	start[64], end[64];
    char	name[44];
    char	*cmd;
    int		len;
    int		i;

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

    for (i = 0; i < nseries; i++, series += HASHSIZE) {
	pmsprintf(name, sizeof(name), "%.*s", HASHSIZE, series);

	/* ZREVRANGEBYSCORE key max min [WITHSCORES] [LIMIT offset count] */
	len = redisFormatCommand(&cmd, "ZREVRANGEBYSCORE"
		" pcp:values:series:%s %s %s WITHSCORES LIMIT %u %u",
		name, end, start, tp->offset, tp->count);

	if (redisAppendFormattedCommand(sp->redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed pcp:values:series:%.*s zrange setup",
		    HASHSIZE, series);
	    /* TODO: error handling */
	    exit(1);
	}
	sp->count++;
	free(cmd);
    }
}

static const char *
series_result(int nseries)
{
    if (nseries == 0)
	return "empty";
    if (nseries == 1)
	return "single";
    return "vector";
}

static void
series_resolve_time(SOLVER *sp, int nseries, char *series)
{
    redisReply	*reply;
    int		i;

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");

    for (i = 0; i < nseries; i++, series += HASHSIZE) {
	if (redisGetReply(sp->redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "zset series %.*s query failed\n",
			HASHSIZE, series);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Non-array response from %.*s zset values (%d)\n",
			HASHSIZE, series, reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}

	series_values_reply(series, reply->elements, reply->element);
	freeReplyObject(reply);

	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  }\n}\n", stdout);
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

static void
series_report_set(int nseries, char *series)
{
    int		i;

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": [\n");
    for (i = 0; i < nseries; series += HASHSIZE, i++) {
	printf("    \"%.*s\"", HASHSIZE, series);
	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  ]\n}\n", stdout);

}

static int
series_time_window(timing_t *tp)
{
    if (tp->ranges || tp->starts || tp->ends || tp->counts)
	return 1;
    return 0;
}

int
series_solve(node_t *root, timing_t *timing /*, callback, arg */)
{
    SOLVER	solver = { 0 }, *sp = &solver;

    solver.redis = redis_init();
    /* solver.callback = callback; */
    /* solver.arg = arg; */

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
    if (!series_time_window(timing)) {
	series_report_set(root->nseries, root->series);
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

static int
series_split(const char *string, char ***series)
{
    char	*sp, *split, *saved, **rp, **result;
    int		nseries = 0;

    if ((split = saved = strdup(string)) == NULL)
	return -ENOMEM;

    while ((sp = strsep(&split, ", ")) != NULL) {
	if (*sp)
	    nseries++;
    }
    strcpy(split = saved, string);

    if (nseries == 0) {
	free(split);
	return 0;
    }
    if ((result = rp = (char **)calloc(nseries, sizeof(char *))) == NULL) {
	free(split);
	return -ENOMEM;
    }
    while ((sp = strsep(&split, ", ")) != NULL) {
	if (*sp)
	    *rp++ = sp;
    }
    *series = result;
    return nseries;
}

static void
series_free(int nseries, char **series)
{
    if (nseries > 0) {
	free(*series);
	free(series);
    }
}

/* build a reverse hash mapping */
static int
reverse_map(int nkeys, redisReply **elements, reverseMap *map)
{
    redisReply	*name, *key;
    int		i;

    __pmHashInit(map);
    for (i = 0; i < nkeys; i += 2) {
	name = elements[i];
	key = elements[i+1];
	if (name->type == REDIS_REPLY_STRING && key->type == name->type) {
	    __pmHashAdd(atoi(key->str), name->str, map);
	} else {
	    fprintf(stderr, "Non-string key/name (%d/%d) building hashmap\n",
		    key->type, name->type);
	    return -EINVAL;
	}
    }
    return nkeys;
}

/*
 * Produce the list of label names (requires reverse mapping from IDs)
 */
static int
series_map_reply(char *series, int nelements, redisReply **elements, reverseMap *map)
{
    redisReply		*id;
    reverseMapNode	node;
    char		*name;
    int			i;

    printf("    \"%.*s\" : [", HASHSIZE, series);
    for (i = 0; i < nelements; i++) {
	id = elements[i];
	if (id->type == REDIS_REPLY_STRING) {
	    if ((node = __pmHashSearch(atoi(id->str), map)) == NULL)
		continue;	/* corrupt - needs fsck */
	    name = (char *)node->data;
//	    if (pmDebugOptions.series)
	        printf(" \"%s\"", name);
	} else {
	    fprintf(stderr, "Bad response in %s set (%d)\n", series, id->type);
	    return -EINVAL;
	}
	if (i < nelements - 1)
	    fputc(',', stdout);
    }
    fputs(" ]", stdout);
    return nelements;
}

static int
series_label(int nseries, char **series, int optional)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    const char		*type = optional ? "note" : "label";
    char		*cmd;
    int			i, len, sts;

    /* prepare command batch */

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:%s.name", type);
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	fprintf(stderr, "Failed pcp:map:%s.name HGETALL", type);
	/* TODO: error handling */
	exit(1);
    }
    free(cmd);

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "SMEMBERS pcp:%s.name:series:%s",
				optional ? "note" : "label", series[i]);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed pcp:%s.name:series:%s smembers",
		    optional ? "note" : "label", series[i]);
	    /* TODO: error handling */
	    exit(1);
	}
	free(cmd);
    }

    /* response handling */

    if (redisGetReply(redis, (void **)&rp) != REDIS_OK) {
	fprintf(stderr, "HGETALL pcp:map:%s.name failed\n", type);
	    /* TODO: improve error handling */
	    exit(1);
    }
    if (rp->type != REDIS_REPLY_ARRAY) {
	fprintf(stderr, "Non-array response from pcp:map:%s HGETALL (%d)\n",
			type, rp->type);
	/* TODO: improve error handling */
	exit(1);
    }

    /* unpack - produce reverse hash map of ids -> names for each label */
    if ((sts = reverse_map(rp->elements, rp->element, &map)) < 0)
	return sts;

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");

    /* unpack - iterate over series and extract label names for each set */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "SMEMBERS series %.*s query failed\n",
			HASHSIZE, series[i]);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Non-array response from %.*s set members (%d)\n",
			HASHSIZE, series[i], reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	series_map_reply(series[i], reply->elements, reply->element, &map);
	freeReplyObject(reply);

	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  }\n}\n", stdout);

    freeReplyObject(rp);
    return 0;
}

static int
series_all_labels(int optional)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    const char		*type = optional ? "note" : "label";
    char		*cmd;
    int			i, len;

    /* prepare command */

    len = redisFormatCommand(&cmd, "HKEYS pcp:map:%s.name", type);
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	fprintf(stderr, "Failed pcp:map:%s.name hkeys", type);
	/* TODO: error handling */
	exit(1);
    }
    free(cmd);

    /* response handling */

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	fprintf(stderr, "HKEYS pcp:map:%s.name failed\n", type);
	    /* TODO: improve error handling */
	    exit(1);
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	fprintf(stderr, "Non-array response from pcp:map:%s hkeys (%d)\n",
			type, reply->type);
	/* TODO: improve error handling */
	exit(1);
    }

    printf("[");
    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
//	    if (pmDebugOptions.series)
	        printf(" \"%s\"", rp->str);
	} else {
	    fprintf(stderr, "Bad response in %s hget (%d)\n", type, rp->type);
	    return -EINVAL;
	}
	if (i + 1 < reply->elements)
	    fputc(',', stdout);
	freeReplyObject(rp);
    }
    printf(" ]\n");

    return 0;
}

static int
series_all_metrics(void)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    char		*cmd;
    int			i, len;

    /* prepare command */

    len = redisFormatCommand(&cmd, "HKEYS pcp:map:metric.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	fprintf(stderr, "Failed pcp:map:metric.name hkeys");
	/* TODO: error handling */
	exit(1);
    }
    free(cmd);

    /* response handling */

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	fprintf(stderr, "HKEYS pcp:map:metric.name failed\n");
	    /* TODO: improve error handling */
	    exit(1);
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	fprintf(stderr, "Non-array response from pcp:map:metric HKEYS (%d)\n",
			reply->type);
	/* TODO: improve error handling */
	exit(1);
    }

    printf("[");
    for (i = 0; i < reply->elements; i++) {
	rp = reply->element[i];
	if (rp->type == REDIS_REPLY_STRING) {
//	    if (pmDebugOptions.series)
	        printf(" \"%s\"", rp->str);
	} else {
	    fprintf(stderr, "Bad response in metrics HGET (%d)\n", rp->type);
	    return -EINVAL;
	}
	if (i + 1 < reply->elements)
	    fputc(',', stdout);
	freeReplyObject(rp);
    }
    printf(" ]\n");

    return 0;
}

static int
series_metric_name_prepare(redisContext *redis)
{
    char		*cmd;
    int			len;

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:metric.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	fprintf(stderr, "Failed pcp:map:metric.name HGETALL");
	/* TODO: error handling */
	exit(1);
    }
    free(cmd);
    return 0;
}

static int
series_metric_name_execute(redisContext *redis, redisReply **rp, reverseMap *mp)
{
    redisReply		*reply;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	fprintf(stderr, "HGETALL pcp:map:metric.name failed\n");
	/* TODO: improve error handling */
	exit(1);
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	fprintf(stderr,
		"Non-array response from pcp:map:metric.name HGETALL (%d)\n",
		reply->type);
	/* TODO: improve error handling */
	exit(1);
    }
    *rp = reply;

    /* unpack - produce reverse hash map of ids -> names for each label */
    return reverse_map(reply->elements, reply->element, mp);
}

static int
series_metric(int nseries, char **series)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    char		*cmd;
    int			i, len, sts;

    series_metric_name_prepare(redis);
    if ((sts = series_metric_name_execute(redis, &rp, &map)) < 0)
	return sts;

    /* prepare command series */
    /* SMEMBERS pcp:metric.name:series:%s */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "SMEMBERS pcp:metric.name:series:%s",
				series[i]);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed pcp:metric.name:series:%s smembers",
		    series[i]);
	    /* TODO: error handling */
	    exit(1);
	}
	free(cmd);
    }

    /* response handling */

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");

    /* unpack - iterate over series and extract names for each via map */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "SMEMBERS series %.*s query failed\n",
			HASHSIZE, series[i]);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Non-array response from %.*s set members (%d)\n",
			HASHSIZE, series[i], reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	series_map_reply(series[i], reply->elements, reply->element, &map);
	freeReplyObject(reply);

	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  }\n}\n", stdout);

    freeReplyObject(rp);
    return 0;
}

static int
extract_number(redisReply *element, unsigned int *response)
{
    char	*endnum;
    if (element->type != REDIS_REPLY_STRING)
	return -EINVAL;
    *response = strtoul(element->str, &endnum, 10);
    if (*endnum != '\0')
	return -EINVAL;
    return 0;
}

static int
extract_string(redisReply *element, char **response)
{
    if (element->type != REDIS_REPLY_STRING)
	return -EINVAL;
    *response = element->str;
    return 0;
}

static char *
lower(char *string)
{
    char	*p;
    for (p = string; *p; p++)
	*p = tolower(*p);
    return string;
}

static int
series_desc_reply(const char *series, int nelements, redisReply **elements)
{
    unsigned int	domain, cluster, item, serial;
    unsigned int	sem, type;
    char		*units;
    pmID		pmid;
    pmInDom		indom;
    char		buf[64];

    if (nelements != 7) {
	fprintf(stderr, "Bad reply from %s HMGET (%d)\n", series, nelements);
	return -EINVAL;
    }

    printf("    \"%.*s\" : {\n", HASHSIZE, series);

    extract_number(elements[0], &domain);
    extract_number(elements[1], &cluster);
    extract_number(elements[2], &item);
    pmid = pmID_build(domain, cluster, item);
    printf("      \"pmid\": \"%s\",\n", pmIDStr_r(pmid, buf, 64));

    if (extract_number(elements[3], &serial) == 0) {
	indom = pmInDom_build(domain, serial);
	printf("      \"indom\": \"%s\",\n", pmInDomStr_r(indom, buf, 64));
    }

    extract_number(elements[4], &sem);
    printf("      \"semantics\": \"%s\",\n", pmSemStr_r(sem, buf, 64));
    extract_number(elements[5], &type);
    printf("      \"type\": \"%s\",\n", lower(pmTypeStr_r(type, buf, 64)));
    if (extract_string(elements[6], &units) != 0)
	units = "none";
    printf("      \"units\": \"%s\"\n", units);

    fputs("    }", stdout);
    return 0;
}

static int
series_desc(int nseries, char **series)
{
    redisContext	*redis = redis_init();
    redisReply		*reply;
    char		*cmd;
    int			i, len;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "HMGET pcp:desc:series:%s "
			"domain cluster item serial semantics type units",
			series[i]);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed pcp:desc:series:%s HMGET",
		    series[i]);
	    /* TODO: error handling */
	    exit(1);
	}
	free(cmd);
    }

    /* response handling */

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");

    /* unpack - iterate over series and extract descriptor for each */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "HMGET series %.*s query failed\n",
			HASHSIZE, series[i]);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Non-array response from %.*s HMGET (%d)\n",
			HASHSIZE, series[i], reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	series_desc_reply(series[i], reply->elements, reply->element);
	freeReplyObject(reply);

	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  }\n}\n", stdout);

    return 0;
}

static int
series_inst_name_prepare(redisContext *redis)
{
    char		*cmd;
    int			len;

    len = redisFormatCommand(&cmd, "HGETALL pcp:map:inst.name");
    if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	fprintf(stderr, "Failed pcp:map:inst.name HGETALL");
	/* TODO: error handling */
	exit(1);
    }
    free(cmd);
    return 0;
}

static int
series_inst_name_execute(redisContext *redis, redisReply **rp, reverseMap *mp)
{
    redisReply		*reply;

    if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	fprintf(stderr, "HGETALL pcp:map:inst.name failed\n");
	/* TODO: improve error handling */
	exit(1);
    }
    if (reply->type != REDIS_REPLY_ARRAY) {
	fprintf(stderr,
		"Non-array response from pcp:map:inst.name HGETALL (%d)\n",
		reply->type);
	/* TODO: improve error handling */
	exit(1);
    }
    *rp = reply;

    /* unpack - produce reverse hash map of ids -> names for each entry */
    return reverse_map(reply->elements, reply->element, mp);
}

static int
series_inst_reply(const char *series, int nelements, redisReply **elements, reverseMap *map)
{
    reverseMapNode	node;
    unsigned int	id, mapid;
    char		*name;

    printf("    \"%.*s\" : {\n", HASHSIZE, series);

    if (nelements == 2) {
	extract_number(elements[0], &id);
	extract_number(elements[1], &mapid);
	if ((node = __pmHashSearch(mapid, map)) != NULL) {
	    name = (char *)node->data;
	    printf("      \"inst\": %u,\n", id);
	    printf("      \"name\": \"%s\"\n", name);
	}
	/* else - corrupt - needs fsck */
    }
    /* else - PM_INDOM_NULL */

    fputs("    }", stdout);
    return 0;
}

static int
series_instance(int nseries, char **series)
{
    redisContext	*redis = redis_init();
    redisReply		*reply, *rp;
    reverseMap		map;
    char		*cmd;
    int			i, len, sts;

    series_inst_name_prepare(redis);
    if ((sts = series_inst_name_execute(redis, &rp, &map)) < 0)
	return sts;

    /* prepare command series */

    for (i = 0; i < nseries; i++) {
	len = redisFormatCommand(&cmd, "HMGET pcp:inst:series:%s id name",
			series[i]);
	if (redisAppendFormattedCommand(redis, cmd, len) != REDIS_OK) {
	    fprintf(stderr, "Failed pcp:inst:series:%s HMGET",
		    series[i]);
	    /* TODO: error handling */
	    exit(1);
	}
	free(cmd);
    }

    /* response handling */

    printf("{\n  \"result\": \"%s\",\n", series_result(nseries));
    printf("  \"series\": {\n");

    /* unpack - iterate over series and extract instance (if any) for each */
    for (i = 0; i < nseries; i++) {
	if (redisGetReply(redis, (void **)&reply) != REDIS_OK) {
	    fprintf(stderr, "HMGET series %.*s query failed\n",
			HASHSIZE, series[i]);
	    /* TODO: improve error handling */
	    exit(1);
	}
	if (reply->type != REDIS_REPLY_ARRAY) {
	    fprintf(stderr, "Non-array response from %.*s HMGET (%d)\n",
			HASHSIZE, series[i], reply->type);
	    /* TODO: improve error handling */
	    exit(1);
	}
	series_inst_reply(series[i], reply->elements, reply->element, &map);
	freeReplyObject(reply);

	if (i + 1 < nseries)
	    fputc(',', stdout);
	fputc('\n', stdout);
    }
    fputs("  }\n}\n", stdout);

    freeReplyObject(rp);
    return 0;
}

int
series_module(const char *command, const char *query /*, cb, arg */)
{
    char	**series;
    int		nseries;
    int		sts;

    if (strcmp(command, "desc") == 0) {
	nseries = series_split(query, &series);
	sts = series_desc(nseries, series /*, cb, arg */);
	series_free(nseries, series);
	return sts;
    }

    if (strcmp(command, "instance") == 0) {
	nseries = series_split(query, &series);
	sts = series_instance(nseries, series /*, cb, arg */);
	series_free(nseries, series);
	return sts;
    }

    if (strcmp(command, "query") == 0)
	return series_query(query /*, cb, arg */);

    if (strcmp(command, "labels") == 0) {
	if (query == NULL || *query == '\0')
	    return series_all_labels(0 /*, cb, arg */);
	nseries = series_split(query, &series);
	sts = series_label(nseries, series, 0 /*, cb, arg */);
	series_free(nseries, series);
	return sts;
    }

    if (strcmp(command, "load") == 0)
	return series_load(query, 0 /*, cb, arg */);

    if (strcmp(command, "loadmeta") == 0)
	return series_load(query, 1 /*, cb, arg */);

    if (strcmp(command, "metrics") == 0) {
	if (query == NULL || *query == '\0')
	    return series_all_metrics(/*, cb, arg */);
	nseries = series_split(query, &series);
	sts = series_metric(nseries, series /*, cb, arg */);
	series_free(nseries, series);
	return sts;
    }

    if (strcmp(command, "notes") == 0) {
	if (query == NULL || *query == '\0')
	    return series_all_labels(1 /*, cb, arg */);
	nseries = series_split(query, &series);
	sts = series_label(nseries, series, 1 /*, cb, arg */);
	series_free(nseries, series);
	return sts;
    }

    return -EOPNOTSUPP;
}

/* TODO: entry point - move to a library (libpcp? libpcp_web?) */
int
pmSeries(const char *engine, const char *query /*, cb, arg */)
{
    const char	module[] = "series/";
    const char	*command;
    int		sts;

    /* TODO: allow addition of new engines and commands */
    /* (hard-code it for now while prototyping things). */
    if (strncmp(engine, module, strlen(module)) != 0)
	return -EOPNOTSUPP;
    command = engine + strlen(module);

    /* TODO: refactor into loadable redis/other modules */
    sts = series_module(command, query /*, cb, arg */);

    /* TODO: generalise the response handling (cb, arg) */
    return sts;
}
