/*
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2015 Red Hat, Inc.
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
 * Thread-safe notes
 *
 * To avoid buffer trampling, on success __pmFindPDUBuf() now returns
 * a pinned PDU buffer.  It is the caller's responsibility to unpin the
 * PDU buffer when safe to do so.
 */

#include "pmapi.h"
#include "impl.h"
#include "compiler.h"
#include <assert.h>
#include <search.h>
#include <stdint.h>

typedef struct bufctl
{
    int		bc_pincnt;
    int		bc_size;
    char	*bc_buf;
    /* The actual buffer happens to follow this struct. */
} bufctl_t;

/* Protected by the pdubuf_lock mutex. */
static void *buf_tree;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	pdubuf_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*pdubuf_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == pdubuf_lock
 */
int
__pmIsPdubufLock(void *lock)
{
    return lock == (void *)&pdubuf_lock;
}
#endif

static void
pdubufdump1(const void *nodep, const VISIT which, const int depth)
{
    const bufctl_t	*pcp = *(bufctl_t **)nodep;

    if (which == postorder || which == leaf)	/* called once per node */
	fprintf(stderr, " " PRINTF_P_PFX "%p...%p[%d](%d)",
		pcp->bc_buf, &pcp->bc_buf[pcp->bc_size - 1], pcp->bc_size,
		pcp->bc_pincnt);
}

static void
pdubufdump(void)
{
    /*
     * There is no longer a pdubuf free list, ergo no
     * fprintf(stderr, "   free pdubuf[size]:\n");
     */
    PM_LOCK(pdubuf_lock);
    if (buf_tree != NULL) {
	fprintf(stderr, "   pinned pdubuf[size](pincnt):");
	/* THREADSAFE - no locks acquired in pdubufdump1() */
	twalk(buf_tree, &pdubufdump1);
	fprintf(stderr, "\n");
    }
    PM_UNLOCK(pdubuf_lock);
}

/*
 * A tsearch(3) comparison function for the buffer-segments used here.
 */
static int
bufctl_t_compare(const void *a, const void *b)
{
    const bufctl_t *aa = (const bufctl_t *)a;
    const bufctl_t *bb = (const bufctl_t *)b;

    /* NB: valid range is bc_buf[0 .. bc_size-1] */
    if ((uintptr_t)&aa->bc_buf[aa->bc_size-1] < (uintptr_t)&bb->bc_buf[0])
	return -1;
    if ((uintptr_t)&bb->bc_buf[bb->bc_size-1] < (uintptr_t)&aa->bc_buf[0])
	return 1;
    return 0;		/* overlap */
}

__pmPDU *
__pmFindPDUBuf(int need)
{
    bufctl_t	*pcp;
    void	*bcp;

    if (unlikely(need < 0)) {
	/* special diagnostic case ... dump buffer state */
	fprintf(stderr, "__pmFindPDUBuf(DEBUG)\n");
	pdubufdump();
	return NULL;
    }

    if ((pcp = (bufctl_t *)malloc(sizeof(*pcp) + need)) == NULL) {
	return NULL;
    }

    pcp->bc_pincnt = 1;
    pcp->bc_size = need;
    pcp->bc_buf = ((char *)pcp) + sizeof(*pcp);

    PM_LOCK(pdubuf_lock);
    /* Insert the node in the tree. */
    /* THREADSAFE - no locks acquired in bufctl_t_compare() */
    bcp = tsearch((void *)pcp, &buf_tree, &bufctl_t_compare);
    if (unlikely(bcp == NULL)) {	/* ENOMEM */
	PM_UNLOCK(pdubuf_lock);
	free(pcp);
	return NULL;
    }
    PM_UNLOCK(pdubuf_lock);

    if (unlikely(pmDebugOptions.pdubuf)) {
	fprintf(stderr, "__pmFindPDUBuf(%d) -> " PRINTF_P_PFX "%p\n",
		need, pcp->bc_buf);
	pdubufdump();
    }

    return (__pmPDU *)pcp->bc_buf;
}

