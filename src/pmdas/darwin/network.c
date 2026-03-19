/*
 * Copyright (c) 2004,2006 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <fcntl.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "network.h"

void
init_network(void)
{

}

/*
 * Ensure we have space for the next interface in our pre-allocated
 * interface stats pool.  If not, make some or pass on the error.
 */
static int
check_stats_size(struct netstats *stats, int count)
{
    if (count > stats->highwater) {
	stats->highwater++;
	stats->interfaces = realloc(stats->interfaces,
				stats->highwater * sizeof(struct ifacestat));
	if (!stats->interfaces) {
	    stats->highwater = 0;
	    return -ENOMEM;
	}
    }
    return 0;
}

/*
 * Insert all interfaces into the global network instance domain.
 */
static int
update_network_indom(struct netstats *all, int count, pmdaIndom *indom)
{
    int	i;

    if (count > 0 && count != indom->it_numinst) {
	i = sizeof(pmdaInstid) * count;
	if ((indom->it_set = realloc(indom->it_set, i)) == NULL) {
	    indom->it_numinst = 0;
	    return -ENOMEM;
	}
    }
    for (i = 0; i < count; i++) {
	indom->it_set[i].i_name = all->interfaces[i].name;
	indom->it_set[i].i_inst = i;
    }
    indom->it_numinst = count;
    return 0;
}

int
refresh_network(struct netstats *stats, pmdaIndom *indom)
{
    int i = 0, status = 0;
    size_t n;
    char *new_buf, *next, *end;
    struct sockaddr_dl *sdl;

    static char *buf=NULL;
    static size_t buf_len=0;

#ifdef RTM_IFINFO2
    struct if_msghdr2 *ifm;
    int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST2, 0};
#else
    struct if_msghdr *ifm;
    int mib[6] = {CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0};
#endif

    if (sysctl(mib, 6, NULL, &n, NULL, 0) < 0) {
	/* unable to query buffer size */
	fprintf( stderr, "%s: get net mib buf len failed\n", pmGetProgname() );
	return -ENXIO;
    }
    if (n > buf_len) {
	if ((new_buf = malloc(n)) == NULL) {
	    /* unable to malloc buf */
	    fprintf( stderr, "%s: net mib buf malloc failed\n", pmGetProgname() );
	    return -ENXIO;
	} else {
	    if (buf != NULL) free(buf);
            buf = new_buf;
            buf_len = n;
	}
    }
    if (sysctl(mib, 6, buf, &n, NULL, 0) < 0) {
	/* unable to copy-in buffer */
	fprintf( stderr, "%s: net mib buf read failed\n", pmGetProgname() );
	return -ENXIO;
    }

    for (next = buf, i=0, end = buf + n; next < end; ) {

#ifdef RTM_IFINFO2
	ifm = (struct if_msghdr2 *)next;
	next += ifm->ifm_msglen;
	if (ifm->ifm_type == RTM_IFINFO2)
#else
	ifm = (struct if_msghdr *)next;
	next += ifm->ifm_msglen;
	if (ifm->ifm_type == RTM_IFINFO )
#endif
	{
	    status = check_stats_size(stats, i + 1);
	    if (status < 0) break;

	    sdl = (struct sockaddr_dl *)(ifm + 1);
	    n = sdl->sdl_nlen < IFNAMEMAX ? sdl->sdl_nlen : IFNAMEMAX;
	    memcpy( stats->interfaces[i].name, sdl->sdl_data, n );
	    stats->interfaces[i].name[n] = 0;

	    stats->interfaces[i].mtu = ifm->ifm_data.ifi_mtu;
	    stats->interfaces[i].baudrate = ifm->ifm_data.ifi_baudrate;
	    stats->interfaces[i].ipackets = ifm->ifm_data.ifi_ipackets;
	    stats->interfaces[i].ierrors = ifm->ifm_data.ifi_ierrors;
	    stats->interfaces[i].opackets = ifm->ifm_data.ifi_opackets;
	    stats->interfaces[i].oerrors = ifm->ifm_data.ifi_oerrors;
	    stats->interfaces[i].collisions = ifm->ifm_data.ifi_collisions;
	    stats->interfaces[i].ibytes = ifm->ifm_data.ifi_ibytes;
	    stats->interfaces[i].obytes = ifm->ifm_data.ifi_obytes;
	    stats->interfaces[i].imcasts = ifm->ifm_data.ifi_imcasts;
	    stats->interfaces[i].omcasts = ifm->ifm_data.ifi_omcasts;
	    stats->interfaces[i].iqdrops = ifm->ifm_data.ifi_iqdrops;
	    i++;
	}
    }
    if (!status) update_network_indom(stats, i, indom);
    return status;
}

/*
 * Compute aggregate network statistics by summing across all interfaces.
 * Mirrors Linux PMDA's network.all.* metrics (CLUSTER_NET_ALL).
 */
