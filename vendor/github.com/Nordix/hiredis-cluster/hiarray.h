/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
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

#ifndef __HIARRAY_H_
#define __HIARRAY_H_

#include <stdint.h>

typedef int (*hiarray_compare_t)(const void *, const void *);
typedef int (*hiarray_each_t)(void *, void *);

struct hiarray {
    uint32_t nelem;  /* # element */
    void *elem;      /* element */
    size_t size;     /* element size */
    uint32_t nalloc; /* # allocated element */
};

#define null_hiarray                                                           \
    { 0, NULL, 0, 0 }

static inline void hiarray_null(struct hiarray *a) {
    a->nelem = 0;
    a->elem = NULL;
    a->size = 0;
    a->nalloc = 0;
}

static inline void hiarray_set(struct hiarray *a, void *elem, size_t size,
                               uint32_t nalloc) {
    a->nelem = 0;
    a->elem = elem;
    a->size = size;
    a->nalloc = nalloc;
}

static inline uint32_t hiarray_n(const struct hiarray *a) { return a->nelem; }

struct hiarray *hiarray_create(uint32_t n, size_t size);
void hiarray_destroy(struct hiarray *a);
void hiarray_deinit(struct hiarray *a);

uint32_t hiarray_idx(struct hiarray *a, void *elem);
void *hiarray_push(struct hiarray *a);
void *hiarray_pop(struct hiarray *a);
void *hiarray_get(struct hiarray *a, uint32_t idx);
void *hiarray_top(struct hiarray *a);
void hiarray_swap(struct hiarray *a, struct hiarray *b);
void hiarray_sort(struct hiarray *a, hiarray_compare_t compare);
int hiarray_each(struct hiarray *a, hiarray_each_t func, void *data);

#endif
