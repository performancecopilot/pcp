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

static int		nnetif;
static int		nnetif_alloc;
static int		*fetched;
static perfstat_netinterface_t	*netifstat;

void
netif_init(int first)
{
    perfstat_id_t	id;
    int			i;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    nnetif =  perfstat_netinterface(NULL, NULL, sizeof(perfstat_netinterface_t), 0);
    if ((fetched = (int *)malloc(nnetif * sizeof(int))) == NULL) {
	fprintf(stderr, "netif_init: fetched malloc[%d] failed: %s\n",
	    nnetif * sizeof(int), osstrerror());
	exit(1);
    }
    if ((netifstat = (perfstat_netinterface_t *)malloc(nnetif * sizeof(perfstat_netinterface_t))) == NULL) {
	fprintf(stderr, "netif_init: netifstat malloc[%d] failed: %s\n",
	    nnetif * sizeof(perfstat_netinterface_t), osstrerror());
	exit(1);
    }
    nnetif_alloc = nnetif;

    /*
     * set up instance domain
     */
    strcpy(id.name, "");
    nnetif = perfstat_netinterface(&id, netifstat, sizeof(perfstat_netinterface_t), nnetif_alloc);

    indomtab[NETIF_INDOM].it_numinst = nnetif;
    indomtab[NETIF_INDOM].it_set = (pmdaInstid *)malloc(nnetif * sizeof(pmdaInstid));
    if (indomtab[NETIF_INDOM].it_set == NULL) {
	fprintf(stderr, "netif_init: indomtab malloc[%d] failed: %s\n",
	    nnetif * sizeof(pmdaInstid), osstrerror());
	exit(1);
    }
    for (i = 0; i < nnetif; i++) {
	indomtab[NETIF_INDOM].it_set[i].i_inst = i;
	indomtab[NETIF_INDOM].it_set[i].i_name = strdup(netifstat[i].name);
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "netif_init: nnetif=%d\n", nnetif);
    }
#endif
}

void
netif_prefetch(void)
{
    int		i;

    for (i = 0; i < nnetif_alloc; i++)
	fetched[i] = 0;
}

static __uint64_t
netif_derived(pmdaMetric *mdesc, int inst)
{
    pmID        pmid;
    __pmID_int  *ip = (__pmID_int *)&pmid;
    __uint64_t  val;
                                                                                
    pmid = mdesc->m_desc.pmid;
    ip->domain = 0;

    switch (pmid) {
	case PMDA_PMID(0,58):	/* hinv.nnetif */
	    val = nnetif;
	    break;

	case PMDA_PMID(0,65):	/* network.interface.total.packets */
	    val = netifstat[inst].ipackets + netifstat[inst].opackets;
	    break;

	case PMDA_PMID(0,66):	/* network.interface.total.bytes */
	    val = netifstat[inst].ibytes + netifstat[inst].obytes;
	    break;

	default:
	    fprintf(stderr, "netif_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "netif_derived: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }
#endif

    return val;
}

int
netif_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    int			offset;

    if (fetched[inst] == 0) {
	int		sts;
	int		i;
	perfstat_id_t	id;

	strcpy(id.name, "");
	sts = perfstat_netinterface(&id, netifstat, sizeof(perfstat_netinterface_t), nnetif_alloc);

	/* TODO ...
	 * - if sts != nnetif, the number of network interfaces has changed,
	 *    need to set fetched[i] to -1 for the missing ones
	 * - is sts > nnetif possible?  worse, if the number of network
	 *   interfaces is > nnetif_alloc what should we do?
	 * - possibly reshape the instance domain?
	 * - error handling?
	 */
	
	for (i = 0; i < nnetif; i++) {
	    fetched[i] = 1;
	}
    }

    /* hinv.nnetif is a singular metric ... so no "instance" for this one */
    if (inst != PM_IN_NULL && fetched[inst] != 1)
	return 0;

    offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
    if (offset == OFF_NOVALUES)
	return 0;

    if (mdesc->m_desc.type == PM_TYPE_U64) {
	if (offset == OFF_DERIVED)
	    atom->ull = netif_derived(mdesc, inst);
	else {
	    __uint64_t		*ullp;
	    ullp = (__uint64_t *)&((char *)&netifstat[inst])[offset];
	    atom->ull = *ullp;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "netif_fetch: pmid %s inst %d val %llu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ull);
	}
#endif
    }
    else {
	if (offset == OFF_DERIVED)
	    atom->ul = (__uint32_t)netif_derived(mdesc, inst);
	else {
	    __uint32_t		*ulp;
	    ulp = (__uint32_t *)&((char *)&netifstat[inst])[offset];
	    atom->ul = *ulp;
	}
#ifdef PCP_DEBUG
	if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	    /* desperate */
	    fprintf(stderr, "netif_fetch: pmid %s inst %d val %lu\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, atom->ul);
	}
#endif
    }

    return 1;
}
