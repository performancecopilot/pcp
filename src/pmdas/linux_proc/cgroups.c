/*
 * Copyright (c) 2012-2015 Red Hat.
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

static void
refresh_cgroup_cpus(void)
{
    pmInDom cpus = INDOM(CPU_INDOM);
    char buf[MAXPATHLEN];
    char *space;
    FILE *fp;

    pmdaCacheOp(cpus, PMDA_CACHE_INACTIVE);
    if ((fp = proc_statsfile("/proc/stat", buf, sizeof(buf))) == NULL)
	return;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "cpu", 3) == 0 && isdigit((int)buf[3])) {
	    if ((space = strchr(buf, ' ')) != NULL) {
	    	*space = '\0';
		pmdaCacheStore(cpus, PMDA_CACHE_ADD, buf, NULL);
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
static void
refresh_cgroup_devices(void)
{
    pmInDom diskindom = INDOM(DISK_INDOM);
    pmInDom devtindom = INDOM(DEVT_INDOM);
    char buf[MAXPATHLEN];
    FILE *fp;

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
	(void)pmdaCacheLookupName(diskindom, namebuf, &dev->inst, NULL);
	(void)pmdaCacheLookup(diskindom, dev->inst, &dev->name, NULL);

	snprintf(buf, sizeof(buf), "%u:%u", major, minor);
	pmdaCacheStore(devtindom, PMDA_CACHE_ADD, buf, (void *)dev);

	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "refresh_devices: \"%s\" \"%d:%d\" inst=%d\n",
			dev->name, dev->major, dev->minor, dev->inst);
    }
    fclose(fp);
}

void
refresh_cgroup_subsys(void)
{
    pmInDom subsys = INDOM(CGROUP_SUBSYS_INDOM);
    char buf[4096];
    FILE *fp;

    pmdaCacheOp(subsys, PMDA_CACHE_INACTIVE);
    if ((fp = proc_statsfile("/proc/cgroups", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	unsigned int hierarchy, num_cgroups, enabled;
	char name[MAXPATHLEN];
	subsys_t *ssp;
	int sts;

	/* skip lines starting with hash (header) */
	if (buf[0] == '#')
	    continue;
	if (sscanf(buf, "%s %u %u %u", &name[0],
			&hierarchy, &num_cgroups, &enabled) < 4)
	    continue;
	sts = pmdaCacheLookupName(subsys, name, NULL, (void **)&ssp);
	if (sts != PMDA_CACHE_INACTIVE) {
	    if ((ssp = (subsys_t *)malloc(sizeof(subsys_t))) == NULL)
		continue;
	}
	ssp->hierarchy = hierarchy;
	ssp->num_cgroups = num_cgroups;
	ssp->enabled = enabled;
	pmdaCacheStore(subsys, PMDA_CACHE_ADD, name, (void *)ssp);

	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "refresh_subsys: \"%s\" h=%u nc=%u on=%u\n",
			name, hierarchy, num_cgroups, enabled);
    }
    fclose(fp);
}

