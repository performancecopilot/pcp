/*
 * Copyright (c) 2013-2014,2017-2018 Red Hat.
 * Copyright (c) 1995-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "pmda.h"
#include "libdefs.h"
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

/*
 * Count the number of instances in an instance domain
 */

int
__pmdaCntInst(pmInDom indom, pmdaExt *pmda)
{
    int		i;
    int		sts = 0;

    if (indom == PM_INDOM_NULL)
	return 1;
    if (pmdaCacheOp(indom, PMDA_CACHE_CHECK)) {
	sts = pmdaCacheOp(indom, PMDA_CACHE_SIZE_ACTIVE);
    }
    else {
	for (i = 0; i < pmda->e_nindoms; i++) {
	    if (pmda->e_indoms[i].it_indom == indom) {
		sts = pmda->e_indoms[i].it_numinst;
		break;
	    }
	}
	if (i == pmda->e_nindoms) {
	    char	strbuf[20];
	    pmNotifyErr(LOG_WARNING, "__pmdaCntInst: unknown indom %s", pmInDomStr_r(indom, strbuf, sizeof(strbuf)));
	}
    }

    if (pmDebugOptions.indom) {
	char	strbuf[20];
	fprintf(stderr, "__pmdaCntInst(indom=%s) -> %d\n", pmInDomStr_r(indom, strbuf, sizeof(strbuf)), sts);
    }

    return sts;
}

/*
 * Commence a new round of instance selection
 */

static pmdaIndom	last;

/*
 * State between here and __pmdaNextInst is a little strange
 *
 * for the classical method,
 *    - pmda->e_idp is set here (points into indomtab[]) and
 *      pmda->e_idp->it_indom is used in __pmdaNextInst
 *
 * for the cache method
 *    - pmda->e_idp is set here (points into last) which is also set
 *      up with the it_indom field (other fields in last are not used),
 *      and pmda->e_idp->it_indom in __pmdaNextInst
 *
 * In both cases, pmda->e_ordinal and pmda->e_singular are set here
 * and updated in __pmdaNextInst.
 *
 * As in most other places, this is not thread-safe and we assume we
 * call __pmdaStartInst and then repeatedly call __pmdaNextInst all
 * for the same indom, before calling __pmdaStartInst again.
 *
 * If we could do this again, adding an indom argument to __pmdaNextInst
 * would be a better design, but the API to __pmdaNextInst has escaped.
 */
void
__pmdaStartInst(pmInDom indom, pmdaExt *pmda)
{
    int		i;

    pmda->e_ordinal = pmda->e_singular = -1;
    if (indom == PM_INDOM_NULL) {
	/* singular value */
	pmda->e_singular = 0;
    }
    else {
	if (pmdaCacheOp(indom, PMDA_CACHE_CHECK)) {
	    pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);
	    last.it_indom = indom;
	    pmda->e_idp = &last;
	    pmda->e_ordinal = 0;
	}
	else {
	    for (i = 0; i < pmda->e_nindoms; i++) {
		if (pmda->e_indoms[i].it_indom == indom) {
		    /* multiple values are possible */
		    pmda->e_idp = &pmda->e_indoms[i];
		    pmda->e_ordinal = 0;
		    break;
		}
	    }
	}
    }

    if (pmDebugOptions.indom) {
	char	strbuf[20];
	fprintf(stderr, "__pmdaStartInst(indom=%s) e_ordinal=%d\n",
	    pmInDomStr_r(indom, strbuf, sizeof(strbuf)), pmda->e_ordinal);
    }
    return;
}

/* 
 * Select the next instance
 */

