/*
 * Copyright (c) 2014-2018 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pmapi.h"
#include "logger.h"

char *chk_emess[] = {
    "No error",
    "Request for (advisory) ON conflicts with current (mandatory) ON state",
    "Request for (advisory) OFF conflicts with current (mandatory) ON state",
    "Request for (advisory) ON conflicts with current (mandatory) OFF state",
    "Request for (advisory) OFF conflicts with current (mandatory) OFF state",
};

static void
undo(task_t *tp, optreq_t *rqp, int inst)
{
    int 	j;
    int		k;
    int		sts;

    if (rqp->r_numinst >= 1) {
	/* remove instance from list of instance */
	for (k =0, j = 0; j < rqp->r_numinst; j++) {
	    if (rqp->r_instlist[j] != inst)
		rqp->r_instlist[k++] = rqp->r_instlist[j];
	}
	rqp->r_numinst = k;
	if ((sts =  __pmOptFetchDel(&tp->t_fetch, rqp)) < 0)
	    die("undo: __pmOptFetchDel", sts);

	if (rqp->r_numinst == 0) {
	    /* no more instances, remove specification */
	    if (tp->t_fetch == NULL) {
		/* no more specifications, remove task */
		task_t	*xtp;
		task_t	*ltp = NULL;
		for (xtp = tasklist; xtp != NULL; xtp = xtp->t_next) {
		    if (xtp == tp) {
			if (ltp == NULL)
			    tasklist = tp->t_next;
			else
			    ltp->t_next = tp->t_next;
			break;
		    }
		    ltp = xtp;
		}
	    }
	    __pmHashDel(rqp->r_desc->pmid, (void *)rqp, &pm_hash);
	    free(rqp);
	}
	else
	    /* re-insert modified specification */
	    __pmOptFetchAdd(&tp->t_fetch, rqp);
    }
    else {
	/*
	 * TODO ... current specification is for all instances,
	 * need to remove this instance from the set ...
	 * this requires some enhancement to optFetch
	 *
	 * pro tem, this metric-instance pair may continue to get
	 * logged, even though the logging state is recorded as
	 * OFF (this is the worst thing that can happen here)
	 */
    }
}

int
chk_one(task_t *tp, pmID pmid, int inst)
{
    optreq_t	*rqp;
    task_t	*ctp;

    rqp = findoptreq(pmid, inst);
    if (rqp == NULL)
	return 0;

    ctp = rqp->r_fetch->f_aux;
    if (ctp == NULL || ctp == tp)
	/*
	 * can only happen if same metric+inst appears more than once
	 * in the same group ... this can never be a conflict
	 */
	return 1;

    if (PMLC_GET_MAND(ctp->t_state)) {
	if (PMLC_GET_ON(ctp->t_state)) {
	    if (PMLC_GET_MAND(tp->t_state) == 0 && PMLC_GET_MAYBE(tp->t_state) == 0) {
		if (PMLC_GET_ON(tp->t_state))
		    return -1;
		else
		    return -2;
	    }
	}
	else {
	    if (PMLC_GET_MAND(tp->t_state) == 0 && PMLC_GET_MAYBE(tp->t_state) == 0) {
		if (PMLC_GET_ON(tp->t_state))
		    return -3;
		else
		    return -4;
	    }
	}
	/*
	 * new mandatory, over-rides the old mandatory
	 */
	undo(ctp, rqp, inst);
    }
    else {
	/*
	 * new anything, over-rides the old advisory
	 */
	undo(ctp, rqp, inst);
    }

    return 0;
}

int
chk_all(task_t *tp, pmID pmid)
{
    optreq_t	*rqp;
    task_t	*ctp;

    rqp = findoptreq(pmid, 0);	/*TODO, not right!*/
    if (rqp == NULL)
	return 0;

    ctp = rqp->r_fetch->f_aux;

    if (pmDebugOptions.log) {
	fprintf(stderr, "chk_all: pmid=%s task=" PRINTF_P_PFX "%p state=%s%s%s%s delta=%d.%06d\n",
		pmIDStr(pmid), tp,
		PMLC_GET_INLOG(tp->t_state) ? " " : "N",
		PMLC_GET_AVAIL(tp->t_state) ? " " : "N",
		PMLC_GET_MAND(tp->t_state) ? "M" : "A",
		PMLC_GET_ON(tp->t_state) ? "Y" : "N",
		(int)tp->t_delta.tv_sec, (int)tp->t_delta.tv_usec);
	if (ctp == NULL)
	    fprintf(stderr, "compared to: NULL\n");
	else
	    fprintf(stderr, "compared to: optreq task=" PRINTF_P_PFX "%p state=%s%s%s%s delta=%d.%06d\n",
		    ctp,
		    PMLC_GET_INLOG(ctp->t_state) ? " " : "N",
		    PMLC_GET_AVAIL(ctp->t_state) ? " " : "N",
		    PMLC_GET_MAND(ctp->t_state) ? "M" : "A",
		    PMLC_GET_ON(ctp->t_state) ? "Y" : "N",
		    (int)ctp->t_delta.tv_sec, (int)ctp->t_delta.tv_usec);
    }
    return 0;
}

