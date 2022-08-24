/*
 * Copyright (c) 2016-2020,2022 Red Hat.
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
#include "pmda.h"
#include "internal.h"
#include <jsonsl/jsonsl.h>
#include "sha256.h"
#include "sort_r.h"
#include "fault.h"

#define MAXLABELSET		(PM_MAXLABELS * 6)	/* 6 hierarchy levels */
#define INC_TOKENS		64	/* parser tokens allocation increment */

static int __pmMergeLabels(const char *, const char *, char *, int, int);
static int __pmParseLabels(const char *, int, pmLabel *, int, __pmHashCtl *, int, char *, int *);

typedef int (*filter_labels)(const pmLabel *, const char *, void *);
static int __pmMergeLabelSets(pmLabel *, const char *, __pmHashCtl *, int,
		pmLabel *, const char *, __pmHashCtl *, int,
		pmLabel *, char *, int *, int, filter_labels, void *);

/*
 * Parse JSONB labels string, building up the array index as we go.
 * This simplified format is parsed:
 * {
 *    "name1": value,
 *    "name2": [1,2],
 *    "name3": "str",
 *    "name4": {"a":1,"b":"2"},
 *    [...]
 * }
 *
 * As we parse the original string, we remove any whitespace and
 * produce "clean" JSONB form into the given buffer.  The labels
 * array indexes into this sanitized buffer.
 *
 * We manage label name parsing using a 'stack' to help with any
 * compound JSON structures (maps, which can contain maps, ...)
 *   e.g.  { "a": 1, "b": {"c": 3, "d": [4,5]} }
 * This string contains three labels - "a", "b.c" and "b.d".
 */

enum parse_state { PARSE_START, PARSE_NAME, PARSE_VALUE,
		   PARSE_ARRAY_VALUE, PARSE_OBJECT_VALUE };
enum stack_style { STACK_DIRECT, STACK_COMPOUND };

typedef struct parser {
    enum parse_state	state;
    int			error;
    unsigned int	nlabels;
    unsigned int	maxlabels;
    __pmHashCtl		*compound;	/* offset-keyed compound-name hash */
    pmLabel		*labels;
    char		*buffer;	/* current offset in output buffer */
    unsigned int	buflen;		/* remaining bytes in output buffer */
    unsigned int	comma;
    const char		*start;		/* start of input buffer */
    const char		*token;		/* start of name/value (input buffer) */
    const char		*array;		/* start of list/array (input buffer) */
    const char		*jsonb;		/* start of output buffer */
    unsigned int	name;		/* active label name (output buffer) */
    unsigned int	namelen;	/* active name length (output buffer) */
    unsigned int	namelevel;	/* active name jsonsl-parsing-level */
    enum stack_style	stackstyle;	/* direct/compound name style */
    unsigned int	stackdepth;	/* compound name depth */
    unsigned int	stacklen;	/* length of compound name 'stack' */
    unsigned int	stackseq;	/* number of 'stack' name changes */
    unsigned int	labelflags;	/* new flags (compound/optional) */
    char		*stack;		/* accumulated compound label name */
} parser_t;

#define MAX_RECURSION_DEPTH	256
#define MAX_DESCENT_LEVEL	64

static const char *
action_string(jsonsl_action_t action)
{
    if (action == JSONSL_ACTION_PUSH) return "push";
    if (action == JSONSL_ACTION_POP) return "pop";
    if (action == JSONSL_ACTION_ERROR) return "error";
    if (action == JSONSL_ACTION_UESCAPE) return "uescape";
    return "?";
}

static const char *
state_string(struct jsonsl_state_st *state)
{
    if (state->type == JSONSL_T_STRING) return "string";
    if (state->type == JSONSL_T_HKEY) return "hkey";
    if (state->type == JSONSL_T_OBJECT) return "object";
    if (state->type == JSONSL_T_LIST) return "list";
    if (state->type == JSONSL_T_SPECIAL) return "special";
    if (state->type == JSONSL_T_UESCAPE) return "uescape";
    return "?";
}

static const char *
label_name(const pmLabel *lp, const char *json, __pmHashCtl *lc)
{
    __pmHashNode	*hp;

    if (!(lp->flags & PM_LABEL_COMPOUND) || lc == NULL ||
	(hp = __pmHashSearch(lp->name, lc)) == NULL)
	return json + lp->name;
    /* compound "a.b.c" style label name */
    return (const char *)hp->data;
}

static void
label_name_length(const pmLabel *lp, const char *json, __pmHashCtl *lc,
		const char **name, int *length)
{
    __pmHashNode	*hp;

    if (!(lp->flags & PM_LABEL_COMPOUND) || lc == NULL ||
	(hp = __pmHashSearch(lp->name, lc)) == NULL) {
	*name = json + lp->name;
	*length = lp->namelen;
    } else {
	*name = hp->data;	/* compound "a.b.c" style label name */
	*length = strlen(hp->data);
    }
}

static inline const char *
label_value(const pmLabel *lp, const char *json)
{
    return json + lp->value;
}

static __pmHashWalkState
labels_hash_destroy_callback(const __pmHashNode *tp, void *cp)
{
    (void)cp;
    free(tp->data);
    return PM_HASH_WALK_DELETE_NEXT;
}

static void
labels_hash_destroy(__pmHashCtl *compound)
{
    if (compound) {
	__pmHashWalkCB(labels_hash_destroy_callback, NULL, compound);
	__pmHashClear(compound);
    }
}

static __pmHashWalkState
labels_hash_duplicate_callback(const __pmHashNode *sp, void *cp)
{
    char	*name = strdup(sp->data);

    if (name)
	__pmHashAdd(sp->key, name, (__pmHashCtl *)cp);
    return PM_HASH_WALK_NEXT;
}

static void
labels_hash_duplicate(__pmHashCtl *source, void **target)
{
    if ((*target = calloc(1, sizeof(__pmHashCtl))) != NULL)
	__pmHashWalkCB(labels_hash_duplicate_callback, *target, source);
}

/* coverity[+free] : arg-0 */
void
pmFreeLabelSets(pmLabelSet *sets, int nsets)
{
    int		i;

    if (sets == NULL)
	return;

    for (i = 0; i < nsets; i++) {
	if (sets[i].nlabels > 0)
	    free(sets[i].labels);
	if (sets[i].json)
	    free(sets[i].json);
	if (sets[i].compound && sets[i].hash) {
	    sets[i].compound = 0;
	    labels_hash_destroy(sets[i].hash);
	    free(sets[i].hash);
	    sets[i].hash = NULL;
	}
    }
    free(sets);
}

