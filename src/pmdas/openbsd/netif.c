/*
 * OpenBSD Kernel PMDA - network interface metrics
 *
 * Copyright (c) 2012,2013 Ken McDonell.  All Rights Reserved.
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

/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "pmapi.h"
#include "pmda.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <sys/gmon.h>

#include "openbsd.h"

static int		valid = -1;

/*
 * algorithm here comes from if.c in /usr/src/usr.bin/netstat
 *
 * This is the FreeBSD explanation ... OpenBSD seems at least
 * related, but I've not found a description for OpenBSD.
 * Include this for reference ... Ken McDonell
 *
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

#define RT_ROUNDUP(a)      ((a) > 0 ? (1 + (((a) - 1) | ((sizeof(uint64_t)) - 1))) : (sizeof(uint64_t)))

static void             
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
    int		i;

    for (i = 0; i < RTAX_MAX; i++) {
	if (addrs & (1 << i)) {
	    rti_info[i] = sa;
	    sa = (struct sockaddr *)((char *)(sa) + RT_ROUNDUP(sa->sa_len));
	} else
	    rti_info[i] = NULL;
    }
}

void
refresh_netif_metrics(void)
{
    int			sts;
    static int		name[] = { CTL_NET, AF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
    u_int		namelen = sizeof(name) / sizeof(name[0]);
    static size_t	buflen = 0;
    static char		*buf = NULL;
    char		*bufend;
    char		*next;
    size_t		new_buflen;
    struct rt_msghdr	*rtm;
    struct if_msghdr	*ifm;
    struct sockaddr	*sa;
    struct sockaddr	*rti_info[RTAX_MAX];
    struct sockaddr_dl	*sdl;
    char		if_name[IF_NAMESIZE];

    if (valid == -1) {
	/* one-trip reload */
	pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_LOAD);
    }
    else {
	/*
	 * Not sure that the order of chained netif structs is invariant,
	 * especially if interfaces are added to the configuration after
	 * initial system boot ... so mark all the instances as inactive
	 * and re-match based on the interface name
	 */
	pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    }

    valid = 0;

    /* get number of if_data structs available */
    if ((sts = sysctl(name, namelen, NULL, &new_buflen, NULL, 0)) != 0) {
	fprintf(stderr, "refresh_netif_metrics: new_buflen sysctl(): %s\n", strerror(errno));
	return;
    }
    if (new_buflen != buflen) {
	/*
	 * one trip ... not sure if it can change thereafter, but just
	 * in case
	 */
	if (buf != NULL)
	    free(buf);
	buf = (char *)malloc(new_buflen);
	if (buf == NULL) {
	    pmNoMem("refresh_disk_metrics: stats", new_buflen, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
	buflen = new_buflen;
    }
    if ((sts = sysctl(name, namelen, buf, &buflen, NULL, 0)) != 0) {
	fprintf(stderr, "refresh_netif_metrics: buf sysctl(): %s\n", strerror(errno));
	return;
    }

    bufend = buf + buflen;
    for (next = buf; next < bufend; next += rtm->rtm_msglen) {
	rtm = (struct rt_msghdr *)next;
	if (rtm->rtm_version != RTM_VERSION)
	    continue;
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
	    ifm = (struct if_msghdr *)next;
	    sa = (struct sockaddr *)(ifm + 1);
	    get_rtaddrs(ifm->ifm_addrs, sa, rti_info);
	    sdl = (struct sockaddr_dl *)rti_info[RTAX_IFP];
	    if (sdl == NULL || sdl->sdl_family != AF_LINK)
		continue;
	    if (sdl->sdl_nlen < IF_NAMESIZE) {
		memcpy(if_name, sdl->sdl_data, sdl->sdl_nlen);
		if_name[sdl->sdl_nlen] = '\0';
	    }
	    else {
		memcpy(if_name, sdl->sdl_data, IF_NAMESIZE-1);
		if_name[IF_NAMESIZE-1] = '\0';
	    }
	    /* skip network interfaces that are not interesting ...  */
	    if (strcmp(if_name, "lo0") == 0)
		continue;
	    if (pmDebugOptions.appl0) {
		sts = pmdaCacheLookupName(indomtab[NETIF_INDOM].it_indom, if_name, NULL, NULL);
		if (sts < 0)
		    fprintf(stderr, "Info: found network interface %s\n", if_name);
	    }
	    if ((sts = pmdaCacheStore(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_ADD, if_name, (void *)ifm)) < 0) {
		fprintf(stderr, "refresh_netif_metrics: pmdaCacheStore(%s) failed: %s\n", if_name, pmErrStr(sts));
		continue;
	    }
	    valid++;
	    break;
	}
    }
    if (valid)
	pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_SAVE);
}

