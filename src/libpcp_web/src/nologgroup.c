/*
 * Copyright (c) 2025 Red Hat.
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
pmLogGroupLabel(pmLogGroupSettings *sp, const char *content, size_t length, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)length; (void)content; (void)sp;
    return -EOPNOTSUPP;
}

int
pmLogGroupMeta(pmWebGroupSettings *sp, int id, const char *content, size_t length, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)length; (void)content; (void)sp;
    return -EOPNOTSUPP;
}

int
pmLogGroupIndex(pmWebGroupSettings *sp, int id, const char *content, size_t length, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)id; (void)length; (void)content; (void)sp;
    return -EOPNOTSUPP;
}

int
pmLogGroupVolume(pmWebGroupSettings *sp, int id, unsigned int vol, const char *content, size_t length, struct dict *dp, void *arg)
{
    (void)arg; (void)dp; (void)vol; (void)id; (void)length; (void)content; (void)sp;
    return -EOPNOTSUPP;
}

int
pmLogGroupSetup(pmLogGroupModule *module)
{
    (void)module;
    return -EOPNOTSUPP;
}

int
pmLogGroupSetEventLoop(pmLogGroupModule *module, void *arg)
{
    (void)module; (void)arg;
    return -EOPNOTSUPP;
}

int
pmLogGroupSetConfiguration(pmLogGroupModule *module, struct dict *config)
{
    (void)module; (void)config;
    return -EOPNOTSUPP;
}

int
pmLogGroupSetMetricRegistry(pmLogGroupModule *module, struct mmv_registry *mmv)
{
    (void)module; (void)mmv;
    return -EOPNOTSUPP;
}

void
pmLogGroupClose(pmLogGroupModule *module)
{
    (void)module;
}
