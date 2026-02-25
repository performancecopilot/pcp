/*
 * IPC (Inter-Process Communication) statistics types
 * Copyright (c) 2026 Red Hat, Paul Smith.
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
 * IPC statistics structure
 * Tracks system-wide socket and mbuf cluster parameters
 * via kern.ipc.* sysctl parameters
 */
typedef struct ipcstats {
    __uint32_t	mbuf_clusters;		/* mbuf cluster count */
    __uint32_t	max_sockbuf;		/* maximum socket buffer size */
    __uint32_t	somaxconn;		/* maximum socket listen backlog */
    __uint64_t	defunct_calls;		/* defunct socket calls */
} ipcstats_t;

extern int refresh_ipc(ipcstats_t *);
extern int fetch_ipc(unsigned int, pmAtomValue *);
