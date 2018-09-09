/*
 * Linux /proc/stat metrics cluster
 *
 * Copyright (c) 2012-2014,2017 Red Hat.
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
 * Copyright (c) 2000,2004-2008 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "linux.h"
#include "proc_stat.h"
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>

/*
 * Allocate instance identifiers for all CPUs.  Note there is a
 * need to deal with CPUs/nodes going online and offline during
 * the life of the PMDA - usually we're only able to get values
 * for online resources (/proc/stat reports online CPUs only).
 *
 * We must create a direct mapping of CPU ID to instance ID, for
 * historical reasons.  So initially all have a NULL private data
 * pointer associated with them, which we'll subsequently fill in
 * if/when the CPU/node is discovered to be online (later).
 */
static void
setup_cpu_indom(pmInDom cpus)
{
    char	name[64];
    int		i;

    if (_pm_ncpus < 1)
	_pm_ncpus = 1;	/* sanity, surely there must be at least one CPU */

    pmdaCacheOp(cpus, PMDA_CACHE_CULL);
    for (i = 0; i < _pm_ncpus; i++) {
	pmsprintf(name, sizeof(name)-1, "cpu%u", i);
	pmdaCacheStore(cpus, PMDA_CACHE_ADD, name, NULL);
    }
}

void
setup_cpu_info(cpuinfo_t *cip)
{
    cip->sapic = -1;
    cip->vendor = -1;
    cip->model = -1;
    cip->model_name = -1;
    cip->stepping = -1;
    cip->flags = -1;
}

static void
cpu_add(pmInDom cpus, unsigned int cpuid, unsigned int nodeid)
{
    percpu_t	*cpu;
    char	name[64];

    if ((cpu = (percpu_t *)calloc(1, sizeof(percpu_t))) == NULL)
	return;
    cpu->cpuid = cpuid;
    cpu->nodeid = nodeid;
    setup_cpu_info(&cpu->info);
    pmsprintf(name, sizeof(name)-1, "cpu%u", cpuid);
    pmdaCacheStore(cpus, PMDA_CACHE_ADD, name, (void*)cpu);
}

static void
node_add(pmInDom nodes, unsigned int nodeid)
{
    pernode_t	*node;
    char	name[64];

    if ((node = (pernode_t *)calloc(1, sizeof(pernode_t))) == NULL)
	return;
    node->nodeid = nodeid;
    pmsprintf(name, sizeof(name)-1, "node%u", nodeid);
    pmdaCacheStore(nodes, PMDA_CACHE_ADD, name, (void*)node);
}

void
cpu_node_setup(void)
{
    const char		*node_path = "sys/devices/system/node";
    pmInDom		cpus, nodes;
    unsigned int	cpu, node;
    struct dirent	**node_files = NULL;
    struct dirent	*cpu_entry;
    DIR			*cpu_dir;
    int			i, count;
    char		path[MAXPATHLEN];
    static int		setup;

    if (setup)
	return;
    setup = 1;

    nodes = INDOM(NODE_INDOM);
    cpus = INDOM(CPU_INDOM);
    setup_cpu_indom(cpus);

    pmsprintf(path, sizeof(path), "%s/%s", linux_statspath, node_path);
    count = scandir(path, &node_files, NULL, versionsort);
    if (!node_files || (linux_test_mode & LINUX_TEST_NCPUS)) {
	/* QA mode or no sysfs support, assume single NUMA node */
	node_add(nodes, 0);	/* default to just node zero */
	for (cpu = 0; cpu < _pm_ncpus; cpu++)
	    cpu_add(cpus, cpu, 0);	/* all in node zero */
	goto done;
    }

    for (i = 0; i < count; i++) {
	if (sscanf(node_files[i]->d_name, "node%u", &node) != 1)
	    continue;
	node_add(nodes, node);
	pmsprintf(path, sizeof(path), "%s/%s/%s",
		 linux_statspath, node_path, node_files[i]->d_name);
	if ((cpu_dir = opendir(path)) == NULL)
	    continue;
	while ((cpu_entry = readdir(cpu_dir)) != NULL) {
	    if (sscanf(cpu_entry->d_name, "cpu%u", &cpu) != 1)
		continue;
	    cpu_add(cpus, cpu, node);
	}
	closedir(cpu_dir);
    }

done:
    if (node_files) {
	for (i = 0; i < count; i++)
	    free(node_files[i]);
	free(node_files);
    }
}

