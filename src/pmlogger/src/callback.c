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

int	last_log_offset;

/*
 * pro tem, we have a single context with the pmcd providing the
 * results, hence need to send the profile each time
 */
static int one_context = 1;

struct timeval	last_stamp;
__pmHashCtl	hist_hash;

/*
 * These structures allow us to keep track of the _last_ fetch
 * for each fetch in each AF group ... needed to track changes in
 * instance availability.
 */
typedef struct _lastfetch {
    struct _lastfetch	*lf_next;
    fetchctl_t		*lf_fp;
    pmResult		*lf_resp;
    __pmPDU		*lf_pb;
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
	    if (hp == (__pmHashNode *)0)
		/* not in history, no flags to update */
		continue;
	    php = (pmidhist_t *)hp->data;

	    /* now we have the metric's entry in the history */

	    if (idp->i_indom != PM_INDOM_NULL) {
		/*
		 * for each instance in the profile for this metric, find
		 * the history entry for the instance if it exists and
		 * reset the "was available at last fetch" flag
		 */
		if (idp->i_numinst)
		    for (i = 0; i < idp->i_numinst; i++) {
			inst = idp->i_instlist[i];
			ihp = &php->ph_instlist[0];
			for (j = 0; j < php->ph_numinst; j++, ihp++)
			    if (ihp->ih_inst == inst) {
				PMLC_SET_AVAIL(ihp->ih_flags, 0);
				break;
			    }
		    }
		else
		    /*
		     * if the profile specifies "all instances" clear EVERY
		     * instance's "available" flag
		     * NOTE: even instances that don't exist any more
		     */
		    for (i = 0; i < php->ph_numinst; i++)
			PMLC_SET_AVAIL(php->ph_instlist[i].ih_flags, 0);
	    }
	    /* indom is PM_INDOM_NULL */
	    else {
		/* if the single-valued metric is in the history it will have 1
		 * instance */
		ihp = &php->ph_instlist[0];
		PMLC_SET_AVAIL(ihp->ih_flags, 0);
	    }
	}
    }
}

static void
setavail(pmResult *resp)
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
		((optreq_t *)hp->data)->r_desc == (pmDesc *)0)
		/* not set up properly yet, not much we can do ... */
		continue;
	    php = (pmidhist_t *)calloc(1, sizeof(pmidhist_t));
	    if (php == (pmidhist_t *)0) {
		__pmNoMem("setavail: new pmid hist entry calloc",
			 sizeof(pmidhist_t), PM_FATAL_ERR);
	    }
	    php->ph_pmid = pmid;
	    php->ph_indom = ((optreq_t *)hp->data)->r_desc->indom;
	    /*
	     * now create a new insthist list for all the instances in the
	     * pmResult and we're done
	     */
	    php->ph_numinst = vsp->numval;
	    ihp = (insthist_t *)calloc(vsp->numval, sizeof(insthist_t));
	    if (ihp == (insthist_t *)0) {
		__pmNoMem("setavail: inst list calloc",
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
	    
	    return;
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

		php->ph_instlist = (insthist_t *)realloc(php->ph_instlist, need);
		if (php->ph_instlist == (insthist_t *)0) {
		    __pmNoMem("setavail: inst list realloc", need, PM_FATAL_ERR);
		}
		ihp = &php->ph_instlist[k];
		ihp->ih_inst = inst;
		ihp->ih_flags = 0;
		php->ph_numinst++;
	    }
	    PMLC_SET_AVAIL(ihp->ih_flags, 1);
	}
    }
}

/* 
 * This has been taken straight from logmeta.c in libpcp. It is required
 * here to get the timestamp of the indom. 
 * Note that the tp argument is used to return the timestamp of the indom.
 * It is a merger of __pmLogGetIndom and searchindom.
 */
int
__localLogGetInDom(__pmLogCtl *lcp, pmInDom indom, __pmTimeval *tp, int **instlist, char ***namelist)
{
    __pmHashNode	*hp;
    __pmLogInDom	*idp;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_LOGMETA)
	fprintf(stderr, "__localLogGetInDom( ..., %s)\n",
	    pmInDomStr(indom));