void
refresh_cgroup_filesys(void)
{
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);
    char buf[MAXPATHLEN];
    filesys_t *fs;
    FILE *fp;
    char *path, *device, *type, *options;
    int sts;

    pmdaCacheOp(mounts, PMDA_CACHE_INACTIVE);
    if ((fp = proc_statsfile("/proc/mounts", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	device = strtok(buf, " ");
	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	options = strtok(NULL, " ");
	if (strcmp(type, "cgroup") != 0)
	    continue;

	sts = pmdaCacheLookupName(mounts, path, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(mounts, PMDA_CACHE_ADD, path, fs);
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
	    pmdaCacheStore(mounts, PMDA_CACHE_ADD, path, fs);
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

char *
cgroup_find_subsys(pmInDom indom, filesys_t *fs)
{
    static char dunno[] = "?";
    static char opts[256];
    char buffer[256];
    char *s, *out = NULL;

    memset(opts, 0, sizeof(opts));
    strncpy(buffer, fs->options, sizeof(buffer));
    buffer[sizeof(buffer)-1] = '\0';

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

int
cgroup_mounts_subsys(const char *system, char *buffer, int length)
{
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);
    pmInDom subsys = INDOM(CGROUP_SUBSYS_INDOM);
    filesys_t *fs;
    char *name;
    int sts;

    /* Iterate over cgroup.mounts.subsys indom, comparing the value
     * with the given subsys - if a match is found, return the inst
     * name, else NULL.
     */
    pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	if (!pmdaCacheLookup(mounts, sts, &name, (void **)&fs))
	    continue;
	if (strcmp(system, cgroup_find_subsys(subsys, fs)) != 0)
	    continue;
	snprintf(buffer, length, "%s%s/", proc_statspath, name);
	buffer[length-1] = '\0';
	return strlen(buffer);
    }
    return 0;
}

static const char *
cgroup_name(const char *path, int offset)
{
    char *name = (char *)path + offset;

    if (*name == '/') {
	while (*name == '/')
	    name++;
	name--;
    } else if (*name == '\0') {
	name = "/";
    }
    return name;
}

static int
check_refresh(const char *cgroup, const char *container, int container_length)
{
    /*
     * See whether a refresh needed - if no container, always needed,
     * otherwise we only refresh for a matching container name/path.
     */
    if (container_length > 0) {
	while (*cgroup == '/')
	    cgroup++;	/* do not compare any leading slashes */
	return (strncmp(cgroup, container, container_length) == 0);
    }
    return 1;
}

static void
cgroup_scan(const char *mnt, const char *path, cgroup_refresh_t refresh,
		const char *container, int container_length)
{
    int length, mntlen = strlen(mnt) + 1;
    DIR *dirp;
    struct stat sbuf;
    struct dirent *dp;
    const char *cgname;
    char cgpath[MAXPATHLEN] = { 0 };

    if (path[0] == '\0') {
	snprintf(cgpath, sizeof(cgpath), "%s%s", proc_statspath, mnt);
	length = strlen(cgpath);
    } else {
	snprintf(cgpath, sizeof(cgpath), "%s%s/%s", proc_statspath, mnt, path);
	length = strlen(proc_statspath) + mntlen;
    }

    if ((dirp = opendir(cgpath)) == NULL)
	return;

    cgname = cgroup_name(cgpath, length);
    if (check_refresh(cgpath + mntlen, container, container_length))
	refresh(cgpath, cgname);

    /* descend into subdirectories to find all cgroups */
    while ((dp = readdir(dirp)) != NULL) {
	if (dp->d_name[0] == '.')
	    continue;
	if (path[0] == '\0')
	    snprintf(cgpath, sizeof(cgpath), "%s%s/%s",
			proc_statspath, mnt, dp->d_name);
	else
	    snprintf(cgpath, sizeof(cgpath), "%s%s/%s/%s",
			proc_statspath, mnt, path, dp->d_name);
	if (stat(cgpath, &sbuf) < 0)
	    continue;
	if (!(S_ISDIR(sbuf.st_mode)))
	    continue;

	cgname = cgroup_name(cgpath, length);
	if (check_refresh(cgpath + mntlen, container, container_length))
	    refresh(cgpath, cgname);
	cgroup_scan(mnt, cgname, refresh, container, container_length);
    }
    closedir(dirp);
}

/*
 * Primary driver interface - finds any/all mount points for a given
 * cgroup subsystem and iteratively expands all of the cgroups below
 * them.  The setup callback inactivates each indoms contents, while
 * the refresh callback is called once per cgroup (with path/name) -
 * its role is to refresh the values for that one named cgroup.
 */
void
refresh_cgroups(const char *subsys, const char *container,
	int length, cgroup_setup_t setup, cgroup_refresh_t refresh)
{
    int sts;
    filesys_t *fs;
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);

    pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	if (!pmdaCacheLookup(mounts, sts, NULL, (void **)&fs))
	    continue;
	if (scan_filesys_options(fs->options, subsys) == NULL)
	    continue;
	setup();
	cgroup_scan(fs->path, "", refresh, container, length);
    }
}

static int
read_oneline_string(const char *file)
{
    char buffer[4096], *result;
    size_t length;
    FILE *fp;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;
    result = fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
    if (!result)
	return -ENOMEM;
    length = strlen(result);
    while (result[--length] == '\n')
	result[length] = '\0';
    return proc_strings_insert(result);
}

static __uint64_t
read_oneline_ull(const char *file)
{
    char buffer[4096], *result, *endp;
    FILE *fp;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;
    result = fgets(buffer, sizeof(buffer), fp);
    fclose(fp);
    if (!result)
	return -ENOMEM;
    return strtoull(result, &endp, 0);
}

void
setup_cpuset(void)
{
    pmdaCacheOp(INDOM(CGROUP_CPUSET_INDOM), PMDA_CACHE_INACTIVE);
}

void
refresh_cpuset(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_CPUSET_INDOM);
    cgroup_cpuset_t *cpuset;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cpuset);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	cpuset = (cgroup_cpuset_t *)malloc(sizeof(cgroup_cpuset_t));
	if (!cpuset)
	    return;
    }
    snprintf(file, sizeof(file), "%s/cpuset.cpus", path);
    cpuset->cpus = read_oneline_string(file);
    snprintf(file, sizeof(file), "%s/cpuset.mems", path);
    cpuset->mems = read_oneline_string(file);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, cpuset);
}

