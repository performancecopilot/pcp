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

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
    kstat_io_t	iostat;
    const char	*diskname;
    kstat_t	*sderr;
} ctl_t;

static int		ndisk;
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
	char errmod[32];
	char errname[32];
	if (strcmp(ksp->ks_class, "disk") != 0)
		 continue;
	if (ksp->ks_type != KSTAT_TYPE_IO)
		continue;
	if ((ctl = (ctl_t *)realloc(ctl, (ndisk+1) * sizeof(ctl_t))) == NULL) {
	    fprintf(stderr, "disk_init: ctl realloc[%d] @ disk=%s failed: %s\n",
		    (int)((ndisk+1) * sizeof(ctl_t)), ksp->ks_name,
		    strerror(errno));
	    exit(1);
	}
	ctl[ndisk].ksp = ksp;
	ctl[ndisk].err = 0;
	ctl[ndisk].diskname = strdup(ksp->ks_name);
	/* cmdk uses 'error', sd uses 'err' */
	/* cmdk uses different names of the fields from sd */
	snprintf(errmod, sizeof(errmod), "%serr", ksp->ks_module);
	snprintf(errname, sizeof(errmod), "%s,err", ksp->ks_name);
	ctl[ndisk].sderr = kstat_lookup(kc, errmod, ksp->ks_instance, errname);
	if (ctl[ndisk].sderr == NULL) {
		fprintf(stderr, "Cannot find module %s, instance %d, name %s\n",
			errmod, ksp->ks_instance, errname);
	}
	indomtab[DISK_INDOM].it_numinst = ndisk+1;
	indomtab[DISK_INDOM].it_set = (pmdaInstid *)realloc(indomtab[DISK_INDOM].it_set, (ndisk+1) * sizeof(pmdaInstid));
	/* TODO check? */
	indomtab[DISK_INDOM].it_set[ndisk].i_inst = ndisk;
	indomtab[DISK_INDOM].it_set[ndisk].i_name = (char *)ctl[ndisk].diskname;
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
	case PMDA_PMID(0,46):	/* disk.all.total */
	case PMDA_PMID(0,52):	/* disk.dev.total */
	    val = iostat->reads + iostat->writes;
	    break;

	case PMDA_PMID(0,49):	/* disk.all.total_bytes */
	case PMDA_PMID(0,55):	/* disk.dev.total_bytes */
	    val = iostat->nread + iostat->nwritten;
	    break;

	case PMDA_PMID(0,144): /* disk.all.wait.time */
	    val = iostat->wtime;
	    break;
	case PMDA_PMID(0,145): /* disk.all.wait.count */
	    val = iostat->wcnt;
	    break;
	case PMDA_PMID(0,146): /* disk.all.run.time */
	    val = iostat->rtime;
	    break;
	case PMDA_PMID(0,147): /* disk.all.run.time */
	    val = iostat->rcnt;
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
    ptrdiff_t		offset;

    ok = 1;
    for (i = 0; i < ndisk; i++) {
	if (inst == PM_IN_NULL || inst == i) {
	    if (ctl[i].fetched == 1)
		continue;
	    if (kstat_read(kc, ctl[i].ksp, &ctl[i].iostat) == -1) {
		if (ctl[i].err == 0) {
		    int e = errno;
		    fprintf(stderr, "Error: disk_fetch(pmid=%s disk=%s ...)\n",
			    pmIDStr(mdesc->m_desc.pmid), ctl[i].diskname);
		    fprintf(stderr, "kstat_read(kc=%p, ksp=%p, ...) failed: "
				    "%s\n", kc, ctl[i].ksp, strerror(e));
		}
		ctl[i].err++;
		ctl[i].fetched = -1;
		ok = 0;
	    }
	    else {
		if (ctl[i].sderr)
			kstat_read(kc,ctl[i].sderr, NULL);
		ctl[i].fetched = 1;
		if (ctl[i].err != 0) {
		    fprintf(stderr, "Success: disk_fetch(pmid=%s disk=%s ...) "
				    "after %d errors as previously reported\n",
			    pmIDStr(mdesc->m_desc.pmid), ctl[i].diskname,
			    ctl[i].err);
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
		ull += disk_derived(mdesc, i, &ctl[i].iostat);
	    } else if (offset > sizeof(ctl[i].iostat)) { /* device_error */
		if (ctl[i].sderr) {
			char * m = (char *)offset;
			kstat_named_t *kn = kstat_data_lookup(ctl[i].sderr, m);
			static char chardat[sizeof(kn->value.c) + 1];

			if (kn == NULL) {
				fprintf(stderr, "No %s in %s\n",
					 m, ctl[i].diskname);
				return 0;
			}

			switch(kn->data_type) {
			case KSTAT_DATA_STRING:
				atom->cp = kn->value.str.addr.ptr;
				return 1;
			case KSTAT_DATA_CHAR:
				memcpy(chardat, kn->value.c,
					sizeof(kn->value.c));
				chardat[sizeof(chardat)] = '\0';
				atom->cp = chardat;
				return 1;
			}
		}
		return 0;
	    } else {
		if (mdesc->m_desc.type == PM_TYPE_U64) {
		    __uint64_t		*ullp;
		    ullp = (__uint64_t *)&((char *)&ctl[i].iostat)[offset];
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
		    ulp = (__uint32_t *)&((char *)&ctl[i].iostat)[offset];
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
