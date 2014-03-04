/*
 * Copyright (c) 2014 Red Hat.
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

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
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
#endif
    return 0;
}
