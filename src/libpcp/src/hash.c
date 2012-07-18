/*
 * Copyright (c) 1995-2002 Silicon Graphics, Inc.  All Rights Reserved.
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
