/*
 * Copyright (c) 2014-2018,2021-2025 Red Hat.
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

int	last_log_offset;

#define SET_DERIVED_LOGGED(x) pmID_build(pmID_domain(x), 2048 | pmID_cluster(x), pmID_item(x))
#define CLEAR_DERIVED_LOGGED(x) pmID_build(pmID_domain(x), ~2048 & pmID_cluster(x), pmID_item(x))

/*
 * We have a single context with the pmcd providing the results,
 * hence need to send the profile each time.
 */
static int	one_context = 1;

__pmTimestamp	last_stamp;
__pmHashCtl	hist_hash;

/*
 * one-trip guard for first pmResult
 */
static int first_result = 1;

/*
 * These structures allow us to keep track of the _last_ fetch
 * for each fetch in each AF group ... needed to track changes in
 * instance availability.
 */
typedef struct _lastfetch {
    struct _lastfetch	*lf_next;
    fetchctl_t		*lf_fp;
    __pmResult		*lf_resp;
} lastfetch_t;

typedef struct _AFctl {
    struct _AFctl	*ac_next;
    int			ac_afid;
    lastfetch_t		*ac_fetch;
} AFctl_t;

static AFctl_t		*achead = (AFctl_t *)0;

/* clear the "metric/instance was available at last fetch" flag for each metric
 * and instance in the specified fetchgroup.
 */
static void
clearavail(fetchctl_t *fcp)
{
    indomctl_t	*idp;
    pmID	pmid;
    pmidctl_t	*pmp;
    pmidhist_t	*php;
    insthist_t	*ihp;
    __pmHashNode	*hp;
    int		i, inst;
    int		j;

    for (idp = fcp->f_idp; idp != (indomctl_t *)0; idp = idp->i_next) {
	for (pmp = idp->i_pmp; pmp != (pmidctl_t *)0; pmp = pmp->p_next) {
	    /* find the metric if it's in the history hash table */
	    pmid = pmp->p_pmid;
	    for (hp = __pmHashSearch(pmid, &hist_hash); hp != (__pmHashNode *)0; hp = hp->next)
		if (pmid == (pmID)hp->key)
		    break;
	    if (hp == (__pmHashNode *)0) {
		/* not in history, no flags to update */
		if (pmDebugOptions.appl8)
		    fprintf(stderr, "%s: PMID %s no history\n", __FUNCTION__, pmIDStr(pmid));
		continue;
	    }
	    php = (pmidhist_t *)hp->data;

	    /* now we have the metric's entry in the history */

	    if (idp->i_indom != PM_INDOM_NULL) {
		/*
		 * for each instance in the profile for this metric, find
		 * the history entry for the instance if it exists and
		 * reset the "was available at last fetch" flag
		 */
		if (idp->i_numinst) {
		    if (pmDebugOptions.appl8)
			fprintf(stderr, "%s: PMID %s clear %d profile instances\n", __FUNCTION__, pmIDStr(pmid), idp->i_numinst);
		    for (i = 0; i < idp->i_numinst; i++) {
			inst = idp->i_instlist[i];
			ihp = &php->ph_instlist[0];
			for (j = 0; j < php->ph_numinst; j++, ihp++)
			    if (ihp->ih_inst == inst) {
				PMLC_SET_AVAIL(ihp->ih_flags, 0);
				break;
			    }
		    }
		}
		else {
		    /*
		     * if the profile specifies "all instances" clear EVERY
		     * instance's "available" flag
		     * NOTE: even instances that don't exist any more
		     */
		    if (pmDebugOptions.appl8)
			fprintf(stderr, "%s: PMID %s clear all %d instances\n", __FUNCTION__, pmIDStr(pmid), php->ph_numinst);
		    for (i = 0; i < php->ph_numinst; i++)
			PMLC_SET_AVAIL(php->ph_instlist[i].ih_flags, 0);
		}
	    }
	    /* indom is PM_INDOM_NULL */
	    else {
		/* if the single-valued metric is in the history it will have 1
		 * instance */
		if (pmDebugOptions.appl8)
		    fprintf(stderr, "%s: PMID %s clear singular metric\n", __FUNCTION__, pmIDStr(pmid));
		ihp = &php->ph_instlist[0];
		PMLC_SET_AVAIL(ihp->ih_flags, 0);
	    }
	}
    }
}

