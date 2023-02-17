/*
 * Copyright (c) 2012-2019,2022 Red Hat.
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
#include "libpcp.h"
#include "pmda.h"
#include "indom.h"
#include "cgroups.h"
#include "clusters.h"
#include "proc_pid.h"
#include <sys/stat.h>
#include <ctype.h>

unsigned int	cgroup_version;

/*
 * Parts of the following two functions are based on systemd code, see
 * https://github.com/systemd/systemd/blob/main/src/basic/unit-name.c
 *
 * The license in that code is: SPDX-License-Identifier: LGPL-2.1+
 */
static int
unhexchar(char c) {

    if (c >= '0' && c <= '9')
	return c - '0';

    if (c >= 'a' && c <= 'f')
	return c - 'a' + 10;

    if (c >= 'A' && c <= 'F')
	return c - 'A' + 10;

    return -EINVAL;
}

/*
 * Un-escape \\xFF hex escape codes used by systemd in some of the cgroup
 * instance names. This is necessary because golang http response parsers
 * don't tolerate the escapes when used in a streamed chunked encoding
 * sequence. We don't generally want escape sequences in PCP instance
 * names anyway. Return fsname if there are no changes.
 */
static char *
unit_name_unescape(const char *fsname, char escname[])
{
    const char *f;
    char *t;

    if (fsname == NULL || strchr(fsname, '\\') == NULL)
	/* normal, fast path - no change */
    	return (char *)fsname;

    for (t = escname, f = fsname; *f; f++) {
	/* nb: t lags f */
	if (f[0] == '\\' && f[1] == 'x') {
	    int a = unhexchar(f[2]);
	    int b = unhexchar(f[3]);

	    *(t++) = (char) (((uint8_t) a << 4U) | (uint8_t) b);
	    f += 3;
	} else
	    *(t++) = *f;
    }
    *t = '\0';

    if (pmDebugOptions.appl0)
    	fprintf(stderr, "%s: mapped fsname <%s> to escname <%s>\n",
			"unit_name_unescape", fsname, escname);

    return escname;
}

static void
refresh_cgroup_cpu_map(void)
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
refresh_cgroup_device_map(void)
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
	    if ((dev = (device_t *)calloc(1, sizeof(device_t))) == NULL) {
		pmNoMem("device", sizeof(device_t), PM_RECOV_ERR);
		continue;
	    }
	    dev->major = major;
	    dev->minor = minor;
	}
	/* keeping track of all fields (major/minor/inst/name) */
	pmdaCacheStore(diskindom, PMDA_CACHE_ADD, namebuf, dev);
	(void)pmdaCacheLookupName(diskindom, namebuf, &dev->inst, NULL);
	(void)pmdaCacheLookup(diskindom, dev->inst, &dev->name, NULL);

	pmsprintf(buf, sizeof(buf), "%u:%u", major, minor);
	pmdaCacheStore(devtindom, PMDA_CACHE_ADD, buf, (void *)dev);

	if (pmDebugOptions.appl0)
	    fprintf(stderr, "refresh_cgroup_devices: \"%s\" \"%d:%d\" inst=%d\n",
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
	    if ((ssp = (subsys_t *)calloc(1, sizeof(subsys_t))) == NULL)
		continue;
	}
	ssp->hierarchy = hierarchy;
	ssp->num_cgroups = num_cgroups;
	ssp->enabled = enabled;
	pmdaCacheStore(subsys, PMDA_CACHE_ADD, name, (void *)ssp);

	if (pmDebugOptions.appl0)
	    fprintf(stderr, "refresh_cgroup_subsys: \"%s\" h=%u nc=%u on=%u\n",
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
    int sts, version, version1 = 0, version2 = 0;

    pmdaCacheOp(mounts, PMDA_CACHE_INACTIVE);
    if ((fp = proc_statsfile("/proc/mounts", buf, sizeof(buf))) == NULL)
	return;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	device = strtok(buf, " ");
	path = strtok(NULL, " ");
	type = strtok(NULL, " ");
	options = strtok(NULL, " ");
	if (strcmp(type, "cgroup2") == 0) {
	    version2++;
	    version = 2;
	} else if (strcmp(type, "cgroup") == 0) {
	    version1++;
	    version = 1;
	} else {
	    continue;
	}

	sts = pmdaCacheLookupName(mounts, path, NULL, (void **)&fs);
	if (sts == PMDA_CACHE_ACTIVE)	/* repeated line in /proc/mounts? */
	    continue;
	if (sts == PMDA_CACHE_INACTIVE) { /* re-activate an old mount */
	    pmdaCacheStore(mounts, PMDA_CACHE_ADD, path, fs);
	    if (strcmp(path, fs->path) != 0) {	/* old device, new path */
		free(fs->path);
		fs->path = strdup(path);
	    }
	    if (version == 1 &&
		strcmp(options, fs->options) != 0) {	/* old device, new opts */
		free(fs->options);
		fs->options = strdup(options);
	    }
	    fs->version = version;
	}
	else {	/* new mount */
	    if ((fs = calloc(1, sizeof(filesys_t))) == NULL)
		continue;
	    fs->path = strdup(path);
	    if (version == 1)
		fs->options = strdup(options);
	    fs->version = version;
	    if (pmDebugOptions.appl0)
		fprintf(stderr, "refresh_cgroup_filesys: add \"%s\" \"%s\"\n",
			fs->path, device);
	    pmdaCacheStore(mounts, PMDA_CACHE_ADD, path, fs);
	}
    }
    fclose(fp);

    /* Detect whether running in solely v2, unified, or v1-only mode
     * (treating unified as essentially the same as v1)
     */
    cgroup_version = (version2 && !version1) ? 2 : 1;
}

