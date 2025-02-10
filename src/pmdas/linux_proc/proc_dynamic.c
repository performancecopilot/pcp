/*
 * Copyright (c) 2015,2021 Red Hat.
 * Copyright (c) 2014-2015 Martins Innus.  All Rights Reserved.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "libpcp.h"
#include "pmda.h"
#include "clusters.h"
#include "proc_pid.h"
#include "indom.h"

#include <ctype.h>

typedef struct {
    int	    item;
    int	    cluster;
    char    *name;
} dynproc_metric_t;

static pmdaNameSpace *dynamic_proc_tree;

enum {
    DYNPROC_GROUP_PSINFO = 0,
    DYNPROC_GROUP_FD,
    DYNPROC_GROUP_ID,
    DYNPROC_GROUP_MEMORY,
    DYNPROC_GROUP_IO,
    DYNPROC_GROUP_SCHEDSTAT,
    DYNPROC_GROUP_NAMESPACE,
    DYNPROC_GROUP_SMAPS,
    DYNPROC_GROUP_AUTOGROUP,
    DYNPROC_GROUP_FDINFO,

    NUM_DYNPROC_GROUPS
};

enum {
    DYNPROC_PROC = 0,
    DYNPROC_HOTPROC = 1,

    NUM_DYNPROC_TREES
};

/*
 * Map proc cluster ID's to new hotproc variants that don't conflict
 */

static int proc_hotproc_cluster_list[][2] = {
	{ CLUSTER_PID_STAT,	    CLUSTER_HOTPROC_PID_STAT },
	{ CLUSTER_PID_STATM,	    CLUSTER_HOTPROC_PID_STATM },
	{ CLUSTER_PID_CGROUP,	    CLUSTER_HOTPROC_PID_CGROUP },
	{ CLUSTER_PID_LABEL,	    CLUSTER_HOTPROC_PID_LABEL },
	{ CLUSTER_PID_STATUS,	    CLUSTER_HOTPROC_PID_STATUS },
	{ CLUSTER_PID_SCHEDSTAT,    CLUSTER_HOTPROC_PID_SCHEDSTAT },
	{ CLUSTER_PID_IO,	    CLUSTER_HOTPROC_PID_IO },
	{ CLUSTER_PID_FD,	    CLUSTER_HOTPROC_PID_FD },
	{ CLUSTER_PID_OOM_SCORE,    CLUSTER_HOTPROC_PID_OOM_SCORE },
	{ CLUSTER_PID_SMAPS,	    CLUSTER_HOTPROC_PID_SMAPS },
	{ CLUSTER_PID_EXE,	    CLUSTER_HOTPROC_PID_EXE },
	{ CLUSTER_PID_CWD,	    CLUSTER_HOTPROC_PID_CWD },
	{ CLUSTER_PID_AUTOGROUP,    CLUSTER_HOTPROC_PID_AUTOGROUP },
	{ CLUSTER_PID_FDINFO,	    CLUSTER_HOTPROC_PID_FDINFO },
};


typedef struct {
    char		*name;
    dynproc_metric_t	*metrics;
    int			nmetrics;
} dynproc_group_t;


static const char *dynproc_members[] = {
	[DYNPROC_PROC]	    = "proc",
	[DYNPROC_HOTPROC]   = "hotproc",
};

/*
 * Remember to add help text to help_text.h for any new dynamic metrics
 */

