/*
 * Linux /proc/stat metrics cluster
 *
 * Copyright (c) 1995 Silicon Graphics, Inc.  All Rights Reserved.
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
    float loadavg[3]; /* 1, 5 and 15 min load average */
    unsigned int runnable;
    unsigned int nprocs;
    unsigned int lastpid;
} proc_loadavg_t;

extern int refresh_proc_loadavg(proc_loadavg_t *);