static char *
cgroup2_mount_point(void)
{
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);
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
	if (fs->version > 1)
	    return name;
    }
    return NULL;
}

static void
cgroup1_mount_subsys(char *buffer, int length, const char *system, const char *suffix)
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
	pmsprintf(buffer, length, "%s%s/%s", proc_statspath, name, suffix);
	break;
    }
}

/*
 * Find a suitable sysfs search point to look for a given cgroup name,
 * based on the detected kernel interface version for cgroups.
 *
 * cgroups v2 kernel interface:
 * Life is easier - just find the cgroup2 filesystem mount point, as long
 * as we're not in 'unified' mode (in which case use v1 interface below).
 *
 * cgroups v1 kernel interface:
 * We must find this systems mount path for a cgroup subsystem that will
 * be in use for all containers - choose memory for this purpose, as its
 * pervasive and all container engines use it.
 */
char *
cgroup_container_path(char *buffer, size_t buflen, const char *container)
{
    if (!cgroup_version)
	refresh_cgroup_filesys();

    if (cgroup_version > 1)
	pmsprintf(buffer, buflen, "%s%s/%s/%s", proc_statspath,
			cgroup2_mount_point(), "machine.slice", container);
    else if (cgroup_version == 1)
	cgroup1_mount_subsys(buffer, buflen, "memory", container);
    return buffer;
}

static char *
scan_filesys_options(const char *options, const char *option)
{
    static char buffer[MAXMNTOPTSLEN];
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

    if (fs->options == NULL)
	return dunno;

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

/*
 * From a cgroup name attempt to extract a container ID
 *    /machine.slice/libpod-06e531[...]85c.scope (e.g. RHEL/Fedora)
 *    /system.slice/docker-0ea0c4[...]3bc0.scope (e.g. RHEL/Fedora)
 *    /docker/0ea0c4[...]3bc0  (e.g. Debian/Ubuntu/SUSE)
 * However, we need to be wary of strings like "docker-containerd".
 * Also, docker may be started with the --parent-cgroup= argument,
 * further complicating our life - /system.slice is just a default.
 */
char *
cgroup_container_search(const char *cgroup, char *cid, int cidlen)
{
    const char *endp = strchr(cgroup, '\n');
    const char *p;
    char *end;
    int len;

    if (endp == NULL)
	endp = cgroup + strlen(cgroup) + 1;
    while (*(endp-1) == '\n')
	endp--;
    for (p = endp; p != cgroup; p--)
	if (*p == '/') break;
    if (p == cgroup)
	return NULL;

    if (strncmp(p, "/libpod-", 8) == 0 || strncmp(p, "/docker-", 8) == 0) {
	p += 8;
	if ((end = strchr(p, '.')) != NULL &&
	    ((len = end - p) < cidlen) && len == SHA256CIDLEN) {
	    strncpy(cid, p, len);
	    cid[len] = '\0';
	    return cid;
	}
    } else if ((len = (endp - p) - 2) == SHA256CIDLEN) {
	strncpy(cid, p + 1, len);
	cid[len] = '\0';
	return cid;
    }
    return NULL;
}

static void
cgroup_container(const char *cgroup, char *buf, int buflen, int *key)
{
    char	*cid;

    if ((cid = cgroup_container_search(cgroup, buf, buflen)) == NULL)
	*key = -1;
    else
	*key = proc_strings_insert(cid);
}

static char *
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

static const char *
cgroup_basename(const char *cgroup)
{
    const char *p, *base = cgroup;

    for (p = base; *p; p++)
	if (*p == '/')
	    base = p + 1;
    return base;
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
	if (strncmp(cgroup, container, container_length) == 0)
	    return 1;
	if (strncmp(cgroup_basename(cgroup), container, container_length) == 0)
	    return 1;
	return 0;
    }
    return 1;
}

