/*
 * IPC (Inter-Process Communication) statistics
 * Copyright (c) 2026 Red Hat, Paul Smith.
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
#include <sys/sysctl.h>
#include "pmapi.h"
#include "pmda.h"
#include "ipc.h"

int
refresh_ipc(ipcstats_t *ipc)
{
    size_t size;

    size = sizeof(ipc->mbuf_clusters);
    if (sysctlbyname("kern.ipc.nmbclusters", &ipc->mbuf_clusters, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(ipc->max_sockbuf);
    if (sysctlbyname("kern.ipc.maxsockbuf", &ipc->max_sockbuf, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(ipc->somaxconn);
    if (sysctlbyname("kern.ipc.somaxconn", &ipc->somaxconn, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(ipc->defunct_calls);
    if (sysctlbyname("kern.ipc.sodefunct_calls", &ipc->defunct_calls, &size, NULL, 0) == -1)
	return -oserror();

    return 0;
}

int
fetch_ipc(unsigned int item, pmAtomValue *atom)
{
    /* No computed metrics yet - all metrics are direct sysctl reads */
    return PM_ERR_PMID;
}
