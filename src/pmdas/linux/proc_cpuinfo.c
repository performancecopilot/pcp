/*
 * Linux /proc/cpuinfo metrics cluster
 *
 * Copyright (c) 2013-2015 Red Hat.
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Portions Copyright (c) 2001 Gilly Ran (gilly@exanet.com) - for the
 * portions supporting the Alpha platform.  All rights reserved.
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

#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "proc_cpuinfo.h"

static void
map_cpu_nodes(proc_cpuinfo_t *proc_cpuinfo)
{
    int i;
    const char *node_path = "sys/devices/system/node";
    char path[MAXPATHLEN];
    DIR *nodes, *cpus;
    struct dirent *de, *ce;
    int node, max_node_number = 0;
    int cpu;
    pmdaIndom *node_idp = PMDAINDOM(NODE_INDOM);
    pmdaIndom *cpu_idp = PMDAINDOM(CPU_INDOM);

    if (cpu_idp->it_numinst == 1) {
	/* fake one numa node, like most kernels do */
	max_node_number = 0;
    	proc_cpuinfo->cpuinfo[0].node = max_node_number;
    }
    else {
	/*
	 * scan /sys to figure out cpu - node mapping
	 */
	for (i = 0; i < cpu_idp->it_numinst; i++)
	    proc_cpuinfo->cpuinfo[i].node = 0;

	snprintf(path, sizeof(path), "%s/%s", linux_statspath, node_path);
	if ((nodes = opendir(path)) != NULL) {
	    while ((de = readdir(nodes)) != NULL) {
		if (sscanf(de->d_name, "node%d", &node) != 1)
		    continue;
		if (node > max_node_number)
		    max_node_number = node;
		snprintf(path, sizeof(path), "%s/%s/%s",
		    linux_statspath, node_path, de->d_name);
		if ((cpus = opendir(path)) == NULL)
		    continue;
		while ((ce = readdir(cpus)) != NULL) {
		    if (sscanf(ce->d_name, "cpu%d", &cpu) != 1)
			continue;
		    if (cpu >= 0 && cpu < cpu_idp->it_numinst)
			proc_cpuinfo->cpuinfo[cpu].node = node;
		}
		closedir(cpus);
	    }
	    closedir(nodes);
	}
    }

    /* initialize node indom */
    node_idp->it_numinst = max_node_number + 1;
    node_idp->it_set = calloc(max_node_number + 1, sizeof(pmdaInstid));
    for (i = 0; i < node_idp->it_numinst; i++) {
	char node_name[16];

	snprintf(node_name, sizeof(node_name), "node%d", i);
	node_idp->it_set[i].i_inst = i;
	node_idp->it_set[i].i_name = strdup(node_name);
    }
    proc_cpuinfo->node_indom = node_idp;
}

char *
cpu_name(proc_cpuinfo_t *proc_cpuinfo, unsigned int cpu_num)
{
    char name[1024];
    char *p;
    FILE *f;
    static int started = 0;

    if (!started) {
	refresh_proc_cpuinfo(proc_cpuinfo);

	proc_cpuinfo->machine = NULL;
	f = linux_statsfile("/proc/sgi_prominfo/node0/version", name, sizeof(name));
	if (f != NULL) {
	    while (fgets(name, sizeof(name), f)) {
		if (strncmp(name, "SGI", 3) == 0) {
		    if ((p = strstr(name, " IP")) != NULL)
			proc_cpuinfo->machine = strndup(p+1, 4);
		    break;
		}
	    }
	    fclose(f);
	}
	if (proc_cpuinfo->machine == NULL)
	    proc_cpuinfo->machine = strdup("linux");

	started = 1;
    }

    snprintf(name, sizeof(name), "cpu%u", cpu_num);
    return strdup(name);
}

/*
 * Refresh state of NUMA node and CPU online state for one
 * CPU or NUMA node ("node" parameter).
 */
int
refresh_sysfs_online(unsigned int node_num, const char *node)
{
    const char *sysfs_path = "sys/devices/system";
    char path[MAXPATHLEN];
    unsigned int online;
    FILE *fp;
    int n;

    snprintf(path, sizeof(path), "%s/%s/%s/%s%u/online",
		linux_statspath, sysfs_path, node, node, node_num);
    if ((fp = fopen(path, "r")) == NULL)
	return 1;
    n = fscanf(fp, "%u", &online);
    fclose(fp);
    if (n != 1)
	return 1;
    return online;
}

static char *
trim_whitespace(char *s)
{
    char *end;

    while (isspace(*s))
	s++;	/* trim leading whitespace */
    if (*s == '\0') 
	return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace(*end))
	end--;	/* trim trailing whitespace */
    *(end + 1) = '\0';
    return s;
}