#endif

    if ((hp = __pmHashSearch((unsigned int)indom, &lcp->l_hashindom)) == NULL)
	return 0;

    idp = (__pmLogInDom *)hp->data;

    if (idp == NULL)
	return PM_ERR_INDOM_LOG;

    *instlist = idp->instlist;
    *namelist = idp->namelist;
    *tp = idp->stamp;

    return idp->numinst;
}


/*
 * compare pmResults for a particular metric, and return 1 if
 * the set of instances has changed.
 */
static int
check_inst(pmValueSet *vsp, int hint, pmResult *lrp)
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
	}
	if (i == lrp->numpmid) {
	    fprintf(stderr, "check_inst: cannot find PMID %s in last result ...\n",
		pmIDStr(vsp->pmid));
	    __pmDumpResult(stderr, lrp);
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
		names = (char **)realloc(names, numnames * sizeof(names[0]) + str_len);
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
    pmResult		*resp;
    __pmPDU		*pb;
    AFctl_t		*acp;
    lastfetch_t		*lfp;
    lastfetch_t		*free_lfp;
    int			needindom;
    int			needti;
    static int		flushsize = 100000;
    long		old_meta_offset;
    long		new_offset;
    long		new_meta_offset;
    int			pdu_bytes = 0;
    int			pdu_metrics = 0;
    int			numinst;
    int			*instlist;
    char		**namelist;
    __pmTimeval		tmp;
    __pmTimeval		resp_tval;
    unsigned long	peek_offset;

#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_APPL2) && (pmDebug & DBG_TRACE_DESPERATE)) {
	struct timeval	now;

	__pmtimevalNow(&now);
	__pmPrintStamp(stderr, &now);
	fprintf(stderr, " do_work(tp=%p): afid=%d parse_done=%d exit_samples=%d\n", tp, tp->t_afid, parse_done, exit_samples);
    }
