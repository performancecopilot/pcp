/*
 * Dynamic namespace metrics for the Linux kernel PMDA
 *
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
#include "indom.h"
#include "dynamic.h"
#include "clusters.h"

static struct dynamic {
    const char	*prefix;
    int		prefixlen;
    int		mtabcount;	/* internal use only */
    int		extratrees;	/* internal use only */
    int		nclusters;
    int		clusters[NUM_CLUSTERS];
    pmnsUpdate	pmnsupdate;
    mtabUpdate	mtabupdate;
    mtabCounts	mtabcounts;
    __pmnsTree 	*pmns;
} *dynamic;

static int dynamic_count;

void
linux_dynamic_pmns(const char *prefix, int *clusters, int nclusters,
	    pmnsUpdate pmnsupdate, mtabUpdate mtabupdate, mtabCounts mtabcounts)
{
    int size = (dynamic_count+1) * sizeof(struct dynamic);

    if ((dynamic = (struct dynamic *)realloc(dynamic, size)) == NULL) {
	__pmNotifyErr(LOG_ERR, "out-of-memory registering dynamic metrics");
	return;
    }
    dynamic[dynamic_count].prefix = prefix;
    dynamic[dynamic_count].prefixlen = strlen(prefix);
    dynamic[dynamic_count].nclusters = nclusters;
    memcpy(dynamic[dynamic_count].clusters, clusters, nclusters * sizeof(int));
    dynamic[dynamic_count].pmnsupdate = pmnsupdate;
    dynamic[dynamic_count].mtabupdate = mtabupdate;
    dynamic[dynamic_count].mtabcounts = mtabcounts;
    dynamic[dynamic_count].pmns = NULL;
    dynamic_count++;
}

__pmnsTree *
linux_dynamic_lookup_name(pmdaExt *pmda, const char *name)
{
    int i;

    for (i = 0; i < dynamic_count; i++) {
	if (strncmp(name, dynamic[i].prefix, dynamic[i].prefixlen) == 0) {
	    dynamic[i].pmnsupdate(pmda, &dynamic[i].pmns);
	    return dynamic[i].pmns;
	}
    }
    return NULL;
}

__pmnsTree *
linux_dynamic_lookup_pmid(pmdaExt *pmda, pmID pmid)
{
    int i, j;
    int cluster = pmid_cluster(pmid);

    for (i = 0; i < dynamic_count; i++) {
	for (j = 0; j < dynamic[i].nclusters; j++) {
	    if (cluster == dynamic[i].clusters[j]) {
		dynamic[i].pmnsupdate(pmda, &dynamic[i].pmns);
		return dynamic[i].pmns;
	    }
	}
    }
    return NULL;
}

/*
 * Call the update function for each new metric we're adding.
 * We pass in the original metric, and the new (uninit'd) slot which
 * needs to be filled in.  All a bit obscure, really.
 */
static void
linux_dynamic_mtab(struct dynamic *dynamic, pmdaMetric *offset)
{
    int m, c, id;
    int metric_count = linux_metrictable_size();
    int tree_count = dynamic->extratrees;

    for (m = 0; m < metric_count; m++) {
	int cluster = pmid_cluster(linux_metrictab[m].m_desc.pmid);
	for (c = 0; c < dynamic->nclusters; c++) {
	    if (dynamic->clusters[c] != cluster)
		continue;
	    for (id = 0; id < tree_count; id++)
		dynamic->mtabupdate(&linux_metrictab[m], offset++, id+1);
	    break;
	}
    }
}

/*
 * Iterate through the dynamic table working out how many additional metric
 * table entries are needed.  Then allocate a new metric table, if needed,
 * and run through the dynamic table once again to fill in the additional
 * entries.  Finally, we update the metric table pointed to be the pmdaExt
 * for libpcp_pmda routines subsequent use.
 */
void
linux_dynamic_metrictable(pmdaExt *pmda)
{
    int i, trees, total, resize = 0;
    pmdaMetric *mtab, *offset;

    for (i = 0; i < dynamic_count; i++)
	dynamic[i].mtabcount = dynamic[i].extratrees = 0;

    for (i = 0; i < dynamic_count; i++) {
	dynamic[i].mtabcounts(&total, &trees);
	dynamic[i].mtabcount += total;
	dynamic[i].extratrees += trees;
	resize += (total * trees);
    }

    if (resize == 0) {
	/* Fits into the default metric table - reset it to original values */
fallback:
	if (pmda->e_metrics != linux_metrictab)
	    free(pmda->e_metrics);
	pmda->e_metrics = linux_metrictab;
	pmda->e_nmetrics = linux_metrictable_size();
    } else {
	resize += linux_metrictable_size();
	if ((mtab = calloc(sizeof(pmdaMetric), resize)) == NULL)
	    goto fallback;
	memcpy(mtab, linux_metrictab, linux_metrictable_size() * sizeof(pmdaMetric));
	offset = mtab + linux_metrictable_size();
	for (i = 0; i < dynamic_count; i++) {
	    linux_dynamic_mtab(&dynamic[i], offset);
	    offset += dynamic[i].mtabcount;
	}
	if (pmda->e_metrics != linux_metrictab)
	    free(pmda->e_metrics);
	pmda->e_metrics = mtab;
	pmda->e_nmetrics = resize;
    }
}
