/*
 * Darwin PMDA loadavg cluster
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

#include <sys/sysctl.h>
#include "pmapi.h"
#include "pmda.h"
#include "darwin.h"
#include "loadavg.h"

extern int mach_loadavg_error;
extern float mach_loadavg[3];

int
refresh_loadavg(float *loadavg)
{
	int			mib[2] = { CTL_VM, VM_LOADAVG };
	size_t		size = sizeof(struct loadavg);
	struct loadavg	loadavgs;

	if (sysctl(mib, 2, &loadavgs, &size, NULL, 0) == -1)
		return -oserror();
	loadavg[0] = (float)loadavgs.ldavg[0] / (float)loadavgs.fscale;
	loadavg[1] = (float)loadavgs.ldavg[1] / (float)loadavgs.fscale;
	loadavg[2] = (float)loadavgs.ldavg[2] / (float)loadavgs.fscale;
	return 0;
}

int
fetch_loadavg(unsigned int item, unsigned int inst, pmAtomValue *atom)
{
	if (mach_loadavg_error)
		return mach_loadavg_error;
	switch (item) {
	case 30:  /* kernel.all.load */
		if (inst == 1)
			atom->f = mach_loadavg[0];
		else if (inst == 5)
			atom->f = mach_loadavg[1];
		else if (inst == 15)
			atom->f = mach_loadavg[2];
		else
			return PM_ERR_INST;
		return 1;
	}
	return PM_ERR_PMID;
}