void
setup_cpuacct(void)
{
    refresh_cgroup_cpus();
    pmdaCacheOp(INDOM(CGROUP_CPUACCT_INDOM), PMDA_CACHE_INACTIVE);
    pmdaCacheOp(INDOM(CGROUP_PERCPUACCT_INDOM), PMDA_CACHE_INACTIVE);
}

static int
read_cpuacct_stats(const char *file, cgroup_cpuacct_t *cap)
{
    static cgroup_cpuacct_t cpuacct;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } cpuacct_fields[] = {
	{ "user",			&cpuacct.user },
	{ "system",			&cpuacct.system },
	{ NULL, NULL }
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	if (sscanf(buffer, "%s %llu\n", &name[0], &value) < 2)
	    continue;
	for (i = 0; cpuacct_fields[i].field != NULL; i++) {
	    if (strcmp(name, cpuacct_fields[i].field) != 0)
		continue;
	    *cpuacct_fields[i].offset = value;
	    break;
	}
    }
    fclose(fp);
    memcpy(cap, &cpuacct, sizeof(cpuacct));
    return 0;
}

static int
read_percpuacct_usage(const char *file, const char *name)
{
    pmInDom indom =  INDOM(CGROUP_PERCPUACCT_INDOM);
    cgroup_percpuacct_t *percpuacct;
    char buffer[16 * 4096], *endp;
    char inst[MAXPATHLEN], *p;
    unsigned long long value;
    FILE *fp;
    int cpu, sts;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;
    p = fgets(buffer, sizeof(buffer), fp);
    if (!p) {
	fclose(fp);
	return -ENOMEM;
    }

    for (cpu = 0; ; cpu++) {
	value = strtoull(p, &endp, 0);
	if (*endp == '\0' || endp == p)
	    break;
	p = endp;
	while (p && isspace((int)*p))
	    p++;
	snprintf(inst, sizeof(inst), "%s::cpu%d", name, cpu);
	sts = pmdaCacheLookupName(indom, inst, NULL, (void **)&percpuacct);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	if (sts != PMDA_CACHE_INACTIVE) {
	    percpuacct = (cgroup_percpuacct_t *)malloc(sizeof(cgroup_percpuacct_t));
	    if (!percpuacct)
		continue;
	}
	percpuacct->usage = value;
	pmdaCacheStore(indom, PMDA_CACHE_ADD, inst, percpuacct);
    }
    fclose(fp);
    return 0;
}

void
refresh_cpuacct(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_CPUACCT_INDOM);
    cgroup_cpuacct_t *cpuacct;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cpuacct);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	cpuacct = (cgroup_cpuacct_t *)malloc(sizeof(cgroup_cpuacct_t));
	if (!cpuacct)
	    return;
    }
    snprintf(file, sizeof(file), "%s/cpuacct.stat", path);
    read_cpuacct_stats(file, cpuacct);
    snprintf(file, sizeof(file), "%s/cpuacct.usage", path);
    cpuacct->usage = read_oneline_ull(file);
    snprintf(file, sizeof(file), "%s/cpuacct.usage_percpu", path);
    read_percpuacct_usage(file, name);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, cpuacct);
}

void
setup_cpusched(void)
{
    pmdaCacheOp(INDOM(CGROUP_CPUSCHED_INDOM), PMDA_CACHE_INACTIVE);
}

static int
read_cpu_stats(const char *file, cgroup_cpustat_t *ccp)
{
    static cgroup_cpustat_t cpustat;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } cpustat_fields[] = {
	{ "nr_periods",			&cpustat.nr_periods },
	{ "nr_throttled",		&cpustat.nr_throttled },
	{ "throttled_time",		&cpustat.throttled_time },
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    memset(&cpustat, 0, sizeof(cpustat));
    if ((fp = fopen(file, "r")) == NULL) {
	memcpy(ccp, &cpustat, sizeof(cpustat));
	return -ENOENT;
    }
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	if (sscanf(buffer, "%s %llu\n", &name[0], &value) < 2)
	    continue;
	for (i = 0; cpustat_fields[i].field != NULL; i++) {
	    if (strcmp(name, cpustat_fields[i].field) != 0)
		continue;
	    *cpustat_fields[i].offset = value;
	    break;
	}
    }
    fclose(fp);
    memcpy(ccp, &cpustat, sizeof(cpustat));
    return 0;
}