static void
cgroup_scan(const char *mnt, const char *path, cgroup_refresh_t refresh,
		const char *container, int container_length, void *arg)
{
    int length, mntlen = strlen(mnt) + 1;
    DIR *dirp;
    struct dirent *dp;
    char *cgname;
    char cgpath[MAXPATHLEN] = { 0 };

    if (path[0] == '\0') {
	pmsprintf(cgpath, sizeof(cgpath), "%s%s", proc_statspath, mnt);
	length = strlen(cgpath);
    } else {
	pmsprintf(cgpath, sizeof(cgpath), "%s%s/%s", proc_statspath, mnt, path);
	length = strlen(proc_statspath) + mntlen;
    }

    if ((dirp = opendir(cgpath)) == NULL)
	return;

    cgname = cgroup_name(cgpath, length);
    if (check_refresh(cgpath + mntlen, container, container_length))
	refresh(cgpath, cgname, arg);

    /* descend into subdirectories to find all cgroups */
    while ((dp = readdir(dirp)) != NULL) {
	if (dp->d_name[0] == '.' || dp->d_type == DT_REG)
	    continue;
	if (path[0] == '\0')
	    pmsprintf(cgpath, sizeof(cgpath), "%s%s/%s",
			proc_statspath, mnt, dp->d_name);
	else
	    pmsprintf(cgpath, sizeof(cgpath), "%s%s/%s/%s",
			proc_statspath, mnt, path, dp->d_name);
	if (dp->d_type == DT_UNKNOWN) {
	    /*
	     * This a bit sad, and probably only seen in QA where
	     * PROC_STATSPATH is set and the test "/proc" files are
	     * on a file system that does not support d_type from
	     * readdir() ... go the old-style way with stat()
	     */
	    int		lsts;
	    struct stat	statbuf;
	    if ((lsts = stat(cgpath, &statbuf)) != 0) {
		if (pmDebugOptions.appl0)
		    fprintf(stderr, "cgroup_scan: stat(%s) -> %d\n", cgpath, lsts);
		continue;
	    }
	    if ((statbuf.st_mode & S_IFMT) != S_IFDIR)
		continue;
	}
	else if (dp->d_type != DT_DIR)
	    continue;

	cgname = cgroup_name(cgpath, length);
	if (check_refresh(cgpath + mntlen, container, container_length))
	    refresh(cgpath, cgname, arg);
	cgroup_scan(mnt, cgname, refresh, container, container_length, arg);
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
static void
refresh_cgroups(const char *subsys, const char *container, size_t length,
		cgroup_setup_t setup, cgroup_refresh_t refresh, void *arg)
{
    int sts;
    filesys_t *fs;
    pmInDom mounts = INDOM(CGROUP_MOUNTS_INDOM);

    pmdaCacheOp(mounts, PMDA_CACHE_WALK_REWIND);
    while ((sts = pmdaCacheOp(mounts, PMDA_CACHE_WALK_NEXT)) != -1) {
	if (!pmdaCacheLookup(mounts, sts, NULL, (void **)&fs))
	    continue;

	/* for v1 cgroups, verify this subsystem is in-use */
	if (fs->version == 1 &&
	    scan_filesys_options(fs->options, subsys) == NULL)
	    continue;

	setup(arg);
	cgroup_scan(fs->path, "", refresh, container, length, arg);
    }
}

static void
read_pressure(FILE *fp, const char *type, cgroup_pressure_t *pp)
{
    static char fmt[] = "TYPE avg10=%f avg60=%f avg300=%f total=%llu\n";
    int		count;

#ifdef __GNUC__
#if __GNUC__ >= 10
    /*
     * gcc 10 on Fedora 32 and Debian unstable falsely report a problem
     * with this strncpy() ... it is safe
     */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif
#endif
    strncpy(fmt, type, 4);
#ifdef __GNUC__
#if __GNUC__ >= 10
#pragma GCC diagnostic pop
#endif
#endif
    count = fscanf(fp, fmt, &pp->avg10sec, &pp->avg1min, &pp->avg5min,
		    (unsigned long long *)&pp->total);
    pp->updated = (count == 4);
}

static int
read_pressures(const char *file, cgroup_pressures_t *pp, int flags)
{
    FILE	*fp;

    if (flags & CG_PSI_SOME)
	memset(&pp->some, 0, sizeof(cgroup_pressure_t));
    if (flags & CG_PSI_FULL)
	memset(&pp->full, 0, sizeof(cgroup_pressure_t));

    if ((fp = fopen(file, "r")) == NULL)
	return -oserror();

    if (flags & CG_PSI_SOME)
	read_pressure(fp, "some", &pp->some);
    if (flags & CG_PSI_FULL)
	read_pressure(fp, "full", &pp->full);

    fclose(fp);
    return 0;
}

static int
read_oneline(const char *file, char *buffer, size_t length)
{
    FILE *fp;
    int sts;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;
    if (fgets(buffer, length, fp) != NULL) {
	buffer[length-1] = '\0';
	sts = 0;
    } else {
	sts = -ENOMEM;
    }
    fclose(fp);
    return sts;
}

static int
read_oneline_string(const char *file)
{
    char buffer[4096];
    size_t length;
    int sts;

    if ((sts = read_oneline(file, buffer, sizeof(buffer))) < 0)
	return sts;
    length = strlen(buffer);
    while (buffer[--length] == '\n')
	buffer[length] = '\0';
    return proc_strings_insert(buffer);
}

static int
read_oneline_ull(const char *file, __uint64_t *value)
{
    char buffer[4096], *endp;
    int sts = read_oneline(file, buffer, sizeof(buffer));
    *value = sts < 0 ? ULONGLONG_MAX : strtoull(buffer, &endp, 0);
    return sts;
}

static int
read_oneline_ll(const char *file, __int64_t *value)
{
    char buffer[4096], *endp;
    int sts = read_oneline(file, buffer, sizeof(buffer));
    *value = sts < 0 ? sts : strtoll(buffer, &endp, 0);
    return sts;
}

static void
setup_cpuset(void *arg)
{
    (void)arg;
    pmdaCacheOp(INDOM(CGROUP_CPUSET_INDOM), PMDA_CACHE_INACTIVE);
}

static void
refresh_cpuset(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_CPUSET_INDOM);
    cgroup_cpuset_t *cpuset;
    char *escname, escbuf[MAXPATHLEN];
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    int sts;

    (void)arg;
    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&cpuset);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE &&
	(cpuset = (cgroup_cpuset_t *)calloc(1, sizeof(cgroup_cpuset_t))) == NULL)
	return;

    pmsprintf(file, sizeof(file), "%s/%s", path, "cpuset.cpus");
    cpuset->cpus = read_oneline_string(file);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpuset.mems");
    cpuset->mems = read_oneline_string(file);
    cgroup_container(name, id, sizeof(id), &cpuset->container);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, cpuset);
}

