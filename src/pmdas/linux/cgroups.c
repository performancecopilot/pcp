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
#include "proc_pid.h"
#include <strings.h>
#include <ctype.h>

/* Add namespace entries and prepare values for one cgroupfs directory entry */
struct cgroup_subsys;
typedef int (*cgroup_prepare_t)(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);
static int prepare_ull(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);
static int prepare_string(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);
static int prepare_named_ull(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);

/*
 * Critical data structures for cgroup subsystem in pmdalinux...
 * Initial comment for each struct talks about lifecycle of that
 * data, in terms of what pmdalinux must do with it (esp. memory
 * allocation related).
 */

typedef struct { /* contents depends on individual kernel cgroups */
    int			item;		/* PMID == domain:cluster:[id:item] */
    int			dynamic;	/* do we need an extra free (string) */
    cgroup_prepare_t	prepare;	/* setup metric name(s) and value(s) */
    char		*suffix;	/* cpus/mems/rss/... */
} cgroup_metrics_t;

typedef struct { /* some metrics are multi-valued, but most have only one */
    int			item;		/* PMID == domain:cluster:[id:item] */
    int			atom_count;	
    pmAtomValue		*atoms;
} cgroup_values_t;

typedef struct { /* contains data for each group users have created, if any */
    int			id;		/* PMID == domain:cluster:[id:item] */
    int			refreshed;	/* boolean: are values all uptodate */
    proc_pid_list_t	process_list;
    cgroup_values_t	*metric_values;
} cgroup_group_t;

typedef struct cgroup_subsys { /* contents covers the known kernel cgroups */
    const char		*name;		/* cpuset/memory/... */
    int			cluster;	/* PMID == domain:cluster:[id:item] */
    int			process_cluster;/* cluster ID for process metrics */
    int			group_count;	/* number of groups (dynamic) */
    int			metric_count;	/* number of metrics (fixed)  */
    cgroup_group_t	*groups;	/* array of groups (dynamic)  */
    cgroup_metrics_t	*metrics;	/* array of metrics (fixed)   */
} cgroup_subsys_t;

static cgroup_metrics_t cpusched_metrics[] = {
    { .item = 0, .suffix = "shares", .prepare = prepare_ull },
};

static cgroup_metrics_t cpuacct_metrics[] = {
    { .item = 0, .suffix = "stat.user", .prepare = prepare_named_ull },
    { .item = 1, .suffix = "stat.system", .prepare = prepare_named_ull },
    { .item = 2, .suffix = "usage", .prepare = prepare_ull },
    { .item = 3, .suffix = "usage_percpu", .prepare = prepare_ull },
};

static cgroup_metrics_t cpuset_metrics[] = {
    { .item = 0, .suffix = "cpus", .prepare = prepare_string, .dynamic = 1 },
    { .item = 1, .suffix = "mems", .prepare = prepare_string, .dynamic = 1 },
};

static cgroup_metrics_t memory_metrics[] = {
    { .item = 0, .suffix = "stat.cache", .prepare = prepare_named_ull },
    { .item = 1, .suffix = "stat.rss", .prepare = prepare_named_ull },
    { .item = 2, .suffix = "stat.pgin", .prepare = prepare_named_ull },
    { .item = 3, .suffix = "stat.pgout", .prepare = prepare_named_ull },
    { .item = 4, .suffix = "stat.swap", .prepare = prepare_named_ull },
    { .item = 5, .suffix = "stat.active_anon", .prepare = prepare_named_ull },
    { .item = 6, .suffix = "stat.inactive_anon", .prepare = prepare_named_ull },
    { .item = 7, .suffix = "stat.active_file", .prepare = prepare_named_ull },
    { .item = 8, .suffix = "stat.inactive_file", .prepare = prepare_named_ull },
    { .item = 9, .suffix = "stat.unevictable", .prepare = prepare_named_ull },
};

static cgroup_metrics_t netclass_metrics[] = {
    { .item = 0, .suffix = "classid", .prepare = prepare_ull },
};