void
refresh_network_all(net_all_t *all, struct netstats *stats)
{
    int i;

    memset(all, 0, sizeof(net_all_t));
    for (i = 0; i < stats->highwater; i++) {
	all->in_bytes	+= stats->interfaces[i].ibytes;
	all->in_packets	+= stats->interfaces[i].ipackets;
	all->in_errors	+= stats->interfaces[i].ierrors;
	all->in_drops	+= stats->interfaces[i].iqdrops;
	all->out_bytes	+= stats->interfaces[i].obytes;
	all->out_packets += stats->interfaces[i].opackets;
	all->out_errors	+= stats->interfaces[i].oerrors;
    }
}

int
fetch_network_all(unsigned int item, pmAtomValue *atom)
{
    extern net_all_t mach_net_all;
    extern int mach_net_error;

    if (mach_net_error)
	return mach_net_error;

    switch (item) {
    case 0: /* network.all.in.bytes */
	atom->ull = mach_net_all.in_bytes;
	return 1;
    case 1: /* network.all.in.packets */
	atom->ull = mach_net_all.in_packets;
	return 1;
    case 2: /* network.all.in.errors */
	atom->ull = mach_net_all.in_errors;
	return 1;
    case 3: /* network.all.in.drops */
	atom->ull = mach_net_all.in_drops;
	return 1;
    case 4: /* network.all.out.bytes */
	atom->ull = mach_net_all.out_bytes;
	return 1;
    case 5: /* network.all.out.packets */
	atom->ull = mach_net_all.out_packets;
	return 1;
    case 6: /* network.all.out.errors */
	atom->ull = mach_net_all.out_errors;
	return 1;
    case 7: /* network.all.out.drops */
	atom->ull = 0;  /* macOS does not track output drops */
	return 1;
    case 8: /* network.all.total.bytes */
	atom->ull = mach_net_all.in_bytes + mach_net_all.out_bytes;
	return 1;
    case 9: /* network.all.total.packets */
	atom->ull = mach_net_all.in_packets + mach_net_all.out_packets;
	return 1;
    case 10: /* network.all.total.errors */
	atom->ull = mach_net_all.in_errors + mach_net_all.out_errors;
	return 1;
    case 11: /* network.all.total.drops */
	atom->ull = mach_net_all.in_drops;  /* input drops only on macOS */
	return 1;
    }
    return PM_ERR_PMID;
}

int
fetch_network(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern netstats_t mach_net;
	extern int mach_net_error;
	extern pmdaIndom indomtab[];

	if (mach_net_error)
		return mach_net_error;
	if (indomtab[NETWORK_INDOM].it_numinst == 0)
		return 0;	/* no values available */
	if (inst < 0 || inst >= indomtab[NETWORK_INDOM].it_numinst)
		return PM_ERR_INST;
	switch (item) {
	case 77: /* network.interface.in.bytes */
		atom->ull = mach_net.interfaces[inst].ibytes;
		return 1;
	case 78: /* network.interface.in.packets */
		atom->ull = mach_net.interfaces[inst].ipackets;
		return 1;
	case 79: /* network.interface.in.errors */
		atom->ull = mach_net.interfaces[inst].ierrors;
		return 1;
	case 80: /* network.interface.in.drops */
		atom->ull = mach_net.interfaces[inst].iqdrops;
		return 1;
	case 81: /* network.interface.in.mcasts */
		atom->ull = mach_net.interfaces[inst].imcasts;
		return 1;
	case 82: /* network.interface.out.bytes */
		atom->ull = mach_net.interfaces[inst].obytes;
		return 1;
	case 83: /* network.interface.out.packets */
		atom->ull = mach_net.interfaces[inst].opackets;
		return 1;
	case 84: /* network.interface.out.errors */
		atom->ull = mach_net.interfaces[inst].oerrors;
		return 1;
	case 85: /* network.interface.out.mcasts */
		atom->ull = mach_net.interfaces[inst].omcasts;
		return 1;
	case 86: /* network.interface.collisions */
		atom->ull = mach_net.interfaces[inst].collisions;
		return 1;
	case 87: /* network.interface.mtu */
		atom->ull = mach_net.interfaces[inst].mtu;
		return 1;
	case 88: /* network.interface.baudrate */
		atom->ull = mach_net.interfaces[inst].baudrate;
		return 1;
	case 89: /* network.interface.total.bytes */
		atom->ull = mach_net.interfaces[inst].ibytes +
			    mach_net.interfaces[inst].obytes;
		return 1;
	case 90: /* network.interface.total.packets */
		atom->ull = mach_net.interfaces[inst].ipackets +
			    mach_net.interfaces[inst].opackets;
		return 1;
	case 91: /* network.interface.total.errors */
		atom->ull = mach_net.interfaces[inst].ierrors +
			    mach_net.interfaces[inst].oerrors;
		return 1;
	case 92: /* network.interface.total.drops */
		atom->ull = mach_net.interfaces[inst].iqdrops;
		return 1;
	case 93: /* network.interface.total.mcasts */
		atom->ull = mach_net.interfaces[inst].imcasts +
			    mach_net.interfaces[inst].omcasts;
		return 1;
	}
	return PM_ERR_PMID;
}
