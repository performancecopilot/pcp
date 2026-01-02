/*
 * VFS (Virtual File System) statistics
 * Copyright (c) 2025 Red Hat.
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
#include <sys/sysctl.h>
#include "pmapi.h"
#include "pmda.h"
#include "vfs.h"

int
refresh_vfs(vfsstats_t *vfs)
{
    size_t size;

    size = sizeof(vfs->num_files);
    if (sysctlbyname("kern.num_files", &vfs->num_files, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->max_files);
    if (sysctlbyname("kern.maxfiles", &vfs->max_files, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->num_vnodes);
    if (sysctlbyname("kern.num_vnodes", &vfs->num_vnodes, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->max_vnodes);
    if (sysctlbyname("kern.maxvnodes", &vfs->max_vnodes, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->num_tasks);
    if (sysctlbyname("kern.num_tasks", &vfs->num_tasks, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->num_threads);
    if (sysctlbyname("kern.num_threads", &vfs->num_threads, &size, NULL, 0) == -1)
	return -oserror();

    return 0;
}

int
fetch_vfs(unsigned int item, pmAtomValue *atom)
{
    extern vfsstats_t mach_vfs;
    extern int mach_vfs_error;

    if (mach_vfs_error)
	return mach_vfs_error;
    switch (item) {
    case 137: /* vfs.files.free */
	atom->ul = mach_vfs.max_files - mach_vfs.num_files;
	return 1;
    }
    return PM_ERR_PMID;
}