pmLabelSet *
__pmDupLabelSets(pmLabelSet *source, int nsets)
{
    pmLabelSet	*sets, *target;
    size_t	size;
    int		i;

    if (nsets == 0 || source == NULL)
	return NULL;
    
    assert(nsets > 0);
    if ((sets = (pmLabelSet *)calloc(nsets, sizeof(pmLabelSet))) == NULL)
	return NULL;

    for (i = 0; i < nsets; i++, source++) {
	target = &sets[i];
	memcpy(target, source, sizeof(pmLabelSet));
	/* guard against cases like {} and empty strings */
	if (target->nlabels <= 0 || target->json == NULL) {
	    target->jsonlen = 0;
	    target->json = NULL;
	    continue;
	}
	if ((target->json = strdup(source->json)) == NULL)
	    break;
	size = source->nlabels * sizeof(pmLabel);
	if ((target->labels = malloc(size)) == NULL)
	    break;
	memcpy(target->labels, source->labels, size);
	if (source->compound && source->hash)
	    labels_hash_duplicate(source->hash, &target->hash);
    }
    if (i == nsets)
	return sets;

    pmFreeLabelSets(sets, nsets);
    return NULL;
}

int
__pmAddLabels(pmLabelSet **lspp, const char *extras, int flags)
{
    __pmHashCtl		compound = {0};
    pmLabelSet		*lsp = *lspp;
    pmLabel		input[PM_MAXLABELS], *lp = NULL;
    pmLabel		output[PM_MAXLABELS];
    char		*json = NULL;
    char		buffer[PM_MAXLABELJSONLEN];
    char		result[PM_MAXLABELJSONLEN];
    int			bytes, count, size, sts;

    if ((bytes = strlen(extras)) + 1 >= PM_MAXLABELJSONLEN)
	return -E2BIG;

    size = sizeof(result);
    if ((sts = __pmParseLabels(extras, bytes, input, PM_MAXLABELS, &compound,
				flags, buffer, &size)) < 0) {
	labels_hash_destroy(&compound);
	return sts;
    }

    if (lsp) {	/* parse the new set and then merge with existing labels */
	sts = __pmMergeLabelSets(lsp->labels, lsp->json,
				 (__pmHashCtl *)lsp->hash, lsp->nlabels,
				 input, buffer, &compound, sts,
				 output, result, &count, sizeof(result), NULL, NULL);
	labels_hash_destroy(&compound);
	if (sts < 0)
	    return sts;
	if (sts >= PM_MAXLABELJSONLEN)
	    return -E2BIG;
	size = sts;
	sts = count;
    }

    if (sts > 0) {
	bytes = sts * sizeof(pmLabel);
	if ((lp = malloc(bytes)) == NULL) {
	    labels_hash_destroy(&compound);
	    return -ENOMEM;
	}
	memcpy(lp, lsp ? output : input, bytes);
    }

    if ((json = strndup(lsp? result : buffer, size + 1)) == NULL) {
	labels_hash_destroy(&compound);
	if (lsp) memset(lsp, 0, sizeof(*lsp));
	if (lp) free(lp);
	return -ENOMEM;
    }

    if (!lsp) {
	if ((lsp = calloc(1, sizeof(pmLabelSet))) == NULL) {
	    labels_hash_destroy(&compound);
	    if (lp) free(lp);
	    free(json);
	    return -ENOMEM;
	}
	lsp->inst = PM_IN_NULL;
    }
    lsp->nlabels = sts;
    lsp->jsonlen = size;
    if (lsp->json)
	free(lsp->json);
    lsp->json = json;
    if (lsp->labels)
	free(lsp->labels);
    lsp->labels = lp;
    if (compound.hsize > 0 && compound.nodes > 0) {
	lsp->compound = 1;
	labels_hash_duplicate(&compound, &lsp->hash);
    }
    labels_hash_destroy(&compound);

    *lspp = lsp;
    return sts;
}

int
__pmParseLabelSet(const char *json, int jlen, int flags, pmLabelSet **set)
{
    __pmHashCtl		jh = {0};
    pmLabelSet		*result;
    pmLabel		*lp, labels[MAXLABELSET];
    char		*bp, buf[PM_MAXLABELJSONLEN];
    int			sts, nlabels, bsz = PM_MAXLABELJSONLEN;

    sts = nlabels = (!jlen || !json) ? 0 :
	__pmParseLabels(json, jlen, labels, MAXLABELSET, &jh, flags, buf, &bsz);
    if (sts < 0) {
	labels_hash_destroy(&jh);
	return sts;
    }

    if (nlabels > 0) {
	if ((lp = calloc(nlabels, sizeof(*lp))) == NULL) {
	    labels_hash_destroy(&jh);
	    return -ENOMEM;
	}
    } else {
	lp = NULL;
    }

    if ((result = (pmLabelSet *)calloc(1, sizeof(*result))) == NULL) {
	labels_hash_destroy(&jh);
	if (lp) free(lp);
	return -ENOMEM;
    }
    if (nlabels > 0) {
	if ((bp = strndup(buf, bsz + 1)) == NULL) {
	    labels_hash_destroy(&jh);
	    free(result);
	    if (lp) free(lp);
	    return -ENOMEM;
	}
    } else {
	bsz = 0;
	bp = NULL;
    }

    result->inst = PM_IN_NULL;
    result->nlabels = nlabels;
    if (nlabels > 0)
	memcpy(lp, labels, nlabels * sizeof(pmLabel));
    result->json = (char *)bp;
    result->jsonlen = bsz;
    result->labels = lp;
    if (jh.hsize > 0 && jh.nodes > 0) {
	labels_hash_duplicate(&jh, &result->hash);
	result->compound = 1;
    }
    labels_hash_destroy(&jh);

    *set = result;
    return 1;
}

static int
namecmp6(const pmLabel *ap, const char *as, __pmHashCtl *ac,
	 const pmLabel *bp, const char *bs, __pmHashCtl *bc)
{
    const char	*name;
    int		sts, length;

    if (ap->namelen > bp->namelen) {
	label_name_length(bp, bs, bc, &name, &length);
	if ((sts = strncmp(label_name(ap, as, ac), name, length)))
	    return sts;
	return 1;	/* ap longer, so remainder sorts larger */
    }
    if (ap->namelen < bp->namelen) {
	label_name_length(ap, as, ac, &name, &length);
	if ((sts = strncmp(name, label_name(bp, bs, bc), length)))
	    return sts;
	return -1;	/* bp longer, so remainder sorts larger */
    }
    /* (ap->namelen == bp->namelen) */
    label_name_length(ap, as, ac, &name, &length);
    return strncmp(name, label_name(bp, bs, bc), length);
}

