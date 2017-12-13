/*
 * Dynamic namespace metrics, PMDA helper routines.
 *
 * Copyright (c) 2013-2015,2017 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
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
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "libdefs.h"

static struct dynamic {
    const char		*prefix;
    int			prefixlen;
    int			mtabcount;	/* internal use only */
    int			extratrees;	/* internal use only */
    pmdaUpdatePMNS	pmnsupdate;
    pmdaUpdateText	textupdate;
    pmdaUpdateMetric	mtabupdate;
    pmdaCountMetrics	mtabcounts;
    pmdaNameSpace	*pmns;
    pmdaMetric		*metrics;	/* original fixed table */
    int			nmetrics;	/* fixed metrics number */
    unsigned int	clustermask;	/* mask out parts of PMID cluster */
} *dynamic_table;

static int dynamic_count;

static void
dynamic_lookup(pmdaExt *pmda, struct dynamic **table, int *count)
{
    e_ext_t *e = (e_ext_t *)pmda->e_ext;

    if (e->ndynamics) {
	*count = e->ndynamics;
	*table = e->dynamics;
    } else {
	*count = dynamic_count;
	*table = dynamic_table;
    }
}

int
dynamic_set_cluster_mask(struct dynamic *table, int count,
			const char *prefix, unsigned int mask)
{
    int i;

    for (i = 0; i < count; i++) {
	if (strcmp(prefix, table[i].prefix) == 0)
	    continue;
	table[i].clustermask = mask;
	return 0;
    }
    return -EINVAL;
}

int
pmdaDynamicSetClusterMask(const char *prefix, unsigned int mask)
{
    return dynamic_set_cluster_mask(dynamic_table, dynamic_count, prefix, mask);
}

int
pmdaExtDynamicSetClusterMask(const char *prefix, unsigned int mask, pmdaExt *pmda)
{
    e_ext_t *e = (e_ext_t *)pmda->e_ext;

    return dynamic_set_cluster_mask(e->dynamics, e->ndynamics, prefix, mask);
}

void
dynamic_namespace(struct dynamic **table, int *count,
	const char *prefix, int *clusters, int nclusters,
	pmdaUpdatePMNS pmnsupdate, pmdaUpdateText textupdate,
	pmdaUpdateMetric mtabupdate, pmdaCountMetrics mtabcounts,
	pmdaMetric *metrics, int nmetrics)
{
    int ndynamic = *count;
    int size = (ndynamic + 1) * sizeof(struct dynamic);
    struct dynamic *dynamic;

    if ((dynamic = (struct dynamic *)realloc(*table, size)) == NULL) {
	pmNotifyErr(LOG_ERR, "out-of-memory registering dynamic metrics");
	return;
    }
    dynamic[ndynamic].prefix = prefix;
    dynamic[ndynamic].prefixlen = strlen(prefix);
    dynamic[ndynamic].pmnsupdate = pmnsupdate;
    dynamic[ndynamic].textupdate = textupdate;
    dynamic[ndynamic].mtabupdate = mtabupdate;
    dynamic[ndynamic].mtabcounts = mtabcounts;
    dynamic[ndynamic].pmns = NULL;
    dynamic[ndynamic].metrics = metrics;
    dynamic[ndynamic].nmetrics = nmetrics;
    dynamic[ndynamic].clustermask = 0;

    *count = ndynamic + 1;
    *table = dynamic;
}

void
pmdaExtDynamicPMNS(const char *prefix, int *clusters, int nclusters,
	pmdaUpdatePMNS pmnsupdate, pmdaUpdateText textupdate,
	pmdaUpdateMetric mtabupdate, pmdaCountMetrics mtabcounts,
	pmdaMetric *metrics, int nmetrics, pmdaExt *pmda)
{
    e_ext_t *e = (e_ext_t *)pmda->e_ext;

    dynamic_namespace(&e->dynamics, &e->ndynamics,
		prefix, clusters, nclusters, pmnsupdate, textupdate,
		mtabupdate, mtabcounts, metrics, nmetrics);
}

void
pmdaDynamicPMNS(const char *prefix, int *clusters, int nclusters,
	pmdaUpdatePMNS pmnsupdate, pmdaUpdateText textupdate,
	pmdaUpdateMetric mtabupdate, pmdaCountMetrics mtabcounts,
	pmdaMetric *metrics, int nmetrics)
{
    dynamic_namespace(&dynamic_table, &dynamic_count,
		prefix, clusters, nclusters, pmnsupdate, textupdate,
		mtabupdate, mtabcounts, metrics, nmetrics);
}

/*
 * Verify if a given pmid should be handled by this dynamic instance.
 * Assumes pmnsupdate has been called so dynamic[index].pmns can't be NULL
 */
static int
dynamic_check_PMID(struct dynamic *table, int count, pmID pmid, int index)
{
    int numfound = 0;
    char **nameset = NULL;

    assert(index < count);
    pmdaNameSpace *tree = table[index].pmns;
    numfound = pmdaTreeName(tree, pmid, &nameset);

    /* Don't need the names, just seeing if its there */
    if (nameset)
	free(nameset);

    if (numfound > 0)
	return numfound;
    return 0;
}

