/*
 * Dynamic namespace metrics, PMDA helper routines.
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 2010 Aconex.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

static struct dynamic {
    const char		*prefix;
    int			prefixlen;
    int			mtabcount;	/* internal use only */
    int			extratrees;	/* internal use only */
    int			nclusters;
    int			*clusters;
    pmdaUpdatePMNS	pmnsupdate;
    pmdaUpdateText	textupdate;
    pmdaUpdateMetric	mtabupdate;
    pmdaCountMetrics	mtabcounts;
    pmdaNameSpace	*pmns;
    pmdaMetric		*metrics;	/* original fixed table */
    int			nmetrics;	/* fixed metrics number */
} *dynamic;

static int dynamic_count;

void
pmdaDynamicPMNS(const char *prefix,
	    int *clusters, int nclusters,
	    pmdaUpdatePMNS pmnsupdate, pmdaUpdateText textupdate,
	    pmdaUpdateMetric mtabupdate, pmdaCountMetrics mtabcounts,
	    pmdaMetric *metrics, int nmetrics)
{
    int size = (dynamic_count+1) * sizeof(struct dynamic);
    int *ctab;
    size_t ctabsz;

    if ((dynamic = (struct dynamic *)realloc(dynamic, size)) == NULL) {
	__pmNotifyErr(LOG_ERR, "out-of-memory registering dynamic metrics");
	return;
    }
    ctabsz = sizeof(int) * nclusters;
    if ((ctab = (int *)malloc(ctabsz)) == NULL) {
	__pmNotifyErr(LOG_ERR, "out-of-memory registering dynamic clusters");
	free(dynamic);
	return;
    }
    dynamic[dynamic_count].prefix = prefix;
    dynamic[dynamic_count].prefixlen = strlen(prefix);
    dynamic[dynamic_count].nclusters = nclusters;
    dynamic[dynamic_count].clusters = ctab;
    memcpy(dynamic[dynamic_count].clusters, clusters, ctabsz);
    dynamic[dynamic_count].pmnsupdate = pmnsupdate;
    dynamic[dynamic_count].textupdate = textupdate;
    dynamic[dynamic_count].mtabupdate = mtabupdate;
    dynamic[dynamic_count].mtabcounts = mtabcounts;
    dynamic[dynamic_count].pmns = NULL;
    dynamic[dynamic_count].metrics = metrics;
    dynamic[dynamic_count].nmetrics = nmetrics;
    dynamic_count++;
}

pmdaNameSpace *
pmdaDynamicLookupName(pmdaExt *pmda, const char *name)
{
    int i;

    for (i = 0; i < dynamic_count; i++) {
	if (strncmp(name, dynamic[i].prefix, dynamic[i].prefixlen) == 0) {
	    if (dynamic[i].pmnsupdate(pmda, &dynamic[i].pmns))
		pmdaDynamicMetricTable(pmda);
	    return dynamic[i].pmns;
	}
    }
    return NULL;
}

pmdaNameSpace *
pmdaDynamicLookupPMID(pmdaExt *pmda, pmID pmid)
{
    int i, j;
    int cluster = pmid_cluster(pmid);

    for (i = 0; i < dynamic_count; i++) {
	for (j = 0; j < dynamic[i].nclusters; j++) {
	    if (cluster == dynamic[i].clusters[j]) {
		if (dynamic[i].pmnsupdate(pmda, &dynamic[i].pmns))
		    pmdaDynamicMetricTable(pmda);
		return dynamic[i].pmns;
	    }
	}
    }
    return NULL;
}

int
pmdaDynamicLookupText(pmID pmid, int type, char **buf, pmdaExt *pmda)
{
    int i, j;
    int cluster = pmid_cluster(pmid);

    for (i = 0; i < dynamic_count; i++) {
	for (j = 0; j < dynamic[i].nclusters; j++)
	    if (cluster == dynamic[i].clusters[j])
		return dynamic[i].textupdate(pmda, pmid, type, buf);
    }
    return -ENOENT;
}

/*
 * Call the update function for each new metric we're adding.
 * We pass in the original metric, and the new (uninit'd) slot
 * which needs to be filled in.  All a bit obscure, really.
 */
static pmdaMetric *
dynamic_metric_table(struct dynamic *dynamic, pmdaMetric *offset)
{
    int m, tree_count = dynamic->extratrees;

    for (m = 0; m < dynamic->nmetrics; m++) {
	int c, id, cluster = pmid_cluster(dynamic->metrics[m].m_desc.pmid);
	for (c = 0; c < dynamic->nclusters; c++)
	    if (dynamic->clusters[c] == cluster)
		break;
	if (c < dynamic->nclusters)
	    for (id = 0; id < tree_count; id++)
		dynamic->mtabupdate(&dynamic->metrics[m], offset++, id+1);
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
    int i, trees, total, resize = 0;
    pmdaMetric *mtab, *fixed, *offset;

    for (i = 0; i < dynamic_count; i++)
	dynamic[i].mtabcount = dynamic[i].extratrees = 0;

    for (i = 0; i < dynamic_count; i++) {
	dynamic[i].mtabcounts(&total, &trees);
	dynamic[i].mtabcount += total;
	dynamic[i].extratrees += trees;
	resize += (total * trees);
    }

    fixed = dynamic[0].metrics;		/* fixed metrics */
    total = dynamic[0].nmetrics;	/* and the count */

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
	for (i = 0; i < dynamic_count; i++)
	    offset = dynamic_metric_table(&dynamic[i], offset);
	if (pmda->e_metrics != fixed)
	    free(pmda->e_metrics);
	pmdaRehash(pmda, mtab, resize);
    }
}