static void
setup_cpuinfo(cpuinfo_t *cpuinfo)
{
    cpuinfo->sapic = -1;
    cpuinfo->vendor = -1;
    cpuinfo->model = -1;
    cpuinfo->model_name = -1;
    cpuinfo->stepping = -1;
    cpuinfo->flags = -1;
}

int
refresh_proc_cpuinfo(proc_cpuinfo_t *proc_cpuinfo)
{
#define PROCESSOR_LINE 1
    char buf[4096];
    FILE *fp;
    int i, cpunum, cpuid;
    int dups = 0, previous = -1;
    cpuinfo_t saved = { 0 };
    cpuinfo_t *info = NULL;
    char *val;
    char *p;
    static int started;

    if (!started) {
	int need = proc_cpuinfo->cpuindom->it_numinst * sizeof(cpuinfo_t);
	proc_cpuinfo->cpuinfo = (cpuinfo_t *)calloc(1, need);
	for (cpunum = 0; cpunum < proc_cpuinfo->cpuindom->it_numinst; cpunum++)
	    setup_cpuinfo(&proc_cpuinfo->cpuinfo[cpunum]);
	started = 1;
    }

    if ((fp = linux_statsfile("/proc/cpuinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    cpunum = -1;
    setup_cpuinfo(&saved);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((val = strrchr(buf, '\n')) != NULL)
	    *val = '\0';
	if ((val = strchr(buf, ':')) == NULL)
	    continue;
	val += 2;

	if (strncmp(buf, "processor", 9) == 0) {
	    cpunum++;
	    if (previous == PROCESSOR_LINE)
		dups = 1;	/* aarch64-mode; dup values at the end */
	    previous = PROCESSOR_LINE;
	    proc_cpuinfo->cpuinfo[cpunum].cpu_num = atoi(val);
	    continue;
	}
	previous = !PROCESSOR_LINE;

	if (cpunum >= proc_cpuinfo->cpuindom->it_numinst)
	    continue;

	/* we may need to save up state before seeing any processor ID */
	if (dups || cpunum < 0) {
	    dups = 1;
	    info = &saved;
	}
	else {
	    info = &proc_cpuinfo->cpuinfo[cpunum];
	}

	/* note: order is important due to strNcmp comparisons */
	if (info->sapic < 0 && strncasecmp(buf, "sapic", 5) == 0)
	    info->sapic = linux_strings_insert(val);
	else if (info->model_name < 0 && strncasecmp(buf, "model name", 10) == 0)
	    info->model_name = linux_strings_insert(val);
	else if (info->model_name < 0 && strncasecmp(buf, "hardware", 8) == 0)
	    info->model_name = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "model", 5) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "cpu model", 9) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->model < 0 && strncasecmp(buf, "cpu variant", 11) == 0)
	    info->model = linux_strings_insert(val);
	else if (info->vendor < 0 && strncasecmp(buf, "vendor", 6) == 0)
	    info->vendor = linux_strings_insert(val);
	else if (info->vendor < 0 && strncasecmp(buf, "cpu implementer", 15) == 0)
	    info->vendor = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "step", 4) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "revision", 8) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->stepping < 0 && strncasecmp(buf, "cpu revision", 12) == 0)
	    info->stepping = linux_strings_insert(val);
	else if (info->flags < 0 && strncasecmp(buf, "flags", 5) == 0)
	    info->flags = linux_strings_insert(val);
	else if (info->flags < 0 && strncasecmp(buf, "features", 8) == 0)
	    info->flags = linux_strings_insert(trim_whitespace(val));
	else if (info->cache == 0 && strncasecmp(buf, "cache size", 10) == 0)
	    info->cache = atoi(val);
	else if (info->cache_align == 0 && strncasecmp(buf, "cache_align", 11) == 0)
	    info->cache_align = atoi(val);
	else if (info->bogomips == 0.0 && strncasecmp(buf, "bogo", 4) == 0)
	    info->bogomips = atof(val);
	else if (strncasecmp(buf, "cpu MHz", 7) == 0) /* cpu MHz can change */
	    info->clock = atof(val);
	else if (info->clock == 0.0 && strncasecmp(buf, "cycle frequency", 15) == 0) {
	    if ((p = strchr(val, ' ')) != NULL)
		*p = '\0';
	    info->clock = (atof(val))/1000000;
	}
    }
    fclose(fp);

    /* all identical processors, duplicate last through earlier instances */
    if (dups) {
	for (i = 0; i < proc_cpuinfo->cpuindom->it_numinst; i++) {
	    if ((cpuid = proc_cpuinfo->cpuinfo[i].cpu_num) == 0)
		cpuid = i;
	    memcpy(&proc_cpuinfo->cpuinfo[i], &saved, sizeof(cpuinfo_t));
	    proc_cpuinfo->cpuinfo[i].cpu_num = cpuid;
	}
    }

    if (started < 2) {
	map_cpu_nodes(proc_cpuinfo);
    	started = 2;
    }

    /* success */
    return 0;
}