#endif

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
	    __pmNoMem("log_callback: new AFctl_t entry calloc",
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
		if (lfp->lf_resp != (pmResult *)0) {
		    pmFreeResult(lfp->lf_resp);
		    lfp->lf_resp =(pmResult *)0;
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
		    __pmNoMem("log_callback: new lastfetch_t entry calloc",
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

	if ((sts = myFetch(fp->f_numpmid, fp->f_pmidlist, &pb)) < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
		fprintf(stderr, "callback: disconnecting because myFetch failed: %s\n", pmErrStr(sts));
#endif
	    disconnect(sts);
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "callback: fetch group %p (%d metrics)\n", fp, fp->f_numpmid);
#endif

	/*
	 * hook to rewrite PDU buffer ...
	lfp */
	pb = rewrite_pdu(pb, archive_version);

	if (rflag) {
	    /*
	     * bytes = PDU len - sizeof (header) + 2 * sizeof (int)
             * see logputresult() in libpcp/logutil.c for details of how
	     * a PDU buffer is reformatted to make len shorter by one int
	     * before the record is written to the external file
	     */
	    pdu_bytes += ((__pmPDUHdr *)pb)->len - sizeof (__pmPDUHdr) + 
		2*sizeof(int); 
	    pdu_metrics += fp->f_numpmid;
	}

	/*
	 * Even without a -v option, we may need to switch volumes
	 * if the data file exceeds 2^31-1 bytes
	 */
	peek_offset = ftell(logctl.l_mfp);
	peek_offset += ((__pmPDUHdr *)pb)->len - sizeof(__pmPDUHdr) + 2*sizeof(int);
	if (peek_offset > 0x7fffffff) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
		fprintf(stderr, "callback: new volume based on max size, currently %ld\n", ftell(logctl.l_mfp));
#endif
	    (void)newvolume(VOL_SW_MAX);
	}

	/*
	 * would prefer to save this up until after any meta data and/or
	 * temporal index writes, but __pmDecodeResult changes the pointers
	 * in the pdu buffer for the non INSITU values ... sigh
	 */
	last_log_offset = ftell(logctl.l_mfp);
	assert(last_log_offset >= 0);
	if ((sts = __pmLogPutResult2(&logctl, pb)) < 0) {
	    fprintf(stderr, "__pmLogPutResult2: %s\n", pmErrStr(sts));
	    exit(1);
	}

	__pmOverrideLastFd(fileno(logctl.l_mfp));
	resp = NULL; /* silence coverity */
	if ((sts = __pmDecodeResult(pb, &resp)) < 0) {
	    fprintf(stderr, "__pmDecodeResult: %s\n", pmErrStr(sts));
	    exit(1);
	}
	setavail(resp);
	resp_tval.tv_sec = resp->timestamp.tv_sec;
	resp_tval.tv_usec = resp->timestamp.tv_usec;

	needti = 0;
	old_meta_offset = ftell(logctl.l_mdfp);
	assert(old_meta_offset >= 0);
	for (i = 0; i < resp->numpmid; i++) {
	    pmValueSet	*vsp = resp->vset[i];
	    pmDesc	desc;
	    char	**names = NULL;
	    int		numnames = 0;

	    sts = __pmLogLookupDesc(&logctl, vsp->pmid, &desc);
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
		if ((sts = __pmLogPutDesc(&logctl, &desc, numnames, names)) < 0) {
		    fprintf(stderr, "__pmLogPutDesc: %s\n", pmErrStr(sts));
		    exit(1);
		}
		if (numnames > 0) {
		    free(names);
		}
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
		 * __pmLogGetInDom has been replaced by __localLogGetInDom so that
		 * the timestamp of the retrieved indom is also returned. The timestamp
		 * is then used to decide if the indom needs to be refreshed.
		 */
		__pmTimeval indom_tval;
		numinst = __localLogGetInDom(&logctl, desc.indom, &indom_tval, &instlist, &namelist);
		if (numinst < 0)
		    needindom = 1;
		else {
		    needindom = 0;
		    /* Need to see if result's insts all exist
		     * somewhere in the hashed/cached insts.
		     * Thus a potential numval^2 search.
                     */
		    for (j = 0; j < vsp->numval; j++) {
			for (k = 0; k < numinst; k++) {
			    if (vsp->vlist[j].inst == instlist[k])
				break;
			}
			if (k == numinst) {
			    needindom = 1;
			    break;
			}
		    }
		}
		/* 
		 * Check here that the instance domain has not been changed
		 * by a previous iteration of this loop.
		 * So, the timestamp of resp must be after the update timestamp
		 * of the target instance domain.
		 */
		if (needindom == 0 && lfp->lf_resp != (pmResult *)0 &&
		    __pmTimevalSub(&resp_tval, &indom_tval) < 0 )
		    needindom = check_inst(vsp, i, lfp->lf_resp);

		if (needindom) {
		    /*
		     * Note.  We do NOT free() instlist and namelist allocated
		     *	  here ... look for magic below log{Put,Get}InDom ...
		     */
		    if ((numinst = pmGetInDom(desc.indom, &instlist, &namelist)) < 0) {
			fprintf(stderr, "pmGetInDom(%s): %s\n", pmInDomStr(desc.indom), pmErrStr(numinst));
			exit(1);
		    }
		    tmp.tv_sec = (__int32_t)resp->timestamp.tv_sec;
		    tmp.tv_usec = (__int32_t)resp->timestamp.tv_usec;
		    if ((sts = __pmLogPutInDom(&logctl, desc.indom, &tmp, numinst, instlist, namelist)) < 0) {
			fprintf(stderr, "__pmLogPutInDom: %s\n", pmErrStr(sts));
			exit(1);
		    }
		    needti = 1;
#ifdef PCP_DEBUG
		    if (pmDebug & DBG_TRACE_APPL2)
			fprintf(stderr, "callback: indom (%s) changed\n", pmInDomStr(desc.indom));
#endif
		}
	    }
	}

	if (ftell(logctl.l_mfp) > flushsize) {
	    needti = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
		fprintf(stderr, "callback: file size (%d) reached flushsize (%d)\n", (int)ftell(logctl.l_mfp), flushsize);
#endif
	}

	if (last_log_offset == 0 || last_log_offset == sizeof(__pmLogLabel)+2*sizeof(int)) {
	    /* first result in this volume */
	    needti = 1;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_APPL2)
		fprintf(stderr, "callback: first result for this volume\n");
#endif
	}

	if (needti) {
	    /*
	     * need to unwind seek pointer to start of most recent
	     * result (but if this is the first one, skip the label
	     * record, what a crock), ... ditto for the meta data
	     */
	    new_offset = ftell(logctl.l_mfp);
	    assert(new_offset >= 0);
	    new_meta_offset = ftell(logctl.l_mdfp);
	    assert(new_meta_offset >= 0);
	    fseek(logctl.l_mfp, last_log_offset, SEEK_SET);
	    fseek(logctl.l_mdfp, old_meta_offset, SEEK_SET);
	    tmp.tv_sec = (__int32_t)resp->timestamp.tv_sec;
	    tmp.tv_usec = (__int32_t)resp->timestamp.tv_usec;
	    __pmLogPutIndex(&logctl, &tmp);
	    /*
	     * ... and put them back
	     */
	    fseek(logctl.l_mfp, new_offset, SEEK_SET);
	    fseek(logctl.l_mdfp, new_meta_offset, SEEK_SET);
	    flushsize = ftell(logctl.l_mfp) + 100000;
	}

	last_stamp = resp->timestamp;	/* struct assignment */

	if (lfp->lf_resp != (pmResult *)0) {
	    /*
	     * release memory that is allocated and pinned in pmDecodeResult
	     */
	    pmFreeResult(lfp->lf_resp);
	}
	lfp->lf_resp = resp;
	if (lfp->lf_pb != NULL)
	    __pmUnpinPDUBuf(lfp->lf_pb);
	lfp->lf_pb = pb;
    }

    if (rflag && tp->t_size == 0 && pdu_metrics > 0) {
	char	*name = NULL;
	int	taskindex;
        int     i;

	tp->t_size = pdu_bytes;

	if (pdu_metrics > 1)
	    fprintf(stderr, "\nGroup [%d metrics] {\n", pdu_metrics);
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
            if (pdu_metrics > 1)
                fprintf(stderr, "}");
        }
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
		    __pmtimevalToReal(&tp->t_delta), pdu_bytes);
	    fprintf(stderr, "or %.2f Mbytes/day\n",
		((double)pdu_bytes * 24 * 60 * 60) /
		(1024 * 1024 * __pmtimevalToReal(&tp->t_delta)));
	}
    }

    if (exit_samples > 0)
	exit_samples--;

    if (exit_samples == 0)
	/* run out of samples in sample counter, so stop logging */
	run_done(0, "Sample limit reached");

    if (exit_bytes != -1 && 
        (vol_bytes + ftell(logctl.l_mfp) >= exit_bytes)) 
        /* reached exit_bytes limit, so stop logging */
        run_done(0, "Byte limit reached");

    if (vol_switch_samples > 0 &&
	++vol_samples_counter == vol_switch_samples) {
        (void)newvolume(VOL_SW_COUNTER);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "callback: new volume based on samples (%d)\n", vol_samples_counter);