void
__pmPinPDUBuf(void *handle)
{
    bufctl_t	*pcp, pcp_search;
    void	*bcp;

    assert(((__psint_t)handle % sizeof(int)) == 0);
    /*
     * Initialize a dummy bufctl_t to use only as search key;
     * only its bc_buf & bc_size fields need to be set, as that's
     * all that bufctl_t_compare will look at.
     */
    pcp_search.bc_buf = handle;
    pcp_search.bc_size = 1;

    PM_LOCK(pdubuf_lock);
    /* THREADSAFE - no locks acquired in bufctl_t_compare() */
    bcp = tfind(&pcp_search, &buf_tree, &bufctl_t_compare);
    /*
     * NB: don't release the lock until final disposition of this object;
     * we don't want to play TOCTOU.
     */
    if (likely(bcp != NULL)) {
	pcp = *(bufctl_t **)bcp;
	assert((&pcp->bc_buf[0] <= (char *)handle) &&
	       ((char *)handle < &pcp->bc_buf[pcp->bc_size]));
	pcp->bc_pincnt++;
    } else {
	PM_UNLOCK(pdubuf_lock);
	__pmNotifyErr(LOG_WARNING, "__pmPinPDUBuf: " PRINTF_P_PFX "%p not in pool!", handle);
	if (pmDebugOptions.pdubuf)
	    pdubufdump();
	return;
    }

    if (unlikely(pmDebugOptions.pdubuf))
	fprintf(stderr, "__pmPinPDUBuf(" PRINTF_P_PFX "%p) -> pdubuf="
			PRINTF_P_PFX "%p, pincnt=%d\n", handle,
		pcp->bc_buf, pcp->bc_pincnt);

    PM_UNLOCK(pdubuf_lock);
}

int
__pmUnpinPDUBuf(void *handle)
{
    bufctl_t	*pcp, pcp_search;
    void	*bcp;

    assert(((__psint_t)handle % sizeof(int)) == 0);
    PM_LOCK(pdubuf_lock);

    /*
     * Initialize a dummy bufctl_t to use only as search key;
     * only its bc_buf & bc_size fields need to be set, as that's
     * all that bufctl_t_compare will look at.
     */
    pcp_search.bc_buf = handle;
    pcp_search.bc_size = 1;

    /* THREADSAFE - no locks acquired in bufctl_t_compare() */
    bcp = tfind(&pcp_search, &buf_tree, &bufctl_t_compare);
    /*
     * NB: don't release the lock until final disposition of this object;
     * we don't want to play TOCTOU.
     */
    if (likely(bcp != NULL)) {
	pcp = *(bufctl_t **)bcp;
    } else {
	PM_UNLOCK(pdubuf_lock);
	if (pmDebugOptions.pdubuf) {
	    fprintf(stderr, "__pmUnpinPDUBuf(" PRINTF_P_PFX "%p) -> fails\n",
		    handle);
	    pdubufdump();
	}
	return 0;
    }

    if (unlikely(pmDebugOptions.pdubuf))
	fprintf(stderr, "__pmUnpinPDUBuf(" PRINTF_P_PFX "%p) -> pdubuf="
			PRINTF_P_PFX "%p, pincnt=%d\n", handle,
		pcp->bc_buf, pcp->bc_pincnt - 1);

    assert((&pcp->bc_buf[0] <= (char*)handle) &&
	   ((char*)handle < &pcp->bc_buf[pcp->bc_size]));

    if (likely(--pcp->bc_pincnt == 0)) {
	/* THREADSAFE - no locks acquired in bufctl_t_compare() */
	tdelete(pcp, &buf_tree, &bufctl_t_compare);
	PM_UNLOCK(pdubuf_lock);
	free(pcp);
    }
    else {
	PM_UNLOCK(pdubuf_lock);
    }

    return 1;
}

/*
 * Used to pass context from __pmCountPDUBuf to the pdubufcount callback.
 * They are protected by the pdubuf_lock mutex.
 */
static int	pdu_bufcnt_need;
static unsigned	pdu_bufcnt;

static void
pdubufcount(const void *nodep, const VISIT which, const int depth)
{
    const bufctl_t	*pcp = *(bufctl_t **)nodep;

    if (which == postorder || which == leaf)	/* called once per node */
	if (pcp->bc_size >= pdu_bufcnt_need)
	    pdu_bufcnt++;
}

void
__pmCountPDUBuf(int need, int *alloc, int *free)
{
    PM_LOCK(pdubuf_lock);

    pdu_bufcnt_need = need;
    pdu_bufcnt = 0;
    /* THREADSAFE - no locks acquired in pdubufcount() */
    twalk(buf_tree, &pdubufcount);
    *alloc = pdu_bufcnt;

    *free = 0;			/* We don't retain freed nodes. */

    PM_UNLOCK(pdubuf_lock);
}
