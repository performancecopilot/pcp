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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef __PMDA_SOLARIS_NETMIB2_H
#define __PMDA_SOLARIS_NETMIB2_H

typedef struct nm2_udp_stats {
    uint64_t ipackets;
    uint64_t opackets;
    int32_t  ierrors;
    int32_t  oerrors;
    uint32_t noports;
    uint32_t overflows;
} nm2_udp_stats_t;

extern nm2_udp_stats_t nm2_udp;

typedef struct nm2_netif_stats {
    uint64_t ipackets;
    uint64_t opackets;
    uint64_t ibytes;
    uint64_t obytes;
    uint64_t delivered;
    uint64_t imcast;
    uint64_t omcast;
    uint64_t ibcast;
    uint64_t obcast;
    uint64_t ierrors;
    uint64_t oerrors;
    int32_t idrops;
    int32_t odrops;
    int mtu;
} nm2_netif_stats_t;

void netmib2_init(int);
void netmib2_refresh(void);
int netmib2_fetch(pmdaMetric *, int, pmAtomValue *);

#endif
