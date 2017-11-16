/*
 * Copyright (C) 2009 Max Matveev. All rights reserved.
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

/* Extract network-related information from the kernel using MIB2
 * interfaces.  MIB2 structures are described by RFC 4113, 4293,
 * 4001. IPv6 specific MIB structures are described in RFC 2465, 2466.
 */

#include <fcntl.h>
#include <stropts.h>
#include <inet/mib2.h>
#include <sys/tihdr.h>
#include <sys/socket.h>
#include <net/if.h>

#include "common.h"
#include "netmib2.h"

static int afd = -1;
static int data_valid;
static int netif_added;

nm2_udp_stats_t nm2_udp;

static nm2_netif_stats_t *
netif_cache_inst(const char *ifname)
{
    pmInDom indom = indomtab[NETIF_INDOM].it_indom;
    nm2_netif_stats_t *ist;
    int rv;

    if (pmdaCacheLookupName(indom, ifname, &rv,
			    (void **)&ist) != PMDA_CACHE_ACTIVE) {
	ist = malloc(sizeof(*ist));
	if (ist == NULL) {
	    pmNotifyErr(LOG_WARNING,
			  "Out of memory for stats on network interface '%s'\n",
			  ifname);
	    return NULL;
	}

	rv = pmdaCacheStore(indom, PMDA_CACHE_ADD, ifname, ist);
	if (rv < 0) {
	    pmNotifyErr(LOG_WARNING,
			  "Cannot create instance for '%s': %s\n",
			  ifname, pmErrStr(rv));
	    free(ist);
	    return NULL;
	}
	netif_added++;
    }

    return ist;
}

static void
ipv4_stats (const void *data, int sz)
{
    const mib2_ipAddrEntry_t *ipa = data;

    while (sz > 0) {
	nm2_netif_stats_t *ist = netif_cache_inst(ipa->ipAdEntIfIndex.o_bytes);

	if (ist) {
	    ist->mtu = ipa->ipAdEntInfo.ae_mtu;
	    /* We get byte count and other stuff from Traffic stats */
	}
	sz -= sizeof(*ipa);
	ipa++;
    }
}

static void
ipv4_ifstats(const void *data, int sz)
{
    const mib2_ipIfStatsEntry_t *ips = data;

    nm2_udp.noports = 0;
    nm2_udp.overflows = 0;

    while (sz > 0) {
	/* index 0 is a pseudo-interface */
	if (ips->ipIfStatsIfIndex) {
	    nm2_netif_stats_t *ist;
	    char name[64];

	    if ((if_indextoname(ips->ipIfStatsIfIndex, name) != NULL) &&
		((ist = netif_cache_inst(name)) != NULL)) {

		ist->ibytes = ips->ipIfStatsHCInOctets;
		ist->obytes = ips->ipIfStatsHCOutOctets;
		ist->ipackets = ips->ipIfStatsHCInReceives;
		ist->opackets = ips->ipIfStatsHCOutTransmits;
		ist->imcast = ips->ipIfStatsHCInMcastPkts;
		ist->omcast = ips->ipIfStatsHCOutMcastPkts;
		ist->ibcast = ips->ipIfStatsHCInBcastPkts;
		ist->obcast = ips->ipIfStatsHCOutBcastPkts;
		ist->delivered = ips->ipIfStatsHCInDelivers;
		ist->idrops = ips->ipIfStatsInDiscards;
		ist->odrops = ips->ipIfStatsOutDiscards;
		ist->ierrors =
		    + (uint64_t)ips->ipIfStatsInHdrErrors
		    + ips->ipIfStatsInTooBigErrors
		    + ips->ipIfStatsInNoRoutes
		    + ips->ipIfStatsInAddrErrors
		    + ips->ipIfStatsInUnknownProtos
		    + ips->ipIfStatsInTruncatedPkts;

		ist->oerrors = ips->ipIfStatsOutFragFails;
	    }
	}

	nm2_udp.noports += ips->udpNoPorts;
	nm2_udp.overflows += ips->udpInOverflows;

	sz -= sizeof(*ips);
	ips++;
    }
}