int
__pmdaNextInst(int *inst, pmdaExt *pmda)
{
    int		j;
    int		myinst;

    if (pmda->e_singular == 0) {
	/* PM_INDOM_NULL ... just the one value */
	*inst = 0;
	pmda->e_singular = -1;
	return 1;
    }
    if (pmda->e_ordinal >= 0) {
	/* scan for next value in the profile */
	if (pmda->e_idp == &last) {
	    /* cache-driven */
	    while ((myinst = pmdaCacheOp(pmda->e_idp->it_indom, PMDA_CACHE_WALK_NEXT)) != -1) {
		pmda->e_ordinal++;
		if (__pmInProfile(pmda->e_idp->it_indom, pmda->e_prof, myinst)) {
		    *inst = myinst;
		    if (pmDebugOptions.indom) {
			char	strbuf[20];
			fprintf(stderr, "__pmdaNextInst(indom=%s) -> %d e_ordinal=%d (cache)\n",
			    pmInDomStr_r(pmda->e_idp->it_indom, strbuf, sizeof(strbuf)), myinst, pmda->e_ordinal);
		    }
		    return 1;
		}
	    }
	}
	else {
	    /* indomtab[]-driven */
	    for (j = pmda->e_ordinal; j < pmda->e_idp->it_numinst; j++) {
		if (__pmInProfile(pmda->e_idp->it_indom, 
				 pmda->e_prof, 
				 pmda->e_idp->it_set[j].i_inst)) {
		    *inst = pmda->e_idp->it_set[j].i_inst;
		    pmda->e_ordinal = j+1;
		    if (pmDebugOptions.indom) {
			char	strbuf[20];
			fprintf(stderr, "__pmdaNextInst(indom=%s) -> %d e_ordinal=%d\n",
			    pmInDomStr_r(pmda->e_idp->it_indom, strbuf, sizeof(strbuf)), *inst, pmda->e_ordinal);
		    }
		    return 1;
		}
	    }
	}
	pmda->e_ordinal = -1;
    }
    return 0;
}

static int
__pmdaCountInst(pmDesc *dp, pmdaExt *pmda)
{
    int		inst, numval = 0;

    if (dp->indom != PM_INDOM_NULL) {
	/* count instances in indom */
	__pmdaStartInst(dp->indom, pmda);
	while (__pmdaNextInst(&inst, pmda))
	    numval++;
    }
    else {
	/* singular instance domains */
	numval = 1;
    }
    return numval;
}


/*
 * Helper routines for performing metric table searches.
 *
 * There are currently three ways - using the PMID hash that hangs off
 * of the e_ext structure, using the direct mapping mechanism (require
 * PMIDs be allocated one after-the-other, all in one cluster), or via
 * a linear search of the metric table array.
 */

static pmdaMetric *
__pmdaHashedSearch(pmID pmid, __pmHashCtl *hash)
{
    __pmHashNode *node;

    if ((node = __pmHashSearch(pmid, hash)) == NULL)
	return NULL;
    return (pmdaMetric *)node->data;
}

static pmdaMetric *
__pmdaDirectSearch(pmID pmid, pmdaExt *pmda)
{
    __pmID_int	*pmidp = (__pmID_int *)&pmid;

    /*
     * pmidp->domain is correct ... PMCD guarantees this, but
     * pmda->e_direct only works for a single cluster
     */
    if (pmidp->item < pmda->e_nmetrics && 
	pmidp->cluster == 
	((__pmID_int *)&pmda->e_metrics[pmidp->item].m_desc.pmid)->cluster) {
	/* pmidp->item is unsigned, so must be >= 0 */
	return &pmda->e_metrics[pmidp->item];
    }
    return NULL;
}

static pmdaMetric *
__pmdaLinearSearch(pmID pmid, pmdaExt *pmda)
{
    int		i;

    for (i = 0; i < pmda->e_nmetrics; i++) {
	if (pmda->e_metrics[i].m_desc.pmid == pmid) {
	    /* found the hard way */
	    return &pmda->e_metrics[i];
	}
    }
    return NULL;
}

static pmdaMetric *
__pmdaMetricSearch(pmdaExt *pmda, pmID pmid, pmdaMetric *mbuf, e_ext_t *extp)
{
    pmdaMetric	*metap;

    if (pmda->e_flags & PMDA_EXT_FLAG_HASHED)
	metap = __pmdaHashedSearch(pmid, &extp->hashpmids);
    else if (pmda->e_direct)
	metap = __pmdaDirectSearch(pmid, pmda);
    else
	metap = __pmdaLinearSearch(pmid, pmda);

    /* possibly a highly dynamic metric with null metrictab[] */
    if (!metap) {
	metap = mbuf;
	memset(metap, 0, sizeof(pmdaMetric));

	if (extp->dispatch->version.any.desc != NULL) {
	    /* may need a temporary pmdaMetric for callback interfaces */
	    (*(extp->dispatch->version.any.desc))(pmid, &mbuf->m_desc, pmda);
        }
    }
    return metap;
}

/*
 * Save the profile away for use in __pmdaNextInst() during subsequent
 * fetches ... it is the _caller_ of pmdaProfile()'s responsibility to
 * ensure that the profile is not freed while it is being used here.
 *
 * For DSO pmdas, the profiles are managed per client in DoProfile() and
 * DeleteClient() but sent to the pmda in SendFetch()
 *
 * For daemon pmdas, the profile is received from pmcd in __pmdaMainPDU
 * and the last received profile is held there
 */

