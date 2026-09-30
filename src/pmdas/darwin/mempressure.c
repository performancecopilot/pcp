/*
 * Memory pressure statistics
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
#include "pmapi.h"
#include "pmda.h"
#include "mempressure.h"

/* kern.memorystatus_vm_pressure_level values (DISPATCH_MEMORYPRESSURE_*) */
#define PRESSURE_NORMAL		1
#define PRESSURE_WARNING	2
#define PRESSURE_CRITICAL	4

int
refresh_mempressure(mempressurestats_t *pressure)
{
    size_t size;

    size = sizeof(pressure->level);
    if (sysctlbyname("kern.memorystatus_vm_pressure_level", &pressure->level, &size, NULL, 0) == -1)
	return -oserror();

    size = sizeof(pressure->available);
    if (sysctlbyname("kern.memorystatus_level", &pressure->available, &size, NULL, 0) == -1)
	return -oserror();

    return 0;
}

static char *
pressure_state_name(__uint32_t level)
{
    switch (level) {
    case PRESSURE_NORMAL:	return "Normal";
    case PRESSURE_WARNING:	return "Warning";
    case PRESSURE_CRITICAL:	return "Critical";
    }
    return "Unknown";
}

int
fetch_mempressure(unsigned int item, pmAtomValue *atom)
{
    extern mempressurestats_t mach_mempressure;
    extern int mach_mempressure_error;

    if (mach_mempressure_error)
	return mach_mempressure_error;
    switch (item) {
    case 1: /* mem.pressure.state */
	atom->cp = pressure_state_name(mach_mempressure.level);
	return 1;
    }
    return PM_ERR_PMID;
}
