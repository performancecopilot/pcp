/*
 * Copyright (c) 2008-2009 Aconex.  All Rights Reserved.
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

#include "hypnotoad.h"

/*
 * Instantiate a value for a single metric-instance pair
 */
int
windows_collect_metric(pdh_metric_t *mp, LPSTR pat, pdh_value_t *vp)
{
    PDH_RAW_COUNTER	raw;
    PDH_STATUS  	pdhsts;
    PDH_HQUERY		queryhdl = NULL;
    PDH_HCOUNTER	counterhdl = NULL;
    int			sts = -1;

    if (mp->flags & M_NOVALUES)
	return sts;

    pdhsts = PdhOpenQueryA(NULL, 0, &queryhdl);
    if (pdhsts != ERROR_SUCCESS) {
	__pmNotifyErr(LOG_ERR, "windows_open: PdhOpenQueryA failed: %s\n",
			pdherrstr(pdhsts));
	return sts;
    }

    pdhsts = PdhAddCounterA(queryhdl, pat, vp->inst, &counterhdl);
    if (pdhsts != ERROR_SUCCESS) {
	__pmNotifyErr(LOG_ERR, "windows_open: Warning: PdhAddCounterA "
				"@ pmid=%s pat=\"%s\": %s\n",
			    pmIDStr(mp->desc.pmid), pat, pdherrstr(pdhsts));
	PdhCloseQuery(queryhdl);
	return sts;
    }

    pdhsts = PdhCollectQueryData(queryhdl);
    if (pdhsts != ERROR_SUCCESS) {
	if ((vp->flags & V_ERROR_SEEN) == 0) {
	    __pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhCollectQueryData "
				   "failed for metric %s pat %s: %s\n",
			pmIDStr(mp->desc.pmid), pat, pdherrstr(pdhsts));
	    vp->flags |= V_ERROR_SEEN;
	}
	goto done;
    }

    pdhsts = PdhGetRawCounterValue(counterhdl, NULL, &raw);
    if (pdhsts != ERROR_SUCCESS) {
	__pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhGetRawCounterValue "
			"failed for metric %s inst %d: %s\n",
			pmIDStr(mp->desc.pmid), vp->inst, pdherrstr(pdhsts));
	/* no values for you! */
	vp->flags = V_NONE;
	goto done;
    }

    switch (mp->ctype) {
	/*
	 * see also open.c for Pdh metric semantics
	 */
	case PERF_COUNTER_COUNTER:
	case PERF_COUNTER_RAWCOUNT:
	    /* these counters are only 32-bit */
	    vp->atom.ul = (__uint32_t)raw.FirstValue;
	    break;

	case PERF_100NSEC_TIMER:
	case PERF_PRECISION_100NS_TIMER:
	    /* convert 100nsec units to usec */
	    vp->atom.ull = raw.FirstValue / 10;
	    break;

	case PERF_RAW_FRACTION:
	    /* v1 / v2 as percentage */
	    vp->atom.f = (float)raw.FirstValue / raw.SecondValue;
	    break;

	case PERF_COUNTER_BULK_COUNT:
	case PERF_COUNTER_LARGE_RAWCOUNT:
	default:
	    vp->atom.ull = raw.FirstValue;
    }
    sts = 0;

done:
    PdhRemoveCounter(counterhdl);
    PdhCloseQuery(queryhdl);
    return sts;
}

void
windows_collect_callback(pdh_metric_t *pmp, LPTSTR pat, pdh_value_t *pvp)
{
    windows_verify_callback(pmp, pat, pvp);

    if (!(pvp->flags & V_COLLECTED))
	if (windows_collect_metric(pmp, pat, pvp) == 0)
	    pvp->flags |= V_COLLECTED;
}

/*
 * Called before each PMDA fetch ... force value refreshes for
 * requested metrics here; special case derived filesys metrics.
 */
void
windows_fetch_refresh(int numpmid, pmID pmidlist[])
{
    int	i, j, extra_filesys = 0;

    for (i = 0; i < metricdesc_sz; i++)
	for (j = 0; j < metricdesc[i].num_vals; j++)
	    metricdesc[i].vals[j].flags = V_NONE;

    for (i = 0; i < numpmid; i++) {
	__pmID_int *pmidp = (__pmID_int *)&pmidlist[i];
	int item = pmidp->item;

	if (item == 117 || item == 118 || item == 119)
	    extra_filesys++;
	else
	    windows_visit_metric(&metricdesc[item], windows_collect_callback);
    }

    if (extra_filesys) {
	windows_visit_metric(&metricdesc[120], windows_collect_callback);
	windows_visit_metric(&metricdesc[121], windows_collect_callback);
    }
}
