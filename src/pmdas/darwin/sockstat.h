/*
 * Socket statistics types
 * Copyright (c) 2026 Red Hat.
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

/*
 * Socket statistics structure
 * Tracks active socket counts via net.inet.{tcp,udp}.pcbcount sysctls
 */
typedef struct sockstats {
    __uint32_t	tcp_inuse;	/* TCP protocol control blocks in use */
    __uint32_t	udp_inuse;	/* UDP protocol control blocks in use */
} sockstats_t;

extern int refresh_sockstat(sockstats_t *);
