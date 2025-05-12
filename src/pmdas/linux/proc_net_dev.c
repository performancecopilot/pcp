/*
 * Linux /proc/net/dev metrics cluster
 *
 * Copyright (c) 2013-2016 Red Hat.
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
#include "linux.h"
#include <ctype.h>
#include <net/if.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include "namespaces.h"
#include "proc_net_dev.h"

static int
refresh_inet_socket(linux_container_t *container)
{
    static int netfd = -1;

    if (container)
	return container_open_network(container);
    if (netfd < 0)
	netfd = socket(AF_INET, SOCK_DGRAM, 0);
    return netfd;
}

static void
refresh_net_dev_ioctl(char *name, net_interface_t *netip,
		linux_container_t *cp, int *need_refresh)
{
    struct ethtool_cmd ecmd;
    struct iwreq iwreq;
    struct ifreq ifr;
    int fd;

    /*
     * Note:
     * Initialization here is not really needed.  If the ioctl()s
     * pass, structs are filled ... but valgrind (at least up to
     * version 3.9.0) does not know about the SIOCETHTOOL ioctl()
     * and thinks e.g. the use of ecmd after this call propagates
     * uninitialized data in to ioc.speed and ioc.duplex, causing
     * failures for qa/957
     * - Ken McDonell, 11 Apr 2014
     */
    memset(&iwreq, 0, sizeof(iwreq));
    memset(&ecmd, 0, sizeof(ecmd));
    memset(&ifr, 0, sizeof(ifr));

    if ((fd = refresh_inet_socket(cp)) < 0)
	return;

    if (need_refresh[REFRESH_NET_MTU]) {
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;
	pmstrncpy(ifr.ifr_name, IF_NAMESIZE, name);
	if (!(ioctl(fd, SIOCGIFMTU, &ifr) < 0))
	    netip->ioc.mtu = ifr.ifr_mtu;
    }

    if (need_refresh[REFRESH_NET_LINKUP] ||
	need_refresh[REFRESH_NET_RUNNING]) {
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;
	pmstrncpy(ifr.ifr_name, IF_NAMESIZE, name);
	if (!(ioctl(fd, SIOCGIFFLAGS, &ifr) < 0)) {
	    netip->ioc.linkup = !!(ifr.ifr_flags & IFF_UP);
	    netip->ioc.running = !!(ifr.ifr_flags & IFF_RUNNING);
	}
    }

    if (need_refresh[REFRESH_NET_TYPE] ||
	need_refresh[REFRESH_NET_SPEED] ||
	need_refresh[REFRESH_NET_DUPLEX] ||
	need_refresh[REFRESH_NET_WIRELESS]) {
	/* ETHTOOL ioctl -> non-root permissions issues for old kernels */
	ecmd.cmd = ETHTOOL_GSET;
	ifr.ifr_data = (caddr_t)&ecmd;
	pmstrncpy(ifr.ifr_name, IF_NAMESIZE, name);
	/* GIWRATE ioctl -> wireless interface bitrate access method */
	pmstrncpy(iwreq.ifr_ifrn.ifrn_name, IF_NAMESIZE, name);

	if (!(ioctl(fd, SIOCETHTOOL, &ifr) < 0)) {
	    /*
	     * speed is defined in ethtool.h and returns the speed in
	     * Mbps, so 100 for 100Mbps, 1000 for 1Gbps, etc.
	     * For kernel ABI reasons, this is split into two 16 bits
	     * fields, which must be combined for speeds above 1Gbps.
	     */
	    netip->ioc.type = 1;
	    netip->ioc.speed = (ecmd.speed_hi << 16) | ecmd.speed;
	    netip->ioc.duplex = ecmd.duplex + 1;
	    netip->ioc.wireless = 0;
	} else if (!(ioctl(fd, SIOCGIWRATE, &iwreq) < 0)) {
	    /*
	     * Wireless interface if the above succeeds; calculate the
	     * canonical speed and set duplex according to iface type.
	     */
	    netip->ioc.type = 0;
	    netip->ioc.speed = (iwreq.u.bitrate.value + 500000) / 1000000;
	    netip->ioc.duplex = 1;
	    netip->ioc.wireless = 1;
	}
    }
}

