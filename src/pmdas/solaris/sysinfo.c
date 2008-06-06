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
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "common.h"

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
} ctl_t;

static int		ncpu;
static int		hz;
static cpu_stat_t	*cpustat;
static ctl_t		*ctl;

void
sysinfo_init(int first)
{
    kstat_t	*ksp;
    int		i;
    char	buf[10];	/* cpuXXXXX */

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    for (ncpu = 0; ; ncpu++) {
	ksp = kstat_lookup(kc, "cpu_stat", ncpu, NULL);
	if (ksp == NULL) break;
	if ((ctl = (ctl_t *)realloc(ctl, (ncpu+1) * sizeof(ctl_t))) == NULL) {
	    fprintf(stderr, "sysinfo_init: ctl realloc[%d] @ cpu=%d failed: %s\n",
		(ncpu+1) * sizeof(ctl_t), ncpu, strerror(errno));
	    exit(1);
	}
	if ((cpustat = (cpu_stat_t *)realloc(cpustat, (ncpu+1) * sizeof(cpu_stat_t))) == NULL) {
	    fprintf(stderr, "sysinfo_init: cpustat realloc[%d] @ cpu=%d failed: %s\n",
		(ncpu+1) * sizeof(cpu_stat_t), ncpu, strerror(errno));
	    exit(1);
	}
	ctl[ncpu].ksp = ksp;
	ctl[ncpu].err = 0;
    }

    indomtab[CPU_INDOM].it_numinst = ncpu;
    indomtab[CPU_INDOM].it_set = (pmdaInstid *)malloc(ncpu * sizeof(pmdaInstid));
    /* TODO check? */

    for (i = 0; i < ncpu; i++) {
	indomtab[CPU_INDOM].it_set[i].i_inst = i;
	snprintf(buf, sizeof(buf), "cpu%d", i);
	indomtab[CPU_INDOM].it_set[i].i_name = strdup(buf);
	/* TODO check? */
    }

    hz = (int)sysconf(_SC_CLK_TCK);

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "sysinfo: ncpu=%d hz=%d\n", ncpu, hz);
    }
#endif
}

static __uint32_t
sysinfo_derived(pmdaMetric *mdesc, int inst)
{
    pmID	pmid;
    __pmID_int	*ip = (__pmID_int *)&pmid;
    __uint32_t	val;

    pmid = mdesc->m_desc.pmid;
    ip->domain = 0;

    switch (pmid) {

	case PMDA_PMID(0,56):	/* hinv.ncpu */
	    if (inst == 0)
		val = ncpu;
	    else
		val = 0;
	    break;

	default:
	    fprintf(stderr, "cpu_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "cpu_derived: pmid %s inst %d val %d\n",
	    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }
#endif
    
    return val;
}

void
sysinfo_prefetch(void)
{
    int		i;

    for (i = 0; i < ncpu; i++)
	ctl[i].fetched = 0;
}

int
sysinfo_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    __uint64_t		ull;
    int			i;
    int			ok;
    int			offset;

    ok = 1;
    for (i = 0; i < ncpu; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    if (ctl[i].fetched == 1)
		continue;
	    if (kstat_read(kc, ctl[i].ksp, &cpustat[i]) == -1) {
		if (ctl[i].err == 0) {
		    fprintf(stderr, "Error: sysinfo_fetch(pmid=%s cpu=%d ...)\n", pmIDStr(mdesc->m_desc.pmid), i);
		    fprintf(stderr, "kstat_read(kc=%p, ksp=%p, ...) failed: %s\n", kc, ctl[i].ksp, strerror(errno));
		}
		ctl[i].err++;
		ctl[i].fetched = -1;
		ok = 0;
	    }
	    else {
		ctl[i].fetched = 1;
		if (ctl[i].err != 0) {
		    fprintf(stderr, "Success: sysinfo_fetch(pmid=%s cpu=%d ...) after %d errors as previously reported\n",
			pmIDStr(mdesc->m_desc.pmid), i, ctl[i].err);
		    ctl[i].err = 0;
		}
	    }
	}
    }

    if (!ok)
	return 0;

    ull = 0;
    for (i = 0; i < ncpu; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
	    if (offset < 0) {
		ull += sysinfo_derived(mdesc, i);
	    }
	    else {
		/* all the kstat fields are 32-bit unsigned */
		__uint32_t		*ulp;
		ulp = (__uint32_t *)&((char *)&cpustat[i])[offset];
		ull += *ulp;
#ifdef PCP_DEBUG
		if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
		    /* desperate */
		    fprintf(stderr, "sysinfo_fetch: pmid %s inst %d val %u\n",
			pmIDStr(mdesc->m_desc.pmid), i, *ulp);
		}
#endif
	    }
	}
    }

    if (mdesc->m_desc.units.dimTime == 1) {
	/* sysinfo times are in ticks, and we export as 64-bit msec */
	atom->ull = ull * 1000 / hz;
    }
    else if (mdesc->m_desc.type == PM_TYPE_U64) {
	/* export as 64-bit value */
	atom->ull = ull;
    }
    else {
	/* else export as a 32-bit */
	atom->ul = (__uint32_t)ull;
    }

    return 1;
}