static void
setup_cpuacct(void *arg)
{
    (void)arg;
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
	{ "user",			&cpuacct.cputime.user },
	{ "system",			&cpuacct.cputime.system },
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
    char *escname, escbuf[MAXPATHLEN];
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

    escname = unit_name_unescape(name, escbuf);
    for (cpu = 0; ; cpu++) {
	value = strtoull(p, &endp, 0);
	if (*endp == '\0' || endp == p)
	    break;
	p = endp;
	while (p && isspace((int)*p))
	    p++;
	pmsprintf(inst, sizeof(inst), "%s::cpu%d", escname, cpu);
	sts = pmdaCacheLookupName(indom, inst, NULL, (void **)&percpuacct);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	if (sts != PMDA_CACHE_INACTIVE) {
	    percpuacct = (cgroup_percpuacct_t *)calloc(1, sizeof(cgroup_percpuacct_t));
	    if (!percpuacct)
		continue;
	}
	percpuacct->usage = value;
	pmdaCacheStore(indom, PMDA_CACHE_ADD, inst, percpuacct);
    }
    fclose(fp);
    return 0;
}

static void
refresh_cpuacct(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_CPUACCT_INDOM);
    cgroup_cpuacct_t *cpuacct;
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    (void)arg;
    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&cpuacct);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	cpuacct = (cgroup_cpuacct_t *)calloc(1, sizeof(cgroup_cpuacct_t));
	if (!cpuacct)
	    return;
    }
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpuacct.stat");
    read_cpuacct_stats(file, cpuacct);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpuacct.usage");
    read_oneline_ull(file, &cpuacct->cputime.usage);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpuacct.usage_percpu");
    read_percpuacct_usage(file, name);
    cgroup_container(name, id, sizeof(id), &cpuacct->container);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, cpuacct);
}

static void
setup_cpusched(void *arg)
{
    (void)arg;
    pmdaCacheOp(INDOM(CGROUP_CPUSCHED_INDOM), PMDA_CACHE_INACTIVE);
}

static int
read_cpu_time(const char *file, cgroup_cputime_t *ccp)
{
    static cgroup_cputime_t cputime;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } cputime_fields[] = {
	{ "usage_usec",			&cputime.usage },
	{ "user_usec",			&cputime.user },
	{ "system_usec",		&cputime.system },
	{ NULL, NULL }
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    memset(&cputime, -1, sizeof(cputime));
    if ((fp = fopen(file, "r")) == NULL) {
	memcpy(ccp, &cputime, sizeof(cputime));
	return -ENOENT;
    }
    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	if (sscanf(buffer, "%s %llu\n", &name[0], &value) < 2)
	    continue;
	for (i = 0; cputime_fields[i].field != NULL; i++) {
	    if (strcmp(name, cputime_fields[i].field) != 0)
		continue;
	    *cputime_fields[i].offset = value;
	    break;
	}
    }
    fclose(fp);
    memcpy(ccp, &cputime, sizeof(cputime));
    return 0;
}

