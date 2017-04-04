/*
 * Copyright (c) 2014 Red Hat.
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

static void
__pmFreeResultValueSets(pmValueSet **ppvstart, pmValueSet **ppvsend)
{
    pmValueSet *pvs;
    pmValueSet **ppvs;
    char	strbuf[20];
    int		j;

    /* if _any_ vset[] -> an address within a pdubuf, we are done */
    for (ppvs = ppvstart; ppvs < ppvsend; ppvs++) {
	if (__pmUnpinPDUBuf((void *)*ppvs))
	    return;
    }

    /* not created from a pdubuf, really free the memory */
    for (ppvs = ppvstart; ppvs < ppvsend; ppvs++) {
	pvs = *ppvs;
	if (pvs->numval > 0 && pvs->valfmt == PM_VAL_DPTR) {
	    /* pmValueBlocks may be malloc'd as well */
	    for (j = 0; j < pvs->numval; j++) {
		if (pmDebug & DBG_TRACE_PDUBUF)
		    fprintf(stderr, "free"
			"(" PRINTF_P_PFX "%p) pmValueBlock pmid=%s inst=%d\n",
			pvs->vlist[j].value.pval,
			pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)),
			pvs->vlist[j].inst);
		free(pvs->vlist[j].value.pval);
	    }
	}
	if (pmDebug & DBG_TRACE_PDUBUF)
	    fprintf(stderr, "free(" PRINTF_P_PFX "%p) vset pmid=%s\n",
		pvs, pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)));
	free(pvs);
    }
}

void
__pmFreeResultValues(pmResult *result)
{
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmFreeResultValues(" PRINTF_P_PFX "%p) numpmid=%d\n",
	    result, result->numpmid);
    if (result->numpmid)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
}

void
pmFreeResult(pmResult *result)
{
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "pmFreeResult(" PRINTF_P_PFX "%p)\n", result);
    __pmFreeResultValues(result);
    free(result);
}

void
pmFreeHighResResult(pmHighResResult *result)
{
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "pmFreeHighResResult(" PRINTF_P_PFX "%p)\n", result);
    if (result->numpmid)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
    free(result);
}
