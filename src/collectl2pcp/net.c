/*
 * Copyright (c) 2013 Red Hat.
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
 * Handler for network.interface.*
 * Net     lo: 216173634 2124262    0    0    0     0          0         0 216173634 2124262    0    0    0     0       0          0
 */

#include "metrics.h"

int
net_handler(handler_t *h, fields_t *f)
{
    int n;
    char *s;
    char *inst;
    pmInDom indom = pmInDom_build(LINUX_DOMAIN, NET_DEV_INDOM);

    /*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 4060748   39057    0    0    0     0          0         0  4060748   39057    0    0    0     0       0          0
  eth0:       0  337614    0    0    0     0          0         0        0  267537    0    0    0 27346      62          0
     */

    if (f->nfields != 18)
    	return -1;

    inst = f->fields[1];
    if ((s = strchr(inst, ':')) != NULL)
    	*s = '\0';

    n = 2;
    put_str_value("network.interface.in.bytes", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.packets", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.errors", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.drops", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.fifo", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.frame", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.compressed", indom, inst, f->fields[n++]);
    put_str_value("network.interface.in.mcasts", indom, inst, f->fields[n++]);

    put_str_value("network.interface.out.bytes", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.packets", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.errors", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.drops", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.fifo", indom, inst, f->fields[n++]);
    put_str_value("network.interface.collisions", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.carrier", indom, inst, f->fields[n++]);
    put_str_value("network.interface.out.compressed", indom, inst, f->fields[n++]);

    return 0;
}

int
net_tcp_handler(handler_t *h, fields_t *f)
{
    int n;

    /*
     * tcp-Tcp: RtoAlgorithm RtoMin RtoMax MaxConn ActiveOpens PassiveOpens AttemptFails EstabResets CurrEstab InSegs OutSegs RetransSegs InErrs OutRsts
     * tcp-Tcp: 1 200 120000 -1 206329 2355899 5366 9034 1629 2319043300 2457610733 51680 55 111341
     */

    if (f->nfields != 15)
    	return -1;

    if (!isdigit((int)(f->fields[1][0])))
    	return -1; /* skip column heading */

    n = 1;
    put_str_value("network.tcp.rtoalgorithm", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.rtomin", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.rtomax", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.maxconn", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.activeopens", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.passiveopens", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.attemptfails", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.estabresets", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.currestab", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.insegs", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.outsegs", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.retranssegs", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.inerrs", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.tcp.outrsts", PM_INDOM_NULL, NULL, f->fields[n++]);

    return 0;
}

int
net_udp_handler(handler_t *h, fields_t *f)
{
    int n;

     /*
      * tcp-Udp: InDatagrams NoPorts InErrors OutDatagrams RcvbufErrors SndbufErrors
      * tcp-Udp: 3687153 1023 0 4953744 0 0
      */
    if (f->nfields != 7)
    	return -1;
    if (!isdigit((int)(f->fields[1][0])))
    	return -1; /* skip column heading */

    n = 1;
    put_str_value("network.udp.indatagrams", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.udp.noports", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.udp.inerrors", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.udp.outdatagrams", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.udp.recvbuferrors", PM_INDOM_NULL, NULL, f->fields[n++]);
    put_str_value("network.udp.sndbuferrors", PM_INDOM_NULL, NULL, f->fields[n++]);

    return 0;
}