static int
namecmp(const void *a, const void *b, void *arg)
{
    const pmLabel	*ap = (const pmLabel *)a;
    const pmLabel	*bp = (const pmLabel *)b;
    struct parser	*parser = (struct parser *)arg;

    return namecmp6(ap, parser->jsonb, parser->compound,
		    bp, parser->jsonb, parser->compound);
}

static int
stash_chars(const char *s, int slen, char **buffer, unsigned int *buflen)
{
    unsigned int	bytes = *buflen;
    char		*bp = *buffer;

    if (slen >= bytes)
	return -E2BIG;
    if (slen) {
	bytes = pmsprintf(bp, bytes, "%.*s", slen, s);
	*buffer = bp + bytes;
	*buflen -= bytes;
	return bytes;
    }
    return 0;
}

static int
stash_label(const pmLabel *lp, const char *json, __pmHashCtl *lc,
	    pmLabel *olabel, const char *obuffer, int *no,
	    char **buffer, int *buflen,
	    filter_labels filter, void *arg)
{
    const char		*name;
    char		*bp = *buffer;
    int			bytes = *buflen;
    int			valuelen, namelen;

    if (filter != NULL && filter(lp, json, arg) == 0)
	return 0;

    label_name_length(lp, json, lc, &name, &namelen);
    if ((namelen + 2 + 1 + lp->valuelen + 1) >= bytes)
	return -E2BIG;

    if (lp->valuelen) {
	bytes = pmsprintf(bp, bytes, "\"%.*s\":%.*s,", namelen, name,
				(int)lp->valuelen, label_value(lp, json));
	valuelen = lp->valuelen;
    } else {
	bytes = pmsprintf(bp, bytes, "\"%.*s\":null,", namelen, name);
	valuelen = 4;
    }

    *buffer = bp + bytes;
    *buflen -= bytes;

    if (olabel) {
	pmLabel	*op = &olabel[*no];
	op->name = (bp - obuffer) + 1;
	op->namelen = namelen;
	op->flags = lp->flags;
	op->value = (bp - obuffer) + 1 + namelen + 2;
	op->valuelen = valuelen;
	*no = *no + 1;
    }

    return bytes;
}

/*
 * Check length and characters in a given label meet the PCP naming rules.
 * Return code is negative for invalid name, else zero or one indicating
 * whether a compound name was detected.  Compound names may appear here
 * only when the nested flag is set, indicating we're merging an already
 * parsed and decoded label string.
 */
static int
verify_label_name(const char *name, size_t length, int nested)
{
    const char		*start = name;
    int			sts = 0;

    if (length == 0)
	return -EINVAL;
    if (length >= MAXLABELNAMELEN)
	return -E2BIG;
    if (!isalpha((int)*name))	/* first character must be alphanumeric */
	return -EINVAL;
    while (++name < (start + length)) {
	if (isalnum((int)*name) || *name == '_')	/* all the rest */
	    continue;
	if (*name == '.' && nested) {
	    sts = 1;
	    continue;
	}
	return -EINVAL;
    }
    return sts;
}

/*
 * Sort and verify labels in the set - no duplicate names are allowed.
 */
static int
sort_labels(pmLabel *lp, int nlabels, struct parser *parser)
{
    const char		*name;
    int			i, namelen;

    if (nlabels > 1)
	sort_r(lp, nlabels, sizeof(pmLabel), namecmp, parser);

    for (i = 0; i < nlabels - 1; i++) {
	if (namecmp(&lp[i], &lp[i+1], parser) == 0) {
	    if (pmDebugOptions.labels) {
		label_name_length(&lp[i], parser->jsonb, parser->compound,
				&name, &namelen);
		fprintf(stderr, "Label name duplicated %.*s", namelen, name);
	    }
	    return -EINVAL;
	}
    }
    return 0;
}

static void
pop_label_name(parser_t *parser)
{
    char		*dot;

    if (parser->stackdepth > 1) {
	parser->stackstyle = STACK_COMPOUND;
	parser->stackdepth--;
    } else {
	parser->stackstyle = STACK_DIRECT;
	parser->stackdepth = 0;
    }
    parser->stackseq++;

    if (parser->stack == NULL)
	return;

    /* handle '{ "a": { "b": { "c":1 } }' situations (label "a.b.c") */
    if ((dot = strrchr(parser->stack, '.')) != NULL) {
	*dot = '\0';
	parser->stacklen = (dot - parser->stack);
    } else {
	free(parser->stack);
	parser->stack = NULL;
	parser->stacklen = 0;
    }
}

static void
push_label_name(parser_t *parser)
{
    char		*stack;	/* dot-separated accumulated 'stack' */
    size_t		bytes;
    int			first = 1;

    if (parser->name) {
	bytes = parser->namelen + 1;
	if (parser->stack) {
	    bytes += parser->stacklen + 1;
	    first = 0;
	}
	if ((stack = realloc(parser->stack, bytes)) == NULL)
	    return;
	/* do not access parser->stack now until reassigned (maybe freed) */
	if (first)
	    pmsprintf(stack, bytes, "%.*s",
			parser->namelen, parser->jsonb + parser->name);
	else
	    pmsprintf(stack+parser->stacklen, bytes-parser->stacklen, ".%.*s",
			parser->namelen, parser->jsonb + parser->name);
	parser->stack = stack;	/* reassigned here, safe to access again */
	parser->stacklen = bytes;
	parser->stackstyle = STACK_COMPOUND;
	parser->stackdepth++;
	parser->stackseq++;
    } else {
	parser->stackstyle = STACK_DIRECT;
    }
}

static void
add_label_value(parser_t *parser, unsigned int valuelen, int noop)
{
    unsigned int	value = (parser->buffer - parser->jsonb) - valuelen;
    char		*stack;
    int			bytes, index = parser->nlabels;

    if (index >= parser->maxlabels) {
	if (pmDebugOptions.labels)
	    fprintf(stderr, "Too many labels (%d)", parser->nlabels);
	parser->error = -E2BIG;
    } else {
	/* insert into name hash for later lookups if compound */
	if (!noop && parser->compound && parser->stackstyle == STACK_COMPOUND) {
	    bytes = parser->stacklen + 1 + parser->namelen + 1;
	    if ((stack = malloc(bytes)) == NULL)
		return;
	    pmsprintf(stack, bytes, "%s.%.*s", parser->stack,
			parser->namelen, parser->jsonb + parser->name);
	    if (__pmHashAdd(parser->name, stack, parser->compound) < 0) {
		free(stack);
		return;
	    }
	}
	/* insert into label array for later sorting+searching */
	memset(&parser->labels[index], 0, sizeof(pmLabel));
	parser->labels[index].name = parser->name;
	parser->labels[index].namelen = parser->namelen;
	parser->labels[index].value = value;
	parser->labels[index].valuelen = valuelen;
	parser->labels[index].flags = parser->labelflags;
	if (parser->stackstyle == STACK_COMPOUND)
	    parser->labels[index].flags |= PM_LABEL_COMPOUND;
	parser->nlabels++;
    }

    parser->name = parser->namelen = 0;	/* reset for further parsing */
}

