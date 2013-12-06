/*
 * hash.c - hash table used by trace pmda and libpcp_trace
 *
 * Copyright (c) 1997-2000 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "hash.h"


static unsigned int
hashindex(const char *key, size_t tablesize)
{
    unsigned int	hash, i;
    char		*p = (char *)key;

    /* use first few chars for hash table distribution */
    for (i=0, hash=0; i < 5 && *p; i++)
	hash = (hash << PM_HASH_SHFT) - hash + *p++;

#ifdef DESPERATE
    fprintf(stderr, "Generated hash number: %d (%s)\n", hash % tablesize, key);
#endif
    return hash % (unsigned int)tablesize;
}


int
__pmhashinit(__pmHashTable *t, size_t tsize, size_t esize,
				__pmHashCmpFunc cmp, __pmHashDelFunc del)
{
    size_t	blocksize;

    t->tsize = tsize;
    t->esize = esize;
    t->entries = 0;
    t->cmp = cmp;
    t->del = del;
    if (t->tsize <= 0)	/* use default */
	t->tsize = PM_HASH_SIZE;
    blocksize = sizeof(__pmHashEnt *) * t->tsize;
    if ((t->rows = (__pmHashEnt **)malloc(blocksize)) == NULL)
	return -oserror();
    memset((void *)t->rows, 0, blocksize);
    return 0;
}


static int
hashalloc(__pmHashTable *t, __pmHashEnt **entry)
{
    int e;

    if ((*entry = (__pmHashEnt *)malloc(sizeof(__pmHashEnt))) == NULL)
	return -oserror();
    if (((*entry)->ent = malloc(t->esize)) == NULL) {
	e = -oserror();
	free(*entry);
	*entry = NULL;
	return e;
    }
    (*entry)->next = NULL;
    return 0;
}


void
__pmhashtrunc(__pmHashTable *t)
{
    if (t == NULL || t->rows == NULL || t->entries <= 0)
	return;
    else {
	__pmHashEnt	*tmp, *e;
	int		i;

	for (i = 0; i < PM_HASH_SIZE; i++) {
	    e = t->rows[i];
	    while (e != NULL) {
		tmp = e;
		e = e->next;
		if (tmp->ent != NULL) {
		    t->del(tmp->ent);
		    tmp->ent = NULL;
		}
		if (tmp) {
		    free(tmp);
		    tmp = NULL;
		}
	    }
	    t->rows[i] = NULL;
	}
	memset((void *)t->rows, 0, sizeof(__pmHashEnt *)*t->tsize);
	t->entries = 0;
    }
}


void *
__pmhashlookup(__pmHashTable *t, const char *key, void *result)
{
    __pmHashEnt	*e;
    int		index;

    if (t->entries == 0)
	return NULL;

    index = hashindex(key, t->tsize);
    e = t->rows[index];
    while (e != NULL) {
	if (t->cmp(e->ent, result))
	   break;
	e = e->next;
    }
    if (e == NULL)
	return NULL;
    return e->ent;
}


int
__pmhashinsert(__pmHashTable *t, const char *key, void *entry)
{
    __pmHashEnt	*hash = NULL;
    int		index, sts;

    if ((sts = hashalloc(t, &hash)) < 0)
	return sts;

    index = hashindex(key, t->tsize);

    /* stick at head of list (locality of reference) */
    memcpy(hash->ent, entry, t->esize);
    if (t->rows[index]) {
	hash->next = t->rows[index]->next;
	t->rows[index]->next = hash;
    }
    else {
	t->rows[index] = hash;
	t->rows[index]->next = NULL;
    }
    t->entries++;
#ifdef DESPERATE
    fprintf(stderr, "Insert: %s at 0x%x (key=%d entry=%d)\n",
	    key, &t->rows[index], index, t->entries);
#endif
    return 0;
}


void
__pmhashtraverse(__pmHashTable *t, __pmHashIterFunc func)
{
    __pmHashEnt		*e = NULL;
    unsigned int	i, hits;

    if (t == NULL || func == NULL)
	return;

    for (i = 0, hits = 0; i < PM_HASH_SIZE && hits < t->entries; i++) {
	e = t->rows[i];
	while (e != NULL && hits < t->entries) {
	    hits++;
	    if (e->ent != NULL)
		func(t, e->ent);
	    e = e->next;
	}
    }
#ifdef DESPERATE
    fprintf(stderr, "Traverse: looped %d times\n", i);
#endif
}
