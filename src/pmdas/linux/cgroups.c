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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "cgroups.h"
#include "filesys.h"
#include "dynamic.h"
#include "clusters.h"
#include <strings.h>
#include <ctype.h>

typedef void (*cgroup_names_t)(__pmnsTree *, int, int, const char *);
typedef void (*cgroup_fetch_t)(int, int);

typedef struct {
    int		item;
    char	*suffix;
} cgroup_metrics_t;

cgroup_metrics_t cpusched_metrics[] = {
	{ 0, "shares" }
};
static void cgroup_fetch_cpusched(int id, int item) { }

cgroup_metrics_t cpuacct_metrics[] = {
	{ 0, "stat" },
	{ 1, "usage_percpu" },
	{ 2, "usage" }
};
static void cgroup_fetch_cpuacct(int id, int item) { }

cgroup_metrics_t cpuset_metrics[] = {
	{ 0, "cpus" },
	{ 1, "mems" }
};
static void cgroup_fetch_cpuset(int id, int item) { }

cgroup_metrics_t memory_metrics[] = {
	{ 0, "cache" },
	{ 1, "rss" },
	{ 2, "pgin" },
	{ 3, "pgout" },
	{ 4, "active_anon" },
	{ 5, "inactive_anon" },
	{ 6, "active_file" },
	{ 7, "inactive_file" },
	{ 8, "unevictable" },
};
static void cgroup_fetch_memory(int id, int item) { }

cgroup_metrics_t netclass_metrics[] = {
	{ 0, "classid" },
};
static void cgroup_fetch_netclass(int id, int item) { }

static void
translate(char *dest, const char *src, size_t size)
{
    char *p;

    if (*src != '\0')	/* non-root */
	*dest = '.';
    strncpy(dest, src, size);
    for (p = dest; *p; p++) {
	if (*p == '/')
	    *p = '.';
    }
}

static int
namespace(__pmnsTree *pmns, const char *cgrp, const char *grp, int proc_cluster,
	  int domain, int cluster, int id, cgroup_metrics_t *mp, int count)
{
    int i;
    pmID pmid;
    char group[128];
    char name[MAXPATHLEN];

    translate(&group[0], grp, sizeof(group));

    for (i = 0; i < count; i++) {
	pmid = cgroup_pmid_build(domain, cluster, 0, ++id);
	snprintf(name, sizeof(name), "cgroup.groups.%s%s.%s",
					cgrp, group, mp[i].suffix);
	__pmAddPMNSNode(pmns, pmid, name);
    }

    /* Any proc.* metric subset could be added here.  Just PIDs for now */
    i = -1;	/* proc metric item */
    pmid = cgroup_pmid_build(domain, proc_cluster, 0, ++i);
    snprintf(name, sizeof(name), "cgroup.groups.%s%s.tasks.pid", cgrp, group);
    __pmAddPMNSNode(pmns, pmid, name);
    
    return id;
}

int
refresh_cgroup_subsys(pmInDom indom)
{
    char buf[4096];
    char name[MAXPATHLEN];
    int hierarchy, numcgroups, enabled, data, sts;
    FILE *fp;

    if ((fp = fopen("/proc/cgroups", "r")) == NULL)
	return 1;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	/* skip lines starting with hash (header) */
	if (buf[0] == '#')
	    continue;
	sscanf(buf, "%s %u %u %u", &name[0], &hierarchy, &numcgroups, &enabled);

	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&data);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	if (sts != PMDA_CACHE_INACTIVE) {
	    char *n = strdup(name);
	    if (n == NULL)
		continue;
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, n, (void *)hierarchy);
	} else if (data != hierarchy) {
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)hierarchy);
	}
    }
    fclose(fp);
    return 0;
}

/*
 * Parse a (comma-separated) mount option string to find one of the known
 * cgroup subsystems, and return a pointer to it or "?" if none found.
 */
char *
cgroup_find_subsys(pmInDom indom, const char *options)
{
    static char dunno[] = "?";
    static char opts[128];
    char buffer[128];
    char *s, *out = NULL;

    strncpy(buffer, options, sizeof(buffer));

    s = strtok(buffer, ",");
    while (s) {
	if (pmdaCacheLookupName(indom, s, NULL, NULL) == PMDA_CACHE_ACTIVE) {
	    if (out) {	/* append option */
		strcat(out, ",");
		strcat(out, s);
		out += strlen(s) + 1;		/* +1 => cater for comma */
	    } else {	/* first option */
		strcat(opts, s);
		out = opts + strlen(s);
	    }
	}
	s = strtok(NULL, ",");
    }
    if (out)
	return opts;
    return dunno;
}

