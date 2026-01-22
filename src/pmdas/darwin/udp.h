/*
 * UDP protocol statistics types
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

/*
 * UDP statistics structure
 * Tracks UDP protocol statistics via net.inet.udp.stats sysctl
 */
typedef struct udpstats {
    __uint64_t	ipackets;	/* total input datagrams */
    __uint64_t	opackets;	/* total output datagrams */
    __uint64_t	noport;		/* datagrams to port with no listener */
    __uint64_t	fullsock;	/* datagrams dropped due to full socket buffers */
    __uint64_t	hdrops;		/* datagrams dropped due to header errors */
    __uint64_t	badsum;		/* datagrams dropped due to bad checksum */
    __uint64_t	badlen;		/* datagrams dropped due to bad length */
} udpstats_t;

extern int refresh_udp(udpstats_t *);
extern int fetch_udp(unsigned int, pmAtomValue *);
