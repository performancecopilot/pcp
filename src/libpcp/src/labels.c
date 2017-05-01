/*
 * Copyright (c) 2016-2017 Red Hat.
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
#include "impl.h"
#include "pmda.h"
#include "internal.h"
#include "fault.h"
#include "jsmn.h"

void
__pmFreeLabelSet(__pmLabelSet *labelset)
{
    if (labelset->nlabels > 0)
	free(labelset->labels);
    if (labelset->json)
	free(labelset->json);
    free(labelset);
}

void
__pmFreeLabelSetArray(__pmLabelSet *labelsets, int nsets)
{
    int		i;

    for (i = 0; i < nsets; i++) {
	__pmLabelSet *lp = &labelsets[i];
	if (lp->nlabels > 0)
	    free(lp->labels);
	if (lp->json)
	    free(lp->json);
    }
    if (nsets > 0)
	free(labelsets);
}

int
__pmAddLabels(__pmLabelSet **lspp, const char *extras)
{
    __pmLabelSet	*lsp = *lspp;
    __pmLabel		labels[MAXLABELS], *lp = NULL;
    char		*json = NULL;
    char		buffer[MAXLABELJSONLEN];
    char		result[MAXLABELJSONLEN];
    int			bytes, size, sts;

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
                                labels, MAXLABELS, result, &size)) < 0) {
	if (lsp) memset(lsp, 0, sizeof(*lsp));
	return sts;
    }

    if (sts > 0) {
	bytes = sts * sizeof(__pmLabel);
	if ((lp = malloc(bytes)) == NULL)
	    return -ENOMEM;
	memcpy(lp, labels, bytes);
    }

    if ((json = strndup(result, size + 1)) == NULL) {
	if (lsp) memset(lsp, 0, sizeof(*lsp));
	if (lp) free(lp);
	return -ENOMEM;
    }

    if (!lsp) {
	if ((lsp = malloc(sizeof(pmdaLabelSet))) == NULL) {
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
    return 1;
}

int
__pmParseLabelSet(const char *json, int jlen, __pmLabelSet **set)
{
    __pmLabelSet *result;
    __pmLabel	*lp, labels[MAXLABELS];
    char	*bp, buf[MAXLABELJSONLEN];
    int		sts, nlabels, bsz = MAXLABELJSONLEN;

    sts = nlabels = (!jlen || !json) ? 0 :
	__pmParseLabels(json, jlen, labels, MAXLABELS, buf, &bsz);
    if (sts < 0)
	return sts;

    if (nlabels > 0) {
	if ((lp = calloc(nlabels, sizeof(*lp))) == NULL)
	    return -ENOMEM;
    } else {
	lp = NULL;
    }

    if ((result = (__pmLabelSet *)malloc(sizeof(*result))) == NULL) {
	if (lp) free(lp);
	return -ENOMEM;
    }
    if (nlabels > 0) {
	if ((bp = strndup(buf, bsz + 1)) == NULL) {
	    free(result);
	    return -ENOMEM;
	}
    } else {
	bsz = 0;
	bp = NULL;
    }

    result->inst = PM_IN_NULL;
    result->nlabels = nlabels;
    if (nlabels > 0)
	memcpy(lp, labels, nlabels * sizeof(__pmLabel));
    result->json = (char *)bp;
    result->jsonlen = bsz;
    result->padding = 0;
    result->labels = lp;

    *set = result;
    return 1;
}

#define MAX_TOKENS	(MAXLABELS * 4)
#define INC_TOKENS	64

#define label_name(lp, json)	(json+(lp)->name)
#define label_value(lp, json)	(json+(lp)->value)

static int
namecmp4(const __pmLabel *ap, const char *as, const __pmLabel *bp, const char *bs)
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
    const __pmLabel	*ap = (const __pmLabel *)a;
    const __pmLabel	*bp = (const __pmLabel *)b;
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
    bytes = sprintf(bp, "%.*s", slen, s);
    *buffer = bp + bytes;
    *buflen -= bytes;
    return bytes;
}

static int
stash_label(__pmLabel *lp, const char *json, char **buffer, int *buflen)
{
    int		bytes = *buflen;
    char	*bp = *buffer;

    if ((lp->namelen + 2 + 1 + lp->valuelen + 1) >= bytes)
	return -E2BIG;
    if (lp->valuelen)
	bytes = sprintf(bp, "\"%.*s\":%.*s,",
			(int)lp->namelen, label_name(lp, json),
			(int)lp->valuelen, label_value(lp, json));
    else
	bytes = sprintf(bp, "\"%.*s\":null,",
			(int)lp->namelen, label_name(lp, json));
    *buffer = bp + bytes;
    *buflen -= bytes;
    return bytes;
}

static int
verify_label_name(__pmLabel *lp, const char *json)
{
    const char	*sp = json + lp->name;
    const char	*start = sp;
    size_t	length = lp->namelen;

    if (length == 0)
	return -EINVAL;
    if (!isalpha(*sp))	/* first character must be alphanumeric */
	return -EINVAL;
    while (++sp < (start + length)) {
	if (isalnum(*sp) || *sp == '_')
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
sort_labels(__pmLabel *lp, int nlabels, const char *json)
{
    void	*data = (void *)json;
    int		i;

    qsort_r(lp, nlabels, sizeof(__pmLabel), namecmp, data);

    for (i = 0; i < nlabels-1; i++) {
	if (namecmp(&lp[i], &lp[i+1], data) == 0) {
	    if (pmDebug & DBG_TRACE_VALUE)
		    __pmNotifyErr(LOG_ERR, "Label name duplicated %.*s",
				(int)lp[i].namelen, label_name(&lp[i], json));
	    return -EINVAL;
	}
	if (verify_label_name(&lp[i], json) < 0) {
	    if (pmDebug & DBG_TRACE_VALUE)
		__pmNotifyErr(LOG_ERR, "Label name is invalid %.*s",
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
int
__pmParseLabels(const char *s, int slen,
		__pmLabel *labels, int maxlabels, char *buffer, int *buflen)
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
	if (sts == JSMN_ERROR_NOMEM && ntokens < MAX_TOKENS)
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
		if (pmDebug & DBG_TRACE_VALUE)
		    __pmNotifyErr(LOG_ERR, "Root element must be JSON object");
		sts = -EINVAL;
		goto done;
	    }
	    stash_chars("{", 1, &json, &jlen);
	    /* Keys are odd-numbered tokens within the object */
	    ntokens = token->size;
	    state = NAME;
	    break;

	case NAME:
	    if (token->type != JSMN_STRING) {
		if (pmDebug & DBG_TRACE_VALUE)
		    __pmNotifyErr(LOG_ERR, "Label name must be JSON string");
		sts = -EINVAL;
		goto done;
	    }

	    namelen = token->end - token->start;
	    if (namelen >= MAXLABELNAMELEN) {	/* will the name fit too? */
		if (pmDebug & DBG_TRACE_VALUE)
		    __pmNotifyErr(LOG_ERR, "Label name is too long %.*s",
				(int)namelen, s + token->start);
		sts = -E2BIG;
		goto done;
	    }

	    if (nlabels >= maxlabels) {	/* will this one fit in the given array? */
		if (pmDebug & DBG_TRACE_VALUE)
		    __pmNotifyErr(LOG_ERR, "Too many labels (%d)", nlabels);
		sts = -E2BIG;
		goto done;
	    }
	    memset(&labels[nlabels], 0, sizeof(__pmLabel));

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
    if (!nlabels || sts < 0)
	goto done;

    if (json - buffer > 0)
	stash_chars("}", 2, &json, &jlen);

    if (sort_labels(labels, nlabels, (void *)buffer) < 0)
	goto done;

    /* pass out JSONB buffer space used and __pmLabel entries consumed */
    *buflen = json - buffer;
    sts = nlabels;

done:
    if (tokens)
	free(tokens);
    return sts;
}

int
__pmMergeLabels(const char *a, const char *b, char *buffer, int buflen)
{
    __pmLabel	alabels[MAXLABELS], blabels[MAXLABELS];
    char	abuf[MAXLABELJSONLEN], bbuf[MAXLABELJSONLEN];
    char	*bp = buffer;
    int		abufsz = MAXLABELJSONLEN, bbufsz = MAXLABELJSONLEN;
    int		sts, na = 0, nb = 0, i, j;

    if (!a || (na = strlen(a)) == 0)
	abufsz = 0;
    else if ((sts = na = __pmParseLabels(a, na,
				alabels, MAXLABELS, abuf, &abufsz)) < 0)
	goto done;

    if (!b || (nb = strlen(b)) == 0)
	bbufsz = 0;
    else if ((sts = nb = __pmParseLabels(b, strlen(b),
				blabels, MAXLABELS, bbuf, &bbufsz)) < 0)
	goto done;

    if (!abufsz && !bbufsz)
	return 0;

    /* Walk over both label sets inserting all names into the result
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
					    &bp, &buflen)) < 0)
		    goto done;
	    } else if (namecmp4(&alabels[i], abuf, &blabels[j], bbuf) < 0) {
		if (j && namecmp4(&alabels[i], abuf, &blabels[j-1], bbuf) == 0) /* dup */
		    i++;
		else if ((sts = stash_label(&alabels[i++], abuf,
					    &bp, &buflen)) < 0)
		    goto done;
	    }
	}
	if (j < nb) {
	    /* use this b-group label? - compare to current a-group name */
	    if (i >= na) {	/* reached end of a-group, so use it */
		if ((sts = stash_label(&blabels[j++], bbuf, &bp, &buflen)) < 0)
		    goto done;
	    } else if (namecmp4(&alabels[i], abuf, &blabels[j], bbuf) >= 0) {
		if ((sts = stash_label(&blabels[j++], bbuf, &bp, &buflen)) < 0)
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
    sts = bp - buffer;

done:
    return sts;
}

/*
 * Walk the "sets" array left to right (increasing precendence)
 * and produce the merged set into the supplied buffer.
 */
int
pmMergeLabels(char **sets, int nsets, char *buffer, int buflen)
{
    char	buf[MAXLABELJSONLEN];
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
	if (bytes >= buflen || bytes >= MAXLABELJSONLEN)
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

    snprintf(lf, sizeof(lf), "%s%c%s", path, __pmPathSeparator(), file);
    if ((fp = fopen(lf, "r")) == NULL)
	return 0;
    bytes = fread(buf, 1, buflen-1, fp);
    fclose(fp);
    buf[bytes] = '\0';
    return bytes;
}

int
__pmGetContextLabels(__pmLabelSet **set)
{
    struct dirent	**list = NULL;
    char		**labels;
    char		path[MAXPATHLEN];
    char		buf[MAXLABELJSONLEN];
    size_t		length;
    int			i, num, sts = 0;

    snprintf(path, MAXPATHLEN, "%s%clabels",
		pmGetConfig("PCP_SYSCONF_DIR"), __pmPathSeparator());
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
	__pmNotifyErr(LOG_WARNING, "Failed to merge %s labels file: %s",
			path, pmErrStr(sts));
    }
    for (i = 0; i < num; i++) {
	if (labels[i]) free(labels[i]);
	free(list[i]);
    }
    free(labels);
    free(list);

    if (sts < 0)
	return sts;

    return __pmParseLabelSet(buf, sts, set);
}

static int
local_context_labels(__pmLabelSet **set)
{
    char	buf[MAXLABELJSONLEN];
    char	host[MAXHOSTNAMELEN];
    int		sts;

    if ((sts = __pmGetContextLabels(set)) < 0)
	return sts;

    gethostname(host, sizeof(host));
    host[sizeof(host)-1] = '\0';

    snprintf(buf, sizeof(buf), "{\"hostname\":\"%s\"}", host);
    buf[sizeof(buf)-1] = '\0';

    return __pmAddLabels(set, buf);
}

int
__pmGetDomainLabels(int domain, const char *name, __pmLabelSet **set)
{
    size_t		length;
    char		buf[MAXLABELJSONLEN];
  
    (void)domain;	/* not currently used */
    length = snprintf(buf, sizeof(buf), "{\"agent\":\"%s\"}", name);
    buf[sizeof(buf)-1] = '\0';

    return __pmParseLabelSet(buf, length, set);
} 

static int
local_domain_labels(__pmDSO *dp, __pmLabelSet **set)
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
    if (type & (PM_LABEL_PMID | PM_LABEL_INSTS))
	return pmid_domain(ident);
    return -EINVAL;
}

static int
getlabels(int ident, int type, __pmLabelSet **sets, int *nsets)
{
    __pmContext	*ctxp;
    int		ctx, sts;

    if ((sts = ctx = pmWhichContext()) < 0)
	return sts;
    if ((ctxp = __pmHandleToPtr(sts)) == NULL)
	return PM_ERR_NOCONTEXT;

    if (ctxp->c_type == PM_CONTEXT_HOST) {
	PM_LOCK(ctxp->c_pmcd->pc_lock);
	int	handle = __pmPtrToHandle(ctxp);
	int	tout = ctxp->c_pmcd->pc_tout_sec;
	int	fd = ctxp->c_pmcd->pc_fd;

	if (!(__pmFeaturesIPC(fd) & PDU_FLAG_LABEL))
	    sts = PM_ERR_NOLABELS;	/* lack pmcd support */
	else if ((sts = __pmSendLabelReq(fd, handle, ident, type)) < 0)
	    sts = __pmMapErrno(sts);
	else {
	    int x_ident = ident, x_type = type;
	    PM_FAULT_POINT("libpcp/" __FILE__ ":1", PM_FAULT_TIMEOUT);
	    sts = __pmRecvLabel(fd, ctxp, tout, &x_ident, &x_type, sets, nsets);
	}
	PM_UNLOCK(ctxp->c_pmcd->pc_lock);
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
	/* assume PM_CONTEXT_ARCHIVE - no label metadata support currently */
	sts = PM_ERR_NOLABELS;
    }

    PM_UNLOCK(ctxp->c_lock);
    return sts;
}

static int
dolabels(int ident, int type, char **labels)
{
    __pmLabelSet	*sets = NULL;
    int			sts, nsets;

    if ((sts = getlabels(ident, type, &sets, &nsets)) < 0)
	return sts;

    if (nsets) {
	assert(nsets == 1);
	*labels = sets[0].json;
	free(sets[0].labels);
	free(sets);
	return 1;
    }

    *labels = NULL;
    return 0;
}

int
pmGetContextLabels(char **labels)
{
    return dolabels(PM_ID_NULL, PM_LABEL_CONTEXT, labels);
}

int
pmGetDomainLabels(int domain, char **labels)
{
    return dolabels(domain, PM_LABEL_DOMAIN, labels);
}

int
pmGetInDomLabels(pmInDom indom, char **labels)
{
    return dolabels(indom, PM_LABEL_INDOM, labels);
}

int
pmGetPMIDLabels(pmID pmid, char **labels)
{
    return dolabels(pmid, PM_LABEL_PMID, labels);
}

int
pmGetLabels(pmID pmid, int **instlist, char ***labelslist)
{
    __pmLabelSet	*sets = NULL;
    char		**llist = NULL, *p;
    int			*ilist = NULL;
    int			i, sts, nsets, need;

    if ((sts = getlabels(pmid, PM_LABEL_INSTS, &sets, &nsets)) < 0)
	return sts;

    /* allocate space needed for returned inst ID and labels lists */
    for (i = need = 0; i < nsets; i++)
	need += sizeof(char *) + sets[i].jsonlen + 1;
    if ((ilist = (int *)malloc(nsets * sizeof(sets[0].inst))) == NULL ||
	(llist = (char **)malloc(need)) == NULL) {
	if (ilist) free(ilist);
	if (llist) free(llist);
	sts = -oserror();
	goto done;
    }

    /* copy out, with space arranged in the style of pmGetInDom(3) */
    p = (char *)&llist[nsets];
    for (i = 0; i < nsets; i++) {
	ilist[i] = sets[i].inst;
	strncpy(p, sets[i].json, sets[i].jsonlen);
	llist[i] = p;
	p += sets[i].jsonlen;
	*p++ = '\0';
    }
    *labelslist = llist;
    *instlist = ilist;
    sts = nsets;

done:
    __pmFreeLabelSetArray(sets, nsets);
    return sts;
}
