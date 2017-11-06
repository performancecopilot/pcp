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

static perfstat_disk_total_t	diskstat;
static int			fetched;

void
disk_total_init(int first)
{
    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;
}

void
disk_total_prefetch(void)
{
    int		i;

    fetched = 0;
}

static __uint64_t
disk_total_derived(pmdaMetric *mdesc, int inst)
{
    pmID        pmid;
    __uint64_t  val;
                                                                                
    pmid = mdesc->m_desc.pmid;
    pmID_build(0, pmID_cluster(pmid), pmID_item(pmid));

    switch (pmid) {
	case PMDA_PMID(0,49):	/* disk.all.total_bytes */
	    val = diskstat.rblks + diskstat.wblks;
	    break;

	default:
	    fprintf(stderr, "disk_total_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

    if (pmDebugOptions.appl0 && pmDebugOptions.desperate) {
	/* desperate */
	fprintf(stderr, "disk_total_derived: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }

    return val;
}

int
disk_total_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    int			offset;

    if (fetched == 0) {
	int	sts;
	sts = perfstat_disk_total(NULL, &diskstat, sizeof(perfstat_disk_total_t), 1);
	if (sts != 1) {
	    /* TODO - how to find/decode errors? */
	    fprintf(stderr, "perfstat_disk_total: failed %s\n", osstrerror());
	    fetched = -1;
	}
	else
	    fetched = 1;
    }

    if (fetched != 1)
	return 0;

    offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
    if (offset == OFF_NOVALUES)
	return 0;

    if (mdesc->m_desc.type == PM_TYPE_U64) {
	if (offset == OFF_DERIVED)
	    atom->ull = disk_total_derived(mdesc, inst);
	else {
	    __uint64_t		*ullp;
	    ullp = (__uint64_t *)&((char *)&diskstat)[offset];
	    atom->ull = *ullp;
	}
	if (pmDebugOptions.appl0 && pmDebugOptions.desperate) {
	    /* desperate */
	    fprintf(stderr, "disk_total_fetch: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ull);
	}
    }
    else {
	if (offset == OFF_DERIVED)
	    atom->ul = (__uint32_t)disk_total_derived(mdesc, inst);
	else {
	    __uint32_t		*ulp;
	    ulp = (__uint32_t *)&((char *)&diskstat)[offset];
	    atom->ul = *ulp;
	}
	if (pmDebugOptions.appl0 && pmDebugOptions.desperate) {
	    /* desperate */
	    fprintf(stderr, "disk_total_fetch: pmid %s inst %d val %lu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ul);
	}
    }

    return 1;
}
