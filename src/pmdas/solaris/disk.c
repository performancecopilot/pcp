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
 */

#include "common.h"

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
} ctl_t;

static int		ndisk;
static kstat_io_t	*iostat;
static ctl_t		*ctl;

void
disk_init(int first)
{
    kstat_t	*ksp;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    /*
     * TODO ... add prelim pass to build indom, sort by name, then
     * scan indom to match by name for each kstat in the second
     * pass
     */

    ndisk = 0;
    for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
	if (strcmp(ksp->ks_class, "disk") != 0) continue;
	if (ksp->ks_type != KSTAT_TYPE_IO) continue;
	if ((ctl = (ctl_t *)realloc(ctl, (ndisk+1) * sizeof(ctl_t))) == NULL) {
	    fprintf(stderr, "disk_init: ctl realloc[%d] @ disk=%d failed: %s\n",
		(int)((ndisk+1) * sizeof(ctl_t)), ndisk, strerror(errno));
	    exit(1);
	}
	if ((iostat = (kstat_io_t *)realloc(iostat, (ndisk+1) * sizeof(kstat_io_t))) == NULL) {
	    fprintf(stderr, "disk_init: iostat realloc[%d] @ disk=%d failed: %s\n",
		(int)((ndisk+1) * sizeof(kstat_io_t)), ndisk, strerror(errno));
	    exit(1);
	}
	ctl[ndisk].ksp = ksp;
	ctl[ndisk].err = 0;
	indomtab[DISK_INDOM].it_numinst = ndisk+1;
	indomtab[DISK_INDOM].it_set = (pmdaInstid *)realloc(indomtab[DISK_INDOM].it_set, (ndisk+1) * sizeof(pmdaInstid));
	/* TODO check? */
	indomtab[DISK_INDOM].it_set[ndisk].i_inst = ndisk;
	indomtab[DISK_INDOM].it_set[ndisk].i_name = strdup(ksp->ks_name);
	/* TODO check? */
	ndisk++;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "disk_init: ndisk=%d\n", ndisk);
    }
#endif
}

void
disk_prefetch(void)
{
    int		i;

    for (i = 0; i < ndisk; i++)
	ctl[i].fetched = 0;
}

static __uint64_t
disk_derived(pmdaMetric *mdesc, int inst)
{
    pmID	pmid;
    __pmID_int	*ip = (__pmID_int *)&pmid;
    __uint64_t	val;

    pmid = mdesc->m_desc.pmid;
    ip->domain = 0;

// from kstat_io_t ...
//
// u_longlong_t	nread;		/* number of bytes read */
// u_longlong_t	nwritten;	/* number of bytes written */
// uint_t	reads;		/* number of read operations */
// uint_t	writes;		/* number of write operations */
//

    switch (pmid) {
	case PMDA_PMID(0,46):	/* disk.all.total */
	case PMDA_PMID(0,52):	/* disk.dev.total */
	    val = iostat[inst].reads + iostat[inst].writes;
	    break;

	case PMDA_PMID(0,49):	/* disk.all.total_bytes */
	case PMDA_PMID(0,55):	/* disk.dev.total_bytes */
	    val = iostat[inst].nread + iostat[inst].nwritten;
	    break;

	case PMDA_PMID(0,57):	/* hinv.ndisk */
	    if (inst == 0)
		val = ndisk;
	    else
		val = 0;
	    break;

	default:
	    fprintf(stderr, "disk_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
	/* desperate */
	fprintf(stderr, "disk_derived: pmid %s inst %d val %llu\n",
	    pmIDStr(mdesc->m_desc.pmid), inst, (unsigned long long)val);
    }
#endif

    return val;
}


int
disk_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    __uint64_t		ull;
    int			i;
    int			ok;
    int			offset;

    ok = 1;
    for (i = 0; i < ndisk; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    if (ctl[i].fetched == 1)
		continue;
	    if (kstat_read(kc, ctl[i].ksp, &iostat[i]) == -1) {
		if (ctl[i].err == 0) {
		    fprintf(stderr, "Error: disk_fetch(pmid=%s disk=%d ...)\n", pmIDStr(mdesc->m_desc.pmid), i);
		    fprintf(stderr, "kstat_read(kc=%p, ksp=%p, ...) failed: %s\n", kc, ctl[i].ksp, strerror(errno));
		}
		ctl[i].err++;
		ctl[i].fetched = -1;
		ok = 0;
	    }
	    else {
		ctl[i].fetched = 1;
		if (ctl[i].err != 0) {
		    fprintf(stderr, "Success: disk_fetch(pmid=%s disk=%d ...) after %d errors as previously reported\n",
			pmIDStr(mdesc->m_desc.pmid), i, ctl[i].err);
		    ctl[i].err = 0;
		}
	    }
	}
    }

    if (!ok)
	return 0;

    ull = 0;
    for (i = 0; i < ndisk; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
	    if (offset < 0) {
		ull += disk_derived(mdesc, i);
	    }
	    else {
		if (mdesc->m_desc.type == PM_TYPE_U64) {
		    __uint64_t		*ullp;
		    ullp = (__uint64_t *)&((char *)&iostat[i])[offset];
		    ull += *ullp;
#ifdef PCP_DEBUG
		    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
			/* desperate */
			fprintf(stderr, "disk_fetch: pmid %s inst %d val %llu\n",
			    pmIDStr(mdesc->m_desc.pmid), i,
			    (unsigned long long)*ullp);
		    }
#endif
		}
		else {
		    __uint32_t		*ulp;
		    ulp = (__uint32_t *)&((char *)&iostat[i])[offset];
		    ull += *ulp;
#ifdef PCP_DEBUG
		    if ((pmDebug & (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) == (DBG_TRACE_APPL0|DBG_TRACE_APPL2)) {
			/* desperate */
			fprintf(stderr, "disk_fetch: pmid %s inst %d val %u\n",
			    pmIDStr(mdesc->m_desc.pmid), i, *ulp);
		    }
#endif
		}
	    }
	}
    }

    if (mdesc->m_desc.type == PM_TYPE_U64) {
	/* export as 64-bit value */
	atom->ull = ull;
    }
    else {
	/* else export as a 32-bit */
	atom->ul = (__uint32_t)ull;
    }

    return 1;
}