/*
 * Called when an error PDU containing PMCD_ADD_AGENT is received.
 * This function checks all of the configured metrics to make sure that
 * they have not changed. For example due to a PMDA being replaced by an
 * updated version 
 */
void
validate_metrics(void)
{
    const task_t	*tp;
    pmID		*new_pmids;
    const pmDesc	*old_desc;
    pmDesc		new_desc;
    int			index;
    int			error;
    int			sts;
    time_t		now;
    char		buf1[20], buf2[20];

    time(&now);
    fprintf(stderr, "%s: Validating metrics after PMCD state changed at %s",
		    pmGetProgname(), ctime(&now));

    /*
     * Check each metric in each element of the task list, whether it is
     * active or not.
     */
    error = 0;
    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	/* We need at least one metric to look up. */
	if (tp->t_numpmid < 1)
	    continue;

	/*
	 * Perform a bulk lookup and then check for consistency.
	 * Lookup the metrics by name, since that's the way they are
	 * specified in the pmlogger config file.
	 * We need a temporary array for the new pmIDs
	 */

	new_pmids = malloc(tp->t_numpmid * sizeof(*tp->t_pmidlist));
	if (new_pmids == NULL) {
	    pmNoMem("allocating pmID array for validating metrice",
		      tp->t_numpmid * sizeof(*tp->t_pmidlist), PM_FATAL_ERR);
	}
	if ((sts = pmLookupName(tp->t_numpmid, tp->t_namelist, new_pmids)) < 0) {
	    fprintf(stderr, "Error looking up metrics: Reason: %s\n",
		    pmErrStr(sts));
	    exit(1);
	}

	/* Now check the individual metrics for problems. */
	for (index = 0; index < tp->t_numpmid; ++index) {
	    /* If there was an error looking up this metric, try again in order
	     * to obtain the reason. If there is no error the second time
	     * (possible), then the needed pmID will be fetched.
	     */
	    if (new_pmids[index] == PM_ID_NULL) {
		if ((sts = pmLookupName(1, &tp->t_namelist[index],
					&new_pmids[index])) < 0) {
		    /* The lookup of the metric is still in error. */
		    fprintf(stderr, "Error looking up %s: Reason: %s\n",
			    tp->t_namelist[index], pmErrStr(sts));
		    ++error;
		    continue;
		}
		/* No error the second time. Fall through */
	    }

	    /*
	     * Check that the pmid, type, semantics, instance domain and units
	     * of the metric have not changed.
	     */
	    if (new_pmids[index] != tp->t_pmidlist[index]) {
		fprintf(stderr, "PMID of metric \"%s\" has changed from %s to %s\n",
			tp->t_namelist[index],
			pmIDStr_r(tp->t_pmidlist[index], buf1, sizeof(buf1)),
			pmIDStr_r(new_pmids[index], buf2, sizeof(buf2)));
		++error;
	    }
	    if ((sts = pmLookupDesc(new_pmids[index], &new_desc)) < 0) {
		fprintf(stderr, "Description unavailable for metric \"%s\": %s\n",
			tp->t_namelist[index], pmErrStr(sts));
		++error;
		continue;
	    }
	    old_desc = &tp->t_desclist[index];
	    if (new_desc.type != old_desc->type) {
		fprintf(stderr, "Type of metric \"%s\" has changed from %s to %s\n",
			tp->t_namelist[index],
			pmTypeStr_r(old_desc->type, buf1, sizeof(buf1)),
			pmTypeStr_r(new_desc.type, buf2, sizeof(buf2)));
		++error;
	    }
	    if (new_desc.sem != old_desc->sem) {
		fprintf(stderr, "Semantics of metric \"%s\" have changed from %s to %s\n",
			tp->t_namelist[index],
			pmSemStr_r(old_desc->sem, buf1, sizeof(buf1)),
			pmSemStr_r(new_desc.sem, buf2, sizeof(buf2)));
		++error;
	    }
	    if (new_desc.indom != old_desc->indom) {
		fprintf(stderr, "Instance domain of metric \"%s\" has changed from %s to %s\n",
			tp->t_namelist[index],
			pmInDomStr_r(old_desc->indom, buf1, sizeof(buf1)),
			pmInDomStr_r(new_desc.indom, buf2, sizeof(buf2)));
		++error;
	    }
	    if (new_desc.units.dimSpace != old_desc->units.dimSpace ||
		new_desc.units.dimTime != old_desc->units.dimTime ||
		new_desc.units.dimCount != old_desc->units.dimCount ||
		new_desc.units.scaleSpace != old_desc->units.scaleSpace ||
		new_desc.units.scaleTime != old_desc->units.scaleTime ||
		new_desc.units.scaleCount != old_desc->units.scaleCount) {
		++error;
		fprintf(stderr, "Units of metric \"%s\" has changed from %s to %s\n",
			tp->t_namelist[index],
			pmUnitsStr_r(&old_desc->units, buf1, sizeof(buf1)),
			pmUnitsStr_r(&new_desc.units, buf2, sizeof(buf2)));
	    }
	} /* loop over metrics */

	free(new_pmids);
    } /* Loop over task list */

    /* We cannot continue, if any of the metrics have changed. */
    if (error) {
	fprintf(stderr, "One or more configured metrics have changed after pmcd state change. Exiting\n");
	exit(1);
    }
}

