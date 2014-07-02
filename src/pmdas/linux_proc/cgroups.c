/*
 * Copyright (c) 2012-2014 Red Hat.
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
#include "cgroups.h"
#include "clusters.h"
#include "proc_pid.h"
#include <sys/stat.h>
#include <ctype.h>

#define CGROUP_ROOT	"cgroup.groups" /* root dynamic PMNS node */

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
static int prepare_block_ull(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);
static int prepare_blocks_ull(__pmnsTree *, const char *,
		struct cgroup_subsys *, const char *, int, int, int);

/*
 * Critical data structures for cgroup subsystem in pmdaproc ...
 * Initial comment for each struct talks about lifecycle of that
 * data, in terms of what pmdaproc must do with it (esp. memory
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
    int			group_count;	/* number of groups (dynamic) */
    int			metric_count;	/* number of metrics (fixed)  */
    time_t		previous_time;	/* used to avoid repeated refresh */
    cgroup_group_t	*groups;	/* array of groups (dynamic)  */
    cgroup_metrics_t	*metrics;	/* array of metrics (fixed)   */
} cgroup_subsys_t;

static cgroup_metrics_t cpusched_metrics[] = {
    { .suffix = "shares",			.prepare = prepare_ull },
};

static cgroup_metrics_t cpuacct_metrics[] = {
    { .suffix = "stat.user",			.prepare = prepare_named_ull },
    { .suffix = "stat.system",			.prepare = prepare_named_ull },
    { .suffix = "usage",			.prepare = prepare_ull },
    { .suffix = "usage_percpu",			.prepare = prepare_ull },
};

static cgroup_metrics_t cpuset_metrics[] = {
    { .suffix = "io_merged",			.prepare = prepare_string },
    { .suffix = "sectors",			.prepare = prepare_string },
};

