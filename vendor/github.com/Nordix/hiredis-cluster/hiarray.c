/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020, Nick <heronr1 at gmail dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
 * Copyright (c) 2020-2021, Viktor Söderqvist <viktor.soderqvist at est dot tech>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <hiredis/alloc.h>
#include <stdlib.h>

#include "hiarray.h"
#include "hiutil.h"

struct hiarray *hiarray_create(uint32_t n, size_t size) {
    struct hiarray *a;

    ASSERT(n != 0 && size != 0);

    a = hi_malloc(sizeof(*a));
    if (a == NULL) {
        return NULL;
    }

    a->elem = hi_malloc(n * size);
    if (a->elem == NULL) {
        hi_free(a);
        return NULL;
    }

    a->nelem = 0;
    a->size = size;
    a->nalloc = n;

    return a;
}

void hiarray_destroy(struct hiarray *a) {
    hiarray_deinit(a);
    hi_free(a);
}

void hiarray_deinit(struct hiarray *a) {
    ASSERT(a->nelem == 0);

    hi_free(a->elem);
    a->elem = NULL;
}

uint32_t hiarray_idx(struct hiarray *a, void *elem) {
    uint8_t *p, *q;
    uint32_t off, idx;

    ASSERT(elem >= a->elem);

    p = a->elem;
    q = elem;
    off = (uint32_t)(q - p);

    ASSERT(off % (uint32_t)a->size == 0);

    idx = off / (uint32_t)a->size;

    return idx;
}

void *hiarray_push(struct hiarray *a) {
    void *elem, *new;
    size_t size;

    if (a->nelem == a->nalloc) {

        /* the array is full; allocate new array */
        size = a->size * a->nalloc;
        new = hi_realloc(a->elem, 2 * size);
        if (new == NULL) {
            return NULL;
        }

        a->elem = new;
        a->nalloc *= 2;
    }

    elem = (uint8_t *)a->elem + a->size * a->nelem;
    a->nelem++;

    return elem;
}

void *hiarray_pop(struct hiarray *a) {
    void *elem;

    ASSERT(a->nelem != 0);

    a->nelem--;
    elem = (uint8_t *)a->elem + a->size * a->nelem;

    return elem;
}

void *hiarray_get(struct hiarray *a, uint32_t idx) {
    void *elem;

    ASSERT(a->nelem != 0);
    ASSERT(idx < a->nelem);

    elem = (uint8_t *)a->elem + (a->size * idx);

    return elem;
}

void *hiarray_top(struct hiarray *a) {
    ASSERT(a->nelem != 0);

    return hiarray_get(a, a->nelem - 1);
}

void hiarray_swap(struct hiarray *a, struct hiarray *b) {
    struct hiarray tmp;

    tmp = *a;
    *a = *b;
    *b = tmp;
}

/*
 * Sort nelem elements of the array in ascending order based on the
 * compare comparator.
 */
void hiarray_sort(struct hiarray *a, hiarray_compare_t compare) {
    ASSERT(a->nelem != 0);

    qsort(a->elem, a->nelem, a->size, compare);
}

/*
 * Calls the func once for each element in the array as long as func returns
 * success. On failure short-circuits and returns the error status.
 */
int hiarray_each(struct hiarray *a, hiarray_each_t func, void *data) {
    uint32_t i, nelem;

    ASSERT(hiarray_n(a) != 0);
    ASSERT(func != NULL);

    for (i = 0, nelem = hiarray_n(a); i < nelem; i++) {
        void *elem = hiarray_get(a, i);
        rstatus_t status;

        status = func(elem, data);
        if (status != HI_OK) {
            return status;
        }
    }

    return HI_OK;
}
