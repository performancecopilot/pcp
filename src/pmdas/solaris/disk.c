/*
 * Copyright (c) 2004 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2010 Max Matveev.  All Rights Reserved.
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

#define SOLARIS_PMDA_TRACE (DBG_TRACE_APPL0|DBG_TRACE_APPL2)

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
    kstat_io_t	iostat;
    kstat_t	*sderr;
    int		sderr_fresh;
} ctl_t;

static ctl_t *
getDiskCtl(pmInDom dindom, const char *name)
{
    ctl_t *ctl = NULL;
    int inst;
    int rv = pmdaCacheLookupName(dindom,name, &inst, (void **)&ctl);

    if (rv == PMDA_CACHE_ACTIVE)
	return ctl;

    if ((rv == PMDA_CACHE_INACTIVE) && ctl) {
	rv = pmdaCacheStore(dindom, PMDA_CACHE_ADD, name, ctl);
	if (rv < 0) {
	    __pmNotifyErr(LOG_WARNING,
			  "Cannot reactivate cached data for disk '%s': %s\n",
			  name, pmErrStr(rv));
	    return NULL;
	}
    } else {
	if ((ctl = (ctl_t *)calloc(1, sizeof(ctl_t))) == NULL) {
	    __pmNotifyErr(LOG_WARNING,
			  "Out of memory to keep state for disk '%s'\n",
			  name);
	   return NULL;
	}

	rv = pmdaCacheStore(dindom, PMDA_CACHE_ADD, name, ctl);
	if (rv < 0) {
	    __pmNotifyErr(LOG_WARNING,
			  "Cannot cache data for disk '%s': %s\n",
			  name, pmErrStr(rv));
	    free(ctl);
	    return NULL;
	}
    }
    return ctl;
}

static void
disk_walk_chains(pmInDom dindom)
{
    kstat_t	*ksp;

    for (ksp = kc->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
	ctl_t *ctl;

	if ((strcmp(ksp->ks_class, "disk") == 0) &&
	    (ksp->ks_type == KSTAT_TYPE_IO)) {
	    if ((ctl = getDiskCtl(dindom, ksp->ks_name)) == NULL)
		continue;

	    ctl->ksp = ksp;
	    ctl->fetched = 0;
	} else if (strcmp(ksp->ks_class, "device_error") == 0) {
	    char *comma;
	    char modname[KSTAT_STRLEN];

	    strcpy(modname, ksp->ks_name);
	    if ((comma = strchr(modname, ',')) == NULL)
		continue;

	    *comma = '\0';
	    if ((ctl = getDiskCtl(dindom, modname)) == NULL)
		    continue;
	    ctl->sderr = ksp;
	    ctl->sderr_fresh = 0;
	}
    }
}

void
disk_init(int first)
{
    pmInDom dindom = indomtab[DISK_INDOM].it_indom;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    pmdaCacheOp(dindom, PMDA_CACHE_LOAD);
    disk_walk_chains(dindom);
    pmdaCacheOp(dindom, PMDA_CACHE_SAVE);
}

void
disk_prefetch(void)
{
    pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_INACTIVE);
    disk_walk_chains(indomtab[DISK_INDOM].it_indom);
    pmdaCacheOp(indomtab[DISK_INDOM].it_indom, PMDA_CACHE_SAVE);
}

static __uint64_t
disk_derived(pmdaMetric *mdesc, int inst, const kstat_io_t *iostat)
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
	case PMDA_PMID(SCLR_DISK,2):	/* disk.all.total */
	case PMDA_PMID(SCLR_DISK,12):	/* disk.dev.total */
	    val = iostat->reads + iostat->writes;
	    break;

	case PMDA_PMID(SCLR_DISK,5):	/* disk.all.total_bytes */
	case PMDA_PMID(SCLR_DISK,15):	/* disk.dev.total_bytes */
	    val = iostat->nread + iostat->nwritten;
	    break;

	/* iostat->wcnt and iostat->rcnt are 32 bit intergers,
	 * these two metrics must be derived because the metrics
	 * are using 64 bit integers to avoid overflows during
	 * accumultion */
	case PMDA_PMID(SCLR_DISK,7): /* disk.all.wait.count */
	    val = iostat->wcnt;
	    break;
	case PMDA_PMID(SCLR_DISK,9): /* disk.all.run.time */
	    val = iostat->rcnt;
	    break;

	default:
	    fprintf(stderr, "disk_derived: Botch: no method for pmid %s\n",
		pmIDStr(mdesc->m_desc.pmid));
	    val = 0;
	    break;
    }

#ifdef PCP_DEBUG
    if ((pmDebug & SOLARIS_PMDA_TRACE) == SOLARIS_PMDA_TRACE) {
	/* desperate */
	fprintf(stderr, "disk_derived: pmid %s inst %d val %llu\n",
	    pmIDStr(mdesc->m_desc.pmid), inst, (unsigned long long)val);
    }
#endif

    return val;
}

