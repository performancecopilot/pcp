/*
 * Copyright (c) 2017 Fujitsu.  All Rights Reserved.
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
    int tcp6_inuse;
    int udp6_inuse;
    int udplite6_inuse;
    int raw6_inuse;
    int frag6_inuse;
    int frag6_memory;
} proc_net_sockstat6_t;

extern int refresh_proc_net_sockstat6(proc_net_sockstat6_t *);

