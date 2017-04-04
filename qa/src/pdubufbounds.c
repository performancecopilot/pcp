/*
 * Copyright (c) 2015 Red Hat
 */

#include <pcp/pmapi.h>
#include <pcp/impl.h>
#include <assert.h>
#include <math.h>
#include "localconfig.h"


/* Test pdubuf bounds checking logic. */

#define INT sizeof(int)
#define INTALIGN(p) ((void*) ((unsigned long)(p) & (~ (INT-1))))

int main ()
{
    size_t sizes[] = { /* nothing smaller than ... */ INT, 16-INT, 15, 16,  400, 1024 };
    unsigned num_sizes = sizeof(sizes)/sizeof(sizes[0]);
    unsigned i;

    /* Allocate individual pdubufs of given size; probe a few bytes above and beyond. */
    for (i = 0; i<num_sizes; i++) {
        unsigned size = sizes[i]; /* NB: measured in bytes, not ints */
        char *buf;

	printf("pdubuf size %u\n", size);
        buf = (void*) __pmFindPDUBuf(size); /* NB: cast for bytewise rather than intwise arithmetic */
        assert (buf != NULL);

        __pmPinPDUBuf(buf); /* new pincnt = 2 */
        __pmFindPDUBuf(-1); /* report */

#define REPORT_UNPIN(ptr) do { \
        char *_p = ptr; \
        int _rc = __pmUnpinPDUBuf (_p); \
        printf ("unpin (%p) -> %d\n", _p, _rc); \
        } while (0);

        REPORT_UNPIN(buf - INT*2); /* previous int; rc=0 */
        REPORT_UNPIN(INTALIGN(buf+size+INT)); /* second next beyond aligned int; rc=0 */
        REPORT_UNPIN(INTALIGN(buf+size+1)); /* next beyond aligned int; rc=0 */
        REPORT_UNPIN(INTALIGN(buf+size-1)); /* last int; rc=1 */
        REPORT_UNPIN(buf); /* first int; rc=1 */
        REPORT_UNPIN(buf); /* first int again; rc=0 */

        __pmFindPDUBuf(-1); /* report */
    }

    return 0;
}