static cgroup_subsys_t controllers[] = {
    {	.name = "cpu",
	.cluster = CLUSTER_CPUSCHED_GROUPS,
	.process_cluster = CLUSTER_CPUSCHED_PROCS,
	.metrics = cpusched_metrics,
	.metric_count = sizeof(cpusched_metrics) / sizeof(cpusched_metrics[0]),
    },
    {	.name = "cpuset",
	.cluster = CLUSTER_CPUSET_GROUPS,
	.process_cluster = CLUSTER_CPUSET_PROCS,
	.metrics = cpuset_metrics,
	.metric_count = sizeof(cpuset_metrics) / sizeof(cpuset_metrics[0]),
    },
    {	.name = "cpuacct",
	.cluster = CLUSTER_CPUACCT_GROUPS,
	.process_cluster = CLUSTER_CPUACCT_PROCS,
	.metrics = cpuacct_metrics,
	.metric_count = sizeof(cpuacct_metrics) / sizeof(cpuacct_metrics[0]),
    },
    {	.name = "memory",
	.cluster = CLUSTER_MEMORY_GROUPS,
	.process_cluster = CLUSTER_MEMORY_PROCS,
	.metrics = memory_metrics,
	.metric_count = sizeof(memory_metrics) / sizeof(memory_metrics[0]),
    },
    {	.name = "net_cls",
	.cluster = CLUSTER_NET_CLS_GROUPS,
	.process_cluster = CLUSTER_NET_CLS_PROCS,
	.metrics = netclass_metrics,
	.metric_count = sizeof(netclass_metrics) / sizeof(netclass_metrics[0]),
    },
};

static int
read_values(char *buffer, int size, const char *path, const char *subsys,
		const char *metric)
{
    int fd, count;

    snprintf(buffer, size, "%s/%s.%s", path, subsys, metric);
    if ((fd = open(buffer, O_RDONLY)) < 0)
	return -errno;
    count = read(fd, buffer, size);
    close(fd);
    if (count < 0)
	return -errno;
    buffer[count-1] = '\0';
    return 0;
}

static int
process_prepare(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int group, int domain)
{
    FILE *fp;
    pmID pmid;
    char process[64];
    char taskpath[MAXPATHLEN];
    proc_pid_list_t *list = &subsys->groups[group].process_list;

    snprintf(taskpath, sizeof(taskpath), "%s/tasks", path);
    if ((fp = fopen(taskpath, "r")) == NULL)
	return -errno;
    while (fgets(process, sizeof(process), fp) != NULL)
	pidlist_append(list, process);
    fclose(fp);
    qsort(list->pids, list->count, sizeof(int), compare_pid);

    pmid = cgroup_pmid_build(domain, subsys->process_cluster, group, 0);
    snprintf(taskpath, sizeof(taskpath), "cgroup.groups.%s%s.tasks.pid",
			subsys->name, name);
    __pmAddPMNSNode(pmns, pmid, taskpath);
    return 0;
}

static void
update_pmns(__pmnsTree *pmns, cgroup_subsys_t *subsys, const char *name,
		cgroup_metrics_t *metrics, int group, int domain)
{
    char entry[MAXPATHLEN];
    pmID pmid;

    snprintf(entry, sizeof(entry), "cgroup.groups.%s%s.%s",
			subsys->name, name, metrics->suffix);
    pmid = cgroup_pmid_build(domain, subsys->cluster, group, metrics->item);
    __pmAddPMNSNode(pmns, pmid, entry);
}

static int
prepare_ull(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain)
{
    int count = 0;
    unsigned long long value;
    char buffer[MAXPATHLEN];
    char *endp, *p = &buffer[0];
    cgroup_group_t *groups = &subsys->groups[group];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];
    pmAtomValue *atoms = groups->metric_values[metric].atoms;

    if (read_values(p, sizeof(buffer), path, subsys->name, metrics->suffix) < 0)
	return -errno;

    while (p && *p) {
	value = strtoull(p, &endp, 0);
	if ((atoms = realloc(atoms, (count + 1) * sizeof(pmAtomValue))) == NULL)
	    return -errno;
	atoms[count++].ull = value;
	if (endp == '\0' || endp == p)
	    break;
	p = endp;
	while (p && isspace(*p))
	    p++;
    }

    groups->metric_values[metric].item = metric;
    groups->metric_values[metric].atoms = atoms;
    groups->metric_values[metric].atom_count = count;
    update_pmns(pmns, subsys, name, metrics, group, domain);
    return 0;
}