/*
 * Verify if a given name should be handled by this dynamic instance.
 * Assumes pmnsupdate has been called so dynamic[index].pmns can't be NULL
 */
int
dynamic_check_name(struct dynamic *table, int count, const char *name, int index)
{
    pmdaNameSpace *tree = table[index].pmns;

    assert(index < count);

    /* Use this because we want non-leaf nodes also,
     * since we are called for child checks.
     */
    if (pmdaNodeLookup(tree->root->first, name) != NULL)
	return 1;
    return 0;
}

pmdaNameSpace *
pmdaDynamicLookupName(pmdaExt *pmda, const char *name)
{
    int i, sts = 0, count;
    struct dynamic *table;

    dynamic_lookup(pmda, &table, &count);

    for (i = 0; i < count; i++)
	sts |= table[i].pmnsupdate(pmda, &table[i].pmns);
    if (sts)
	pmdaDynamicMetricTable(pmda);
    for (i = 0; i < count; i++)
	if (dynamic_check_name(table, count, name, i))
	    return table[i].pmns;
    return NULL;
}

pmdaNameSpace *
pmdaDynamicLookupPMID(pmdaExt *pmda, pmID pmid)
{
    int i, sts = 0, count;
    struct dynamic *table;

    dynamic_lookup(pmda, &table, &count);

    for (i = 0; i < count; i++)
	sts |= table[i].pmnsupdate(pmda, &table[i].pmns);
    if (sts)
	pmdaDynamicMetricTable(pmda);
    for (i = 0; i < count; i++)
	if (dynamic_check_PMID(table, count, pmid, i))
	    return table[i].pmns;
    return NULL;
}

int
pmdaDynamicLookupText(pmID pmid, int type, char **buf, pmdaExt *pmda)
{
    int i, sts = 0, count;
    struct dynamic *table;

    dynamic_lookup(pmda, &table, &count);

    for (i = 0; i < count; i++)
	sts |= table[i].pmnsupdate(pmda, &table[i].pmns);
    if (sts)
	pmdaDynamicMetricTable(pmda);
    for (i = 0; i < count; i++)
	if (dynamic_check_PMID(table, count, pmid, i))
	    return table[i].textupdate(pmda, pmid, type, buf);
    return -ENOENT;
}

/*
 * Call the update function for each new metric we're adding.
 * We pass in the original metric, and the new (uninit'd) slot
 * which needs to be filled in.  All a bit obscure, really.
 */
static pmdaMetric *
dynamic_metric_table(struct dynamic *table, int count,
		int index, pmdaMetric *offset, pmdaExt *pmda)
{
    struct dynamic *dp = &table[index];
    int m, tree_count = dp->extratrees;

    assert(index < count);

    /* Ensure that the pmns is up to date, need this in case a PMDA calls
     * pmdaDynamicMetricTable directly very early on in its life.
     */
    dp->pmnsupdate(pmda, &dp->pmns);

    for (m = 0; m < dp->nmetrics; m++) {
	pmdaMetric *mp = &dp->metrics[m];
	int gid;

	if (dynamic_check_PMID(table, count, mp->m_desc.pmid, index))
	    for (gid = 0; gid < tree_count; gid++)
		dp->mtabupdate(mp, offset++, gid + 1);
    }
    return offset;
}

/*
 * Iterate through the dynamic table working out how many additional metric
 * table entries are needed.  Then allocate a new metric table, if needed,
 * and run through the dynamic table once again to fill in the additional
 * entries.  Finally, we update the metric table pointer within the pmdaExt
 * for libpcp_pmda callback routines subsequent use.
 */
void
pmdaDynamicMetricTable(pmdaExt *pmda)
{
    int i, count, trees, total, resize = 0;
    pmdaMetric *mtab, *fixed, *offset;
    struct dynamic *table;

    dynamic_lookup(pmda, &table, &count);

    for (i = 0; i < count; i++)
	table[i].mtabcount = table[i].extratrees = 0;

    for (i = 0; i < count; i++) {
	table[i].mtabcounts(&total, &trees);
	table[i].mtabcount += total;
	table[i].extratrees += trees;
	resize += (total * trees);
    }

    fixed = table[0].metrics;		/* fixed metrics */
    total = table[0].nmetrics;		/* and the count */

    if (resize == 0) {
	/* Fits into the default metric table - reset it to original values */
fallback:
	if (pmda->e_metrics != fixed)
	    free(pmda->e_metrics);
	pmdaRehash(pmda, fixed, total);
    } else {
	resize += total;
	if ((mtab = calloc(resize, sizeof(pmdaMetric))) == NULL)
	    goto fallback;
	memcpy(mtab, fixed, total * sizeof(pmdaMetric));
	offset = mtab + total;
	for (i = 0; i < count; i++)
	    offset = dynamic_metric_table(table, count, i, offset, pmda);
	if (pmda->e_metrics != fixed)
	    free(pmda->e_metrics);
	pmdaRehash(pmda, mtab, resize);
    }
}