static cgroup_metrics_t memory_metrics[] = {
    { .suffix = "stat.cache",			.prepare = prepare_named_ull },
    { .suffix = "stat.rss",			.prepare = prepare_named_ull },
    { .suffix = "stat.rss_huge",		.prepare = prepare_named_ull },
    { .suffix = "stat.mapped_file",		.prepare = prepare_named_ull },
    { .suffix = "stat.writeback",		.prepare = prepare_named_ull },
    { .suffix = "stat.swap",			.prepare = prepare_named_ull },
    { .suffix = "stat.pgpgin",			.prepare = prepare_named_ull },
    { .suffix = "stat.pgpgout",			.prepare = prepare_named_ull },
    { .suffix = "stat.pgfault",			.prepare = prepare_named_ull },
    { .suffix = "stat.pgmajfault",		.prepare = prepare_named_ull },
    { .suffix = "stat.inactive_anon",		.prepare = prepare_named_ull },
    { .suffix = "stat.active_anon",		.prepare = prepare_named_ull },
    { .suffix = "stat.inactive_file",		.prepare = prepare_named_ull },
    { .suffix = "stat.active_file",		.prepare = prepare_named_ull },
    { .suffix = "stat.unevictable",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_cache",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_rss",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_rss_huge",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_mapped_file",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_writeback",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_swap",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_pgpgin",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_pgpgout",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_pgfault",		.prepare = prepare_named_ull },
    { .suffix = "stat.total_pgmajfault",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_inactive_anon",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_active_anon",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_inactive_file",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_active_file",	.prepare = prepare_named_ull },
    { .suffix = "stat.total_unevictable",	.prepare = prepare_named_ull },
    { .suffix = "stat.recent_rotated_anon",	.prepare = prepare_named_ull },
    { .suffix = "stat.recent_rotated_file",	.prepare = prepare_named_ull },
    { .suffix = "stat.recent_scanned_anon",	.prepare = prepare_named_ull },
    { .suffix = "stat.recent_scanned_file",	.prepare = prepare_named_ull },
};

static cgroup_metrics_t netclass_metrics[] = {
    { .suffix = "classid",			.prepare = prepare_ull },
};

static cgroup_metrics_t blkio_metrics[] = {
    { .suffix = "io_merged.read",		.prepare = prepare_blocks_ull },
    { .suffix = "io_merged.write",		.prepare = prepare_blocks_ull },
    { .suffix = "io_merged.sync",		.prepare = prepare_blocks_ull },
    { .suffix = "io_merged.async",		.prepare = prepare_blocks_ull },
    { .suffix = "io_merged.total",		.prepare = prepare_blocks_ull },
    { .suffix = "io_queued.read",		.prepare = prepare_blocks_ull },
    { .suffix = "io_queued.write",		.prepare = prepare_blocks_ull },
    { .suffix = "io_queued.sync",		.prepare = prepare_blocks_ull },
    { .suffix = "io_queued.async",		.prepare = prepare_blocks_ull },
    { .suffix = "io_queued.total",		.prepare = prepare_blocks_ull },
    { .suffix = "io_service_bytes.read",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_bytes.write",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_bytes.sync",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_bytes.async",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_bytes.total",	.prepare = prepare_blocks_ull },
    { .suffix = "io_serviced.read",		.prepare = prepare_blocks_ull },
    { .suffix = "io_serviced.write",		.prepare = prepare_blocks_ull },
    { .suffix = "io_serviced.sync",		.prepare = prepare_blocks_ull },
    { .suffix = "io_serviced.async",		.prepare = prepare_blocks_ull },
    { .suffix = "io_serviced.total",		.prepare = prepare_blocks_ull },
    { .suffix = "io_service_time.read",		.prepare = prepare_blocks_ull },
    { .suffix = "io_service_time.write",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_time.sync",		.prepare = prepare_blocks_ull },
    { .suffix = "io_service_time.async",	.prepare = prepare_blocks_ull },
    { .suffix = "io_service_time.total",	.prepare = prepare_blocks_ull },
    { .suffix = "io_wait_time.read",		.prepare = prepare_blocks_ull },
    { .suffix = "io_wait_time.write",		.prepare = prepare_blocks_ull },
    { .suffix = "io_wait_time.sync",		.prepare = prepare_blocks_ull },
    { .suffix = "io_wait_time.async",		.prepare = prepare_blocks_ull },
    { .suffix = "io_wait_time.total",		.prepare = prepare_blocks_ull },
    { .suffix = "sectors",			.prepare = prepare_block_ull },
    { .suffix = "time",				.prepare = prepare_block_ull },
};

static const char *block_stats_names[] = \
	{ "read", "write", "sync", "async", "total" };
#define BLKIOS	(sizeof(block_stats_names)/sizeof(block_stats_names[0]))

static cgroup_subsys_t controllers[] = {
    {	.name = "cpu",
	.cluster = CLUSTER_CPUSCHED_GROUPS,
	.metrics = cpusched_metrics,
	.metric_count = sizeof(cpusched_metrics) / sizeof(cgroup_metrics_t),
    },
    {	.name = "cpuset",
	.cluster = CLUSTER_CPUSET_GROUPS,
	.metrics = cpuset_metrics,
	.metric_count = sizeof(cpuset_metrics) / sizeof(cgroup_metrics_t),
    },
    {	.name = "cpuacct",
	.cluster = CLUSTER_CPUACCT_GROUPS,
	.metrics = cpuacct_metrics,
	.metric_count = sizeof(cpuacct_metrics) / sizeof(cgroup_metrics_t),
    },
    {	.name = "memory",
	.cluster = CLUSTER_MEMORY_GROUPS,
	.metrics = memory_metrics,
	.metric_count = sizeof(memory_metrics) / sizeof(cgroup_metrics_t),
    },
    {	.name = "net_cls",
	.cluster = CLUSTER_NET_CLS_GROUPS,
	.metrics = netclass_metrics,
	.metric_count = sizeof(netclass_metrics) / sizeof(cgroup_metrics_t),
    },
    {	.name = "blkio",
	.cluster = CLUSTER_BLKIO_GROUPS,
	.metrics = blkio_metrics,
	.metric_count = sizeof(blkio_metrics) / sizeof(cgroup_metrics_t),
    },
};

/*
 * Data structures used by individual cgroup subsystem controllers
 */
typedef struct {
    __uint32_t	major;
    __uint32_t	minor;
    int		inst;
    char	*name;
} device_t;

typedef struct {
    device_t	dev;
    __uint64_t	values[BLKIOS];	/* read, write, sync, async, total */
} block_stats_t;

typedef struct filesys {
    int		id;
    char	*device;
    char	*path;
    char	*options;
} filesys_t;

void
refresh_cgroup_cpus(pmInDom indom)
{
    char buf[MAXPATHLEN];
    char *space;
    FILE *fp;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    if ((fp = proc_statsfile("/proc/stat", buf, sizeof(buf))) == NULL)
	return;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "cpu", 3) == 0 && isdigit((int)buf[3])) {
	    if ((space = strchr(buf, ' ')) != NULL) {
	    	*space = '\0';
		pmdaCacheStore(indom, PMDA_CACHE_ADD, buf, NULL);
	    }
	}
    }
    fclose(fp);
}

