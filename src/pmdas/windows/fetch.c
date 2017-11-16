/*
 * Copyright (c) 2008-2010 Aconex.  All Rights Reserved.
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

MEMORYSTATUSEX	windows_memstat;

void
windows_fetch_memstat(void)
{
    ZeroMemory(&windows_memstat, sizeof(MEMORYSTATUSEX));
    windows_memstat.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&windows_memstat);
}

/*
 * Instantiate a value for a single metric-instance pair
 */
int
windows_collect_metric(pdh_metric_t *mp, LPSTR pat, pdh_value_t *vp)
{
    PDH_STATUS  	pdhsts;
    PDH_HQUERY		queryhdl = NULL;
    PDH_HCOUNTER	counthdl = NULL;
    int			sts = -1;

    if (mp->flags & M_NOVALUES)
	return sts;

    pdhsts = PdhOpenQueryA(NULL, 0, &queryhdl);
    if (pdhsts != ERROR_SUCCESS) {
	pmNotifyErr(LOG_ERR, "windows_open: PdhOpenQueryA failed: %s\n",
			pdherrstr(pdhsts));
	return sts;
    }

    pdhsts = PdhAddCounterA(queryhdl, pat, vp->inst, &counthdl);
    if (pdhsts != ERROR_SUCCESS) {
	pmNotifyErr(LOG_ERR, "windows_open: Warning: PdhAddCounterA "
				"@ pmid=%s pat=\"%s\": %s\n",
			    pmIDStr(mp->desc.pmid), pat, pdherrstr(pdhsts));
	PdhCloseQuery(queryhdl);
	return sts;
    }

    pdhsts = PdhCollectQueryData(queryhdl);
    if (pdhsts != ERROR_SUCCESS) {
	if ((vp->flags & V_ERROR_SEEN) == 0) {
	    pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhCollectQueryData "
				   "failed for metric %s pat %s: %s\n",
			pmIDStr(mp->desc.pmid), pat, pdherrstr(pdhsts));
	    vp->flags |= V_ERROR_SEEN;
	}
    } else if ((mp->ctype == PERF_ELAPSED_TIME) ||
		mp->ctype == PERF_LARGE_RAW_FRACTION) {
	PDH_FMT_COUNTERVALUE fmt;
	DWORD type;

	if (mp->ctype == PERF_ELAPSED_TIME)
	    type = PDH_FMT_LARGE;
	else	/* PERF_LARGE_RAW_FRACTION */
	    type = PDH_FMT_DOUBLE;

	pdhsts = PdhGetFormattedCounterValue(counthdl, type, NULL, &fmt);
	if (pdhsts != ERROR_SUCCESS) {
	    pmNotifyErr(LOG_ERR, "Error: PdhGetFormattedCounterValue "
			"failed for metric %s inst %d: %s\n",
			pmIDStr(mp->desc.pmid), vp->inst, pdherrstr(pdhsts));
	    vp->flags = V_NONE;	/* no values for you! */
	} else if (mp->ctype == PERF_ELAPSED_TIME) {
	    vp->atom.ull = fmt.largeValue;
	    sts = 0;
	} else {	/* PERF_LARGE_RAW_FRACTION */
	    vp->atom.d = fmt.doubleValue;
	    sts = 0;
	}
    } else {
	PDH_RAW_COUNTER	raw;

	pdhsts = PdhGetRawCounterValue(counthdl, NULL, &raw);
	if (pdhsts != ERROR_SUCCESS) {
	    pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhGetRawCounterValue "
				"failed for metric %s inst %d: %s\n",
			pmIDStr(mp->desc.pmid), vp->inst, pdherrstr(pdhsts));
	    vp->flags = V_NONE;	/* no values for you! */
	} else {
	    switch (mp->ctype) {
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
	}
    }
    PdhRemoveCounter(counthdl);
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
 * requested metrics here; and special case any derived metrics.
 */
void
windows_fetch_refresh(int numpmid, pmID pmidlist[], pmdaExt *pmda)
{
    int	i, j, extra_filesys = 0, extra_memstat = 0;
    int extra_hinv_ncpu = -1, extra_hinv_ndisk = -1;
    int extra_network = -1;

    for (i = 0; i < NUMINDOMS; i++)
	windows_indom_reset[i] = 0;

    for (i = 0; i < metricdesc_sz; i++)
	for (j = 0; j < metricdesc[i].num_vals; j++)
	    metricdesc[i].vals[j].flags = V_NONE;

    for (i = 0; i < numpmid; i++) {
	unsigned int cluster = pmID_cluster(pmidlist[i]);
	unsigned int item = pmID_item(pmidlist[i]);

	if (cluster == 1)
	    extra_memstat = 1;
	else if (cluster != 0)
	    continue;
	else if (item == 106)
	    extra_memstat = 1;
	else if (item == 107 && extra_hinv_ncpu == -1)
	    extra_hinv_ncpu = 1;
	else if (item == 108 && extra_hinv_ndisk == -1)
	    extra_hinv_ndisk = 1;
	else if (item >= 117 && item <= 119)
	    extra_filesys = 1;
	else if (item >= 236 && item <= 237 && extra_network == -1)
	    extra_network = 1;
	else {
	    if (item >= 4 && item <= 7)
		extra_hinv_ncpu = 0;
	    else if ((item >=  21 && item <=  26) || item ==  68 ||
		     (item >= 217 && item <= 219) || item == 101 ||
		     (item >= 226 && item <= 231) || item == 133)
		extra_hinv_ndisk = 0;
	    else if (item == 235)
		extra_network = 0;

	    windows_visit_metric(&metricdesc[item], windows_collect_callback);
	}
    }

    if (extra_memstat)
	windows_fetch_memstat();
    if (extra_hinv_ncpu == 1)
	windows_visit_metric(&metricdesc[4], NULL);
    if (extra_hinv_ndisk == 1)
	windows_visit_metric(&metricdesc[21], NULL);
    if (extra_filesys) {
	windows_visit_metric(&metricdesc[120], windows_collect_callback);
	windows_visit_metric(&metricdesc[121], windows_collect_callback);
    }
    if (extra_network == 1)
	windows_visit_metric(&metricdesc[235], windows_collect_callback);

    for (i = 0; i < NUMINDOMS; i++) {
	/* Do we want to persist this instance domain to disk? */
	if (windows_indom_reset[i] && windows_indom_fixed(i))
	    pmdaCacheOp(INDOM(pmda->e_domain, i), PMDA_CACHE_SAVE);
    }
}
