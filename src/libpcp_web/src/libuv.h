/*
 * Copyright (c) 2018 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#ifndef SERIES_LIBUV_H
#define SERIES_LIBUV_H

#include "pmapi.h"
#include "redis.h"

#if defined(HAVE_LIBUV)
/* associate a libuv event loop with an async context */
extern int redisEventAttach(redisAsyncContext *, void *);
#else
#define redisEventAttach(ac, p)	(REDIS_ERR)
#endif

#endif /* SERIES_LIBUV_H */
