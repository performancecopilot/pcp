/*
 * Copyright (c) 2017-2018 Red Hat.
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
#include "sdsalloc.h"
#include "private.h"
#include <stdlib.h>
#include <stdio.h>

static void
s_malloc_default_oom(size_t size)
{
    fprintf(stderr, "s_malloc: Out of memory allocating %llu bytes\n",
		(unsigned long long)size);
    fflush(stderr);
    abort();
}
static void (*s_malloc_oom_handler)(size_t) = s_malloc_default_oom;

void *
s_malloc(size_t size)
{
    void	*p;

    p = malloc(size);
    if (UNLIKELY(p == NULL))
	s_malloc_oom_handler(size);
    return p;
}

void *
s_realloc(void *ptr, size_t size)
{
    void	*p;

    if (ptr == NULL)
	return s_malloc(size);
    p = realloc(ptr, size);
    if (UNLIKELY(p == NULL))
	s_malloc_oom_handler(size);
    return p;
}

void
s_free(void *ptr)
{
    if (LIKELY(ptr != NULL))
	free(ptr);
}