static int
prepare_named_ull(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain)
{
    int i, count;
    unsigned long long value;
    char filename[64], buffer[MAXPATHLEN];
    char *offset, *p = &buffer[0];
    cgroup_group_t *groups = &subsys->groups[group];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];

    if (groups->refreshed)
	return 0;

    /* metric => e.g. stat.user and stat.system - split it up first */
    offset = index(metrics->suffix, '.');
    if (!offset)
	return PM_ERR_CONV;
    count = (offset - metrics->suffix);
    strncpy(filename, metrics->suffix, count);
    filename[count] = '\0';

    if (read_values(p, sizeof(buffer), path, subsys->name, filename) < 0)
	return -errno;

    /* buffer contains <name <value> pairs */
    while (p && *p) {
	char *endp, *field, *offset;

	if ((field = index(p, ' ')) == NULL)
	    return PM_ERR_CONV;
	offset = field + 1;
	*field = '\0';
	field = p;		/* field now points to <name> */
	p = offset;
	value = strtoull(p, &endp, 0);
	p = endp;
	while (p && isspace(*p))
	    p++;

	for (i = 0; i < subsys->metric_count; i++) {
	    pmAtomValue *atoms = groups->metric_values[i].atoms;
	    metrics = &subsys->metrics[i];

	    if (strcmp(field, metrics->suffix + count + 1) != 0)
		continue;
	    if ((atoms = calloc(1, sizeof(pmAtomValue))) == NULL)
		return -errno;
	    atoms[0].ull = value;

	    groups->metric_values[i].item = i;
	    groups->metric_values[i].atoms = atoms;
	    groups->metric_values[i].atom_count = 1;
	    update_pmns(pmns, subsys, name, metrics, group, domain);
	    break;
	}
    }
    groups->refreshed = 1;
    return 0;
}

static int
prepare_string(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain)
{
    char buffer[MAXPATHLEN];
    cgroup_group_t *groups = &subsys->groups[group];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];
    pmAtomValue *atoms = groups->metric_values[metric].atoms;
    char *p = &buffer[0];

    if (read_values(p, sizeof(buffer), path, subsys->name, metrics->suffix) < 0)
	return -errno;

    if ((atoms = malloc(sizeof(pmAtomValue))) == NULL)
	return -errno;
    if ((atoms[0].cp = strdup(buffer)) == NULL)
	return -errno;
    groups->metric_values[metric].item = metric;
    groups->metric_values[metric].atoms = atoms;
    groups->metric_values[metric].atom_count = 1;
    update_pmns(pmns, subsys, name, metrics, group, domain);
    return 0;
}

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
namespace(__pmnsTree *pmns, cgroup_subsys_t *subsys,
		const char *cgrouppath, const char *cgroupname, int domain)
{
    int i, id, sts;
    cgroup_values_t *cvp;
    char group[128];

    translate(&group[0], cgroupname, sizeof(group));

    /* allocate space for this group */
    sts = (subsys->group_count + 1) * sizeof(cgroup_group_t);
    subsys->groups = (cgroup_group_t *)realloc(subsys->groups, sts);
    if (subsys->groups == NULL)
	return -errno;
    /* allocate space for all values up-front */
    sts = subsys->metric_count * sizeof(cgroup_values_t);
    if ((cvp = (cgroup_values_t *)calloc(1, sts)) == NULL)
	return -errno;
    id = subsys->group_count++;
    memset(&subsys->groups[id], 0, sizeof(cgroup_group_t));
    subsys->groups[id].id = id;
    subsys->groups[id].metric_values = cvp;

    for (i = 0; i < subsys->metric_count; i++) {
	cgroup_metrics_t *metrics = &subsys->metrics[i];
	metrics->prepare(pmns, cgrouppath, subsys, group, i, id, domain);
    }
    process_prepare(pmns, cgrouppath, subsys, group, id, domain);
    return 0;
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
	if (sscanf(buf, "%s %u %u %u", &name[0],
			&hierarchy, &numcgroups, &enabled) != 4)
	    continue;
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

static void
cgroup_namespace(__pmnsTree *pmns, const char *options,
		const char *cgrouppath, const char *cgroupname, int domain)
{
    int i;

    /* use options to tell which cgroup controller(s) are active here */
    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	if (scan_filesys_options(options, controllers[i].name) == NULL)
	    continue;
	namespace(pmns, &controllers[i], cgrouppath, cgroupname, domain);
    }
}

