/*
 * Copyright (c) 2016-2018 Red Hat.
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
#include "sort_r.h"
#include "fault.h"
#include "jsmn.h"

#define MAXLABELSET		(PM_MAXLABELS * 6)	/* 6 hierarchy levels */
#define INC_TOKENS		64	/* parser tokens allocation increment */

#define label_name(lp, json)	(json+(lp)->name)
#define label_value(lp, json)	(json+(lp)->value)

static int __pmMergeLabels(const char *, const char *, char *, int);
static int __pmParseLabels(const char *, int, pmLabel *, int, char *, int *);

void
pmFreeLabelSets(pmLabelSet *sets, int nsets)
{
    int		i;

    for (i = 0; i < nsets; i++) {
	if (sets[i].nlabels > 0)
	    free(sets[i].labels);
	if (sets[i].json)
	    free(sets[i].json);
    }
    if (nsets > 0)
	free(sets);
}

static pmLabelSet *
__pmDupLabelSets(pmLabelSet *source, int nsets)
{
    pmLabelSet	*sets, *target;
    size_t	size;
    int		i;

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
    }
    if (i == nsets)
	return sets;

    pmFreeLabelSets(sets, nsets);
    return NULL;
}

int
__pmAddLabels(pmLabelSet **lspp, const char *extras, int flags)
{
    pmLabelSet	*lsp = *lspp;
    pmLabel	labels[PM_MAXLABELS], *lp = NULL;
    char	*json = NULL;
    char	buffer[PM_MAXLABELJSONLEN];
    char	result[PM_MAXLABELJSONLEN];
    int		bytes, size, sts, i;

    if (lsp)
	json = lsp->json;

    if ((sts = __pmMergeLabels(json, extras, buffer, sizeof(buffer))) < 0)
	return sts;
    bytes = sts;

    if (lsp) {
	if (lsp->json) free(lsp->json);
	if (lsp->labels) free(lsp->labels);
    }

    size = sizeof(result);
    if ((sts = __pmParseLabels(buffer, bytes,
                                labels, PM_MAXLABELS, result, &size)) < 0) {
	if (lsp) memset(lsp, 0, sizeof(*lsp));
	return sts;
    }

    if (sts > 0) {
	bytes = sts * sizeof(pmLabel);
	if ((lp = malloc(bytes)) == NULL)
	    return -ENOMEM;
	memcpy(lp, labels, bytes);
	for (i = 0; i < sts; i++)
	    lp[i].flags |= flags;
    }

    if ((json = strndup(result, size + 1)) == NULL) {
	if (lsp) memset(lsp, 0, sizeof(*lsp));
	if (lp) free(lp);
	return -ENOMEM;
    }

    if (!lsp) {
	if ((lsp = malloc(sizeof(pmLabelSet))) == NULL) {
	    if (lp) free(lp);
	    free(json);
	    return -ENOMEM;
	}
	lsp->inst = PM_IN_NULL;
	lsp->padding = 0;
    }
    lsp->nlabels = sts;
    lsp->jsonlen = size;
    lsp->json = json;
    lsp->labels = lp;

    *lspp = lsp;
    return sts;
}

