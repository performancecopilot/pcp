/*
 * IPv6 protocol statistics
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
#include <sys/types.h>
#include "pmapi.h"
#include "pmda.h"
#include "ipv6.h"

/*
 * IPv6 statistics structure from kernel (netinet6/ip6_var.h)
 * We only define the fields we need for metrics
 */
struct ip6stat {
    __uint64_t ip6s_total;		/* total packets received */
    __uint64_t ip6s_tooshort;		/* packet too short */
    __uint64_t ip6s_toosmall;		/* not enough data */
    __uint64_t ip6s_fragments;		/* fragments received */
    __uint64_t ip6s_fragdropped;	/* frags dropped */
    __uint64_t ip6s_fragtimeout;	/* fragments timed out */
    __uint64_t ip6s_fragoverflow;	/* fragments that exceeded limit */
    __uint64_t ip6s_forward;		/* packets forwarded */
    __uint64_t ip6s_cantforward;	/* packets rcvd for unreachable dest */
    __uint64_t ip6s_redirectsent;	/* packets forwarded on same net */
    __uint64_t ip6s_delivered;		/* datagrams delivered to upper level */
    __uint64_t ip6s_localout;		/* total ip packets generated here */
    __uint64_t ip6s_odropped;		/* lost packets due to nobufs, etc. */
    __uint64_t ip6s_reassembled;	/* total packets reassembled ok */
    __uint64_t ip6s_atmfrag_rcvd;	/* atomic fragments received */
    __uint64_t ip6s_fragmented;		/* datagrams successfully fragmented */
    __uint64_t ip6s_ofragments;		/* output fragments created */
    /* Additional fields exist but we only need the above for our metrics */
};

int
refresh_ipv6(ipv6stats_t *ipv6)
{
    struct ip6stat stats;
    size_t size = sizeof(stats);

    if (sysctlbyname("net.inet6.ip6.stats", &stats, &size, NULL, 0) == -1)
	return -oserror();

    /* Map kernel stats to PCP metrics */
    ipv6->inreceives = stats.ip6s_total;
    ipv6->outforwarded = stats.ip6s_forward;
    ipv6->indiscards = stats.ip6s_toosmall + stats.ip6s_tooshort;
    ipv6->outdiscards = stats.ip6s_odropped;
    ipv6->fragcreates = stats.ip6s_ofragments;
    ipv6->reasmoks = stats.ip6s_reassembled;

    return 0;
}

int
fetch_ipv6(unsigned int item, pmAtomValue *atom)
{
    extern ipv6stats_t mach_ipv6;
    extern int mach_ipv6_error;

    if (mach_ipv6_error)
	return mach_ipv6_error;

    switch (item) {
    case 187: /* network.ipv6.inreceives */
	atom->ull = mach_ipv6.inreceives;
	return 1;
    case 188: /* network.ipv6.outforwarded */
	atom->ull = mach_ipv6.outforwarded;
	return 1;
    case 189: /* network.ipv6.indiscards */
	atom->ull = mach_ipv6.indiscards;
	return 1;
    case 190: /* network.ipv6.outdiscards */
	atom->ull = mach_ipv6.outdiscards;
	return 1;
    case 191: /* network.ipv6.fragcreates */
	atom->ull = mach_ipv6.fragcreates;
	return 1;
    case 192: /* network.ipv6.reasmoks */
	atom->ull = mach_ipv6.reasmoks;
	return 1;
    }
    return PM_ERR_PMID;
}
