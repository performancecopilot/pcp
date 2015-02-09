/*
 * Copyright (c) 2012-2014 Red Hat.
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


/* return one of these when a status request is made from a PCP 1.x pmlc */
typedef struct {
    __pmTimeval  ls_start;	/* start time for log */
    __pmTimeval  ls_last;	/* last time log written */
    __pmTimeval  ls_timenow;	/* current time */
    int		ls_state;	/* state of log (from __pmLogCtl) */
    int		ls_vol;		/* current volume number of log */
    __int64_t	ls_size;	/* size of current volume */
    char	ls_hostname[PM_LOG_MAXHOSTLEN];
				/* name of pmcd host */
    char	ls_tz[40];      /* $TZ at collection host */
    char	ls_tzlogger[40]; /* $TZ at pmlogger */
} __pmLoggerStatus_v1;

#ifdef PCP_DEBUG
/* This crawls over the data structure looking for weirdness */
void
reality_check(void)
{
    __pmHashNode		*hp;
    task_t		*tp;
    task_t		*tp2;
    fetchctl_t		*fp;
    optreq_t		*rqp;
    pmID		pmid;
    int			i = 0, j, k;

    /* check that all fetch_t's f_aux point back to their parent task */
    for (tp = tasklist; tp != NULL; tp = tp->t_next, i++) {
	if (tp->t_fetch == NULL)
	    fprintf(stderr, "task[%d] @" PRINTF_P_PFX "%p has no fetch group\n", i, tp);
	j = 0;
	for (fp = tp->t_fetch; fp != NULL; fp = fp->f_next) {
	    if (fp->f_aux != (void *)tp)
		fprintf(stderr, "task[%d] fetch group[%d] has invalid task pointer\n",
			i, j);
	    j++;
	}

	/* check that all optreq_t's in hash list have valid r_fetch->f_aux
	 * pointing to a task in the task list.
	 */
	for (j = 0; j < tp->t_numpmid; j++) {
	    pmid = tp->t_pmidlist[j];
	    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
		if (pmid != (pmID)hp->key)
		continue;
		rqp = (optreq_t *)hp->data;
		for (tp2 = tasklist; tp2 != NULL; tp2 = tp2->t_next)
		    if (rqp->r_fetch->f_aux == (void *)tp2)
			break;
		if (tp2 == NULL) {
		    fprintf(stderr, "task[%d] pmid %s optreq " PRINTF_P_PFX "%p for [",
			    i, pmIDStr(pmid), rqp);
		    if (rqp->r_numinst == 0)
			fputs("`all instances' ", stderr);
		    else
			for (k = 0; k < rqp->r_numinst; k++)
			    fprintf(stderr, "%d ", rqp->r_instlist[k]);
		    fputs("] bad task pointer\n", stderr);
		}
	    }
	}
    }
}

void
dumpit(void)
{
    int		i;
    task_t	*tp;

    reality_check();
    for (tp = tasklist, i = 0; tp != NULL; tp = tp->t_next, i++) {
	fprintf(stderr,
		"\ntask[%d] @" PRINTF_P_PFX "%p: %s %s \ndelta = %f\n", i, tp,
		PMLC_GET_MAND(tp->t_state) ? "mandatory " : "advisory ",
		PMLC_GET_ON(tp->t_state) ? "on " : "off ",
		tp->t_delta.tv_sec + (float)tp->t_delta.tv_usec / 1.0e6);
	__pmOptFetchDump(stderr, tp->t_fetch);
    }
}

/*
 * stolen from __pmDumpResult
 */
static void
dumpcontrol(FILE *f, const pmResult *resp, int dovalue)
{
    int		i;
    int		j;

    fprintf(f,"LogControl dump from " PRINTF_P_PFX "%p", resp);
    fprintf(f, " numpmid: %d\n", resp->numpmid);
    for (i = 0; i < resp->numpmid; i++) {
	pmValueSet	*vsp = resp->vset[i];
	fprintf(f,"  %s :", pmIDStr(vsp->pmid));
	if (vsp->numval == 0) {
	    fprintf(f, " No values!\n");
	    continue;
	}
	else if (vsp->numval < 0) {
	    fprintf(f, " %s\n", pmErrStr(vsp->numval));
	    continue;
	}
	fprintf(f, " numval: %d", vsp->numval);
	fprintf(f, " valfmt: %d", vsp->valfmt);
	for (j = 0; j < vsp->numval; j++) {
	    pmValue	*vp = &vsp->vlist[j];
	    if (vsp->numval > 1 || vp->inst != PM_INDOM_NULL) {
		fprintf(f," inst [%d]", vp->inst);
	    }
	    else
		fprintf(f, " singular");
	    if (dovalue) {
		fprintf(f, " value ");
		pmPrintValue(f, vsp->valfmt, PM_TYPE_U32, vp, 1); 
	    }
	    fputc('\n', f);
	}
    }
}

#endif

/* Called when optFetch or _pmHash routines fail.  This is terminal. */
void
die(char *name, int sts)
{
    __pmNotifyErr(LOG_ERR, "%s error unrecoverable: %s\n", name, pmErrStr(sts));
    exit(1);
}

optreq_t *
findoptreq(pmID pmid, int inst)
{
    __pmHashNode	*hp;
    optreq_t	*rqp;
    optreq_t	*all_rqp = NULL;
    int		j;

    /*
     * Note:
     * The logic here assumes that for each metric-inst pair, there is
     * at most one optreq_t structure, corresponding to the logging
     * state of ON (mandatory or advisory) else OFF (mandatory).  Other
     * requests change the data structures, but do not leave optreq_t
     * structures lying about, i.e. MAYBE (mandatory) is the default,
     * and does not have to be explicitly stored, while OFF (advisory)
     * reverts to MAYBE (mandatory).
     * There is one exception to the above assumption, namely for
     * cases where the initial specification includes "all" instances,
     * then some later concurrent specification may refer to specific
     * instances ... in this case, the specific optreq_t structure is
     * the one that applies.
     */
    
    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if (rqp->r_numinst == 0) {
	    all_rqp = rqp;
	    continue;
	}
	for (j = 0; j < rqp->r_numinst; j++)
	    if (inst == rqp->r_instlist[j])
		return rqp;
    }

    if (all_rqp != NULL)
	return all_rqp;
    else
	return NULL;
}

/* Determine whether a metric is currently known.  Returns
 *	-1 if metric not known
 *	inclusive OR of the flags below if it is known
 */
