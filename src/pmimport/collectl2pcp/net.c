/*
 * Copyright (c) 2013 Red Hat Inc.
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

