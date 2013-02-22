/*
 * Linux /proc/net_dev metrics cluster
 *
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <ctype.h>
#include "proc_net_dev.h"

int
refresh_net_socket()
{
    static int netfd = -1;
    if (netfd < 0)
	netfd = socket(AF_INET, SOCK_DGRAM, 0);
    return netfd;
}

void
refresh_net_dev_ioctl(char *name, net_interface_t *netip)
{
    struct ethtool_cmd ecmd;
    struct ifreq ifr;
    int fd;

    memset(&netip->ioc, 0, sizeof(netip->ioc));
    if ((fd = refresh_net_socket()) < 0)
	return;

    ecmd.cmd = ETHTOOL_GSET;
    ifr.ifr_data = (caddr_t)&ecmd;
    strncpy(ifr.ifr_name, name, IF_NAMESIZE);
    if (!(ioctl(fd, SIOCETHTOOL, &ifr) < 0)) {
	/*
	 * speed is defined in ethtool.h and returns the speed in
	 * Mbps, so 100 for 100Mbps, 1000 for 1Gbps, etc
	 */
	netip->ioc.speed = ecmd.speed;
	netip->ioc.duplex = ecmd.duplex + 1;
    }
    if (!(ioctl(fd, SIOCGIFMTU, &ifr) < 0))
	netip->ioc.mtu = ifr.ifr_mtu;
    if (!(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)) {
	netip->ioc.linkup = !!(ifr.ifr_flags & IFF_UP);
	netip->ioc.running = !!(ifr.ifr_flags & IFF_RUNNING);
    }
}

void
refresh_net_inet_ioctl(char *name, net_inet_t *netip)
{
    struct sockaddr_in *sin;
    struct ifreq ifr;
    int fd;

    if ((fd = refresh_net_socket()) < 0)
	return;

    strcpy(ifr.ifr_name, name);
    ifr.ifr_addr.sa_family = AF_INET;
    if (!(ioctl(fd, SIOCGIFADDR, &ifr) < 0)) {
	netip->hasip = 1;
	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	netip->addr = sin->sin_addr;
    }
}

int
refresh_proc_net_dev(pmInDom indom)
{
    char		buf[1024];
    FILE		*fp;
    unsigned long long	llval;
    char		*p, *v;
    int			j, sts;
    net_interface_t	*netip;

    static uint64_t	gen;	/* refresh generation number */
    static uint32_t	cache_err;

    if ((fp = fopen("/proc/net/dev", "r")) == (FILE *)0)
    	return -oserror();

    if (gen == 0) {
	/*
	 * first time, reload cache from external file, and force any
	 * subsequent changes to be saved
	 */
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
    }
    gen++;

    /*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 4060748   39057    0    0    0     0          0         0  4060748   39057    0    0    0     0       0          0
  eth0:       0  337614    0    0    0     0          0         0        0  267537    0    0    0 27346      62          0
     */

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((p = v = strchr(buf, ':')) == NULL)
	    continue;
	*p = '\0';
	for (p=buf; *p && isspace((int)*p); p++) {;}

	sts = pmdaCacheLookupName(indom, p, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_interface_t *)calloc(1, sizeof(net_interface_t));
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_proc_net_dev: initialize \"%s\"\n", p);
	    }
#endif
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), p, pmErrStr(sts));
	    }
	    continue;
	}
	if (netip->last_gen != gen-1) {
	    /*
	     * rediscovered one that went away and has returned
	     *
	     * kernel counters are reset, so clear last_counters to
	     * avoid false overflows
	     */
	    for (j=0; j < PROC_DEV_COUNTERS_PER_LINE; j++) {
		netip->last_counters[j] = 0;
	    }
	}
	netip->last_gen = gen;
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, p, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, " PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), p, netip, pmErrStr(sts));
	    }
	    continue;
	}

	/* Issue ioctls for remaining data, not exported through proc */
	refresh_net_dev_ioctl(p, netip);

	for (p=v, j=0; j < PROC_DEV_COUNTERS_PER_LINE; j++) {
	    for (; !isdigit((int)*p); p++) {;}
	    sscanf(p, "%llu", &llval);
	    if (llval >= netip->last_counters[j]) {
		netip->counters[j] +=
		    llval - netip->last_counters[j];
	    }
	    else {
	    	/* 32bit counter has wrapped */
		netip->counters[j] +=
		    llval + (UINT_MAX - netip->last_counters[j]);
	    }
	    netip->last_counters[j] = llval;
	    for (; !isspace((int)*p); p++) {;}
	}
    }

    pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    /* success */
    fclose(fp);
    return 0;
}

/*
 * This separate indom provides the IP addresses for all interfaces including
 * aliases (e.g. eth0, eth0:0, eth0:1, etc) - this is what ifconfig does.
 */
int
refresh_net_dev_inet(pmInDom indom)
{
    int n, fd, sts, numreqs = 30;
    struct ifconf ifc;
    struct ifreq *ifr;
    net_inet_t *netip;
    static uint32_t cache_err;

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((fd = refresh_net_socket()) < 0)
	return fd;

    ifc.ifc_buf = NULL;
    for (;;) {
	ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);

	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
	    free(ifc.ifc_buf);
	    return -oserror();
	}
	if (ifc.ifc_len == sizeof(struct ifreq) * numreqs) {
	    /* assume it overflowed and try again */
	    numreqs *= 2;
	    continue;
	}
	break;
    }

    for (n = 0, ifr = ifc.ifc_req;
	 n < ifc.ifc_len;
	 n += sizeof(struct ifreq), ifr++) {
	sts = pmdaCacheLookupName(indom, ifr->ifr_name, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_inet_t *)calloc(1, sizeof(net_inet_t));
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_inet: pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, ifr->ifr_name, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_inet: pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, " PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, netip, pmErrStr(sts));
	    }
	    continue;
	}

	refresh_net_inet_ioctl(ifr->ifr_name, netip);
    }
    free(ifc.ifc_buf);

    pmdaCacheOp(indom, PMDA_CACHE_SAVE);
    return 0;
}