static int
_pm_isloop(char *dname)
{
    return strncmp(dname, "loop", 4) == 0;
}

static int
_pm_isramdisk(char *dname)
{
    return strncmp(dname, "ram", 3) == 0;
}

/*
 * For block devices we have one instance domain for dev_t
 * based lookup, and another for (real) name lookup.
 * The reason we need this is that the blkio cgroup stats
 * are exported using the major:minor numbers, and not the
 * device names - we must perform that mapping ourselves.
 * In some places (value refresh) we need to lookup the blk
 * name from device major/minor, in other places (instances
 * refresh) we need the usual external instid:name lookup.
 */
void
refresh_cgroup_devices(pmInDom diskindom)
{
    pmInDom devtindom = INDOM(DEVT_INDOM);
    char buf[MAXPATHLEN];
    static time_t before;
    time_t now;
    FILE *fp;

    if ((now = time(NULL)) == before)
	return;
    before = now;

    pmdaCacheOp(devtindom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(diskindom, PMDA_CACHE_INACTIVE);

    if ((fp = proc_statsfile("/proc/diskstats", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	unsigned int major, minor, unused;
	device_t *dev = NULL;
	char namebuf[1024];
	int inst;

	if (sscanf(buf, "%u %u %s %u", &major, &minor, namebuf, &unused) != 4)
	    continue;
	if (_pm_isloop(namebuf) || _pm_isramdisk(namebuf))
	    continue;
	if (pmdaCacheLookupName(diskindom, namebuf, &inst, (void **)&dev) < 0 ||
	    dev == NULL) {
	    if (!(dev = (device_t *)malloc(sizeof(device_t)))) {
		__pmNoMem("device", sizeof(device_t), PM_RECOV_ERR);
		continue;
	    }
	    dev->major = major;
	    dev->minor = minor;
	}
	/* keeping track of all fields (major/minor/inst/name) */
	pmdaCacheStore(diskindom, PMDA_CACHE_ADD, namebuf, dev);
	pmdaCacheLookupName(diskindom, namebuf, &dev->inst, NULL);
	pmdaCacheLookup(diskindom, dev->inst, &dev->name, NULL);

	snprintf(buf, sizeof(buf), "%u:%u", major, minor);
	pmdaCacheStore(devtindom, PMDA_CACHE_ADD, buf, (void *)dev);

	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "refresh_devices: \"%s\" \"%d:%d\" inst=%d\n",
			dev->name, dev->major, dev->minor, dev->inst);
    }
    fclose(fp);
}

void
refresh_cgroup_subsys(pmInDom indom)
{
    char buf[4096];
    static time_t before;
    time_t now;
    FILE *fp;

    if ((now = time(NULL)) == before)
	return;
    before = now;

    if ((fp = proc_statsfile("/proc/cgroups", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	unsigned int numcgroups, enabled;
	char name[MAXPATHLEN];
	long hierarchy;
	long *data;
	int sts;

	/* skip lines starting with hash (header) */
	if (buf[0] == '#')
	    continue;
	if (sscanf(buf, "%s %ld %u %u", &name[0],
			&hierarchy, &numcgroups, &enabled) != 4)
	    continue;
	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&data);
	if (sts == PMDA_CACHE_ACTIVE) {
	    if (*data != hierarchy) {
		/*
		 * odd ... instance name repeated but different
		 * hierarchy ... we cannot support more than one hierarchy
		 * yet
		 */
		fprintf(stderr, "refresh_cgroup_subsys: \"%s\": entries for hierarchy %ld ignored (hierarchy %ld seen first)\n", name, hierarchy, *data);
	    }
	    continue;
	}
	else if (sts != PMDA_CACHE_INACTIVE) {
	    if ((data = (long *)malloc(sizeof(long))) == NULL) {
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_APPL0)
		    fprintf(stderr, "refresh_cgroup_subsys: \"%s\": malloc failed\n", name);
#endif
		continue;
	    }
	    *data = hierarchy;
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, name, (void *)data);
#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "refresh_cgroup_subsys: add \"%s\" [hierarchy %ld]\n", name, hierarchy);
#endif
    }
    fclose(fp);
}

void
refresh_cgroup_filesys(pmInDom indom)
{
    char buf[MAXPATHLEN];
    filesys_t *fs;
    FILE *fp;
    time_t now;
    static time_t before;
    char *path, *device, *type, *options;
    int sts;

    if ((now = time(NULL)) == before)
	return;
    before = now;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((fp = proc_statsfile("/proc/mounts", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	device = strtok(buf, " ");
	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	options = strtok(NULL, " ");
	if (strcmp(type, "cgroup") != 0)
	    continue;

	sts = pmdaCacheLookupName(indom, path, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, fs);
	    if (strcmp(path, fs->path) != 0) {	/* old device, new path */
		free(fs->path);
		fs->path = strdup(path);
	    }
	    if (strcmp(options, fs->options) != 0) {	/* old device, new opts */
		free(fs->options);
		fs->options = strdup(options);
	    }
	}
	else {	/* new mount */
	    if ((fs = malloc(sizeof(filesys_t))) == NULL)
		continue;
	    fs->path = strdup(path);
	    fs->options = strdup(options);
	    if (pmDebug & DBG_TRACE_APPL0)
		fprintf(stderr, "refresh_filesys: add \"%s\" \"%s\"\n",
			fs->path, device);
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, fs);
	}
    }
    fclose(fp);
}

static char *
scan_filesys_options(const char *options, const char *option)
{
    static char buffer[128];
    char *s;

    strncpy(buffer, options, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

    s = strtok(buffer, ",");
    while (s) {
	if (strcmp(s, option) == 0)
	    return s;
        s = strtok(NULL, ",");
    }
    return NULL;
}

static int
read_values(char *buffer, int size, const char *path, const char *subsys,
		const char *metric)
{
    int fd, count;

    snprintf(buffer, size, "%s/%s.%s", path, subsys, metric);
    if ((fd = open(buffer, O_RDONLY)) < 0)
	return -oserror();
    count = read(fd, buffer, size);
    close(fd);
    if (count < 0)
	return -oserror();
    buffer[count-1] = '\0';
    return 0;
}

static pmID
update_pmns(__pmnsTree *pmns, cgroup_subsys_t *subsys, const char *name,
		cgroup_metrics_t *metrics, int group, int domain)
{
    char entry[MAXPATHLEN];
    pmID pmid;

    snprintf(entry, sizeof(entry), "%s.%s%s.%s",
		CGROUP_ROOT, subsys->name, name, metrics->suffix);
    pmid = cgroup_pmid_build(domain, subsys->cluster, group, metrics->item);
    __pmAddPMNSNode(pmns, pmid, entry);
    return pmid;
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
	return -oserror();

    while (p && *p) {
	value = strtoull(p, &endp, 0);
	if ((atoms = realloc(atoms, (count + 1) * sizeof(pmAtomValue))) == NULL)
	    return -oserror();
	atoms[count++].ull = value;
	if (endp == '\0' || endp == p)
	    break;
	p = endp;
	while (p && isspace((int)*p))
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

    /* metric => e.g. stat.user and stat.system - split it up first */
    offset = index(metrics->suffix, '.');
    if (!offset)
	return PM_ERR_CONV;
    count = (offset - metrics->suffix);
    strncpy(filename, metrics->suffix, count);
    filename[count] = '\0';

    if (read_values(p, sizeof(buffer), path, subsys->name, filename) < 0)
	return -oserror();

    /* buffer contains <name> <value> pairs */
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
	while (p && isspace((int)*p))
	    p++;

	for (i = 0; i < subsys->metric_count; i++) {
	    pmAtomValue *atoms = groups->metric_values[i].atoms;
	    metrics = &subsys->metrics[i];

	    if (strcmp(field, metrics->suffix + count + 1) != 0)
		continue;
	    if ((atoms = groups->metric_values[i].atoms) == NULL)
		if ((atoms = calloc(1, sizeof(pmAtomValue))) == NULL)
		    return -oserror();
	    atoms[0].ull = value;

	    groups->metric_values[i].item = i;
	    groups->metric_values[i].atoms = atoms;
	    groups->metric_values[i].atom_count = 1;
	    update_pmns(pmns, subsys, name, metrics, group, domain);
	    break;
	}
    }
    return 0;
}

static int
prepare_block(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain,
		block_stats_t *stats, int value_count)
{
    pmID pmid;
    char *iname;
    char buf[MAXPATHLEN];
    device_t *dev;
    pmAtomValue *atoms;
    int delta, count, size, inst, sts, m, i, j;
    pmInDom devtindom = INDOM(DEVT_INDOM);
    cgroup_group_t *groups = &subsys->groups[group];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];

    /* map major:minor to real device name via diskstats */
    dev = &stats->dev;
    snprintf(buf, sizeof(buf), "%u:%u", dev->major, dev->minor);

    sts = pmdaCacheLookupName(devtindom, buf, NULL, (void **)&dev);
    iname = dev->name;
    inst = dev->inst;

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "prepare_block: preparing %s found=%s (%s)\n",
		buf, sts == PMDA_CACHE_ACTIVE ? "ok" : "no", iname);

    /* batch update metric value(s) now, since we have 'em all */
    for (j = 0; j < value_count; j++) {
	m = metric + j;
	atoms = groups->metric_values[m].atoms;
	count = groups->metric_values[m].atom_count;

	if (inst >= count) {
	    delta = inst - count;
	    size = (inst + 1) * sizeof(pmAtomValue);
	    if ((atoms = realloc(atoms, size)) == NULL)
		return -oserror();
	    for (i = count; i < inst + 1; i++)
		atoms[i].ull = ULLONG_MAX;
	    count = inst + 1;
	}
	/* move on-stack value into global struct, add to PMNS */
	atoms[inst].ull = stats->values[j];
	pmid = update_pmns(pmns, subsys, name, metrics + j, group, domain);

	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "prepare_block: prepared "
			    "metric=%s inst=%s[%d] value=%llu\n",
			    pmIDStr(pmid), iname, inst,
			    (unsigned long long)atoms[inst].ull);

	groups->metric_values[m].item = m;
	groups->metric_values[m].atoms = atoms;
	groups->metric_values[m].atom_count = count;
    }
    return 0;
}