int
pmdaProfile(pmProfile *prof, pmdaExt *pmda)
{
    pmda->e_prof = prof;
    return 0;
}

/*
 * Return description of an instance or instance domain
 */

int
pmdaInstance(pmInDom indom, int inst, char *name, pmInResult **result, pmdaExt *pmda)
{
    int			i;
    int			namelen;
    int			err = 0;
    pmInResult  	*res;
    pmdaIndom		*idp = NULL;	/* initialize to pander to gcc */
    int			have_cache = 0;
    int			myinst;
    char		*np;

    if (pmdaCacheOp(indom, PMDA_CACHE_CHECK)) {
	have_cache = 1;
    }
    else {
	/*
	 * check this is an instance domain we know about -- code below
	 * assumes this test is complete
	 */
	for (i = 0; i < pmda->e_nindoms; i++) {
	    if (pmda->e_indoms[i].it_indom == indom)
		break;
	}
	if (i >= pmda->e_nindoms)
	    return PM_ERR_INDOM;
	idp = &pmda->e_indoms[i];
    }

    if ((res = (pmInResult *)malloc(sizeof(*res))) == NULL)
        return -oserror();
    res->indom = indom;

    if (name == NULL && inst == PM_IN_NULL)
	res->numinst = __pmdaCntInst(indom, pmda);
    else
	res->numinst = 1;

    if (inst == PM_IN_NULL) {
	if ((res->instlist = (int *)malloc(res->numinst * sizeof(res->instlist[0]))) == NULL) {
	    free(res);
	    return -oserror();
	}
    }
    else
	res->instlist = NULL;

    if (name == NULL) {
	if ((res->namelist = (char **)malloc(res->numinst * sizeof(res->namelist[0]))) == NULL) {
	    __pmFreeInResult(res);
	    return -oserror();
	}
	for (i = 0; i < res->numinst; i++)
	    res->namelist[0] = NULL;
    }
    else
	res->namelist = NULL;

    if (name == NULL && inst == PM_IN_NULL) {
	/* return inst and name for everything */
	if (have_cache) {
	    pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);
	    i = 0;
	    while (i < res->numinst && (myinst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) != -1) {
		if (pmdaCacheLookup(indom, myinst, &np, NULL) != PMDA_CACHE_ACTIVE)
		    continue;

		res->instlist[i] = myinst;
		if ((res->namelist[i++] = strdup(np)) == NULL) {
		    __pmFreeInResult(res);
		    return -oserror();
		}
	    }
	}
	else {
	    for (i = 0; i < res->numinst; i++) {
		res->instlist[i] = idp->it_set[i].i_inst;
		if ((res->namelist[i] = strdup(idp->it_set[i].i_name)) == NULL) {
		    __pmFreeInResult(res);
		    return -oserror();
		}
	    }
	}
    }
    else if (name == NULL) {
	/* given an inst, return the name */
	if (have_cache) {
	    if (pmdaCacheLookup(indom, inst, &np, NULL) == PMDA_CACHE_ACTIVE) {
		if ((res->namelist[0] = strdup(np)) == NULL) {
		    __pmFreeInResult(res);
		    return -oserror();
		}
	    }
	    else
		err = 1;
	}
	else {
	    for (i = 0; i < idp->it_numinst; i++) {
		if (inst == idp->it_set[i].i_inst) {
		    if ((res->namelist[0] = strdup(idp->it_set[i].i_name)) == NULL) {
			__pmFreeInResult(res);
			return -oserror();
		    }
		    break;
		}
	    }
	    if (i == idp->it_numinst)
		err = 1;
	}
    }
    else if (inst == PM_IN_NULL && (namelen = (int)strlen(name)) > 0) {
	if (have_cache) {
	    if (pmdaCacheLookupName(indom, name, &myinst, NULL) == PMDA_CACHE_ACTIVE)
		res->instlist[0] = myinst;
	    else
		err = 1;
	}
	else {
	    /* given a name, return an inst. If the name contains spaces,
	     * only exact matches are good enough for us, otherwise, we're
	     * prepared to accept a match upto the first space in the
	     * instance name on the assumption that pmdas will play by the
	     * rules and guarantee the first "word" in the instance name
	     * is unique. That allows for the things like "1 5 15" to match
	     * instances for kernel.all.load["1 minute","5 minute","15 minutes"]
	     */
	    char * nspace = strchr (name, ' ');

	    for (i = 0; i < idp->it_numinst; i++) {
		char *instname = idp->it_set[i].i_name;
		if (strcmp(name, instname) == 0) {
		    /* accept an exact match */
		    if (pmDebugOptions.libpmda) {
			fprintf(stderr, 
				"pmdaInstance: exact match name=%s id=%d\n",
				name, idp->it_set[i].i_inst);
		    }
		    res->instlist[0] = idp->it_set[i].i_inst;
		    break;
		}
		else if (nspace == NULL) {
		    /* all of name must match instname up to the the first
		     * space in instname.  */
		    char *p = strchr(instname, ' ');
		    if (p != NULL) {
			int len = (int)(p - instname);
			if (namelen == len && strncmp(name, instname, len) == 0) {
			    if (pmDebugOptions.libpmda) {
				fprintf(stderr, "pmdaInstance: matched argument name=\"%s\" with indom id=%d name=\"%s\" len=%d\n",
				    name, idp->it_set[i].i_inst, instname, len);
			    }
			    res->instlist[0] = idp->it_set[i].i_inst;
			    break;
			}
		    }
		}
	    }
	    if (i == idp->it_numinst)
		err = 1;
	}
    }
    else
	err = 1;
    if (err == 1) {
	/* bogus arguments or instance id/name */
	__pmFreeInResult(res);
	return PM_ERR_INST;
    }

    *result = res;
    return 0;
}