#define MF_HAS_INDOM	0x1		/* has an instance domain */
#define MF_HAS_ALL	0x2		/* has an "all instances" */
#define MF_HAS_INST	0x4		/* has specific instance(s) */
#define MF_HAS_MAND	0x8		/* has at least one inst with mandatory */
					/* logging (or is mandatory if no indom) */
static int
find_metric(pmID pmid)
{
    __pmHashNode	*hp;
    optreq_t	*rqp;
    int		result = 0;
    int		found = 0;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if (found++ == 0)
	    if (rqp->r_desc->indom != PM_INDOM_NULL) {
		result |= MF_HAS_INDOM;
		if (rqp->r_numinst == 0)
		    result |= MF_HAS_ALL;
		else
		    result |= MF_HAS_INST;
	    }
	if (PMLC_GET_MAND(((task_t *)(rqp->r_fetch->f_aux))->t_state))
	    result |= MF_HAS_MAND;
    }
    return found ? result : -1;
}

/* Find an optreq_t suitable for adding a new instance */

/* Add a new metric (given a pmValueSet and a pmDesc) to the specified task.
 * Allocate and return a new task_t if the specified task pointer is nil.
 *
 * Note that this should only be called for metrics not currently in the
 * logging data structure.  All instances in the pmValueSet are added!
 */
static int
add_metric(pmValueSet *vsp, task_t **result)
{
    pmID	pmid = vsp->pmid;
    task_t	*tp = *result;
    optreq_t	*rqp;
    pmDesc	*dp;
    char	*name;
    int		sts, i, need = 0;

    dp = (pmDesc *)malloc(sizeof(pmDesc));
    if (dp == NULL) {
	__pmNoMem("add_metric: new pmDesc malloc", sizeof(pmDesc), PM_FATAL_ERR);
    }
    if ((sts = pmLookupDesc(pmid, dp)) < 0)
	die("add_metric: lookup desc", sts);
    if ((sts = pmNameID(pmid, &name)) < 0)
	die("add_metric: lookup name", sts);

    /* allocate a new task if null task pointer passed in */
    if (tp == NULL) {
	tp = calloc(1, sizeof(task_t));
	if (tp == NULL) {
	    __pmNoMem("add_metric: new task calloc", sizeof(task_t), PM_FATAL_ERR);
	}
	*result = tp;
    }

    /* add metric (and any instances specified) to task */
    i = tp->t_numpmid++;
    need = tp->t_numpmid * sizeof(pmID);
    if (!(tp->t_pmidlist = (pmID *)realloc(tp->t_pmidlist, need)))
	__pmNoMem("add_metric: new task pmidlist realloc", need, PM_FATAL_ERR);
    need = tp->t_numpmid * sizeof(char *);
    if (!(tp->t_namelist = (char **)realloc(tp->t_namelist, need)))
	__pmNoMem("add_metric: new task namelist realloc", need, PM_FATAL_ERR);
    need = tp->t_numpmid * sizeof(pmDesc);
    if (!(tp->t_desclist = (pmDesc *)realloc(tp->t_desclist, need)))
	__pmNoMem("add_metric: new task desclist realloc", need, PM_FATAL_ERR);
    tp->t_pmidlist[i] = pmid;
    tp->t_namelist[i] = name;
    tp->t_desclist[i] = *dp;	/* struct assignment */

    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
    if (rqp == NULL) {
	__pmNoMem("add_metric: new task optreq calloc", need, PM_FATAL_ERR);
    }
    rqp->r_desc = dp;

    /* Now copy instances if required.  Remember that metrics with singular
     * values actually have one instance specified to distinguish them from the
     * "all instances" case (which has no instances).  Use the pmDesc to check
     * for this.
     */
    if (dp->indom != PM_INDOM_NULL)
	need = rqp->r_numinst = vsp->numval;
    if (need) {
	need *= sizeof(rqp->r_instlist[0]);
	rqp->r_instlist = (int *)malloc(need);
	if (rqp->r_instlist == NULL) {
	    __pmNoMem("add_metric: new task optreq instlist malloc", need,
		     PM_FATAL_ERR);
	}
	for (i = 0; i < vsp->numval; i++)
	    rqp->r_instlist[i] = vsp->vlist[i].inst;
    }

    /* Add new metric to task's fetchgroup(s) and global hash table */
    __pmOptFetchAdd(&tp->t_fetch, rqp);
    linkback(tp);
    if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
	die("add_metric: __pmHashAdd", sts);
    return 0;
}

/* Return true if a request for a new logging state (newstate) will be honoured
 * when current state is curstate.
 */
static int
update_ok(int curstate, int newstate)
{
    /* If new state is advisory and current is mandatory, reject request.
     * Any new mandatory state is accepted.  If the new state is advisory
     * and the current state is advisory, it is accepted.
     * Note that a new state of maybe (mandatory maybe) counts as mandatory
     */
    if (PMLC_GET_MAND(newstate) == 0 && PMLC_GET_MAYBE(newstate) == 0 &&
	PMLC_GET_MAND(curstate))
	return 0;
    else
	return 1;
}

/* Given a task and a pmID, find an optreq_t associated with the task suitable
 * for inserting a new instance into.
 * The one with the smallest number of instances is chosen.  We could also
 * have just used the first, but smallest-first gives a more even distribution.
 */
static optreq_t *
find_instoptreq(task_t *tp, pmID pmid)
{
    optreq_t	*result = NULL;
    optreq_t	*rqp;
    int		ni = 0;
    __pmHashNode	*hp;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL;
	 hp = hp->next) {
	if (pmid != (pmID)hp->key)
	    continue;
	rqp = (optreq_t *)hp->data;
	if ((task_t *)rqp->r_fetch->f_aux != tp)
	    continue;
	if (rqp->r_numinst == 0)
	    continue;			/* don't want "all instances" cases */
	if (ni == 0 || rqp->r_numinst < ni) {
	    result = rqp;
	    ni = rqp->r_numinst;
	}
    }
    return result;
}

/* Delete an optreq_t from its task, free it and remove it from the hash list.
 */