void
refresh_cpusched(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_CPUSCHED_INDOM);
    cgroup_cpusched_t *cpusched;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cpusched);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	cpusched = (cgroup_cpusched_t *)malloc(sizeof(cgroup_cpusched_t));
	if (!cpusched)
	    return;
    }
    snprintf(file, sizeof(file), "%s/cpu.stat", path);
    read_cpu_stats(file, &cpusched->stat);
    snprintf(file, sizeof(file), "%s/cpu.shares", path);
    cpusched->shares = read_oneline_ull(file);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, cpusched);
}

void
setup_memory(void)
{
    pmdaCacheOp(INDOM(CGROUP_MEMORY_INDOM), PMDA_CACHE_INACTIVE);
}

static int
read_memory_stats(const char *file, cgroup_memory_t *cmp)
{
    static cgroup_memory_t memory;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } memory_fields[] = {
	{ "cache",			&memory.stat.cache },
	{ "rss",			&memory.stat.rss },
	{ "rss_huge",			&memory.stat.rss_huge },
	{ "mapped_file",		&memory.stat.mapped_file },
	{ "writeback",			&memory.stat.writeback },
	{ "swap",			&memory.stat.swap },
	{ "pgpgin",			&memory.stat.pgpgin },
	{ "pgpgout",			&memory.stat.pgpgout },
	{ "pgfault",			&memory.stat.pgfault },
	{ "pgmajfault",			&memory.stat.pgmajfault },
	{ "inactive_anon",		&memory.stat.inactive_anon },
	{ "active_anon",		&memory.stat.active_anon },
	{ "inactive_file",		&memory.stat.inactive_file },
	{ "active_file",		&memory.stat.active_file },
	{ "unevictable",		&memory.stat.unevictable },
	{ "total_cache",		&memory.total.cache },
	{ "total_rss",			&memory.total.rss },
	{ "total_rss_huge",		&memory.total.rss_huge },
	{ "total_mapped_file",		&memory.total.mapped_file },
	{ "total_writeback",		&memory.total.writeback },
	{ "total_swap",			&memory.total.swap },
	{ "total_pgpgin",		&memory.total.pgpgin },
	{ "total_pgpgout",		&memory.total.pgpgout },
	{ "total_pgfault",		&memory.total.pgfault },
	{ "total_pgmajfault",		&memory.total.pgmajfault },
	{ "total_inactive_anon",	&memory.total.inactive_anon },
	{ "total_active_anon",		&memory.total.active_anon },
	{ "total_inactive_file",	&memory.total.inactive_file },
	{ "total_active_file",		&memory.total.active_file },
	{ "total_unevictable",		&memory.total.unevictable },
	{ "recent_rotated_anon",	&memory.recent_rotated_anon },
	{ "recent_rotated_file",	&memory.recent_rotated_file },
	{ "recent_scanned_anon",	&memory.recent_scanned_anon },
	{ "recent_scanned_file",	&memory.recent_scanned_file },
	{ NULL, NULL }
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    memset(&memory, 0, sizeof(memory));
    if ((fp = fopen(file, "r")) == NULL) {
	memcpy(cmp, &memory, sizeof(memory));
	return -ENOENT;
    }
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	if (sscanf(buffer, "%s %llu\n", &name[0], &value) < 2)
	    continue;
	for (i = 0; memory_fields[i].field != NULL; i++) {
	    if (strcmp(name, memory_fields[i].field) != 0)
		continue;
	    *memory_fields[i].offset = value;
	    break;
	}
    }
    fclose(fp);
    memcpy(cmp, &memory, sizeof(memory));
    return 0;
}

void
refresh_memory(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_MEMORY_INDOM);
    cgroup_memory_t *memory;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&memory);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	memory = (cgroup_memory_t *)malloc(sizeof(cgroup_memory_t));
	if (!memory)
	    return;
    }
    snprintf(file, sizeof(file), "%s/memory.stat", path);
    read_memory_stats(file, memory);
    snprintf(file, sizeof(file), "%s/memory.limit_in_bytes", path);
    memory->limit = read_oneline_ull(file);
    snprintf(file, sizeof(file), "%s/memory.usage_in_bytes", path);
    memory->usage = read_oneline_ull(file);
    snprintf(file, sizeof(file), "%s/memory.failcnt", path);
    memory->failcnt = read_oneline_ull(file);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, memory);
}

