/*
 * Copyright (c) 2017 Red Hat.
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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
#include "import.h"
#include "private.h"

static __pmTimeval	stamp;

int
_pmi_put_result(pmi_context *current, pmResult *result)
{
    int		sts;
    char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmPDU	*pb;
    __pmLogCtl	*lcp = &current->logctl;
    int		k;
    int		i;
    int		m;
    int		needti;

    /*
     * some front-end tools use lazy discovery of instances and/or process
     * data in non-deterministic order ... it is simpler for everyone if
     * we sort the values into ascending instance order.
     */
    pmSortInstances(result);

    stamp.tv_sec = result->timestamp.tv_sec;
    stamp.tv_usec = result->timestamp.tv_usec;

    if (current->state == CONTEXT_START) {
	if (current->hostname == NULL) {
	    (void)gethostname(myname, MAXHOSTNAMELEN);
	    myname[MAXHOSTNAMELEN-1] = '\0';
	    host = myname;
	}
	else
	    host = current->hostname;

	sts = __pmLogCreate(host, current->archive, PM_LOG_VERS02, lcp);
	if (sts < 0)
	    return sts;

	if (current->timezone == NULL) {
	    char	tzbuf[PM_TZ_MAXLEN];
	    strcpy(lcp->l_label.ill_tz, __pmTimezone_r(tzbuf, sizeof(tzbuf)));
	}
	else
	    strcpy(lcp->l_label.ill_tz, current->timezone);
	pmNewZone(lcp->l_label.ill_tz);
	current->state = CONTEXT_ACTIVE;

	/*
	 * do the label records (it is too late when __pmLogPutResult
	 * or __pmLogPutResult2 is called as we've already output some
	 * metadata) ... this code is stolen from logputresult() in
	 * libpcp
	 */
	lcp->l_label.ill_start.tv_sec = stamp.tv_sec;
	lcp->l_label.ill_start.tv_usec = stamp.tv_usec;
	lcp->l_label.ill_vol = PM_LOG_VOL_TI;
	__pmLogWriteLabel(lcp->l_tifp, &lcp->l_label);
	lcp->l_label.ill_vol = PM_LOG_VOL_META;
	__pmLogWriteLabel(lcp->l_mdfp, &lcp->l_label);
	lcp->l_label.ill_vol = 0;
	__pmLogWriteLabel(lcp->l_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
	__pmLogPutIndex(&current->logctl, &stamp);
    }

    __pmOverrideLastFd(__pmFileno(lcp->l_mfp));
    if ((sts = __pmEncodeResult(__pmFileno(lcp->l_mfp), result, &pb)) < 0)
	return sts;

    needti = 0;
    for (k = 0; k < result->numpmid; k++) {
	for (m = 0; m < current->nmetric; m++) {
	    if (result->vset[k]->pmid != current->metric[m].pmid)
		continue;
	    if (current->metric[m].meta_done == 0) {
		char	**namelist = &current->metric[m].name;

		if ((sts = __pmLogPutDesc(lcp, &current->metric[m].desc, 1, namelist)) < 0) {
		    __pmUnpinPDUBuf(pb);
		    return sts;
		}
		current->metric[m].meta_done = 1;
		needti = 1;
	    }
	    if (current->metric[m].desc.indom != PM_INDOM_NULL) {
		for (i = 0; i < current->nindom; i++) {
		    if (current->metric[m].desc.indom == current->indom[i].indom) {
			if (current->indom[i].meta_done == 0) {
			    if ((sts = __pmLogPutInDom(lcp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
				__pmUnpinPDUBuf(pb);
				return sts;
			    }
			    current->indom[i].meta_done = 1;
			    needti = 1;
			}
		    }
		}
	    }
	    break;
	}
    }
    if (needti) {
	__pmLogPutIndex(lcp, &stamp);
    }

    if ((sts = __pmLogPutResult2(lcp, pb)) < 0) {
	__pmUnpinPDUBuf(pb);
	return sts;
    }

    __pmUnpinPDUBuf(pb);
    return 0;
}

int
_pmi_end(pmi_context *current)
{
    /* Final temporal index update to finish the archive
     * ... same logic here as in run_done() for pmlogger
     */
    __pmLogPutIndex(&current->logctl, &stamp);

    __pmLogClose(&current->logctl);

    current->state = CONTEXT_END;
    return 0;
}
