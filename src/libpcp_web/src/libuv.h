/*
 * Copyright (c) 2018 Red Hat.
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
#ifndef SERIES_LIBUV_H
#define SERIES_LIBUV_H

#include "pmapi.h"
#include "redis.h"

#if defined(HAVE_LIBUV)
/* associate a libuv event loop with an async context */
extern int redisEventAttach(redisAsyncContext *, void *);
#else
#define redisEventAttach(ac, p)	do { } while (0)
#endif

#endif /* SERIES_LIBUV_H */
