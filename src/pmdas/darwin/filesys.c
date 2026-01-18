/*
 * Filesystem statistics
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
#include <errno.h>
#include <sys/mount.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "filesys.h"

int
refresh_filesys(struct statfs **filesys, pmdaIndom *indom)
{
	int	i, count = getmntinfo(filesys, MNT_NOWAIT);

	if (count < 0) {
		indom->it_numinst = 0;
		indom->it_set = NULL;
		return -oserror();
	}
	if (count > 0 && count != indom->it_numinst) {
		i = sizeof(pmdaInstid) * count;
		if ((indom->it_set = realloc(indom->it_set, i)) == NULL) {
			indom->it_numinst = 0;
			return -ENOMEM;
		}
	}
	for (i = 0; i < count; i++) {
		indom->it_set[i].i_name = (*filesys)[i].f_mntfromname;
		indom->it_set[i].i_inst = i;
	}
	indom->it_numinst = count;
	return 0;
}

int
fetch_filesys(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern struct statfs *mach_fs;
	extern int mach_fs_error;
	extern pmdaIndom indomtab[];
	__uint64_t	ull, used;

	if (mach_fs_error)
		return mach_fs_error;
	if (item == 31) {	/* hinv.nfilesys */
		atom->ul = indomtab[FILESYS_INDOM].it_numinst;
		return 1;
	}
	if (indomtab[FILESYS_INDOM].it_numinst == 0)
		return 0;	/* no values available */
	if (inst < 0 || inst >= indomtab[FILESYS_INDOM].it_numinst)
		return PM_ERR_INST;
	switch (item) {
	case 32: /* filesys.capacity */
		ull = (__uint64_t)mach_fs[inst].f_blocks;
		atom->ull = ull * mach_fs[inst].f_bsize >> 10;
		return 1;
	case 33: /* filesys.used */
		used = (__uint64_t)(mach_fs[inst].f_blocks - mach_fs[inst].f_bfree);
		atom->ull = used * mach_fs[inst].f_bsize >> 10;
		return 1;
	case 34: /* filesys.free */
		ull = (__uint64_t)mach_fs[inst].f_bfree;
		atom->ull = ull * mach_fs[inst].f_bsize >> 10;
		return 1;
	case 129: /* filesys.maxfiles */
		atom->ul = mach_fs[inst].f_files;
		return 1;
	case 35: /* filesys.usedfiles */
		atom->ul = mach_fs[inst].f_files - mach_fs[inst].f_ffree;
		return 1;
	case 36: /* filesys.freefiles */
		atom->ul = mach_fs[inst].f_ffree;
		return 1;
	case 37: /* filesys.mountdir */
		atom->cp = mach_fs[inst].f_mntonname;
		return 1;
	case 38: /* filesys.full */
		used = (__uint64_t)(mach_fs[inst].f_blocks - mach_fs[inst].f_bfree);
		ull = used + (__uint64_t)mach_fs[inst].f_bavail;
		atom->d = (100.0 * (double)used) / (double)ull;
		return 1;
	case 39: /* filesys.blocksize */
		atom->ul = mach_fs[inst].f_bsize;
		return 1;
	case 40: /* filesys.avail */
		ull = (__uint64_t)mach_fs[inst].f_bavail;
		atom->ull = ull * mach_fs[inst].f_bsize >> 10;
		return 1;
	case 41: /* filesys.type */
		atom->cp = mach_fs[inst].f_fstypename;
		return 1;
	}
	return PM_ERR_PMID;
}