int
__pmParseLabelSet(const char *json, int jlen, int flags, pmLabelSet **set)
{
    pmLabelSet	*result;
    pmLabel	*lp, labels[MAXLABELSET];
    char	*bp, buf[PM_MAXLABELJSONLEN];
    int		i, sts, nlabels, bsz = PM_MAXLABELJSONLEN;

    sts = nlabels = (!jlen || !json) ? 0 :
	__pmParseLabels(json, jlen, labels, MAXLABELSET, buf, &bsz);
    if (sts < 0)
	return sts;

    if (nlabels > 0) {
	if ((lp = calloc(nlabels, sizeof(*lp))) == NULL)
	    return -ENOMEM;
	for (i = 0; i < nlabels; i++)
	    labels[i].flags |= flags;
    } else {
	lp = NULL;
    }

    if ((result = (pmLabelSet *)malloc(sizeof(*result))) == NULL) {
	if (lp) free(lp);
	return -ENOMEM;
    }
    if (nlabels > 0) {
	if ((bp = strndup(buf, bsz + 1)) == NULL) {
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
    result->padding = 0;
    result->labels = lp;

    *set = result;
    return 1;
}

static int
namecmp4(const pmLabel *ap, const char *as, const pmLabel *bp, const char *bs)
{
    int		sts;

    if (ap->namelen > bp->namelen) {
	if ((sts = strncmp(label_name(ap, as), label_name(bp, bs), bp->namelen)))
	    return sts;
	return 1;	/* ap longer, so remainder sorts larger */
    }
    if (ap->namelen < bp->namelen) {
	if ((sts = strncmp(label_name(ap, as), label_name(bp, bs), ap->namelen)))
	    return sts;
	return -1;	/* bp longer, so remainder sorts larger */
    }
    /* (ap->namelen == bp->namelen) */
    return strncmp(label_name(ap, as), label_name(bp, bs), ap->namelen);
}

static int
namecmp(const void *a, const void *b, void *arg)
{
    const pmLabel	*ap = (const pmLabel *)a;
    const pmLabel	*bp = (const pmLabel *)b;
    const char		*json = (const char *)arg;

    return namecmp4(ap, json, bp, json);
}

static int
stash_chars(const char *s, int slen, char **buffer, int *buflen)
{
    int		bytes = *buflen;
    char	*bp = *buffer;

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
stash_label(const pmLabel *lp, const char *json,
	    pmLabel *olabel, const char *obuffer, int *no,
	    char **buffer, int *buflen,
	    int (*filter)(const pmLabel *, const char *, void *), void *arg)
{
    char	*bp = *buffer;
    int		bytes = *buflen;
    int		valuelen;

    if (filter != NULL && filter(lp, json, arg) == 0)
	return 0;

    if ((lp->namelen + 2 + 1 + lp->valuelen + 1) >= bytes)
	return -E2BIG;

    if (lp->valuelen) {
	bytes = pmsprintf(bp, bytes, "\"%.*s\":%.*s,",
			(int)lp->namelen, label_name(lp, json),
			(int)lp->valuelen, label_value(lp, json));
	valuelen = lp->valuelen;
    } else {
	bytes = pmsprintf(bp, bytes, "\"%.*s\":null,",
			(int)lp->namelen, label_name(lp, json));
	valuelen = 4;
    }

    *buffer = bp + bytes;
    *buflen -= bytes;

    if (olabel) {
	pmLabel	*op = &olabel[*no];
	op->name = (bp - obuffer) + 1;
	op->namelen = lp->namelen;
	op->flags = lp->flags;
	op->value = (bp - obuffer) + 1 + lp->namelen + 2;
	op->valuelen = valuelen;
	*no = *no + 1;
    }

    return bytes;
}

static int
verify_label_name(pmLabel *lp, const char *json)
{
    const char	*sp = json + lp->name;
    const char	*start = sp;
    size_t	length = lp->namelen;

    if (length == 0)
	return -EINVAL;
    if (!isalpha((int)*sp))	/* first character must be alphanumeric */
	return -EINVAL;
    while (++sp < (start + length)) {
	if (isalnum((int)*sp) || *sp == '_')
	    continue;
	return -EINVAL;
    }
    return 0;
}

/*
 * Sort and verify labels in the set - no (internal) duplicate
 * names are allowed, and the naming rules must be satisfied.
 */
int
sort_labels(pmLabel *lp, int nlabels, const char *json)
{
    void	*data = (void *)json;
    int		i;

    sort_r(lp, nlabels, sizeof(pmLabel), namecmp, data);

    for (i = 0; i < nlabels-1; i++) {
	if (namecmp(&lp[i], &lp[i+1], data) == 0) {
	    if (pmDebugOptions.labels)
		pmNotifyErr(LOG_ERR, "Label name duplicated %.*s",
				(int)lp[i].namelen, label_name(&lp[i], json));
	    return -EINVAL;
	}
	if (verify_label_name(&lp[i], json) < 0) {
	    if (pmDebugOptions.labels)
		pmNotifyErr(LOG_ERR, "Label name is invalid %.*s",
			    (int)lp[i].namelen, label_name(&lp[i], json));
	    return -EINVAL;
	}
    }
    return 0;
}

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
 */
static int
__pmParseLabels(const char *s, int slen,
		pmLabel *labels, int maxlabels, char *buffer, int *buflen)
{
    enum { START, NAME, VALUE, ARRAY_VALUE, OBJECT_VALUE } state;
    jsmn_parser parser;
    jsmntok_t	*token, *tokens = NULL;
    char	*json = buffer;
    int		namelen = 0, jlen = *buflen;
    int		ntokens = 0, nlabels = 0;
    int		i, j, sz, sts, stack = 0;

    state = START;
    for (;;) {
	ntokens += INC_TOKENS;
	tokens = (jsmntok_t *)realloc(tokens, sizeof(jsmntok_t) * ntokens);
	if (!tokens)
	    return -ENOMEM;
	jsmn_init(&parser);
	sts = jsmn_parse(&parser, s, slen, tokens, ntokens);
	if (sts == JSMN_ERROR_NOMEM && ntokens < MAXLABELSET)
	    continue;	/* try again, larger buffer */
	if (sts == JSMN_ERROR_INVAL || sts == JSMN_ERROR_PART) {
	    sts = -EINVAL;
	    goto done;
	}
	break;
    }

    for (i = 0, j = 1; j > 0; i++, j--) {
	token = &tokens[i];
	assert(token->start != -1 && token->end != -1);

	if (token->type == JSMN_ARRAY || token->type == JSMN_OBJECT)
	    j += token->size;

	switch (state) {
	case START:
	    if (token->type != JSMN_OBJECT) {
		if (pmDebugOptions.labels)
		    pmNotifyErr(LOG_ERR, "Root element must be JSON object");
		sts = -EINVAL;
		goto done;
	    }
	    stash_chars("{", 1, &json, &jlen);
	    /* Keys are odd-numbered tokens within the object */
	    state = NAME;
	    break;

	case NAME:
	    namelen = token->end - token->start;
	    if (token->type != JSMN_STRING) {
		if (pmDebugOptions.labels)
		    pmNotifyErr(LOG_ERR, "Label name must be JSON string not type %d %.*s", token->type, (int)namelen, s + token->start);
		sts = -EINVAL;
		goto done;
	    }

	    if (namelen >= MAXLABELNAMELEN) {	/* will the name fit too? */
		if (pmDebugOptions.labels)
		    pmNotifyErr(LOG_ERR, "Label name is too long %.*s",
				(int)namelen, s + token->start);
		sts = -E2BIG;
		goto done;
	    }

	    if (nlabels >= maxlabels) {	/* will this one fit in the given array? */
		if (pmDebugOptions.labels)
		    pmNotifyErr(LOG_ERR, "Too many labels (%d)", nlabels);
		sts = -E2BIG;
		goto done;
	    }
	    memset(&labels[nlabels], 0, sizeof(pmLabel));

	    if (nlabels > 0)
		stash_chars(",", 1, &json, &jlen);

	    stash_chars("\"", 1, &json, &jlen);
	    labels[nlabels].name = json - buffer;
	    labels[nlabels].namelen = namelen;
	    stash_chars(s + token->start, namelen, &json, &jlen);
	    stash_chars("\":", 2, &json, &jlen);

	    if (!token->size) {
		stash_chars("null", 4, &json, &jlen);	/* valid JSON */
		nlabels++;	/* no value given, move to next label */
	    } else {
		state = VALUE;
		stack = 0;	/* value stack, for compound elements */
		j += token->size;
	    }
	    break;

	case VALUE:
	case ARRAY_VALUE:
	case OBJECT_VALUE:
	    sz = token->end - token->start;
	    if (labels[nlabels].value == 0)
		labels[nlabels].value = json - buffer;
	    if (token->type == JSMN_PRIMITIVE) {
		sts = stash_chars(s + token->start, sz, &json, &jlen);
	    } else if (token->type == JSMN_STRING) {
		stash_chars("\"", 1, &json, &jlen);
		sts = stash_chars(s + token->start, sz, &json, &jlen);
		stash_chars("\"", 1, &json, &jlen);
		sts += 2;	/* include double quotes */
	    } else if (token->type == JSMN_ARRAY && !stack) {
		state = ARRAY_VALUE;
		sts = stash_chars("[", 1, &json, &jlen);
	    } else if (token->type == JSMN_OBJECT && !stack) {
		state = OBJECT_VALUE;
		sts = stash_chars("{", 1, &json, &jlen);
	    } else {	/* deeper nesting in values is disallowed */
		sts = -EINVAL;
		goto done;
	    }
	    labels[nlabels].valuelen += sts;

	    if (state != VALUE && stack > 0) {
		if (--stack + token->size > 0) {
		    if (state == OBJECT_VALUE && token->size > 0)
			stash_chars(":", 1, &json, &jlen);
		    else
			stash_chars(",", 1, &json, &jlen);
		    labels[nlabels].valuelen++;
		    j += token->size;
		}
	    }

	    stack += token->size + 1;	/* +1 for current token */
	    if (--stack == 0) {	/* completely parsed the value component */
		if (state != VALUE) {
		    if (state == OBJECT_VALUE)
			stash_chars("}", 1, &json, &jlen);
		    else
			stash_chars("]", 1, &json, &jlen);
		    labels[nlabels].valuelen++;
		}
		state = NAME;
		nlabels++;
	    }
	    break;
	}
    }
    if (nlabels == 0) {
	/*
	 * Zero labels. Happens if we are passed an empty labelset string.
	 * Argument buffer is set to a zero length string and we return 0
	 */
	sts = nlabels;
	*buflen = 0;
	buffer[0] = '\0';
	goto done;
    }
    if (sts < 0)
	goto done;

    if (json - buffer > 0)
	stash_chars("}", 2, &json, &jlen);

    if ((sts = sort_labels(labels, nlabels, (void *)buffer)) < 0)
	goto done;

    /* pass out JSONB buffer space used and pmLabel entries consumed */
    *buflen = json - buffer;
    sts = nlabels;

done:
    if (tokens)
	free(tokens);
    return sts;
}

int
__pmMergeLabelSets(pmLabel *alabels, const char *abuf, int na,
		   pmLabel *blabels, const char *bbuf, int nb,
		   pmLabel *olabels, char *output, int *no, int buflen,
	int (*filter)(const pmLabel *, const char *, void *), void *arg)
{
    char	*bp = output;
    int		sts, i, j;

    if (no)
	*no = 0;	/* number of output labels */

    /* Walk over both label sets inserting all names into the output
     * buffer, but prefering b-group values over those in the a-group.
     * As we go, check for duplicates between a-group & b-group (since
     * thats invalid and we'd generate invalid output - JSONB format).
     */
    if ((sts = stash_chars("{", 1, &bp, &buflen)) < 0)
	goto done;
    i = j = 0;
    do {
	if (i < na) {
	    /* use this a-group label? - compare to current b-group name */
	    if (j >= nb) {	/* reached end of b-group, so use it */
		if (j && namecmp4(&alabels[i], abuf, &blabels[j-1], bbuf) == 0)
		    i++;	/* but skip if its a duplicate name */
		else if ((sts = stash_label(&alabels[i++], abuf,
					    olabels, output, no,
					    &bp, &buflen, filter, arg)) < 0)
		    goto done;
	    } else if (namecmp4(&alabels[i], abuf, &blabels[j], bbuf) < 0) {
		if (j && namecmp4(&alabels[i], abuf, &blabels[j-1], bbuf) == 0) /* dup */
		    i++;
		else if ((sts = stash_label(&alabels[i++], abuf,
					    olabels, output, no,
					    &bp, &buflen, filter, arg)) < 0)
		    goto done;
	    }
	}
	if (j < nb) {
	    /* use this b-group label? - compare to current a-group name */
	    if (i >= na) {	/* reached end of a-group, so use it */
		if ((sts = stash_label(&blabels[j++], bbuf,
					olabels, output, no,
					&bp, &buflen, filter, arg)) < 0)
		    goto done;
	    } else if (namecmp4(&alabels[i], abuf, &blabels[j], bbuf) >= 0) {
		if ((sts = stash_label(&blabels[j++], bbuf,
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
    if ((sts = stash_chars("}", 2, &bp, &buflen)) < 0)
	goto done;
    sts = bp - output;

done:
    return sts;
}

static int
__pmMergeLabels(const char *a, const char *b, char *buffer, int buflen)
{
    pmLabel	alabels[MAXLABELSET], blabels[MAXLABELSET];
    char	abuf[PM_MAXLABELJSONLEN], bbuf[PM_MAXLABELJSONLEN];
    int		abufsz = PM_MAXLABELJSONLEN, bbufsz = PM_MAXLABELJSONLEN;
    int		sts, na = 0, nb = 0;

    if (!a || (na = strlen(a)) == 0)
	abufsz = 0;
    else if ((sts = na = __pmParseLabels(a, na,
				alabels, MAXLABELSET, abuf, &abufsz)) < 0)
	return sts;

    if (!b || (nb = strlen(b)) == 0)
	bbufsz = 0;
    else if ((sts = nb = __pmParseLabels(b, strlen(b),
				blabels, MAXLABELSET, bbuf, &bbufsz)) < 0)
	return sts;

    if (!abufsz && !bbufsz)
	return 0;

    return __pmMergeLabelSets(alabels, abuf, na, blabels, bbuf, nb,
				NULL, buffer, NULL, buflen, NULL, NULL);
}

/*
 * Walk the "sets" array left to right (increasing precendence)
 * and produce the merged set into the supplied buffer.
 * An optional user-supplied callback routine allows fine-tuning
 * of the resulting set of labels.
 */
int
pmMergeLabelSets(pmLabelSet **sets, int nsets, char *buffer, int buflen,
	int (*filter)(const pmLabel *, const char *, void *), void *arg)
{
    pmLabel	olabels[MAXLABELSET];
    pmLabel	blabels[MAXLABELSET];
    char	buf[PM_MAXLABELJSONLEN];
    int		nlabels = 0;
    int		i, sts = 0;

    if (!sets || nsets < 1)
	return -EINVAL;

    for (i = 0; i < nsets; i++) {
	if (sets[i] == NULL)
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
	    fprintf(stderr, "pmMergeLabelSets: merging set [%d] ", i);
	    __pmDumpLabelSet(stderr, sets[i]);
	}

	/*
	 * Merge sets[i] with blabels into olabels. Any duplicate label
	 * names in sets[i] prevail over those in blabels.
	 */
	if ((sts = __pmMergeLabelSets(blabels, buf, nlabels,
				sets[i]->labels, sets[i]->json, sets[i]->nlabels,
				olabels, buffer, &nlabels, buflen,
				filter, arg)) < 0)
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
    char	buf[PM_MAXLABELJSONLEN];
    int		bytes = 0;
    int		i, sts;

    if (!sets || nsets < 1)
	return -EINVAL;

    memset(buf, 0, sizeof(buf));
    for (i = 0; i < nsets; i++) {
	if (bytes)
	    memcpy(buf, buffer, bytes);
	if ((sts = bytes = __pmMergeLabels(buf, sets[i], buffer, buflen)) < 0)
	    return sts;
	if (bytes >= buflen || bytes >= PM_MAXLABELJSONLEN)
	    return -E2BIG;
    }
    return bytes;
}

static size_t
labelfile(const char *path, const char *file, char *buf, int buflen)
{
    FILE		*fp;
    char		lf[MAXPATHLEN];
    size_t		bytes;

    pmsprintf(lf, sizeof(lf), "%s%c%s", path, pmPathSeparator(), file);
    if ((fp = fopen(lf, "r")) == NULL)
	return 0;
    bytes = fread(buf, 1, buflen-1, fp);
    fclose(fp);
    buf[bytes] = '\0';
    if (pmDebugOptions.labels)
	fprintf(stderr, "labelfile: loaded from %s:\n%s", file, buf);
    return bytes;
}

int
__pmGetContextLabels(pmLabelSet **set)
{
    struct dirent	**list = NULL;
    char		**labels;
    char		path[MAXPATHLEN];
    char		buf[PM_MAXLABELJSONLEN];
    size_t		length;
    int			i, num, sts = 0;

    pmsprintf(path, MAXPATHLEN, "%s%clabels",
		pmGetConfig("PCP_SYSCONF_DIR"), pmPathSeparator());
    if ((num = scandir(path, &list, NULL, alphasort)) < 0)
	return -oserror();

    if ((labels = calloc(num, sizeof(char *))) == NULL) {
	for (i = 0; i < num; i++) free(list[i]);
	free(list);
	return -oserror();
    }

    for (i = 0; i < num; i++) {
	if (list[i]->d_name[0] == '.')
	    continue;
	length = labelfile(path, list[i]->d_name, buf, sizeof(buf));
	labels[i] = strndup(buf, length + 1);
    }
    if ((sts = pmMergeLabels(labels, num, buf, sizeof(buf))) < 0) {
	pmNotifyErr(LOG_WARNING, "Failed to merge %s labels file: %s",
			path, pmErrStr(sts));
    }
    for (i = 0; i < num; i++) {
	if (labels[i]) free(labels[i]);
	free(list[i]);
    }
    free(labels);
    free(list);

    if (sts <= 0)
	return sts;

    return __pmParseLabelSet(buf, sts, PM_LABEL_CONTEXT, set);
}

static char *
archive_host_labels(__pmContext *ctxp, char *buffer, int buflen)
{
    /*
     * Backward compatibility fallback, for archives created before
     * labels support is added to pmlogger.
     * Once that's implemented (TYPE_LABEL in .meta) fields will be
     * added to the context structure and we'll be able to read 'em
     * here to provide complete archive label support.
     */
    pmsprintf(buffer, buflen, "{\"hostname\":\"%s\"}",
	     ctxp->c_archctl->ac_log->l_label.ill_hostname);
    buffer[buflen-1] = '\0';
    return buffer;
}

static int
archive_context_labels(__pmContext *ctxp, pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    char	*hostp;
    int		sts;

    hostp = archive_host_labels(ctxp, buf, sizeof(buf));
    if ((sts = __pmAddLabels(&lp, hostp, PM_LABEL_CONTEXT)) < 0)
	return sts;
    *sets = lp;
    return 1;
}

static char *
local_host_labels(char *buffer, int buflen)
{
    char	host[MAXHOSTNAMELEN];
    char	domain[MAXDOMAINNAMELEN];
    char	machineid[MAXMACHINEIDLEN];

    if (gethostname(host, sizeof(host)) < 0)
	pmsprintf(host, sizeof(host), "localhost");
    else
	host[sizeof(host)-1] = '\0';
    if (getdomainname(domain, sizeof(domain)) < 0 || domain[0] == '\0' || strcmp(domain, "(none)") == 0)
	pmsprintf(domain, sizeof(domain), "localdomain");
    else
	domain[sizeof(domain)-1] = '\0';
    if (getmachineid(machineid, sizeof(machineid)) < 0)
	pmsprintf(machineid, sizeof(machineid), "localmachine");
    else
	machineid[sizeof(machineid)-1] = '\0';
    pmsprintf(buffer, buflen,
	    "{\"hostname\":\"%s\",\"domainname\":\"%s\",\"machineid\":\"%s\"}",
	    host, domain, machineid);
    return buffer;
}

static int
local_context_labels(pmLabelSet **sets)
{
    pmLabelSet	*lp = NULL;
    char	buf[PM_MAXLABELJSONLEN];
    char	*hostp;
    int		sts;

    if ((sts = __pmGetContextLabels(&lp)) < 0)
	return sts;
    hostp = local_host_labels(buf, sizeof(buf));
    if ((sts = __pmAddLabels(&lp, hostp, PM_LABEL_CONTEXT)) > 0) {
	*sets = lp;
	return 1;
    }
    pmFreeLabelSets(lp, 1);
    return sts;
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
    if (type & PM_LABEL_INDOM)
	return pmInDom_domain(ident);
    if (type & (PM_LABEL_CLUSTER | PM_LABEL_ITEM | PM_LABEL_INSTANCES))
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
	else if ((sts = __pmSendLabelReq(fd, handle, ident, type)) < 0)
	    sts = __pmMapErrno(sts);
	else {
	    int x_ident = ident, x_type = type;
	    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
	    sts = __pmRecvLabel(fd, ctxp, tout, &x_ident, &x_type, sets, nsets);
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
	if ((sts = __pmLogLookupLabel(ctxp->c_archctl, type,
				ident, sets, &ctxp->c_origin)) < 0) {
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
    int		n, sts, count, total;

    if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	return sts;

    /* context, domain, [indom], cluster, item, [insts...] */
    total = (desc.indom == PM_INDOM_NULL) ? 4 : 6;
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

    if (desc.indom != PM_INDOM_NULL) {
	if ((sts = n = pmGetInstancesLabels(desc.indom, &lsp)) < 0)
	    goto fail;
	if (lsp && n + count > total) {
	    /* make space on the end for additional instance sets */
	    sets = realloc(sets, (count + n) * sizeof(pmLabelSet));
	    if (sets == NULL) {
		free(lsp);
		lsp = NULL;
		sts = -ENOMEM;
		goto fail;
	    }
	}
	if (lsp) {
	    memcpy(&sets[count], lsp, n * sizeof(pmLabelSet));
	    count += n;
	    free(lsp);
	    lsp = NULL;
	}
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