static void
del_optreq(optreq_t *rqp)
{
    int		sts;
    task_t	*tp = (task_t *)rqp->r_fetch->f_aux;

    if ((sts = __pmOptFetchDel(&tp->t_fetch, rqp)) < 0)
	die("del_optreq: __pmOptFetchDel", sts);
    if ((sts = __pmHashDel(rqp->r_desc->pmid, (void *)rqp, &pm_hash)) < 0)
	die("del_optreq: __pmHashDel", sts);
    free(rqp->r_desc);
    if (rqp->r_numinst)
	free(rqp->r_instlist);
    free(rqp);
    /* TODO: remove pmid from task if that was the last optreq_t for it */
    /* TODO: remove task if last pmid removed */
}

/* Delete every instance of a given metric from the data structure.
 * The pmid is deleted from the pmidlist of every task containing an instance.
 * Return a pointer to the first pmDesc found (the only thing salvaged from the
 * smoking ruins), or nil if no instances were found.
 */
static pmDesc *
del_insts(pmID pmid)
{
    optreq_t	*rqp;
    __pmHashNode	*hp;
    task_t	*tp;
    pmDesc	*dp = NULL;
    int		i, sts, keep;

    for (hp = __pmHashSearch(pmid, &pm_hash); hp != NULL; ) {
	/* Do that BEFORE we nuke the node */
    	__pmHashNode * nextnode = hp->next;

	if (pmid == (pmID)hp->key) {
	    rqp = (optreq_t *)hp->data;
	    tp = (task_t *)rqp->r_fetch->f_aux;
	    if ((sts = __pmOptFetchDel(&tp->t_fetch, rqp)) < 0)
		die("del_insts: __pmOptFetchDel", sts);
	    if ((sts = __pmHashDel(pmid, (void *)rqp, &pm_hash)) < 0)
		die("del_insts: __pmHashDel", sts);

	    /* save the first pmDesc pointer for return and subsequent
	     *  re-use, but free all the others
	     */
	    if (dp != NULL)
		free(rqp->r_desc);
	    else
		dp = rqp->r_desc;

	    if (rqp->r_numinst)
		free(rqp->r_instlist);
	    free(rqp);

	    /* remove pmid from the task's pmid list */
	    for (i = 0; i < tp->t_numpmid; i++)
		if (tp->t_pmidlist[i] == pmid)
		    break;
	    keep = (tp->t_numpmid - 1 - i) * sizeof(tp->t_pmidlist[0]);
	    if (keep) {
		memmove(&tp->t_pmidlist[i], &tp->t_pmidlist[i+1], keep);
		memmove(&tp->t_desclist[i], &tp->t_desclist[i+1], keep);
		memmove(&tp->t_namelist[i], &tp->t_namelist[i+1], keep);
	    }

	    /* don't bother shrinking the pmidlist */
	    tp->t_numpmid--;
	    if (tp->t_numpmid == 0) {
		/* TODO: nuke the task if that was the last pmID */
	    }
	}
	hp = nextnode;
    }

    return dp;
}

/* Update an existing metric (given a pmValueSet) adding it to the specified
 * task. Allocate and return a new task_t if the specified task pointer is nil.
 */