static void
refresh_net_ipv4_addr(char *name, net_addr_t *addr, linux_container_t *cp)
{
    struct ifreq ifr;
    int fd;

    if ((fd = refresh_inet_socket(cp)) < 0)
	return;
    pmstrncpy(ifr.ifr_name, IF_NAMESIZE, name);
    ifr.ifr_addr.sa_family = AF_INET;
    if (ioctl(fd, SIOCGIFADDR, &ifr) >= 0) {
	struct sockaddr_in *sin = (struct sockaddr_in *)&ifr.ifr_addr;
	if (inet_ntop(AF_INET, &sin->sin_addr, addr->inet, INET_ADDRSTRLEN))
	    addr->has_inet = 1;
    }
}

/*
 * No ioctl support or no permissions (more likely), so we
 * fall back to grovelling about in /sys/class/net in a last
 * ditch attempt to find the ethtool interface data (duplex
 * and speed).
 */
static char *
read_oneline(const char *path, char *buffer)
{
    FILE *fp = fopen(path, "r");

    if (fp) {
	int i = fscanf(fp, "%63s", buffer);
	fclose(fp);
	if (i == 1)
	    return buffer;
    }
    return NULL;
}

static int
refresh_net_dev_sysfs(char *name, net_interface_t *netip, int *need_refresh)
{
    char path[MAXPATHLEN];
    char line[64];
    char *value;

    if (need_refresh[REFRESH_NET_SPEED]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/speed",
		linux_statspath, name);
	value = read_oneline(path, line);
	if (value == NULL)
	    return PM_ERR_AGAIN;	/* no sysfs, try ioctl */
	netip->ioc.speed = atoi(value);
    }
    if (need_refresh[REFRESH_NET_MTU]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/mtu",
		linux_statspath, name);
	value = read_oneline(path, line);
	if (value == NULL)
	    return PM_ERR_AGAIN;	/* no sysfs, try ioctl */
	netip->ioc.mtu = atoi(value);
    }
    if (need_refresh[REFRESH_NET_LINKUP] ||
	need_refresh[REFRESH_NET_RUNNING]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/flags",
		linux_statspath, name);
	value = read_oneline(path, line);
	if (value == NULL)
	    return PM_ERR_AGAIN;	/* no sysfs, try ioctl */
	else {
	    unsigned long flags = strtoul(value, &value, 16);
	    netip->ioc.linkup = !!(flags & IFF_UP);
	    netip->ioc.running = !!(flags & IFF_RUNNING);
	}
    }
    if (need_refresh[REFRESH_NET_DUPLEX]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/duplex",
		linux_statspath, name);
	value = read_oneline(path, line);
	if (value == NULL) {
	    if (access(dirname(path), F_OK) != 0)
		return PM_ERR_AGAIN;	/* no sysfs, try ioctl */
	    netip->ioc.duplex = 0;
	}
	else if (strcmp(value, "half") == 0)
	    netip->ioc.duplex = 1;
	else if (strcmp(value, "full") == 0)
	    netip->ioc.duplex = 2;
	else
	    netip->ioc.duplex = 0;
    }
    if (need_refresh[REFRESH_NET_WIRELESS]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/wireless",
		linux_statspath, name);
	if (access(path, F_OK) == 0)
	    netip->ioc.wireless = 1;
	else
	    netip->ioc.wireless = 0;
    }
    if (need_refresh[REFRESH_NET_TYPE]) {
	pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/type",
		linux_statspath, name);
	value = read_oneline(path, line);
	if (value == NULL)
	    return PM_ERR_AGAIN;	/* no sysfs, try ioctl */
	netip->ioc.type = atoi(value);
    }
    if (need_refresh[REFRESH_NET_VIRTUAL]) {
	pmsprintf(path, sizeof(path), "%s/sys/devices/virtual/net/%s",
		linux_statspath, name);
	if (access(path, R_OK|X_OK) == 0)
	    netip->ioc.virtuali = 1;
	else
	    netip->ioc.virtuali = 0;
    }
    return 0;
}

static void
refresh_net_hw_addr(char *name, net_addr_t *netip)
{
    char path[MAXPATHLEN];
    char line[64];
    char *value;

    pmsprintf(path, sizeof(path), "%s/sys/class/net/%s/address",
		linux_statspath, name);
    value = read_oneline(path, line);
    if (value) {
	netip->has_hw = 1;
	pmstrncpy(netip->hw_addr, sizeof(netip->hw_addr), value);
    } else {
	netip->hw_addr[0] = '\0';
    }
}

