/*
 * VFS (Virtual File System) statistics types
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
 * VFS statistics structure
 * Tracks system-wide file descriptor, vnode, process and thread counts
 * via kern.* sysctl parameters
 */
typedef struct vfsstats {
    __uint32_t	num_files;	/* current number of open files */
    __uint32_t	max_files;	/* maximum number of files allowed */
    __uint32_t	num_vnodes;	/* current number of vnodes in use */
    __uint32_t	max_vnodes;	/* maximum number of vnodes allowed */
    __uint32_t	num_tasks;	/* current number of processes */
    __uint32_t	num_threads;	/* current number of threads */
    __uint32_t	maxproc;	/* maximum processes (system-wide) */
    __uint32_t	maxprocperuid;	/* maximum processes per user */
    __uint32_t	maxfiles;	/* maximum file descriptors (system-wide) */
    __uint32_t	maxfilesperproc;	/* maximum file descriptors per process */
    __uint32_t	recycled_vnodes;	/* recycled vnode count */
} vfsstats_t;

extern int refresh_vfs(vfsstats_t *);
extern int fetch_vfs(unsigned int, pmAtomValue *);
