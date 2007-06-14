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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#ident "$Id: fetchlocal.c,v 1.2 2004/06/07 10:17:19 nathans Exp $"

#include <stdio.h>
#include <sys/time.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

extern int	errno;

int
__pmFetchLocal(int numpmid, pmID pmidlist[], pmResult **result)
{
    int		sts;
    int		ctx;
    int		j;
    int		k;
    __pmContext	*ctxp;
    pmResult	*ans;
    pmResult	*tmp_ans;
    __pmDSO	*dp;
    int		need;

    static pmID * splitlist=NULL;
    static int	splitmax=0;

    if (numpmid < 1)
	return PM_ERR_TOOSMALL;

    if ((ctx = pmWhichContext()) < 0)
	return ctx;
    ctxp = __pmHandleToPtr(ctx);

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
	return -errno;

    /*
     * Check if we have enough space to accomodate "best" case scenario -
     * all pmids are from the same domain
     */
    if ( splitmax < numpmid ) {
	splitmax = numpmid;
	if ((splitlist = (pmID *)realloc (splitlist,
					  sizeof (pmID)*splitmax)) == NULL) {
	    splitmax = 0;
	    return -errno;
	}
    }
		
    ans->numpmid = numpmid;
    gettimeofday(&ans->timestamp, NULL);
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
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_FETCH)
		    fprintf(stderr, 
			    "__pmFetchLocal: calling ???_profile(domain: %d), "
			    "context: %d\n", dp->domain, ctx);
#endif
		if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		    sts = dp->dispatch.version.one.profile(ctxp->c_instprof);
		else
		    sts = dp->dispatch.version.two.profile(ctxp->c_instprof,
							   dp->dispatch.version.two.ext);
		if (sts >= 0)
		    ctxp->c_sent = dp->domain;
	    }

	}

	/* Copy all pmID for the current domain into the temp. list */
	for (cnt=0, k=j; k < numpmid; k++ ) {
	    if (((__pmID_int*)(pmidlist+k))->domain == ((__pmID_int*)(pmidlist+j))->domain) {
		splitlist[cnt++] = pmidlist[k];
	    }
	}

	if (sts >= 0) {
	    if (dp->dispatch.comm.pmda_interface == PMDA_INTERFACE_1)
		sts = dp->dispatch.version.one.fetch(cnt, splitlist, &tmp_ans);
	    else
		sts = dp->dispatch.version.two.fetch(cnt, splitlist, &tmp_ans,
						     dp->dispatch.version.two.ext);
	}

	/* Copy results back
	 *
	 * Note: We DO NOT have to free tmp_ans since DSO PMDA would
	 *		ALWAYS return a pointer to the static area.
	 */
	for (cnt=0, k = j; k < numpmid; k++) {
	    if ( pmidlist[k] == splitlist[cnt] ) {
		if (sts < 0) {
		    ans->vset[k] = (pmValueSet *)malloc(sizeof(pmValueSet));
		    if (ans->vset[k] == NULL)
			return -errno;
		    ans->vset[k]->numval = sts;
		}
		else {
		    ans->vset[k] = tmp_ans->vset[cnt];
		}
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_FETCH) {
		    fprintf(stderr, "__pmFetchLocal: [%d] PMID=%s nval=",
			    k, pmIDStr(pmidlist[k]));
		    if (ans->vset[k]->numval < 0)
			fprintf(stderr, "%s\n",
				pmErrStr(ans->vset[k]->numval));
		    else
			fprintf(stderr, "%d\n", ans->vset[k]->numval);
		}
#endif
		cnt++;
	    }
	}
    }
    *result = ans;

    return 0;
}
