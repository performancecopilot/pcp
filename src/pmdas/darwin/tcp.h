/*
 * TCP protocol statistics types
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

#include <netinet/tcp_var.h>

/*
 * TCP statistics structure
 * Tracks TCP protocol statistics via net.inet.tcp.stats sysctl
 */
typedef struct tcpstats {
    struct tcpstat	stats;	/* kernel tcpstat from netinet/tcp_var.h */
} tcpstats_t;

extern int refresh_tcp(tcpstats_t *);
extern int fetch_tcp(unsigned int, pmAtomValue *);