void
setup_netcls(void)
{
    pmdaCacheOp(INDOM(CGROUP_NETCLS_INDOM), PMDA_CACHE_INACTIVE);
}

void
refresh_netcls(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_NETCLS_INDOM);
    cgroup_netcls_t *netcls;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&netcls);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	netcls = (cgroup_netcls_t *)malloc(sizeof(cgroup_netcls_t));
	if (!netcls)
	    return;
    }
    snprintf(file, sizeof(file), "%s/net_cls.classid", path);
    netcls->classid = read_oneline_ull(file);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, netcls);
}

void
setup_blkio(void)
{
    refresh_cgroup_devices();
    pmdaCacheOp(INDOM(CGROUP_BLKIO_INDOM), PMDA_CACHE_INACTIVE);
    pmdaCacheOp(INDOM(CGROUP_PERDEVBLKIO_INDOM), PMDA_CACHE_INACTIVE);
}

static cgroup_blkiops_t *
get_blkiops(int style, cgroup_perdevblkio_t *perdev)
{
    switch (style) {
    case CG_BLKIO_IOMERGED_TOTAL:
	return &perdev->stats.io_merged;
    case CG_BLKIO_IOQUEUED_TOTAL:
	return &perdev->stats.io_queued;
    case CG_BLKIO_IOSERVICEBYTES_TOTAL:
	return &perdev->stats.io_service_bytes;
    case CG_BLKIO_IOSERVICED_TOTAL:
	return &perdev->stats.io_serviced;
    case CG_BLKIO_IOSERVICETIME_TOTAL:
	return &perdev->stats.io_service_time;
    case CG_BLKIO_IOWAITTIME_TOTAL:
	return &perdev->stats.io_wait_time;
    }
    return NULL;
}

static char *
get_blkdev(pmInDom devtindom, unsigned int major, unsigned int minor)
{
    char	buf[64];
    device_t	*dev;
    int		sts;

    snprintf(buf, sizeof(buf), "%u:%u", major, minor);
    sts = pmdaCacheLookupName(devtindom, buf, NULL, (void **)&dev);
    if (sts == PMDA_CACHE_ACTIVE)
	return dev->name;
    return NULL;
}

static cgroup_perdevblkio_t *
get_perdevblkio(pmInDom indom, const char *name, const char *disk,
		char *inst, size_t size)
{
    cgroup_perdevblkio_t *cdevp;
    int		sts;

    snprintf(inst, size, "%s::%s", name, disk);
    sts = pmdaCacheLookupName(indom, inst, NULL, (void **)&cdevp);
    if (sts == PMDA_CACHE_ACTIVE) {
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "get_perdevblkio active %s\n", inst);
	return cdevp;
    }
    if (sts != PMDA_CACHE_INACTIVE) {
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "get_perdevblkio new %s\n", inst);
	cdevp = (cgroup_perdevblkio_t *)malloc(sizeof(cgroup_perdevblkio_t));
	if (!cdevp)
	    return NULL;
    } else {
	if (pmDebug & DBG_TRACE_APPL0)
	    fprintf(stderr, "get_perdevblkio inactive %s\n", inst);
    }
    memset(cdevp, 0, sizeof(cgroup_perdevblkio_t));
    return cdevp;
}