static dynproc_metric_t psinfo_metrics[] = {
	{ .name = "pid",	    .cluster = CLUSTER_PID_STAT,	.item=0 },
	{ .name = "cmd",	    .cluster = CLUSTER_PID_STAT,	.item=1 },
	{ .name = "sname",	    .cluster = CLUSTER_PID_STAT,	.item=2 },
	{ .name = "ppid",	    .cluster = CLUSTER_PID_STAT,	.item=3 },
	{ .name = "pgrp",	    .cluster = CLUSTER_PID_STAT,	.item=4 },
	{ .name = "session",	    .cluster = CLUSTER_PID_STAT,	.item=5 },
	{ .name = "tty",	    .cluster = CLUSTER_PID_STAT,	.item=6 },
	{ .name = "tty_pgrp",	    .cluster = CLUSTER_PID_STAT,	.item=7 },
	{ .name = "flags",	    .cluster = CLUSTER_PID_STAT,	.item=8 },
	{ .name = "minflt",	    .cluster = CLUSTER_PID_STAT,	.item=9 },
	{ .name = "cmin_flt",	    .cluster = CLUSTER_PID_STAT,	.item=10 },
	{ .name = "maj_flt",	    .cluster = CLUSTER_PID_STAT,	.item=11 },
	{ .name = "cmaj_flt",	    .cluster = CLUSTER_PID_STAT,	.item=12 },
	{ .name = "utime",	    .cluster = CLUSTER_PID_STAT,	.item=13 },
	{ .name = "stime",	    .cluster = CLUSTER_PID_STAT,	.item=14 },
	{ .name = "cutime",	    .cluster = CLUSTER_PID_STAT,	.item=15 },
	{ .name = "cstime",	    .cluster = CLUSTER_PID_STAT,	.item=16 },
	{ .name = "priority",	    .cluster = CLUSTER_PID_STAT,	.item=17 },
	{ .name = "nice",	    .cluster = CLUSTER_PID_STAT,	.item=18 },
	{ .name = "it_real_value",  .cluster = CLUSTER_PID_STAT,	.item=20 },
	{ .name = "start_time",	    .cluster = CLUSTER_PID_STAT,	.item=21 },
	{ .name = "vsize",	    .cluster = CLUSTER_PID_STAT,	.item=22 },
	{ .name = "rss",	    .cluster = CLUSTER_PID_STAT,	.item=23 },
	{ .name = "rss_rlim",	    .cluster = CLUSTER_PID_STAT,	.item=24 },
	{ .name = "start_code",	    .cluster = CLUSTER_PID_STAT,	.item=25 },
	{ .name = "end_code",	    .cluster = CLUSTER_PID_STAT,	.item=26 },
	{ .name = "start_stack",    .cluster = CLUSTER_PID_STAT,	.item=27 },
	{ .name = "esp",	    .cluster = CLUSTER_PID_STAT,	.item=28 },
	{ .name = "eip",	    .cluster = CLUSTER_PID_STAT,	.item=29 },
	{ .name = "signal",	    .cluster = CLUSTER_PID_STAT,	.item=30 },
	{ .name = "blocked",	    .cluster = CLUSTER_PID_STAT,	.item=31 },
	{ .name = "sigignore",	    .cluster = CLUSTER_PID_STAT,	.item=32 },
	{ .name = "sigcatch",	    .cluster = CLUSTER_PID_STAT,	.item=33 },
	{ .name = "wchan",	    .cluster = CLUSTER_PID_STAT,	.item=34 },
	{ .name = "nswap",	    .cluster = CLUSTER_PID_STAT,	.item=35 },
	{ .name = "cnswap",	    .cluster = CLUSTER_PID_STAT,	.item=36 },
	{ .name = "exit_signal",    .cluster = CLUSTER_PID_STAT,	.item=37 },
	{ .name = "processor",	    .cluster = CLUSTER_PID_STAT,	.item=38 },
	{ .name = "ttyname",	    .cluster = CLUSTER_PID_STAT,	.item=39 },
	{ .name = "wchan_s",	    .cluster = CLUSTER_PID_STAT,	.item=40 },
	{ .name = "psargs",	    .cluster = CLUSTER_PID_STAT,	.item=41 },
	{ .name = "rt_priority",    .cluster = CLUSTER_PID_STAT,	.item=42 },
	{ .name = "policy",	    .cluster = CLUSTER_PID_STAT,	.item=43 },
	{ .name = "delayacct_blkio_time",   .cluster = CLUSTER_PID_STAT,.item=44 },
	{ .name = "guest_time",	    .cluster = CLUSTER_PID_STAT,	.item=45 },
	{ .name = "cguest_time",    .cluster = CLUSTER_PID_STAT,	.item=46 },
	{ .name = "environ",        .cluster = CLUSTER_PID_STAT,	.item=47 },
	{ .name = "policy_s",	    .cluster = CLUSTER_PID_STAT,	.item=48 },
	{ .name = "signal_s",	    .cluster = CLUSTER_PID_STATUS,	.item=16 },
	{ .name = "blocked_s",	    .cluster = CLUSTER_PID_STATUS,	.item=17 },
	{ .name = "sigignore_s",    .cluster = CLUSTER_PID_STATUS,	.item=18 },
	{ .name = "sigcatch_s",	    .cluster = CLUSTER_PID_STATUS,	.item=19 },
	{ .name = "threads",	    .cluster = CLUSTER_PID_STATUS,	.item=28 },
	{ .name = "cgroups",	    .cluster = CLUSTER_PID_CGROUP,	.item=0 },
	{ .name = "labels",	    .cluster = CLUSTER_PID_LABEL,	.item=0 },
	{ .name = "vctxsw",	    .cluster = CLUSTER_PID_STATUS,	.item=29 },
	{ .name = "nvctxsw",	    .cluster = CLUSTER_PID_STATUS,	.item=30 },
	{ .name = "cpusallowed",    .cluster = CLUSTER_PID_STATUS,	.item=31 },
	{ .name = "ngid",	    .cluster = CLUSTER_PID_STATUS,	.item=32 },
        { .name = "tgid",	    .cluster = CLUSTER_PID_STATUS,	.item=41 },
	{ .name = "oom_score",	    .cluster = CLUSTER_PID_OOM_SCORE,	.item=0 },
	{ .name = "exe",	    .cluster = CLUSTER_PID_EXE,		.item=0 },
	{ .name = "cwd",	    .cluster = CLUSTER_PID_CWD,		.item=0 },
};

