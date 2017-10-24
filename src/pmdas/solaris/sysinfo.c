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
 */

#include "common.h"
#include <sys/utsname.h>
#include <sys/loadavg.h>

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
    cpu_stat_t	cpustat;
    kstat_t	*info;
    int		info_is_good;
} ctl_t;

static int		ncpu;
static int		hz;
static long		pagesize;
static ctl_t		*ctl;
static char		uname_full[SYS_NMLN * 5];
static int		nloadavgs;
static double		loadavgs[3];

void
sysinfo_init(int first)
{
    kstat_t	*ksp;
    int		i;
    char	buf[10];	/* cpuXXXXX */
    kstat_ctl_t *kc;

    if (!first)
	/* TODO ... not sure if/when we'll use this re-init hook */
	return;

    if ((kc = kstat_ctl_update()) == NULL)
	return;

    for (ncpu = 0; ; ncpu++) {
	ksp = kstat_lookup(kc, "cpu_stat", ncpu, NULL);
	if (ksp == NULL) break;
	if ((ctl = (ctl_t *)realloc(ctl, (ncpu+1) * sizeof(ctl_t))) == NULL) {
	    fprintf(stderr, "sysinfo_init: ctl realloc[%d] @ cpu=%d failed: %s\n",
		(int)((ncpu+1) * sizeof(ctl_t)), ncpu, osstrerror());
	    exit(1);
	}
	ctl[ncpu].info = kstat_lookup(kc, "cpu_info", ncpu, NULL);
	ctl[ncpu].ksp = ksp;
	ctl[ncpu].err = 0;
    }

    indomtab[CPU_INDOM].it_numinst = ncpu;
    indomtab[CPU_INDOM].it_set = (pmdaInstid *)malloc(ncpu * sizeof(pmdaInstid));
    /* TODO check? */

    for (i = 0; i < ncpu; i++) {
	indomtab[CPU_INDOM].it_set[i].i_inst = i;
	pmsprintf(buf, sizeof(buf), "cpu%d", i);
	indomtab[CPU_INDOM].it_set[i].i_name = strdup(buf);
	/* TODO check? */
    }

    hz = (int)sysconf(_SC_CLK_TCK);
    pagesize = sysconf(_SC_PAGESIZE);

    if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
	/* desperate */
	fprintf(stderr, "sysinfo: ncpu=%d hz=%d\n", ncpu, hz);
    }
}

static __uint32_t
sysinfo_derived(pmdaMetric *mdesc, int inst)
{
    pmID	pmid = mdesc->m_desc.pmid;
    __pmID_int	*ip = (__pmID_int *)&pmid;
    __uint32_t	val;

    ip->domain = 0;

    switch (pmid) {

	case PMDA_PMID(SCLR_SYSINFO,56):	/* hinv.ncpu */
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

    if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
	/* desperate */
	fprintf(stderr, "cpu_derived: pmid %s inst %d val %d\n",
	    pmIDStr(mdesc->m_desc.pmid), inst, val);
    }

    return val;
}

void
sysinfo_prefetch(void)
{
    int		i;

    nloadavgs = -1;
    for (i = 0; i < ncpu; i++) {
	ctl[i].fetched = 0;
	ctl[i].info_is_good = 0;
    }
}

int
kstat_named_to_pmAtom(const kstat_named_t *kn, pmAtomValue *atom)
{
    static char chardat[sizeof(kn->value.c) + 1];

    switch (kn->data_type) {
    case KSTAT_DATA_UINT64:
	atom->ull = kn->value.ui64;
	return 1;
    case KSTAT_DATA_INT64:
	atom->ull = kn->value.i64;
	return 1;
    case KSTAT_DATA_UINT32:
	atom->ull = kn->value.ui32;
	return 1;
    case KSTAT_DATA_INT32:
	atom->ull = kn->value.i32;
	return 1;
    case KSTAT_DATA_STRING:
	atom->cp = kn->value.str.addr.ptr;
	return 1;
    case KSTAT_DATA_CHAR:
	memcpy(chardat, kn->value.c, sizeof(kn->value.c));
	chardat[sizeof(chardat)-1] = '\0';
	atom->cp = chardat;
	return 1;
    default:
	return 0;
    }
}

static int
kstat_fetch_named(kstat_ctl_t *kc, pmAtomValue *atom, char *metric,
		  int shift_bits)
{
    kstat_t *ks;

    if ((ks = kstat_lookup(kc, "unix", -1, "system_pages")) != NULL) {
	kstat_named_t *kn;

	if (kstat_read(kc, ks, NULL) == -1)
	    return 0;

	if (((kn = kstat_data_lookup(ks, metric)) != NULL) &&
	    kstat_named_to_pmAtom(kn, atom)) {
		atom->ull = (atom->ull * pagesize) >> shift_bits;
		return 1;
	}
    }
    return 0;
}