/*
 * The first byte of the (unused) timestamp field has been
 * co-opted as a flags byte - now indicating state changes
 * that have happened within a PMDA and that need to later
 * be propogated through to any connected clients.
 */

static void
__pmdaEncodeStatus(pmResult *result, unsigned char byte)
{
    unsigned char	*flags;

    memset(&result->timestamp, 0, sizeof(result->timestamp));
    if (byte) {
	flags = (unsigned char *)&result->timestamp;
	*flags |= byte;
    }
}

#define PMDA_STATUS_CHANGE (PMDA_EXT_LABEL_CHANGE|PMDA_EXT_NAMES_CHANGE)

/*
 * Resize the pmResult and call the e_callback for each metric instance
 * required in the profile.
 */

int
pmdaFetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt *pmda)
{
    int			i;		/* over pmidlist[] */
    int			j;		/* over metatab and vset->vlist[] */
    int			sts;
    int			need;
    int			inst;
    int			numval;
    int			version;
    unsigned char	flags;
    pmValueSet		*vset;
    pmValueSet		*tmp_vset;
    pmDesc		*dp;
    pmdaMetric          metabuf;
    pmdaMetric		*metap;
    pmAtomValue		atom;
    int			type;
    int			lsts;
    char		idbuf[20];
    char		strbuf[20];
    e_ext_t		*extp = (e_ext_t *)pmda->e_ext;

    if ((pmDebugOptions.libpmda) && (pmDebugOptions.desperate)) {
	fprintf(stderr, "pmdaFetch(%d, pmid[0] %s", numpmid, pmIDStr_r(pmidlist[0], idbuf, sizeof(idbuf)));
	if (numpmid > 1)
	    fprintf(stderr, "... pmid[%d] %s", numpmid-1, pmIDStr_r(pmidlist[numpmid-1], idbuf, sizeof(idbuf)));
	fprintf(stderr, ", ...) called\n");
    }

    if (extp->dispatch->version.any.ext != pmda)
	fprintf(stderr, "Botch: pmdaFetch: PMDA domain=%d pmda=%p extp=%p backpointer=%p pmda-via-backpointer %p NOT EQUAL to pmda\n",
	    pmda->e_domain, pmda, extp, extp->dispatch, extp->dispatch->version.any.ext);

    version = extp->dispatch->comm.pmda_interface;
    if (version >= PMDA_INTERFACE_5)
	__pmdaSetContext(pmda->e_context);

    if (numpmid > extp->maxnpmids) {
	if (extp->res != NULL)
	    free(extp->res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
	if ((extp->res = (pmResult *) malloc(need)) == NULL)
	    return -oserror();
	extp->maxnpmids = numpmid;
    }
    extp->res->numpmid = numpmid;

    flags = 0;
    if (version >= PMDA_INTERFACE_7 && (pmda->e_flags & PMDA_STATUS_CHANGE)) {
	if (pmda->e_flags & PMDA_EXT_LABEL_CHANGE)
	    flags |= PMCD_LABEL_CHANGE;
	if (pmda->e_flags & PMDA_EXT_NAMES_CHANGE)
	    flags |= PMCD_NAMES_CHANGE;
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "pmdaFetch flags pmda=0x%x to pmcd=0x%x\n",
			    pmda->e_flags, (int)flags);
    }
    __pmdaEncodeStatus(extp->res, flags);

    /* Look up the pmDesc for the incoming pmids in our pmdaMetrics tables,
       if present.  Fall back to .desc callback if not found (for highly
       dynamic pmdas). */
    for (i = 0; i < numpmid; i++) {
	metap = __pmdaMetricSearch(pmda, pmidlist[i], &metabuf, extp);
	/*
	 * if search failed, then metap == metabuf, and metabuf.m_desc.pmid
	 * will be zero
	 */
	dp = &(metap->m_desc);
	if (dp->pmid != 0)
	    numval = __pmdaCountInst(dp, pmda);
	else {
	    /* dynamic name metrics may often vanish, avoid log spam */
	    if (version < PMDA_INTERFACE_4) {
		pmNotifyErr(LOG_ERR,
			"pmdaFetch: Requested metric %s is not defined",
			 pmIDStr_r(pmidlist[i], strbuf, sizeof(strbuf)));
	    }
	    numval = PM_ERR_PMID;
	}

	/* Must use individual malloc()s because of pmFreeResult() */
	if (numval >= 1)
	    extp->res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) +
					    (numval - 1)*sizeof(pmValue));
	else
	    extp->res->vset[i] = vset = (pmValueSet *)malloc(sizeof(pmValueSet) -
					    sizeof(pmValue));
	if (vset == NULL) {
	    sts = -oserror();
	    goto error;
	}
	vset->pmid = pmidlist[i];
	vset->numval = numval;
	vset->valfmt = PM_VAL_INSITU;
	if (vset->numval <= 0)
	    continue;

	if (dp->indom == PM_INDOM_NULL)
	    inst = PM_IN_NULL;
	else {
	    __pmdaStartInst(dp->indom, pmda);
	    __pmdaNextInst(&inst, pmda);
	}
	type = dp->type;
	j = 0;
	do {
	    if (j == numval) {
		/* more instances than expected! */
		numval++;
		extp->res->vset[i] = tmp_vset = (pmValueSet *)realloc(vset,
			    sizeof(pmValueSet) + (numval - 1)*sizeof(pmValue));
		if (tmp_vset == NULL) {
		    free(vset);
		    vset = NULL;
		    sts = -oserror();
		    goto error;
		}
		vset = tmp_vset;
	    }
	    vset->vlist[j].inst = inst;

	    if ((sts = (*(pmda->e_fetchCallBack))(metap, inst, &atom)) < 0) {
		pmIDStr_r(dp->pmid, strbuf, sizeof(strbuf));
		if (sts == PM_ERR_PMID) {
		    pmNotifyErr(LOG_ERR, 
		        "pmdaFetch: PMID %s not handled by fetch callback\n",
				strbuf);
		}
		else if (sts == PM_ERR_INST) {
		    if (pmDebugOptions.libpmda) {
			pmNotifyErr(LOG_ERR,
			    "pmdaFetch: Instance %d of PMID %s not handled by fetch callback\n",
				    inst, strbuf);
		    }
		}
		else if (sts == PM_ERR_APPVERSION ||
			 sts == PM_ERR_PERMISSION ||
			 sts == PM_ERR_AGAIN ||
			 sts == PM_ERR_NYI) {
		    if (pmDebugOptions.libpmda) {
			pmNotifyErr(LOG_ERR,
			     "pmdaFetch: Unavailable metric PMID %s[%d]\n",
				    strbuf, inst);
		    }
		}
		else {
		    pmNotifyErr(LOG_ERR,
			"pmdaFetch: Fetch callback error from metric PMID %s[%d]: %s\n",
				strbuf, inst, pmErrStr(sts));
		}
	    }
	    else {
		/*
		 * PMDA_INTERFACE_2
		 *	>= 0 => OK
		 * PMDA_INTERFACE_3 or PMDA_INTERFACE_4
		 *	== 0 => no values
		 *	> 0  => OK
		 * PMDA_INTERFACE_5 or later
		 *	== 0 (PMDA_FETCH_NOVALUES) => no values
		 *	== 1 (PMDA_FETCH_STATIC) or > 2 => OK
		 *	== 2 (PMDA_FETCH_DYNAMIC) => OK and free(atom.vp)
		 *	     after __pmStuffValue() called
		 */
		if ((version == PMDA_INTERFACE_2) || (version >= PMDA_INTERFACE_3 && sts > 0)) {

		    if ((lsts = __pmStuffValue(&atom, &vset->vlist[j], type)) == PM_ERR_TYPE) {
			pmNotifyErr(LOG_ERR, "pmdaFetch: Descriptor type (%s) for metric %s is bad",
				    pmTypeStr_r(type, strbuf, sizeof(strbuf)),
				    pmIDStr_r(dp->pmid, idbuf, sizeof(idbuf)));
		    }
		    else if (lsts >= 0) {
			vset->valfmt = lsts;
			j++;
		    }
		    if (version >= PMDA_INTERFACE_5 && sts == PMDA_FETCH_DYNAMIC) {
			if (type == PM_TYPE_STRING)
			    free(atom.cp);
			else if (type == PM_TYPE_AGGREGATE)
			    free(atom.vbp);
			else {
			    pmNotifyErr(LOG_WARNING, "pmdaFetch: Attempt to free value for metric %s of wrong type %s\n",
					pmIDStr_r(dp->pmid, idbuf, sizeof(idbuf)),
					pmTypeStr_r(type, strbuf, sizeof(strbuf)));
			}
		    }
		    if (lsts < 0)
			sts = lsts;
		}
	    }
	} while (dp->indom != PM_INDOM_NULL && __pmdaNextInst(&inst, pmda));

	if (j == 0)
	    vset->numval = sts;
	else
	    vset->numval = j;
    }

    /* success, we will send this PDU - safe to clear flags */
    pmda->e_flags &= ~PMDA_STATUS_CHANGE;
    *resp = extp->res;
    return 0;

