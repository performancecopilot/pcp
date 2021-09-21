/*
 * Copyright (c) 2021 Red Hat.
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

/* Stub routines for platforms lacking necessary features (UV) */
#include "pmwebapi.h"

int
pmWebTimerSetup(void)
{
    return -EOPNOTSUPP;
}

void
pmWebTimerClose(void)
{
    /* noop */
}

int
pmWebTimerSetEventLoop(void *arg)
{
    (void)arg;
    return -EOPNOTSUPP;
}

int
pmWebTimerSetMetricRegistry(struct mmv_registry *registry)
{
    (void)registry;
    return -EOPNOTSUPP;
}

int
pmWebTimerRegister(pmWebTimerCallBack callback, void *data)
{
    (void)data; (void)callback;
    return -EOPNOTSUPP;
}

int
pmWebTimerRelease(int seq)
{
    (void)seq;
    return -EOPNOTSUPP;
}