static int
update_metric(pmValueSet *vsp, int reqstate, int mflags, task_t **result)
{
    pmID	pmid = vsp->pmid;
    task_t	*ntp = *result;		/* pointer to new task */
    task_t	*ctp;			/* pointer to current task */
    optreq_t	*rqp;
    pmDesc	*dp;
    int		i, j, inst;
    int		sts, need = 0;
    int		addpmid = 0;
    int		freedp;

    /* allocate a new task if null task pointer passed in */
    if (ntp == NULL) {
	ntp = calloc(1, sizeof(task_t));
	if (ntp == NULL) {
	    __pmNoMem("update_metric: new task calloc", sizeof(task_t),
		     PM_FATAL_ERR);
	}
	*result = ntp;
    }

    if ((mflags & MF_HAS_INDOM) == 0) {
	rqp = findoptreq(pmid, 0);
	ctp = (task_t *)(rqp->r_fetch->f_aux);
	if (!update_ok(ctp->t_state, reqstate))
	    return 1;

	/* if the new state is advisory off, just remove the metric */
	if ((PMLC_GET_MAYBE(reqstate)) ||
	    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0))
	    del_optreq(rqp);
	else {
	    /* update the optreq.  For single valued metrics there are no
	     * instances involved so the sole optreq can just be re-used.
	     */
	    if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
		die("update_metric: 1 metric __pmOptFetchDel", sts);
	    __pmOptFetchAdd(&ntp->t_fetch, rqp);
	    linkback(ntp);
	    addpmid = 1;
	}
    }
    else {
	/* metric has an instance domain */
	if (vsp->numval > 0) {
	    /* tricky: since optFetch can't handle instance profiles of the
	     * form "all except these specific instances", and managing it
	     * manually is just too hard, reject requests for specific
	     * metric instances if "all instances" of the metric are already
	     * being logged.
	     * Note: advisory off "all instances" is excepted since ANY request
	     * overrides and advisory off.  E.g. "advisory off all" followed by
	     * "advisory on someinsts" turns on advisory logging for
	     * "someinsts".  mflags will be zero for "advisory off" metrics.
	     */
	    if (mflags & MF_HAS_ALL)
		return 1;		/* can't turn "all" into specific insts */

	    for (i = 0; i < vsp->numval; i++) {
		dp = NULL;
		freedp = 0;
		inst = vsp->vlist[i].inst;
		rqp = findoptreq(pmid, inst);
		if (rqp != NULL) {
		    dp = rqp->r_desc;
		    ctp = (task_t *)(rqp->r_fetch->f_aux);
		    /* no work required if new task and current are the same */
		    if (ntp == ctp)
			continue;
		    if (!update_ok(ctp->t_state, reqstate))
			continue;

		    /* remove inst's group from current task */
		    if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
			die("update_metric: instance add __pmOptFetchDel", sts);

		    /* put group back if there are any instances left */
		    if (rqp->r_numinst > 1) {
			/* remove inst from group */
			for (j = 0; j < rqp->r_numinst; j++)
			    if (inst == rqp->r_instlist[j])
				break;
			/* don't call memmove to move zero bytes */
			if (j < rqp->r_numinst - 1)
			    memmove(&rqp->r_instlist[j], &rqp->r_instlist[j+1],
				    (rqp->r_numinst - 1 - j) *
				    sizeof(rqp->r_instlist[0]));
			rqp->r_numinst--;
			/* (don't bother realloc-ing the instlist to a smaller size) */

			__pmOptFetchAdd(&ctp->t_fetch, rqp);
			linkback(ctp);
			/* no need to update hash list, rqp already there */
		    }
		    /* if that was the last instance, free the group */
		    else {
			if (( sts = __pmHashDel(pmid, (void *)rqp, &pm_hash)) < 0)
			    die("update_metric: instance __pmHashDel", sts);
			freedp = 1;
			free(rqp->r_instlist);
			free(rqp);
		    }
		}

		/* advisory off (mandatory maybe) metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    if (freedp)
			free(dp);
		    continue;
		}
		addpmid = 1;

		/* try to find an existing optreq_t for the instance */
		rqp = find_instoptreq(ntp, pmid);
		if (rqp != NULL) {
		    if ((sts = __pmOptFetchDel(&ntp->t_fetch, rqp)) < 0)
			die("update_metric: instance add __pmOptFetchDel", sts);
		}
		/* no existing optreq_t found, allocate & populate a new one */
		else {
		    rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
		    if (rqp == NULL) {
			__pmNoMem("update_metric: optreq calloc",
				 sizeof(optreq_t), PM_FATAL_ERR);
		    }
		    /* if the metric existed but the instance didn't, we don't
		     * have a valid pmDesc (dp), so find one.
		     */
		    if (dp == NULL)  {
			/* find metric and associated pmDesc */
			__pmHashNode	*hp;

			for (hp = __pmHashSearch(pmid, &pm_hash);
			     hp != NULL; hp = hp->next) {
			    if (pmid == (pmID)hp->key)
				break;
			}
			assert(hp != NULL);
			dp = ((optreq_t *)hp->data)->r_desc;
		    }
		    /* recycle pmDesc from the old group, if possible */
		    if (freedp) {
			rqp->r_desc = dp;
			freedp = 0;
		    }
		    /* otherwise allocate & copy a new pmDesc via dp */
		    else {
			need = sizeof(pmDesc);
			rqp->r_desc = (pmDesc *)malloc(need);
			if (rqp->r_desc == NULL) {
			    __pmNoMem("update_metric: new inst pmDesc malloc",
				     need, PM_FATAL_ERR);
			}
			memcpy(rqp->r_desc, dp, need);
		    }
		    if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
			die("update_metric: __pmHashAdd", sts);
		}
		    
		need = (rqp->r_numinst + 1) * sizeof(rqp->r_instlist[0]);
		rqp->r_instlist = (int *)realloc(rqp->r_instlist, need);
		if (rqp->r_instlist == NULL) {
		    __pmNoMem("update_metric: inst list resize", need,
			     PM_FATAL_ERR);
		}
		rqp->r_instlist[rqp->r_numinst++] = inst;
		__pmOptFetchAdd(&ntp->t_fetch, rqp);
		linkback(ntp);
		if (freedp)
		    free(dp);
	    }
	}
	/* the vset has numval == 0, a request for "all instances" */
	else {
	    /* if the metric is a singular instance that has mandatory logging
	     * or has at least one instance with mandatory logging on, a
	     * request for advisory logging cannot be honoured
	     */
	    if ((mflags & MF_HAS_MAND) &&
		PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_MAYBE(reqstate) == 0)
		return 1;

	    if (mflags & MF_HAS_ALL) {
		/* if there is an "all instances" for the metric, it will be
		 * the only optreq_t for the metric
		 */
		rqp = findoptreq(pmid, 0);
		ctp = (task_t *)rqp->r_fetch->f_aux;

		/* if the metric is "advisory on, all instances"  and the
		 * request is for "mandatory maybe, all instances" the current
		 * advisory logging state of the metric is retained
		 */
		if (PMLC_GET_MAND(ctp->t_state) == 0 && PMLC_GET_MAYBE(reqstate))
		    return 0;

		/* advisory off & mandatory maybe metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    del_optreq(rqp);
		    return 0;
		}

		addpmid = 1;
		if ((sts = __pmOptFetchDel(&ctp->t_fetch, rqp)) < 0)
		    die("update_metric: all inst __pmOptFetchDel", sts);
		/* don't delete from hash list, rqp re-used */
		__pmOptFetchAdd(&ntp->t_fetch, rqp);
		linkback(ntp);
	    }
	    else {
		/* there are one or more specific instances for the metric.
		 * The metric cannot have an "all instances" at the same time.
		 *
		 * if the request is for "mandatory maybe, all instances" and
		 * the only instances of the metric all have advisory logging
		 * on, retain the current advisory semantics.
		 */
		if (PMLC_GET_MAYBE(reqstate) &&
		    (mflags & MF_HAS_INST) && !(mflags & MF_HAS_MAND))
		    return 0;

		dp = del_insts(pmid);

		/* advisory off (mandatory maybe) metrics don't get put into
		 * the data structure
		 */
		if (PMLC_GET_MAYBE(reqstate) ||
		    (PMLC_GET_MAND(reqstate) == 0 && PMLC_GET_ON(reqstate) == 0)) {
		    free(dp);
		    return 0;
		}

		addpmid = 1;
		rqp = (optreq_t *)calloc(1, sizeof(optreq_t));
		if (rqp == NULL) {
		    __pmNoMem("update_metric: all inst calloc",
			     sizeof(optreq_t), PM_FATAL_ERR);
		}
		rqp->r_desc = dp;
		__pmOptFetchAdd(&ntp->t_fetch, rqp);
		linkback(ntp);
		if ((sts = __pmHashAdd(pmid, (void *)rqp, &pm_hash)) < 0)
		    die("update_metric: all inst __pmHashAdd", sts);
	    }
	}
    }

    if (!addpmid)
	return 0;

    /* add pmid to new task if not already there */
    for (i = 0; i < ntp->t_numpmid; i++)
	if (pmid == ntp->t_pmidlist[i])
	    break;
    if (i >= ntp->t_numpmid) {
	pmDesc	desc;
	char	*name;
	int	need;

	if ((sts = pmLookupDesc(pmid, &desc)) < 0)
	    die("update_metric: cannot lookup desc", sts);
	if ((sts = pmNameID(pmid, &name)) < 0)
	    die("update_metric: cannot lookup name", sts);

	need = (ntp->t_numpmid + 1) * sizeof(pmID);
	if (!(ntp->t_pmidlist = (pmID *)realloc(ntp->t_pmidlist, need)))
	    __pmNoMem("update_metric: grow task pmidlist", need, PM_FATAL_ERR);
	need = (ntp->t_numpmid + 1) * sizeof(char *);
	if (!(ntp->t_namelist = (char **)realloc(ntp->t_namelist, need)))
	    __pmNoMem("update_metric: grow task namelist", need, PM_FATAL_ERR);
	need = (ntp->t_numpmid + 1) * sizeof(pmDesc);
	if (!(ntp->t_desclist = (pmDesc *)realloc(ntp->t_desclist, need)))
	    __pmNoMem("update_metric: grow task desclist", need, PM_FATAL_ERR);
	i = ntp->t_numpmid;
	ntp->t_pmidlist[i] = pmid;
	ntp->t_namelist[i] = name;
	ntp->t_desclist[i] = desc;
	ntp->t_numpmid++;
    }
    return 0;
}