static void
labels_token_callback(jsonsl_t json, jsonsl_action_t action,
		struct jsonsl_state_st *state, const char *at)
{
    struct parser	*parser = (struct parser *)json->data;
    const char		*string;
    char		*name;
    unsigned int	nested;
    int			length, sts;

    if (pmDebugOptions.labels && pmDebugOptions.desperate) {
	fprintf(stderr, "\n== %s\n", "labels_token_callback");
	fprintf(stderr, "action: %s\n", action_string(action));
	fprintf(stderr, "state type: %s offset: %ld level: %d nelem: %lu\n",
		state_string(state), (long)state->pos_begin,
		state->level, (unsigned long)state->nelem);
	fprintf(stderr, "at: %p (%c)\n", at, *at);
	fprintf(stderr, "pos: %llu\n", (unsigned long long)json->pos);
    }

    /*
     * First part of producing JSONB form: no redundant whitespace.
     * We also do sanity checking here - number of labels must not
     * exceed the maximum, and name validity; finally building up
     * the labelset data structure as we go.
     */
    if (action == JSONSL_ACTION_PUSH) {
	switch (state->type) {
	case JSONSL_T_OBJECT:
	    if (parser->comma)
		stash_chars(",", 1, &parser->buffer, &parser->buflen);
	    stash_chars("{", 1, &parser->buffer, &parser->buflen);
	    push_label_name(parser);
	    parser->comma = 0;
	    break;

	case JSONSL_T_LIST:
	    if (parser->comma)
		stash_chars(",", 1, &parser->buffer, &parser->buflen);
	    stash_chars("[", 1, &parser->buffer, &parser->buflen);
	    parser->array = parser->start + json->pos;
	    parser->comma = 0;
	    break;

	case JSONSL_T_HKEY:
	    if (parser->comma)
		stash_chars(",", 1, &parser->buffer, &parser->buflen);
	    parser->token = parser->start + json->pos;
	    parser->comma = 0;
	    break;

	case JSONSL_T_STRING:
	case JSONSL_T_SPECIAL:
	    if (parser->comma)
		stash_chars(",", 1, &parser->buffer, &parser->buflen);
	    parser->token = parser->start + json->pos;
	    parser->comma = 1;
	    break;

	default:
	    break;
	}
    }
    else if (action == JSONSL_ACTION_POP) {
	switch (state->type) {
	case JSONSL_T_OBJECT:
	    stash_chars("}", 1, &parser->buffer, &parser->buflen);
	    if (parser->namelevel == state->level && parser->name != 0)
		add_label_value(parser, 2, 1);	/* empty map */
	    pop_label_name(parser);
	    parser->comma = 1;
	    break;

	case JSONSL_T_LIST:
	    stash_chars("]", 1, &parser->buffer, &parser->buflen);
	    string = parser->array;
	    length = (at - string) + 1;
	    if (parser->namelevel == state->level && parser->name != 0)
		add_label_value(parser, length, 0);
	    parser->array = NULL;
	    parser->comma = 1;
	    break;

	case JSONSL_T_HKEY:
	    string = parser->token + 1;
	    length = at - string;
	    nested = parser->labelflags & PM_LABEL_COMPOUND;
	    if ((sts = verify_label_name(string, length, nested)) < 0) {
		if (pmDebugOptions.labels)
		    fprintf(stderr, "Label name is invalid %.*s", length, string);
		parser->error = sts;
	    }
	    parser->name = (parser->buffer - parser->jsonb) + 1;
	    if (sts > 0 && parser->compound) {
		if ((name = strndup(string, length)) != NULL)
		    if (__pmHashAdd(parser->name, name, parser->compound) < 0)
			free(name);
	    }
	    parser->namelen = length;
	    parser->namelevel = state->level;
	    stash_chars(parser->token, length+1, &parser->buffer, &parser->buflen);
	    stash_chars("\":", 2, &parser->buffer, &parser->buflen);
	    parser->token = NULL;
	    break;

	case JSONSL_T_STRING:
	case JSONSL_T_SPECIAL:
	    string = parser->token;
	    length = at - string;
	    if (state->type == JSONSL_T_STRING)
		length++;	/* include closing quote */
	    stash_chars(string, length, &parser->buffer, &parser->buflen);
	    if (parser->namelevel == state->level && parser->name != 0)
		add_label_value(parser, length, 0);
	    parser->token = NULL;
	    break;

	default:
	    string = parser->token;
	    length = at - string;
	    stash_chars(string, length, &parser->buffer, &parser->buflen);
	    break;
	}
    }

    if (pmDebugOptions.labels && pmDebugOptions.desperate)
	fprintf(stderr, "json: %s\n", parser->jsonb);
}

static int
labels_error_callback(jsonsl_t json, jsonsl_error_t error,
		struct jsonsl_state_st *state, char *at)
{
    struct parser	*parser = (struct parser *)json->data;

    (void)at; (void)state;
    if (pmDebugOptions.labels)
	pmNotifyErr(LOG_ERR, "Error parsing labels at offset %lu: %s\n",
			(unsigned long)json->pos, jsonsl_strerror(error));
    parser->error = -EINVAL;
    return 0;
}