error:

    if (i) {
	extp->res->numpmid = i;
	__pmFreeResultValues(extp->res);
    }
    return sts;
}

/*
 * Return the metric description
 */

int
pmdaDesc(pmID pmid, pmDesc *desc, pmdaExt *pmda)
{
    e_ext_t		*extp = (e_ext_t *)pmda->e_ext;
    pmdaMetric		*metric;
    char		strbuf[32];
    int			version;
    int			sts;

    version = extp->dispatch->comm.pmda_interface;
    if (version >= PMDA_INTERFACE_5)
	__pmdaSetContext(pmda->e_context);

    if (pmda->e_flags & PMDA_EXT_FLAG_HASHED)
	metric = __pmdaHashedSearch(pmid, &extp->hashpmids);
    else if (pmda->e_direct)
	metric = __pmdaDirectSearch(pmid, pmda);
    else
	metric = __pmdaLinearSearch(pmid, pmda);

    if (metric) {
	*desc = metric->m_desc;
	sts = 0;
    }
    else {
	sts = PM_ERR_PMID;
	/* dynamic name metrics may often vanish, avoid log spam */
	if (version < PMDA_INTERFACE_4)
	    pmNotifyErr(LOG_ERR, "pmdaDesc: Requested metric %s is not defined",
			    pmIDStr_r(pmid, strbuf, sizeof(strbuf)));
    }

    if ((pmDebugOptions.libpmda) && (pmDebugOptions.desperate)) {
	char	dbgbuf[20];
	fprintf(stderr, "pmdaDesc(%s, ...) method=", pmIDStr_r(pmid, dbgbuf, sizeof(dbgbuf)));
	if (pmda->e_flags & PMDA_EXT_FLAG_HASHED)
	    fprintf(stderr, "hashed");
	else if (pmda->e_direct)
	    fprintf(stderr, "direct");
	else
	    fprintf(stderr, "linear");
	if (sts == 0)
	    fprintf(stderr, " success\n");
	else
	    fprintf(stderr, " fail\n");
    }

    return sts;
}

