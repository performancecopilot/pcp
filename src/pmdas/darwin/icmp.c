/*
 * ICMP protocol statistics
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
#include <netinet/ip_icmp.h>
#include <netinet/icmp_var.h>
#include "pmapi.h"
#include "pmda.h"
#include "icmp.h"

int
refresh_icmp(icmpstats_t *icmp)
{
    struct icmpstat stats;
    size_t size = sizeof(stats);
    unsigned int i;

    if (sysctlbyname("net.inet.icmp.stats", &stats, &size, NULL, 0) == -1)
	return -oserror();

    /* Calculate total incoming messages (sum of histogram) */
    icmp->inmsgs = 0;
    for (i = 0; i < sizeof(stats.icps_inhist) / sizeof(stats.icps_inhist[0]); i++)
	icmp->inmsgs += stats.icps_inhist[i];

    /* Calculate total outgoing messages (sum of histogram) */
    icmp->outmsgs = 0;
    for (i = 0; i < sizeof(stats.icps_outhist) / sizeof(stats.icps_outhist[0]); i++)
	icmp->outmsgs += stats.icps_outhist[i];

    /* Calculate input errors (sum of error fields) */
    icmp->inerrors = stats.icps_error + stats.icps_badcode +
                     stats.icps_tooshort + stats.icps_checksum + stats.icps_badlen;

    /* Extract specific message types from histograms */
    icmp->indestunreachs = stats.icps_inhist[ICMP_UNREACH];
    icmp->inechos = stats.icps_inhist[ICMP_ECHO];
    icmp->inechoreps = stats.icps_inhist[ICMP_ECHOREPLY];
    icmp->outechos = stats.icps_outhist[ICMP_ECHO];
    icmp->outechoreps = stats.icps_outhist[ICMP_ECHOREPLY];

    return 0;
}

int
fetch_icmp(unsigned int item, pmAtomValue *atom)
{
    extern icmpstats_t mach_icmp;
    extern int mach_icmp_error;

    if (mach_icmp_error)
	return mach_icmp_error;

    switch (item) {
    case 147: /* network.icmp.inmsgs */
	atom->ull = mach_icmp.inmsgs;
	return 1;
    case 148: /* network.icmp.outmsgs */
	atom->ull = mach_icmp.outmsgs;
	return 1;
    case 149: /* network.icmp.inerrors */
	atom->ull = mach_icmp.inerrors;
	return 1;
    case 150: /* network.icmp.indestunreachs */
	atom->ull = mach_icmp.indestunreachs;
	return 1;
    case 151: /* network.icmp.inechos */
	atom->ull = mach_icmp.inechos;
	return 1;
    case 152: /* network.icmp.inechoreps */
	atom->ull = mach_icmp.inechoreps;
	return 1;
    case 153: /* network.icmp.outechos */
	atom->ull = mach_icmp.outechos;
	return 1;
    case 154: /* network.icmp.outechoreps */
	atom->ull = mach_icmp.outechoreps;
	return 1;
    }
    return PM_ERR_PMID;
}
