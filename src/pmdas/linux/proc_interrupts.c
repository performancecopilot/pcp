/*
 * Copyright (c) 2012-2014,2016,2019-2021 Red Hat.
 * Copyright (c) 2011 Aconex.  All Rights Reserved.
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
#include "filesys.h"
#include "proc_interrupts.h"
#include <sys/stat.h>
#include <ctype.h>

static char *iobuf;			/* buffer sized based on CPU count */
static unsigned int iobufsz;

static online_cpu_t *online_cpumap;	/* maps input columns to CPU info */
unsigned int irq_err_count;
unsigned int irq_mis_count;

/*
 * One-shot initialisation for global interrupt-metric-related state
 */
static void
setup_buffers(void)
{
    static int setup;

    if (!setup) {
	if ((iobufsz = (_pm_ncpus * 64)) < BUFSIZ)
	    iobufsz = BUFSIZ;
	if ((iobuf = malloc(iobufsz)) == NULL)
	    return;
	online_cpumap = calloc(_pm_ncpus, sizeof(online_cpu_t));
	if (!online_cpumap) {
	    free(iobuf);
	    return;
	}
	setup = 1;
    }
}

/*
 * Create mapping of numeric CPU identifiers to column numbers
 */
static int
map_online_cpus(char *buffer)
{
    unsigned int i = 0, cpuid;
    char *s, *end;

    for (s = buffer; i < _pm_ncpus && *s != '\0'; s++) {
	if (!isdigit((int)*s))
	    continue;
	cpuid = (unsigned int)strtoul(s, &end, 10);
	if (end == s)
	    break;
	online_cpumap[i++].cpuid = cpuid;
	s = end;
    }
    return i;
}

/*
 * Lookup numeric CPU identifiers based on column number
 */
static int
column_to_cpuid(int column)
{
    int i;

    if (online_cpumap[column].cpuid == column)
	return column;
    for (i = 0; i < _pm_ncpus; i++)
	if (online_cpumap[i].cpuid == column)
	    return i;
    return 0;
}

/*
 * Create descriptive label value - remove duplicates and end-of-line marker
 */
static char *
label_reformat(char *buf)
{
    char *result, *start, *end;

    /* position end marker, and skip over whitespace at the start */
    for (start = end = buf; *end != '\n' && *end != '\0'; end++)
	if (isspace((int)*start) && isspace((int)*end))
	    start = end+1;
    *end = '\0';

    /* squash duplicate whitespace and remove trailing whitespace */
    for (result = start; *result != '\0'; result++) {
	if (isspace((int)result[0]) && (isspace((int)result[1]) || result[1] == '\0')) {
	    memmove(&result[0], &result[1], end - &result[0]);
	    result--;
	}
    }
    return start;
}

static char *
extract_interrupt_name(char *buffer, char **suffix)
{
    char *s = buffer, *end, *prev;

    while (isspace((int)*s))		/* find start of name */
	s++;
    for (end = s; *end && !isspace((int)*end); end++);	/* find end */
    prev = end - 1;
    if (*prev == '_' || *prev == ':')	/* overwrite final non-name char */
	end--;				/* and then move end of name */
    *end = '\0';			/* mark end of name */
    *suffix = end + 1;			/* mark values start */
    return s;
}

static int
extract_interrupt_errors(char *buffer)
{
    return (sscanf(buffer, "ERR: %u", &irq_err_count) == 1 ||
	    sscanf(buffer, "Err: %u", &irq_err_count) == 1  ||
	    sscanf(buffer, "BAD: %u", &irq_err_count) == 1);
}

static int
extract_interrupt_misses(char *buffer)
{
    return sscanf(buffer, "MIS: %u", &irq_mis_count) == 1;
}

