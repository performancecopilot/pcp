/*
 * Copyright (c) 2014 Red Hat.
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
 * Routines to allocate and free assorted pmResult structs
 *
 * Threadsafe notes.
 *
 * - result_pool (head of list => all of the list) is guarded by the
 *   result_lock mutex
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

/*
 * Linked list of allocations made by __pmAllocResult(), but not
 * yet released by pmFreeResult().
 */
typedef struct result_pool_t {
    struct result_pool_t	*next;
    __pmResult			*rp;
} result_pool_t;

result_pool_t	*result_pool;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	result_lock;
#else
void			*result_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == result_lock
 */
int
__pmIsresultLock(void *lock)
{
    return lock == (void *)&result_lock;
}
#endif

void
init_result_lock(void)
{
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&result_lock);
#endif
}

/*
 * Allocate a __pmResult with enough space for numpmid metrics
 * ... return NULL on failure, and let caller decide what to do next
 */
__pmResult *
__pmAllocResult(int numpmid)
{
    size_t		need;
    result_pool_t	*new = (result_pool_t *)malloc(sizeof(*new));

    PM_INIT_LOCKS();

    if (new == NULL) {
	if (pmDebugOptions.alloc)
	    fprintf(stderr, "__pmAllocResult: new alloc failed\n");
	return NULL;
    }

    need = sizeof(__pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
    new->rp = (__pmResult *)malloc(need);
    if (new->rp == NULL) {
	if (pmDebugOptions.alloc)
	    fprintf(stderr, "__pmAllocResult: __pmResult %zd failed\n", need);
	free(new);
	return NULL;
    }

    PM_LOCK(result_lock);
    new->next = result_pool;
    result_pool = new;

    if (pmDebugOptions.alloc) {
	int		n = 0;
	result_pool_t	*pool;
	for (pool = result_pool; pool != NULL; pool = pool->next)
	    n++;
	fprintf(stderr, "__pmAllocResult ->" PRINTF_P_PFX "%p (%d in pool)\n", new->rp, n);
    }

    PM_UNLOCK(result_lock);

    return new->rp;
}

static void
__pmFreeResultValueSets(pmValueSet **ppvstart, pmValueSet **ppvsend)
{
    pmValueSet *pvs;
    pmValueSet **ppvs;
    char	strbuf[20];
    int		j;

    /* if _any_ vset[] -> an address within a pdubuf, we are done */
    for (ppvs = ppvstart; ppvs < ppvsend; ppvs++) {
	if (__pmUnpinPDUBuf((void *)*ppvs))
	    return;
    }

    /* not created from a pdubuf, really free the memory */
    for (ppvs = ppvstart; ppvs < ppvsend; ppvs++) {
	pvs = *ppvs;
	if (pvs->numval > 0 && pvs->valfmt == PM_VAL_DPTR) {
	    /* pmValueBlocks may be malloc'd as well */
	    for (j = 0; j < pvs->numval; j++) {
		if (pmDebugOptions.alloc)
		    fprintf(stderr, "free"
			"(" PRINTF_P_PFX "%p) pmValueBlock pmid=%s inst=%d\n",
			pvs->vlist[j].value.pval,
			pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)),
			pvs->vlist[j].inst);
		free(pvs->vlist[j].value.pval);
	    }
	}
	if (pmDebugOptions.alloc)
	    fprintf(stderr, "free(" PRINTF_P_PFX "%p) vset pmid=%s\n",
		pvs, pmIDStr_r(pvs->pmid, strbuf, sizeof(strbuf)));
	free(pvs);
    }
}

void
__pmFreeResultValues(pmResult *result)
{
    if (pmDebugOptions.alloc)
	fprintf(stderr, "__pmFreeResultValues(" PRINTF_P_PFX "%p) numpmid=%d\n",
	    result, result->numpmid);
    if (result->numpmid > 0)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
}

void
pmFreeResult(pmResult *result)
{
    result_pool_t	*pool;
    result_pool_t	*prior = NULL;

    PM_INIT_LOCKS();
    PM_LOCK(result_lock);

    if (result == NULL) {
	/*
	 * special debug callback .. or no-op w/out -Dalloc
	 */
	if (pmDebugOptions.alloc) {
	    int	n = 0;
	    for (pool = result_pool; pool != NULL; pool = pool->next) {
		fprintf(stderr, "__pmResult [%d] %p -> rp %p\n", n, pool, pool->rp);
		n++;
	    }
	    if (n == 0)
		fprintf(stderr, "__pmResult pool is empty\n");
	}
	PM_UNLOCK(result_lock);
	return;
    }

    if (pmDebugOptions.alloc) {
	int	n = 0;
	for (pool = result_pool; pool != NULL; pool = pool->next)
	    n++;
	fprintf(stderr, "pmFreeResult(" PRINTF_P_PFX "%p) (%d in pool)", result, n);
    }

    for (pool = result_pool; pool != NULL; pool = pool->next) {
	if (result == __pmOffsetResult(pool->rp)) {
	    if (pmDebugOptions.alloc)
		fprintf(stderr, " [in " PRINTF_P_PFX "%p]", pool->rp);
	    break;
	}
	prior = pool;
    }
    if (pmDebugOptions.alloc)
	fputc('\n', stderr);
    __pmFreeResultValues(result);
    if (pool != NULL) {
	if (prior == NULL)
	    result_pool = pool->next;
	else
	    prior->next = pool->next;
	free(pool->rp);
	free(pool);
    }
    else {
	free(result);
    }
    PM_UNLOCK(result_lock);
}

void
__pmFreeHighResResultValues(pmHighResResult *result)
{
    if (pmDebugOptions.alloc)
	fprintf(stderr, "__pmFreeHighResResultValues(" PRINTF_P_PFX "%p) numpmid=%d\n",
	    result, result->numpmid);
    if (result->numpmid > 0)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
}

void
__pmFreeHighResResult(pmHighResResult *result)
{
    result_pool_t	*pool;
    result_pool_t	*prior = NULL;

    PM_INIT_LOCKS();
    PM_LOCK(result_lock);

    if (pmDebugOptions.alloc)
	fprintf(stderr, "__pmFreeHighResResult(" PRINTF_P_PFX "%p)", result);

    for (pool = result_pool; pool != NULL; pool = pool->next) {
	if (result == __pmOffsetHighResResult(pool->rp)) {
	    if (pmDebugOptions.alloc)
		fprintf(stderr, " [in " PRINTF_P_PFX "%p]", pool->rp);
	    break;
	}
	prior = pool;
    }
    if (pmDebugOptions.alloc)
	fputc('\n', stderr);
    __pmFreeHighResResultValues(result);
    if (pool != NULL) {
	if (prior == NULL)
	    result_pool = pool->next;
	else
	    prior->next = pool->next;
	free(pool->rp);
	free(pool);
    }
    else {
	free(result);
    }
    PM_UNLOCK(result_lock);
}
