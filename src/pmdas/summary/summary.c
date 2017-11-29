/*
 * Copyright (c) 1995,2003 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <ctype.h>
#include <signal.h>
#include "pmapi.h"
#include "libpcp.h"
#include "pmda.h"
#include "summary.h"
#include "domain.h"
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

int			nmeta;
meta_t			*meta;
pmResult 		*cachedResult;
static int		*freeList;

static int
summary_desc(pmID pmid, pmDesc *desc, pmdaExt * ex)
{
    static int		i=0;

    if ( i < nmeta && pmid == meta[i].desc.pmid) {
found:
	if (meta[i].desc.type == PM_TYPE_NOSUPPORT)
	    return PM_ERR_AGAIN; /* p76 rules ok */
	*desc = meta[i].desc;	/* struct assignment */
	return 0; /* success */
    }

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].desc.pmid)
	    goto found;
    }
    return PM_ERR_PMID;
}

void
service_client(__pmPDU *pb)
{
    int		n;
    int		i;
    int		j;
    pmDesc	desc;
    pmDesc	foundDesc;
    pmResult	*resp;
    pmValueSet	*vsp;
    __pmPDUHdr   *ph = (__pmPDUHdr *)pb;

    switch (ph->type) {

    case PDU_DESC:
	if ((n = __pmDecodeDesc(pb, &desc)) < 0) {
	    fprintf(stderr, "service_client: __pmDecodeDesc failed: %s\n",
                    pmErrStr(n));
	    exit(1);
	}

	if (desc.indom != PM_INDOM_NULL) {
	    fprintf(stderr, "service_client: Warning: ignored desc for pmid=%s: indom is not singular\n", pmIDStr(desc.pmid));
	    return;
	}

	if (summary_desc(desc.pmid, &foundDesc, NULL) == 0) {
	    /* already in table */
	    fprintf(stderr,
                    "service_client: Warning: duplicate desc for pmid=%s\n",
                    pmIDStr(desc.pmid));
	    return;
	}

	nmeta++;
	if ((meta = (meta_t *)realloc(meta, nmeta * sizeof(meta_t))) == NULL) {
	    pmNoMem("service_client: meta realloc", nmeta * sizeof(meta_t), PM_FATAL_ERR);
	}
	memcpy(&meta[nmeta-1].desc, &desc, sizeof(pmDesc));

	break;

    case PDU_RESULT:
	if ((n = __pmDecodeResult(pb, &resp)) < 0) {
	    fprintf(stderr, "service_client: __pmDecodeResult failed: %s\n", pmErrStr(n));
	    exit(1);
	}

	if (cachedResult == NULL) {
	    int		need;
	    need = (int)sizeof(pmResult) - (int)sizeof(pmValueSet *);
	    if ((cachedResult = (pmResult *)malloc(need)) == NULL) {
		pmNoMem("service_client: result malloc", need, PM_FATAL_ERR);
	    }
	    cachedResult->numpmid = 0;
	}

	/*
	 * swap values from resp with those in cachedResult, expanding
	 * cachedResult if there are metrics we've not seen before
	 */
	for (i = 0; i < resp->numpmid; i++) {
	    for (j = 0; j < cachedResult->numpmid; j++) {
		if (resp->vset[i]->pmid == cachedResult->vset[j]->pmid) {
		    /* found matching PMID, update this value */
		    break;
		}
	    }

	    if (j == cachedResult->numpmid) {
		/* new PMID, expand cachedResult and initialize vset */
		int		need;
		cachedResult->numpmid++;
		need = (int)sizeof(pmResult) +
		    (cachedResult->numpmid-1) * (int)sizeof(pmValueSet *);
		if ((cachedResult = (pmResult *)realloc(cachedResult, need)) == NULL) {
		    pmNoMem("service_client: result realloc", need, PM_FATAL_ERR);
		}
		if ((cachedResult->vset[j] = (pmValueSet *)malloc(sizeof(pmValueSet))) == NULL) {
		    pmNoMem("service_client: vset[]", sizeof(pmValueSet), PM_FATAL_ERR);
		}
		cachedResult->vset[j]->pmid = resp->vset[i]->pmid;
		cachedResult->vset[j]->numval = 0;
	    }

	    /*
	     * swap vsets
	     */
	    vsp = cachedResult->vset[j];
	    cachedResult->vset[j] = resp->vset[i];
	    resp->vset[i] = vsp;

	}

	pmFreeResult(resp);
	break;
	    
    case PDU_ERROR:
	if ((n = __pmDecodeError(pb, &i)) < 0) {
	    fprintf(stderr, "service_client: __pmDecodeError failed: %s\n", pmErrStr(n));
	    exit(1);
	}
	fprintf(stderr, "service_client: Error PDU! %s\n", pmErrStr(i));
	break;
    
    default:
	fprintf(stderr, "service_client: Bogus PDU type %d\n", ph->type);
	exit(1);
    }
}