static int
prepare_block_ull(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain)
{
    char buf[MAXPATHLEN];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];
    block_stats_t stats;
    FILE *fp;
    char *p;

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "prepare_block_ull: %s metric=%d group=%d domain=%d\n",
		path, metric, group, domain);

    snprintf(buf, sizeof(buf), "%s/%s.%s", path, subsys->name, metrics->suffix);
    if ((fp = fopen(buf, "r")) == NULL)
	return -oserror();

    memset(&stats, 0, sizeof(stats));
    while ((fgets(buf, sizeof(buf), fp)) != NULL) {
	if (sscanf(buf, "%u:%u ", &stats.dev.major, &stats.dev.minor) != 2)
	    continue;
	for (p = buf; *p && !isspace(*p); p++) { } /* skip device number */
	for (p = buf; *p && isspace(*p); p++) { }  /* skip over spaces */
	if (sscanf(p, "%llu", (unsigned long long *)&stats.values[0]) != 1)
	    stats.values[0] = 0;
	prepare_block(pmns, path, subsys, name,
			  metric, group, domain, &stats, 1);
    }
    fclose(fp);
    return 0;
}

static int
prepare_blocks_ull(__pmnsTree *pmns, const char *path, cgroup_subsys_t *subsys,
		const char *name, int metric, int group, int domain)
{
    char buf[MAXPATHLEN];
    cgroup_metrics_t *metrics = &subsys->metrics[metric];
    block_stats_t stats;
    FILE *fp;
    char *p;
    int j;

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "prepare_blocks_ull: %s metric=%d group=%d domain=%d\n",
		path, metric, group, domain);

    if (metric % BLKIOS != 0)
	return 0;

    snprintf(buf, sizeof(buf), "%s/%s.%s", path, subsys->name, metrics->suffix);
    buf[strlen(buf) - sizeof("read")] = '\0';

    if (pmDebug & DBG_TRACE_APPL2)
	fprintf(stderr, "prepare_blocks_ull: opening \"%s\"\n", buf);

    if ((fp = fopen(buf, "r")) == NULL)
	return -oserror();

    memset(&stats, 0, sizeof(stats));
    while ((fgets(buf, sizeof(buf), fp)) != NULL) {
	if (sscanf(buf, "%u:%u ", &stats.dev.major, &stats.dev.minor) != 2)
	    continue;

	/* iterate over read/write/sync/async/total (reverse for async) */
	for (j = BLKIOS-1; j >= 0; j--) {
	    if ((p = strcasestr(buf, block_stats_names[j])) == NULL)
		continue;
	    p += strlen(block_stats_names[j]) + 1;
	    if (sscanf(p, "%llu", (unsigned long long *)&stats.values[j]) != 1)
		stats.values[j] = 0;
	    break;
	}

	if (j == BLKIOS - 1) {	/* Total: last one, update incore structures */
	    prepare_block(pmns, path, subsys, name,
			  metric, group, domain, &stats, BLKIOS);
	    /* reset on-stack structure for next outer loop iteration */
	    memset(&stats, 0, sizeof(stats));
	}
    }
    fclose(fp);
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
	return -oserror();

    if ((atoms = malloc(sizeof(pmAtomValue))) == NULL)
	return -oserror();
    if ((atoms[0].cp = strdup(buffer)) == NULL) {
	free(atoms);
	return -oserror();
    }
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
    int i, id;
    size_t size;
    cgroup_values_t *cvp;
    char group[128];

    translate(&group[0], cgroupname, sizeof(group));

    /* allocate space for this group */
    size = (subsys->group_count + 1) * sizeof(cgroup_group_t);
    subsys->groups = (cgroup_group_t *)realloc(subsys->groups, size);
    if (subsys->groups == NULL)
	return -oserror();

    /* allocate space for all values up-front */
    size = subsys->metric_count;
    cvp = (cgroup_values_t *)calloc(size, sizeof(cgroup_values_t));
    if (cvp == NULL)
	return -oserror();

    id = subsys->group_count++;
    memset(&subsys->groups[id], 0, sizeof(cgroup_group_t));
    subsys->groups[id].id = id;
    subsys->groups[id].metric_values = cvp;

    for (i = 0; i < size; i++) {
	cgroup_metrics_t *metrics = &subsys->metrics[i];
	metrics->prepare(pmns, cgrouppath, subsys, group, i, id, domain);
    }
    return 1;
}