int
kstat_named_to_typed_atom(const kstat_named_t *kn, int pmtype,
			  pmAtomValue *atom)
{
    static char chardat[sizeof(kn->value.c) + 1];

    switch (pmtype) {
    case PM_TYPE_32:
	if (kn->data_type == KSTAT_DATA_INT32) {
	    atom->l = kn->value.i32;
	    return 1;
	}
	break;
    case PM_TYPE_U32:
	if (kn->data_type == KSTAT_DATA_UINT32) {
	    atom->ul = kn->value.ui32;
	    return 1;
	}
	break;
    case PM_TYPE_64:
	if (kn->data_type == KSTAT_DATA_INT64) {
	    atom->ll = kn->value.i64;
	    return 1;
	}
	break;
    case PM_TYPE_U64:
	if (kn->data_type == KSTAT_DATA_UINT64) {
	    atom->ull = kn->value.ui64;
	    return 1;
	}
	break;
    case PM_TYPE_STRING:
	switch(kn->data_type) {
	case KSTAT_DATA_STRING:
	    atom->cp = kn->value.str.addr.ptr;
	    return 1;
	case KSTAT_DATA_CHAR:
	    memcpy(chardat, kn->value.c, sizeof(kn->value.c));
	    chardat[sizeof(chardat)-1] = '\0';
	    atom->cp = chardat;
	    return 1;
	}
	break;
    }
    return 0;
}

int
sysinfo_fetch(pmdaMetric *mdesc, int inst, pmAtomValue *atom)
{
    __uint64_t		ull;
    int			i;
    int			ok;
    ptrdiff_t		offset;
    struct utsname	u;
    kstat_ctl_t		*kc;

    if ((kc = kstat_ctl_update()) == NULL)
	return 0;

    /* Special processing of metrics which notionally belong
     * to sysinfo category */
    switch (pmid_item(mdesc->m_desc.pmid)) {
    case 109: /* hinv.physmem */
	return kstat_fetch_named(kc, atom, "physmem", 20);
    case 136: /* mem.physmem */
	return kstat_fetch_named(kc, atom, "physmem", 10);
    case 137: /* mem.freemem */
	return kstat_fetch_named(kc, atom, "freemem", 10);
    case 138: /* mem.lotsfree */
	return kstat_fetch_named(kc, atom, "lotsfree", 10);
    case 139: /* mem.availrmem */
	return kstat_fetch_named(kc, atom, "availrmem", 10);

    case 108: /* hinv.pagesize */
	atom->ul = pagesize;
	return 1;

    case 107: /* pmda.uname */
	if (uname(&u) < 0)
	    return 0;

	pmsprintf(uname_full, sizeof(uname_full), "%s %s %s %s %s",
		 u.sysname, u.nodename, u.release, u.version, u.machine);
	atom->cp = uname_full;
	return 1;
    case 135: /* kernel.all.load */
	if (nloadavgs < 0) {
		if ((nloadavgs = getloadavg(loadavgs, 3)) < 0)
			return 0;
	}

	switch (inst) {
	case 1:
		atom->f = (float)loadavgs[LOADAVG_1MIN];
		return nloadavgs > LOADAVG_1MIN;
	case 5:
		atom->f = (float)loadavgs[LOADAVG_5MIN];
		return nloadavgs > LOADAVG_5MIN;
	case 15:
		atom->f = (float)loadavgs[LOADAVG_15MIN];
		return nloadavgs > LOADAVG_15MIN;
	}
	return PM_ERR_INST;
    }

    ok = 1;
    for (i = 0; i < ncpu; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    if (!ctl[i].info_is_good) {
		ctl[i].info_is_good = (ctl[i].info &&
					(kstat_read(kc, ctl[i].info,
						    NULL) != -1));
	    }
	    if (ctl[i].fetched == 1)
		continue;
	    if (kstat_read(kc, ctl[i].ksp, &ctl[i].cpustat) == -1) {
		if (ctl[i].err == 0) {
		    fprintf(stderr, "Error: sysinfo_fetch(pmid=%s cpu=%d ...)\n", pmIDStr(mdesc->m_desc.pmid), i);
		    fprintf(stderr, "kstat_read(kc=%p, ksp=%p, ...) failed: %s\n", kc, ctl[i].ksp, osstrerror());
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

	    if (offset == -1) {
		ull += sysinfo_derived(mdesc, i);
	    } else if (offset > sizeof(ctl[i].cpustat)) {
		char *stat = (char *)offset;
                kstat_named_t *kn;

		if (!ctl[i].info_is_good)
			return 0;

		if ((kn = kstat_data_lookup(ctl[i].info, stat)) == NULL) {
		    fprintf(stderr, "No kstat called %s for CPU %d\n", stat, i);
		    return 0;
		}

		return kstat_named_to_typed_atom(kn, mdesc->m_desc.type, atom);
	    } else {
		/* all the kstat fields are 32-bit unsigned */
		__uint32_t		*ulp;
		ulp = (__uint32_t *)&((char *)&ctl[i].cpustat)[offset];
		ull += *ulp;
		if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
		    /* desperate */
		    fprintf(stderr, "sysinfo_fetch: pmid %s inst %d val %u\n",
			pmIDStr(mdesc->m_desc.pmid), i, *ulp);
		}
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
