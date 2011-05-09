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

/*
 * Fast memory pool allocator for fixed-size blocks
 * Pools can only grow in size (no free or coallesing) ... fall-through
 * to malloc/free if not one of the sizes we support.
 */

#include "pmapi.h"
#include "impl.h"

typedef struct nurd {
    struct nurd	*next;
} nurd_t;

typedef struct {
    size_t	pc_size;
    void	*pc_head;
    int		alloc;
} poolctl_t;

/*
 * pool types, and their common occurrences ... see pmFreeResultValues()
 * for code that calls __pmPoolFree()
 *
 * [0] 20 bytes, pmValueSet when numval == 1
 * [1] 12 bytes, pmValueBlock for an 8 byte value
 */

static poolctl_t	poolctl[] = {
    { sizeof(pmValueSet),		NULL,	0 },
    { sizeof(int)+sizeof(__int64_t),	NULL,	0 }
};

static int	npool = sizeof(poolctl) / sizeof(poolctl[0]);

void *
__pmPoolAlloc(size_t size)
{
    int		i;
    void	*p;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    if (poolctl[i].pc_head == NULL) {
		poolctl[i].alloc++;
		p = malloc(size);
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDUBUF)
		    fprintf(stderr, "__pmPoolAlloc(%d) -> " PRINTF_P_PFX "%p [malloc, pool empty]\n",
			(int)size, p);
#endif
		return p;
	    }
	    else {
		nurd_t	*np;
		np = (nurd_t *)poolctl[i].pc_head;
		poolctl[i].pc_head = np->next;
#ifdef PCP_DEBUG
		if (pmDebug & DBG_TRACE_PDUBUF)
		    fprintf(stderr, "__pmPoolAlloc(%d) -> " PRINTF_P_PFX "%p\n",
			(int)size, np);
#endif
		return (void *)np;
	    }
	}
	i++;
    } while (i < npool);

    p = malloc(size);
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPoolAlloc(%d) -> " PRINTF_P_PFX "%p [malloc, not pool size]\n",
	    (int)size, p);
#endif

    return p;
}

/*
 * Note: size must be known so that block is returned to the correct pool.
 */
void
__pmPoolFree(void *ptr, size_t size)
{
    int		i;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    nurd_t	*np;
	    np = (nurd_t *)ptr;
	    np->next = poolctl[i].pc_head;
	    poolctl[i].pc_head = np;
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_PDUBUF)
		fprintf(stderr, "__pmPoolFree(" PRINTF_P_PFX "%p, %d)\n", ptr, (int)size);
#endif
	    return;
	}
	i++;
    } while (i < npool);

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_PDUBUF)
	fprintf(stderr, "__pmPoolFree(" PRINTF_P_PFX "%p, %d) [free, not pool size]\n",
	    ptr, (int)size);
#endif
    free(ptr);
    return;
}

void
__pmPoolCount(size_t size, int *alloc, int *free)
{
    int		i;

    i = 0;
    do {
	if (size == poolctl[i].pc_size) {
	    nurd_t	*np;
	    int		count = 0;
	    np = (nurd_t *)poolctl[i].pc_head;
	    while (np != NULL) {
		count++;
		np = np->next;
	    }
	    *alloc = poolctl[i].alloc;
	    *free = count;
	    return;
	}
	i++;
    } while (i < npool);

    /* not one of the sizes in the pool! */
    *alloc = *free = 0;
    return;
}