static void
refresh_net_all(proc_net_all_t *all, net_interface_t *net, const char *name)
{
    int		physical;

    physical = (regexec(&all->regex, name, 0, NULL, 0) == REG_NOMATCH);
    if (physical) {
	all->in.bytes += net->counters[0];
	all->in.packets += net->counters[1];
	all->in.errors += net->counters[2];
	all->in.drops += net->counters[3];
	all->out.bytes += net->counters[8];
	all->out.packets += net->counters[9];
	all->out.errors += net->counters[10];
	all->out.drops += net->counters[11];
	all->total.bytes = all->in.bytes + all->out.bytes;
	all->total.packets = all->in.packets + all->out.packets;
	all->total.errors = all->in.errors + all->out.errors;
	all->total.drops = all->in.drops + all->out.drops;
    }
    if (pmDebugOptions.libpmda)
	fprintf(stderr, "%s: %s interface %s\n",
		"refresh_net_all", physical? "keep" : "cull", name);
}

static char *
appendc(char *string, size_t length, int c)
{
    char	*result;

    if ((result = realloc(string, length + 1)) != NULL) {
	result[length] = (char) c;
	return result;
    }
    free(string);
    return NULL;
}

static void
setup_proc_net_all(proc_net_all_t *all)
{
    const char	*default_pattern = "^(lo|bond[0-9]+|team[0-9]+|face)$";
    char	filename[MAXPATHLEN], buffer[128];
    char	*pattern = NULL, *p;
    size_t	length = 0;
    FILE	*file;
    int		skip = 0, sts;

    pmsprintf(filename, sizeof(filename), "%s/linux/interfaces.conf",
			pmGetConfig("PCP_SYSCONF_DIR"));
    if ((file = fopen(filename, "r")) == NULL)
	goto defaults;

    while (fgets(buffer, sizeof(buffer), file)) {
	for (p = buffer; *p; p++) {
	    if (*p == '#')		/* skip over comments */
		skip = 1;
	    else if (*p == '\n')	/* finish the comment */
		skip = 0;
	    else if (!skip && !isspace(*p) &&	/* cull space */
		((pattern = appendc(pattern, length, *p)) != NULL))
		length++;
	}
    }
    fclose(file);
    if (pattern != NULL)
	pattern = appendc(pattern, length, '\0');
    if (pattern == NULL)
	goto defaults;

    if ((sts = regcomp(&all->regex, pattern, REG_EXTENDED|REG_NOSUB)) != 0) {
	regerror(sts, &all->regex, buffer, sizeof(buffer));
	pmNotifyErr(LOG_ERR, "%s: ignoring \"%s\" pattern from %s: %s\n",
		    pmGetProgname(), pattern, filename, buffer);
defaults:
	(void)regcomp(&all->regex, default_pattern, REG_EXTENDED|REG_NOSUB);
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "%s: %s interface regular expression:\n%s\n",
		    "setup_proc_net_all", "default", default_pattern);
    } else {
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "%s: %s interface regular expression:\n%s\n",
		    "setup_proc_net_all", filename, pattern);
    }
    if (pattern)
	free(pattern);
}

void
refresh_proc_net_all(pmInDom indom, proc_net_all_t *all)
{
    static int		setup;
    net_interface_t	*netip;
    char		*name;
    int			sts;

    if (!setup) {
	setup_proc_net_all(all);
	setup = 1;
    }

    memset(&all->in, 0, sizeof(net_all_t));
    memset(&all->out, 0, sizeof(net_all_t));
    memset(&all->total, 0, sizeof(net_all_t));

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
        if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
            break;
        if (!pmdaCacheLookup(indom, sts, &name, (void **)&netip) || !netip)
            continue;
	refresh_net_all(all, netip, name);
    }
}

