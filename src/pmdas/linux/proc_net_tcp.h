/*
 * Copyright (c) 2018 Red Hat.
 * Copyright (c) 1999,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Originally contributed by Michal Kara (lemming@arthur.plbohnice.cz)
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

enum {
	_PM_TCP_ESTABLISHED = 1,
	_PM_TCP_SYN_SENT,
	_PM_TCP_SYN_RECV,
	_PM_TCP_FIN_WAIT1,
	_PM_TCP_FIN_WAIT2,
	_PM_TCP_TIME_WAIT,
	_PM_TCP_CLOSE,
	_PM_TCP_CLOSE_WAIT,
	_PM_TCP_LAST_ACK,
	_PM_TCP_LISTEN,
	_PM_TCP_CLOSING,
	_PM_TCP_LAST
};

typedef struct tcpconn_stats {
    unsigned int	stat[_PM_TCP_LAST];
} tcpconn_stats_t;

typedef struct tcpconn_stats proc_net_tcp_t;
typedef struct tcpconn_stats proc_net_tcp6_t;

extern int refresh_proc_net_tcp(proc_net_tcp_t *);
extern int refresh_proc_net_tcp6(proc_net_tcp6_t *);
