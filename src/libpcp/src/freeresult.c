/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: freeresult.c,v 1.1 2000/12/05 03:49:43 max Exp $"

#include "pmapi.h"
#include "impl.h"

#define MAGIC PM_VAL_HDR_SIZE + sizeof(__int64_t)

/* Free result buffer routines */

void
__pmFreeResultValues(pmResult *result)
{
    register pmValueSet *pvs;
    register pmValueSet **ppvs;
    register pmValueSet **ppvsend;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "__pmFreeResultValues(" PRINTF_P_PFX "%p) numpmid=%d\n",
	    result, result->numpmid);
    }
#endif

    if (result->numpmid == 0)
	return;

    ppvsend = &result->vset[result->numpmid];

    /* if _any_ vset[] -> an address within a pdubuf, we are done */
    for (ppvs = result->vset; ppvs < ppvsend; ppvs++) {
	if (__pmUnpinPDUBuf((void *)*ppvs))
	    return;
    }

    /* not created from a pdubuf, really free the memory */
    for (ppvs = result->vset; ppvs < ppvsend; ppvs++) {
	pvs = *ppvs;
	if (pvs->numval > 0 && pvs->valfmt == PM_VAL_DPTR) {
	    /* pmValueBlocks may be malloc'd as well */
	    int		j;
	    for (j = 0; j < pvs->numval; j++) {
		if (pvs->vlist[j].value.pval->vlen == MAGIC) {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_PDUBUF) {
			fprintf(stderr, "__pmPoolFree(" PRINTF_P_PFX "%p) pmValueBlock pmid=%s inst=%d\n",
			    pvs->vlist[j].value.pval, pmIDStr(pvs->pmid),
			    pvs->vlist[j].inst);
		    }
#endif
		    __pmPoolFree(pvs->vlist[j].value.pval, MAGIC);
		}
		else {
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_PDUBUF) {
			fprintf(stderr, "free(" PRINTF_P_PFX "%p) pmValueBlock pmid=%s inst=%d\n",
			    pvs->vlist[j].value.pval, pmIDStr(pvs->pmid),
			    pvs->vlist[j].inst);
		    }
#endif
		    free(pvs->vlist[j].value.pval);
		}
	    }
	}
	if (pvs->numval == 1) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF) {
		fprintf(stderr, "__pmPoolFree(" PRINTF_P_PFX "%p) vset pmid=%s\n",
		    pvs, pmIDStr(pvs->pmid));
	    }
#endif
	    __pmPoolFree(pvs, sizeof(pmValueSet));
	}
	else {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF) {
		fprintf(stderr, "free(" PRINTF_P_PFX "%p) vset pmid=%s\n",
		    pvs, pmIDStr(pvs->pmid));
	    }
#endif
	    free(pvs);
	}
    }
}

void
pmFreeResult(pmResult *result)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF) {
	fprintf(stderr, "pmFreeResult(" PRINTF_P_PFX "%p)\n", result);
    }
#endif
    __pmFreeResultValues(result);
    free(result);
}