static int
extract_interrupt_values(char *name, char *buffer, pmInDom intr, pmInDom cpuintr, int ncolumns)
{
    unsigned long i, cpuid, value;
    char *s = buffer, *end = NULL;
    char cpubuf[64];
    interrupt_cpu_t *cpuip;
    interrupt_t *ip = NULL;
    int sts, changed = 0;

    sts = pmdaCacheLookupName(intr, name, NULL, (void **)&ip);
    if (sts < 0 || ip == NULL) {
	if ((ip = calloc(1, sizeof(interrupt_t))) == NULL)
	    return 0;
	changed = 1;
    }

    ip->total = 0;
    for (i = 0; i < ncolumns; i++) {
	value = strtoul(s, &end, 10);
	if (!isspace(*end))
	    continue;
	s = end;
	cpuip = NULL;
	cpuid = column_to_cpuid(i);
	online_cpumap[cpuid].intr_count += value;
	pmsprintf(cpubuf, sizeof cpubuf, "%s::cpu%lu", name, cpuid);
	sts = pmdaCacheLookupName(cpuintr, cpubuf, NULL, (void **)&cpuip);
	if (sts < 0 || cpuip == NULL) {
	    if ((cpuip = calloc(1, sizeof(interrupt_cpu_t))) == NULL)
	        continue;
	    cpuip->row = ip;
	}
	cpuip->cpuid = cpuid;
	cpuip->value = value;
	ip->total += value;

	pmdaCacheStore(cpuintr, PMDA_CACHE_ADD, cpubuf, cpuip);
    }
    pmdaCacheStore(intr, PMDA_CACHE_ADD, name, ip);

    if (ip->label == NULL)
	ip->label = end ? strdup(label_reformat(end)) : NULL;

    return changed;
}

