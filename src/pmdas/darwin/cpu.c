/*
 * CPU statistics
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
#include <stdlib.h>
#include <string.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"
#include "cpu.h"
#include "darwin.h"

int
refresh_cpus(struct processor_cpu_load_info **cpuload, pmdaIndom *indom)
{
	extern mach_port_t mach_host;
	natural_t ncpu, icount;
	processor_info_array_t iarray;
	struct processor_cpu_load_info *cpuinfo;
	int error, i, info = PROCESSOR_CPU_LOAD_INFO;

	error = host_processor_info(mach_host, info, &ncpu, &iarray, &icount);
	if (error != KERN_SUCCESS)
		return -oserror();

	cpuinfo = (struct processor_cpu_load_info *)iarray;
	if (ncpu != indom->it_numinst) {
		char	name[16];	/* 8 is real max atm, but be conservative */

		error = -ENOMEM;
		i = sizeof(unsigned long) * CPU_STATE_MAX * ncpu;
		if ((*cpuload = realloc(*cpuload, i)) == NULL)
			goto vmdealloc;

		i = sizeof(pmdaInstid) * ncpu;
		if ((indom->it_set = realloc(indom->it_set, i)) == NULL) {
			free(*cpuload);
			*cpuload = NULL;
			indom->it_numinst = 0;
			goto vmdealloc;
		}

		for (i = 0; i < ncpu; i++) {
			pmsprintf(name, sizeof(name), "cpu%d", i);
			indom->it_set[i].i_name = strdup(name);
			indom->it_set[i].i_inst = i;
		}
		indom->it_numinst = ncpu;
	}

	error = 0;
	for (i = 0; i < ncpu; i++)
		memcpy(&(*cpuload)[i], &cpuinfo[i],
			sizeof(unsigned long) * CPU_STATE_MAX);

vmdealloc:
	vm_deallocate(mach_host, (vm_address_t)iarray, icount);
	return error;
}

int
fetch_cpu(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	extern struct processor_cpu_load_info *mach_cpu;
	extern int mach_cpu_error;
	extern pmdaIndom indomtab[];
	extern unsigned int mach_hertz;

	if (mach_cpu_error)
		return mach_cpu_error;
	if (item == 71) {	/* hinv.ncpu */
		atom->ul = indomtab[CPU_INDOM].it_numinst;
		return 1;
	}
	if (indomtab[CPU_INDOM].it_numinst == 0)	/* uh-huh. */
		return 0;	/* no values available */
	if (inst < 0 || inst >= indomtab[CPU_INDOM].it_numinst)
		return PM_ERR_INST;
	switch (item) {
	case 72: /* kernel.percpu.cpu.user */
		atom->ull = LOAD_SCALE * (double)
				mach_cpu[inst].cpu_ticks[CPU_STATE_USER] / mach_hertz;
		return 1;
	case 73: /* kernel.percpu.cpu.nice */
		atom->ull = LOAD_SCALE * (double)
				mach_cpu[inst].cpu_ticks[CPU_STATE_NICE] / mach_hertz;
		return 1;
	case 74: /* kernel.percpu.cpu.sys */
		atom->ull = LOAD_SCALE * (double)
				mach_cpu[inst].cpu_ticks[CPU_STATE_SYSTEM] / mach_hertz;
		return 1;
	case 75: /* kernel.percpu.cpu.idle */
		atom->ull = LOAD_SCALE * (double)
				mach_cpu[inst].cpu_ticks[CPU_STATE_IDLE] / mach_hertz;
		return 1;
	}
	return PM_ERR_PMID;
}
