/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2017 Fujitsu.
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
    int total;
    int tcp_inuse;
    int tcp_orphan;
    int tcp_tw;
    int tcp_alloc;
    int tcp_mem;
    int udp_inuse;
    int udp_mem;
    int udplite_inuse;
    int raw_inuse;
    int frag_inuse;
    int frag_memory;
} proc_net_sockstat_t;

extern int refresh_proc_net_sockstat(proc_net_sockstat_t *);

