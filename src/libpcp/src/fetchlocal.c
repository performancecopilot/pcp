/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <stdio.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "internal.h"

/*
 * Called with valid context locked ...
 */
int
__pmFetchLocal(__pmContext *ctxp, int numpmid, pmID pmidlist[], pmResult **result)
{
    int		sts;
    int		ctx;
    int		j;
    int		k;
    int		n;
    pmResult	*ans;
    pmResult	*tmp_ans;
    __pmDSO	*dp;
    int		need;

    static pmID * splitlist=NULL;
    static int	splitmax=0;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	/* Local context requires single-threaded applications */
	return PM_ERR_THREAD;
    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    ctx = __pmPtrToHandle(ctxp);

    /*
     * this is very ugly ... the DSOs have a high-water mark
     * allocation algorithm for the result skeleton, but the
     * code that calls us assumes it has freedom to retain
     * this result structure for as long as it wishes, and
     * then to call pmFreeResult
     *
     * we make another skeleton, selectively copy and return that
     *
     * (numpmid - 1) because there's room for one valueSet
     * in a pmResult
     */
    need = (int)sizeof(pmResult) + (numpmid - 1) * (int)sizeof(pmValueSet *);
    if ((ans = (pmResult *)malloc(need)) == NULL)
	return -oserror();

    /*
     * Check if we have enough space to accomodate "best" case scenario -
     * all pmids are from the same domain
     */
    if (splitmax < numpmid) {
	splitmax = numpmid;
	pmID *tmp_list = (pmID *)realloc(splitlist, sizeof(pmID)*splitmax);
	if (tmp_list == NULL) {
	    free(splitlist);
	    splitmax = 0;
	    free(ans);
	    return -oserror();
	}
	splitlist = tmp_list;
    }

    ans->numpmid = numpmid;
    __pmtimevalNow(&ans->timestamp);
    for (j = 0; j < numpmid; j++)
	ans->vset[j] = NULL;

    for (j = 0; j < numpmid; j++) {
	int cnt;

	if (ans->vset[j] != NULL)
	    /* picked up in a previous fetch */
	    continue;

	sts = 0;
	if ((dp = __pmLookupDSO(((__pmID_int *)&pmidlist[j])->domain)) == NULL)
	    /* based on domain, unknown PMDA */
	    sts = PM_ERR_NOAGENT;
	else {
	    if (ctxp->c_sent != dp->domain) {
		/*
		 * current profile is _not_ already cached at other end of
		 * IPC, so send get current profile ...
		 * Note: trickier than the non-local case, as no per-PMDA
		 *	 caching at the PMCD end, so need to remember the
		 *	 last domain to receive a profile
		 */
		if (pmDebugOptions.fetch)
		    fprintf(stderr, 
			    "__pmFetchLocal: calling ???_profile(domain: %d), "
			    "context: %d\n", dp->domain, ctx);
		if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		    dp->dispatch.version.four.ext->e_context = ctx;
		sts = dp->dispatch.version.any.profile(ctxp->c_instprof,
						dp->dispatch.version.any.ext);
		if (sts >= 0)
		    ctxp->c_sent = dp->domain;
	    }
	}

	/* Copy all pmID for the current domain into the temp. list */
	for (cnt=0, k=j; k < numpmid; k++ ) {
	    if (((__pmID_int*)(pmidlist+k))->domain ==
		((__pmID_int*)(pmidlist+j))->domain)
		splitlist[cnt++] = pmidlist[k];
	}

	if (sts >= 0) {
	    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		dp->dispatch.version.four.ext->e_context = ctx;
	    sts = dp->dispatch.version.any.fetch(cnt, splitlist, &tmp_ans,
						dp->dispatch.version.any.ext);
	}

	/* Copy results back
	 *
	 * Note: We DO NOT have to free tmp_ans since DSO PMDA would
	 *		ALWAYS return a pointer to the static area.
	 */
	for (n = 0, k = j; k < numpmid && n < cnt; k++) {
	    if (pmidlist[k] == splitlist[n]) {
		if (sts < 0) {
		    ans->vset[k] = (pmValueSet *)malloc(sizeof(pmValueSet));
		    if (ans->vset[k] == NULL) {
			/* cleanup all partial allocations for ans->vset[] */
			for (k--; k >=0; k--)
			    free(ans->vset[k]);
			free(ans);
			return -oserror();
		    }
		    ans->vset[k]->numval = sts;
		    ans->vset[k]->pmid = pmidlist[k];
		}
		else {
		    ans->vset[k] = tmp_ans->vset[n];
		}
		if (pmDebugOptions.fetch) {
		    char	strbuf[20];
		    char	errmsg[PM_MAXERRMSGLEN];
		    fprintf(stderr, "__pmFetchLocal: [%d] PMID=%s nval=",
			    k, pmIDStr_r(pmidlist[k], strbuf, sizeof(strbuf)));
		    if (ans->vset[k]->numval < 0)
			fprintf(stderr, "%s\n",
				pmErrStr_r(ans->vset[k]->numval, errmsg, sizeof(errmsg)));
		    else
			fprintf(stderr, "%d\n", ans->vset[k]->numval);
		}
		n++;
	    }
	}
    }
    *result = ans;

    return 0;
}