void
refresh_proc_net_dev(pmInDom indom, linux_container_t *container)
{
    static int		setup;		/* first pass through */
    static uint32_t	cache_err;	/* throttle messages */
    char		buf[1024];
    FILE		*fp;
    char		*p, *v;
    int			j, sts;
    net_interface_t	*netip;

    /*
     * first time, reload cache from external file, and force any
     * subsequent changes to be saved
     */
    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((fp = linux_statsfile("/proc/net/dev", buf, sizeof(buf))) == NULL)
	return;

    /*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 4060748   39057    0    0    0     0          0         0  4060748   39057    0    0    0     0       0          0
  eth0:       0  337614    0    0    0     0          0         0        0  267537    0    0    0 27346      62          0
     */

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if ((p = v = strchr(buf, ':')) == NULL)
	    continue;
	*p = '\0';
	for (p=buf; *p && isspace((int)*p); p++) {;}

	sts = pmdaCacheLookupName(indom, p, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_interface_t *)calloc(1, sizeof(net_interface_t));
	    if (pmDebugOptions.libpmda) {
		fprintf(stderr, "refresh_proc_net_dev: initialize \"%s\"\n", p);
	    }
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), p, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, p, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_proc_net_dev: pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, " PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), p, netip, pmErrStr(sts));
	    }
	    continue;
	}

	memset(&netip->ioc, 0, sizeof(netip->ioc));
	for (p=v, j=0; j < PROC_DEV_COUNTERS_PER_LINE; j++) {
	    for (; !isdigit((int)*p); p++) {;}
	    sscanf(p, "%llu", (long long unsigned int *)&netip->counters[j]);
	    for (; !isspace((int)*p); p++) {;}
	}
    }

    /* success */
    fclose(fp);

    if (!container)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);
}

int
refresh_net_sysfs(pmInDom indom, int *need_refresh)
{
    net_interface_t	*netip;
    char		*p;
    int			sts = 0;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, sts, &p, (void **)&netip) || !p)
	    continue;
	if ((sts = refresh_net_dev_sysfs(p, netip, need_refresh)) < 0)
	    break;
    }
    return sts;
}

void
refresh_net_ioctl(pmInDom indom, linux_container_t *cp, int *need_refresh)
{
    net_interface_t	*netip;
    char		*p;
    int			sts;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((sts = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, sts, &p, (void **)&netip) || !p)
	    continue;
	refresh_net_dev_ioctl(p, netip, cp, need_refresh);
    }
}

static void
refresh_net_dev_ipv4_addr(pmInDom indom, linux_container_t *container)
{
    int n, fd, sts, numreqs = 30;
    struct ifconf ifc;
    struct ifreq *ifr;
    net_addr_t *netip;
    static uint32_t cache_err;

    if ((fd = refresh_inet_socket(container)) < 0)
	return;

    ifc.ifc_buf = NULL;
    for (;;) {
	ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);

	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
	    free(ifc.ifc_buf);
	    return;
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
	    netip = (net_addr_t *)calloc(1, sizeof(net_addr_t));
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv4_addr: "
			"pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, ifr->ifr_name, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv4_addr: "
			"pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, "
			PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), ifr->ifr_name, netip, pmErrStr(sts));
	    }
	    continue;
	}

	refresh_net_ipv4_addr(ifr->ifr_name, netip, container);
    }
    free(ifc.ifc_buf);
}

static int
refresh_net_dev_hw_addr(pmInDom indom)
{
    int sts;
    DIR *dp;
    char *devname;
    struct dirent *dentry;
    char path[MAXPATHLEN];
    net_addr_t *netip;

    static uint32_t cache_err;

    pmsprintf(path, sizeof(path), "%s/sys/class/net", linux_statspath);
    if ((dp = opendir(path)) != NULL) {
	while ((dentry = readdir(dp)) != NULL) {
	    if (dentry->d_name[0] == '.')
		continue;
	    devname = dentry->d_name;
	    sts = pmdaCacheLookupName(indom, devname, NULL, (void **)&netip);
	    if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
		/* first time since re-loaded, else new one */
		netip = (net_addr_t *)calloc(1, sizeof(net_addr_t));
	    }
	    else if (sts < 0) {
		if (cache_err++ < 10) {
		    fprintf(stderr, "refresh_net_dev_hw_addr: "
				"pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
			pmInDomStr(indom), devname, pmErrStr(sts));
		}
		continue;
	    }
	    if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, devname, (void *)netip)) < 0) {
		if (cache_err++ < 10) {
		    fprintf(stderr, "refresh_net_dev_hw_addr: "
				"pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, "
				PRINTF_P_PFX "%p) failed: %s\n",
			    pmInDomStr(indom), devname, netip, pmErrStr(sts));
		}
		continue;
	    }

	    refresh_net_hw_addr(devname, netip);
	}
	closedir(dp);
    }
    return 0;
}

