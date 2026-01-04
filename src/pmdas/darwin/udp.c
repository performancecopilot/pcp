/*
 * UDP protocol statistics
 * Copyright (c) 2026 Red Hat.
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
#include <netinet/in.h>
#include <netinet/udp_var.h>
#include "pmapi.h"
#include "pmda.h"
#include "udp.h"

int
refresh_udp(udpstats_t *udp)
{
    struct udpstat stats;
    size_t size = sizeof(stats);

    if (sysctlbyname("net.inet.udp.stats", &stats, &size, NULL, 0) == -1)
	return -oserror();

    /* Copy and convert from u_int32_t to u_int64_t */
    udp->ipackets = stats.udps_ipackets;
    udp->opackets = stats.udps_opackets;
    udp->noport = stats.udps_noport;
    udp->fullsock = stats.udps_fullsock;
    udp->hdrops = stats.udps_hdrops;
    udp->badsum = stats.udps_badsum;
    udp->badlen = stats.udps_badlen;

    return 0;
}

int
fetch_udp(unsigned int item, pmAtomValue *atom)
{
    extern udpstats_t mach_udp;
    extern int mach_udp_error;

    if (mach_udp_error)
	return mach_udp_error;

    switch (item) {
    case 145: /* network.udp.inerrors */
	atom->ull = mach_udp.hdrops + mach_udp.badsum + mach_udp.badlen;
	return 1;
    }
    return PM_ERR_PMID;
}