static struct {
    const char		*name;
    int			cluster;
    int			processes;
    int			count;
    int			padding;
    cgroup_metrics_t	*metrics;
    cgroup_fetch_t	fetch;
} controllers[] = {
    {	.name = "cpu",
	.cluster = CLUSTER_CPUSCHED_GROUPS,
	.processes = CLUSTER_CPUSCHED_PROCS,
	.metrics = cpusched_metrics,
	.count = sizeof(cpusched_metrics) / sizeof(cpusched_metrics[0]),
	.fetch = cgroup_fetch_cpusched,
    },
    {	.name = "cpuset",
	.cluster = CLUSTER_CPUSET_GROUPS,
	.processes = CLUSTER_CPUSET_PROCS,
	.metrics = cpuset_metrics,
	.count = sizeof(cpuset_metrics) / sizeof(cpuset_metrics[0]),
	.fetch = cgroup_fetch_cpuset,
    },
    {	.name = "cpuacct",
	.cluster = CLUSTER_CPUACCT_GROUPS,
	.processes = CLUSTER_CPUACCT_PROCS,
	.metrics = cpuacct_metrics,
	.count = sizeof(cpuacct_metrics) / sizeof(cpuacct_metrics[0]),
	.fetch = cgroup_fetch_cpuacct,
    },
    {	.name = "memory",
	.cluster = CLUSTER_MEMORY_GROUPS,
	.processes = CLUSTER_MEMORY_PROCS,
	.metrics = memory_metrics,
	.count = sizeof(memory_metrics) / sizeof(memory_metrics[0]),
	.fetch = cgroup_fetch_memory,
    },
    {	.name = "net_cls",
	.cluster = CLUSTER_NET_CLS_GROUPS,
	.processes = CLUSTER_NET_CLS_PROCS,
	.metrics = netclass_metrics,
	.count = sizeof(netclass_metrics) / sizeof(netclass_metrics[0]),
	.fetch = cgroup_fetch_netclass,
    },
};

/* Ensure cgroup name can be used as a PCP namespace entry, ignore it if not */
static int
valid_pmns_name(char *name)
{
    if (!isalpha(name[0]))
	return 0;
    for (; *name != '\0'; name++)
	if (!isalnum(*name) && *name != '_')
	    return 0;
    return 1;
}

static int
cgroup_namespace(__pmnsTree *pmns, const char *options,
		const char *cgrouppath, const char *cgroupname,
		int domain, int id)
{
    int i;

    /* use options to tell which cgroup controller(s) are active here */
    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	if (scan_filesys_options(options, controllers[i].name) == NULL)
	    continue;
	id = namespace(pmns, controllers[i].name, cgroupname,
			     controllers[i].processes, domain,
			     controllers[i].cluster, id,
			     controllers[i].metrics, controllers[i].count);
    }
    return id;
}

static int
cgroup_scan(const char *mnt, const char *path, const char *options,
	    int domain, int id, __pmnsTree *pmns, int root)
{
    int length;
    DIR *dirp;
    struct stat sbuf;
    struct dirent *dp;
    char *cgroupname;
    char cgrouppath[MAXPATHLEN];

    if (root)
	strncpy(cgrouppath, mnt, sizeof(cgrouppath));
    else
	snprintf(cgrouppath, sizeof(cgrouppath), "%s/%s", mnt, path);

    if ((dirp = opendir(cgrouppath)) == NULL)
	return -errno;

    length = strlen(cgrouppath);
    cgroupname = &cgrouppath[length];

    id = cgroup_namespace(pmns, options, cgrouppath, cgroupname, domain, id);

    /*
     * readdir - descend into directories to find all cgroups, then
     * populate namespace with <controller>[.<groupname>].<metrics>
     */
    while ((dp = readdir(dirp)) != NULL) {
	if (!valid_pmns_name(dp->d_name))
	    continue;
	snprintf(cgrouppath, sizeof(cgrouppath), "%s/%s/%s",
			mnt, path, dp->d_name);
	cgroupname = &cgrouppath[length];
	if (stat(cgrouppath, &sbuf) < 0)
	    continue;
	if (!(S_ISDIR(sbuf.st_mode)))
	    continue;

        id = cgroup_namespace(pmns, options, cgrouppath, cgroupname, domain, id);

	/* also scan for any child cgroups */
        id = cgroup_scan(mnt, cgrouppath, options, domain, id, pmns, 0);
    }
    closedir(dirp);

    return id;
}

void
refresh_cgroup_groups(pmdaExt *pmda, pmInDom mounts, __pmnsTree **pmns)
{
    filesys_t *fs;
    int sts, count = 0, domain = pmda->e_domain;
    __pmnsTree *tree = pmns ? *pmns : NULL;

    if (tree)
	__pmFreePMNS(tree);

    if ((sts = __pmNewPMNS(&tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	if (pmns)
	    *pmns = NULL;
	return;
    }

    pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	if (!pmdaCacheLookup(mounts, sts, NULL, (void **)&fs))
	    continue;
	/* walk this cgroup mount finding groups (subdirs) */
	count = cgroup_scan(fs->path, "", fs->options, domain, count, tree, 1);
    }

    if (pmns)
	*pmns = tree;
    else
	__pmFreePMNS(tree);
}

void
cgroup_init(void)
{
    int set[] = { CLUSTER_CPUSET_GROUPS, CLUSTER_CPUSET_PROCS,
		  CLUSTER_CPUACCT_GROUPS, CLUSTER_CPUACCT_PROCS,
		  CLUSTER_CPUSCHED_GROUPS, CLUSTER_CPUSCHED_PROCS,
		  CLUSTER_MEMORY_GROUPS, CLUSTER_MEMORY_PROCS,
		  CLUSTER_NET_CLS_GROUPS, CLUSTER_NET_CLS_PROCS,
		};

    linux_dynamic_pmns("cgroup.groups", set, sizeof(set), refresh_cgroups);
}