static void
refresh_net_dev_ipv6_addr(pmInDom indom)
{
    FILE *fp;
    char addr6p[8][5];
    char addr6[40], devname[20+1];
    char addr[INET6_ADDRSTRLEN];
    char buf[MAXPATHLEN];
    struct sockaddr_in6 sin6;
    int sts, plen, scope, dad_status, if_idx;
    net_addr_t *netip;
    static uint32_t cache_err;

    if ((fp = linux_statsfile("/proc/net/if_inet6", buf, sizeof(buf))) == NULL)
	return;

    /*
     * expecting something like ...
     * fe80000000000000fc5400fffe906c44 20e 40 20 80    vnet9
     */
    while (fscanf(fp, "%4s%4s%4s%4s%4s%4s%4s%4s %x %x %x %x %20s\n",
		  addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		  addr6p[4], addr6p[5], addr6p[6], addr6p[7],
		  &if_idx, &plen, &scope, &dad_status, devname) != EOF) {
	sts = pmdaCacheLookupName(indom, devname, NULL, (void **)&netip);
	if (sts == PM_ERR_INST || (sts >= 0 && netip == NULL)) {
	    /* first time since re-loaded, else new one */
	    netip = (net_addr_t *)calloc(1, sizeof(net_addr_t));
	}
	else if (sts < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv6_addr: "
				"pmdaCacheLookupName(%s, %s, ...) failed: %s\n",
		    pmInDomStr(indom), devname, pmErrStr(sts));
	    }
	    continue;
	}
	if ((sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, devname, (void *)netip)) < 0) {
	    if (cache_err++ < 10) {
		fprintf(stderr, "refresh_net_dev_ipv6_addr: "
			"pmdaCacheStore(%s, PMDA_CACHE_ADD, %s, "
			PRINTF_P_PFX "%p) failed: %s\n",
		    pmInDomStr(indom), devname, netip, pmErrStr(sts));
	    }
	    continue;
	}

	pmsprintf(addr6, sizeof(addr6), "%s:%s:%s:%s:%s:%s:%s:%s",
		addr6p[0], addr6p[1], addr6p[2], addr6p[3],
		addr6p[4], addr6p[5], addr6p[6], addr6p[7]);
	if (inet_pton(AF_INET6, addr6, sin6.sin6_addr.s6_addr) != 1)
	    continue;

	sin6.sin6_family = AF_INET6;
	sin6.sin6_port = 0;
	if (!inet_ntop(AF_INET6, &sin6.sin6_addr, addr, INET6_ADDRSTRLEN))
	    continue;
	pmsprintf(netip->ipv6, sizeof(netip->ipv6), "%s/%d", addr, plen);
	netip->ipv6scope = (uint16_t)scope;
	netip->has_ipv6 = 1;
    }
    fclose(fp);
}

/*
 * This separate indom provides the addresses for all interfaces including
 * aliases (e.g. eth0, eth0:0, eth0:1, etc) - this is what ifconfig does.
 */
void
clear_net_addr_indom(pmInDom indom)
{
    net_addr_t *p;
    int	inst;

    for (pmdaCacheOp(indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((inst = pmdaCacheOp(indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(indom, inst, NULL, (void **)&p) || !p)
	    continue;
	p->has_inet = 0;
	p->has_ipv6 = 0;
	p->has_hw   = 0;
    }
    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
}

void
store_net_addr_indom(pmInDom indom, linux_container_t *container)
{
    if (!container)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);
}

void
refresh_net_addr_sysfs(pmInDom indom, int *need_refresh)
{
    if (need_refresh[REFRESH_NETADDR_HW])
	refresh_net_dev_hw_addr(indom);
}

void
refresh_net_addr_ioctl(pmInDom indom, linux_container_t *cp, int *need_refresh)
{
    if (need_refresh[REFRESH_NETADDR_INET])
	refresh_net_dev_ipv4_addr(indom, cp);
    if (need_refresh[REFRESH_NETADDR_IPV6])
	refresh_net_dev_ipv6_addr(indom);
}

char *
lookup_ipv6_scope(int scope)
{
    switch (scope) {
    case IPV6_ADDR_ANY:
        return "Global";
    case IPV6_ADDR_LINKLOCAL:
        return "Link";
    case IPV6_ADDR_SITELOCAL:
        return "Site";
    case IPV6_ADDR_COMPATv4:
        return "Compat";
    case IPV6_ADDR_LOOPBACK:
        return "Host";
    }
    return "Unknown";
}
