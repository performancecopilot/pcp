/*
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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <mach/mach.h>
#include "pmapi.h"
#include "pmda.h"

#include "darwin.h"

extern mach_port_t	mach_host;
extern int		mach_hertz;

int
refresh_vmstat(struct vm_statistics *vmstat)
{
    int error, info = HOST_VM_INFO;
    natural_t count = HOST_VM_INFO_COUNT;

    error = host_statistics(mach_host, info, (host_info_t)vmstat, &count);
    return (error != KERN_SUCCESS) ? -oserror() : 0;
}

int
refresh_cpuload(struct host_cpu_load_info *cpuload)
{
    int error, info = HOST_CPU_LOAD_INFO;
    natural_t count = HOST_CPU_LOAD_INFO_COUNT;

    error = host_statistics(mach_host, info, (host_info_t)cpuload, &count);
    return (error != KERN_SUCCESS) ? -oserror() : 0;
}

int
refresh_uname(struct utsname *utsname)
{
    return (uname(utsname) == -1) ? -oserror() : 0;
}

int
refresh_hertz(unsigned int *hertz)
{
    int			mib[2] = { CTL_KERN, KERN_CLOCKRATE };
    size_t		size = sizeof(struct clockinfo);
    struct clockinfo	clockrate;

    if (sysctl(mib, 2, &clockrate, &size, NULL, 0) == -1)
	return -oserror();
    *hertz = clockrate.hz;
    return 0;
}

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
refresh_uptime(unsigned int *uptime)
{
    static struct timeval	boottime;
    struct timeval		timediff;

    if (!boottime.tv_sec) {
	int	mib[2] = { CTL_KERN, KERN_BOOTTIME };
	size_t	size = sizeof(struct timeval);

	if (sysctl(mib, 2, &boottime, &size, NULL, 0) == -1)
	    return -oserror();
    }

    pmtimevalNow(&timediff);
    pmtimevalDec(&timediff, &boottime);
    *uptime = timediff.tv_sec;
    return 0;
}

int
refresh_cpus(struct processor_cpu_load_info **cpuload, pmdaIndom *indom)
{
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
refresh_hinv(void)
{
    int			mib[2] = { CTL_HW, HW_MODEL };
    size_t		size = MODEL_SIZE;

    if (sysctl(mib, 2, hw_model, &size, NULL, 0) == -1)
	return -oserror();
    return 0;
#if 0
sysctl...others
hw.machine = Power Macintosh
hw.model = PowerMac4,2
hw.busfrequency = 99837332
hw.cpufrequency = 700000000
hw.cachelinesize = 32
hw.l1icachesize = 32768
hw.l1dcachesize = 32768
hw.l2settings = 2147483648
hw.l2cachesize = 262144
#endif
}