static dynproc_metric_t id_metrics[] = {
        { .name = "uid",	.cluster = CLUSTER_PID_STATUS,  .item=0 },
        { .name = "euid",	.cluster = CLUSTER_PID_STATUS,  .item=1 },
        { .name = "suid",	.cluster = CLUSTER_PID_STATUS,  .item=2 },
        { .name = "fsuid",	.cluster = CLUSTER_PID_STATUS,  .item=3 },
        { .name = "gid",	.cluster = CLUSTER_PID_STATUS,  .item=4 },
        { .name = "egid",	.cluster = CLUSTER_PID_STATUS,  .item=5 },
        { .name = "sgid",	.cluster = CLUSTER_PID_STATUS,  .item=6 },
        { .name = "fsgid",	.cluster = CLUSTER_PID_STATUS,  .item=7 },
        { .name = "uid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=8 },
        { .name = "euid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=9 },
        { .name = "suid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=10 },
        { .name = "fsuid_nm",   .cluster = CLUSTER_PID_STATUS,  .item=11 },
        { .name = "gid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=12 },
        { .name = "egid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=13 },
        { .name = "sgid_nm",	.cluster = CLUSTER_PID_STATUS,  .item=14 },
        { .name = "fsgid_nm",   .cluster = CLUSTER_PID_STATUS,  .item=15 },
        { .name = "container",  .cluster = CLUSTER_PID_CGROUP,  .item=1 },
};

static dynproc_metric_t memory_metrics[] = {
        { .name = "size",     .cluster = CLUSTER_PID_STATM,  .item=0 },
        { .name = "rss",      .cluster = CLUSTER_PID_STATM,  .item=1 },
        { .name = "share",    .cluster = CLUSTER_PID_STATM,  .item=2 },
        { .name = "textrss",  .cluster = CLUSTER_PID_STATM,  .item=3 },
        { .name = "librss",   .cluster = CLUSTER_PID_STATM,  .item=4 },
        { .name = "datrss",   .cluster = CLUSTER_PID_STATM,  .item=5 },
        { .name = "dirty",    .cluster = CLUSTER_PID_STATM,  .item=6 },
        { .name = "maps",     .cluster = CLUSTER_PID_STATM,  .item=7 },
        { .name = "vmsize",   .cluster = CLUSTER_PID_STATUS, .item=20 },
        { .name = "vmlock",   .cluster = CLUSTER_PID_STATUS, .item=21 },
        { .name = "vmrss",    .cluster = CLUSTER_PID_STATUS, .item=22 },
        { .name = "vmdata",   .cluster = CLUSTER_PID_STATUS, .item=23 },
        { .name = "vmstack",  .cluster = CLUSTER_PID_STATUS, .item=24 },
        { .name = "vmexe",    .cluster = CLUSTER_PID_STATUS, .item=25 },
        { .name = "vmlib",    .cluster = CLUSTER_PID_STATUS, .item=26 },
        { .name = "vmswap",   .cluster = CLUSTER_PID_STATUS, .item=27 },
        { .name = "vmpeak",   .cluster = CLUSTER_PID_STATUS, .item=33 },
	{ .name = "vmpin",    .cluster = CLUSTER_PID_STATUS, .item=34 },
	{ .name = "vmhwm",    .cluster = CLUSTER_PID_STATUS, .item=35 },
	{ .name = "vmpte",    .cluster = CLUSTER_PID_STATUS, .item=36 },
	{ .name = "vmreal",   .cluster = CLUSTER_PID_STATUS, .item=43 },
	{ .name = "vmnonlib", .cluster = CLUSTER_PID_STATUS, .item=44 },
};

static dynproc_metric_t namespace_metrics[] = {
        { .name = "tgid",   .cluster = CLUSTER_PID_STATUS, .item=37 },
        { .name = "pid",    .cluster = CLUSTER_PID_STATUS, .item=38 },
        { .name = "pgid",   .cluster = CLUSTER_PID_STATUS, .item=39 },
        { .name = "sid",    .cluster = CLUSTER_PID_STATUS, .item=40 },
        { .name = "envid",  .cluster = CLUSTER_PID_STATUS, .item=42 },
};

static dynproc_metric_t autogroup_metrics[] = {
        { .name = "enabled",  .cluster = CLUSTER_PID_AUTOGROUP, .item=0 },
        { .name = "id",       .cluster = CLUSTER_PID_AUTOGROUP, .item=1 },
        { .name = "nice",     .cluster = CLUSTER_PID_AUTOGROUP, .item=2 },
};

static dynproc_metric_t io_metrics[] = {
        { .name = "rchar",		    .cluster = CLUSTER_PID_IO,  .item=0 },
        { .name = "wchar",		    .cluster = CLUSTER_PID_IO,  .item=1 },
        { .name = "syscr",		    .cluster = CLUSTER_PID_IO,  .item=2 },
        { .name = "syscw",		    .cluster = CLUSTER_PID_IO,  .item=3 },
        { .name = "read_bytes",		    .cluster = CLUSTER_PID_IO,  .item=4 },
        { .name = "write_bytes",	    .cluster = CLUSTER_PID_IO,  .item=5 },
        { .name = "cancelled_write_bytes",  .cluster = CLUSTER_PID_IO,  .item=6 },
};

static dynproc_metric_t fd_metrics[] = {
        { .name = "count",   .cluster = 51,  .item=0 },
};

static dynproc_metric_t schedstat_metrics[] = {
	{ .name = "cpu_time",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=0 },
	{ .name = "run_delay",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=1 },
	{ .name = "pcount",	.cluster = CLUSTER_PID_SCHEDSTAT,	.item=2 },
};

static dynproc_metric_t smaps_metrics[] = {
	{ .name = "rss",             .cluster = CLUSTER_PID_SMAPS,  .item=0 },
	{ .name = "pss",             .cluster = CLUSTER_PID_SMAPS,  .item=1 },
	{ .name = "pss_anon",        .cluster = CLUSTER_PID_SMAPS,  .item=2 },
	{ .name = "pss_file",        .cluster = CLUSTER_PID_SMAPS,  .item=3 },
	{ .name = "pss_shmem",       .cluster = CLUSTER_PID_SMAPS,  .item=4 },
	{ .name = "shared_clean",    .cluster = CLUSTER_PID_SMAPS,  .item=5 },
	{ .name = "shared_dirty",    .cluster = CLUSTER_PID_SMAPS,  .item=6 },
	{ .name = "private_clean",   .cluster = CLUSTER_PID_SMAPS,  .item=7 },
	{ .name = "private_dirty",   .cluster = CLUSTER_PID_SMAPS,  .item=8 },
	{ .name = "referenced",      .cluster = CLUSTER_PID_SMAPS,  .item=9 },
	{ .name = "anonymous",       .cluster = CLUSTER_PID_SMAPS,  .item=10 },
	{ .name = "lazyfree",        .cluster = CLUSTER_PID_SMAPS,  .item=11 },
	{ .name = "anonhugepages",   .cluster = CLUSTER_PID_SMAPS,  .item=12 },
	{ .name = "shmempmdmapped",  .cluster = CLUSTER_PID_SMAPS,  .item=13 },
	{ .name = "filepmdmapped",   .cluster = CLUSTER_PID_SMAPS,  .item=14 },
	{ .name = "shared_hugetlb",  .cluster = CLUSTER_PID_SMAPS,  .item=15 },
	{ .name = "private_hugetlb", .cluster = CLUSTER_PID_SMAPS,  .item=16 },
	{ .name = "swap",            .cluster = CLUSTER_PID_SMAPS,  .item=17 },
	{ .name = "swappss",         .cluster = CLUSTER_PID_SMAPS,  .item=18 },
	{ .name = "locked",          .cluster = CLUSTER_PID_SMAPS,  .item=19 },
	{ .name = "pss_dirty",       .cluster = CLUSTER_PID_SMAPS,  .item=20 },
};

static dynproc_metric_t fdinfo_metrics[] = {
	{ .name = "drm_memory_cpu",             .cluster = CLUSTER_PID_FDINFO, .item=0 },
	{ .name = "drm_memory_gtt",             .cluster = CLUSTER_PID_FDINFO, .item=1 },
	{ .name = "drm_memory_vram",            .cluster = CLUSTER_PID_FDINFO, .item=2 },
	{ .name = "drm_shared_cpu",             .cluster = CLUSTER_PID_FDINFO, .item=3 },
	{ .name = "drm_shared_gtt",             .cluster = CLUSTER_PID_FDINFO, .item=4 },
	{ .name = "drm_shared_vram",            .cluster = CLUSTER_PID_FDINFO, .item=5 },
	{ .name = "amd_evicted_visible_vram",   .cluster = CLUSTER_PID_FDINFO, .item=6 },
	{ .name = "amd_evicted_vram",           .cluster = CLUSTER_PID_FDINFO, .item=7 },
	{ .name = "amd_memory_visible_vram",    .cluster = CLUSTER_PID_FDINFO, .item=8 },
	{ .name = "amd_requested_gtt",          .cluster = CLUSTER_PID_FDINFO, .item=9 },
	{ .name = "amd_requested_visible_vram", .cluster = CLUSTER_PID_FDINFO, .item=10 },
	{ .name = "amd_requested_vram",         .cluster = CLUSTER_PID_FDINFO, .item=11 },
};

static dynproc_group_t dynproc_groups[] = {
	[DYNPROC_GROUP_PSINFO]    = { .name = "psinfo",	    .metrics = psinfo_metrics,	    .nmetrics = sizeof(psinfo_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_ID]	  = { .name = "id",	    .metrics = id_metrics,	    .nmetrics = sizeof(id_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_MEMORY]    = { .name = "memory",	    .metrics = memory_metrics,	    .nmetrics = sizeof(memory_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_IO]	  = { .name = "io",	    .metrics = io_metrics,	    .nmetrics = sizeof(io_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_FD]	  = { .name = "fd",	    .metrics = fd_metrics,	    .nmetrics = sizeof(fd_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_SCHEDSTAT] = { .name = "schedstat",  .metrics = schedstat_metrics,   .nmetrics = sizeof(schedstat_metrics)/sizeof(dynproc_metric_t) },
	[DYNPROC_GROUP_NAMESPACE] = { .name = "namespaces", .metrics = namespace_metrics,   .nmetrics = sizeof(namespace_metrics)/sizeof(dynproc_metric_t) },
	[DYNPROC_GROUP_SMAPS]     = { .name = "smaps",	    .metrics = smaps_metrics,	    .nmetrics = sizeof(smaps_metrics)/sizeof(dynproc_metric_t)},
	[DYNPROC_GROUP_AUTOGROUP] = { .name = "autogroup", .metrics = autogroup_metrics,   .nmetrics = sizeof(autogroup_metrics)/sizeof(dynproc_metric_t) },
	[DYNPROC_GROUP_FDINFO]    = { .name = "fdinfo",	    .metrics = fdinfo_metrics,	    .nmetrics = sizeof(fdinfo_metrics)/sizeof(dynproc_metric_t) },
};

/*
 * Get the hotproc cluster that corresponds to this proc cluster
 */

static int
get_hot_cluster(int proc_cluster)
{
    int i;
    int num_mappings = sizeof(proc_hotproc_cluster_list)/(sizeof(int)*2);

    for (i = 0; i < num_mappings; i++) {
	if (proc_hotproc_cluster_list[i][0] == proc_cluster)
	    return proc_hotproc_cluster_list[i][1];
    }
    return -1;
}

/*
 * Given a cluster/item return the name
 */
static int
get_name(int cluster, int item, char *name, int length)
{
    unsigned int tree, group, metric;
    int num_dynproc_trees = sizeof(dynproc_members)/sizeof(char*);
    int num_dynproc_groups = sizeof(dynproc_groups)/sizeof(dynproc_group_t);

    for (tree = 0; tree < num_dynproc_trees; tree++) {
        for (group = 0; group < num_dynproc_groups; group++) {
            dynproc_metric_t * cur_metrics = dynproc_groups[group].metrics;
            int num_cur_metrics = dynproc_groups[group].nmetrics;

            for (metric = 0; metric < num_cur_metrics; metric++) {
		int _cluster =  cur_metrics[metric].cluster;
                int _item =  cur_metrics[metric].item;

		if (tree == DYNPROC_HOTPROC)
		    _cluster = get_hot_cluster(_cluster);

		if (_cluster == cluster && _item == item) {
		    pmsprintf(name, length, "%s.%s",
			dynproc_groups[group].name, cur_metrics[metric].name); 
		    return 1;
		}
	    }
	}
    }
    return 0;
}

/*
 * Loop through the structs and build the namespace
 */

static void
build_dynamic_proc_tree(int domain)
{
    char entry[128];
    pmID pmid;
    unsigned int num_hash_entries=0;
    unsigned int tree, group, metric;
    int num_dynproc_trees = sizeof(dynproc_members)/sizeof(char*);
    int num_dynproc_groups = sizeof(dynproc_groups)/sizeof(dynproc_group_t);

    for (tree = 0; tree < num_dynproc_trees; tree++) {
	for (group = 0; group < num_dynproc_groups; group++) {

	    dynproc_metric_t * cur_metrics = dynproc_groups[group].metrics;
	    int num_cur_metrics = dynproc_groups[group].nmetrics;

	    for (metric = 0; metric < num_cur_metrics; metric++) {

		pmsprintf(entry, sizeof(entry), "%s.%s.%s",
			dynproc_members[tree], dynproc_groups[group].name,
			cur_metrics[metric].name);

		int cluster =  cur_metrics[metric].cluster;
		int item =  cur_metrics[metric].item;

		if (tree == DYNPROC_HOTPROC)
		    cluster = get_hot_cluster(cluster);

		pmid = pmID_build(domain, cluster, item);
		pmdaTreeInsert(dynamic_proc_tree, pmid, entry);

		num_hash_entries++;
	    }
	}
    }

    /* Must now call this in all cases when we update the tree */
    pmdaTreeRebuildHash(dynamic_proc_tree, num_hash_entries);
}

/*
 * Create a new metric table entry based on an existing one.
 * Will use the templates we have in pmda.c, modifying cluster values
 * In this case we assume the only 2 metric groups are proc and hotproc
 *
 * I assume id=0 is proc and id=1 is hotproc.
 * Not even called for 0 here
 *
 * Only metrics that are supposed to
 * be dynamic should flow through here
 *
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int id)
{
    int domain = pmID_domain(source->m_desc.pmid);
    int cluster = pmID_cluster(source->m_desc.pmid);
    int item = pmID_item(source->m_desc.pmid);
    int hotcluster = -1;

    memcpy(dest, source, sizeof(pmdaMetric));

    if (id == 1) {
	hotcluster = get_hot_cluster(cluster);
	if (hotcluster != -1) {
	    dest->m_desc.pmid = pmID_build(domain, hotcluster, item);
	    if (source->m_desc.indom == PM_INDOM_NULL) {
		dest->m_desc.indom = PM_INDOM_NULL;
	    }
	    else {
		/* Indom is not set for us with dynamic metrics */
		dest->m_desc.indom = pmInDom_build(domain, HOTPROC_INDOM);
	    }
	}
	else {
	    fprintf(stderr, "Got bad hotproc cluster for %d:%d:%d id=%d\n",
		domain, cluster, item, id);
	}
    }
    else {
	fprintf(stderr, "DYNAMIC PROC : refresh_metrictable called for %d:%d:%d id=%d\n", domain, cluster, item, id);
	fprintf(stderr, "Did you try to add another dynamic proc tree? Implementation incomplete.\n");
    }
}

/* Add the pmns entries based on appropriate cluster and namespace information
 *
 * A little more complicated. Need to build up the full tree from all the structs at the top.
 *
 */
static int
refresh_dynamic_proc(pmdaExt *pmda, pmdaNameSpace **tree)
{
    int sts, dom = pmda->e_domain;

    if (dynamic_proc_tree) {
        *tree = dynamic_proc_tree;
    } else if ((sts = pmdaTreeCreate(&dynamic_proc_tree)) < 0) {
        pmNotifyErr(LOG_ERR, "%s: failed to create dynamic_proc names: %s\n",
                        pmGetProgname(), pmErrStr(sts));
        *tree = NULL;
    } else {
	/* Construct the PMNS by multiple calls to pmdaTreeInsert */
	build_dynamic_proc_tree(dom);
	*tree = dynamic_proc_tree;
	/* Only return 1 if we are building the pmns */
        return 1;
    }
    return 0;
}

/* 
 * This should only return the number of additional entries, 
 * do not inclue the "template" entries in pmda.c
 *
 * Otherwise you get "orphaned" entries that waste space
 * 
 */

static void
size_metrictable(int *total, int *trees)
{
    int i, num_leaf_nodes = 0;
    int num_dyn_groups =  sizeof(dynproc_groups)/sizeof(dynproc_group_t);

    for (i = 0; i < num_dyn_groups; i++) {
	num_leaf_nodes += dynproc_groups[i].nmetrics;
    }

    *total = num_leaf_nodes; /* calc based on all the structs above. */
    *trees = sizeof(dynproc_members)/sizeof(char*) -1; /* will be 1 (hotproc) */

    if (pmDebugOptions.libpmda)
        fprintf(stderr, "size_metrictable: %d total x %d trees\n",
                *total, *trees);
}

/*
 * Header file of help text, generated from the original static help file by a perl script 
 * Shared by both proc and hotproc
 */

#include "help_text.h"

static int
dynamic_proc_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    int item = pmID_item(pmid);
    int cluster = pmID_cluster(pmid);
    char name[128];

    if (get_name(cluster, item, name, sizeof(name))) {
        int num_help_entries = sizeof(help_text)/sizeof(help_text_t);
        int i;

        for (i = 0; i < num_help_entries; i++) {
	    if (strcmp(name, help_text[i].name) == 0) {
		if ((type & PM_TEXT_ONELINE) || help_text[i].longhelp[0] =='\0')
		    *buf = help_text[i].shorthelp;
		else
		    *buf = help_text[i].longhelp;
		return 0;
	    }
	}
    }
    *buf = "";
    return 0;
}

/*
 * Build up the dynamic infrastructure.
 */
void
proc_dynamic_init(pmdaMetric *metrics, int nmetrics)
{
    int clusters[1] = {0}; /* Not needed, kept for interface compatibility */
    int nclusters = 0;

    pmdaDynamicPMNS("proc",
                    clusters, nclusters,
                    refresh_dynamic_proc, dynamic_proc_text,
                    refresh_metrictable, size_metrictable,
                    metrics, nmetrics);
}
