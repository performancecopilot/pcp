/*
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _PM_SOCKSTAT_INUSE	0
#define _PM_SOCKSTAT_HIGHEST	1
#define _PM_SOCKSTAT_UTIL	2
typedef struct {
    int tcp[3];
    int udp[3];
    int raw[3];
} proc_net_sockstat_t;

extern int refresh_proc_net_sockstat(proc_net_sockstat_t *);

