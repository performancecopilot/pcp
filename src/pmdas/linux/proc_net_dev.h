/*
 * Linux /proc/net/dev metrics cluster
 *
 * Copyright (c) 2013 Red Hat.
 * Copyright (c) 1995,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

typedef struct {
    uint32_t	mtu;
    uint32_t	speed;
    uint8_t	duplex;
    uint8_t	linkup;
    uint8_t	running;
    uint8_t	pad;
} net_dev_t;

#define HWADDRSTRLEN 64

typedef struct {
    int		has_inet : 1;
    int		has_ipv6 : 1;
    int		has_hw   : 1;
    int		padding : 13;
    uint16_t	ipv6scope;
    char	inet[INET_ADDRSTRLEN];
    char	ipv6[INET6_ADDRSTRLEN+16];	/* extra for /plen */
    char	hw_addr[HWADDRSTRLEN];
} net_addr_t;

#define PROC_DEV_COUNTERS_PER_LINE   16

typedef struct {
    uint64_t	last_gen;
    uint64_t	last_counters[PROC_DEV_COUNTERS_PER_LINE];
    uint64_t	counters[PROC_DEV_COUNTERS_PER_LINE];
    net_dev_t	ioc;
} net_interface_t;

#ifndef ETHTOOL_GSET
#define ETHTOOL_GSET	0x1
#endif

#ifndef SIOCGIFCONF
#define SIOCGIFCONF	0x8912
#endif

#ifndef SIOCGIFFLAGS
#define SIOCGIFFLAGS	0x8913
#endif

#ifndef SIOCGIFADDR
#define SIOCGIFADDR	0x8915
#endif

#ifndef SIOCGIFMTU
#define SIOCGIFMTU	0x8921
#endif

#ifndef SIOCETHTOOL
#define SIOCETHTOOL	0x8946
#endif

/* ioctl(SIOCIFETHTOOL) GSET ("get settings") structure */
struct ethtool_cmd {
    uint32_t	cmd;
    uint32_t	supported;      /* Features this interface supports */
    uint32_t	advertising;    /* Features this interface advertises */
    uint16_t	speed;          /* The forced speed, 10Mb, 100Mb, gigabit */
    uint8_t	duplex;         /* Duplex, half or full */
    uint8_t	port;           /* Which connector port */
    uint8_t	phy_address;
    uint8_t	transceiver;    /* Which tranceiver to use */
    uint8_t	autoneg;        /* Enable or disable autonegotiation */
    uint32_t	maxtxpkt;       /* Tx pkts before generating tx int */
    uint32_t	maxrxpkt;       /* Rx pkts before generating rx int */
    uint32_t	reserved[4];
};

#define IPV6_ADDR_ANY           0x0000U
#define IPV6_ADDR_UNICAST       0x0001U
#define IPV6_ADDR_MULTICAST     0x0002U
#define IPV6_ADDR_ANYCAST       0x0004U
#define IPV6_ADDR_LOOPBACK      0x0010U
#define IPV6_ADDR_LINKLOCAL     0x0020U
#define IPV6_ADDR_SITELOCAL     0x0040U
#define IPV6_ADDR_COMPATv4      0x0080U

struct linux_container;
extern int refresh_proc_net_dev(pmInDom, struct linux_container *);
extern int refresh_net_dev_addr(pmInDom, struct linux_container *);
extern char *lookup_ipv6_scope(int);