/* Given a state and a delta, return the first matching task.
 * Return NULL if a matching task was not found.
 */
task_t *
find_task(int state, struct timeval *delta)
{
    task_t	*tp;

    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	if (state == (tp->t_state & 0x3) &&  /* MAND|ON */
	    delta->tv_sec == tp->t_delta.tv_sec &&
	    delta->tv_usec == tp->t_delta.tv_usec)
	    break;
    }
    return tp;
}

/* Return a mask containing the history flags for a given metric/instance.
 * the history flags indicate whether the metric/instance is in the log at all
 * and whether the last fetch of the metric/instance was successful.
 *
 * The result is suitable for ORing into the result returned by a control log
 * request.
 */
static int
gethistflags(pmID pmid, int inst)
{
    __pmHashNode	*hp;
    pmidhist_t		*php;
    insthist_t		*ihp;
    int			i, found;
    int			val;

    for (hp = __pmHashSearch(pmid, &hist_hash); hp != NULL; hp = hp->next)
	if ((pmID)hp->key == pmid)
	    break;
    if (hp == NULL)
	return 0;
    php = (pmidhist_t *)hp->data;
    ihp = &php->ph_instlist[0];
    val = 0;
    if (php->ph_indom != PM_INDOM_NULL) {
	for (i = 0; i < php->ph_numinst; i++, ihp++)
	    if (ihp->ih_inst == inst)
		break;
	found = i < php->ph_numinst;
    }
    else
	found = php->ph_numinst > 0;
    if (found) {
	PMLC_SET_INLOG(val, 1);
	val |= ihp->ih_flags;		/* only "available flag" is ever set */
    }
    return val;
}

/* take a pmResult (from a control log request) and half-clone it: return a
 * pointer to a new pmResult struct which shares the pmValueSets in the
 * original that have numval > 0, and has null pointers for the pmValueSets
 * in the original with numval <= 0
 */
static pmResult *
siamise_request(pmResult *request)
{
    int		i, need;
    pmValueSet	*vsp;
    pmResult	*result;

    need = sizeof(pmResult) + (request->numpmid - 1) * sizeof(pmValueSet *);
    result = (pmResult *)malloc(need);
    if (result == NULL) {
	__pmNoMem("siamise_request: malloc pmResult", need, PM_FATAL_ERR);
    }
    for (i = 0; i < request->numpmid; i++) {
	vsp = request->vset[i];
	if (vsp->numval > 0)
	    result->vset[i] = request->vset[i];
	else
	    result->vset[i] = NULL;
    }
    result->timestamp = request->timestamp; /* structure assignment */
    result->numpmid = request->numpmid;

    return result;
}

/* Temporarily borrow a bit in the metric/instance history to indicate that
 * the instance currently exists in the instance domain.  The macros below
 * set and get the bit, which is cleared after we are finished with it here.
 */

#define PMLC_SET_USEINDOM(val, flag) (val = (val & ~0x1000) | (flag << 12 ))
#define PMLC_GET_USEINDOM(val) ((val & 0x1000) >> 12)

/* create a pmValueSet large enough to contain the union of the current
 * instance domain of the specified metric and any previous instances from
 * the history list.
 */
static pmValueSet *
build_vset(pmID pmid, int usehist)
{
    __pmHashNode		*hp;
    pmidhist_t		*php = NULL;
    insthist_t		*ihp;
    int			need = 0;
    int			i, numindom = 0;
    pmDesc		desc;
    int			have_desc;
    int			*instlist = NULL;
    char		**namelist = NULL;
    pmValueSet		*vsp;

   if (usehist) {
	/* find the number of instances of the metric in the history (1 if
	 * single-valued metric)
	 */
	for (hp = __pmHashSearch(pmid, &hist_hash); hp != NULL; hp = hp->next)
	    if ((pmID)hp->key == pmid)
		break;
	if (hp != NULL) {
	    php = (pmidhist_t *)hp->data;
	    need = php->ph_numinst;
	}
    }
    /*
     * get the current instance domain, so that if the metric hasn't been
     * logged yet a sensible result is returned.
     */
    if ((have_desc = pmLookupDesc(pmid, &desc)) < 0)
	goto no_info;
    if (desc.indom == PM_INDOM_NULL)
	need = 1;			/* will be same in history */
    else {
	int	j;
	    
	if ((numindom = pmGetInDom(desc.indom, &instlist, &namelist)) < 0) {
	    have_desc = numindom;
	    goto no_info;
	}
	/* php will be null if usehist is false or there is no history yet */
	if (php == NULL)
	    need = numindom;		/* no history => use indom */
	else
	    for (i = 0; i < numindom; i++) {
		int	inst = instlist[i];
		
		for (j = 0; j < php->ph_numinst; j++)
		    if (inst == php->ph_instlist[j].ih_inst)
			break;
		/*
		 * if instance is in history but not instance domain, leave
		 * extra space for it in vset, otherwise use the USEINDOM
		 * flag to avoid another NxM comparison when building the vset
		 * instances later.
		 */
		if (j >= php->ph_numinst)
		    need++;
		else
		    PMLC_SET_USEINDOM(php->ph_instlist[j].ih_flags, 1);
	    }
    }

no_info:

    need = sizeof(pmValueSet) + (need - 1) * sizeof(pmValue);
    vsp = (pmValueSet *)malloc(need);
    if (vsp == NULL) {
	__pmNoMem("build_vset for control/enquire", need, PM_FATAL_ERR);
    }
    vsp->pmid = pmid;
    if (have_desc < 0) {
	vsp->numval = have_desc;
    }
    else if (desc.indom == PM_INDOM_NULL) {
	vsp->vlist[0].inst = PM_IN_NULL;
	vsp->numval = 1;
    }
    else {
	int	j;
	
	i = 0;
	/* get instances out of instance domain first */
	if (numindom > 0)
	    for (j = 0; j < numindom; j++)
		vsp->vlist[i++].inst = instlist[j];

	/* then any not in instance domain from history */
	if (php != NULL) {
	    ihp = &php->ph_instlist[0];
	    for (j = 0; j < php->ph_numinst; j++, ihp++)
		if (PMLC_GET_USEINDOM(ihp->ih_flags))
		    /* it's already in the indom */
		    PMLC_SET_USEINDOM(ihp->ih_flags, 0);
		else
		    vsp->vlist[i++].inst = ihp->ih_inst;
	}
	vsp->numval = i;
    }
    if (instlist)
	free(instlist);
    if (namelist)
	free(namelist);
    
    return vsp;
}

