/*
 * Copyright (c) 2019 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "openmetrics.h"
#include "libpcp.h"

static void
labelname(const pmLabel *lp, const char *json, __pmHashCtl *lc,
		const char **name, int *length)
{
    __pmHashNode	*hp;

    if (!(lp->flags & PM_LABEL_COMPOUND) || lc == NULL ||
	(hp = __pmHashSearch(lp->name, lc)) == NULL) {
	*name = json + lp->name;
	*length = lp->namelen;
    } else {
	*name = hp->data;       /* compound "a.b.c" style label name */
	*length = strlen(hp->data);
    }
}

static void
labelvalue(const pmLabel *lp, const char *json, const char **value, int *length)
{
    const char		*offset;
    int			bytes;

    bytes = lp->valuelen;
    offset = json + lp->value;
    if (*offset == '\"' && bytes >= 2 && offset[bytes-1] == '\"') {
	bytes -= 2;
	offset++;
    }
    *value = offset;
    *length = bytes;
}

static void
labeladd(void *arg, const struct dictEntry *entry)
{
    pmWebLabelSet	*labels = (pmWebLabelSet *)arg;

    if (sdslen(labels->buffer) == 0)	/* first time */
	sdscatfmt(labels->buffer, "%S=%S", entry->key, entry->v.val);
    else
	sdscatfmt(labels->buffer, ",%S=%S", entry->key, entry->v.val);
}

/* convert an array of PCP labelsets into Open Metrics form */
void
open_metrics_labels(pmWebLabelSet *labels, struct dict *dict)
{
    unsigned long	cursor = 0;
    pmLabelSet		*labelset;
    pmLabel		*label;
    const char		*offset;
    sds			key, value;
    int			i, j, length;

    /* walk labelset in order adding labels to a temporary dictionary */
    for (i = 0; i < labels->nsets; i++) {
	labelset = labels->sets[i];
	for (j = 0; j < labelset->nlabels; j++) {
	    label = &labelset->labels[j];

	    /* extract the label name */
	    labelname(label, labelset->json, labelset->hash, &offset, &length);
	    key = sdsnewlen(offset, length);

	    /* extract the label value without any surrounding quotes */
	    labelvalue(label, labelset->json, &offset, &length);
	    value = sdscatrepr(sdsempty(), offset, length);

	    /* overwrite entries from earlier passes: label hierarchy */
	    dictReplace(dict, key, value);
	}
    }

    /* finally produce the merged set of labels in the desired format */
    do {
	cursor = dictScan(dict, cursor, labeladd, NULL, labels);
    } while (cursor);
}

/* check if PCP metric type has valid Open Metrics form */
int
open_metrics_type_check(sds type)
{
    static const char * const	typename[] = {
	"32", "u32", "64", "u64", "float", "double"
    };
    int		i;

    for (i = 0; i < sizeof(typename) / sizeof(typename[0]); i++)
	if (strcmp(type, typename[i]) == 0)
	    return 0;
    return -ESRCH;
}

/* convert PCP metric name to Open Metrics form */
sds
open_metrics_name(sds metric, int compat)
{
    sds		p, name = sdsdup(metric);
    char	sep = compat ? ':' : '_';

    for (p = name; p && *p; p++) {
	/* swap dots with underscores in name */
	if (*p == '.')
	    *p = sep;
    }
    return name;
}

/* convert PCP metric type to Open Metrics form */
sds
open_metrics_semantics(sds sem)
{
    if (strncmp(sem, "instant", 7) == 0 || strncmp(sem, "discrete", 8) == 0)
	return sdsnew("gauge");
    return sdsnew("counter");
}