char *
cgroup_find_subsys(pmInDom indom, void *data)
{
    static char dunno[] = "?";
    static char opts[256];
    char buffer[256];
    char *s, *out = NULL;
    filesys_t *fs = (filesys_t *)data;

    memset(opts, 0, sizeof(opts));
    strncpy(buffer, fs->options, sizeof(buffer));

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
    if (!isalpha((int)name[0]))
	return 0;
    for (; *name != '\0'; name++)
	if (!isalnum((int)*name) && *name != '_')
	    return 0;
    return 1;
}

static int
cgroup_scan(const char *mnt, const char *path, cgroup_subsys_t *subsys,
		int domain, __pmnsTree *pmns, int root)
{
    int sts, length;
    DIR *dirp;
    struct stat sbuf;
    struct dirent *dp;
    char *cgroupname;
    char cgrouppath[MAXPATHLEN];

    if (root) {
	snprintf(cgrouppath, sizeof(cgrouppath), "%s%s", proc_statspath, mnt);
	length = strlen(cgrouppath);
    } else {
	snprintf(cgrouppath, sizeof(cgrouppath), "%s%s/%s", proc_statspath, mnt, path);
	length = strlen(proc_statspath) + strlen(mnt) + 1;
    }

    if ((dirp = opendir(cgrouppath)) == NULL)
	return -oserror();

    cgroupname = &cgrouppath[length];
    sts = namespace(pmns, subsys, cgrouppath, cgroupname, domain);

    /*
     * readdir - descend into directories to find all cgroups, then
     * populate namespace with <controller>[.<groupname>].<metrics>
     */
    while ((dp = readdir(dirp)) != NULL) {
	int	lsts;
	if (!valid_pmns_name(dp->d_name))
	    continue;
	if (path[0] == '\0')
	    snprintf(cgrouppath, sizeof(cgrouppath), "%s%s/%s",
			proc_statspath, mnt, dp->d_name);
	else
	    snprintf(cgrouppath, sizeof(cgrouppath), "%s%s/%s/%s",
			proc_statspath, mnt, path, dp->d_name);
	cgroupname = &cgrouppath[length];
	if (stat(cgrouppath, &sbuf) < 0)
	    continue;
	if (!(S_ISDIR(sbuf.st_mode)))
	    continue;

	lsts = namespace(pmns, subsys, cgrouppath, cgroupname, domain);
	if (lsts > 0)
	    sts = 1;

	/*
	 * also scan for any child cgroups, but cgroup_scan() may return
	 * an error
	 */
	lsts = cgroup_scan(mnt, cgroupname, subsys, domain, pmns, 0);
	if (lsts > 0)
	    sts = 1;
    }
    closedir(dirp);
    return sts;
}

