/*
 * Copyright (c) 2014,2022 Red Hat.
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
#include "fault.h"

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
    result_pool_t	*new;

    /*
     * set oserror() in case we take the fault return
     * ... for normal use, the first successful malloc() will clear this
     */
    setoserror(ENOMEM);	

PM_FAULT_RETURN(NULL);

    PM_INIT_LOCKS();

    new = (result_pool_t *)malloc(sizeof(*new));
    if (new == NULL) {
	if (pmDebugOptions.alloc)
	    fprintf(stderr, "__pmAllocResult: new alloc failed\n");
	return NULL;
    }

    if (numpmid < 1)
	numpmid = 1;
    need = sizeof(__pmResult) + (numpmid - 1) * sizeof(pmValueSet *);
    new->rp = (__pmResult *)malloc(need);
    if (new->rp == NULL) {
	if (pmDebugOptions.alloc)
	    fprintf(stderr, "__pmAllocResult: __pmResult %zu failed\n", need);
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

/*
 * special debug callback (used via null result) ... no-op w/out -Dalloc
 */
static void
__pmDumpResultPool(void)
{
    if (pmDebugOptions.alloc) {
	result_pool_t	*pool;
	size_t		n = 0;

	for (pool = result_pool; pool != NULL; pool = pool->next) {
	    fprintf(stderr, "__pmResult [%zu] %p -> rp %p\n", n, pool, pool->rp);
	    n++;
	}
	if (n == 0)
	    fprintf(stderr, "__pmResult pool is empty\n");
    }
}

static size_t
__pmSizeResultPool(void)
{
    result_pool_t	*pool;
    size_t		size = 0;

    for (pool = result_pool; pool != NULL; pool = pool->next)
	size++;
    return size;
}

static void
__pmFreeResultFromPool(result_pool_t *pool, result_pool_t *prior)
{
    if (prior == NULL)
	result_pool = pool->next;
    else
	prior->next = pool->next;
    free(pool->rp);
    free(pool);
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
__pmFreeResult(__pmResult *result)
{
    result_pool_t	*pool, *prior = NULL;

    PM_INIT_LOCKS();
    PM_LOCK(result_lock);

    if (result == NULL) {
	__pmDumpResultPool();
	PM_UNLOCK(result_lock);
	return;
    }

    if (pmDebugOptions.alloc)
	fprintf(stderr, "%s(" PRINTF_P_PFX "%p) (%zu in pool)",
			"__pmFreeResult", result, __pmSizeResultPool());

    for (pool = result_pool; pool != NULL; pool = pool->next) {
	if (result == pool->rp) {
	    if (pmDebugOptions.alloc)
		fprintf(stderr, " [in " PRINTF_P_PFX "%p]", pool->rp);
	    break;
	}
	prior = pool;
    }
    if (pmDebugOptions.alloc)
	fputc('\n', stderr);
    if (result->numpmid > 0)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
    if (pool != NULL)
	__pmFreeResultFromPool(pool, prior);
    /* else on-stack */
    PM_UNLOCK(result_lock);
}

void
__pmFreeResultValues(pmResult *result)
{
    if (pmDebugOptions.alloc)
	fprintf(stderr, "%s(" PRINTF_P_PFX "%p) numpmid=%d\n",
		"__pmFreeResultValues", result, result->numpmid);
    if (result->numpmid > 0)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
}

void
__pmFreeHighResResultValues(pmHighResResult *result)
{
    if (pmDebugOptions.alloc)
	fprintf(stderr, "%s(" PRINTF_P_PFX "%p) numpmid=%d\n",
		"__pmFreeHighResResultValues", result, result->numpmid);
    if (result->numpmid > 0)
	__pmFreeResultValueSets(result->vset, &result->vset[result->numpmid]);
}

void
pmFreeResult(pmResult *result)
{
    result_pool_t	*pool, *prior = NULL;

    PM_INIT_LOCKS();
    PM_LOCK(result_lock);

    if (result == NULL) {
	__pmDumpResultPool();
	PM_UNLOCK(result_lock);
	return;
    }

    if (pmDebugOptions.alloc)
	fprintf(stderr, "%s(" PRINTF_P_PFX "%p) (%zu in pool)",
			"pmFreeResult", result, __pmSizeResultPool());

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
    if (pool != NULL)
	__pmFreeResultFromPool(pool, prior);
    else
	free(result);	/* not allocated by __pmAllocResult */
    PM_UNLOCK(result_lock);
}

void
__pmFreeHighResResult(pmHighResResult *result)
{
    result_pool_t	*pool, *prior = NULL;

    PM_INIT_LOCKS();
    PM_LOCK(result_lock);

    if (result == NULL) {
	__pmDumpResultPool();
	PM_UNLOCK(result_lock);
	return;
    }

    if (pmDebugOptions.alloc)
	fprintf(stderr, "%s(" PRINTF_P_PFX "%p) (%zu in pool)",
			"pmFreeHighResResult", result, __pmSizeResultPool());

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
    if (pool != NULL)
	__pmFreeResultFromPool(pool, prior);
    else
	free(result);	/* not allocated by __pmAllocResult */
    PM_UNLOCK(result_lock);
}

void
pmFreeHighResResult(pmHighResResult *result)
{
    return __pmFreeHighResResult(result);
}