static int
__pmParseLabels(const char *s, int slen,
		pmLabel *labels, int maxlabels, __pmHashCtl *compound, int flags,
		char *buffer, int *buflen)
{
    parser_t		parser = {0};
    jsonsl_t		json;
    int			sts = 0;

    parser.state = PARSE_START;
    parser.maxlabels = maxlabels;
    parser.start = s;
    parser.jsonb = buffer;
    parser.labels = labels;
    parser.buffer = buffer;
    parser.buflen = *buflen;
    parser.compound = compound;
    parser.labelflags = flags;

    if ((json = jsonsl_new(MAX_RECURSION_DEPTH)) == NULL)
	return -ENOMEM;
    jsonsl_enable_all_callbacks(json);
    json->action_callback = labels_token_callback;
    json->error_callback = labels_error_callback;
    json->max_callback_level = MAX_DESCENT_LEVEL;
    json->data = &parser;

    jsonsl_feed(json, s, slen);
    jsonsl_destroy(json);
    if (parser.stack) free(parser.stack);

    if (parser.error) {
	labels_hash_destroy(compound);
	return parser.error;
    }

    if (parser.nlabels == 0) {
	/*
	 * Zero labels happens if we are passed an empty labelset string.
	 * Argument buffer is set to a zero length string and 0 returned.
	 */
	*buflen = 0;
	buffer[0] = '\0';
	labels_hash_destroy(compound);
	return 0;
    }

    if (pmDebugOptions.labels && pmDebugOptions.desperate) {
	char	flbuf[PM_MAXLABELJSONLEN];
	char	*fls;
	int	i;

	for (i = 0; i < parser.nlabels; i++) {
	    fls = __pmLabelFlagString(labels[i].flags, flbuf, sizeof(flbuf));
	    fprintf(stderr, "    [%d] name(%d,%d) : value(%d,%d) [%s]\n", i,
			    labels[i].name, labels[i].namelen,
			    labels[i].value, labels[i].valuelen, fls);
	}
    }

    /* sorting will fail if duplicate names are found (invalid JSONB) */
    if ((sts = sort_labels(labels, parser.nlabels, &parser)) < 0) {
	labels_hash_destroy(compound);
  	return sts;
    }

    /* pass out JSONB buffer space used and pmLabel entries consumed */
    *buflen -= parser.buflen;
    return parser.nlabels;
}

static int
__pmMergeLabelSets(pmLabel *alabels, const char *abuf, __pmHashCtl *ac, int na,
		   pmLabel *blabels, const char *bbuf, __pmHashCtl *bc, int nb,
		   pmLabel *olabels, char *output, int *no, int buflen,
		   filter_labels filter, void *arg)
{
    char		*bp = output;
    int			sts, i, j;

    /* integrity check */
    if ((na > 0 && alabels == NULL) || (nb > 0 && blabels == NULL) ||
        (olabels == NULL && no != NULL) || (olabels != NULL && no == NULL)) {
	if (pmDebugOptions.labels)
	    fprintf(stderr, "__pmMergeLabelSets: invalid or corrupt arguments\n");
	sts = -EINVAL;
	goto done;
    }

    if (no)
	*no = 0;	/* number of output labels */

    /* Walk over both label sets inserting all names into the output
     * buffer, but prefering b-group values over those in the a-group.
     * As we go, check for duplicates between a-group & b-group (since
     * thats invalid and we'd generate invalid output - JSONB format).
     */
    if ((sts = stash_chars("{", 1, &bp, (unsigned int *)&buflen)) < 0)
	goto done;
    i = j = 0;
    do {
	if (i < na) {
	    /* use this a-group label? - compare to current b-group name */
	    if (j >= nb) {	/* reached end of b-group, so use it */
		if (j && namecmp6(&alabels[i], abuf, ac,
				  &blabels[j-1], bbuf, bc) == 0)
		    i++;	/* but skip if its a duplicate name */
		else if ((sts = stash_label(&alabels[i++], abuf, ac,
					    olabels, output, no,
					    &bp, &buflen, filter, arg)) < 0)
		    goto done;
	    } else if (namecmp6(&alabels[i], abuf, ac,
				&blabels[j], bbuf, bc) < 0) {
		if (j && namecmp6(&alabels[i], abuf, ac,
				  &blabels[j-1], bbuf, bc) == 0) /* dup */
		    i++;
		else if ((sts = stash_label(&alabels[i++], abuf, ac,
					    olabels, output, no,
					    &bp, &buflen, filter, arg)) < 0)
		    goto done;
	    }
	}
	if (j < nb) {
	    /* use this b-group label? - compare to current a-group name */
	    if (i >= na) {	/* reached end of a-group, so use it */
		if ((sts = stash_label(&blabels[j++], bbuf, bc,
					olabels, output, no,
					&bp, &buflen, filter, arg)) < 0)
		    goto done;
	    } else if (namecmp6(&alabels[i], abuf, ac,
				&blabels[j], bbuf, bc) >= 0) {
		if ((sts = stash_label(&blabels[j++], bbuf, bc,
					olabels, output, no,
					&bp, &buflen, filter, arg)) < 0)
		    goto done;
	    }
	}
    } while (i < na || j < nb);

    if (na || nb) {	/* overwrite final comma, already inserted */
	bp--;
	buflen--;
    }
    if ((sts = stash_chars("}", 2, &bp, (unsigned int *)&buflen)) < 0)
	goto done;
    sts = bp - output;

done:
    return sts;
}

static int
__pmMergeLabels(const char *a, const char *b, char *buffer, int buflen, int flags)
{
    __pmHashCtl		acompound = {0}, bcompound = {0};
    pmLabel		alabels[MAXLABELSET], blabels[MAXLABELSET];
    char		abuf[PM_MAXLABELJSONLEN], bbuf[PM_MAXLABELJSONLEN];
    int			abufsz, bbufsz;
    int			sts, na = 0, nb = 0;

    abufsz = bbufsz = PM_MAXLABELJSONLEN;
    if (!a || (na = strlen(a)) == 0)
	abufsz = 0;
    else if ((sts = na = __pmParseLabels(a, na,
				alabels, MAXLABELSET, &acompound, flags,
				abuf, &abufsz)) < 0) {
	labels_hash_destroy(&acompound);
	return sts;
    }

    if (!b || (nb = strlen(b)) == 0)
	bbufsz = 0;
    else if ((sts = nb = __pmParseLabels(b, strlen(b),
				blabels, MAXLABELSET, &bcompound, flags,
				bbuf, &bbufsz)) < 0) {
	labels_hash_destroy(&bcompound);
	return sts;
    }

    if (!abufsz && !bbufsz)
	return 0;

    sts = __pmMergeLabelSets(alabels, abuf, &acompound, na,
			     blabels, bbuf, &bcompound, nb,
			     NULL, buffer, NULL, buflen, NULL, NULL);
    labels_hash_destroy(&acompound);
    labels_hash_destroy(&bcompound);
    return sts;
}

/*
 * Walk the "sets" array left to right (increasing precedence)
 * and produce the merged set into the supplied buffer.
 * An optional user-supplied callback routine allows fine-tuning
 * of the resulting set of labels.
 */
