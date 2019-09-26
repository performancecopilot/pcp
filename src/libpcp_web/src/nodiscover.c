/*
 * Copyright (c) 2019 Red Hat.
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

/* Stub routines for platforms lacking necessary features (UV, SSL) */
#include "pmwebapi.h"

int
pmDiscoverRegister(const char *path, pmDiscoverModule *module,
		pmDiscoverCallBacks *callbacks, void *arg)
{
    (void)arg; (void)path; (void)module; (void)callbacks;
    return -EOPNOTSUPP;
}

void
pmDiscoverUnregister(int handle)
{
    (void)handle;
}