static int
read_cpu_stats(const char *file, cgroup_cpustat_t *ccp)
{
    static cgroup_cpustat_t cpustat;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } cpustat_fields[] = {
	{ "usage_usec",			&cpustat.cputime.usage },
	{ "user_usec",			&cpustat.cputime.user },
	{ "system_usec",		&cpustat.cputime.system },
	{ "nr_periods",			&cpustat.nr_periods },
	{ "nr_throttled",		&cpustat.nr_throttled },
	{ "throttled_time",		&cpustat.throttled_time },
	{ NULL, NULL }
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    memset(&cpustat, -1, sizeof(cpustat));
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

static void
refresh_cpusched(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_CPUSCHED_INDOM);
    cgroup_cpusched_t *cpusched;
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    (void)arg;
    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&cpusched);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE) {
	cpusched = (cgroup_cpusched_t *)calloc(1, sizeof(cgroup_cpusched_t));
	if (!cpusched)
	    return;
    }
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.stat");
    read_cpu_stats(file, &cpusched->stat);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.shares");
    read_oneline_ull(file, &cpusched->shares);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.cfs_period_us");
    read_oneline_ull(file, &cpusched->cfs_period);
    pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.cfs_quota_us");
    read_oneline_ll(file, &cpusched->cfs_quota);
    cgroup_container(name, id, sizeof(id), &cpusched->container);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, cpusched);
}

static void
setup_memory(void *arg)
{
    (void)arg;
    pmdaCacheOp(INDOM(CGROUP_MEMORY_INDOM), PMDA_CACHE_INACTIVE);
}

static int
read_memory_stats(const char *file, cgroup_memstat_t *cmp)
{
    static cgroup_memstat_t memory;
    static struct {
	char		*field;
	__uint64_t	*offset;
    } memory_fields[] = {
	{ "active_anon",		&memory.active_anon },
	{ "active_file",		&memory.active_file },
	{ "anon",			&memory.anon },
	{ "anon_thp",			&memory.anon_thp },
	{ "cache",			&memory.cache },
	{ "file",			&memory.file },
	{ "file_dirty",			&memory.file_dirty },
	{ "file_mapped",		&memory.file_mapped },
	{ "file_writeback",		&memory.file_writeback },
	{ "inactive_anon",		&memory.inactive_anon },
	{ "inactive_file",		&memory.inactive_file },
	{ "kernel_stack",		&memory.kernel_stack },
	{ "mapped_file",		&memory.mapped_file },
	{ "pgactivate",			&memory.pgactivate },
	{ "pgdeactivate",		&memory.pgdeactivate },
	{ "pgfault",			&memory.pgfault },
	{ "pglazyfree",			&memory.pglazyfree },
	{ "pglazyfreed",		&memory.pglazyfreed },
	{ "pgmajfault",			&memory.pgmajfault },
	{ "pgpgin",			&memory.pgpgin },
	{ "pgpgout",			&memory.pgpgout },
	{ "pgrefill",			&memory.pgrefill },
	{ "pgscan",			&memory.pgscan },
	{ "pgsteal",			&memory.pgsteal },
	{ "recent_rotated_anon",	&memory.recent_rotated_anon },
	{ "recent_rotated_file",	&memory.recent_rotated_file },
	{ "recent_scanned_anon",	&memory.recent_scanned_anon },
	{ "recent_scanned_file",	&memory.recent_scanned_file },
	{ "rss",			&memory.rss },
	{ "rss_huge",			&memory.rss_huge },
	{ "shmem",			&memory.shmem },
	{ "slab",			&memory.slab },
	{ "slab_reclaimable",		&memory.slab_reclaimable },
	{ "slab_unreclaimable",		&memory.slab_unreclaimable },
	{ "sock",			&memory.sock },
	{ "swap",			&memory.swap },
	{ "thp_collapse_alloc",		&memory.thp_collapse_alloc },
	{ "thp_fault_alloc",		&memory.thp_fault_alloc },
	{ "total_cache",		&memory.total_cache },
	{ "total_rss",			&memory.total_rss },
	{ "total_rss_huge",		&memory.total_rss_huge },
	{ "total_mapped_file",		&memory.total_mapped_file },
	{ "total_writeback",		&memory.total_writeback },
	{ "total_swap",			&memory.total_swap },
	{ "total_pgpgin",		&memory.total_pgpgin },
	{ "total_pgpgout",		&memory.total_pgpgout },
	{ "total_pgfault",		&memory.total_pgfault },
	{ "total_pgmajfault",		&memory.total_pgmajfault },
	{ "total_inactive_anon",	&memory.total_inactive_anon },
	{ "total_active_anon",		&memory.total_active_anon },
	{ "total_inactive_file",	&memory.total_inactive_file },
	{ "total_active_file",		&memory.total_active_file },
	{ "total_unevictable",		&memory.total_unevictable },
	{ "unevictable",		&memory.unevictable },
	{ "workingset_activate",	&memory.workingset_activate },
	{ "workingset_nodereclaim",	&memory.workingset_nodereclaim },
	{ "workingset_refault",		&memory.workingset_refault },
	{ "writeback",			&memory.writeback },
	{ NULL, NULL }
    };
    char buffer[4096], name[64];
    unsigned long long value;
    FILE *fp;
    int i;

    memset(&memory, -1, sizeof(memory));
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

static void
refresh_memory(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_MEMORY_INDOM);
    cgroup_memory_t *memory;
    char *escname, escbuf[MAXPATHLEN];
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    int sts;

    (void)arg;
    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&memory);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE &&
	(memory = (cgroup_memory_t *)calloc(1, sizeof(cgroup_memory_t))) == NULL)
	return;

    pmsprintf(file, sizeof(file), "%s/%s", path, "memory.stat");
    read_memory_stats(file, &memory->stat);
    pmsprintf(file, sizeof(file), "%s/%s", path, "memory.current");
    read_oneline_ull(file, &memory->current);
    pmsprintf(file, sizeof(file), "%s/%s", path, "memory.limit_in_bytes");
    read_oneline_ull(file, &memory->limit);
    pmsprintf(file, sizeof(file), "%s/%s", path, "memory.usage_in_bytes");
    read_oneline_ull(file, &memory->usage);
    pmsprintf(file, sizeof(file), "%s/%s", path, "memory.failcnt");
    read_oneline_ull(file, &memory->failcnt);
    cgroup_container(name, id, sizeof(id), &memory->container);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, memory);
}

