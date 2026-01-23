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
refresh_hinv(void)
{
    int			mib[2] = { CTL_HW, HW_MODEL };
    size_t		size = MODEL_SIZE;

    if (sysctl(mib, 2, hw_model, &size, NULL, 0) == -1)
	return -oserror();
    return 0;

}
