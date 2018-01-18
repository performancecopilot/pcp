/* 
 * Linux /proc/sys/kernel metrics cluster
 *
 * Copyright (c) 2017-2018 Red Hat.
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
    int		errcode;	/* error from previous refresh */
    uint32_t	entropy_avail;
    uint32_t	random_poolsize;
    uint32_t	pid_max;
} proc_sys_kernel_t;

extern int refresh_proc_sys_kernel(proc_sys_kernel_t *);