static int
do_control(__pmPDU *pb)
{
    int			sts;
    int			control;
    int			state;
    int			delta;
    pmResult		*request;
    pmResult		*result;
    int			siamised = 0;	/* the verb from siamese (as in twins) */
    int			i;
    int			j;
    int			val;
    pmValueSet		*vsp;
    optreq_t		*rqp;
    task_t		*tp;
    time_t		now;
    int			reqstate = 0;

    /*
     * TODO	- encoding for logging interval in requests and results?
     */
    if ((sts = __pmDecodeLogControl(pb, &request, &control, &state, &delta)) < 0)
	return sts;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	fprintf(stderr, "do_control: control=%d state=%d delta=%d request ...\n",
		control, state, delta);
	dumpcontrol(stderr, request, 0);
    }
#endif

    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	time(&now);
	fprintf(stderr, "\n%s", ctime(&now));
	fprintf(stderr, "pmlc request from %s: %s",
	    pmlc_host, control == PM_LOG_MANDATORY ? "mandatory" : "advisory");
	if (state == PM_LOG_ON) {
	    if (delta == 0)
		fprintf(stderr, " on once\n");
	    else
		fprintf(stderr, " on %.1f sec\n", (float)delta/1000);
	}
	else if (state == PM_LOG_OFF)
	    fprintf(stderr, " off\n");
	else
	    fprintf(stderr, " maybe\n");
    }

    /*
     * access control checks
     */
    sts = 0;
    switch (control) {
	case PM_LOG_MANDATORY:
	    if (denyops & PM_OP_LOG_MAND)
		sts = PM_ERR_PERMISSION;
	    break;

	case PM_LOG_ADVISORY:
	    if (denyops & PM_OP_LOG_ADV)
		sts = PM_ERR_PERMISSION;
	    break;

	case PM_LOG_ENQUIRE:
	    /*
	     * Don't need to check [access] as you have to have _some_
	     * permission (at least one of PM_OP_LOG_ADV or PM_OP_LOG_MAND
	     * and PM_OP_LOG_ENQ) to make a connection ... and if you
	     * have either PM_OP_LOG_ADV or PM_OP_LOG_MAND it makes no
	     * sense to deny PM_OP_LOG_ENQ operations.
	     */
	    break;

	default:
	    fprintf(stderr, "Bad control PDU type %d\n", control);
	    sts = PM_ERR_IPC;
	    break;
    }
    if (sts < 0) {
	fprintf(stderr, "Error: %s\n", pmErrStr(sts));
	if ((sts = __pmSendError(clientfd, FROM_ANON, sts)) < 0)
	    __pmNotifyErr(LOG_ERR,
			 "do_control: error sending Error PDU to client: %s\n",
			 pmErrStr(sts));
	pmFreeResult(request);
	return sts;
    }

    /* handle everything except PM_LOG_ENQUIRE */
    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	/* update the logging status of metrics */

	task_t		*newtp = NULL; /* task for metrics/insts in request */
	struct timeval	tdelta = { 0 };
	int		newtask;
	int		mflags;

	/* convert state and control to the bitmask used in pmlogger and values
	 * returned in results.  Remember that reqstate starts with nothing on.
	 */
	if (state == PM_LOG_ON)
	    PMLC_SET_ON(reqstate, 1);
	else
	    PMLC_SET_ON(reqstate, 0);
	if (control == PM_LOG_MANDATORY) {
	    if (state == PM_LOG_MAYBE)
		/* mandatory+maybe => maybe+advisory+off  */
		PMLC_SET_MAYBE(reqstate, 1);
	    else
		PMLC_SET_MAND(reqstate, 1);
	}

	/* try to find an existing task for the request
	 * Never return a "once only" task, it may have gone off already and just
	 * be hanging around like a bad smell.
	 */
	if (delta != 0) {
	    tdelta.tv_sec = delta / 1000;
	    tdelta.tv_usec = (delta % 1000) * 1000;
	    newtp = find_task(reqstate, &tdelta);
	}
	newtask = (newtp == NULL);

	for (i = 0; i < request->numpmid; i++) {
	    vsp = request->vset[i];
	    if (vsp->numval < 0)
		/*
		 * request is malformed, as we cannot control logging
		 * for an undefined instance ... there is no way to
		 * return an error from here, so simply ignore this
		 * metric
		 */
		continue;
	    mflags = find_metric(vsp->pmid);
	    if (mflags < 0) {
		/* only add new metrics if they are ON or MANDATORY OFF
		 * Careful: mandatory+maybe is mandatory+maybe+off
		 */
		if (PMLC_GET_ON(reqstate) ||
		    (PMLC_GET_MAND(reqstate) && !PMLC_GET_MAYBE(reqstate)))
		    add_metric(vsp, &newtp);
	    }
	    else
		/* already a specification for this metric */
		update_metric(vsp, reqstate, mflags, &newtp);
	}

	/* schedule new logging task if new metric(s) specified */
	if (newtask && newtp != NULL) {
	    if (newtp->t_fetch == NULL) {
		/* the new task ended up with no fetch groups, throw it away */
		if (newtp->t_pmidlist != NULL)
		    free(newtp->t_pmidlist);
		free(newtp);
	    }
	    else {
		/* link new task into tasklist */
		newtp->t_next = tasklist;
		tasklist = newtp;

		/* use only the MAND/ADV and ON/OFF bits of reqstate */
		newtp->t_state = PMLC_GET_STATE(reqstate);
		if (PMLC_GET_ON(reqstate)) {
		    newtp->t_delta = tdelta;
		    newtp->t_afid = __pmAFregister(&tdelta, (void *)newtp,
					       log_callback);
		}
		else
		    newtp->t_delta.tv_sec = newtp->t_delta.tv_usec = 0;
		linkback(newtp);
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
	dumpit();
#endif

    /* just ignore advisory+maybe---the returned pmResult will have the metrics
     * in their original state indicating that the request could not be
     * satisfied.
     */

    result = request;
    result->timestamp.tv_sec = result->timestamp.tv_usec = 0;	/* for purify */
    /* write the current state of affairs into the result _pmResult */
    for (i = 0; i < request->numpmid; i++) {

	if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
	    char	**names;

	    sts = pmNameAll(request->vset[i]->pmid, &names);
	    if (sts < 0)
		fprintf(stderr, "  metric: %s", pmIDStr(request->vset[i]->pmid));
	    else {
		fprintf(stderr, "  metric: ");
		__pmPrintMetricNames(stderr, sts, names, " or ");
		free(names);
	    }
	}

	if (request->vset[i]->numval <= 0 && !siamised) {
	    result = siamise_request(request);
	    siamised = 1;
	}
	/*
	 * pmids with numval <= 0 in the request have a null vset ptr in the
	 * in the corresponding place in the siamised result.
	 */
	if (result->vset[i] != NULL)
	    vsp = result->vset[i];
	else {
	    /* the result should also contain the history for an all instances
	     * enquire request.  Control requests just get the current indom
	     * since the user of pmlc really wants to see what's being logged
	     * now rather than in the past.
	     */
	    vsp = build_vset(request->vset[i]->pmid, control == PM_LOG_ENQUIRE);
	    result->vset[i] = vsp;
	}
	vsp->valfmt = PM_VAL_INSITU;
	for (j = 0; j < vsp->numval; j++) {
	    rqp = findoptreq(vsp->pmid, vsp->vlist[j].inst);
	    val = 0;
	    if (rqp == NULL) {
		PMLC_SET_STATE(val, 0);
		PMLC_SET_DELTA(val, 0);
	    }
	    else {
		tp = rqp->r_fetch->f_aux;
		PMLC_SET_STATE(val, tp->t_state);
		PMLC_SET_DELTA(val, (tp->t_delta.tv_sec*1000 + tp->t_delta.tv_usec/1000));
	    }

	    val |= gethistflags(vsp->pmid, vsp->vlist[j].inst);
	    vsp->vlist[j].value.lval = val;

	    if (control == PM_LOG_MANDATORY || control == PM_LOG_ADVISORY) {
		int	expstate = 0;
		int	statemask = 0;
		int	expdelta;
		if (rqp != NULL && rqp->r_desc->indom != PM_INDOM_NULL) {
		    char	*p;
		    if (j == 0)
			fputc('\n', stderr);
		    if (pmNameInDom(rqp->r_desc->indom, vsp->vlist[j].inst, &p) >= 0) {
			fprintf(stderr, "    instance: %s", p);
			free(p);
		    }
		    else
			fprintf(stderr, "    instance: #%d", vsp->vlist[j].inst);
		}
		else {
		    /* no pmDesc ... punt */
		    if (vsp->numval > 1 || vsp->vlist[j].inst != PM_IN_NULL) {
			if (j == 0)
			    fputc('\n', stderr);
			fprintf(stderr, "    instance: #%d", vsp->vlist[j].inst);
		    }
		}
		if (state != PM_LOG_MAYBE) {
		    if (control == PM_LOG_MANDATORY)
			PMLC_SET_MAND(expstate, 1);
		    else
			PMLC_SET_MAND(expstate, 0);
		    if (state == PM_LOG_ON)
			PMLC_SET_ON(expstate, 1);
		    else
			PMLC_SET_ON(expstate, 0);
		    PMLC_SET_MAND(statemask, 1);
		    PMLC_SET_ON(statemask, 1);
		}
		else {
		    PMLC_SET_MAND(expstate, 0);
		    PMLC_SET_MAND(statemask, 1);
		}
		expdelta = PMLC_GET_ON(expstate) ? delta : 0;
		if ((PMLC_GET_STATE(val) & statemask) != expstate ||
		    PMLC_GET_DELTA(val) != expdelta)
			fprintf(stderr, " [request failed]");
		fputc('\n', stderr);
	    }
	}
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOG) {
	__pmDumpResult(stderr, result);
    }
#endif

    if ((sts = __pmSendResult(clientfd, FROM_ANON, result)) < 0)
		__pmNotifyErr(LOG_ERR,
			     "do_control: error sending Error PDU to client: %s\n",
			     pmErrStr(sts));

    if (siamised) {
	for (i = 0; i < request->numpmid; i++)
	    if (request->vset[i]->numval <= 0)
		free(result->vset[i]);
	free(result);
    }
    pmFreeResult(request);

    return 0;
}

