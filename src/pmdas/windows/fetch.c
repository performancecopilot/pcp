/*
 * Fetch support for shim.exe.
 *
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "./shim.h"
#include "./domain.h"

/*
 * the real callback to instantiate a value for a single metric-instance
 * pair
 */
static int
shim_fetch(int m, int c, pmAtomValue *atom)
{
    PDH_STATUS  		pdhsts;
    PDH_RAW_COUNTER		raw;
    shm_metric_t		*sp = &shm_metrictab[m];
    shim_metric_t		*pp = &shim_metrictab[m];

    if (sp->m_flags & M_NOVALUES) {
	/*
	 * previously decided there is nothing we can do for this metric,
	 * return "no values"
	 */
	return 0;
    }

    if ((querytab[sp->m_qid].q_flags & Q_COLLECTED) == 0) {
	/*
	 * first metric in this query for this fetch ... refresh values
	 */
	pdhsts = PdhCollectQueryData(querytab[sp->m_qid].q_hdl);
	if (pdhsts != ERROR_SUCCESS) {
	    if ((querytab[sp->m_qid].q_flags & Q_ERR_SEEN) == 0) {
		fprintf(stderr, "fetch: Error: PdhCollectQueryData failed for querytab[%d]: %s\n",
		    sp->m_qid, pdherrstr(pdhsts));
		querytab[sp->m_qid].q_flags |= Q_ERR_SEEN;
	    }
	    return 0;	// no values available
	}
    }

    /*
     * parameter "c" identifies the instance
     */
    pdhsts = PdhGetRawCounterValue(pp->m_ctrs[c].c_hdl, NULL, &raw);
    if (pdhsts != ERROR_SUCCESS) {
	fprintf(stderr, "fetch: Error: PdhGetRawCounterValue failed for metric %s inst %d: %s\n",
	    pmIDStr(sp->m_desc.pmid), pp->m_ctrs[c].c_inst, pdherrstr(pdhsts));
	/* no values for you! */
	return 0;
    }
    switch (pp->m_ctype) {
	/*
	 * see also init.c for Pdh metric semantics
	 */
	case PERF_COUNTER_COUNTER:
	case PERF_COUNTER_RAWCOUNT:
	    /* these counters are only 32-bit */
	    atom->ul = (__uint32_t)raw.FirstValue;
	    break;

	case PERF_100NSEC_TIMER:
	case PERF_PRECISION_100NS_TIMER:
	    /* convert 100nsec units to usec */
	    atom->ull = raw.FirstValue / 10;
	    break;

	case PERF_RAW_FRACTION:
	    /* v1 / v2 as percentage */
	    atom->f = (float)raw.FirstValue / raw.SecondValue;
	    break;

	case PERF_COUNTER_BULK_COUNT:
	case PERF_COUNTER_LARGE_RAWCOUNT:
	default:
	    atom->ull = raw.FirstValue;
    }
    return 1;
}

/*
 * called before each PMDA fetch ... force queries to be refreshed if we
 * are asked for metrics covered by a query, and instantiates values
 */
int
prefetch(int numpmid)
{
    int			i;
    int			m;		// metrictab index
    int			c;		// counter index
    int			sts;
    shm_result_t	*rp;
    pmID		*src;
    pmID		*pmidlist;
    int			numatoms;
    int			delta;

    for (i = 0; i < querytab_sz; i++) {
	querytab[i].q_flags &= ~Q_COLLECTED;
    }

    if ((pmidlist = (pmID *)malloc(numpmid * sizeof(pmID))) == NULL) {
	return PM_ERR_TOOBIG;
    }
    src = (pmID *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];
    memmove(pmidlist, src, numpmid * sizeof(pmID));

    /*
     * find the maxmum number of pmAtomValues we can retrieve values
     * for
     */
    numatoms = 0;
    for (i = 0; i < numpmid; i++) {
	for (m = 0; m < metrictab_sz; m++) {
	    if (pmidlist[i] == shm_metrictab[m].m_desc.pmid) {
		if ((shm_metrictab[m].m_flags & M_NOVALUES) == 0) {
		    numatoms += shim_metrictab[m].m_num_ctrs;
		}
		break;
	    }
	}
    }

    delta = numatoms * sizeof(shm_result_t) - shm->segment[SEG_SCRATCH].elt_size * shm->segment[SEG_SCRATCH].nelt;
    if (delta > 0) {
	memcpy(new_hdr, shm, hdr_size);
	new_hdr->segment[SEG_SCRATCH].nelt = numatoms * sizeof(shm_result_t);
	new_hdr->size += delta;
	shm_reshape(new_hdr);
    }

    numatoms = 0;
    rp = (shm_result_t *)&((char *)shm)[shm->segment[SEG_SCRATCH].base];

    for (i = 0; i < numpmid; i++) {
	for (m = 0; m < metrictab_sz; m++) {
	    if (pmidlist[i] == shm_metrictab[m].m_desc.pmid) {
		if (shm_metrictab[m].m_flags & M_NOVALUES)
		    break;
		for (c = 0; c < shim_metrictab[m].m_num_ctrs; c++) {
		    sts = shim_fetch(m, c, &rp->r_atom);
		    if (sts == 1) {
			rp->r_pmid = pmidlist[i];
			rp->r_inst = shim_metrictab[m].m_ctrs[c].c_inst;
			numatoms++;
			rp++;
		    }
		}
		break;
	    }
	}
    }

    free(pmidlist);

    return numatoms;
}