/*
 * PMNS traversal callback - if this is a metric we're not currently
 * logging, activate it by making a control request .. just the same
 * as if pmlc had sent such a request over the wire.
 */
static void
add_dynamic_metric(const char *name, void *data)
{
    int		sts;
    int		timedelta;
    int		sendresult = 0;
    pmID	pmid;
    dynroot_t	*d = (dynroot_t *)data;
    pmResult	*logreq;
    __pmHashNode *hp;

    if ((sts = pmLookupName(1, (char **)&name, &pmid)) < 0 || pmid == PM_ID_NULL) {
	/* hmm, that metric has gone away, or something went wrong  - ignore it */
	return;
    }

    /* check if this metric is already being logged */
    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
        if (pmid == (pmID)hp->key)
	    return;
    }

    /*
     * construct the pmResult for the control request
     * - only has one valueset, with one value
     */
    logreq = (pmResult *)malloc(sizeof(pmResult));
    memset(logreq, 0, sizeof(pmResult));
    logreq->vset[0] = (pmValueSet *)malloc(sizeof(pmValueSet));
    memset(logreq->vset[0], 0, sizeof(pmValueSet));
    logreq->numpmid = 1;
    logreq->vset[0]->pmid = pmid;

    /* Call the control request function - note: do_control_req() frees our logreq */
    timedelta = d->delta.tv_sec*1000 + d->delta.tv_usec/1000;
    sts = do_control_req(logreq, d->control, d->state, timedelta, sendresult=0);

    if (pmDebugOptions.log) {
	fprintf(stderr, "%s: do_control_req from dynamic root \"%s\"\n",
	    pmGetProgname(), d->name);
	fprintf(stderr, "... pmid %s, name \"%s\", state=0x%x, control=0x%x: sts=%d\n",
	    pmIDStr(pmid), name, d->state, d->control, sts);
    }
}

/*
 * In response to pmFetch returning with the PMCD_NAMES_CHANGE flag set,
 * walk the dynamic root list and add any new metrics that have appeared
 * to a suitable task.
 */
void
check_dynamic_metrics(void)
{
    int			i;
    int			sts;
    time_t		now;

    if (pmDebugOptions.log) {
	time(&now);
	fprintf(stderr, "%s: checking for new metrics after PMCD_NAMES_CHANGE state changed at %s",
	    pmGetProgname(), ctime(&now));
    }

    for (i=0; i < n_dyn_roots; i++) {
	if ((sts = pmTraversePMNS_r(dyn_roots[i].name, add_dynamic_metric, (void *)&dyn_roots[i])) < 0 ) {
	    ; /* hmm. ignore error, but maybe we should report it? */
	}
    }
}