static void
setup_netcls(void *arg)
{
    (void)arg;
    pmdaCacheOp(INDOM(CGROUP_NETCLS_INDOM), PMDA_CACHE_INACTIVE);
}

static void
refresh_netcls(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_NETCLS_INDOM);
    cgroup_netcls_t *netcls;
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    (void)arg;
    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&netcls);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE &&
	(netcls = (cgroup_netcls_t *)calloc(1, sizeof(cgroup_netcls_t))) == NULL)
	return;

    pmsprintf(file, sizeof(file), "%s/%s", path, "net_cls.classid");
    read_oneline_ull(file, &netcls->classid);
    cgroup_container(name, id, sizeof(id), &netcls->container);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, netcls);
}

static void
setup_cgroup2(void *arg)
{
    pmdaCacheOp(INDOM(CGROUP2_INDOM), PMDA_CACHE_INACTIVE);
}

static void
setup_cgroup2_iostats(void *arg)
{
    pmdaCacheOp(INDOM(CGROUP2_PERDEV_INDOM), PMDA_CACHE_INACTIVE);
}

static void
setup_blkio(void *arg)
{
    (void)arg;
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
    case CG_BLKIO_THROTTLEIOSERVICEBYTES_TOTAL:
	return &perdev->stats.throttle_io_service_bytes;
    case CG_BLKIO_THROTTLEIOSERVICED_TOTAL:
	return &perdev->stats.throttle_io_serviced;
    }
    return NULL;
}

static char *
get_blkdev(pmInDom devtindom, unsigned int major, unsigned int minor)
{
    char	buf[64];
    device_t	*dev;
    int		sts;

    pmsprintf(buf, sizeof(buf), "%u:%u", major, minor);
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
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    escname = unit_name_unescape(name, escbuf);
    pmsprintf(inst, size, "%s::%s", escname, disk);
    sts = pmdaCacheLookupName(indom, inst, NULL, (void **)&cdevp);
    if (sts == PMDA_CACHE_ACTIVE) {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdevblkio active %s\n", inst);
	return cdevp;
    }
    if (sts != PMDA_CACHE_INACTIVE) {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdevblkio new %s\n", inst);
	cdevp = (cgroup_perdevblkio_t *)calloc(1, sizeof(cgroup_perdevblkio_t));
	if (!cdevp)
	    return NULL;
    } else {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdevblkio inactive %s\n", inst);
	memset(cdevp, 0, sizeof(cgroup_perdevblkio_t));
    }
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
	    if (strcmp(op, blkio_fields[i].field) != 0)
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

static void
refresh_blkio(const char *path, const char *name, void *arg)
{
    pmInDom indom = INDOM(CGROUP_BLKIO_INDOM);
    cgroup_blkio_t *blkio;
    char file[MAXPATHLEN];
    char id[MAXCIDLEN];
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&blkio);
    if (sts == PMDA_CACHE_ACTIVE)
	return;
    if (sts != PMDA_CACHE_INACTIVE &&
	(blkio = (cgroup_blkio_t *)calloc(1, sizeof(cgroup_blkio_t))) == NULL)
	return;

    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_merged");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOMERGED_TOTAL, &blkio->total.io_merged);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_queued");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOQUEUED_TOTAL, &blkio->total.io_queued);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_service_bytes");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICEBYTES_TOTAL, &blkio->total.io_service_bytes);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_serviced");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICED_TOTAL, &blkio->total.io_serviced);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_service_time");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOSERVICETIME_TOTAL, &blkio->total.io_service_time);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.io_wait_time");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_IOWAITTIME_TOTAL, &blkio->total.io_wait_time);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.sectors");
    read_blkio_devices_value(file, name,
		CG_BLKIO_SECTORS, &blkio->total.sectors);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.time");
    read_blkio_devices_value(file, name,
		CG_BLKIO_TIME, &blkio->total.time);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.throttle.io_service_bytes");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_THROTTLEIOSERVICEBYTES_TOTAL, &blkio->total.throttle_io_service_bytes);
    pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.throttle.io_serviced");
    read_blkio_devices_stats(file, name,
		CG_BLKIO_THROTTLEIOSERVICED_TOTAL, &blkio->total.throttle_io_serviced);
    cgroup_container(name, id, sizeof(id), &blkio->container);

    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, blkio);
}

