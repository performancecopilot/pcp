/*
 * Copyright (c) 2014 Red Hat.
 * Copyright (c) 2009-2010 Aconex.  All Rights Reserved.
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
#include "pmda.h"

#define NONLEAF(node)	((node)->pmid == PM_ID_NULL)

/*
 * Fixup the parent pointers of the tree.
 * Fill in the hash table with nodes from the tree.
 * Hashing is done on pmid.
 */
static void
__pmdaTreeReindexHash(__pmnsTree *tree, __pmnsNode *root)
{
    __pmnsNode	*np;

    for (np = root->first; np != NULL; np = np->next) {
	np->parent = root;
	if (np->pmid != PM_ID_NULL) {
	    int i = np->pmid % tree->htabsize;
	    np->hash = tree->htab[i];
	    tree->htab[i] = np;
	}
	__pmdaTreeReindexHash(tree, np);
    }
}

/*
 * "Make the average hash list no longer than 5, and the number
 * of hash table entries not a multiple of 2, 3 or 5."
 * [From __pmFixPMNSHashTab; without mark_all, dinks with pmids]
 */
void
pmdaTreeRebuildHash(__pmnsTree *tree, int numpmid)
{
    if (tree) {
	int htabsize = numpmid / 5;

	if (htabsize % 2 == 0) htabsize++;
	if (htabsize % 3 == 0) htabsize += 2;
	if (htabsize % 5 == 0) htabsize += 2;
	tree->htabsize = htabsize;
	tree->htab = (__pmnsNode **)calloc(htabsize, sizeof(__pmnsNode *));
	if (tree->htab) {
	    __pmdaTreeReindexHash(tree, tree->root);
	} else {
	    __pmNoMem("pmdaTreeRebuildHash",
			htabsize * sizeof(__pmnsNode *), PM_RECOV_ERR);
	    tree->htabsize = 0;
	}
    }
}

static int
__pmdaNodeCount(__pmnsNode *parent)
{
    __pmnsNode *np;
    int count;

    count = 0;
    for (np = parent->first; np != NULL; np = np->next) {
	if (np->pmid != PM_ID_NULL)
	    count++;
	else
	    count += __pmdaNodeCount(np);
    }
    return count;
}

int
pmdaTreeSize(__pmnsTree *pmns)
{
    if (pmns && pmns->root)
	return __pmdaNodeCount(pmns->root);
    return 0;
}

__pmnsNode *
pmdaNodeLookup(__pmnsNode *node, const char *name)
{
    while (node != NULL) {
	size_t length = strlen(node->name);
	if (strncmp(name, node->name, length) == 0) {
	    if (name[length] == '\0')
		return node;
	    if (name[length] == '.' && NONLEAF(node))
		return pmdaNodeLookup(node->first, name + length + 1);
	}
	node = node->next;
    }
    return NULL;
}

int
pmdaTreePMID(__pmnsTree *pmns, const char *name, pmID *pmid)
{
    if (pmns && pmns->root) {
	__pmnsNode *node;

	if ((node = pmdaNodeLookup(pmns->root->first, name)) == NULL)
	    return PM_ERR_NAME;
	if (NONLEAF(node))
	    return PM_ERR_NAME;
	*pmid = node->pmid;
	return 0;
    }
    return PM_ERR_NAME;
}

static char *
__pmdaNodeAbsoluteName(__pmnsNode *node, char *buffer)
{
    if (node && node->parent) {
	buffer = __pmdaNodeAbsoluteName(node->parent, buffer);
	strcpy(buffer, node->name);
	buffer += strlen(node->name);
	*buffer++ = '.';
    }
    return buffer;
}

int
pmdaTreeName(__pmnsTree *pmns, pmID pmid, char ***nameset)
{
    __pmnsNode *hashchain, *node, *parent;
    int nmatch = 0, length = 0;
    char *p, **list;

    if (!pmns)
	return PM_ERR_PMID;

    hashchain = pmns->htab[pmid % pmns->htabsize];
    for (node = hashchain; node != NULL; node = node->hash) {
	if (node->pmid == pmid) {
	    for (parent = node; parent->parent; parent = parent->parent)
		length += strlen(parent->name) + 1;
	    nmatch++;
	}
    }

    if (nmatch == 0)
	return PM_ERR_PMID;

    length += nmatch * sizeof(char *);		/* pointers to names */

    if ((list = (char **)malloc(length)) == NULL)
	return -oserror();

    p = (char *)&list[nmatch];
    nmatch = 0;
    for (node = hashchain; node != NULL; node = node->hash) {
	if (node->pmid == pmid) {
	    list[nmatch++] = p;
	    p = __pmdaNodeAbsoluteName(node, p);
	    *(p-1) = '\0';	/* overwrite final '.' */
	}
    }

    *nameset = list;
    return nmatch;
}