/*
 * Return the help text for a metric
 */

int
pmdaText(int ident, int type, char **buffer, pmdaExt *pmda)
{
    e_ext_t		*extp = (e_ext_t *)pmda->e_ext;

    if (extp->dispatch->comm.pmda_interface >= PMDA_INTERFACE_5)
	__pmdaSetContext(pmda->e_context);

    if (pmda->e_help >= 0) {
	if ((type & PM_TEXT_PMID) == PM_TEXT_PMID)
	    *buffer = pmdaGetHelp(pmda->e_help, (pmID)ident, type);
	else
	    *buffer = pmdaGetInDomHelp(pmda->e_help, (pmInDom)ident, type);
    }
    else
	*buffer = NULL;

    return (*buffer == NULL) ? PM_ERR_TEXT : 0;
}

/*
 * Provide default handlers to fill set(s) of labels for the context
 * or requested ID of type domain, indom, cluster, item or instances.
 */

int
pmdaLabel(int ident, int type, pmLabelSet **lpp, pmdaExt *pmda)
{
    e_ext_t		*extp = (e_ext_t *)pmda->e_ext;
    pmLabelSet		*rlp, *lp = NULL;
    size_t		size;
    char		idbuf[32], *idp;
    char		errbuf[PM_MAXERRMSGLEN];
    int			sts = 0, count, inst, numinst;

    if (extp->dispatch->comm.pmda_interface >= PMDA_INTERFACE_5)
	__pmdaSetContext(pmda->e_context);

    switch (type) {
    case PM_LABEL_CONTEXT:
	if (pmDebugOptions.labels)
	    fprintf(stderr, "pmdaLabel: context %d labels request\n",
			pmda->e_context);
	if ((lp = *lpp) == NULL)	/* use default handler */
	    return __pmGetContextLabels(lpp);
	return pmdaAddLabelFlags(lp, type);

    case PM_LABEL_DOMAIN:
	if (pmDebugOptions.labels)
	    fprintf(stderr, "pmdaLabel: domain %d (%s) labels request\n",
			pmda->e_domain, pmda->e_name);
	if ((lp = *lpp) == NULL)	/* use default handler */
	    return __pmGetDomainLabels(pmda->e_domain, pmda->e_name, lpp);
	return pmdaAddLabelFlags(lp, type);

    case PM_LABEL_INDOM:
	if (pmDebugOptions.labels)
	    fprintf(stderr, "pmdaLabel: InDom %s labels request\n",
			    pmInDomStr_r(ident, idbuf, sizeof(idbuf)));
	if ((lp = *lpp) == NULL)	/* no default handler */
	    return 0;
	return pmdaAddLabelFlags(lp, type);

    case PM_LABEL_CLUSTER:
	if (pmDebugOptions.labels) {
	    pmIDStr_r(ident, idbuf, sizeof(idbuf));
	    idp = rindex(idbuf, '.');
	    *idp = '\0';	/* drop the final (item) part */
	    fprintf(stderr, "pmdaLabel: cluster %s labels request\n", idbuf);
	}
	if ((lp = *lpp) == NULL)	/* no default handler */
	    return 0;
	return pmdaAddLabelFlags(lp, type);

    case PM_LABEL_ITEM:
	if (pmDebugOptions.labels)
	    fprintf(stderr, "pmdaLabel: cluster %s labels request\n",
			    pmIDStr_r(ident, idbuf, sizeof(idbuf)));
	if ((lp = *lpp) == NULL)	/* no default handler */
	    return 0;
	return pmdaAddLabelFlags(lp, type);

    case PM_LABEL_INSTANCES:
	if (extp->dispatch->comm.pmda_interface < PMDA_INTERFACE_7)
	    return 0;

	if (ident == PM_INDOM_NULL)
	    numinst = 1;
	else
	    numinst = __pmdaCntInst(ident, pmda);

	if (pmDebugOptions.labels)
	    fprintf(stderr, "pmdaLabel: InDom %s %d instance labels request\n",
			    pmInDomStr_r(ident, idbuf, sizeof(idbuf)), numinst);

	if (numinst == 0)
	    return 0;

	/* allocate minimally-sized chunk of contiguous memory upfront */
	size = numinst * sizeof(pmLabelSet);
	if ((lp = (pmLabelSet *)malloc(size)) == NULL)
	    return -oserror();
	*lpp = lp;

	inst = PM_IN_NULL;
	if (ident != PM_INDOM_NULL) {
	    __pmdaStartInst(ident, pmda);
	    __pmdaNextInst(&inst, pmda);
	}

	count = 0;
	do {
	    if (count == numinst) {
		/* more instances than expected! */
		numinst++;
		size = numinst * sizeof(pmLabelSet);
		if ((rlp = (pmLabelSet *)realloc(*lpp, size)) == NULL)
		    return -oserror();
		*lpp = rlp;
		lp = rlp + count;
	    }
	    memset(lp, 0, sizeof(*lp));

	    if (pmda->e_labelCallBack != NULL) {
		if ((sts = (*(pmda->e_labelCallBack))(ident, inst, &lp)) < 0) {
		    pmInDomStr_r(ident, idbuf, sizeof(idbuf));
		    pmErrStr_r(sts, errbuf, sizeof(errbuf));
		    pmNotifyErr(LOG_DEBUG, "pmdaLabel: "
				    "InDom %s[%d]: %s\n", idbuf, inst, errbuf);
		}
	    }
	    if ((lp->nlabels = sts) > 0)
		pmdaAddLabelFlags(lp, type);
	    lp->inst = inst;
	    count++;
	    lp++;

	} while (ident != PM_INDOM_NULL && __pmdaNextInst(&inst, pmda));

	return count;

    default:
	break;
    }

    return PM_ERR_TYPE;
}

