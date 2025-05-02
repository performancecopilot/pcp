/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2021-2022 Red Hat.
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
#include "libpcp.h"
#include "pmda.h"
#include "internal.h"

static pmID *splitlist;
static int splitmax;

static int
resize_splitlist(int numpmid)
{
    pmID	*tmp_list;
    size_t	need;

    splitmax = numpmid;
    need = sizeof(pmID) * splitmax;
    if ((tmp_list = (pmID *)realloc(splitlist, need)) == NULL) {
	free(splitlist);
	splitmax = 0;
	return -oserror();
    }
    splitlist = tmp_list;
    return 0;
}

static int
copyvset(const char *caller, pmID pmid, int sts,
		pmValueSet **tmpvset, int n, pmValueSet **ansvset, int k)
{
    if (sts < 0) {
	ansvset[k] = (pmValueSet *)malloc(sizeof(pmValueSet));
	if (ansvset[k] == NULL) {
	    /* cleanup all partial allocations for ansvset[] */
	    for (k--; k >=0; k--)
		free(ansvset[k]);
	    return -oserror();
	}
	ansvset[k]->numval = sts;
	ansvset[k]->pmid = pmid;
    }
    else {
	ansvset[k] = tmpvset[n];
    }

    if (pmDebugOptions.fetch) {
	char	strbuf[20];
	char	errmsg[PM_MAXERRMSGLEN];

	fprintf(stderr, "%s: [%d] PMID=%s nval=",
		caller, k, pmIDStr_r(pmid, strbuf, sizeof(strbuf)));
	if (ansvset[k]->numval < 0)
	    fprintf(stderr, "%s\n",
		    pmErrStr_r(ansvset[k]->numval, errmsg, sizeof(errmsg)));
	else
	    fprintf(stderr, "%d\n", ansvset[k]->numval);
    }

    return 0;
}

static int
dsofetch(const char *caller, __pmContext *ctxp, int ctx, int j,
		pmID pmidlist[], int numpmid, int *cntp, pmResult **resultp)
{
    int		sts = 0;
    int		cnt;
    int		k;
    __pmDSO	*dp;

    if ((dp = __pmLookupDSO(((__pmID_int *)&pmidlist[j])->domain)) == NULL) {
	/* based on domain, unknown PMDA */
	sts = PM_ERR_NOAGENT;
    } else {
	if (ctxp->c_sent == 0 || dp->ctx_last_prof != ctx) {
	    /*
	     * current profile for this context is _not_ already cached
	     * at the DSO PMDA, so send current profile ...
	     * Note: trickier than the non-local case, as no per-client
	     *	 caching at the PMCD end
	     */
	    if (pmDebugOptions.fetch)
		fprintf(stderr, 
			"%s: calling ???_profile(domain: %d), "
			    "context: %d\n", caller, dp->domain, ctx);
	    if (dp->dispatch.comm.pmda_interface >= PMDA_INTERFACE_5)
		dp->dispatch.version.four.ext->e_context = ctx;
	    sts = dp->dispatch.version.any.profile(ctxp->c_instprof,
						dp->dispatch.version.any.ext);
	    if (sts >= 0) {
		ctxp->c_sent = 1;
		dp->ctx_last_prof = ctx;
	    }
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
	sts = dp->dispatch.version.any.fetch(cnt, splitlist, resultp,
					dp->dispatch.version.any.ext);
    }

    *cntp = cnt;

    return sts;
}

/*
 * Called with valid context locked ...
 */

int
__pmFetchLocal(__pmContext *ctxp, int numpmid, pmID pmidlist[], __pmResult **result)
{
    int		sts;
    int		ctx;
    int		j;
    int		k;
    int		n;
    pmResult	*tmp_ans;	/* maintains timeless PMDA fetch interface */
    __pmResult	*ans;

    if (PM_MULTIPLE_THREADS(PM_SCOPE_DSO_PMDA))
	/* Local context requires single-threaded applications */
	return PM_ERR_THREAD;
    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    ctx = __pmPtrToHandle(ctxp);

    /*
     * Check if we have enough space to accomodate "best" case scenario -
     * all pmids are from the same domain
     */
    if (splitmax < numpmid &&
	(sts = resize_splitlist(numpmid)) < 0)
	return sts;

    /*
     * There's two issues being addressed here:
     * - the DSOs have a high-water mark allocation algorithm
     *   for the result skeleton, but the code that calls us
     *   assumes it has freedom to retain this result structure
     *   for as long as it wishes, and then to call one of the
     *   pmFreeResult variants.
     * - the PMDA fetch API requires a pmResult but we may be
     *   using either timespec of timeval resolutions (i.e. in
     *   highres sampling mode, or traditional mode).
     * 
     * So we make a __pmResult, selectively copy and return it.
     */
    if ((ans = __pmAllocResult(numpmid)) == NULL)
	return -oserror();

    /* mark all metrics as not picked, yet */
    for (j = 0; j < numpmid; j++)
	ans->vset[j] = NULL;

    ans->numpmid = numpmid;
    __pmGetTimestamp(&ans->timestamp);

    for (j = 0; j < numpmid; j++) {
	int cnt, res;

	if (ans->vset[j] != NULL)
	    /* picked up in a previous fetch */
	    continue;

	res = dsofetch("__pmFetchLocal", ctxp, ctx,
			j, pmidlist, numpmid, &cnt, &tmp_ans);

	/* Copy results back
	 *
	 * Note: We DO NOT have to free tmp_ans since DSO PMDA would
	 *		ALWAYS return a pointer to the static area.
	 */
	for (n = 0, k = j; k < numpmid && n < cnt; k++) {
	    if (pmidlist[k] == splitlist[n]) {
		sts = copyvset("__pmFetchLocal", splitlist[n], res,
				tmp_ans->vset, n, ans->vset, k);
		if (sts < 0) {
		    free(ans);
		    return sts;
		}
		n++;
	    }
	}
    }
    *result = ans;
    return 0;
}
