/**
 * Copied in from libpcp_web
 * Provides compatibility shim layer for inih, and the redis dict implementation
 */

#include <stdlib.h>
#include "zmalloc.h"
#include "dict.h"
#include "sds.h"

void *
zmalloc(size_t size)
{
    return malloc(size);
}

void *
zcalloc(size_t elem, size_t size)
{
    return calloc(elem, size);
}

void
zfree(void *ptr)
{
    free(ptr);
}

void *
s_malloc(size_t size)
{
    return malloc(size);
}

void *
s_realloc(void *ptr, size_t size)
{
    return realloc(ptr, size);
}

void
s_free(void *ptr)
{
    free(ptr);
}
