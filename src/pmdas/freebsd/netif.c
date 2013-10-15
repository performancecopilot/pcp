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
#include <net/if_var.h>
#include <net/if_types.h>

#include "freebsd.h"

#define WARN_INIT 	1
#define WARN_READ_HEAD	2

void
refresh_netif_metrics(void)
{
    int			i;
    int			sts;
    unsigned long	kaddr;
    struct ifnethead	ifnethead;
    struct ifnet	ifnet;
    struct ifnet	*ifp;
    static int		warn = 0;	/* warn once control */

    /*
     * Not sure that the order of chained netif structs is invariant,
     * especially if interfaces are added to the configuration after
     * initial system boot ... so mark all the instances as inactive
     * and re-match based on the interface name
     */
    pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_INACTIVE);

    kaddr = symbols[KERN_IFNET].n_value;
    if (kvmp == NULL || kaddr == 0) {
	/* no network interface metrics for us today ... */
	if ((warn & WARN_INIT) == 0) {
	    fprintf(stderr, "refresh_netif_metrics: Warning: cannot get any network interface metrics\n");
	    warn |= WARN_INIT;
	}
	return;
    }

    /*
     * Kernel data structures for the linked list of network interface
     * information.
     *
     * _ifnet -> struct ifnethead {
     *		    struct ifnet *tqh_first;
     *		    struct ifnet **tqh_last;
     *		    ...
     *		 }
     *
     * and within an ifnet struct (declared in <net/if_var.h>) we find
     * the linked list maintained in if_link, the external interface
     * name in if_xname[] and if_data which is a nested if_data stuct
     * (declared in <net/if.h>) that contains many of the goodies we're
     * after, e.g. u_char ifi_type, u_long ifi_mtu, u_long ifi_baudrate,
     * u_long ifi_ipackets, u_long ifi_opackets, u_long ifi_ibytes,
     * u_long ifi_obytes, etc.
     */
    if (kvm_read(kvmp, kaddr, (char *)&ifnethead, sizeof(ifnethead)) != sizeof(ifnethead)) {
	if ((warn & WARN_READ_HEAD) == 0) {
	    fprintf(stderr, "refresh_netif_metrics: Warning: kvm_read: ifnethead: %s\n", kvm_geterr(kvmp));
	    warn |= WARN_READ_HEAD;
	}
	return;
    }

    for (i = 0; ; i++) {
	if (i == 0)
	    kaddr = (unsigned long)TAILQ_FIRST(&ifnethead);
	else
	    kaddr = (unsigned long)TAILQ_NEXT(&ifnet, if_link);

	if (kaddr == 0)
	    break;

	if (kvm_read(kvmp, kaddr, (char *)&ifnet, sizeof(ifnet)) != sizeof(ifnet)) {
	    fprintf(stderr, "refresh_netif_metrics: Error: kvm_read: ifnet[%d]: %s\n", i, kvm_geterr(kvmp));
	    return;
	}

	/* skip network interfaces that are not interesting ...  */
	if (strcmp(ifnet.if_xname, "lo0") == 0)
	    continue;

	sts = pmdaCacheLookupName(indomtab[NETIF_INDOM].it_indom, ifnet.if_xname, NULL, (void **)&ifp);
	if (sts == PMDA_CACHE_ACTIVE) {
	    fprintf(stderr, "refresh_netif_metrics: Warning: duplicate name (%s) in network interface indom\n", ifnet.if_xname);
	    continue;
	}
	else if (sts == PMDA_CACHE_INACTIVE) {
	    /* reactivate an existing entry */
	    pmdaCacheStore(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_ADD, ifnet.if_xname, (void *)ifp);
	}
	else {
	    /* new entry */
	    ifp = (struct ifnet *)malloc(sizeof(*ifp));
	    if (ifp == NULL) {
		fprintf(stderr, "Error: struct ifnet alloc failed for network interface \"%s\"\n", ifnet.if_xname);
		__pmNoMem("refresh_netif_metrics", sizeof(*ifp), PM_FATAL_ERR);
		/*NOTREACHED*/
	    }
	    pmdaCacheStore(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_ADD, ifnet.if_xname, (void *)ifp);
	}
	memcpy((void *)ifp, (void *)&ifnet, sizeof(*ifp));
    }
}

int
do_netif_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct ifnet	*ifp;
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
		    atom->ul = (__uint32_t)ifp->if_data.ifi_mtu;
		    break;

		case 1:		/* network.interface.up */
		    atom->ul = (ifp->if_flags & IFF_UP) == IFF_UP;
		    break;

		case 2:		/* network.interface.baudrate */
		    atom->ull = ifp->if_data.ifi_baudrate;
		    break;

		case 3:		/* network.interface.in.bytes */
		    atom->ull = ifp->if_data.ifi_ibytes;
		    break;

		case 4:		/* network.interface.in.packets */
		    atom->ull = ifp->if_data.ifi_ipackets;
		    break;

		case 5:		/* network.interface.in.mcasts */
		    atom->ull = ifp->if_data.ifi_imcasts;
		    break;

		case 6:		/* network.interface.in.errors */
		    atom->ull = ifp->if_data.ifi_ierrors;
		    break;

		case 7:		/* network.interface.in.drops */
		    atom->ull = ifp->if_data.ifi_iqdrops;
		    break;

		case 8:		/* network.interface.out.bytes */
		    atom->ull = ifp->if_data.ifi_obytes;
		    break;

		case 9:		/* network.interface.out.packets */
		    atom->ull = ifp->if_data.ifi_opackets;
		    break;

		case 10:	/* network.interface.out.mcasts */
		    atom->ull = ifp->if_data.ifi_omcasts;
		    break;

		case 11:	/* network.interface.out.errors */
		    atom->ull = ifp->if_data.ifi_oerrors;
		    break;

		case 12:	/* network.interface.out.collisions */
		    atom->ull = ifp->if_data.ifi_collisions;
		    break;

		case 13:	/* network.interface.total.bytes */
		    atom->ull = ifp->if_data.ifi_ibytes + ifp->if_data.ifi_obytes;
		    break;

		case 14:	/* network.interface.total.packets */
		    atom->ull = ifp->if_data.ifi_ipackets + ifp->if_data.ifi_opackets;
		    break;

		case 15:	/* network.interface.total.mcasts */
		    atom->ull = ifp->if_data.ifi_imcasts + ifp->if_data.ifi_omcasts;
		    break;

		case 16:	/* network.interface.total.errors */
		    atom->ull = ifp->if_data.ifi_ierrors + ifp->if_data.ifi_oerrors;
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
