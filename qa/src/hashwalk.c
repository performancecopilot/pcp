/*
 * Copyright (c) 2013 Red Hat.
 *
 * Exercise libpcp hash walk interfaces
 */

#include <pcp/pmapi.h>
#include "libpcp.h"

void
dumpnode(unsigned int key, long data)
{
    printf("%u => %ld\n", key, data);
}

__pmHashWalkState
walker(const __pmHashNode *n, void *v)
{
    __pmHashWalkState state = (__pmHashWalkState)(long)v;
    dumpnode(n->key, (long)n->data);
    return state;
}

void
chained(__pmHashCtl *h)
{
    __pmHashNode *n;

    for (n = __pmHashWalk(h, PM_HASH_WALK_START);
         n != NULL;
         n = __pmHashWalk(h, PM_HASH_WALK_NEXT)) {
	dumpnode(n->key, (long)n->data);
    }
}

int
main(int argc, char **argv)
{
    __pmHashCtl hc = { 0 };

    printf("adding entries\n");
    __pmHashAdd(0, (void *)0L, &hc);
    __pmHashAdd(1, (void *)1L, &hc);
    __pmHashAdd(2, (void *)2L, &hc);
    __pmHashAdd(3, (void *)3L, &hc);

    if (argc >= 2) {
        if (strcmp(argv[1], "callback") == 0)
            __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
        else if (strcmp(argv[1], "linked") == 0)
            chained(&hc);
        exit(0);
    }

    printf("iterating WALK_STOP\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_STOP, &hc);
    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
    printf("iterating WALK_DELETE_STOP\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_DELETE_STOP, &hc);
    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);
    printf("iterating WALK_DELETE_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_DELETE_NEXT, &hc);
    printf("iterating WALK_NEXT\n");
    __pmHashWalkCB(walker, (void *)PM_HASH_WALK_NEXT, &hc);

    exit(0);
}
