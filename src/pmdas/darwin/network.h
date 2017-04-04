/*
 * Network interface statistics types
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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

#include <sys/mount.h>
#include <nfs/rpcv2.h>
#include <nfs/nfsproto.h>
#include <nfs/nfs.h>

#define IFNAMEMAX	16	/* largest interface name we allow */

/*
 * Per-interface statistics
 */
typedef struct ifacestat {
    __uint64_t	mtu;			/* maximum transmission unit */
    __uint64_t	baudrate;		/* linespeed */
    __uint64_t	ipackets;		/* packets received on interface */
    __uint64_t	ierrors;		/* input errors on interface */
    __uint64_t	opackets;		/* packets sent on interface */
    __uint64_t	oerrors;		/* output errors on interface */
    __uint64_t	collisions;		/* collisions on csma interfaces */
    __uint64_t	ibytes;			/* total number of octets received */
    __uint64_t	obytes;			/* total number of octets sent */
    __uint64_t	imcasts;		/* packets received via multicast */
    __uint64_t	omcasts;		/* packets sent via multicast */
    __uint64_t	iqdrops;		/* dropped on input, this interface */
    char	name[IFNAMEMAX + 1];
} ifacestat_t;

/*
 * Global statistics.
 * 
 * We avoid continually realloc'ing memory by keeping track
 * of the maximum number of interfaces we've allocated space
 * for so far, and only realloc new space if we go beyond that.
 */
typedef struct netstats {
    int		highwater;	/* largest number of interfaces seen so far */
    ifacestat_t	*interfaces;	/* space for highwater number of interfaces */
} netstats_t;