int
do_netif_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    struct if_msghdr	*ifm;
    int			sts;

    if (!valid)
	return 0;

    if (inst != PM_IN_NULL) {
	/*
	 * per-network interface metrics
	 */
	sts = pmdaCacheLookup(indomtab[NETIF_INDOM].it_indom, inst, NULL, (void **)&ifm);
	if (sts == PMDA_CACHE_ACTIVE) {
	    sts = 1;
	    /* cluster and domain already checked, just need item ... */
	    switch (pmID_item(mdesc->m_desc.pmid)) {
		case 0:		/* network.interface.mtu */
		    atom->ull = ifm->ifm_data.ifi_mtu;
		    break;

		case 1:		/* network.interface.up */
		    atom->ul = (ifm->ifm_flags & IFF_UP) == IFF_UP;
		    break;

		case 2:		/* network.interface.baudrate */
		    atom->ull = ifm->ifm_data.ifi_baudrate;
		    break;

		case 3:		/* network.interface.in.bytes */
		    atom->ull = ifm->ifm_data.ifi_ibytes;
		    break;

		case 4:		/* network.interface.in.packets */
		    atom->ull = ifm->ifm_data.ifi_ipackets;
		    break;

		case 5:		/* network.interface.in.mcasts */
		    atom->ull = ifm->ifm_data.ifi_imcasts;
		    break;

		case 6:		/* network.interface.in.errors */
		    atom->ull = ifm->ifm_data.ifi_ierrors;
		    break;

		case 7:		/* network.interface.in.drops */
		    atom->ull = ifm->ifm_data.ifi_iqdrops;
		    break;

		case 8:		/* network.interface.out.bytes */
		    atom->ull = ifm->ifm_data.ifi_obytes;
		    break;

		case 9:		/* network.interface.out.packets */
		    atom->ull = ifm->ifm_data.ifi_opackets;
		    break;

		case 10:	/* network.interface.out.mcasts */
		    atom->ull = ifm->ifm_data.ifi_omcasts;
		    break;

		case 11:	/* network.interface.out.errors */
		    atom->ull = ifm->ifm_data.ifi_oerrors;
		    break;

		case 12:	/* network.interface.out.collisions */
		    atom->ull = ifm->ifm_data.ifi_collisions;
		    break;

		case 13:	/* network.interface.total.bytes */
		    atom->ull = ifm->ifm_data.ifi_ibytes + ifm->ifm_data.ifi_obytes;
		    break;

		case 14:	/* network.interface.total.packets */
		    atom->ull = ifm->ifm_data.ifi_ipackets + ifm->ifm_data.ifi_opackets;
		    break;

		case 15:	/* network.interface.total.mcasts */
		    atom->ull = ifm->ifm_data.ifi_imcasts + ifm->ifm_data.ifi_omcasts;
		    break;

		case 16:	/* network.interface.total.errors */
		    atom->ull = ifm->ifm_data.ifi_ierrors + ifm->ifm_data.ifi_oerrors;
		    break;

		default:
		    sts = PM_ERR_PMID;
		    break;
	    }
	}
	else
	    sts = 0;
    }
    else {
	/*
	 * most network interface metrics don't have an instance domain
	 *
	 * cluster and domain already checked, just need item ...
	 */
	switch (pmID_item(mdesc->m_desc.pmid)) {
	    case 17:		/* hinv.interface */
		atom->ul = valid;
		sts = 1;
		break;

	    default:
		sts = PM_ERR_INST;
		break;
	}
    }

    return sts;
}