static int
read_blkio_devices_stats(const char *file, const char *name, int style,
			cgroup_blkiops_t *total)
{
    pmInDom indom = INDOM(CGROUP_PERDEVBLKIO_INDOM);
    pmInDom devtindom = INDOM(DEVT_INDOM);
    cgroup_perdevblkio_t *blkdev;
    cgroup_blkiops_t *blkios;
    char *devname = NULL;
    char buffer[4096];
    FILE *fp;

    static cgroup_blkiops_t blkiops;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } blkio_fields[] = {
	{ "Read",			&blkiops.read },
	{ "Write",			&blkiops.write },
	{ "Sync",			&blkiops.sync },
	{ "Async",			&blkiops.async },
	{ "Total",			&blkiops.total },
	{ NULL, NULL },
    };

    /* reset, so counts accumulate from zero for this set of devices */
    memset(total, 0, sizeof(cgroup_blkiops_t));

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	unsigned int major, minor;
	unsigned long long value;
	char *realname, op[8];
	int i;

	i = sscanf(buffer, "Total %llu\n", &value);
	if (i == 2) {	/* final field - per-cgroup Total operations */
	    break;
	}

	i = sscanf(buffer, "%u:%u %s %llu\n", &major, &minor, &op[0], &value);
	if (i < 3)
	    continue;
	realname = get_blkdev(devtindom, major, minor);
	if (!realname)
	    continue;
	if (!devname || strcmp(devname, realname) != 0) /* lines for next device */
	    memset(&blkiops, 0, sizeof(cgroup_blkiops_t));
	devname = realname;
	for (i = 0; blkio_fields[i].field != NULL; i++) {
	    if (strcmp(name, blkio_fields[i].field) != 0)
		continue;
	    *blkio_fields[i].offset = value;
	    if (strcmp("Total", blkio_fields[i].field) != 0)
		break;
	    /* all device fields are now acquired, update indom and cgroup totals */
	    blkdev = get_perdevblkio(indom, name, devname, buffer, sizeof(buffer));
	    blkios = get_blkiops(style, blkdev);
	    memcpy(blkios, &blkiops, sizeof(cgroup_blkiops_t));
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, blkdev);
	    /* accumulate stats for this latest device into the per-cgroup totals */
	    total->read += blkiops.read;
	    total->write += blkiops.write;
	    total->sync += blkiops.sync;
	    total->async += blkiops.async;
	    total->total += blkiops.total;
	    break;
	}
    }
    fclose(fp);
    return 0;
}

static int
read_blkio_devices_value(const char *file, const char *name, int style,
			__uint64_t *total)
{
    pmInDom indom = INDOM(CGROUP_PERDEVBLKIO_INDOM);
    pmInDom devtindom = INDOM(DEVT_INDOM);
    cgroup_perdevblkio_t *blkdev;
    char buffer[4096];
    FILE *fp;

    /* reset, so counts accumulate from zero for this set of devices */
    memset(total, 0, sizeof(__uint64_t));

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	unsigned int major, minor;
	unsigned long long value;
	char *devname;
	int i;

	i = sscanf(buffer, "%u:%u %llu\n", &major, &minor, &value);
	if (i < 3)
	    continue;
	if ((devname = get_blkdev(devtindom, major, minor)) == NULL)
	    continue;
	/* all device fields are now acquired, update indom and cgroup total */
	blkdev = get_perdevblkio(indom, name, devname, buffer, sizeof(buffer));
	if (style == CG_BLKIO_SECTORS)
	    blkdev->stats.sectors = value;
	if (style == CG_BLKIO_TIME)
	    blkdev->stats.time = value;
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, blkdev);
	/* accumulate stats for this latest device into the per-cgroup total */
	*total += value;
    }
    fclose(fp);
    return 0;
}

void
refresh_blkio(const char *path, const char *name)
{
    pmInDom indom = INDOM(CGROUP_BLKIO_INDOM);
    cgroup_blkio_t *blkio;
    char file[MAXPATHLEN];
    int sts;

    sts = pmdaCacheLookupName(indom, name, NULL, (void **)&blkio);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	blkio = (cgroup_blkio_t *)malloc(sizeof(cgroup_blkio_t));
	if (!blkio)
	    return;
    }
    snprintf(file, sizeof(file), "%s/blkio.io_merged", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOMERGED_TOTAL, &blkio->total.io_merged);
    snprintf(file, sizeof(file), "%s/blkio.io_queued", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOQUEUED_TOTAL, &blkio->total.io_queued);
    snprintf(file, sizeof(file), "%s/blkio.io_service_bytes", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICEBYTES_TOTAL, &blkio->total.io_service_bytes);
    snprintf(file, sizeof(file), "%s/blkio.io_serviced", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICED_TOTAL, &blkio->total.io_serviced);
    snprintf(file, sizeof(file), "%s/blkio.io_service_time", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICETIME_TOTAL, &blkio->total.io_service_time);
    snprintf(file, sizeof(file), "%s/blkio.io_wait_time", path);
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOWAITTIME_TOTAL, &blkio->total.io_wait_time);
    snprintf(file, sizeof(file), "%s/blkio.sectors", path);
    read_blkio_devices_value(file, name,
		CG_BLKIO_SECTORS, &blkio->total.sectors);
    snprintf(file, sizeof(file), "%s/blkio.time", path);
    read_blkio_devices_value(file, name,
		CG_BLKIO_TIME, &blkio->total.time);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, name, blkio);
}
