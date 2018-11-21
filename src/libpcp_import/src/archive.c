/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "libpcp.h"
#include "import.h"
#include "private.h"

static pmTimeval	stamp;

int
_pmi_put_result(pmi_context *current, pmResult *result)
{
    int		sts;
    char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmPDU	*pb;
    __pmLogCtl	*lcp = &current->logctl;
    __pmArchCtl	*acp = &current->archctl;
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

	acp->ac_log = &current->logctl;
	sts = __pmLogCreate(host, current->archive, PM_LOG_VERS02, acp);
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
	__pmLogWriteLabel(acp->ac_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
	__pmLogPutIndex(&current->archctl, &stamp);
    }

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));
    if ((sts = __pmEncodeResult(__pmFileno(acp->ac_mfp), result, &pb)) < 0)
	return sts;

    needti = 0;
    for (k = 0; k < result->numpmid; k++) {
	for (m = 0; m < current->nmetric; m++) {
	    if (result->vset[k]->pmid != current->metric[m].pmid)
		continue;
	    if (current->metric[m].meta_done == 0) {
		char	**namelist = &current->metric[m].name;

		if ((sts = __pmLogPutDesc(acp, &current->metric[m].desc, 1, namelist)) < 0) {
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
			    if ((sts = __pmLogPutInDom(acp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
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
	__pmLogPutIndex(acp, &stamp);
    }

    if ((sts = __pmLogPutResult2(acp, pb)) < 0) {
	__pmUnpinPDUBuf(pb);
	return sts;
    }

    __pmUnpinPDUBuf(pb);
    return 0;
}

int
_pmi_put_text(pmi_context *current)
{
    int		sts;
    char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmLogCtl	*lcp = &current->logctl;
    __pmArchCtl	*acp = &current->archctl;
    pmi_text	*tp;
    int		i;
    int		m;
    int		t;
    int		needti;

    /* last_stamp has been set by the caller. */
    stamp.tv_sec = current->last_stamp.tv_sec;
    stamp.tv_usec = current->last_stamp.tv_usec;

    if (current->state == CONTEXT_START) {
	/* TODO: factor this code */
	if (current->hostname == NULL) {
	    (void)gethostname(myname, MAXHOSTNAMELEN);
	    myname[MAXHOSTNAMELEN-1] = '\0';
	    host = myname;
	}
	else
	    host = current->hostname;

	acp->ac_log = &current->logctl;
	sts = __pmLogCreate(host, current->archive, PM_LOG_VERS02, acp);
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
	__pmLogWriteLabel(acp->ac_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
	__pmLogPutIndex(&current->archctl, &stamp);
    }

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));

    needti = 0;
    for (t = 0; t < current->ntext; t++) {
	tp = &current->text[t];
	if (tp->meta_done)
	    continue; /* Already written */

	if ((tp->type & PM_TEXT_PMID)) {
	    /*
	     * This text is for a metric. Make sure that the metric desc
	     * has been written.
	     */
	    /* TODO: factor this code */
	    for (m = 0; m < current->nmetric; m++) {
		if (tp->id != current->metric[m].pmid)
		    continue;
		if (current->metric[m].meta_done == 0) {
		    char	**namelist = &current->metric[m].name;

		    if ((sts = __pmLogPutDesc(acp, &current->metric[m].desc, 1, namelist)) < 0) {
			return sts;
		    }
		    current->metric[m].meta_done = 1;
		    needti = 1;
		}
		if (current->metric[m].desc.indom != PM_INDOM_NULL) {
		    for (i = 0; i < current->nindom; i++) {
			if (current->metric[m].desc.indom == current->indom[i].indom) {
			    if (current->indom[i].meta_done == 0) {
				if ((sts = __pmLogPutInDom(acp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
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
	else if ((tp->type & PM_TEXT_INDOM)) {
	    /*
	     * This text is for an indom. Make sure that the indom
	     * has been written.
	     */
	    /* TODO: factor this code */
	    for (i = 0; i < current->nindom; i++) {
		if (tp->id != current->indom[i].indom)
		    continue;
		if (current->indom[i].meta_done == 0) {
		    if ((sts = __pmLogPutInDom(acp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
			return sts;
		    }
		    current->indom[i].meta_done = 1;
		    needti = 1;
		}
		break;
	    }
	}

	/*
	 * Now write out the text record.
	 * libpcp, via __pmLogPutText(), makes a copy of the storage pointed
	 * to by buffer.
	 */
	if ((sts = __pmLogPutText(&current->archctl, tp->id, tp->type,
				  tp->content, 1/*cached*/)) < 0)
	    return sts;

	tp->meta_done = 1;
    }

    if (needti)
	__pmLogPutIndex(acp, &stamp);

    return 0;
}

int
_pmi_put_label(pmi_context *current)
{
    int		sts;
    char	*host;
    char	myname[MAXHOSTNAMELEN];
    __pmLogCtl	*lcp = &current->logctl;
    __pmArchCtl	*acp = &current->archctl;
    pmi_label	*lp;
    int		i;
    int		m;
    int		l;
    int		needti;

    /* last_stamp has been set by the caller. */
    stamp.tv_sec = current->last_stamp.tv_sec;
    stamp.tv_usec = current->last_stamp.tv_usec;

    if (current->state == CONTEXT_START) {
	/* TODO: factor this code */
	if (current->hostname == NULL) {
	    (void)gethostname(myname, MAXHOSTNAMELEN);
	    myname[MAXHOSTNAMELEN-1] = '\0';
	    host = myname;
	}
	else
	    host = current->hostname;

	acp->ac_log = &current->logctl;
	sts = __pmLogCreate(host, current->archive, PM_LOG_VERS02, acp);
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
	__pmLogWriteLabel(acp->ac_mfp, &lcp->l_label);
	lcp->l_state = PM_LOG_STATE_INIT;
	__pmLogPutIndex(&current->archctl, &stamp);
    }

    __pmOverrideLastFd(__pmFileno(acp->ac_mfp));

    needti = 0;
    for (l = 0; l < current->nlabel; l++) {
	lp = &current->label[l];

	if (lp->type == PM_LABEL_ITEM) {
	    /*
	     * This label is for a metric. Make sure that the metric desc
	     * has been written.
	     */
	    /* TODO: factor this code */
	    for (m = 0; m < current->nmetric; m++) {
		if (lp->id != current->metric[m].pmid)
		    continue;
		if (current->metric[m].meta_done == 0) {
		    char	**namelist = &current->metric[m].name;

		    if ((sts = __pmLogPutDesc(acp, &current->metric[m].desc, 1, namelist)) < 0) {
			return sts;
		    }
		    current->metric[m].meta_done = 1;
		    needti = 1;
		}
		if (current->metric[m].desc.indom != PM_INDOM_NULL) {
		    for (i = 0; i < current->nindom; i++) {
			if (current->metric[m].desc.indom == current->indom[i].indom) {
			    if (current->indom[i].meta_done == 0) {
				if ((sts = __pmLogPutInDom(acp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
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
	else if (lp->type == PM_LABEL_INDOM || lp->type == PM_LABEL_INSTANCES) {
	    /*
	     * This label is for an indom. Make sure that the indom
	     * has been written.
	     */
	    /* TODO: factor this code */
	    for (i = 0; i < current->nindom; i++) {
		if (lp->id != current->indom[i].indom)
		    continue;
		if (current->indom[i].meta_done == 0) {
		    if ((sts = __pmLogPutInDom(acp, current->indom[i].indom, &stamp, current->indom[i].ninstance, current->indom[i].inst, current->indom[i].name)) < 0) {
			return sts;
		    }
		    current->indom[i].meta_done = 1;
		    needti = 1;
		}
		break;
	    }
	}

	/*
	 * Now write out the label record.
	 * libpcp, via __pmLogPutLabel(), assumes control of the
	 * storage pointed to by lp->labelset.
	 */
	if ((sts = __pmLogPutLabel(&current->archctl, lp->type, lp->id,
				   1, lp->labelset, &stamp)) < 0)
	    return sts;

	lp->labelset = NULL;
    }

    /* We no longer need the accumulated list of labelsets. */
    free(current->label);
    current->nlabel = 0;
    current->label = NULL;

    if (needti)
	__pmLogPutIndex(acp, &stamp);

    return 0;
}

int
_pmi_end(pmi_context *current)
{
    /* Final temporal index update to finish the archive
     * ... same logic here as in run_done() for pmlogger
     */
    __pmLogPutIndex(&current->archctl, &stamp);

    __pmLogClose(&current->archctl);

    current->state = CONTEXT_END;
    return 0;
}