void
refresh_cgroups1(const char *cgroup, size_t cgrouplen, void *arg)
{
    int *need_refresh = (int *)arg;

    if (need_refresh[CLUSTER_CPUACCT_GROUPS])
	refresh_cgroup_cpu_map();
    if (need_refresh[CLUSTER_BLKIO_GROUPS])
	refresh_cgroup_device_map();

    if (need_refresh[CLUSTER_CPUSET_GROUPS])
	refresh_cgroups("cpuset", cgroup, cgrouplen,
			setup_cpuset, refresh_cpuset, arg);
    if (need_refresh[CLUSTER_CPUACCT_GROUPS])
	refresh_cgroups("cpuacct", cgroup, cgrouplen,
			setup_cpuacct, refresh_cpuacct, arg);
    if (need_refresh[CLUSTER_CPUSCHED_GROUPS])
	refresh_cgroups("cpu", cgroup, cgrouplen,
			setup_cpusched, refresh_cpusched, arg);
    if (need_refresh[CLUSTER_MEMORY_GROUPS])
	refresh_cgroups("memory", cgroup, cgrouplen,
			setup_memory, refresh_memory, arg);
    if (need_refresh[CLUSTER_NETCLS_GROUPS])
	refresh_cgroups("netcls", cgroup, cgrouplen,
			setup_netcls, refresh_netcls, arg);
    if (need_refresh[CLUSTER_BLKIO_GROUPS])
	refresh_cgroups("blkio", cgroup, cgrouplen,
			setup_blkio, refresh_blkio, arg);
}

static cgroup_perdev_iostat_t *
get_perdev_iostat(pmInDom indom, const char *name, const char *disk,
                char *inst, size_t size)
{
    cgroup_perdev_iostat_t *cdevp;
    char *escname, escbuf[MAXPATHLEN];
    int sts;

    escname = unit_name_unescape(name, escbuf);
    pmsprintf(inst, size, "%s::%s", escname, disk);
    sts = pmdaCacheLookupName(indom, inst, NULL, (void **)&cdevp);
    if (sts == PMDA_CACHE_ACTIVE) {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdev_iostat active %s\n", inst);
	return cdevp;
    }
    if (sts != PMDA_CACHE_INACTIVE) {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdev_iostat new %s\n", inst);
	cdevp = (cgroup_perdev_iostat_t *)calloc(1, sizeof(*cdevp));
	if (!cdevp)
	    return NULL;
    } else {
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "get_perdev_iostat inactive %s\n", inst);
	memset(cdevp, 0, sizeof(*cdevp));
    }
    return cdevp;
}

static int
read_io_stats(const char *file, const char *name)
{
    pmInDom indom = INDOM(CGROUP2_PERDEV_INDOM);
    pmInDom devtindom = INDOM(DEVT_INDOM);
    cgroup_perdev_iostat_t *iodev;
    char buffer[4096];
    FILE *fp;

    if ((fp = fopen(file, "r")) == NULL)
	return -ENOENT;

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
	unsigned int major, minor;
	cgroup_iostat_t io;
	char *devname;
	int i;

	i = sscanf(buffer, "%u:%u rbytes=%llu wbytes=%llu rios=%llu wios=%llu "
			   "dbytes=%llu dios=%llu\n", &major, &minor,
		(unsigned long long *)&io.rbytes, (unsigned long long *)&io.wbytes,
		(unsigned long long *)&io.rios, (unsigned long long *)&io.wios,
		(unsigned long long *)&io.dbytes, (unsigned long long *)&io.dios);
	if (i < 8)
	    continue;
	if ((devname = get_blkdev(devtindom, major, minor)) == NULL)
	    continue;
	/* all device fields are now acquired, update indom and cgroup total */
	iodev = get_perdev_iostat(indom, name, devname, buffer, sizeof(buffer));
	iodev->stats = io;	/* struct copy */
	pmdaCacheStore(indom, PMDA_CACHE_ADD, buffer, iodev);
    }
    fclose(fp);
    return 0;
}

