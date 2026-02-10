/*
 * Socket statistics
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
#include "sockstat.h"

int
refresh_sockstat(sockstats_t *sockstat)
{
    size_t size;

    /* Fetch TCP PCB count */
    size = sizeof(sockstat->tcp_inuse);
    if (sysctlbyname("net.inet.tcp.pcbcount", &sockstat->tcp_inuse, &size, NULL, 0) == -1)
	return -oserror();

    /* Fetch UDP PCB count */
    size = sizeof(sockstat->udp_inuse);
    if (sysctlbyname("net.inet.udp.pcbcount", &sockstat->udp_inuse, &size, NULL, 0) == -1)
	return -oserror();

    return 0;
}
