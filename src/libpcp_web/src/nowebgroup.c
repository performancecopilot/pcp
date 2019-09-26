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
pmWebGroupContext(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
    return -EOPNOTSUPP;
}

void
pmWebGroupDerive(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupFetch(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupInDom(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupMetric(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupChildren(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupProfile(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupScrape(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

void
pmWebGroupStore(pmWebGroupSettings *sp, sds id, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)sp;
}

int
pmWebGroupSetup(pmWebGroupModule *module)
{
    (void)module;
    return -EOPNOTSUPP;
}

int
pmWebGroupSetEventLoop(pmWebGroupModule *module, void *arg)
{
    (void)module;
    return -EOPNOTSUPP;
}

int
pmWebGroupSetConfiguration(pmWebGroupModule *module, struct dict *config)
{
    (void)module; (void)config;
    return -EOPNOTSUPP;
}

int
pmWebGroupSetMetricRegistry(pmWebGroupModule *module, struct mmv_registry *mmv)
{
    (void)module; (void)mmv;
    return -EOPNOTSUPP;
}

void
pmWebGroupClose(pmWebGroupModule *module)
{
    (void)module;
}