static void
reset_subsys_stats(cgroup_subsys_t *subsys)
{
    int g, k, a;

    for (g = 0; g < subsys->group_count; g++) {
	cgroup_group_t *group = &subsys->groups[g];
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
    subsys->group_count = 0;
}

int
refresh_cgroups(pmdaExt *pmda, __pmnsTree **pmns)
{
    int i, sts, mtab = 0;
    int domain = pmda->e_domain;
    filesys_t *fs;
    time_t now;
    static time_t before;
    static __pmnsTree *beforetree;
    __pmnsTree *tree = pmns ? *pmns : NULL;
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);
    pmInDom devices = INDOM(DISK_INDOM);

    now = time(NULL);
    if (tree) {
	if (now == before) {
	    *pmns = beforetree;
	    return 0;
	}
    } else if (now == before)
	return 0;

    refresh_cgroup_filesys(mounts);
    refresh_cgroup_devices(devices);

    if (tree)
	__pmFreePMNS(tree);

    if ((sts = __pmNewPMNS(&tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create new pmns: %s\n",
			pmProgname, pmErrStr(sts));
	return 0;
    }

    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	cgroup_subsys_t *subsys = &controllers[i];

	/*
	 * Fetch latest state for subsystem and groups of the given clusters,
	 * by walking the cgroup mounts, finding the mounts of this subsystem
	 * type, and descending into all of the groups (subdirs)
	 */
	reset_subsys_stats(subsys);

	pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
	while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	    if (!pmdaCacheLookup(mounts, sts, NULL, (void **)&fs))
		continue;
	    if (scan_filesys_options(fs->options, subsys->name) == NULL)
		continue;
	    sts = cgroup_scan(fs->path, "", subsys, domain, tree, 1);
	    if (sts > 0)
		mtab = 1;
	}
    }

    if (pmns) {
	*pmns = tree;
	beforetree = tree;
	before = now;
    } else
	__pmFreePMNS(tree);

    return mtab;
}