static int
cgroup_scan(const char *mnt, const char *path, const char *options,
		int domain, __pmnsTree *pmns, int root)
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

    cgroup_namespace(pmns, options, cgrouppath, cgroupname, domain);

    /*
     * readdir - descend into directories to find all cgroups, then
     * populate namespace with <controller>[.<groupname>].<metrics>
     */
    while ((dp = readdir(dirp)) != NULL) {
	if (!valid_pmns_name(dp->d_name))
	    continue;
	if (path[0] == '\0')
	    snprintf(cgrouppath, sizeof(cgrouppath), "%s/%s",
			mnt, dp->d_name);
	else
	    snprintf(cgrouppath, sizeof(cgrouppath), "%s/%s/%s",
			mnt, path, dp->d_name);
	cgroupname = &cgrouppath[length];
	if (stat(cgrouppath, &sbuf) < 0)
	    continue;
	if (!(S_ISDIR(sbuf.st_mode)))
	    continue;

        cgroup_namespace(pmns, options, cgrouppath, cgroupname, domain);

	/* also scan for any child cgroups */
        cgroup_scan(mnt, cgrouppath, options, domain, pmns, 0);
    }
    closedir(dirp);
    return 0;
}

void
refresh_cgroup_groups(pmdaExt *pmda, pmInDom mounts, __pmnsTree **pmns)
{
    int i, j, k, a, sts, domain = pmda->e_domain;
    filesys_t *fs;
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

    /* reset our view of subsystems and groups */
    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	cgroup_subsys_t *subsys = &controllers[i];
	for (j = 0; j < subsys->group_count; j++) {
	    cgroup_group_t *group = &subsys->groups[j];
	    for (k = 0; k < subsys->metric_count; k++) {
		pmAtomValue *atoms = group->metric_values[k].atoms;
		if (subsys->metrics[k].dynamic)
		    for (a = 0; a < group->metric_values[k].atom_count; a++)
			free(atoms[a].cp);
		free(atoms);
	    }
	    free(group->metric_values);
	    if (group->process_list.size)
		free(group->process_list.pids);
	    memset(group, 0, sizeof(cgroup_group_t));
	}
	controllers[i].group_count = 0;
    }

    pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	if (!pmdaCacheLookup(mounts, sts, NULL, (void **)&fs))
	    continue;
	/* walk this cgroup mount finding groups (subdirs) */
	cgroup_scan(fs->path, "", fs->options, domain, tree, 1);
    }

    if (pmns)
	*pmns = tree;
    else
	__pmFreePMNS(tree);
}

int
cgroup_group_fetch(int cluster, int item, unsigned int inst, pmAtomValue *atom)
{
    int i, j, k, gid;

    gid = cgroup_pmid_group(item);
    item = cgroup_pmid_metric(item);

    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
 	cgroup_subsys_t *subsys = &controllers[i];

	if (subsys->cluster != cluster)
	    continue;
	for (j = 0; j < subsys->group_count; j++) {
	    cgroup_group_t *group = &subsys->groups[j];

	    if (group->id != gid)
		continue;
	    for (k = 0; k < subsys->metric_count; k++) {
		cgroup_values_t *cvp = &group->metric_values[k];

		if (cvp->item != item)
		    continue;
		if (cvp->atom_count <= 0)
		    return PM_ERR_VALUE;
		if (inst == PM_IN_NULL)
		    inst = 0;
		else if (inst >= cvp->atom_count)
		    return PM_ERR_INST;
		*atom = cvp->atoms[inst];
		return 1;
	    }
	}
    }
    return PM_ERR_PMID;
}

int
cgroup_procs_fetch(int cluster, int item, unsigned int inst, pmAtomValue *atom)
{
    int i, j, gid;

    gid = cgroup_pmid_group(item);
    item = cgroup_pmid_metric(item);

    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
 	cgroup_subsys_t *subsys = &controllers[i];
	if (subsys->cluster != cluster)
	    continue;

	for (j = 0; j < subsys->group_count; j++) {
	    cgroup_group_t *group = &subsys->groups[j];

	    if (group->id != gid)
		continue;
	    
	    /* TODO */
	}
    }
    return PM_ERR_PMID;
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
