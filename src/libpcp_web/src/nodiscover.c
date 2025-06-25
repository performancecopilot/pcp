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
#include "discover.h"

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

discoverModuleData *
getDiscoverModuleData(pmDiscoverModule *module)
{
    (void)module;
    return NULL;
}

int
pmDiscoverSetSlots(pmDiscoverModule *module, void *slots)
{
    (void)slots; (void)module;
    return -EOPNOTSUPP;
}

int
pmDiscoverSetConfiguration(pmDiscoverModule *module, dict *config)
{
    (void)module; (void)config;
    return -EOPNOTSUPP;
}

int
pmDiscoverSetEventLoop(pmDiscoverModule *module, void *events)
{
    (void)module; (void)events;
    return -EOPNOTSUPP;
}

void
pmDiscoverSetupMetrics(pmDiscoverModule *module)
{
    (void)module;
}

int
pmDiscoverSetMetricRegistry(pmDiscoverModule *module, mmv_registry_t *registry)
{
    (void)module; (void)registry;
    return -EOPNOTSUPP;
}

int
pmDiscoverSetup(pmDiscoverModule *module, pmDiscoverCallBacks *cbs, void *arg)
{
    (void)module; (void)cbs; (void)arg;
    return -EOPNOTSUPP;
}

void
pmDiscoverClose(pmDiscoverModule *module)
{
    (void)module;
}