void
setup_all(void *arg)
{
    int *need_refresh = (int *)arg;

    if (need_refresh[CLUSTER_CPUACCT_GROUPS])
	refresh_cgroup_cpu_map();
    if (need_refresh[CLUSTER_CGROUP2_IO_STAT] ||
	need_refresh[CLUSTER_BLKIO_GROUPS])
	refresh_cgroup_device_map();

    if (need_refresh[CLUSTER_CGROUP2_CPU_PRESSURE] ||
	need_refresh[CLUSTER_CGROUP2_CPU_STAT] ||
	need_refresh[CLUSTER_CGROUP2_IO_PRESSURE] ||
	need_refresh[CLUSTER_CGROUP2_IO_STAT] ||
	need_refresh[CLUSTER_CGROUP2_MEM_PRESSURE]) {
	setup_cgroup2(arg);
	if (need_refresh[CLUSTER_CGROUP2_IO_STAT])
	    setup_cgroup2_iostats(arg);
    }

    if (need_refresh[CLUSTER_CPUSET_GROUPS])
	setup_cpuset(arg);
    if (need_refresh[CLUSTER_CPUACCT_GROUPS])
	setup_cpuacct(arg);
    if (need_refresh[CLUSTER_CPUSCHED_GROUPS])
	setup_cpusched(arg);
    if (need_refresh[CLUSTER_MEMORY_GROUPS])
	setup_memory(arg);
    if (need_refresh[CLUSTER_NETCLS_GROUPS])
	setup_netcls(arg);
    if (need_refresh[CLUSTER_BLKIO_GROUPS])
	setup_blkio(arg);
}

static void
refresh_all(const char *path, const char *name, void *arg)
{
    cgroup2_t *cgroup;
    pmInDom indom = INDOM(CGROUP2_INDOM);
    char file[MAXPATHLEN], id[MAXCIDLEN];
    char *escname, escbuf[MAXPATHLEN+16];
    int sts, *need_refresh = (int *)arg;

    escname = unit_name_unescape(name, escbuf);
    sts = pmdaCacheLookupName(indom, escname, NULL, (void **)&cgroup);
    if (sts == PMDA_CACHE_ACTIVE)
	goto v1;
    if (sts != PMDA_CACHE_INACTIVE &&
	(cgroup = (cgroup2_t *)calloc(1, sizeof(cgroup2_t))) == NULL)
	goto v1;

    if (need_refresh[CLUSTER_CGROUP2_CPU_PRESSURE]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.pressure");
	read_pressures(file, &cgroup->cpu_pressures, CG_PSI_SOME);
    }

    if (need_refresh[CLUSTER_CGROUP2_CPU_STAT]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.stat");
	read_cpu_time(file, &cgroup->cputime);
    }

    if (need_refresh[CLUSTER_CGROUP2_IO_PRESSURE]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "io.pressure");
	read_pressures(file, &cgroup->io_pressures, CG_PSI_SOME|CG_PSI_FULL);
    }

    if (need_refresh[CLUSTER_CGROUP2_IO_STAT]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "io.stat");
	read_io_stats(file, name);
    }

    if (need_refresh[CLUSTER_CGROUP2_MEM_PRESSURE]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "memory.pressure");
	read_pressures(file, &cgroup->mem_pressures, CG_PSI_SOME|CG_PSI_FULL);
    }

    if (need_refresh[CLUSTER_CGROUP2_IRQ_PRESSURE]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "irq.pressure");
	read_pressures(file, &cgroup->irq_pressures, CG_PSI_FULL);
    }

    cgroup_container(name, id, sizeof(id), &cgroup->container);
    pmdaCacheStore(indom, PMDA_CACHE_ADD, escname, cgroup);

v1:
    /*
     * Deprecated v1 cgroup subsystems follow, some rarely used now
     * (memory stats, however, are still always handled this way).
     */

    if (need_refresh[CLUSTER_CPUSET_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "cpuset.cpus");
	if (access(path, R_OK) == 0)
	    refresh_cpuset(file, name, NULL);
    }

    if (need_refresh[CLUSTER_CPUACCT_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "cpuacct.cpus");
	if (access(path, R_OK) == 0)
	    refresh_cpuacct(file, name, NULL);
    }

    if (need_refresh[CLUSTER_CPUSCHED_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "cpu.shares");
	if (access(file, R_OK) == 0)
	    refresh_cpusched(path, name, NULL);
    }

    if (need_refresh[CLUSTER_MEMORY_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "memory.stat");
	if (access(file, R_OK) == 0)
	    refresh_memory(path, name, NULL);
    }

    if (need_refresh[CLUSTER_NETCLS_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "net_cls.classid");
	if (access(file, R_OK) == 0)
	    refresh_netcls(path, name, NULL);
    }

    if (need_refresh[CLUSTER_BLKIO_GROUPS]) {
	pmsprintf(file, sizeof(file), "%s/%s", path, "blkio.time");
	if (access(file, R_OK) == 0)
	    refresh_blkio(path, name, NULL);
    }
}

void
refresh_cgroups2(const char *cgroup, size_t cgrouplen, void *arg)
{
    refresh_cgroups(NULL, cgroup, cgrouplen, setup_all, refresh_all, arg);
}