int
pmMergeLabelSets(pmLabelSet **sets, int nsets, char *buffer, int buflen,
		filter_labels filter, void *arg)
{
    __pmHashCtl		*compound, bhash = {0};
    pmLabel		olabels[MAXLABELSET];
    pmLabel		blabels[MAXLABELSET];
    char		buf[PM_MAXLABELJSONLEN];
    int			nlabels = 0;
    int			i, sts = 0;

    if (!sets || nsets < 1)
	return -EINVAL;

    for (i = 0; i < nsets; i++) {
	if (sets[i] == NULL || sets[i]->nlabels < 0)
	    continue;

	/* avoid overwriting the working set, if there is one */
	if (sts > 0) {
	    memcpy(buf, buffer, sts);
	    memcpy(blabels, olabels, nlabels * sizeof(pmLabel));
	} else {
	    memset(buf, 0, sizeof(buf));
	    memset(blabels, 0, sizeof(blabels));
	}

	if (pmDebugOptions.labels) {
	    fprintf(stderr, "%s: merging set [%d]\n", "pmMergeLabelSets", i);
	    __pmDumpLabelSet(stderr, sets[i]);
	}

	if (sets[i]->compound)
	    compound = (__pmHashCtl *)sets[i]->hash;
	else
	    compound = NULL;

	/*
	 * Merge sets[i] with blabels into olabels. Any duplicate label
	 * names in sets[i] prevail over those in blabels.
	 */
	sts = __pmMergeLabelSets(blabels, buf, &bhash, nlabels,
				sets[i]->labels, sets[i]->json,
				compound, sets[i]->nlabels,
				olabels, buffer, &nlabels, buflen, filter, arg);
	labels_hash_destroy(&bhash);
	if (sts < 0)
	    return sts;
	if (sts >= buflen || sts >= PM_MAXLABELJSONLEN)
	    return -E2BIG;
    }
    return sts;
}

/*
 * Walk the "sets" array left to right (increasing precendence)
 * of JSON and produce the merged set into the supplied buffer.
 * JSONB-only variant (no indexing or flags available).
 */
int
pmMergeLabels(char **sets, int nsets, char *buffer, int buflen)
{
    char		buf[PM_MAXLABELJSONLEN];
    int			bytes = 0;
    int			i, sts, flags = PM_LABEL_COMPOUND;

    if (!sets || nsets < 1)
	return -EINVAL;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < nsets; i++) {
	if (bytes)
	    memcpy(buf, buffer, bytes);
	if ((sts = __pmMergeLabels(buf, sets[i], buffer, buflen, flags)) < 0)
	    return sts;
	bytes = sts;
	if (bytes >= buflen || bytes >= PM_MAXLABELJSONLEN)
	    return -E2BIG;
    }
    return bytes;
}

static void
labelfile(const char *path, const char *file, char *buf, int buflen)
{
    FILE		*fp;
    char		lf[MAXPATHLEN];
    size_t		bytes;

    pmsprintf(lf, sizeof(lf), "%s%c%s", path, pmPathSeparator(), file);
    if ((fp = fopen(lf, "r")) != NULL) {
	bytes = fread(buf, 1, buflen-1, fp);
	fclose(fp);
	buf[bytes] = '\0';
	if (pmDebugOptions.labels)
	    fprintf(stderr, "labelfile: loaded from %s:\n%s", file, buf);
    } else {
	buf[0] = '\0';
    }
}

int
__pmGetContextLabelSet(pmLabelSet **set, int flags, const char *path)
{
    struct dirent	**list = NULL;
    char		buf[PM_MAXLABELJSONLEN];
    int			i, num, sts = 0;

    if ((num = scandir(path, &list, NULL, alphasort)) < 0)
	return -oserror();

    for (i = 0; i < num; i++) {
	if (list[i]->d_name[0] == '.')
	    continue;
	labelfile(path, list[i]->d_name, buf, sizeof(buf));
	if ((sts = __pmAddLabels(set, buf, flags)) < 0) {
	    if (pmDebugOptions.labels)
		pmNotifyErr(LOG_ERR, "Error parsing %s labels in %s, ignored\n",
				list[i]->d_name, path);
	    continue;
	}
    }
    for (i = 0; i < num; i++)
	free(list[i]);
    free(list);
    return sts > 0;
}

int
__pmGetContextLabels(pmLabelSet **set)
{
    char		path[MAXPATHLEN];
    char		*sysconfdir = pmGetConfig("PCP_SYSCONF_DIR");
    int			sts, sep = pmPathSeparator(), flags = PM_LABEL_COMPOUND;

    flags |= PM_LABEL_CONTEXT;
    pmsprintf(path, sizeof(path), "%s%clabels", sysconfdir, sep);
    if ((sts = __pmGetContextLabelSet(set, flags, path)) < 0)
	return sts;
    flags |= PM_LABEL_OPTIONAL;
    pmsprintf(path, sizeof(path), "%s%clabels%coptional", sysconfdir, sep, sep);
    __pmGetContextLabelSet(set, flags, path);

    if (pmDebugOptions.labels) {
	fprintf(stderr, "%s: return sts=%d:\n", "__pmGetContextLabels", sts);
	__pmDumpLabelSet(stderr, *set);
    }

    return sts;
}

static char *
archive_host_labels(__pmContext *ctxp, char *buffer, int buflen)
{
    /*
     * Backward compatibility fallback, for archives created before
     * labels support is added to pmlogger.
     * Once that's implemented (TYPE_LABEL_V2 in .meta) fields will be
     * added to the context structure and we'll be able to read 'em
     * here to provide complete archive label support.
     */
    pmsprintf(buffer, buflen, "{\"hostname\":\"%s\"}",
		ctxp->c_archctl->ac_log->label.hostname);
    buffer[buflen-1] = '\0';
    return buffer;
}

static int
archive_context_labels(__pmContext *ctxp, pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    char	*hostp;
    int		sts, flags = PM_LABEL_CONTEXT;

    hostp = archive_host_labels(ctxp, buf, sizeof(buf));
    if ((sts = __pmAddLabels(&lp, hostp, flags)) < 0)
	return sts;
    *sets = lp;
    return 1;
}

const char * 
__pmGetLabelConfigHostName(char *host, size_t length)
{
    if (gethostname(host, length) < 0)
	pmsprintf(host, length, "localhost");
    else
	host[length-1] = '\0';
    return "hostname";
}

const char *
__pmGetLabelConfigDomainName(char *domain, size_t length)
{
    if ((getdomainname(domain, length) < 0) ||
	(domain[0] == '\0') || (strcmp(domain, "(none)") == 0))
	pmsprintf(domain, length, "localdomain");
    else
	domain[length-1] = '\0';
    return "domainname";
}

/*
 * Extract name and value for the local machine ID label.
 * Supported forms are 'machineid' and 'machineid_sha256'
 * based on 'machineid_hash' setting in labels.conf file.
 */
