/*
 * Copyright (c) 2018 Red Hat.
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

typedef struct proc_net_unix {
    unsigned int	datagram_count;
    unsigned int	stream_established;
    unsigned int	stream_listen;
    unsigned int	stream_count;
} proc_net_unix_t;

extern int refresh_proc_net_unix(proc_net_unix_t *);