static void
setavail(__pmResult *resp)
{
    int			i;

    for (i = 0; i < resp->numpmid; i++) {
	pmID		pmid;
	pmValueSet	*vsp;
	__pmHashNode	*hp;
	pmidhist_t	*php;
	insthist_t	*ihp;
	int		j;
	
	vsp = resp->vset[i];
	pmid = vsp->pmid;
	for (hp = __pmHashSearch(pmid, &hist_hash); hp != (__pmHashNode *)0; hp = hp->next)
	    if (pmid == (pmID)hp->key)
		break;

	if (hp != (__pmHashNode *)0)
	    php = (pmidhist_t *)hp->data;
	else {
	    /* add new pmid to history if it's pmValueSet is OK */
	    if (vsp->numval <= 0)
		continue;
	    /*
	     * use the OTHER hash list to find the pmid's desc and thereby its
	     * indom
	     */
	    for (hp = __pmHashSearch(pmid, &pm_hash); hp != (__pmHashNode *)0; hp = hp->next)
		if (pmid == (pmID)hp->key)
		    break;
	    if (hp == (__pmHashNode *)0 ||
		((optreq_t *)hp->data)->r_desc == (pmDesc *)0) {
		/* not set up properly yet, not much we can do ... */
		if (pmDebugOptions.appl8) {
		    if (hp == (__pmHashNode *)0)
			fprintf(stderr, "%s: PMID %s not in pm_hash\n", __FUNCTION__, pmIDStr(pmid));
		    else
			fprintf(stderr, "%s: PMID %s in pm_hash but pmDesc missing\n", __FUNCTION__, pmIDStr(pmid));
		}
		continue;
	    }
	    php = (pmidhist_t *)calloc(1, sizeof(pmidhist_t));
	    if (php == (pmidhist_t *)0) {
		pmNoMem("setavail: new pmid hist entry calloc",
			 sizeof(pmidhist_t), PM_FATAL_ERR);
	    }
	    php->ph_pmid = pmid;
	    php->ph_indom = ((optreq_t *)hp->data)->r_desc->indom;
	    /*
	     * now create a new insthist list for all the instances in the
	     * __pmResult and we're done
	     */
	    php->ph_numinst = vsp->numval;
	    ihp = (insthist_t *)calloc(vsp->numval, sizeof(insthist_t));
	    if (ihp == (insthist_t *)0) {
		pmNoMem("setavail: inst list calloc",
			 vsp->numval * sizeof(insthist_t), PM_FATAL_ERR);
	    }
	    php->ph_instlist = ihp;
	    for (j = 0; j < vsp->numval; j++, ihp++) {
		ihp->ih_inst = vsp->vlist[j].inst;
		PMLC_SET_AVAIL(ihp->ih_flags, 1);
	    }
	    if ((j = __pmHashAdd(pmid, (void *)php, &hist_hash)) < 0) {
		die("setavail: __pmHashAdd(hist_hash)", j);
	    }
	    if (pmDebugOptions.appl8)
		fprintf(stderr, "%s: PMID %s new history, set avail for %d instances\n", __FUNCTION__, pmIDStr(pmid), vsp->numval);
	    
	    continue;
	}

	/* update an existing pmid history entry, adding any previously unseen
	 * instances
	 */
	for (j = 0; j < vsp->numval; j++) {
	    int		inst = vsp->vlist[j].inst;
	    int		k;

	    for (k = 0; k < php->ph_numinst; k++)
		if (inst == php->ph_instlist[k].ih_inst)
		    break;

	    if (k < php->ph_numinst)
		ihp = &php->ph_instlist[k];
	    else {
		/* allocate new instance if required */
		int	need = (k + 1) * sizeof(insthist_t);
		insthist_t	*tmp_instlist;

		tmp_instlist = (insthist_t *)realloc(php->ph_instlist, need);
		if (tmp_instlist == NULL) {
		    pmNoMem("setavail: inst list realloc", need, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		php->ph_instlist = tmp_instlist;
		ihp = &php->ph_instlist[k];
		ihp->ih_inst = inst;
		ihp->ih_flags = 0;
		php->ph_numinst++;
	    }
	    PMLC_SET_AVAIL(ihp->ih_flags, 1);
	    if (pmDebugOptions.appl8)
		fprintf(stderr, "%s: PMID %s set avail for instance %d\n", __FUNCTION__, pmIDStr(pmid), inst);
	}
    }
}

/* 
 * This has been taken straight from logmeta.c in libpcp. It is required
 * here to get the timestamp of the indom. 
 * Note that the tp argument is used to return the timestamp of the indom.
 * It is a merger of __pmLogGetInDom and searchindom.
 */
static int
__localLogGetInDom(__pmLogCtl *lcp, __pmLogInDom *lidp)
{
    __pmHashNode	*hp;
    __pmLogInDom	*idp;
    int			sts;

    if (pmDebugOptions.logmeta && pmDebugOptions.desperate)
	fprintf(stderr, "__localLogGetInDom( ..., %s) -> ",
	    pmInDomStr(lidp->indom));

    if ((hp = __pmHashSearch((unsigned int)lidp->indom, &lcp->hashindom)) == NULL) {
	sts = -1;
	goto done;
    }

    idp = (__pmLogInDom *)hp->data;

    if (idp == NULL) {
	sts = PM_ERR_INDOM_LOG;
	goto done;
    }

    lidp->stamp = idp->stamp;
    sts = idp->numinst;
    lidp->instlist = idp->instlist;
    lidp->namelist = idp->namelist;
    lidp->alloc = 0;

done:
    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
	if (hp == NULL)
	    fprintf(stderr, "%d (__pmHashSearch failed)\n", sts);
	else if (sts >= 0) {
	    fprintf(stderr, "%d @ ", sts);
	    __pmPrintTimestamp(stderr, &lidp->stamp);
	    fputc('\n', stderr);
	}
	else
	    fprintf(stderr, "%s\n", pmErrStr(sts));
    }
    lidp->numinst = sts;

    return sts;
}


/*
 * compare __pmResults for a particular metric, and return 1 if
 * the set of instances has changed.
 */
static int
check_inst(pmValueSet *vsp, int hint, __pmResult *lrp)
{
    int		i;
    int		j;
    pmValueSet	*lvsp;

    /* Make sure vsp->pmid exists in lrp's result */
    /* and find which value set in lrp it is. */
    if (hint < lrp->numpmid && lrp->vset[hint]->pmid == vsp->pmid)
	i = hint;
    else {
	for (i = 0; i < lrp->numpmid; i++) {
	    if (lrp->vset[i]->pmid == vsp->pmid)
		break;
	    if (IS_DERIVED(vsp->pmid) &&
		SET_DERIVED_LOGGED(vsp->pmid) == lrp->vset[i]->pmid)
		/* PMID rewritten for logged derived metric */
		break;
	}
	if (i == lrp->numpmid) {
	    fprintf(stderr, "check_inst: cannot find PMID %s in last result ...\n",
		pmIDStr(vsp->pmid));
	    __pmPrintResult(stderr, lrp);
	    return 0;
	}
    }

    lvsp = lrp->vset[i];

    if (lvsp->numval != vsp->numval)
	return 1;

    /* compare instances */
    for (i = 0; i < lvsp->numval; i++) {
	if (lvsp->vlist[i].inst != vsp->vlist[i].inst) {
	    /* the hard way */
	    for (j = 0; j < vsp->numval; j++) {
		if (lvsp->vlist[j].inst == vsp->vlist[i].inst)
		    break;
	    }
	    if (j == vsp->numval)
		return 1;
	}
    }

    return 0;
}

/* Return 1 (true) if the sets are the same, 0 (false) otherwise */
static int
samelabels(int na, const pmLabelSet *a, int nb, const pmLabelSet *b)
{
    int	i;

    if (na != nb)
	return 0;
    for (i = 0; i < na; i++) {
	if (!__pmEqualLabelSet(&a[i], &b[i]))            
	    break;
    }
    return i == na;
}

static int
putlabels(unsigned int type, unsigned int ident, const __pmTimestamp *tsp, int np, pmLabelSet *prior)
{
    int		len, sts;
    pmLabelSet	*label;

    if (type == PM_LABEL_CONTEXT)
	len = pmGetContextLabels(&label);
    else if (type == PM_LABEL_DOMAIN)
	len = pmGetDomainLabels(ident, &label);
    else if (type == PM_LABEL_CLUSTER)
	len = pmGetClusterLabels(ident, &label);
    else if (type == PM_LABEL_INDOM)
	len = pmGetInDomLabels(ident, &label);
    else if (type == PM_LABEL_ITEM)
	len = pmGetItemLabels(ident, &label);
    else if (type == PM_LABEL_INSTANCES) {
	len = pmGetInstancesLabels(ident, &label);
	/* do not log exactly the same instance sets as previously written */
	if (len > 0 && np > 0 && samelabels(np, prior, len, label)) {
	    pmFreeLabelSets(label, len);
	    return 0;
	}
    }
    else
	len = 0;

    if (len <= 0)
	return 0;

    if ((sts = __pmLogPutLabels(&archctl, type, ident, len, label, tsp)) < 0)
	/*
	 * on success, labels are stashed by __pmLogPutLabels();
	 * on failure, we need to free labels.
	 */
	pmFreeLabelSets(label, len);

    return sts;
}

static int
manageLabels(pmDesc *desc, const __pmTimestamp *tsp, int only_instances)
{
    int		i = 0;
    int		sts = 0;
    pmLabelSet	*label;
    unsigned int type;
    unsigned int ident = PM_IN_NULL;
    unsigned int label_types[] = {
	PM_LABEL_CONTEXT, PM_LABEL_DOMAIN, PM_LABEL_INDOM,
	PM_LABEL_CLUSTER, PM_LABEL_ITEM, PM_LABEL_INSTANCES
    };
    const unsigned int	ntypes = sizeof(label_types) / sizeof(label_types[0]);

    if (only_instances)
	i = ntypes - 1;

    for (; i < ntypes; i++) {
	type = label_types[i];

	if (type == PM_LABEL_INDOM || type == PM_LABEL_INSTANCES) {
	    if (desc->indom == PM_INDOM_NULL)
		continue;
	    ident = desc->indom;
	}
	else if (type == PM_LABEL_DOMAIN)
	    ident = pmID_domain(desc->pmid);
	else if (type == PM_LABEL_CLUSTER)
	    ident = pmID_build(pmID_domain(desc->pmid), pmID_cluster(desc->pmid), 0);
	else if (type == PM_LABEL_ITEM) {
	    ident = desc->pmid;
	    if (IS_DERIVED(ident))
		/* derived metric, rewrite cluster field ... */
		ident = SET_DERIVED_LOGGED(ident);
	}
	else
	    ident = PM_IN_NULL;

	/*
	 * Lookup returns >= 0 when the key exists
	 *
	 * In the instance-domain-is-changing scenario we can skip this
	 * (as we must log label metadata in that special case) - unless
	 * the labelsets are all exactly the same as those already logged.
	 */
	if ((sts = __pmLogLookupLabel(&archctl, type, ident, &label, tsp)) >= 0
	    && !only_instances)
	    continue;
	if ((sts = putlabels(type, ident, tsp, sts, label)) < 0)
	    break;
    }
    return sts;
}

static int
manageText(pmDesc *desc)
{
    int		i;
    int		j;
    int		sts;
    int		level;
    int		indom;
    unsigned int types;
    unsigned int ident;
    char	*text;
    unsigned int text_types[] = { PM_TEXT_ONELINE, PM_TEXT_HELP };
    unsigned int ident_types[] = { PM_TEXT_PMID, PM_TEXT_INDOM };
    const unsigned int	ntypes = sizeof(text_types) / sizeof(text_types[0]);
    const unsigned int	nidents = sizeof(ident_types) / sizeof(ident_types[0]);

    sts = 0;
    for (i = 0; i < ntypes; i++) {
	for (j = 0; j < nidents; j++) {
	    types = text_types[i] | ident_types[j];
	    level = text_types[i] | PM_TEXT_DIRECT;
	    indom = ident_types[j] & PM_TEXT_INDOM;
	    if (indom) {
		if (desc->indom == PM_INDOM_NULL)
		    /* singular metric, no indom help text */
		    continue;
		ident = desc->indom;
	    }
	    else {
		ident = desc->pmid;
	    }

	    /* Lookup returns >= 0 when the key exists */
	    if (__pmLogLookupText(&archctl, ident, types, &text) >= 0)
		continue;

	    if (indom)
		sts = pmLookupInDomText(ident, level, &text);
	    else {
		sts = pmLookupText(ident, level, &text);
		if (IS_DERIVED(ident))
		    /* derived metric, rewrite cluster field ... */
		    ident = SET_DERIVED_LOGGED(ident);
	    }

	    /*
	     * Only cache indoms help texts (final parameter) - there
	     * are far fewer (less memory used), and we only need to
	     * make sure we log them once, not PMID help text which
	     * is guarded by the pmDesc logging-once logic.
	     */
	    if (sts == 0) {
		sts = __pmLogPutText(&archctl, ident, types, text, indom);
		free(text);
		if (sts < 0)
		    break;
	    }
	}
    }
    return sts;
}

/*
 * Lookup the first cache index associated with a given PMID in a given task.
 */
static int
lookupTaskCacheIndex(task_t *tp, pmID pmid)
{
    int i;

    for (i = 0; i < tp->t_numpmid; i++)
	if (tp->t_pmidlist[i] == pmid)
	    return i;
    return -1;
}

/*
 * Iterate over *all* tasks and return all names for a given PMID.
 * Returns the number of names found, and nameset allocated in a
 * single allocation call (which the caller must free).
 */
static int
lookupTaskCacheNames(pmID pmid, char ***namesptr)
{
    int		i;
    int		j;
    int		numnames;
    int		str_len = 0;
    char	*data;
    char	**names;
    char	**tmp_names;
    task_t	*tp;

    /*
     * start with names (including duplicate, if any) from the PMNS
     */
    numnames = pmNameAll(pmid, &names);
    if (numnames < 0) {
	numnames = 0;
	names = (char **)malloc(0);
    }
    for (i = 0; i < numnames; i++) {
	str_len += strlen(names[i]) + 1;
    }

    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	for (i = 0; i < tp->t_numpmid; i++) {
	    if (tp->t_pmidlist[i] != pmid)
		continue;
	    for (j = 0; j < numnames; j++) {
		if (strcmp(names[j], tp->t_namelist[i]) == 0) {
		    /* in task list, and already in names[] ... skip */
		    break;
		}
	    }
	    if (j == numnames) {
		/*
		 * need to append this one ... this is rare!
		 * only known case where this happens is when the
		 * pmlogger configuration file names a dynamic metric
		 * that is mapped to an existing PMID which is in
		 * the PMNS, but the PMID->name service knows nothing
		 * about the name of the dynamic metric
		 */
		int	old_str_len = str_len;
		str_len += strlen(tp->t_namelist[i]) + 1;
		numnames++;
		tmp_names = (char **)realloc(names, numnames * sizeof(names[0]) + str_len);
		if (tmp_names == NULL) {
		    pmNoMem("lookupTaskCacheNames: names realloc", numnames * sizeof(names[0]) + str_len, PM_FATAL_ERR);
		    /* NOTREACHED */
		}
		names = tmp_names;
		data = (char *)names + ((numnames-1) * sizeof(names[0]));
		/* relocate strings */
		memmove(data+sizeof(names[0]), data, old_str_len);
		data += sizeof(names[0]);
		for (j = 0; j < numnames; j++) {
		    names[j] = data;
		    if (j == numnames - 1) {
			strcpy(data, tp->t_namelist[i]);
		    }
		    data += (strlen(names[j]) + 1);
		}
	    }
	}
    }

    *namesptr = names;
    return numnames;
}

