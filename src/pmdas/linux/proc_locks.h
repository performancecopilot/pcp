/*
 * Linux /proc/locks metrics cluster
 *
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

typedef struct lock_stats {
    unsigned int	read;
    unsigned int	write;
    unsigned int	count;
} lock_stats_t;

typedef struct proc_locks {
    lock_stats_t	posix;
    lock_stats_t	flock;
    lock_stats_t	lease;
} proc_locks_t;

extern int refresh_proc_locks(proc_locks_t *);