int
refresh_proc_interrupts(void)
{
    static int setup;
    FILE *fp;
    char *name, *values;
    int i, save, ncolumns;
    pmInDom intr_indom = INDOM(INTERRUPT_INDOM);
    pmInDom cpu_intr_indom = INDOM(INTERRUPT_CPU_INDOM);

    if (!setup) {
	pmdaCacheOp(cpu_intr_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(intr_indom, PMDA_CACHE_LOAD);
	setup = 1;
    }
    pmdaCacheOp(cpu_intr_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(intr_indom, PMDA_CACHE_INACTIVE);

    setup_buffers();
    for (i = 0; i < _pm_ncpus; i++)
	online_cpumap[i].intr_count = 0;

    if ((fp = linux_statsfile("/proc/interrupts", iobuf, iobufsz)) == NULL)
	return -oserror();

    /* first parse header, which maps online CPU number to column number */
    if (fgets(iobuf, iobufsz, fp)) {
	ncolumns = map_online_cpus(iobuf);
    } else {
	fclose(fp);
	return -EINVAL;		/* unrecognised file format */
    }

    save = 0;
    while (fgets(iobuf, iobufsz, fp) != NULL) {
	/* extract interrupt line (or other) and values from each row */
	if (extract_interrupt_errors(iobuf))
	    continue;
	if (extract_interrupt_misses(iobuf))
	    continue;
	name = extract_interrupt_name(iobuf, &values);
	save |= extract_interrupt_values(name, values, intr_indom, cpu_intr_indom, ncolumns);
    }
    fclose(fp);

    if (save) {
	pmdaCacheOp(cpu_intr_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(intr_indom, PMDA_CACHE_SAVE);
    }
    return 0;
}

static int
extract_softirq_values(char *name, char *buffer, pmInDom sirq, pmInDom cpusirq, int ncolumns)
{
    unsigned long i, cpuid, value;
    char *s = buffer, *end = NULL;
    char cpubuf[64];
    interrupt_cpu_t *cpuip;
    interrupt_t *ip = NULL;
    int sts, changed = 0;

    sts = pmdaCacheLookupName(sirq, name, NULL, (void **)&ip);
    if (sts < 0 || ip == NULL) {
	if ((ip = calloc(1, sizeof(interrupt_t))) == NULL)
	    return 0;
	changed = 1;
    }

    ip->total = 0;
    for (i = 0; i < ncolumns; i++) {
	value = strtoul(s, &end, 10);
	if (!isspace(*end))
	    continue;
	s = end;
	cpuip = NULL;
	cpuid = column_to_cpuid(i);
	online_cpumap[cpuid].sirq_count += value;
	pmsprintf(cpubuf, sizeof cpubuf, "%s::cpu%lu", name, cpuid);
	sts = pmdaCacheLookupName(cpusirq, cpubuf, NULL, (void **)&cpuip);
	if (sts < 0 || cpuip == NULL) {
	    if ((cpuip = calloc(1, sizeof(interrupt_cpu_t))) == NULL)
	        continue;
	    cpuip->row = ip;
	}
	cpuip->cpuid = cpuid;
	cpuip->value = value;
	ip->total += value;

	pmdaCacheStore(cpusirq, PMDA_CACHE_ADD, cpubuf, cpuip);
    }
    pmdaCacheStore(sirq, PMDA_CACHE_ADD, name, ip);

    if (ip->label == NULL)
	ip->label = end ? strdup(label_reformat(end)) : NULL;

    return changed;
}

int
refresh_proc_softirqs(void)
{
    static int setup;
    FILE *fp;
    char *name, *values;
    int i = 0, save, ncolumns;
    pmInDom sirq_indom = INDOM(SOFTIRQ_INDOM);
    pmInDom cpu_sirq_indom = INDOM(SOFTIRQ_CPU_INDOM);

    if (!setup) {
	pmdaCacheOp(cpu_sirq_indom, PMDA_CACHE_LOAD);
	pmdaCacheOp(sirq_indom, PMDA_CACHE_LOAD);
	setup = 1;
    }
    pmdaCacheOp(cpu_sirq_indom, PMDA_CACHE_INACTIVE);
    pmdaCacheOp(sirq_indom, PMDA_CACHE_INACTIVE);

    setup_buffers();
    for (i = 0; i < _pm_ncpus; i++)
	online_cpumap[i].sirq_count = 0;

    if ((fp = linux_statsfile("/proc/softirqs", iobuf, iobufsz)) == NULL)
	return -oserror();

    /* first parse header, which maps online CPU number to column number */
    if (fgets(iobuf, iobufsz, fp)) {
	ncolumns = map_online_cpus(iobuf);
    } else {
	fclose(fp);
	return -EINVAL;		/* unrecognised file format */
    }

    save = 0;
    while (fgets(iobuf, iobufsz, fp) != NULL) {
	/* extract values from all subsequent softirqs file lines */
	name = extract_interrupt_name(iobuf, &values);
	save |= extract_softirq_values(name, values, sirq_indom, cpu_sirq_indom, ncolumns);
    }
    fclose(fp);

    if (save) {
	pmdaCacheOp(cpu_sirq_indom, PMDA_CACHE_SAVE);
	pmdaCacheOp(sirq_indom, PMDA_CACHE_SAVE);
    }
    return 0;
}

int
proc_interrupts_fetch(int cluster, int item, unsigned int inst, pmAtomValue *atom)
{
    interrupt_cpu_t *cpuip;
    interrupt_t *ip;
    pmInDom indom;
    int cpuid, sts;

    switch (cluster) {
    case CLUSTER_INTERRUPTS:
	if (item == 0) {	/* kernel.all.interrupts.total */
	    indom = INDOM(INTERRUPT_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&ip);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;
	    atom->ull = ip->total;
	    return 1;
	}
	if (item == 1) {	/* kernel.percpu.interrupts */
	    indom = INDOM(INTERRUPT_CPU_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&cpuip);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;
	    atom->ul = cpuip->value;
	    return 1;
	}
	if (item == 4) {	/* kernel.percpu.intr */
	    if (inst >= _pm_ncpus)
		return PM_ERR_INST;
	    cpuid = column_to_cpuid(inst);
	    atom->ull = online_cpumap[cpuid].intr_count;
	    return 1;
	}
	break;

    case CLUSTER_SOFTIRQS_TOTAL:
	if (item == 0) {	/* kernel.all.softirqs.total */
	    indom = INDOM(SOFTIRQ_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&ip);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;
	    atom->ull = ip->total;
	    return 1;
	}
	break;

    case CLUSTER_SOFTIRQS:
	if (item == 0) {	/* kernel.percpu.softirqs */
	    indom = INDOM(SOFTIRQ_CPU_INDOM);
	    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&cpuip);
	    if (sts < 0)
		return sts;
	    if (sts != PMDA_CACHE_ACTIVE)
	    	return PM_ERR_INST;
	    atom->ul = cpuip->value;
	    return 1;
	}
	if (item == 1) {	/* kernel.percpu.sirq */
	    if (inst >= _pm_ncpus)
		return PM_ERR_INST;
	    cpuid = column_to_cpuid(inst);
	    atom->ull = online_cpumap[cpuid].sirq_count;
	    return 1;
	}
	break;

    default:
	break;
    }
    return PM_ERR_PMID;
}