static int
fetch_disk_data(const pmdaMetric *mdesc, ctl_t *ctl, const char *diskname)
{
    if (ctl->fetched == 1)
	return 1;

    if (ctl->ksp == NULL)
	return 0;

    if ((kstat_read(kc, ctl->ksp, &ctl->iostat) == -1)) {
	if (ctl->err == 0) {
	    int e = errno;
	    __pmNotifyErr(LOG_WARNING,
			  "Error: disk_fetch(pmid=%s disk=%s ...) - "
			   "kstat_read(kc=%p, ksp=%p, ...) failed: %s\n",
			   pmIDStr(mdesc->m_desc.pmid), diskname,
			   kc, ctl->ksp, strerror(e));
	    }
	    ctl->err++;
	    ctl->fetched = -1;
	    return 0;
    }

    ctl->fetched = 1;
    if (ctl->err != 0) {
	__pmNotifyErr(LOG_INFO,
		      "Success: disk_fetch(pmid=%s disk=%s ...) "
		      "after %d errors as previously reported\n",
		      pmIDStr(mdesc->m_desc.pmid), diskname, ctl->err);
	ctl->err = 0;
    }

    return 1;
}

static int
get_instance_value(pmdaMetric *mdesc, pmInDom dindom, int inst,
		   pmAtomValue *atom)
{
    ctl_t *ctl;
    char *diskname;
    uint64_t ull;
    ptrdiff_t offset = ((metricdesc_t *)mdesc->m_user)->md_offset;

    if (pmdaCacheLookup(dindom, inst, &diskname,
			(void **)&ctl) != PMDA_CACHE_ACTIVE) {
#ifdef PCP_DEBUG
	if ((pmDebug & SOLARIS_PMDA_TRACE) == SOLARIS_PMDA_TRACE) {
	    fprintf(stderr,
		    "Unexpected cache result - instance %d "
		    "is not active in disk indom cache\n",
		    inst);
	}
#endif
	return 0;
    }

    if (offset == -1) {
	if (!fetch_disk_data(mdesc, ctl, diskname))
	    return 0;
	ull = disk_derived(mdesc, inst, &ctl->iostat);
    } else if (offset > sizeof(ctl->iostat)) { /* device_error */
	if (ctl->sderr) {
	    kstat_named_t *kn;
	    char * m = (char *)offset;

	    if (!ctl->sderr_fresh) {
		ctl->sderr_fresh = (kstat_read(kc, ctl->sderr, NULL) != -1);

		if (!ctl->sderr_fresh)
		    return 0;
	    }

	    if ((kn = kstat_data_lookup(ctl->sderr, m)) == NULL) {
#ifdef PCP_DEBUG
		if ((pmDebug & SOLARIS_PMDA_TRACE) == SOLARIS_PMDA_TRACE)
		    fprintf(stderr, "No %s in %s\n", m, diskname);
#endif
		return 0;
	    }

	    return kstat_named_to_pmAtom(kn, atom);
	}
	return 0;
    } else {
	char *iop = ((char *)&ctl->iostat) + offset;
	if (!fetch_disk_data(mdesc, ctl, diskname))
	    return 0;
	if (mdesc->m_desc.type == PM_TYPE_U64) {
	    __uint64_t *ullp = (__uint64_t *)iop;
	    ull = *ullp;
#ifdef PCP_DEBUG
	    if ((pmDebug & SOLARIS_PMDA_TRACE) == SOLARIS_PMDA_TRACE) {
		/* desperate */
		fprintf(stderr, "disk_fetch: pmid %s inst %d val %llu\n",
			pmIDStr(mdesc->m_desc.pmid), inst,
			(unsigned long long)*ullp);
	    }
#endif
	}
	else {
	    __uint32_t *ulp = (__uint32_t *)iop;
	    ull = *ulp;
#ifdef PCP_DEBUG
	    if ((pmDebug & SOLARIS_PMDA_TRACE) == SOLARIS_PMDA_TRACE) {
		/* desperate */
		fprintf(stderr, "disk_fetch: pmid %s inst %d val %u\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, *ulp);
	    }
#endif
	}
    }

    if (mdesc->m_desc.type == PM_TYPE_U64) {
	/* export as 64-bit value */
	atom->ull += ull;
    }
    else {
	/* else export as a 32-bit */
	atom->ul += (__uint32_t)ull;
    }

    return 1;
}

int
disk_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    int	i;
    pmInDom dindom = indomtab[DISK_INDOM].it_indom;

    if (pmid_item(mdesc->m_desc.pmid) == 20) { /* hinv.ndisk */
	i = pmdaCacheOp(dindom, PMDA_CACHE_SIZE_ACTIVE);
	if (i < 0) {
		return 0;
	} else {
		atom->ul = i;
		return 1;
	}
    }

    memset(atom, 0, sizeof(*atom));

    if (inst == PM_IN_NULL) {
	pmdaCacheOp(dindom,PMDA_CACHE_WALK_REWIND);
	while ((i = pmdaCacheOp(dindom, PMDA_CACHE_WALK_NEXT)) != -1) {
	    if (get_instance_value(mdesc, dindom, i, atom) == 0)
		return 0;
	}
	return 1;
    }

    return get_instance_value(mdesc, dindom, inst, atom);
}
