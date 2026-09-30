/*
 * VFS (Virtual File System) statistics
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
#include <sys/sysctl.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "vfs.h"

extern mach_port_t mach_host;

/*
 * Live task and thread counts, as top(1) reports them.
 * Not kern.num_tasks/kern.num_threads: despite their names those
 * sysctls are the kernel's task and thread table limits.
 */
static int
refresh_task_counts(vfsstats_t *vfs)
{
    static processor_set_name_t default_pset = MACH_PORT_NULL;
    struct processor_set_load_info load;
    mach_msg_type_number_t count = PROCESSOR_SET_LOAD_INFO_COUNT;

    if (default_pset == MACH_PORT_NULL &&
	processor_set_default(mach_host, &default_pset) != KERN_SUCCESS)
	return PM_ERR_VALUE;

    if (processor_set_statistics(default_pset, PROCESSOR_SET_LOAD_INFO,
		(processor_set_info_t)&load, &count) != KERN_SUCCESS)
	return PM_ERR_VALUE;

    vfs->num_tasks = load.task_count;
    vfs->num_threads = load.thread_count;
    return 0;
}

int
refresh_vfs(vfsstats_t *vfs)
{
    size_t size;
    int error;

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

    error = refresh_task_counts(vfs);
    if (error < 0)
	return error;

    size = sizeof(vfs->maxproc);
    if (sysctlbyname("kern.maxproc", &vfs->maxproc, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->maxprocperuid);
    if (sysctlbyname("kern.maxprocperuid", &vfs->maxprocperuid, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->maxfiles);
    if (sysctlbyname("kern.maxfiles", &vfs->maxfiles, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->maxfilesperproc);
    if (sysctlbyname("kern.maxfilesperproc", &vfs->maxfilesperproc, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(vfs->recycled_vnodes);
    if (sysctlbyname("kern.num_recycledvnodes", &vfs->recycled_vnodes, &size, NULL, 0) == -1)
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