int
pmdaAddLabelFlags(pmLabelSet *lsp, int flags)
{
    int		i;

    if (lsp == NULL)
	return 0;
    for (i = 0; i < lsp->nlabels; i++)
	lsp->labels[i].flags |= flags;
    return 1;
}

/*
 * Add labels (name:value pairs) to a labelset, varargs style.
 */

int
pmdaAddLabels(pmLabelSet **lsp, const char *fmt, ...)
{
    char		errbuf[PM_MAXERRMSGLEN];
    char		buf[PM_MAXLABELJSONLEN];
    va_list		arg;
    int			sts;

    va_start(arg, fmt);
    sts = vsnprintf(buf, sizeof(buf), fmt, arg);
    va_end(arg);
    if (sts < 0)
	return sts;
    if (sts >= sizeof(buf))
	buf[sizeof(buf)-1] = '\0';

    if (pmDebugOptions.labels)
	fprintf(stderr, "pmdaAddLabels: %s\n", buf);

    if ((sts = __pmAddLabels(lsp, buf, 0)) < 0) {
	pmNotifyErr(LOG_ERR, "pmdaAddLabels: %s (%s)\n", buf,
		pmErrStr_r(sts, errbuf, sizeof(errbuf)));
    }
    return sts;
}

/*
 * Add notes (optional name:value pairs) to a labelset, varargs style.
 */

