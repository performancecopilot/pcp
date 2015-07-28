/*
 * Copyright (c) 2015 Red Hat, Inc.  All Rights Reserved.
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

/* extracted from /proc/net/softnet_stat */

typedef struct {
    uint64_t processed;
    uint64_t dropped;
    uint64_t time_squeeze;
    uint64_t cpu_collision;
    uint64_t received_rps;
    uint64_t flow_limit_count;

    unsigned flags;
} proc_net_softnet_t;

enum {
    SN_PROCESSED	= 1<<0,
    SN_DROPPED		= 1<<1,
    SN_TIME_SQUEEZE	= 1<<2,
    SN_CPU_COLLISION	= 1<<3,
    SN_RECEIVED_RPS	= 1<<4,
    SN_FLOW_LIMIT_COUNT	= 1<<5,
};

extern int refresh_proc_net_softnet(proc_net_softnet_t *);
