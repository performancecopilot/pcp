/*
 * Linux /proc/stat metrics cluster
 *
 * Copyright (c) 2012-2014 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include "proc_cpuinfo.h"
#include "proc_stat.h"

int
refresh_proc_stat(proc_cpuinfo_t *proc_cpuinfo, proc_stat_t *proc_stat)
{
    pmdaIndom *idp = PMDAINDOM(CPU_INDOM);
    char buf[MAXPATHLEN];
    char fmt[64];
    static int fd = -1; /* kept open until exit() */
    static int started;
    static char *statbuf;
    static int maxstatbuf;
    static char **bufindex;
    static int nbufindex;
    static int maxbufindex;
    int size;
    int n;
    int i;
    int j;

    if (fd >= 0) {
	if (lseek(fd, 0, SEEK_SET) < 0)
	    return -oserror();
    } else {
	snprintf(buf, sizeof(buf), "%s/proc/stat", linux_statspath);
	if ((fd = open(buf, O_RDONLY)) < 0)
	    return -oserror();
    }

    for (n=0;;) {
	while (n >= maxstatbuf) {
	    size = maxstatbuf + 512;
	    if ((statbuf = (char *)realloc(statbuf, size)) == NULL)
		return -ENOMEM;
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
	size = 4 * sizeof(char *);
	if ((bufindex = (char **)malloc(size)) == NULL)
	    return -ENOMEM;
	maxbufindex = 4;
    }

    nbufindex = 0;
    bufindex[nbufindex] = statbuf;
    for (i=0; i < n; i++) {
	if (statbuf[i] == '\n' || statbuf[i] == '\0') {
	    statbuf[i] = '\0';
	    if (nbufindex + 1 >= maxbufindex) {
		size = (maxbufindex + 4) * sizeof(char *);
		if ((bufindex = (char **)realloc(bufindex, size)) == NULL)
		    return -ENOMEM;
	    	maxbufindex += 4;
	    }
	    bufindex[++nbufindex] = statbuf + i + 1;
	}
    }

    if (!started) {
	started = 1;
	memset(proc_stat, 0, sizeof(*proc_stat));

	/* scan ncpus */
	for (i=0; i < nbufindex; i++) {
	    if (strncmp("cpu", bufindex[i], 3) == 0 && isdigit((int)bufindex[i][3]))
	    	proc_stat->ncpu++;
	}
	if (proc_stat->ncpu == 0)
	    proc_stat->ncpu = 1; /* non-SMP kernel? */
	proc_stat->cpu_indom = idp;
	proc_stat->cpu_indom->it_numinst = proc_stat->ncpu;
	proc_stat->cpu_indom->it_set = (pmdaInstid *)malloc(
		proc_stat->ncpu * sizeof(pmdaInstid));
	/*
	 * Map out the CPU instance domain.
	 *
	 * The first call to cpu_name() does initialization on the
	 * proc_cpuinfo structure.
	 */
	for (i=0; i < proc_stat->ncpu; i++) {
	    proc_stat->cpu_indom->it_set[i].i_inst = i;
	    proc_stat->cpu_indom->it_set[i].i_name = cpu_name(proc_cpuinfo, i);
	}

	n = proc_stat->ncpu * sizeof(unsigned long long);
	proc_stat->p_user = (unsigned long long *)calloc(1, n);
	proc_stat->p_nice = (unsigned long long *)calloc(1, n);
	proc_stat->p_sys = (unsigned long long *)calloc(1, n);
	proc_stat->p_idle = (unsigned long long *)calloc(1, n);
	proc_stat->p_wait = (unsigned long long *)calloc(1, n);
	proc_stat->p_irq = (unsigned long long *)calloc(1, n);
	proc_stat->p_sirq = (unsigned long long *)calloc(1, n);
	proc_stat->p_steal = (unsigned long long *)calloc(1, n);
	proc_stat->p_guest = (unsigned long long *)calloc(1, n);
	proc_stat->p_guest_nice = (unsigned long long *)calloc(1, n);

	n = proc_cpuinfo->node_indom->it_numinst * sizeof(unsigned long long);
	proc_stat->n_user = calloc(1, n);
	proc_stat->n_nice = calloc(1, n);
	proc_stat->n_sys = calloc(1, n);
	proc_stat->n_idle = calloc(1, n);
	proc_stat->n_wait = calloc(1, n);
	proc_stat->n_irq = calloc(1, n);
	proc_stat->n_sirq = calloc(1, n);
	proc_stat->n_steal = calloc(1, n);
	proc_stat->n_guest = calloc(1, n);
	proc_stat->n_guest_nice = calloc(1, n);
    }
    else {
	/* reset per-node stats */
	n = proc_cpuinfo->node_indom->it_numinst * sizeof(unsigned long long);
	memset(proc_stat->n_user, 0, n);
	memset(proc_stat->n_nice, 0, n);
	memset(proc_stat->n_sys, 0, n);
	memset(proc_stat->n_idle, 0, n);
	memset(proc_stat->n_wait, 0, n);
	memset(proc_stat->n_irq, 0, n);
	memset(proc_stat->n_sirq, 0, n);
	memset(proc_stat->n_steal, 0, n);
	memset(proc_stat->n_guest, 0, n);
	memset(proc_stat->n_guest_nice, 0, n);
    }
    /*
     * cpu  95379 4 20053 6502503
     * 2.6 kernels have 3 additional fields
     * for wait, irq and soft_irq.
     */
    strcpy(fmt, "cpu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu");
    n = sscanf((const char *)bufindex[0], fmt,
	&proc_stat->user, &proc_stat->nice,
	&proc_stat->sys, &proc_stat->idle,
	&proc_stat->wait, &proc_stat->irq,
	&proc_stat->sirq, &proc_stat->steal,
	&proc_stat->guest, &proc_stat->guest_nice);

    /*
     * per-cpu stats
     * e.g. cpu0 95379 4 20053 6502503
     * 2.6 kernels have 3 additional fields for wait, irq and soft_irq.
     * More recent (2008) 2.6 kernels have an extra field for guest and
     * also (since 2009) guest_nice.
     */
    if (proc_stat->ncpu == 1) {
	/*
	 * Don't bother scanning - the per-cpu and per-node counters are the
         * same as for "all" cpus, as already scanned above.
	 * This also handles the non-SMP code where
	 * there is no line starting with "cpu0".
	 */
	proc_stat->p_user[0] = proc_stat->n_user[0] = proc_stat->user;
	proc_stat->p_nice[0] = proc_stat->n_nice[0] = proc_stat->nice;
	proc_stat->p_sys[0] = proc_stat->n_sys[0] = proc_stat->sys;
	proc_stat->p_idle[0] = proc_stat->n_idle[0] = proc_stat->idle;
	proc_stat->p_wait[0] = proc_stat->n_wait[0] = proc_stat->wait;
	proc_stat->p_irq[0] = proc_stat->n_irq[0] = proc_stat->irq;
	proc_stat->p_sirq[0] = proc_stat->n_sirq[0] = proc_stat->sirq;
	proc_stat->p_steal[0] = proc_stat->n_steal[0] = proc_stat->steal;
    	proc_stat->p_guest[0] = proc_stat->n_guest[0] = proc_stat->guest;
    	proc_stat->p_guest_nice[0] = proc_stat->n_guest_nice[0] = proc_stat->guest_nice;
    }
    else {
	strcpy(fmt, "cpu%d %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu");
	for (i=0; i < proc_stat->ncpu; i++) {
	    for (j=0; j < nbufindex; j++) {
		if (strncmp("cpu", bufindex[j], 3) == 0 && isdigit((int)bufindex[j][3])) {
		    int c;
		    int cpunum = atoi(&bufindex[j][3]);
		    int node;
		    if (cpunum >= 0 && cpunum < proc_stat->ncpu) {
			n = sscanf(bufindex[j], fmt, &c,
			    &proc_stat->p_user[cpunum],
			    &proc_stat->p_nice[cpunum],
			    &proc_stat->p_sys[cpunum],
			    &proc_stat->p_idle[cpunum],
			    &proc_stat->p_wait[cpunum],
			    &proc_stat->p_irq[cpunum],
			    &proc_stat->p_sirq[cpunum],
			    &proc_stat->p_steal[cpunum],
			    &proc_stat->p_guest[cpunum],
			    &proc_stat->p_guest_nice[cpunum]);
			if ((node = proc_cpuinfo->cpuinfo[cpunum].node) != -1) {
			    proc_stat->n_user[node] += proc_stat->p_user[cpunum];
			    proc_stat->n_nice[node] += proc_stat->p_nice[cpunum];
			    proc_stat->n_sys[node] += proc_stat->p_sys[cpunum];
			    proc_stat->n_idle[node] += proc_stat->p_idle[cpunum];
			    proc_stat->n_wait[node] += proc_stat->p_wait[cpunum];
			    proc_stat->n_irq[node] += proc_stat->p_irq[cpunum];
			    proc_stat->n_sirq[node] += proc_stat->p_sirq[cpunum];
			    proc_stat->n_steal[node] += proc_stat->p_steal[cpunum];
			    proc_stat->n_guest[node] += proc_stat->p_guest[cpunum];
			    proc_stat->n_guest_nice[node] += proc_stat->p_guest_nice[cpunum];
			}
		    }
		}
	    }
	    if (j == nbufindex)
		break;
	}
    }

    /*
     * page 59739 34786
     * Note: this has moved to /proc/vmstat in 2.6 kernels
     */
    strcpy(fmt, "page %u %u");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt,
		&proc_stat->page[0], &proc_stat->page[1]);
	    break;
	}
    }

    /*
     * swap 0 1
     * Note: this has moved to /proc/vmstat in 2.6 kernels
     */
    strcpy(fmt, "swap %u %u");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt,
		&proc_stat->swap[0], &proc_stat->swap[1]);
	    break;
	}
    }

    /*
     * intr 32845463 24099228 2049 0 2 ....
     * (just export the first number, which is total interrupts)
     */
    strcpy(fmt, "intr %llu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->intr);
	    break;
	}
    }

    /*
     * ctxt 1733480
     */
    strcpy(fmt, "ctxt %llu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 5) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->ctxt);
	    break;
	}
    }

    /*
     * btime 1733480
     */
    strcpy(fmt, "btime %lu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 6) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->btime);
	    break;
	}
    }

    /*
     * processes 2213
     */
    strcpy(fmt, "processes %lu");
    for (j=0; j < nbufindex; j++) {
    	if (strncmp(fmt, bufindex[j], 10) == 0) {
	    sscanf((const char *)bufindex[j], fmt, &proc_stat->processes);
	    break;
	}
    }

    /* success */
    return 0;
}