void
netmib2_refresh(void)
{
    struct strbuf ctrl;
    struct opthdr *oh;
    uint64_t buf[64]; /* Arbitrary size, just large enough to fit req + opthdr */
    struct T_optmgmt_req *omreq = (struct T_optmgmt_req *)buf;
    struct T_optmgmt_ack *omack = (struct T_optmgmt_ack *)buf;

    omreq->PRIM_type = T_SVR4_OPTMGMT_REQ;
    omreq->OPT_offset = sizeof (*omreq);
    omreq->OPT_length = sizeof (*oh);
    omreq->MGMT_flags = T_CURRENT;

    oh = (struct opthdr *)(omreq + 1);
    oh->level = /*EXPER_IP_AND_TESTHIDDEN*/MIB2_IP;
    oh->name  = 0;
    oh->len   = 0;

    ctrl.buf = (char *)buf;
    ctrl.len = omreq->OPT_length + omreq->OPT_offset;

    data_valid = 0;

    if (putmsg(afd, &ctrl, NULL, 0) == -1) {
	pmNotifyErr(LOG_ERR, "Failed to push message down stream: %s\n",
		      osstrerror());
	return;
    }

    oh = (struct opthdr *)(omack + 1);
    ctrl.maxlen = sizeof(buf);

    netif_added = 0;

    for (;;) {
	int flags = 0;
	struct strbuf data;
	int rv;

	rv = getmsg(afd, &ctrl, NULL, &flags);
	if (rv < 0) {
	    pmNotifyErr(LOG_ERR, "netmib2: failed to get a response: %s\n",
			  osstrerror());
	    break;
	}

        if ((rv == 0) && (ctrl.len >= sizeof(*omack)) &&
	    (omack->PRIM_type == T_OPTMGMT_ACK) &&
	    (omack->MGMT_flags == T_SUCCESS) && (oh->len == 0)) {
	    data_valid = 1;
	    break;
	}

	if ((rv != MOREDATA) || (ctrl.len < sizeof(*omack)) ||
	    (omack->PRIM_type != T_OPTMGMT_ACK) ||
	    (omack->MGMT_flags != T_SUCCESS)) {
	    pmNotifyErr(LOG_ERR, "netmib2: Unexpected message received\n");
	    break;
	}

	memset(&data, 0, sizeof(data));
	data.buf = malloc(oh->len);
	if (data.buf == NULL) {
	    pmNotifyErr(LOG_ERR, "netmib2: Out of memory\n");
	    break;
	}

	data.maxlen = oh->len;
	flags = 0;

	rv = getmsg(afd, NULL, &data, &flags);
	if (rv) {
	    pmNotifyErr(LOG_ERR,
			  "net2mib: Failed to get additional data: %s\n",
			  osstrerror());
	    break;
	}

	switch (oh->level) {
	case MIB2_IP:
	    switch(oh->name) {
	    case 0: /* Overall statistic */
		break;

	    case MIB2_IP_ADDR:
		ipv4_stats(data.buf, data.len);
		break;

	    case MIB2_IP_TRAFFIC_STATS:
		ipv4_ifstats(data.buf, data.len);
		break;
	    }
	    break;

	case MIB2_IP6:
	    break;

	case MIB2_UDP:
	    if (oh->name == 0) {
		mib2_udp_t *m2u = (mib2_udp_t *)data.buf;

#ifdef EXPER_IP_AND_TESTHIDDEN
		nm2_udp.ipackets = m2u->udpHCInDatagrams;
		nm2_udp.opackets = m2u->udpHCOutDatagrams;
#else
		nm2_udp.ipackets = m2u->udpInDatagrams;
		nm2_udp.opackets = m2u->udpOutDatagrams;
#endif
		nm2_udp.ierrors = m2u->udpInErrors;
		nm2_udp.oerrors = m2u->udpOutErrors;
	    }
	    break;

	case MIB2_TCP:
	    break;
	}

	free(data.buf);
    }

    if (netif_added) {
	pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_SAVE);
    }
}

int
netmib2_fetch(pmdaMetric *pm, int inst, pmAtomValue *av)
{
    char *fsname;
    metricdesc_t *md = pm->m_user;
    char *ist;

    if (pm->m_desc.indom == PM_INDOM_NULL) {
	switch (pm->m_desc.type) {
	case PM_TYPE_U32:
	    av->ul = *(uint32_t *)md->md_offset;
	    return 1;

	case PM_TYPE_U64:
	    av->ull = *(uint64_t *)md->md_offset;
	    return 1;
	}

	return PM_ERR_APPVERSION;
    }

    if (pmdaCacheLookup(indomtab[NETIF_INDOM].it_indom, inst, &fsname,
			(void **)&ist) != PMDA_CACHE_ACTIVE)
	return PM_ERR_INST;

    if (ist) {
	switch (pm->m_desc.type) {
	case PM_TYPE_U32:
	    av->ul = *(uint32_t *)(ist + md->md_offset);
	    return 1;
 
	case PM_TYPE_U64:
	    av->ull = *(uint64_t *)(ist + md->md_offset);
	    return 1;
	}

	return PM_ERR_APPVERSION;
    }

    /* Even if we've copied the values don't admit they're good unless
     * the update was problem-free. */
    return data_valid;
}

void
netmib2_init(int first)
{
    char *mods[] = {"tcp", "udp", "icmp"};
    int i;

    if (afd >= 0)
	return;

    afd = open("/dev/arp", O_RDWR);
    if (afd < 0) {
	pmNotifyErr(LOG_ERR, "Cannot open /dev/arp: %s\n", osstrerror());
	return;
    }

    for (i = 0; i < 3; i++ ) {
	if (ioctl(afd, I_PUSH, mods[i]) < 0) {
	    pmNotifyErr(LOG_ERR, "Cannot push %s into /dev/arp: %s\n",
			  mods[i], osstrerror());
	    close(afd);
	    afd = -1;
	    return;
	}
    }

    pmdaCacheOp(indomtab[NETIF_INDOM].it_indom, PMDA_CACHE_LOAD);
    netmib2_refresh();
}
