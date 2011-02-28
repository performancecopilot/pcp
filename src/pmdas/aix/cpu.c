/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "common.h"

static int		ncpu;
static int		ncpu_alloc;
static int		*fetched;
static perfstat_cpu_t	*cpustat;

void
cpu_init(int first)
{
    perfstat_id_t	id;
    int			i;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    ncpu =  perfstat_cpu(NULL, NULL, sizeof(perfstat_cpu_t), 0);
    if ((fetched = (int *)malloc(ncpu * sizeof(int))) == NULL) {
	fprintf(stderr, "cpu_init: fetched malloc[%d] failed: %s\n",
	    ncpu * sizeof(int), osstrerror());
	exit(1);
    }
    if ((cpustat = (perfstat_cpu_t *)malloc(ncpu * sizeof(perfstat_cpu_t))) == NULL) {
	fprintf(stderr, "cpu_init: cpustat malloc[%d] failed: %s\n",
	    ncpu * sizeof(perfstat_cpu_t), osstrerror());
	exit(1);
    }
    ncpu_alloc = ncpu;

    /*
     * set up instance domain
     */
    strcpy(id.name, "");
    ncpu = perfstat_cpu(&id, cpustat, sizeof(perfstat_cpu_t), ncpu_alloc);

    indomtab[CPU_INDOM].it_numinst = ncpu;
    indomtab[CPU_INDOM].it_set = (pmdaInstid *)malloc(ncpu * sizeof(pmdaInstid));
    if (indomtab[CPU_INDOM].it_set == NULL) {
	fprintf(stderr, "cpu_init: indomtab malloc[%d] failed: %s\n",
	    ncpu * sizeof(pmdaInstid), osstrerror());
	exit(1);
    }
    for (i = 0; i < ncpu; i++) {
	indomtab[CPU_INDOM].it_set[i].i_inst = i;
	indomtab[CPU_INDOM].it_set[i].i_name = strdup(cpustat[i].name);
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "cpu_init: ncpu=%d\n", ncpu);
    }
#endif
}

void
cpu_prefetch(void)
{
    int		i;

    for (i = 0; i < ncpu_alloc; i++)
	fetched[i] = 0;
}

static __uint64_t
cpu_derived(pmdaMetric *mdesc, int inst)
{
    pmID        pmid;
    __pmID_int  *ip = (__pmID_int *)&pmid;
    __uint64_t  val;
                                                                                
    pmid = mdesc->m_desc.pmid;
    ip->domain = 0;

    switch (pmid) {

	default:
	    fprintf(stderr, "cpu_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "cpu_derived: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }
#endif

    return val;
}

int
cpu_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    int			offset;

    if (fetched[inst] == 0) {
	int		sts;
	int		i;
	perfstat_id_t	id;

	strcpy(id.name, "");
	sts = perfstat_cpu(&id, cpustat, sizeof(perfstat_cpu_t), ncpu_alloc);

	/* TODO ...
	 * - if sts != ncpu, the number of CPUs has changed, need to set
	 *   fetched[i] to -1 for the missing ones
	 * - is sts > ncpu possible?  worse, if the number of cpus is >
	 *   ncpu_alloc what should we do?
	 * - possibly reshape the instance domain?
	 * - error handling?
	 */
	
	for (i = 0; i < ncpu; i++) {
	    fetched[i] = 1;
	}
    }

    if (fetched[inst] != 1)
	return 0;

    offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
    if (offset == OFF_NOVALUES)
	return 0;

    if (mdesc->m_desc.type == PM_TYPE_U64) {
	if (offset == OFF_DERIVED)
	    atom->ull = cpu_derived(mdesc, inst);
	else {
	    __uint64_t		*ullp;
	    ullp = (__uint64_t *)&((char *)&cpustat[inst])[offset];
	    atom->ull = *ullp;
	}
	if (mdesc->m_desc.units.scaleTime == PM_TIME_MSEC) {
	    /* assumed to be CPU time */
	    atom->ull *= 1000 / HZ;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "cpu_fetch: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ull);
	}
#endif
    }
    else {
	if (offset == OFF_DERIVED)
	    atom->ul = (__uint32_t)cpu_derived(mdesc, inst);
	else {
	    __uint32_t		*ulp;
	    ulp = (__uint32_t *)&((char *)&cpustat[inst])[offset];
	    atom->ul = *ulp;
	}
	if (mdesc->m_desc.units.scaleTime == PM_TIME_MSEC) {
	    /* assumed to be CPU time */
	    atom->ul *= 1000 / HZ;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "cpu_fetch: pmid %s inst %d val %lu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ul);
	}
#endif
    }

    return 1;
}
