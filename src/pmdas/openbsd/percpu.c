/*
 * OpenBSD Kernel PMDA - percpu cpu metrics
 *
 * Copyright (c) 2015 Ken McDonell.  All Rights Reserved.
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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * TODO
 *   +  no support for changes in ncpu (or indeed hotplug cpus), so
 *	ncpu at the start stays fixed for the life of the PMDA
 *   +  sysctl [CTL_KERN,KERN_CP_ID] returns a "Mapping of CPU number
 *	to CPU id" ... we assume number {i} => cpu{i}
 *   +  wait time? here and for "all"
 */

#include "pmapi.h"
#include "pmda.h"
#include "openbsd.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/sched.h>
#include <errno.h>
#include <string.h>

static uint64_t		*stats;
static int		valid;

void
refresh_percpu_metrics(void)
{
    int		sts;
    static int	name[] = { CTL_KERN, KERN_CPTIME };
    u_int	namelen = sizeof(name) / sizeof(name[0]);
    size_t	buflen = ncpu*CPUSTATES*sizeof(uint64_t);

    if (stats == NULL) {
	/* initialization */
	if (pmDebugOptions.appl0)
	    fprintf(stderr, "Info: refresh_percpu_metrics: ncpu=%d\n", ncpu);
	stats = (uint64_t *)malloc(buflen);
	if (stats == NULL) {
	    pmNoMem("refresh_percpu_metrics: stats", buflen, PM_FATAL_ERR);
	    /* NOTREACHED */
	}
    }
    /* fetch all the available data */
    if ((sts = sysctl(name, namelen, stats, &buflen, NULL, 0)) != 0) {
	fprintf(stderr, "refresh_percpu_metrics: stats sysctl(): %s\n", strerror(errno));
	valid = 0;
	return;
    }

    valid = 1;
}

int
do_percpu_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int		sts;

    if (!valid)
	return 0;

    if (inst != PM_IN_NULL) {
	/*
	 * per-cpu metrics
	 */
	if (inst < 0 || inst >= ncpu) {
	    return PM_ERR_INST;
	}
	sts = 1;
	/* cluster and domain already checked, just need item ... */
	switch (pmID_item(mdesc->m_desc.pmid)) {

	    case 3:		/* kernel.percpu.cpu.user */
		atom->ull = 1000 * stats[inst*CPUSTATES+CP_USER] / cpuhz;
		break;

	    case 4:		/* kernel.percpu.cpu.nice */
		atom->ull = 1000 * stats[inst*CPUSTATES+CP_NICE] / cpuhz;
		break;

	    case 5:		/* kernel.percpu.cpu.sys */
		atom->ull = 1000 * stats[inst*CPUSTATES+CP_SYS] / cpuhz;
		break;

	    case 6:		/* kernel.percpu.cpu.intr */
		atom->ull = 1000 * stats[inst*CPUSTATES+CP_INTR] / cpuhz;
		break;

	    case 7:		/* kernel.percpu.cpu.idle */
		atom->ull = 1000 * stats[inst*CPUSTATES+CP_IDLE] / cpuhz;
		break;

	    default:
		sts = PM_ERR_PMID;
		break;
	}
    }
    else {
	/*
	 * should not get here for kernel.all.cpu metrics ... they
	 * are instantiated directly from sysctl() in openbsd.c
	 */
	sts = PM_ERR_INST;
    }

    return sts;
}
