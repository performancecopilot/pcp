/*
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
#ifndef TCPCONN_H
#define TCPCONN_H

#include <netinet/tcp_fsm.h>

/*
 * TCP connection state statistics
 * Indexed by TCPS_* constants from tcp_fsm.h
 */
typedef struct tcpconn_stats {
    uint32_t state[TCP_NSTATES];  /* Count of connections in each TCP state */
} tcpconn_stats_t;

extern int refresh_tcpconn(tcpconn_stats_t *);

#endif /* TCPCONN_H */