const char *
__pmGetLabelConfigMachineID(char *machineid, size_t length)
{
    FILE		*fp;
    char		buf[BUFSIZ], *p;
    int			sha256 = 0;

    if (getmachineid(machineid, length) < 0)
	pmsprintf(machineid, length, "localmachine");
    else
	machineid[length-1] = '\0';

    pmsprintf(buf, sizeof(buf), "%s%c%s", pmGetConfig("PCP_SYSCONF_DIR"),
			pmPathSeparator(), "labels.conf");
    if ((fp = fopen(buf, "r")) == NULL)
	return "machineid";

    /* cheap and cheerful ini-style labels.conf parsing */
    while ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
	while (isspace(*p))
	    p++;
	if (*p == '\0' || *p == '#')	/* comment or nothing */
	    continue;
	if (strncmp(p, "[global]", 8) == 0)  /* global section */
	    continue;
	if (strncmp(p, "[", 1) == 0)	/* a new section header */
	    break;

	/* only the one configurable global setting at this time */
	if (strncmp(p, "machineid_hash", 14) != 0)
	    continue;
	p += 15;
	while (isspace(*p) || *p == ':' || *p == '=')
	    p++;
	if (strncmp(p, "sha256", 6) == 0)
	    sha256 = 1;
	else if (strncmp(p, "none", 4) != 0)
	    pmNotifyErr(LOG_INFO, "Ignoring unknown %s %s value \"%s\"\n",
			"labels.conf", "machineid_hash", p);
	break;
    }
    fclose(fp);

    if (sha256) {
	const char	*cset = "0123456789abcdef";
	unsigned char	hash[SHA256_BLOCK_SIZE];
	SHA256_CTX	ctx;
	int		i;

	sha256_init(&ctx);
	sha256_update(&ctx, (unsigned char *)machineid, strlen(machineid));
	sha256_final(&ctx, hash);

	assert(length >= SHA256_BLOCK_SIZE*2 + 1);
	for (i = 0; i < SHA256_BLOCK_SIZE; i++) {
	    machineid[i*2] = cset[((hash[i]&0xF0)>>4)];
	    machineid[i*2+1] = cset[(hash[i]&0xF)];
	}
	machineid[SHA256_BLOCK_SIZE*2] = '\0';
	return "machineid_sha256";
    }
    return "machineid";
}

static char *
local_host_labels(char *buffer, int buflen)
{
    char	host[MAXHOSTNAMELEN];
    char	domain[MAXDOMAINNAMELEN];
    char	machineid[MAXMACHINEIDLEN];
    const char	*host_label, *domain_label, *machineid_label;

    host_label = __pmGetLabelConfigHostName(host, sizeof(host));
    domain_label = __pmGetLabelConfigDomainName(domain, sizeof(domain));
    machineid_label = __pmGetLabelConfigMachineID(machineid, sizeof(machineid));

    pmsprintf(buffer, buflen, "{\"%s\":\"%s\",\"%s\":\"%s\",\"%s\":\"%s\"}",
	    host_label, host, domain_label, domain, machineid_label, machineid);
    return buffer;
}

#if defined(HAVE_GETUID) && defined(HAVE_GETGID)
static char *
local_user_labels(char *buffer, int buflen)
{
    pmsprintf(buffer, buflen, "{\"groupid\":%u,\"userid\":%u}",
		    (unsigned int)getgid(), (unsigned int)getuid());
    return buffer;
}
#endif

static int
local_context_labels(pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    char	*hostp;
#if defined(HAVE_GETUID) && defined(HAVE_GETGID)
    char	*userp;
#endif
    int		sts, flags = PM_LABEL_CONTEXT;

    if ((sts = __pmGetContextLabels(&lp)) < 0)
	return sts;

    hostp = local_host_labels(buf, sizeof(buf));
    if ((sts = __pmAddLabels(&lp, hostp, flags)) <= 0) {
	pmFreeLabelSets(lp, 1);
	return sts;
    }

#if defined(HAVE_GETUID) && defined(HAVE_GETGID)
    flags |= PM_LABEL_OPTIONAL;
    userp = local_user_labels(buf, sizeof(buf));
    if ((sts = __pmAddLabels(&lp, userp, flags)) <= 0) {
	pmFreeLabelSets(lp, 1);
	return sts;
    }
#endif

    *sets = lp;
    return 1;
}

int
__pmGetDomainLabels(int domain, const char *name, pmLabelSet **set)
{
    int		buflen, namelen;
    char	buf[PM_MAXLABELJSONLEN];
  
    (void)domain;	/* not currently used */
    /* clean the string - strip "pmda" prefix and "DSO" suffix */
    if (strncmp(name, "pmda", 4) == 0 && name[4] != '\0')
	name += 4;
    namelen = strlen(name);
    if (namelen > 4 && strcmp(name + namelen - 4, " DSO") == 0)
	namelen -= 4;

    buflen = pmsprintf(buf, sizeof(buf), "{\"agent\":\"%.*s\"}", namelen, name);
    return __pmParseLabelSet(buf, buflen, PM_LABEL_DOMAIN, set);
}

static int
local_domain_labels(__pmDSO *dp, pmLabelSet **set)
{
    return __pmGetDomainLabels(dp->domain, dp->name, set);
}

static int
lookup_domain(int ident, int type)
{
    if (type & PM_LABEL_CONTEXT)
	return 0;
    if (type & PM_LABEL_DOMAIN)
	return ident;
    if (type & (PM_LABEL_INDOM | PM_LABEL_INSTANCES))
	return pmInDom_domain(ident);
    if (type & (PM_LABEL_CLUSTER | PM_LABEL_ITEM))
	return pmID_domain(ident);
    return -EINVAL;
}

