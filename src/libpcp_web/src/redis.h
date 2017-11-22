/*
 * Copyright (c) 2017 Red Hat.
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
#ifndef REDIS_SERIES_H
#define REDIS_SERIES_H

#include "load.h"
#include <hiredis/hiredis.h>

extern redisContext *redis_init(void);
extern void redis_stop(redisContext *);

extern void redis_series_metadata(redisContext *, metric_t *, value_t *);
extern void redis_series_addvalue(redisContext *, metric_t *, value_t *);

#endif	/* REDIS_SERIES_H */
