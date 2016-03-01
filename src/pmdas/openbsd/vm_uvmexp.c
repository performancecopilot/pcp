/*
 * OpenBSD Kernel PMDA - metrics from vm.uvmexp sysctl()
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

#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "openbsd.h"
#include <sys/param.h>
#include <sys/sysctl.h>
#include <uvm/uvmexp.h>
#include <errno.h>
#include <string.h>

static int valid;
static struct uvmexp stats;

void
refresh_vm_uvmexp_metrics(void)
{
    int		sts;
    static int	name[] = { CTL_VM, VM_UVMEXP };
    u_int	namelen = sizeof(name) / sizeof(name[0]);
    size_t	buflen = sizeof(stats);

    sts = sysctl(name, namelen, &stats, &buflen, NULL, 0);
    if (sts != 0) {
	fprintf(stderr, "refresh_vm_uvmexp_metrics: sysctl(): %s\n", strerror(errno));
	valid = 0;
    }
    else
	valid = 1;
}

int
do_vm_uvmexp_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int		sts;

    if (inst == PM_IN_NULL) {
	if (valid == 0)
	    /* refesh failed */
	    return PM_ERR_AGAIN;

	sts = 1;
	/* cluster and domain already checked, just need item ... */
	switch (pmid_item(mdesc->m_desc.pmid)) {
	    case 1:		/* kernel.all.pswitch */
		atom->ull = stats.swtch;
		break;

	    case 2:		/* kernel.all.syscall */
		atom->ull = stats.syscalls;
		break;

	    case 3:		/* kernel.all.intr */
		atom->ull = stats.intrs;
		break;

	    case 4:		/* mem.util.all */
		atom->ul = ((int64_t)stats.npages*stats.pagesize+512)/1024;
		break;

	    case 5:		/* mem.util.used */
		atom->ul = ((int64_t)(stats.npages-stats.free)*stats.pagesize+512)/1024;
		break;

	    case 6:		/* mem.util.free */
		atom->ul = ((int64_t)stats.free*stats.pagesize+512)/1024;
		break;

	    case 7:		/* mem.util.paging */
		atom->ul = ((int64_t)stats.paging*stats.pagesize+512)/1024;
		break;

	    case 8:		/* mem.util.cached */
		atom->ul = stats.vnodepages + stats.vtextpages;
		atom->ul = ((int64_t)atom->ul*stats.pagesize+512)/1024;
		break;

	    case 9:		/* mem.util.wired */
		atom->ul = ((int64_t)stats.wired*stats.pagesize+512)/1024;
		break;

	    case 10:		/* mem.util.active */
		atom->ul = ((int64_t)stats.active*stats.pagesize+512)/1024;
		break;

	    case 11:		/* mem.util.inactive */
		atom->ul = ((int64_t)stats.inactive*stats.pagesize+512)/1024;
		break;

	    case 12:		/* mem.util.zeropages */
		atom->ul = ((int64_t)stats.zeropages*stats.pagesize+512)/1024;
		break;

	    case 13:		/* mem.util.pagedaemonpages */
		atom->ul = ((int64_t)stats.reserve_pagedaemon*stats.pagesize+512)/1024;
		break;

	    case 14:		/* mem.util.kernelpages */
		atom->ul = ((int64_t)stats.reserve_kernel*stats.pagesize+512)/1024;
		break;

	    case 15:		/* mem.util.anonpages */
		atom->ul = ((int64_t)stats.anonpages*stats.pagesize+512)/1024;
		break;

	    case 16:		/* mem.util.vnodepages */
		atom->ul = ((int64_t)stats.vnodepages*stats.pagesize+512)/1024;
		break;

	    case 17:		/* mem.util.vtextpages */
		atom->ul = ((int64_t)stats.vtextpages*stats.pagesize+512)/1024;
		break;

	    default:
		sts = PM_ERR_PMID;
		break;
	}
    }
    else {
	/*
	 * these metrics don't have an instance domain
	 */
	sts = PM_ERR_INST;
    }

    return sts;
}