static int
getlabels(int ident, int type, pmLabelSet **sets, int *nsets)
{
    __pmContext	*ctxp;
    int		ctx, sts;

    if ((sts = ctx = pmWhichContext()) < 0)
	return sts;
    if ((ctxp = __pmHandleToPtr(sts)) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	int	handle = __pmPtrToHandle(ctxp);
	int	tout = ctxp->c_pmcd->pc_tout_sec;
	int	fd = ctxp->c_pmcd->pc_fd;

	if (!(__pmFeaturesIPC(fd) & PDU_FLAG_LABELS))
	    sts = PM_ERR_NOLABELS;	/* lack pmcd support */
	else {
	    sts = 0;
	    if ((type & PM_LABEL_INSTANCES) && ctxp->c_sent == 0) {
	    	/* profile not current for label instances request */
		if (pmDebugOptions.profile || pmDebugOptions.labels) {
		    fprintf(stderr, "dolabels: sent profile, indom=%d\n", ident);
		    __pmDumpProfile(stderr, ident, ctxp->c_instprof);
		}
		if ((sts = __pmSendProfile(fd, __pmPtrToHandle(ctxp),
                                   ctxp->c_slot, ctxp->c_instprof)) < 0)
		    sts = __pmMapErrno(sts);
		else {
		    /* no reply expected for profile */
		    ctxp->c_sent = 1;
		}
	    }
	    if (sts >= 0) {
		if ((sts = __pmSendLabelReq(fd, handle, ident, type)) < 0)
		    sts = __pmMapErrno(sts);
		else {
		    int x_ident = ident, x_type = type;
		    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_CALL);
		    sts = __pmRecvLabel(fd, ctxp, tout, &x_ident, &x_type, sets, nsets);
		}
	    }
	}
    }
    else if (ctxp->c_type == PM_CONTEXT_LOCAL) {
	__pmDSO	*dp;
	int	domain = lookup_domain(ident, type);

	if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	    /* Local context requires single-threaded applications */
	    sts = PM_ERR_THREAD;
	else if (domain < 0)
	    sts = domain;
	else if (type & PM_LABEL_CONTEXT)
	    sts = *nsets = local_context_labels(sets);
	else if ((dp = __pmLookupDSO(domain)) == NULL)
	    sts = PM_ERR_NOAGENT;
	else {
	    pmdaInterface	*pmda = &dp->dispatch;
	    pmdaExt		*pmdaext = pmda->version.any.ext;

	    if (pmda->comm.pmda_interface >= PMDA_INTERFACE_7) {
		pmdaext->e_context = ctx;
		sts = pmda->version.seven.label(ident, type, sets, pmdaext);
	    } else if (type & PM_LABEL_DOMAIN) {
		/* supply domain labels for PMDAs lacking label support */
		sts = local_domain_labels(dp, sets);
	    } else {
		sts = 0;
	    }

	    if (sts >= 0)
		*nsets = sts;
	}
    }
    else {
	sts = __pmLogLookupLabel(ctxp->c_archctl, type, ident, sets, &ctxp->c_origin);

	if (sts < 0) {
	    /* supply context labels for archives lacking label support */
	    if (type & PM_LABEL_CONTEXT)
		sts = archive_context_labels(ctxp, sets);
	    else
		sts = 0;
	} else if (sts > 0) {
	    /* sets currently points into the context structures - copy */
	    if ((*sets = __pmDupLabelSets(*sets, sts)) == NULL)
		sts = -ENOMEM;
	}
	if (sts >= 0)
	    *nsets = sts;
    }

    PM_UNLOCK(ctxp->c_lock);
    return sts;
}

static int
dolabels(int ident, int type, pmLabelSet **labels)
{
    pmLabelSet	*sets = NULL;
    int		sts, nsets = 0;

    if ((sts = getlabels(ident, type, &sets, &nsets)) < 0)
	return sts;

    if (nsets) {
	*labels = sets;
	return nsets;
    }

    *labels = NULL;
    return 0;
}

int
pmGetContextLabels(pmLabelSet **labels)
{
    return dolabels(PM_ID_NULL, PM_LABEL_CONTEXT, labels);
}

int
pmGetDomainLabels(int domain, pmLabelSet **labels)
{
    return dolabels(domain, PM_LABEL_DOMAIN, labels);
}

int
pmGetInDomLabels(pmInDom indom, pmLabelSet **labels)
{
    return dolabels(indom, PM_LABEL_INDOM, labels);
}

int
pmGetClusterLabels(pmID pmid, pmLabelSet **labels)
{
    return dolabels(pmid, PM_LABEL_CLUSTER, labels);
}

int
pmGetItemLabels(pmID pmid, pmLabelSet **labels)
{
    return dolabels(pmid, PM_LABEL_ITEM, labels);
}

int
pmGetInstancesLabels(pmInDom indom, pmLabelSet **labels)
{
    return dolabels(indom, PM_LABEL_INSTANCES, labels);
}

int
pmLookupLabels(pmID pmid, pmLabelSet **labels)
{
    pmLabelSet	*lsp = NULL, *sets = NULL;
    pmDesc	desc;
    pmID	ident;
    int		sts, count, total;

    if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	return sts;

    /* context, domain, [indom], cluster, item */
    total = (desc.indom == PM_INDOM_NULL) ? 4 : 5;
    if ((sets = calloc(total, sizeof(pmLabelSet))) == NULL)
	return -ENOMEM;
    count = 0;

    if ((sts = pmGetContextLabels(&lsp)) < 0) {
	free(sets);
	return sts;
    }
    if (lsp) {
	sets[count++] = *lsp;
	free(lsp);
	lsp = NULL;
    }

    ident = pmID_domain(pmid);
    if ((sts = pmGetDomainLabels(ident, &lsp)) < 0)
	goto fail;
    if (lsp) {
	sets[count++] = *lsp;
	free(lsp);
	lsp = NULL;
    }

    if (desc.indom != PM_INDOM_NULL) {
	if ((sts = pmGetInDomLabels(desc.indom, &lsp)) < 0)
	    goto fail;
	if (lsp) {
	    sets[count++] = *lsp;
	    free(lsp);
	    lsp = NULL;
	}
    }

    ident = pmID_build(ident, pmID_cluster(pmid), 0);
    if ((sts = pmGetClusterLabels(ident, &lsp)) < 0)
	goto fail;
    if (lsp) {
	sets[count++] = *lsp;
	free(lsp);
	lsp = NULL;
    }

    if ((sts = pmGetItemLabels(pmid, &lsp)) < 0)
	goto fail;
    if (lsp) {
	sets[count++] = *lsp;
	free(lsp);
	lsp = NULL;
    }

    *labels = sets;
    return count;

fail:
    pmFreeLabelSets(sets, count);
    if (sets && !count)
	free(sets);
    if (lsp)
	free(lsp);
    return sts;
}

void
pmPrintLabelSets(FILE *fp, int ident, int type, pmLabelSet *sets, int nsets)
{
    int		i, n;
    char	*flags;
    char	idbuf[64], fbuf[32];
    pmLabelSet	*p;

    __pmLabelIdentString(ident, type, idbuf, sizeof(idbuf));
    for (n = 0; n < nsets; n++) {
	p = &sets[n];
	if (type & PM_LABEL_INSTANCES)
	    fprintf(fp, "    %s[%u] labels (%u bytes): %s\n",
			idbuf, p->inst, p->jsonlen, p->json);
	else
	    fprintf(fp, "    %s labels (%u bytes): %s\n",
			idbuf, p->jsonlen, p->json);
	for (i = 0; i < p->nlabels; i++) {
	    flags = __pmLabelFlagString(p->labels[i].flags, fbuf, sizeof(fbuf));
	    fprintf(fp, "        [%d] name(%d,%d) : value(%d,%d) [%s]\n", i,
		    p->labels[i].name, p->labels[i].namelen,
		    p->labels[i].value, p->labels[i].valuelen, flags);
	}
    }
}
