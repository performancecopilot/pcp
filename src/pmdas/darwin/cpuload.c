/*
 * Darwin PMDA cpuload cluster
 *
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

#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "cpuload.h"

extern mach_port_t mach_host;
extern int mach_cpuload_error;
extern struct host_cpu_load_info mach_cpuload;
extern unsigned int mach_hertz;

int
refresh_cpuload(struct host_cpu_load_info *cpuload)
{
	int error, info = HOST_CPU_LOAD_INFO;
	natural_t count = HOST_CPU_LOAD_INFO_COUNT;

	error = host_statistics(mach_host, info, (host_info_t)cpuload, &count);
	return (error != KERN_SUCCESS) ? -oserror() : 0;
}

int
fetch_cpuload(unsigned int item, pmAtomValue *atom)
{
	if (mach_cpuload_error)
		return mach_cpuload_error;
	switch (item) {
	case 42: /* kernel.all.cpu.user */
		atom->ull = LOAD_SCALE * (double)
			mach_cpuload.cpu_ticks[CPU_STATE_USER] / mach_hertz;
		return 1;
	case 43: /* kernel.all.cpu.nice */
		atom->ull = LOAD_SCALE * (double)
			mach_cpuload.cpu_ticks[CPU_STATE_NICE] / mach_hertz;
		return 1;
	case 44: /* kernel.all.cpu.sys */
		atom->ull = LOAD_SCALE * (double)
			mach_cpuload.cpu_ticks[CPU_STATE_SYSTEM] / mach_hertz;
		return 1;
	case 45: /* kernel.all.cpu.idle */
		atom->ull = LOAD_SCALE * (double)
			mach_cpuload.cpu_ticks[CPU_STATE_IDLE] / mach_hertz;
		return 1;
	}
	return PM_ERR_PMID;
}