static int
find_line_format(const char *fmt, int fmtlen, char **bufindex, int nbufindex, int start)
{
    int j;

    if (start < nbufindex-1 && strncmp(fmt, bufindex[++start], fmtlen) == 0)
	return start;	/* fast-path, next line found where expected */

    for (j = 0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) != 0)
	    continue;
	return j;
    }
    return -1;
}

/*
 * We use /proc/stat as a single source of truth regarding online/offline
 * state for CPUs (its per-CPU stats are for online CPUs only).
 * This drives the contents of the CPU indom for all per-CPU metrics, so
 * it is important to ensure this refresh routine is called first before
 * refreshing any other per-CPU metrics (e.g. interrupts, softnet).
 */
int
refresh_proc_stat(proc_stat_t *proc_stat)
{
    pernode_t	*np;
    percpu_t	*cp;
    pmInDom	cpus, nodes;
    char	buf[MAXPATHLEN], *name, *sp, **bp;
    int		n = 0, i, size;

    static int fd = -1; /* kept open until exit(), unless testing */
    static char *statbuf;
    static int maxstatbuf;
    static char **bufindex;
    static int nbufindex;
    static int maxbufindex;

    cpu_node_setup();
    cpus = INDOM(CPU_INDOM);
    pmdaCacheOp(cpus, PMDA_CACHE_INACTIVE);
    nodes = INDOM(NODE_INDOM);

    /* reset per-node aggregate CPU utilisation stats */
    for (pmdaCacheOp(nodes, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(nodes, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(nodes, i, NULL, (void **)&np) || !np)
	    continue;
	memset(&np->stat, 0, sizeof(np->stat));
    }

    /* in test mode we replace procfs files (keeping fd open thwarts that) */
    if (fd >= 0 && (linux_test_mode & LINUX_TEST_STATSPATH)) {
	close(fd);
	fd = -1;
    }

    if (fd >= 0) {
	if (lseek(fd, 0, SEEK_SET) < 0)
	    return -oserror();
    } else {
	pmsprintf(buf, sizeof(buf), "%s/proc/stat", linux_statspath);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    return -oserror();
    }

    for (;;) {
	while (n >= maxstatbuf) {
	    size = maxstatbuf + 512;
	    if ((sp = (char *)realloc(statbuf, size)) == NULL)
		return -ENOMEM;
	    statbuf = sp;
	    maxstatbuf = size;
	}
	size = (statbuf + maxstatbuf) - (statbuf + n);
	if ((i = read(fd, statbuf + n, size)) > 0)
	    n += i;
	else
	    break;
    }
    statbuf[n] = '\0';

    if (bufindex == NULL) {
	size = 16 * sizeof(char *);
	if ((bufindex = (char **)malloc(size)) == NULL)
	    return -ENOMEM;
	maxbufindex = 16;
    }

    nbufindex = 0;
    bufindex[nbufindex] = statbuf;
    for (i = 0; i < n; i++) {
	if (statbuf[i] == '\n' || statbuf[i] == '\0') {
	    statbuf[i] = '\0';
	    if (nbufindex + 1 >= maxbufindex) {
		size = (maxbufindex + 4) * sizeof(char *);
		if ((bp = (char **)realloc(bufindex, size)) == NULL)
		    return -ENOMEM;
		bufindex = bp;
	    	maxbufindex += 4;
	    }
	    bufindex[++nbufindex] = statbuf + i + 1;
	}
    }

#define ALLCPU_FMT "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu"
    n = sscanf((const char *)bufindex[0], ALLCPU_FMT,
	&proc_stat->all.user, &proc_stat->all.nice,
	&proc_stat->all.sys, &proc_stat->all.idle,
	&proc_stat->all.wait, &proc_stat->all.irq,
	&proc_stat->all.sirq, &proc_stat->all.steal,
	&proc_stat->all.guest, &proc_stat->all.guest_nice);

#define PERCPU_FMT "cpu%u %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu"
    /*
     * per-CPU stats
     * e.g. cpu0 95379 4 20053 6502503
     * 2.6 kernels have 3 additional fields for wait, irq and soft_irq.
     * More recent (2008) 2.6 kernels have an extra field for guest and
     * also (since 2009) guest_nice.
     * In the single-CPU system case, don't bother scanning, use "all";
     * this handles non-SMP kernels with no line starting with "cpu0".
     */
    if ((size = pmdaCacheOp(cpus, PMDA_CACHE_SIZE)) == 1) {
	pmdaCacheLookup(cpus, 0, &name, (void **)&cp);
	memcpy(&cp->stat, &proc_stat->all, sizeof(cp->stat));
	pmdaCacheStore(cpus, PMDA_CACHE_ADD, name, (void *)cp);
	pmdaCacheLookup(nodes, 0, NULL, (void **)&np);
	memcpy(&np->stat, &proc_stat->all, sizeof(np->stat));
    }
    else {
	for (n = 0; n < nbufindex; n++) {
	    if (strncmp("cpu", bufindex[n], 3) != 0 ||
		!isdigit((int)bufindex[n][3]))
		continue;
	    cp = NULL;
	    np = NULL;
	    i = atoi(&bufindex[n][3]);	/* extract CPU identifier */
	    if (pmdaCacheLookup(cpus, i, &name, (void **)&cp) < 0 || !cp)
		continue;
	    memset(&cp->stat, 0, sizeof(cp->stat));
	    sscanf(bufindex[n], PERCPU_FMT, &i,
		    &cp->stat.user, &cp->stat.nice, &cp->stat.sys,
		    &cp->stat.idle, &cp->stat.wait, &cp->stat.irq,
		    &cp->stat.sirq, &cp->stat.steal, &cp->stat.guest,
		    &cp->stat.guest_nice);
	    pmdaCacheStore(cpus, PMDA_CACHE_ADD, name, (void *)cp);

	    /* update per-node aggregate CPU utilisation stats as well */
	    if (pmdaCacheLookup(nodes, cp->nodeid, NULL, (void **)&np) < 0)
		continue;
	    np->stat.user += cp->stat.user;
	    np->stat.nice += cp->stat.nice;
	    np->stat.sys += cp->stat.sys;
	    np->stat.idle += cp->stat.idle;
	    np->stat.wait += cp->stat.wait;
	    np->stat.irq += cp->stat.irq;
	    np->stat.sirq += cp->stat.sirq;
	    np->stat.steal += cp->stat.steal;
	    np->stat.guest += cp->stat.guest;
	    np->stat.guest_nice += cp->stat.guest_nice;
	}
    }

    i = size;

#define PAGE_FMT "page %u %u"	/* NB: moved to /proc/vmstat in 2.6 kernels */
    if ((i = find_line_format(PAGE_FMT, 5, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], PAGE_FMT,
		&proc_stat->page[0], &proc_stat->page[1]);

#define SWAP_FMT "swap %u %u"	/* NB: moved to /proc/vmstat in 2.6 kernels */
    if ((i = find_line_format(SWAP_FMT, 5, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], SWAP_FMT,
		&proc_stat->swap[0], &proc_stat->swap[1]);

#define INTR_FMT "intr %llu"	/* (export 1st 'total interrupts' value only) */
    if ((i = find_line_format(INTR_FMT, 5, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], INTR_FMT, &proc_stat->intr);

#define CTXT_FMT "ctxt %llu"
    if ((i = find_line_format(CTXT_FMT, 5, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], CTXT_FMT, &proc_stat->ctxt);

#define BTIME_FMT "btime %lu"
    if ((i = find_line_format(BTIME_FMT, 6, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], BTIME_FMT, &proc_stat->btime);

#define PROCESSES_FMT "processes %lu"
    if ((i = find_line_format(PROCESSES_FMT, 10, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], PROCESSES_FMT, &proc_stat->processes);

#define RUNNING_FMT "procs_running %lu"
    if ((i = find_line_format(RUNNING_FMT, 14, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], RUNNING_FMT, &proc_stat->procs_running);

#define BLOCKED_FMT "procs_blocked %lu"
    if ((i = find_line_format(BLOCKED_FMT, 14, bufindex, nbufindex, i)) >= 0)
	sscanf((const char *)bufindex[i], BLOCKED_FMT, &proc_stat->procs_blocked);

    /* success */
    return 0;
}