/*
 * Shared fetch callback for all cgroups metrics
 */
int
cgroup_group_fetch(pmID pmid, unsigned int inst, pmAtomValue *atom)
{
    int i, j, k;
    int gid, cluster, metric;

    gid = cgroup_pmid_group(pmid);
    metric = cgroup_pmid_metric(pmid);
    cluster = proc_pmid_cluster(pmid);

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

		if (cvp->item != metric)
		    continue;
		else if (cvp->atom_count <= 0)
		    return PM_ERR_VALUE;
		else if (inst == PM_IN_NULL)
		    inst = 0;
		else if (inst >= cvp->atom_count)
		    return PM_ERR_INST;
		else if (cvp->atoms[inst].ull == ULLONG_MAX)
		    return PM_ERR_INST;
		*atom = cvp->atoms[inst];
		return 1;
	    }
	}
    }
    return PM_ERR_PMID;
}

/*
 * Needs to answer the question: how much extra space needs to be allocated
 * in the metric table for (dynamic) cgroup metrics"?  We have static entries
 * for group ID zero - if we have any non-zero group IDs, we need entries to
 * cover those.  Return value is the number of additional entries needed.
 */
static void
size_metrictable(int *total, int *trees)
{
    int i, g, maxgroup = 0, nmetrics = 0;

    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	cgroup_subsys_t *subsys = &controllers[i];

	for (g = 0; g < subsys->group_count; g++) {
	    cgroup_group_t *group = &subsys->groups[g];

	    if (group->id > maxgroup)
		maxgroup = group->id;
	}
	nmetrics += subsys->metric_count + 0;	/* +1 for task.pid */
    }

    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "size_metrictable: %d total x %d trees\n",
		nmetrics, maxgroup);

    *total = nmetrics;
    *trees = maxgroup;
}

