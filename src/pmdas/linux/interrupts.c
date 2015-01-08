/*
 * Copyright (c) 2012-2014 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "filesys.h"
#include "clusters.h"
#include "interrupts.h"
#include <sys/stat.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>

typedef struct {
    unsigned int	id;		/* becomes PMID item number */
    char		*name;		/* becomes PMNS sub-component */
    char		*text;		/* one-line metric help text */
    unsigned long long	*values;	/* per-CPU values for this counter */
} interrupt_t;

static unsigned int cpu_count;
static int *online_cpumap;		/* maps input columns to CPU IDs */
static unsigned int lines_count;
static interrupt_t *interrupt_lines;
static unsigned int other_count;
static interrupt_t *interrupt_other;

static __pmnsTree *interrupt_tree;
unsigned int irq_err_count;

static void
update_lines_pmns(int domain, unsigned int item, unsigned int id)
{
    char entry[128];
    pmID pmid = pmid_build(domain, CLUSTER_INTERRUPT_LINES, item);

    snprintf(entry, sizeof(entry), "kernel.percpu.interrupts.line%d", id);
    __pmAddPMNSNode(interrupt_tree, pmid, entry);
}

static void
update_other_pmns(int domain, unsigned int item, const char *name)
{
    char entry[128];
    pmID pmid = pmid_build(domain, CLUSTER_INTERRUPT_OTHER, item);

    snprintf(entry, sizeof(entry), "kernel.percpu.interrupts.%s", name);
    __pmAddPMNSNode(interrupt_tree, pmid, entry);
}

static int
map_online_cpus(char *buffer)
{
    unsigned long i = 0, cpuid;
    char *s, *end;

    for (s = buffer; *s != '\0'; s++) {
	if (!isdigit((int)*s))
	    continue;
	cpuid = strtoul(s, &end, 10);
	if (end == s)
	    break;
	online_cpumap[i++] = cpuid;
	s = end;
    }
    return i;
}

static int
column_to_cpuid(int column)
{
    int i;

    if (online_cpumap[column] == column)
	return column;
    for (i = 0; i < cpu_count; i++)
	if (online_cpumap[i] == column)
	    return i;
    return 0;
}

static char *
extract_values(char *buffer, unsigned long long *values, int ncolumns)
{
    unsigned long i, value, cpuid;
    char *s = buffer, *end = NULL;

    for (i = 0; i < ncolumns; i++) {
	value = strtoul(s, &end, 10);
	if (*end != ' ')
	    return NULL;
	s = end;
	cpuid = column_to_cpuid(i);
	values[cpuid] = value;
    }
    return end;
}

/* Create oneline help text - remove duplicates and end-of-line marker */
static char *
oneline_reformat(char *buf)
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

static void
initialise_interrupt(interrupt_t *ip, unsigned int id, char *s, char *end)
{
    ip->id = id;
    ip->name = strdup(s);
    if (end)
	ip->text = strdup(oneline_reformat(end));
}

static int
extend_interrupts(interrupt_t **interp, unsigned int *countp)
{
    int cnt = cpu_count * sizeof(unsigned long long);
    unsigned long long *values = malloc(cnt);
    interrupt_t *interrupt = *interp;
    int count = *countp + 1;

    if (!values)
	return 0;

    interrupt = realloc(interrupt, count * sizeof(interrupt_t));
    if (!interrupt) {
	free(values);
	return 0;
    }
    interrupt[count-1].values = values;
    *interp = interrupt;
    *countp = count;
    return 1;
}

static char *
extract_interrupt_name(char *buffer, char **suffix)
{
    char *s = buffer, *end;

    while (isspace((int)*s))		/* find start of name */
	s++;
    for (end = s; *end && isalnum((int)*end); end++) { }
    *end = '\0';		/* mark end of name */
    *suffix = end + 1;		/* mark values start */
    return s;
}

static int
extract_interrupt_lines(char *buffer, int ncolumns, int nlines)
{
    unsigned long id;
    char *name, *end, *values;
    int resize = (nlines >= lines_count);

    name = extract_interrupt_name(buffer, &values);
    id = strtoul(name, &end, 10);
    if (*end != '\0')
	return 0;
    if (resize && !extend_interrupts(&interrupt_lines, &lines_count))
	return 0;
    end = extract_values(values, interrupt_lines[nlines].values, ncolumns);
    if (resize)
	initialise_interrupt(&interrupt_lines[nlines], id, name, end);
    return 1;
}

static int
extract_interrupt_errors(char *buffer)
{
    return (sscanf(buffer, " ERR: %u", &irq_err_count) == 1 ||
	    sscanf(buffer, "Err: %u", &irq_err_count) == 1  ||
	    sscanf(buffer, "BAD: %u", &irq_err_count) == 1);
}

static int
extract_interrupt_misses(char *buffer)
{
    unsigned int irq_mis_count;	/* not exported */
    return sscanf(buffer, " MIS: %u", &irq_mis_count) == 1;
}

static int
extract_interrupt_other(char *buffer, int ncolumns, int nlines)
{
    char *name, *end, *values;
    int resize = (nlines >= other_count);

    name = extract_interrupt_name(buffer, &values);
    if (resize && !extend_interrupts(&interrupt_other, &other_count))
	return 0;
    end = extract_values(values, interrupt_other[nlines].values, ncolumns);
    if (resize)
	initialise_interrupt(&interrupt_other[nlines], nlines, name, end);
    return 1;
}

