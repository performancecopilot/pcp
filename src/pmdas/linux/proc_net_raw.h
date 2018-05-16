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

typedef struct rawconn_stats {
    unsigned int	count;
} rawconn_stats_t;

typedef struct rawconn_stats proc_net_raw_t;
typedef struct rawconn_stats proc_net_raw6_t;

extern int refresh_proc_net_raw(proc_net_raw_t *);
extern int refresh_proc_net_raw6(proc_net_raw6_t *);
