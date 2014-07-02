/*
 * Copyright (c) 2013-2014 Red Hat.
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
#ifndef _CGROUP_H
#define _CGROUP_H

/*
 * Note: cgroup metrics have an "extra" component - the cluster part
 * of the PMID (12 bits) is split into two (6 bits each): the bottom
 * part contains the regular metric (cluster) ID while the top holds
 * the cgroup ID (index - e.g. this is the 3rd cgroup we've seen for
 * a particular subsystem).
 */

#define CGROUP_SPLIT	6
#define CGROUP_MASK	((1 << CGROUP_SPLIT) - 1)

static inline pmID
cgroup_pmid_build(unsigned int domain, unsigned int cluster,
		  unsigned int gid, unsigned int metric)
{
    return pmid_build(domain, (gid << CGROUP_SPLIT) | cluster, metric);
}

static inline unsigned int
cgroup_pmid_group(pmID id)
{
    return pmid_cluster(id) >> CGROUP_SPLIT;
}

static inline unsigned int
proc_pmid_cluster(pmID id)
{
    return pmid_cluster(id) & CGROUP_MASK;
}

static inline unsigned int
cgroup_pmid_metric(pmID id)
{
    return pmid_item(id);
}

/*
 * General cgroup interfaces
 */
extern void cgroup_init(pmdaMetric *, int);
extern char *cgroup_find_subsys(pmInDom, void *);
extern int cgroup_group_fetch(pmID, unsigned int, pmAtomValue *);

/*
 * Metric name and value refresh interfaces
 */
extern int refresh_cgroups(pmdaExt *, __pmnsTree **);

/*
 * Indom-specific interfaces
 */
extern void refresh_cgroup_cpus(pmInDom);
extern void refresh_cgroup_devices(pmInDom);
extern void refresh_cgroup_filesys(pmInDom);
extern void refresh_cgroup_subsys(pmInDom);

#endif /* _CGROUP_H */
