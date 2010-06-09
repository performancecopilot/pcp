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
    int		nclusters;
    int		clusters[NUM_CLUSTERS];
    pmnsUpdate	update;
    __pmnsTree 	*pmns;
} *dynamic;

static int dynamic_count;

void
linux_dynamic_pmns(const char *prefix, int *clusters, int nclusters,
		    pmnsUpdate update)
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
    dynamic[dynamic_count].update = update;
    dynamic[dynamic_count].pmns = NULL;
    dynamic_count++;
}

__pmnsTree *
linux_dynamic_lookup_name(pmdaExt *pmda, const char *name)
{
    int i;

    for (i = 0; i < dynamic_count; i++) {
	if (strncmp(name, dynamic[i].prefix, dynamic[i].prefixlen) == 0) {
	    dynamic[i].update(pmda, &dynamic[i].pmns);
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
		dynamic[i].update(pmda, &dynamic[i].pmns);
		return dynamic[i].pmns;
	    }
	}
    }
    return NULL;
}
