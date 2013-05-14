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
net_handler(char *buf)
{
    char *s;
    char *inst;

    /*
Inter-|   Receive                                                |  Transmit
 face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs drop fifo colls carrier compressed
    lo: 4060748   39057    0    0    0     0          0         0  4060748   39057    0    0    0     0       0          0
  eth0:       0  337614    0    0    0     0          0         0        0  267537    0    0    0 27346      62          0
     */

    if ((s = strchr(buf, ':')) != NULL)
    	*s = ' ';
    s = strtok(buf, " "); /* "Net" */
    inst = strtok(NULL, " "); /* netname */

    s = strtok(NULL, " ");
    put_str_value("network.interface.in.bytes", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.packets", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.errors", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.drops", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.fifo", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.frame", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.compressed", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.in.mcasts", NET_DEV_INDOM, inst, s);

    s = strtok(NULL, " ");
    put_str_value("network.interface.out.bytes", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.packets", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.errors", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.drops", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.fifo", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.collisions", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.carrier", NET_DEV_INDOM, inst, s);
    s = strtok(NULL, " ");
    put_str_value("network.interface.out.compressed", NET_DEV_INDOM, inst, s);

    return 0;
}

