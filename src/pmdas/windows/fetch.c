/*
 * Copyright (c) 2008 Aconex.  All Rights Reserved.
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
 */

#include "hypnotoad.h"

/*
 * Instantiate a value for a single metric-instance pair
 */
static int
pdh_fetch(pdh_metric_t *mp, int c)
{
    PDH_STATUS		pdhsts;
    PDH_RAW_COUNTER	raw;
    pdh_value_t		*vp = &mp->vals[c];

    vp->flags &= ~V_COLLECTED;

    if (mp->flags & M_NOVALUES)
	return 0;

    if ((querydesc[mp->qid].flags & Q_COLLECTED) == 0) {
	pdhsts = PdhCollectQueryData(vp->hdl);
	if (pdhsts == ERROR_SUCCESS)
	    querydesc[mp->qid].flags |= Q_COLLECTED;
	else {
	    if ((querydesc[mp->qid].flags & Q_ERR_SEEN) == 0) {
		__pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhCollectQueryData "
				"failed for querydesc[%d]: %s\n",
				mp->qid, pdherrstr(pdhsts));
		querydesc[mp->qid].flags |= Q_ERR_SEEN;
	    }
	    return 0;
	}
    }

    pdhsts = PdhGetRawCounterValue(vp->hdl, NULL, &raw);
    if (pdhsts != ERROR_SUCCESS) {
	__pmNotifyErr(LOG_ERR, "pdh_fetch: Error: PdhGetRawCounterValue "
			"failed for metric %s inst %d: %s\n",
			pmIDStr(mp->desc.pmid), vp->inst, pdherrstr(pdhsts));
	/* no values for you! */
	vp->flags = V_NONE;
	return 0;
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
    vp->flags |= V_COLLECTED;
    return 1;
}

/*
 * Called before each PMDA fetch ... force query groups to be refreshed if
 * we are asked for metrics covered by a query, and instantiates values.
 */
void
windows_fetch_refresh(int numpmid, pmID pmidlist[])
{
    int		i, v;

    for (i = 0; i < querydesc_sz; i++)
	querydesc[i].flags &= ~Q_COLLECTED;

    for (i = 0; i < numpmid; i++) {
	__pmID_int	*pmidp = (__pmID_int *)&pmidlist[i];
	pdh_metric_t	*mp = &metricdesc[pmidp->item];

	for (v = 0; v < mp->num_vals; v++)
	    pdh_fetch(mp, v);
    }
}