int
refresh_interrupt_values(void)
{
    FILE *fp;
    char buf[4096];
    int i, ncolumns;

    if (cpu_count == 0) {
	long ncpus = sysconf(_SC_NPROCESSORS_CONF);
	online_cpumap = malloc(ncpus * sizeof(int));
	if (!online_cpumap)
	    return -oserror();
	cpu_count = ncpus;
    }
    memset(online_cpumap, 0, cpu_count * sizeof(int));

    if ((fp = linux_statsfile("/proc/interrupts", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* first parse header, which maps online CPU number to column number */
    if (fgets(buf, sizeof(buf), fp)) {
	ncolumns = map_online_cpus(buf);
    } else {
	fclose(fp);
	return -EINVAL;		/* unrecognised file format */
    }

    /* next we parse each interrupt line row (starting with a digit) */
    i = 0;
    while (fgets(buf, sizeof(buf), fp))
	if (!extract_interrupt_lines(buf, ncolumns, i++))
	    break;

    /* parse other per-CPU interrupt counter rows (starts non-digit) */
    i = 0;
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (extract_interrupt_errors(buf))
	    continue;
	if (extract_interrupt_misses(buf))
	    continue;
	if (!extract_interrupt_other(buf, ncolumns, i++))
	    break;
    }

    fclose(fp);
    return 0;
}

static int
refresh_interrupts(pmdaExt *pmda, __pmnsTree **tree)
{
    int i, sts, dom = pmda->e_domain;

    if (interrupt_tree) {
	*tree = interrupt_tree;
    } else if ((sts = __pmNewPMNS(&interrupt_tree)) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to create interrupt names: %s\n",
			pmProgname, pmErrStr(sts));
	*tree = NULL;
    } else if ((sts = refresh_interrupt_values()) < 0) {
	__pmNotifyErr(LOG_ERR, "%s: failed to update interrupt values: %s\n",
			pmProgname, pmErrStr(sts));
	*tree = NULL;
    } else {
	for (i = 0; i < lines_count; i++)
	    update_lines_pmns(dom, i, interrupt_lines[i].id);
	for (i = 0; i < other_count; i++)
	    update_other_pmns(dom, i, interrupt_other[i].name);
	*tree = interrupt_tree;
	pmdaTreeRebuildHash( interrupt_tree, lines_count+other_count );
	return 1;
    }
    return 0;
}

int
interrupts_fetch(int cluster, int item, unsigned int inst, pmAtomValue *atom)
{
    if (inst >= cpu_count)
	return PM_ERR_INST;

    switch (cluster) {
	case CLUSTER_INTERRUPT_LINES:
	    if (item > lines_count)
		return PM_ERR_PMID;
	    atom->ull = interrupt_lines[item].values[inst];
	    return 1;
	case CLUSTER_INTERRUPT_OTHER:
	    if (item > other_count)
		return PM_ERR_PMID;
	    atom->ull = interrupt_other[item].values[inst];
	    return 1;
    }
    return PM_ERR_PMID;
}

/*
 * Create a new metric table entry based on an existing one.
 */
static void
refresh_metrictable(pmdaMetric *source, pmdaMetric *dest, int id)
{
    int domain = pmid_domain(source->m_desc.pmid);
    int cluster = pmid_cluster(source->m_desc.pmid);

    memcpy(dest, source, sizeof(pmdaMetric));
    dest->m_desc.pmid = pmid_build(domain, cluster, id);

    if (pmDebug & DBG_TRACE_LIBPMDA)
	fprintf(stderr, "interrupts refresh_metrictable: (%p -> %p) "
			"metric ID dup: %d.%d.%d -> %d.%d.%d\n",
		source, dest, domain, cluster,
		pmid_item(source->m_desc.pmid), domain, cluster, id);
}

/*
 * Needs to answer the question: how much extra space needs to be
 * allocated in the metric table for (dynamic) interrupt metrics"?  
 * Return value is the number of additional entries/trees needed.
 */
static void
size_metrictable(int *total, int *trees)
{
    *total = 2;	/* lines and other */
    *trees = lines_count > other_count ? lines_count : other_count;

    if (pmDebug & DBG_TRACE_LIBPMDA)
	fprintf(stderr, "interrupts size_metrictable: %d total x %d trees\n",
		*total, *trees);
}

static int
interrupts_text(pmdaExt *pmda, pmID pmid, int type, char **buf)
{
    int item = pmid_item(pmid);
    int cluster = pmid_cluster(pmid);

    switch (cluster) {
	case CLUSTER_INTERRUPT_LINES:
	    if (item > lines_count)
		return PM_ERR_PMID;
	    if (interrupt_lines[item].text == NULL)
		return PM_ERR_TEXT;
	    *buf = interrupt_lines[item].text;
	    return 0;
	case CLUSTER_INTERRUPT_OTHER:
	    if (item > other_count)
		return PM_ERR_PMID;
	    if (interrupt_other[item].text == NULL)
		return PM_ERR_TEXT;
	    *buf = interrupt_other[item].text;
	    return 0;
    }
    return PM_ERR_PMID;
}

void
interrupts_init(pmdaMetric *metrictable, int nmetrics)
{
    int set[] = { CLUSTER_INTERRUPT_LINES, CLUSTER_INTERRUPT_OTHER };

    pmdaDynamicPMNS("kernel.percpu.interrupts",
		    set, sizeof(set)/sizeof(int),
		    refresh_interrupts, interrupts_text,
		    refresh_metrictable, size_metrictable,
		    metrictable, nmetrics);
}
