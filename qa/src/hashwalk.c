/*
 * Copyright (c) 2013,2025 Red Hat.
 *
 * Exercise libpcp hash walk interfaces
 */

#include <pcp/pmapi.h>
#include "libpcp.h"
#include <stdint.h>

void
dumphash(const __pmHashCtl *h)
{
    printf("HASH: %u %s, %u %s\n",
	    h->nodes, h->nodes == 1 ? "entry" : "entries",
	    h->hsize, h->hsize == 1 ? "bucket" : "buckets");
}

void
dumpnode(unsigned int key, __int64_t data)
{
    printf("NODE: %u => %" FMT_INT64 "\n", key, data);
}

__pmHashWalkState
walker(const __pmHashNode *n, void *v)
{
    uintptr_t	v_int;
    __pmHashWalkState state;
    v_int = (uintptr_t)v;
    state = (__pmHashWalkState)(v_int & 0xf);
    dumpnode(n->key, (__int64_t)((__psint_t)n->data));
    return state;
}

void
chained(__pmHashCtl *h)
{
    __pmHashNode *n;

    for (n = __pmHashWalk(h, PM_HASH_WALK_START);
         n != NULL;
         n = __pmHashWalk(h, PM_HASH_WALK_NEXT)) {
	dumpnode(n->key, (__int64_t)((__psint_t)n->data));
    }
}

int
main(int argc, char **argv)
{
    __pmHashCtl hc = { 0 };

    dumphash(&hc);

    printf("adding entries\n");
    __pmHashAdd(0, (void *)0L, &hc);
    __pmHashAdd(1, (void *)1L, &hc);
    __pmHashAdd(2, (void *)2L, &hc);
    __pmHashAdd(3, (void *)3L, &hc);

    dumphash(&hc);

    if (argc >= 2) {
        if (strcmp(argv[1], "callback") == 0)
            __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
        else if (strcmp(argv[1], "linked") == 0)
            chained(&hc);
	dumphash(&hc);
        exit(0);
    }

    printf("iterating WALK_STOP\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_STOP, &hc);
    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
    printf("iterating WALK_DELETE_STOP\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_DELETE_STOP, &hc);

    dumphash(&hc);

    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
    printf("iterating WALK_DELETE_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_DELETE_NEXT, &hc);
    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);

    dumphash(&hc);

    exit(0);
}
