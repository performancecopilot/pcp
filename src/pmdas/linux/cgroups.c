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
#include "filesys.h"
#include "dynamic.h"
#include "clusters.h"

typedef void (*cgroup_names_t)(__pmnsTree *, int, const char *, const char *);
typedef void (*cgroup_fetch_t)(int, int);

void cgroup_names_tasks(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_tasks(int index, int item) {}

void cgroup_names_cpusched(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_cpusched(int index, int item) {}

void cgroup_names_cpuset(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_cpuset(int index, int item) {}

void cgroup_names_cpuacct(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_cpuacct(int index, int item) {}

void cgroup_names_memory(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_memory(int index, int item) {}

void cgroup_names_netclass(__pmnsTree *pmns, int index, const char *mnt, const char *grp) {}
void cgroup_fetch_netclass(int index, int item) {}

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
    cgroup_names_t	names;
    cgroup_fetch_t	fetch;
} controllers[] = {
    { "cpu", cgroup_names_cpusched, cgroup_fetch_cpusched },
    { "cpuset", cgroup_names_cpuset, cgroup_fetch_cpuset },
    { "cpuacct", cgroup_names_cpuacct, cgroup_fetch_cpuacct },
    { "memory", cgroup_names_memory, cgroup_fetch_memory },
    { "net_cls", cgroup_names_netclass, cgroup_fetch_netclass },
};

static int
cgroup_scan(const char *mnt, const char *path, const char *options, int index, __pmnsTree *pmns)
{
    int i;
    DIR *dirp;
    struct stat sbuf;
    struct dirent *dp;
    char cgrouppath[MAXPATHLEN];

    if ((dirp = opendir(path)) == NULL)
	return -errno;

    /*
     * readdir - descend into directories to find all cgroups - populate namespace
     * with <groupname>/metrics-for-each-active-controller.  Normalise group name!
     */
    while ((dp = readdir(dirp)) != NULL) {
	if (dp->d_name[0] == '.')
	    continue;
	sprintf(cgrouppath, "%s/%s", path, dp->d_name);
	if (stat(cgrouppath, &sbuf) < 0)
	    continue;
	if (!(S_ISDIR(sbuf.st_mode)))
	    continue;

	cgroup_names_tasks(pmns, index, mnt, cgrouppath);

	/* use options to tell what controllers are active (and hence which metrics) */
	for (i = 0; i < sizeof(controllers)/sizeof(controllers[0]); i++) {
	    if (scan_filesys_options(options, controllers[i].name) == NULL)
		continue;
	    controllers[i].names(pmns, index, mnt, cgrouppath);
	}
	index++;

	/* also scan for any child cgroups */
        index = cgroup_scan(mnt, cgrouppath, options, index, pmns);
    }
    closedir(dirp);

    return index;
}

void
refresh_cgroup_groups(pmInDom mounts, __pmnsTree **pmns)
{
    filesys_t *fs;
    int sts, count = 0;
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
	count = cgroup_scan(fs->path, "", fs->options, count, tree);
    }

    if (pmns)
	*pmns = tree;
    else
	__pmFreePMNS(tree);
}

void
cgroup_init(pmInDom subsys, pmInDom mounts)
{
    int cgroups[] = { CLUSTER_CGROUP_CPUSET, 
		      CLUSTER_CGROUP_CPUACCT, CLUSTER_CGROUP_CPUSCHED, 
		      CLUSTER_CGROUP_MEMORY, CLUSTER_CGROUP_NET_CLS };

    linux_dynamic_pmns("cgroup.groups.", cgroups, sizeof(cgroups),
			mounts, refresh_cgroup_groups);
}