static int
summary_profile(pmProfile *prof, pmdaExt * ex)
{
    /*
     * doesn't make sense since summary metrics 
     * always have a singular instance domain.
     */
    return 0;
}

static int
summary_instance(pmInDom indom, int inst, char *name, pmInResult **result,
                 pmdaExt * ex)
{
    return PM_ERR_INDOM;
}

static void
freeResultCallback(pmResult *res)
{
    int i;

    /*
     * pmResult has now been sent to pmcd. Only free the
     * value sets that had no values available because
     * the valid ones were reused from the cachedResult.
     */
    for (i=0; i < res->numpmid; i++) {
	if (freeList[i])
	    free(res->vset[i]);
    }
    return;
}


static int
summary_fetch(int numpmid, pmID pmidlist[], pmResult **resp, pmdaExt * ex)
{
    int			i;		/* over pmidlist[] */
    int			j;		/* over vset->vlist[] */
    int			sts;
    int			need;
    int			validpmid;
    pmID		pmid;
    pmDesc		desc;
    static pmResult	*res = NULL;
    static int		maxnpmids = 0;

    if (numpmid > maxnpmids) {
	maxnpmids = numpmid;
	if (res != NULL)
	    free(res);
	/* (numpmid - 1) because there's room for one valueSet in a pmResult */
	need = sizeof(pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
	if ((res = (pmResult *)malloc(need)) == NULL)
	    return -oserror();

	if (freeList != NULL)
	    free(freeList);
	if ((freeList = (int *)malloc(numpmid * sizeof(int))) == NULL)
	    return -oserror();
    }
    res->timestamp.tv_sec = 0;
    res->timestamp.tv_usec = 0;
    res->numpmid = numpmid;

    for (i = 0; i < numpmid; i++) {
	pmid = pmidlist[i];

	/*
	 * do we know about the descriptor for this pmid?
	 * If not, then the error is PM_ERR_PMID
	 * regardless of whether there is an entry in
	 * the cached result.
	 */
	sts = summary_desc(pmid, &desc, NULL);
	validpmid = (sts == 0);

	res->vset[i] = NULL;
	freeList[i] = 1;

	if (validpmid && cachedResult != NULL) {
	    for (j=0; j < cachedResult->numpmid; j++) {
		if (pmid == cachedResult->vset[j]->pmid) {
		    res->vset[i] = cachedResult->vset[j];
		    freeList[i] = 0;
		    break;
		}
	    }
	}

	if (!validpmid || res->vset[i] == NULL) {
	    /* no values available or the metric has no descriptor */
	    if ((res->vset[i] = (pmValueSet *)malloc(sizeof(pmValueSet))) == NULL)
		return -oserror();
	    res->vset[i]->pmid = pmid;
	    res->vset[i]->valfmt = PM_VAL_INSITU;
	    res->vset[i]->numval = validpmid ? PM_ERR_VALUE : sts;
	}
    }
    *resp = res;

    return numpmid;
}

static int
summary_store(pmResult *result, pmdaExt * ex)
{
    return PM_ERR_PERMISSION;
}

void
summary_init(pmdaInterface *dp)
{
    void (*callback)() = freeResultCallback;

    dp->version.two.profile = summary_profile;
    dp->version.two.fetch = summary_fetch;
    dp->version.two.desc = summary_desc;
    dp->version.two.instance = summary_instance;
    dp->version.two.store = summary_store;

    mainLoopFreeResultCallback(callback);

    pmdaInit(dp, NULL, 0, NULL, 0);
}

void
summary_done(void)
{
    int st;

    fprintf(stderr, "summary agent pid=%" FMT_PID " done\n", (pid_t)getpid());
    kill(clientPID, SIGINT);
    waitpid(clientPID, &st, 0);
}
