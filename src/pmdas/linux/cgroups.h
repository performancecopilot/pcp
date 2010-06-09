/*
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

static inline pmID
cgroup_pmid_build(unsigned int domain, unsigned int cluster,
		  unsigned int item, unsigned int id)
{
    return pmid_build(domain, cluster, (item << 5) | id);
}

static inline unsigned int
cgroup_pmid_item(pmID id)
{
    return pmid_item(id) >> 5;
}

static inline unsigned int
cgroup_pmid_metric(pmID id)
{
    return pmid_item(id) & ((1 << 5) - 1);
}

extern void cgroup_init();
extern void refresh_cgroups(pmdaExt *pmda, __pmnsTree **);
extern int refresh_cgroup_subsys(pmInDom subsys);
extern char *cgroup_find_subsys(pmInDom subsys, const char *);
extern void refresh_cgroup_groups(pmdaExt *pmda, pmInDom mounts, __pmnsTree **);