/*
 * Create new metric table entry for a group based on an existing one.
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int gid)
{
    int domain = pmid_domain(source->m_desc.pmid);
    int cluster = proc_pmid_cluster(source->m_desc.pmid);
    int item = pmid_item(source->m_desc.pmid);

    memcpy(dest, source, sizeof(pmdaMetric));
    dest->m_desc.pmid = cgroup_pmid_build(domain, cluster, gid, item);

    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "refresh_metrictable: (%p -> %p)\n", source, dest);
    if (pmDebug & DBG_TRACE_APPL0)
	fprintf(stderr, "cgroup metric ID dup: %d.[%d.%d].%d - %d.[%d.%d].%d\n",
		domain, cluster,
		cgroup_pmid_group(source->m_desc.pmid),
		cgroup_pmid_metric(source->m_desc.pmid),
		pmid_domain(dest->m_desc.pmid),
		proc_pmid_cluster(dest->m_desc.pmid),
		cgroup_pmid_group(dest->m_desc.pmid),
		cgroup_pmid_metric(dest->m_desc.pmid));
}

static int
cgroup_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    return PM_ERR_TEXT;
}

static void
cgroup_metrics_init(pmdaMetric *metrics, int nmetrics)
{
    int i, j, item, cluster = 0;

    for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	cgroup_subsys_t *subsys = &controllers[i];

	/* set initial default values for controller metrics item field */
	for (j = 0; j < subsys->metric_count; j++)
	    subsys->metrics[j].item = j;

	/* set initial seed values for dynamic PMIDs in global metric table */
	for (j = item = 0; j < nmetrics; j++) {
	    if (pmid_cluster(metrics[j].m_desc.pmid) == subsys->cluster) {
		if (cluster != subsys->cluster) {
		    cluster = subsys->cluster;
		    item = 0;
		}
		metrics[j].m_desc.pmid = PMDA_PMID(cluster, item++);
	    }
	}
    }
}

void
cgroup_init(pmdaMetric *metrics, int nmetrics)
{
    static int set[] = {
	CLUSTER_BLKIO_GROUPS,
	CLUSTER_CPUSET_GROUPS,
	CLUSTER_CPUACCT_GROUPS,
	CLUSTER_CPUSCHED_GROUPS,
	CLUSTER_MEMORY_GROUPS,
	CLUSTER_NET_CLS_GROUPS,
    };

    cgroup_metrics_init(metrics, nmetrics);

    pmdaDynamicPMNS(CGROUP_ROOT,
		    set, sizeof(set) / sizeof(set[0]),
		    refresh_cgroups, cgroup_text,
		    refresh_metrictable, size_metrictable,
		    metrics, nmetrics);
    pmdaDynamicSetClusterMask(CGROUP_ROOT, CGROUP_MASK);
}
