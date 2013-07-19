/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2013 Red Hat, Inc.
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
#include "impl.h"
#include <stddef.h>

void
__pmHashInit(__pmHashCtl *hcp)
{
    memset(hcp, 0, sizeof(*hcp));
}

__pmHashNode *
__pmHashSearch(unsigned int key, __pmHashCtl *hcp)
{
    __pmHashNode	*hp;

    if (hcp->hsize == 0)
	return NULL;

    for (hp = hcp->hash[key % hcp->hsize]; hp != NULL; hp = hp->next) {
	if (hp->key == key)
	    return hp;
    }
    return NULL;
}

int
__pmHashAdd(unsigned int key, void *data, __pmHashCtl *hcp)
{
    __pmHashNode    *hp;
    int		k;

    hcp->nodes++;

    if (hcp->hsize == 0) {
	hcp->hsize = 1;	/* arbitrary number */
	if ((hcp->hash = (__pmHashNode **)calloc(hcp->hsize, sizeof(__pmHashNode *))) == NULL) {
	    hcp->hsize = 0;
	    return -oserror();
	}
    }
    else if (hcp->nodes / 4 > hcp->hsize) {
	__pmHashNode	*tp;
	__pmHashNode	**old = hcp->hash;
	int		oldsize = hcp->hsize;

	hcp->hsize *= 2;
	if (hcp->hsize % 2) hcp->hsize++;
	if (hcp->hsize % 3) hcp->hsize += 2;
	if (hcp->hsize % 5) hcp->hsize += 2;
	if ((hcp->hash = (__pmHashNode **)calloc(hcp->hsize, sizeof(__pmHashNode *))) == NULL) {
	    hcp->hsize = oldsize;
	    hcp->hash = old;
	    return -oserror();
	}
	/*
	 * re-link chains
	 */
	while (oldsize) {
	    for (hp = old[--oldsize]; hp != NULL; ) {
		tp = hp;
		hp = hp->next;
		k = tp->key % hcp->hsize;
		tp->next = hcp->hash[k];
		hcp->hash[k] = tp;
	    }
	}
	free(old);
    }

    if ((hp = (__pmHashNode *)malloc(sizeof(__pmHashNode))) == NULL)
	return -oserror();

    k = key % hcp->hsize;
    hp->key = key;
    hp->data = data;
    hp->next = hcp->hash[k];
    hcp->hash[k] = hp;

    return 1;
}

int
__pmHashDel(unsigned int key, void *data, __pmHashCtl *hcp)
{
    __pmHashNode    *hp;
    __pmHashNode    *lhp = NULL;

    if (hcp->hsize == 0)
	return 0;

    for (hp = hcp->hash[key % hcp->hsize]; hp != NULL; hp = hp->next) {
	if (hp->key == key && hp->data == data) {
	    if (lhp == NULL)
		hcp->hash[key % hcp->hsize] = hp->next;
	    else
		lhp->next = hp->next;
	    free(hp);
	    return 1;
	}
	lhp = hp;
    }

    return 0;
}

void
__pmHashClear(__pmHashCtl *hcp)
{
    if (hcp->hsize != 0) {
	free(hcp->hash);
	hcp->hsize = 0;
    }
}

/*
 * Iterate over the entire hash table.  For each entry, call *cb,
 * passing *cdata and the current key/value pair.  The function's
 * return value decides how to continue or abort iteration.  The
 * callback function must not modify the hash table.
 */
void
__pmHashWalkCB(__pmHashWalkCallback cb, void *cdata, const __pmHashCtl *hcp)
{
    int n;

    for (n = 0; n < hcp->hsize; n++) {
        __pmHashNode *tp = hcp->hash[n];
        __pmHashNode **tpp = & hcp->hash[n];

        while (tp != NULL) {
            __pmHashWalkState state = (*cb)(tp, cdata);

            switch (state) {
            case PM_HASH_WALK_DELETE_STOP:
                *tpp = tp->next;  /* unlink */
                free(tp);         /* delete */
                return;           /* & stop */

            case PM_HASH_WALK_NEXT:
                tpp = &tp->next;
                tp = *tpp;
                break;

            case PM_HASH_WALK_DELETE_NEXT:
                *tpp = tp->next;  /* unlink */
                /* NB: do not change tpp.  It will still point at the previous
                 * node's "next" pointer.  Consider consecutive CONTINUE_DELETEs.
                 */
                free(tp);         /* delete */
                tp = *tpp; /* == tp->next, except that tp is already freed. */
                break;            /* & next */

            case PM_HASH_WALK_STOP:
            default:
                return;
            }
        }
    }
}

/*
 * Walk a hash table; state flow is START ... NEXT ... NEXT ...
 */
__pmHashNode *
__pmHashWalk(__pmHashCtl *hcp, __pmHashWalkState state)
{
    __pmHashNode	*node;

    if (hcp->hsize == 0)
	return NULL;

    if (state == PM_HASH_WALK_START) {
        hcp->index = 0;
        hcp->next = hcp->hash[0];
    }

    while (hcp->next == NULL) {
        hcp->index++;
        if (hcp->index >= hcp->hsize)
            return NULL;
        hcp->next = hcp->hash[hcp->index];
    }

    node = hcp->next;
    hcp->next = node->next;
    return node;
}