/*
 * sendstatus
 */
static int
sendstatus(void)
{
    int				rv;
    int				end;
    int				version;
    static int			firsttime = 1;
    static char			*tzlogger;
    struct timeval		now;

    if (firsttime) {
        tzlogger = __pmTimezone();
	firsttime = 0;
    }

    if ((version = __pmVersionIPC(clientfd)) < 0)
	return version;

    if (version >= LOG_PDU_VERSION2) {
	__pmLoggerStatus		ls;

	if ((ls.ls_state = logctl.l_state) == PM_LOG_STATE_NEW)
	    ls.ls_start.tv_sec = ls.ls_start.tv_usec = 0;
	else
	    memcpy(&ls.ls_start, &logctl.l_label.ill_start, sizeof(ls.ls_start));
	memcpy(&ls.ls_last, &last_stamp, sizeof(ls.ls_last));
	__pmtimevalNow(&now);
	ls.ls_timenow.tv_sec = (__int32_t)now.tv_sec;
	ls.ls_timenow.tv_usec = (__int32_t)now.tv_usec;
	ls.ls_vol = logctl.l_curvol;
	ls.ls_size = ftell(logctl.l_mfp);
	assert(ls.ls_size >= 0);

	/* be careful of buffer size mismatches when copying strings */
	end = sizeof(ls.ls_hostname) - 1;
	strncpy(ls.ls_hostname, logctl.l_label.ill_hostname, end);
	ls.ls_hostname[end] = '\0';
        /* BTW, that string should equal pmcd_host[]. */

        /* NB: FQDN cleanup: there is no such thing as 'the fully
           qualified domain name' of a server: it may have several or
           none; the set may have changed since the time the log
           archive was collected.  Now that we store the then-current
           pmcd.hostname in the ill_hostname (and thus get it reported
           in ls_hostname), we could pass something else informative
           in the ls_fqdn slot.  Namely, pmcd_host_conn[], which is the
           access path pmlogger's using to get to the pmcd. */
	end = sizeof(ls.ls_fqdn) - 1;
        strncpy(ls.ls_fqdn, pmcd_host_conn, end);
	ls.ls_fqdn[end] = '\0';

	end = sizeof(ls.ls_tz) - 1;
	strncpy(ls.ls_tz, logctl.l_label.ill_tz, end);
	ls.ls_tz[end] = '\0';
	end = sizeof(ls.ls_tzlogger) - 1;
	if (tzlogger != NULL)
	    strncpy(ls.ls_tzlogger, tzlogger, end);
	else
	    end = 0;
	ls.ls_tzlogger[end] = '\0';

	rv = __pmSendLogStatus(clientfd, &ls);
    }
    else
	rv = PM_ERR_IPC;
    return rv;
}

