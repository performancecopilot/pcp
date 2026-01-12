/*
 * VM statistics
 * Copyright (c) 2026 Red Hat.
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include "vmstat.h"

#define page_count_to_kb(x) (((__uint64_t)(x) << mach_page_shift) >> 10)
#define page_count_to_mb(x) (((__uint64_t)(x) << mach_page_shift) >> 20)

int
refresh_vmstat(struct vm_statistics64 *vmstat)
{
	extern mach_port_t mach_host;
	int error, info = HOST_VM_INFO64;
	natural_t count = HOST_VM_INFO64_COUNT;

	error = host_statistics64(mach_host, info, (host_info_t)vmstat, &count);
	return (error != KERN_SUCCESS) ? -oserror() : 0;
}

int
refresh_swap(struct xsw_usage *swapusage)
{
	int		mib[2] = { CTL_VM, VM_SWAPUSAGE };
	size_t	size = sizeof(struct xsw_usage);

	if (sysctl(mib, 2, swapusage, &size, NULL, 0) == -1)
		return -oserror();
	return 0;
}

int
fetch_vmstat(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern struct vm_statistics64 mach_vmstat;
	extern int mach_vmstat_error;
	extern struct xsw_usage mach_swap;
	extern int mach_swap_error;
	extern unsigned int mach_page_shift;

	if (mach_vmstat_error)
		return mach_vmstat_error;
	switch (item) {
	case 2: /* hinv.physmem */
		atom->ul = (__uint32_t)page_count_to_mb(
				mach_vmstat.free_count + mach_vmstat.wire_count +
				mach_vmstat.active_count + mach_vmstat.inactive_count);
		return 1;
	case 3: /* mem.physmem */
		atom->ull = page_count_to_kb(
				mach_vmstat.free_count + mach_vmstat.wire_count +
				mach_vmstat.active_count + mach_vmstat.inactive_count);
		return 1;
	case 4: /* mem.freemem */
		atom->ull = page_count_to_kb(mach_vmstat.free_count);
		return 1;
	case 5: /* mem.active */
		atom->ull = page_count_to_kb(mach_vmstat.active_count);
		return 1;
	case 6: /* mem.inactive */
		atom->ull = page_count_to_kb(mach_vmstat.inactive_count);
		return 1;
	case 19: /* mem.util.wired */
		atom->ull = page_count_to_kb(mach_vmstat.wire_count);
		return 1;
	case 20: /* mem.util.active */
		atom->ull = page_count_to_kb(mach_vmstat.active_count);
		return 1;
	case 21: /* mem.util.inactive */
		atom->ull = page_count_to_kb(mach_vmstat.inactive_count);
		return 1;
	case 22: /* mem.util.free */
		atom->ull = page_count_to_kb(mach_vmstat.free_count);
		return 1;
	case 23: /* mem.util.used */
		atom->ull = page_count_to_kb(mach_vmstat.wire_count+mach_vmstat.active_count+mach_vmstat.inactive_count);
		return 1;
	case 24: /* swap.length */
		if (mach_swap_error)
			return mach_swap_error;
		atom->ull = mach_swap.xsu_total >> 10;  // bytes to KB
		return 1;
	case 25: /* swap.used */
		if (mach_swap_error)
			return mach_swap_error;
		atom->ull = mach_swap.xsu_used >> 10;   // bytes to KB
		return 1;
	case 26: /* swap.free */
		if (mach_swap_error)
			return mach_swap_error;
		atom->ull = mach_swap.xsu_avail >> 10;  // bytes to KB
		return 1;
	case 130: /* mem.util.compressed */
		atom->ull = page_count_to_kb(mach_vmstat.compressor_page_count);
		return 1;
	}
	return PM_ERR_PMID;
}
