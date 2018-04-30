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
#ifndef ZMALLOC_H
#define ZMALLOC_H 1

#include <sys/types.h>

extern void *zmalloc(size_t);
extern void *zcalloc(size_t);
extern void zfree(void *);

#endif /* ZMALLOC_H */
