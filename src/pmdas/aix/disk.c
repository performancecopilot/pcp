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

static int		ndisk;
static int		ndisk_alloc;
static int		*fetched;
static perfstat_disk_t	*diskstat;

void
disk_init(int first)
{
    perfstat_id_t	id;
    int			i;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    ndisk =  perfstat_disk(NULL, NULL, sizeof(perfstat_disk_t), 0);
    if ((fetched = (int *)malloc(ndisk * sizeof(int))) == NULL) {
	fprintf(stderr, "disk_init: fetched malloc[%d] failed: %s\n",
	    ndisk * sizeof(int), osstrerror());
	exit(1);
    }
    if ((diskstat = (perfstat_disk_t *)malloc(ndisk * sizeof(perfstat_disk_t))) == NULL) {
	fprintf(stderr, "disk_init: diskstat malloc[%d] failed: %s\n",
	    ndisk * sizeof(perfstat_disk_t), osstrerror());
	exit(1);
    }
    ndisk_alloc = ndisk;

    /*
     * set up instance domain
     */
    strcpy(id.name, "");
    ndisk = perfstat_disk(&id, diskstat, sizeof(perfstat_disk_t), ndisk_alloc);

    indomtab[DISK_INDOM].it_numinst = ndisk;
    indomtab[DISK_INDOM].it_set = (pmdaInstid *)malloc(ndisk * sizeof(pmdaInstid));
    if (indomtab[DISK_INDOM].it_set == NULL) {
	fprintf(stderr, "disk_init: indomtab malloc[%d] failed: %s\n",
	    ndisk * sizeof(pmdaInstid), osstrerror());
	exit(1);
    }
    for (i = 0; i < ndisk; i++) {
	indomtab[DISK_INDOM].it_set[i].i_inst = i;
	indomtab[DISK_INDOM].it_set[i].i_name = strdup(diskstat[i].name);
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "disk_init: ndisk=%d\n", ndisk);
    }
#endif
}

void
disk_prefetch(void)
{
    int		i;

    for (i = 0; i < ndisk_alloc; i++)
	fetched[i] = 0;
}

static __uint64_t
disk_derived(pmdaMetric *mdesc, int inst)
{
    pmID        pmid;
    __pmID_int  *ip = (__pmID_int *)&pmid;
    __uint64_t  val;
                                                                                
    pmid = mdesc->m_desc.pmid;
    ip->domain = 0;

    switch (pmid) {
	case PMDA_PMID(0,55):	/* disk.dev.total_bytes */
	    val = diskstat[inst].rblks + diskstat[inst].wblks;
	    break;

	default:
	    fprintf(stderr, "disk_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "disk_derived: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }
#endif

    return val;
}

int
disk_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    int			offset;

    if (fetched[inst] == 0) {
	int		sts;
	int		i;
	perfstat_id_t	id;

	strcpy(id.name, "");
	sts = perfstat_disk(&id, diskstat, sizeof(perfstat_disk_t), ndisk_alloc);

	/* TODO ...
	 * - if sts != ndisk, the number of disks has changed, need to set
	 *   fetched[i] to -1 for the missing ones
	 * - is sts > ndisk possible?  worse, if the number of disks is >
	 *   ndisk_alloc what should we do?
	 * - possibly reshape the instance domain?
	 * - error handling?
	 */
	
	for (i = 0; i < ndisk; i++) {
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
	    atom->ull = disk_derived(mdesc, inst);
	else {
	    __uint64_t		*ullp;
	    ullp = (__uint64_t *)&((char *)&diskstat[inst])[offset];
	    atom->ull = *ullp;
	}
	if (mdesc->m_desc.units.scaleTime == PM_TIME_MSEC) {
	    /* assumed to be CPU time */
	    atom->ull *= 1000 / HZ;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "disk_fetch: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ull);
	}
#endif
    }
    else {
	if (offset == OFF_DERIVED)
	    atom->ul = (__uint32_t)disk_derived(mdesc, inst);
	else {
	    __uint32_t		*ulp;
	    ulp = (__uint32_t *)&((char *)&diskstat[inst])[offset];
	    atom->ul = *ulp;
	}
	if (mdesc->m_desc.units.scaleTime == PM_TIME_MSEC) {
	    /* assumed to be CPU time */
	    atom->ul *= 1000 / HZ;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "disk_fetch: pmid %s inst %d val %lu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ul);
	}
#endif
    }

    return 1;
}