#endif
    }

    if (vol_switch_bytes > 0 &&
        (ftell(logctl.l_mfp) >= vol_switch_bytes)) {
        (void)newvolume(VOL_SW_BYTES);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL2)
	    fprintf(stderr, "callback: new volume based on size (%d)\n", (int)ftell(logctl.l_mfp));
#endif
    }

}

int
putmark(void)
{
    struct {
	__pmPDU		hdr;
	__pmTimeval	timestamp;	/* when returned */
	int		numpmid;	/* zero PMIDs to follow */
	__pmPDU		tail;
    } mark;

    if (last_stamp.tv_sec == 0 && last_stamp.tv_usec == 0)
	/* no earlier pmResult, no point adding a mark record */
	return 0;

    mark.hdr = htonl((int)sizeof(mark));
    mark.tail = mark.hdr;
    mark.timestamp.tv_sec = last_stamp.tv_sec;
    mark.timestamp.tv_usec = last_stamp.tv_usec + 1000;	/* + 1msec */
    if (mark.timestamp.tv_usec > 1000000) {
	mark.timestamp.tv_usec -= 1000000;
	mark.timestamp.tv_sec++;
    }
    mark.timestamp.tv_sec = htonl(mark.timestamp.tv_sec);
    mark.timestamp.tv_usec = htonl(mark.timestamp.tv_usec);
    mark.numpmid = htonl(0);

    if (fwrite(&mark, 1, sizeof(mark), logctl.l_mfp) != sizeof(mark))
	return -oserror();
    else
	return 0;
}