static int
do_request(__pmPDU *pb)
{
    int		sts;
    int		type;

    if ((sts = __pmDecodeLogRequest(pb, &type)) < 0) {
	__pmNotifyErr(LOG_ERR, "do_request: error decoding PDU: %s\n", pmErrStr(sts));
	return PM_ERR_IPC;
    }

    switch (type) {
	case LOG_REQUEST_STATUS:
	    /*
	     * Don't need to check [access] as you have to have _some_
	     * permission (at least one of PM_OP_LOG_ADV or PM_OP_LOG_MAND
	     * and PM_OP_LOG_ENQ) to make a connection ... and if you
	     * have either PM_OP_LOG_ADV or PM_OP_LOG_MAND it makes no
	     * sense to deny LOG_REQUEST_STATUS operations.
	     * Also, this is needed internally by pmlc to discover pmcd's
	     * hostname.
	     */
	    sts = sendstatus();
	    break;

	case LOG_REQUEST_NEWVOLUME:
	    if (denyops & PM_OP_LOG_MAND)
		sts = __pmSendError(clientfd, FROM_ANON, PM_ERR_PERMISSION);
	    else {
		sts = newvolume(VOL_SW_PMLC);
		if (sts >= 0)
		    sts = logctl.l_label.ill_vol;
		sts = __pmSendError(clientfd, FROM_ANON, sts);
	    }
	    break;

	case LOG_REQUEST_SYNC:
	    /*
	     * Don't need to check access controls, as this is now
	     * a no-op with unbuffered I/O from pmlogger.
	     *
	     * Do nothing, simply send status 0 back to pmlc.
	     */
	    sts = __pmSendError(clientfd, FROM_ANON, 0);
	    break;

	/*
	 * QA support ... intended for error injection
	 * If the request is > QA_OFF then this is a code to enable
	 * a specific style of error behaviour.  If the request
	 * is QA_OFF, this disables the error behaviour.
	 *
	 * Supported behaviours.
	 * QA_SLEEPY
	 *	After this exchange with pmlc, sleep for 5 seconds
	 * 	after each incoming pmlc request ... allows testing
	 * 	of timeout logic in pmlc
	 */

	case QA_OFF:
	    if (denyops & PM_OP_LOG_MAND)
		sts = __pmSendError(clientfd, FROM_ANON, PM_ERR_PERMISSION);
	    else {
		qa_case = 0;
		sts = __pmSendError(clientfd, FROM_ANON, 0);
	    }
	    break;

	case QA_SLEEPY:
	    if (denyops & PM_OP_LOG_MAND)
		sts = __pmSendError(clientfd, FROM_ANON, PM_ERR_PERMISSION);
	    else {
		qa_case = type;
		sts = __pmSendError(clientfd, FROM_ANON, 0);
	    }
	    break;

	default:
	    fprintf(stderr, "do_request: bad request type %d\n", type);
	    sts = PM_ERR_IPC;
	    break;
    }
    return sts;
}

static int
do_creds(__pmPDU *pb)
{
    int		i;
    int		sts;
    int		version = UNKNOWN_VERSION;
    int		credcount;
    int		sender;
    __pmCred	*credlist = NULL;

    if ((sts = __pmDecodeCreds(pb, &sender, &credcount, &credlist)) < 0) {
	__pmNotifyErr(LOG_ERR, "do_creds: error decoding PDU: %s\n", pmErrStr(sts));
		return PM_ERR_IPC;
    }

    for (i = 0; i < credcount; i++) {
	if (credlist[i].c_type == CVERSION) {
	    version = credlist[i].c_vala;
	    if ((sts = __pmSetVersionIPC(clientfd, version)) < 0) {
		free(credlist);
		return sts;
	    }
	}
    }

    if (credlist)
	free(credlist);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1)
	fprintf(stderr, "do_creds: pmlc version=%d\n", version);
#endif

    return sts;
}

/*
 * Service a request from the pmlogger client.
 * Return non-zero if the client has closed the connection.
 */
int
client_req(void)
{
    int		sts;
    __pmPDU	*pb;
    __pmPDUHdr	*php;
    int		pinpdu;

    if ((pinpdu = sts = __pmGetPDU(clientfd, ANY_SIZE, TIMEOUT_DEFAULT, &pb)) <= 0) {
	if (sts != 0)
	    fprintf(stderr, "client_req: %s\n", pmErrStr(sts));
	return 1;
    }
    if (qa_case == QA_SLEEPY) {
	/* error injection - delay before processing and responding */
	sleep(5);
    }
    php = (__pmPDUHdr *)pb;
    sts = 0;

    switch (php->type) {
	case PDU_CREDS:		/* version 2 PDU */
	    sts = do_creds(pb);
	    break;
	case PDU_LOG_REQUEST:	/* version 2 PDU */
	    sts = do_request(pb);
	    break;
	case PDU_LOG_CONTROL:	/* version 2 PDU */
	    sts = do_control(pb);
	    break;
	default:		/*  unknown PDU  */
	    fprintf(stderr, "client_req: bad PDU type 0x%x\n", php->type);
	    sts = PM_ERR_IPC;
	    break;
    }
    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);
    
    if (sts >= 0)
	return 0;
    else {
	/* the client isn't playing by the rules */
	__pmSendError(clientfd, FROM_ANON, sts);
	return 1;
    }
}
