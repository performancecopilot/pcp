/*
 * FreeBSD Kernel PMDA - network interface metrics
 *
 * Copyright (c) 2012 Ken McDonell.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ifaddrs.h>

#include "freebsd.h"

#define WARN_INIT 	1
#define WARN_READ_HEAD	2

void
refresh_netif_metrics(void)
{
    int			sts;
    struct ifaddrs	*ifa, *ifc;
    struct if_data	*ifp, *ifd;

    static int		warn = 0;	/* warn once control */

    /*
     * Not sure that the order of chained netif structs is invariant,
     * especially if interfaces are added to the configuration after
     * initial system boot ... so mark all the instances as inactive
     * and re-match based on the interface name
     */
    pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_INACTIVE);

    if (getifaddrs(&ifa) != 0) {
	if ((warn & WARN_READ_HEAD) == 0) {
	    fprintf(stderr, "refresh_netif_metrics: Warning: getifaddrs: %s\n", strerror(errno));
	    warn |= WARN_READ_HEAD;
	}
	return;
    }

    for (ifc = ifa; ifc != NULL; ifc = ifc->ifa_next) {
	if (ifc->ifa_addr->sa_family != AF_LINK) {
	   continue;
	}
	/* skip network interfaces that are not interesting ...  */
	if (strcmp(ifc->ifa_name, "lo0") == 0) {
	    continue;
	}
	sts = pmdaCacheLookupName(indomtab[NETIF_INDOM].it_indom, ifc->ifa_name, NULL, (void **)&ifp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    fprintf(stderr, "refresh_netif_metrics: Warning: duplicate name (%s) in network interface indom\n", ifc->ifa_name);
	    continue;
	}
	else if (sts == PMDA_CACHE_INACTIVE) {
	    /* reactivate an existing entry */
	    pmdaCacheStore(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_ADD, ifc->ifa_name, (void *)ifp);
	}
	else {
	    /* new entry */
	    ifp = (struct if_data *)malloc(sizeof(*ifp));
	    if (ifp == NULL) {
		fprintf(stderr, "Error: struct if_data alloc failed for network interface \"%s\"\n", ifc->ifa_name);
		__pmNoMem("refresh_netif_metrics", sizeof(*ifp), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    pmdaCacheStore(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_ADD, ifc->ifa_name, (void *)ifp);
	}
	ifd = (struct if_data *)ifc->ifa_data;
	memcpy((void *)ifp, (void *)ifd, sizeof(*ifp));
    }

    freeifaddrs(ifa);
}

int
do_netif_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct if_data	*ifp;
    int			sts = 0;

    if (inst != PM_IN_NULL) {
	/*
	 * per-network interface metrics
	 */
	sts = pmdaCacheLookup(indomtab[NETIF_INDOM].it_indom, inst, NULL, (void **)&ifp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    sts = 1;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmid_item(mdesc->m_desc.pmid)) {
		case 0:		/* network.interface.mtu */
		    atom->ul = (__uint32_t)ifp->ifi_mtu;
		    break;

		case 1:		/* network.interface.up */
		    atom->ul = (ifp->ifi_link_state & LINK_STATE_UP) == LINK_STATE_UP;
		    break;

		case 2:		/* network.interface.baudrate */
		    atom->ull = ifp->ifi_baudrate;
		    break;

		case 3:		/* network.interface.in.bytes */
		    atom->ull = ifp->ifi_ibytes;
		    break;

		case 4:		/* network.interface.in.packets */
		    atom->ull = ifp->ifi_ipackets;
		    break;

		case 5:		/* network.interface.in.mcasts */
		    atom->ull = ifp->ifi_imcasts;
		    break;

		case 6:		/* network.interface.in.errors */
		    atom->ull = ifp->ifi_ierrors;
		    break;

		case 7:		/* network.interface.in.drops */
		    atom->ull = ifp->ifi_iqdrops;
		    break;

		case 8:		/* network.interface.out.bytes */
		    atom->ull = ifp->ifi_obytes;
		    break;

		case 9:		/* network.interface.out.packets */
		    atom->ull = ifp->ifi_opackets;
		    break;

		case 10:	/* network.interface.out.mcasts */
		    atom->ull = ifp->ifi_omcasts;
		    break;

		case 11:	/* network.interface.out.errors */
		    atom->ull = ifp->ifi_oerrors;
		    break;

		case 12:	/* network.interface.out.collisions */
		    atom->ull = ifp->ifi_collisions;
		    break;

		case 13:	/* network.interface.total.bytes */
		    atom->ull = ifp->ifi_ibytes + ifp->ifi_obytes;
		    break;

		case 14:	/* network.interface.total.packets */
		    atom->ull = ifp->ifi_ipackets + ifp->ifi_opackets;
		    break;

		case 15:	/* network.interface.total.mcasts */
		    atom->ull = ifp->ifi_imcasts + ifp->ifi_omcasts;
		    break;

		case 16:	/* network.interface.total.errors */
		    atom->ull = ifp->ifi_ierrors + ifp->ifi_oerrors;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else
	    sts = 0;
    }

    return sts;
}
