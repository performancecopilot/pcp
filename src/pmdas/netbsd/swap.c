/*
 * NetBSD Kernel PMDA - swap metrics
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
#include "netbsd.h"
#include <sys/swap.h>
#include <errno.h>
#include <string.h>

static int		nref;
static struct swapent	*stats;
static int		ndev = -1;

void
refresh_swap_metrics(void)
{
    int		sts;

    /* always succeeds ... at least as per man swapctl(2) */
    sts = swapctl(SWAP_NSWAP, NULL, 0);
    if (sts != ndev) {
	if (stats != NULL)
	    free(stats);
	stats = (struct swapent *)malloc(sts*sizeof(stats[0]));
	if (stats == NULL) {
	    __pmNoMem("refresh_swap_metrics", sts*sizeof(stats[0]), PM_FATAL_ERR);
	    /* NOTREACHED */
	}
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_APPL0) {
	    if (ndev == -1)
		fprintf(stderr, "Info: refresh_swap_metrics: initial ndev=%d\n", sts);
	    else
		fprintf(stderr, "Info: refresh_swap_metrics: ndev changed from %d to %d\n", ndev, sts);
	}
#endif
	ndev = sts;
    }
    sts = swapctl(SWAP_STATS, (void *)stats, ndev);
    if (sts != ndev) {
	fprintf(stderr, "refresh_swap_metrics: swapctl(SWAP_STATS, ...) returns %d not %d", sts, ndev);
	if (sts < 0)
	    fprintf(stderr, " %s", strerror(errno));
	fputc('\n', stderr);
	ndev = 0;
    }
    nref++;
}

int
do_swap_metrics(pmdaMetric *mdesc, unsigned int inst, pmAtomValue *atom)
{
    int		sts;
    int		i;

    if (inst == PM_IN_NULL) {
	atom->ull = 0;
	sts = 1;
	/* cluster and domain already checked, just need item ... */
	switch (pmid_item(mdesc->m_desc.pmid)) {
	    case 1:		/* swap.length */
		atom->ull = 0;
		for (i = 0; i < ndev; i++)
		    atom->ull += stats[i].se_nblks;
		atom->ull = atom->ull / 2;	/* 512-byte -> Kb */
		break;

	    case 2:		/* swap.used */
		atom->ull = 0;
		for (i = 0; i < ndev; i++)
		    atom->ull += stats[i].se_inuse;
		atom->ull = atom->ull / 2;	/* 512-byte -> Kb */
		break;

	    case 3:		/* swap.free */
		atom->ull = 0;
		for (i = 0; i < ndev; i++)
		    atom->ull += stats[i].se_nblks - stats[i].se_inuse;
		atom->ull = atom->ull / 2;	/* 512-byte -> Kb */
		break;

	    case 4:		/* swap.in */
	    case 5:		/* swap.out */
	    case 6:		/* swap.pagesin */
	    case 7:		/* swap.pagesout */
		atom->ull = ndev*100 + nref;
		break;

	    default:
		sts = PM_ERR_PMID;
		break;
	}
    }
    else {
	/*
	 * swap metrics don't have an instance domain ... yet
	 * TODO - swapctl() returns per-swap-dev metrics so we could
	 * expose these.
	 */
	sts = PM_ERR_INST;
    }

    return sts;
}