/*
 * want the size in the archive file, not the size over the wire
 */
static size_t
pduresultbytes(__pmResult *resp)
{
    size_t	need, vneed;

    __pmGetResultSize(PDU_RESULT, resp->numpmid, resp->vset, &need, &vneed);
    /*
     * extra word for sec in timestamp for V3
     */
    if (archive_version >= PM_LOG_VERS03)
	need += sizeof(__int32_t);
    return need + vneed;
}

/*
 * Warning: called in signal handler context ... be careful
 */
void
log_callback(int afid, void *data)
{
    task_t		*tp;
    for (tp = tasklist; tp != NULL; tp = tp->t_next) {
	if (tp->t_afid == afid) {
	    tp->t_alarm = 1;
	    log_alarm = 1;
	    break;
	}
    }
}

/*
 * do real work from callback ...
 */
void
do_work(task_t *tp)
{
    int			i;
    int			j;
    int			k;
    int			sts;
    fetchctl_t		*fp;
    indomctl_t		*idp;
    __pmResult		*resp;
    __pmPDU		*pb;
    AFctl_t		*acp;
    lastfetch_t		*lfp;
    lastfetch_t		*free_lfp;
    char		*caller = pmGetProgname();
    int			changed;
    int			needindom;
    int			needti;
    static off_t	flushsize = 100000;
    long		old_meta_offset;
    long		label_offset;
    long		new_offset;
    long		new_meta_offset;
    int			pdu_bytes = 0;
    int			pdu_metrics = 0;
    size_t		pdu_payload;
    __pmLogInDom	old;
    __uint64_t		max_offset;
    unsigned long	peek_offset;

    label_offset = __pmLogLabelSize(archctl.ac_log);

    if ((pmDebugOptions.appl2) && (pmDebugOptions.desperate))
	pmNotifyErr(LOG_INFO, "do_work(tp=%p): afid=%d parse_done=%d exit_samples=%d", tp, tp->t_afid, parse_done, exit_samples);

    if (!parse_done)
	/* ignore callbacks until all of the config file has been parsed */
	return;

    /* find AFctl_t for this afid */
    for (acp = achead; acp != (AFctl_t *)0; acp = acp->ac_next) {
	if (acp->ac_afid == tp->t_afid)
	    break;
    }
    if (acp == (AFctl_t *)0) {
	acp = (AFctl_t *)calloc(1, sizeof(AFctl_t));
	if (acp == (AFctl_t *)0) {
	    pmNoMem("log_callback: new AFctl_t entry calloc",
		     sizeof(AFctl_t), PM_FATAL_ERR);
	}
	acp->ac_afid = tp->t_afid;
	acp->ac_next = achead;
	achead = acp;
    }
    else {
	/* cleanup any fetchgroups that have gone away */
	for (lfp = acp->ac_fetch; lfp != (lastfetch_t *)0; lfp = lfp->lf_next) {
	    for (fp = tp->t_fetch; fp != (fetchctl_t *)0; fp = fp->f_next) {
		if (fp == lfp->lf_fp)
		    break;
	    }
	    if (fp == (fetchctl_t *)0) {
		lfp->lf_fp = (fetchctl_t *)0;	/* mark lastfetch_t as free */
		if (lfp->lf_resp != NULL) {
		    __pmFreeResult(lfp->lf_resp);
		    lfp->lf_resp = NULL;
		}
	    }
	}
    }

    for (fp = tp->t_fetch; fp != (fetchctl_t *)0; fp = fp->f_next) {
	
	/* find lastfetch_t for this fetch group, else make a new one */
	free_lfp = (lastfetch_t *)0;
	for (lfp = acp->ac_fetch; lfp != (lastfetch_t *)0; lfp = lfp->lf_next) {
	    if (lfp->lf_fp == fp)
		break;
	    if (lfp->lf_fp == (fetchctl_t *)0 && free_lfp == (lastfetch_t *)0)
		    free_lfp = lfp;
	}
	if (lfp == (lastfetch_t *)0) {
	    /* need new one */
	    if (free_lfp != (lastfetch_t *)0)
		lfp = free_lfp;				/* lucky */
	    else {
		lfp = (lastfetch_t *)calloc(1, sizeof(lastfetch_t));
		if (lfp == (lastfetch_t *)0) {
		    pmNoMem("log_callback: new lastfetch_t entry calloc",
			     sizeof(lastfetch_t), PM_FATAL_ERR);
		}
		lfp->lf_next = acp->ac_fetch;
		acp->ac_fetch = lfp;
	    }
	    lfp->lf_fp = fp;
	}

	if (one_context || fp->f_state & OPT_STATE_PROFILE) {
	    /* profile for this fetch group has changed */
	    pmAddProfile(PM_INDOM_NULL, 0, (int *)0);
	    for (idp = fp->f_idp; idp != (indomctl_t *)0; idp = idp->i_next) {
		if (idp->i_indom != PM_INDOM_NULL && idp->i_numinst != 0)
		    pmAddProfile(idp->i_indom, idp->i_numinst, idp->i_instlist);
	    }
	    fp->f_state &= ~OPT_STATE_PROFILE;
	}

	clearavail(fp);

	if ((sts = changed = myFetch(fp->f_numpmid, fp->f_pmidlist, &resp)) < 0) {
	    if (sts == -EINTR) {
		/* disconnect() already done in myFetch() */
		return;
	    }
	    if (sts != -ETIMEDOUT) {
		/* optionally report and disconnect() the first time thru */
		if (pmDebugOptions.appl2)
		    pmNotifyErr(LOG_INFO, "callback: disconnecting because myFetch failed: %s", pmErrStr(sts));
		disconnect(sts);
	    }
	    continue;
	}

	if (first_result) {
	    int		lsts;
	    /*
	     * delayed prologue until we have first fetch
	     */
	    if (pmDebugOptions.dev0) {
		fprintf(stderr, "Note: epoch reset from ");
		__pmPrintTimestamp(stderr, &epoch);
		fprintf(stderr, " to ");
		__pmPrintTimestamp(stderr, &resp->timestamp);
		fputc('\n', stderr);
	    }
	    epoch = resp->timestamp;	/* struct assignment */
	    first_result = 0;

	    if ((lsts = do_prologue()) < 0)
		fprintf(stderr, "Warning: problem writing archive prologue: %s\n",
		    pmErrStr(lsts));
	}

	pdu_payload = pduresultbytes(resp);

	if (pmDebugOptions.appl2)
	    pmNotifyErr(LOG_INFO, "callback: fetch group %p (%d metrics, 0x%x change)", fp, fp->f_numpmid, changed);

	if (rflag) {
	    /*
	     * bytes = PDU len - sizeof (header) + 2 * sizeof (int)
             * see logputresult() in libpcp/logutil.c for details of how
	     * a PDU buffer is reformatted to make len shorter by one int
	     * before the record is written to the external file
	     */
	    pdu_bytes += pdu_payload - sizeof (__pmPDUHdr) + 2*sizeof(int); 
	    pdu_metrics += fp->f_numpmid;
	}

	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes (v2 archives) or
	 * 2^63-1 bytes (for v3 archives).
	 */
	max_offset = (archive_version == PM_LOG_VERS02) ? 0x7fffffff : LONGLONG_MAX;
	peek_offset = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller);
	peek_offset += pdu_payload - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > max_offset) {
	    if (pmDebugOptions.appl2)
		pmNotifyErr(LOG_INFO, "callback: new volume based on max size, currently %" FMT_INT64, archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller));
	    (void)newvolume(VOL_SW_MAX);
	}

	/*
	 * Output write ordering ... need to do any required metadata
	 * changes first, then the fetch result, then optionally a new
	 * index entry.
	 *
	 * But for 32-bit pointer platforms, __pmDecodeResult changes the
	 * pointers in the input PDU buffer for the non INSITU values and
	 * __pmDecodeResult also does potential hton*() translation
	 * within the input PDU buffer.  If we have derived metrics we
	 * need to rewrite the PMIDs too, which can't be done until after
	 * the input PDU buffer is decoded into a pmResult.  Finally, if
	 * writing a version 2 archive after a high resolution fetch, or
	 * a version 3 archive after a original fetch, result timestamp
	 * translation is required.
	 *
	 * So in general there is no real choice but to first call
	 * __pmDecodeResult (done in myFetch), do the result processing
	 * and after the metadata changes have been written out, call
	 * __pmEncodeResult to encode the right PDU buffer before doing
	 * the correct style of result write.
	 */
	last_log_offset = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller);
	assert(last_log_offset >= 0);

	setavail(resp);

	if (changed & PMCD_LABEL_CHANGE) {
	    /*
	     * Change to the context labels associated with logged host
	     */
	    putlabels(PM_LABEL_CONTEXT, PM_IN_NULL, &resp->timestamp, 0, NULL);
	}

	needti = 0;
	old_meta_offset = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_META, caller);
	assert(old_meta_offset >= 0);

	for (i = 0; i < resp->numpmid; i++) {
	    pmValueSet	*vsp = resp->vset[i];
	    pmDesc	desc;
	    char	**names = NULL;
	    int		numnames = 0;

	    sts = __pmLogLookupDesc(&archctl, vsp->pmid, &desc);
	    if (sts < 0) {
		/* lookup name and descriptor in task cache */
		int taskindex = lookupTaskCacheIndex(tp, vsp->pmid);
		if (taskindex == -1) {
		    fprintf(stderr, "lookupTaskCacheIndex cannot find PMID %s\n",
				pmIDStr(vsp->pmid));
		    exit(1);
		}
		desc = tp->t_desclist[taskindex];
		numnames = lookupTaskCacheNames(vsp->pmid, &names);
		if (numnames < 1) {
		    fprintf(stderr, "lookupTaskCacheNames(%s, ...): %s\n", pmIDStr(vsp->pmid), pmErrStr(sts));
		    exit(1);
		}
		if (IS_DERIVED(desc.pmid))
		    /* derived metric, rewrite cluster field ... */
		    desc.pmid = SET_DERIVED_LOGGED(desc.pmid);
		if ((sts = __pmLogPutDesc(&archctl, &desc, numnames, names)) < 0) {
		    fprintf(stderr, "__pmLogPutDesc: %s\n", pmErrStr(sts));
		    exit(1);
		}
		if (IS_DERIVED_LOGGED(desc.pmid))
		    /* derived metric, restore cluster field ... */
		    desc.pmid = CLEAR_DERIVED_LOGGED(desc.pmid);
		free(names);
		manageLabels(&desc, &resp->timestamp, 0);
		manageText(&desc);
	    }
	    if (desc.type == PM_TYPE_EVENT) {
		/*
		 * Event records need some special handling ...
		 */
		if ((sts = do_events(vsp)) < 0) {
		    fprintf(stderr, "Failed to process event records: %s\n", pmErrStr(sts));
		    exit(1);
		}
	    }
	    if (desc.indom != PM_INDOM_NULL && vsp->numval > 0) {
		/*
		 * __pmLogGetInDom has been replaced by __localLogGetInDom
		 * so that the timestamp of the retrieved indom is also
		 * returned. The timestamp is then used to decide if
		 * the indom needs to be refreshed.
		 */
		old.indom = desc.indom;
		old.alloc = 0;
		(void)__localLogGetInDom(&logctl, &old);
		if (old.numinst > 0 && __pmTimestampSub(&resp->timestamp, &old.stamp) <= 0) {
		    /*
		     * Already have indom with the same (or later, in the
		     * case of some time warp) timestamp compared to the
		     * timestamp for this pmResult (from a previous metric
		     * in the pmResult with the same indom).
		     * Avoid doing it again:
		     * (a) duplicate indoms with same timestamp in the
		     *     metadata file is not a good idea
		     * (b) (worse) pmGetInDom may return different instances
		     *     if the indom is dynamic, like proc metrics
		     */
		    needindom = 0;
		    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
			fprintf(stderr, "time warp: pmResult: % " FMT_INT64 ".%09d last %s indom: %" FMT_INT64 ".%09d\n",
			    resp->timestamp.sec, resp->timestamp.nsec,
			    pmInDomStr(old.indom),
			    old.stamp.sec, old.stamp.nsec);
		    }
		}
		else if (old.numinst < 0) {
		    needindom = 1;
		    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
			fprintf(stderr, "numinst=%d => needindom %s\n", old.numinst, pmInDomStr(old.indom));
		    }
		}
		else {
		    needindom = 0;
		    /* Need to see if result's insts all exist
		     * somewhere in the most recent hashed/cached indom.
		     * Thus a potential numval^2 search.
                     */
		    for (j = 0; j < vsp->numval; j++) {
			for (k = 0; k < old.numinst; k++) {
			    if (vsp->vlist[j].inst == old.instlist[k])
				break;
			}
			if (k == old.numinst) {
			    needindom = 1;
			    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
				fprintf(stderr, "inst %d in pmResult, not in cached indom => needindom %s\n",
				    vsp->vlist[j].inst, pmInDomStr(desc.indom));
			    }
			    break;
			}
		    }
		    /* 
		     * Check that instances have not diminished between
		     * consecutive pmFetch's ... this would pass all the
		     * tests above, but still the indom still needs to
		     * be refeshed.
		     */
		    if (needindom == 0 && lfp->lf_resp != NULL) {
			needindom = check_inst(vsp, i, lfp->lf_resp);
			if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
			    if (needindom)
				fprintf(stderr, "check_inst => needindom %s\n",
				    pmInDomStr(desc.indom));
			}
		    }
		}

		if (needindom) {
		    /*
		     * Looks like the indom may have changed, in which case it needs
		     * to be pushed to the .meta file.
		     *
		     * Note.  We do NOT free() new_instlist and new_namelist allocated
		     *	  here unless this indom is a duplicate (look for magic below
		     *	  log{Put,Get}InDom) or the indom really has not changed.
		     */
		    __pmLogInDom	new;
		    __pmLogInDom	new_delta;
		    new.stamp = resp->timestamp;	/* struct assignment */
		    new.indom = desc.indom;
		    new.alloc = 0;
		    if ((new.numinst = pmGetInDom(desc.indom, &new.instlist, &new.namelist)) < 0) {
			fprintf(stderr, "pmGetInDom(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(new.numinst));
			exit(1);
		    }
		    new.alloc = (PMLID_INSTLIST | PMLID_NAMELIST);
		    /*
		     * sort the indom based in internal instance identifier
		     */
		    pmaSortInDom(&new);
		    /*
		     * if this is the first time we've seen this indom,
		     * or the current and previous indoms are different
		     * we need to call __pmLogPutInDom()
		     *
		     * needindom from here on down ...
		     * 0 => no change, add nothing to metadata
		     * 1 => add full indom to metadata
		     * 2 => add delta indom to metadata
		     */
		    needindom = 0;
		    if (old.numinst < 0)
			needindom = 1;
		    else if (archive_version == PM_LOG_VERS02) {
			if (!pmaSameInDom(&old, &new))
			    needindom = 1;
		    }
		    else if (archive_version >= PM_LOG_VERS03) {
			needindom = pmaDeltaInDom(&old, &new, &new_delta);
		    }
		    if (needindom == 1) {
			int	pdu_type;
			if (pmDebugOptions.appl2)
			    pmNotifyErr(LOG_INFO, "callback: indom (%s) full change", pmInDomStr(desc.indom));
			new.stamp = resp->timestamp;	/* struct assignment */
			new.indom = desc.indom;
			if (archive_version >= PM_LOG_VERS03)
			    pdu_type = TYPE_INDOM;
			else
			    pdu_type = TYPE_INDOM_V2;
			/* emit full indom record */
			if ((sts = __pmLogPutInDom(&archctl, pdu_type, &new)) < 0) {
			    fprintf(stderr, "__pmLogPutInDom(%s): full: %s\n", pmInDomStr(desc.indom), pmErrStr(sts));
			    exit(1);
			}
			if (sts == PMLOGPUTINDOM_DUP) {
			    if (pmDebugOptions.logmeta && pmDebugOptions.desperate) {
				fprintf(stderr, "__pmLogPutInDom -> PMLOGPUTINDOM_DUP\n");
			    }
			    free(new.instlist);
			    free(new.namelist);
			}
			manageLabels(&desc, &new.stamp, 1);
			needti = 1;
		    }
		    else if (needindom == 2) {
			if (pmDebugOptions.appl2)
			    pmNotifyErr(LOG_INFO, "callback: indom (%s) delta change", pmInDomStr(desc.indom));
			new_delta.stamp = resp->timestamp;	/* struct assignment */
			new_delta.indom = desc.indom;
			/* emit delta indom record */
			if ((sts = __pmLogPutInDom(&archctl, TYPE_INDOM_DELTA, &new_delta)) < 0) {
			    fprintf(stderr, "__pmLogPutInDom(%s): delta: %s\n", pmInDomStr(desc.indom), pmErrStr(sts));
			    exit(1);
			}
			/*
			 * IMPORTANT ... need to add the full indom (new)
			 * not new_delta into the hashed structures here
			 */
			new.stamp = new_delta.stamp;	/* struct assignment */
			if ((sts = __pmLogAddInDom(&archctl, TYPE_INDOM, &new, NULL)) < 0) {
			    fprintf(stderr, "__pmLogAddInDom(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(sts));
			    exit(1);
			}
			free(new_delta.instlist);
			free(new_delta.namelist);
			manageLabels(&desc, &new_delta.stamp, 1);
			needti = 1;
		    }
		    else {
			free(new.instlist);
			free(new.namelist);
		    }
		}
	    }
	}

	if (last_log_offset == 0 || last_log_offset == label_offset) {
	    /* first result in this volume */
	    needti = 1;
	    if (pmDebugOptions.appl2)
		pmNotifyErr(LOG_INFO, "callback: first result for this volume");
	}

	if (tp->t_dm != 0) {
	    /*
	     * Result contains at least one derived metric, rewrite
	     * the cluster field of the PMID(s) (set the top bit)
	     * before output ... no need to restore PMID(s) because
	     * we're finished with this result once the write is
	     * done.
	     *
	     * This forces the PMID in the archive to NOT look like
	     * the PMID of a derived metric, which is need to replay
	     * the archive correctly.
	     */
	    for (i = 0; i < resp->numpmid; i++) {
		pmValueSet	*vsp = resp->vset[i];
		if (IS_DERIVED(vsp->pmid))
		    vsp->pmid = SET_DERIVED_LOGGED(vsp->pmid);
	    }
	}

	if ((sts = __pmEncodeResult(&logctl, resp, &pb)) < 0) {
	    fprintf(stderr, "__pmEncodeResult: %s\n", pmErrStr(sts));
	    exit(1);
	}
	if (archive_version >= PM_LOG_VERS03) {
	    if ((sts = __pmLogPutResult3(&archctl, pb)) < 0) {
		fprintf(stderr, "__pmLogPutResult3: (encode) %s\n", pmErrStr(sts));
		exit(1);
	    }
	} else {
	    if ((sts = __pmLogPutResult2(&archctl, pb)) < 0) {
		fprintf(stderr, "__pmLogPutResult2: (encode) %s\n", pmErrStr(sts));
		exit(1);
	    }
	}
	__pmUnpinPDUBuf(pb);
	if (!remote.conn)
	    __pmOverrideLastFd(__pmFileno(archctl.ac_mfp));

	if (archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller) > flushsize) {
	    needti = 1;
	    if (pmDebugOptions.appl2)
		pmNotifyErr(LOG_INFO, "callback: file size (%" FMT_INT64 ") reached flushsize (%ld)", archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller), (long)flushsize);
	}

	if (needti) {
	    /*
	     * need to unwind seek pointer to start of most recent
	     * result (but if this is the first one, skip the label
	     * record, what a crock), ... ditto for the meta data
	     */
	    new_offset = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller);
	    assert(new_offset >= 0);
	    new_meta_offset = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_META, caller);
	    assert(new_meta_offset >= 0);
	    archctl.ac_reset_cb(&archctl, PM_LOG_VOL_CURRENT, last_log_offset, caller);
	    archctl.ac_reset_cb(&archctl, PM_LOG_VOL_META, old_meta_offset, caller);
	    __pmLogPutIndex(&archctl, &resp->timestamp);
	    /*
	     * ... and put them back
	     */
	    archctl.ac_reset_cb(&archctl, PM_LOG_VOL_CURRENT, new_offset, caller);
	    archctl.ac_reset_cb(&archctl, PM_LOG_VOL_META, new_meta_offset, caller);
	    flushsize = archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller) + 100000;
	}

	last_stamp = resp->timestamp;	/* struct assignment */

	if (lfp->lf_resp != NULL) {
	    /*
	     * release memory that is allocated in pmDecodeResult
	     */
	    __pmFreeResult(lfp->lf_resp);
	}
	lfp->lf_resp = resp;
    }

    if (rflag && tp->t_size == 0 && pdu_metrics > 0) {
	char	*name = NULL;
	int	taskindex;

	tp->t_size = pdu_bytes;

	if (pdu_metrics > 1) {
	    fprintf(stderr, "\nGroup [%d metrics", pdu_metrics);
	    if (tp->t_dm > 0)
		fprintf(stderr, ", %d derived", tp->t_dm);
	    fprintf(stderr, "] {\n");
	}
	else
	    fprintf(stderr, "\nMetric ");
        
        for (fp = tp->t_fetch; fp != (fetchctl_t *)0; fp = fp->f_next) {
            for (i = 0; i < fp->f_numpmid; i++) {
                name = NULL;
                taskindex = lookupTaskCacheIndex(tp, fp->f_pmidlist[i]);
                if (taskindex >= 0)
                    name = tp->t_namelist[taskindex];
                if (pdu_metrics > 1)
                    fprintf(stderr, "\t");
                fprintf(stderr, "%s", name ? name : pmIDStr(fp->f_pmidlist[i]));
                if (pdu_metrics > 1)
                    fprintf(stderr, "\n");
            } 
        }
	if (pdu_metrics > 1)
	    fprintf(stderr, "}");
	fprintf(stderr, " logged ");

	if (tp->t_delta.tv_sec == 0 && tp->t_delta.tv_usec == 0)
	    fprintf(stderr, "once: %d bytes\n", pdu_bytes);
	else {
	    if (tp->t_delta.tv_usec == 0) {
		fprintf(stderr, "every %d sec: %d bytes ",
		    (int)tp->t_delta.tv_sec, pdu_bytes);
	    }
	    else
		fprintf(stderr, "every %.3f sec: %d bytes ",
		    pmtimevalToReal(&tp->t_delta), pdu_bytes);
	    fprintf(stderr, "or %.2f Mbytes/day\n",
		((double)pdu_bytes * 24 * 60 * 60) /
		(1024 * 1024 * pmtimevalToReal(&tp->t_delta)));
	}
    }

    if (exit_samples > 0)
	exit_samples--;

    if (exit_samples == 0)
	/* run out of samples in sample counter, so stop logging */
	run_done(0, "Sample limit reached");

    if (exit_bytes != -1 && 
        (vol_bytes + archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller) >= exit_bytes))
        /* reached exit_bytes limit, so stop logging */
        run_done(0, "Byte limit reached");

    if (vol_switch_samples > 0 &&
	++vol_samples_counter == vol_switch_samples) {
        (void)newvolume(VOL_SW_COUNTER);
	if (pmDebugOptions.appl2)
	    pmNotifyErr(LOG_INFO, "callback: new volume based on samples (%d)", vol_samples_counter);
    }

    if (vol_switch_bytes > 0 &&
        (archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller) >= vol_switch_bytes)) {
        (void)newvolume(VOL_SW_BYTES);
	if (pmDebugOptions.appl2)
	    pmNotifyErr(LOG_INFO, "callback: new volume based on size (%" FMT_INT64 ")", archctl.ac_tell_cb(&archctl, PM_LOG_VOL_CURRENT, caller));
    }
}

int
putmark(void)
{
    __pmTimestamp	msec = { 0, 1000000 };		/* 1msec */

    if (last_stamp.sec == 0 && last_stamp.nsec == 0)
	/* no earlier result, no point adding a mark record */
	return 0;

    return __pmLogWriteMark(&archctl, &last_stamp, &msec);
}
