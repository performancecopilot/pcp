/*
 * TCP protocol statistics
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
#include <netinet/in.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include "pmapi.h"
#include "pmda.h"
#include "tcp.h"
#include "tcpconn.h"

int
refresh_tcp(tcpstats_t *tcp)
{
	size_t size = sizeof(tcp->stats);

	if (sysctlbyname("net.inet.tcp.stats", &tcp->stats, &size, NULL, 0) == -1)
		return -oserror();

	return 0;
}

int
fetch_tcp(unsigned int item, pmAtomValue *atom)
{
	extern tcpstats_t mach_tcp;
	extern int mach_tcp_error;
	extern tcpconn_stats_t mach_tcpconn;

	if (mach_tcp_error)
		return mach_tcp_error;

	switch (item) {
	case 172: /* network.tcp.currestab */
		/* Current established connections from tcpconn state */
		atom->ul = mach_tcpconn.state[TCPS_ESTABLISHED];
		return 1;

	case 176: /* network.tcp.inerrs */
		/* Sum of various receive errors */
		atom->ull = mach_tcp.stats.tcps_rcvbadsum +
			    mach_tcp.stats.tcps_rcvbadoff +
			    mach_tcp.stats.tcps_rcvshort +
			    mach_tcp.stats.tcps_rcvmemdrop;
		return 1;

	case 177: /* network.tcp.outrsts */
		/* Control segments sent (includes RST) */
		atom->ull = mach_tcp.stats.tcps_sndctrl;
		return 1;

	case 179: /* network.tcp.rtoalgorithm */
		/* Van Jacobson's algorithm = 4 (matches Linux) */
		atom->ul = 4;
		return 1;

	case 180: /* network.tcp.rtomin */
		/* Minimum RTO in milliseconds */
		atom->ul = 200;
		return 1;

	case 181: /* network.tcp.rtomax */
		/* Maximum RTO in milliseconds */
		atom->ul = 64000;
		return 1;

	case 182: /* network.tcp.maxconn */
		/* No fixed connection limit, -1 indicates dynamic */
		atom->l = -1;
		return 1;
	}
	return PM_ERR_PMID;
}