static int
__pmdaNodeRelativeChildren(__pmnsNode *base, char ***offspring, int **status)
{
    __pmnsNode *node;
    char **list, *p;
    int *leaf, length = 0, nmatch = 0;
 
    for (node = base; node != NULL; node = node->next, nmatch++)
	length += strlen(node->name) + 1;
    if (nmatch == 0) {
	/*
	 * no need to allocate zero sized arrays for offspring[]
	 * and status[]
	 */
	return 0;
    }
    length += nmatch * sizeof(char *);	/* pointers to names */
    if ((list = (char **)malloc(length)) == NULL)
	return -oserror();
    if ((leaf = (int *)malloc(nmatch * sizeof(int))) == NULL) {
	free(list);
	return -oserror();
    }
    p = (char *)&list[nmatch];
    nmatch = 0;
    for (node = base; node != NULL; node = node->next, nmatch++) {
	leaf[nmatch] = NONLEAF(node) ? PMNS_NONLEAF_STATUS : PMNS_LEAF_STATUS;
	list[nmatch] = p;
	strcpy(p, node->name);
	p += strlen(node->name);
	*p++ = '\0';
    }

    *offspring = list;
    *status = leaf;
    return nmatch;
}

static void
__pmdaNodeChildrenGetSize(__pmnsNode *base, int kids, int *length, int *nmetrics)
{
    __pmnsNode *node, *parent;

    /* walk to every leaf & then add its (absolute name) length */
    for (node = base; node != NULL; node = node->next) {
	if (NONLEAF(node)) {
	    __pmdaNodeChildrenGetSize(node->first, 1, length, nmetrics);
	    continue;
	}
	for (parent = node; parent->parent; parent = parent->parent)
	    *length += strlen(parent->name) + 1;
	(*nmetrics)++;
	if (!kids)
	    break;
    }
}

/*
 * Fill the pmdaChildren buffers - names and leaf status.  Called recursively
 * to descend down to all leaf nodes.  Offset parameter is the current offset
 * into the name list buffer, and its also returned at the end of each call -
 * it keeps track of where the next name is to start in (list) output buffer.
 */
static char *
__pmdaNodeChildrenGetList(__pmnsNode *base, int kids, int *nmetrics, char *p, char **list, int *leaf)
{
    __pmnsNode *node;
    int count = *nmetrics;
    char *start = p;

    for (node = base; node != NULL; node = node->next) {
	if (NONLEAF(node)) {
	    p = __pmdaNodeChildrenGetList(node->first, 1, &count, p, list, leaf);
	    start = p;
	    continue;
	}
	leaf[count] = PMNS_LEAF_STATUS;
	list[count] = start;
	p = __pmdaNodeAbsoluteName(node, p);
	*(p-1) = '\0';	/* overwrite final '.' */
	start = p;
	count++;
	if (!kids)
	    break;
    }
    *nmetrics = count;
    return p;
}

static int
__pmdaNodeAbsoluteChildren(__pmnsNode *node, char ***offspring, int **status)
{
    char *p, **list;
    int *leaf, descend = 0, length = 0, nmetrics = 0;

    if (NONLEAF(node)) {
	node = node->first;
	descend = 1;
    }
    __pmdaNodeChildrenGetSize(node, descend, &length, &nmetrics);
 
    length += nmetrics * sizeof(char *);	/* pointers to names */
    if ((list = (char **)malloc(length)) == NULL)
	return -oserror();
    if ((leaf = (int *)malloc(nmetrics * sizeof(int))) == NULL) {
	free(list);
	return -oserror();
    }

    p = (char *)&list[nmetrics];
    nmetrics = 0;	/* start at the start */
    __pmdaNodeChildrenGetList(node, descend, &nmetrics, p, list, leaf);

    *offspring = list;
    *status = leaf;
    return nmetrics;
}

int
pmdaTreeChildren(__pmnsTree *pmns, const char *name, int traverse, char ***offspring, int **status)
{
    __pmnsNode	*node;
    int		sts;

    if (!pmns)
	return PM_ERR_NAME;

    if ((node = pmdaNodeLookup(pmns->root->first, name)) == NULL)
	return PM_ERR_NAME;

    if (traverse == 0)
	sts = __pmdaNodeRelativeChildren(node->first, offspring, status);
    else
	sts = __pmdaNodeAbsoluteChildren(node, offspring, status);
    return sts;
}