int
pmdaAddNotes(pmLabelSet **lsp, const char *fmt, ...)
{
    char		errbuf[PM_MAXERRMSGLEN];
    char		buf[PM_MAXLABELJSONLEN];
    va_list		arg;
    int			sts;

    va_start(arg, fmt);
    sts = vsnprintf(buf, sizeof(buf), fmt, arg);
    va_end(arg);
    if (sts < 0)
	return sts;
    if (sts >= sizeof(buf))
	buf[sizeof(buf)-1] = '\0';

    if (pmDebugOptions.labels)
	fprintf(stderr, "pmdaAddNotes: %s\n", buf);

    if ((sts = __pmAddLabels(lsp, buf, PM_LABEL_OPTIONAL)) < 0) {
	pmNotifyErr(LOG_ERR, "pmdaAddNotes: %s (%s)\n", buf,
		pmErrStr_r(sts, errbuf, sizeof(errbuf)));
    }
    return sts;
}

/*
 * Tell PMCD there is nothing to store
 */

int
pmdaStore(pmResult *result, pmdaExt *pmda)
{
    return PM_ERR_PERMISSION;
}

/*
 * Expect routines pmdaPMID(), pmdaName() and pmdaChildren() below
 * to be overridden with real routines for any PMDA that is
 * using PMDA_INTERFACE_4 or later and supporting dynamic metrics.
 *
 * Expect the pmdaAttribute() routine to be overridden with a real
 * routine for a PMDA using PMDA_INTERFACE_6 or later supporting
 * metrics whose behaviour depends on clients being authenticated.
 *
 * These implementations are stubs that return appropriate errors
 * if they are ever called.
 */

int
pmdaPMID(const char *name, pmID *pmid, pmdaExt *pmda)
{
    return PM_ERR_NAME;
}

int
pmdaName(pmID pmid, char ***nameset, pmdaExt *pmda)
{
    return PM_ERR_PMID;
}

int
pmdaChildren(const char *name, int traverse, char ***offspring, int **status, pmdaExt *pmda)
{
    return PM_ERR_NAME;
}

int
pmdaAttribute(int ctx, int attr, const char *value, int size, pmdaExt *pmda)
{
    if (pmDebugOptions.attr || pmDebugOptions.auth) {
	char buffer[256];
	if (!__pmAttrStr_r(attr, value, buffer, sizeof(buffer))) {
	    pmNotifyErr(LOG_ERR, "Bad attr: ctx=%d, attr=%d\n", ctx, attr);
	} else {
	    buffer[sizeof(buffer)-1] = '\0';
	    pmNotifyErr(LOG_INFO, "Attribute: ctx=%d %s", ctx, buffer);
	}
    }
    return 0;
}
