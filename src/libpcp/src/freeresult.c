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
 */

#include "pmapi.h"
#include "impl.h"

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
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDUBUF) {
		    char	strbuf[20];
		    fprintf(stderr, "free(" PRINTF_P_PFX "%p) pmValueBlock pmid=%s inst=%d\n",
			pvs->vlist[j].value.pval, pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)),
			pvs->vlist[j].inst);
		}
#endif
		free(pvs->vlist[j].value.pval);
	    }
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_PDUBUF) {
	    char	strbuf[20];
	    fprintf(stderr, "free(" PRINTF_P_PFX "%p) vset pmid=%s\n",
		pvs, pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)));
	}
#endif
	free(pvs);
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
