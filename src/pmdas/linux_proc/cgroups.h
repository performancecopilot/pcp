/*
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

/*
 * Note: cgroup metrics have an "extra" component - the final item part
 * of the PMID (10 bits) is split into two (5 bits each) - top contains
 * the metric ID (item), bottom holds the cgroup ID (index - e.g. this
 * is the 3rd cgroup we've seen).
 */

#define CGROUP_ROOT	"cgroup" /* cgroup root pmns node */

static inline pmID
cgroup_pmid_build(unsigned int domain, unsigned int cluster,
		  unsigned int gid, unsigned int metric)
{
    return pmid_build(domain, cluster, (gid << 5) | metric);
}

static inline unsigned int
cgroup_pmid_group(pmID id)
{
    return pmid_item(id) >> 5;
}

static inline unsigned int
cgroup_pmid_metric(pmID id)
{
    return pmid_item(id) & ((1 << 5) - 1);
}

extern void cgroup_init(pmdaMetric *, int);
extern char *cgroup_find_subsys(pmInDom, const char *);

extern int refresh_cgroups(pmdaExt *, __pmnsTree **);
extern int refresh_cgroup_subsys(pmInDom);
extern int refresh_cgroup_groups(pmdaExt *, pmInDom, __pmnsTree **);

extern int cgroup_group_fetch(int, int, unsigned int, pmAtomValue *);
