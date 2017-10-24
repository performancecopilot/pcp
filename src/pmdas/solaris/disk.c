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
#include <libdevinfo.h>

typedef struct {
    int		fetched;
    int		err;
    kstat_t	*ksp;
    kstat_io_t	iostat;
    kstat_t	*sderr;
    int		sderr_fresh;
} ctl_t;

static di_devlink_handle_t devlink_hndl = DI_LINK_NIL;
static di_node_t di_root = DI_NODE_NIL;

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
    kstat_ctl_t *kc;

    if ((kc = kstat_ctl_update()) == NULL)
	return;

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
    if (di_root != DI_NODE_NIL) {
	di_fini(di_root);
	di_root = DI_NODE_NIL;
    }

    if (devlink_hndl != DI_LINK_NIL) {
	di_devlink_fini(&devlink_hndl);
	devlink_hndl = DI_LINK_NIL;
    }
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

    if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
	/* desperate */
	fprintf(stderr, "disk_derived: pmid %s inst %d val %llu\n",
	    pmIDStr(mdesc->m_desc.pmid), inst, (unsigned long long)val);
    }

    return val;
}

static int
fetch_disk_data(kstat_ctl_t *kc, const pmdaMetric *mdesc, ctl_t *ctl,
		const char *diskname)
{
    if (ctl->fetched == 1)
	return 1;

    if (ctl->ksp == NULL)
	return 0;

    if ((kstat_read(kc, ctl->ksp, &ctl->iostat) == -1)) {
	if (ctl->err == 0) {
	    __pmNotifyErr(LOG_WARNING,
			  "Error: disk_fetch(pmid=%s disk=%s ...) - "
			   "kstat_read(kc=%p, ksp=%p, ...) failed: %s\n",
			   pmIDStr(mdesc->m_desc.pmid), diskname,
			   kc, ctl->ksp, osstrerror());
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
get_devlink_path(di_devlink_t devlink, void *arg)
{
	const char **p = arg;
        *p = di_devlink_path(devlink);
        return DI_WALK_TERMINATE;
}

static int
fetch_disk_devlink(const kstat_t *ksp, pmAtomValue *atom)
{
    di_node_t n;

    if (di_root == DI_NODE_NIL) {
	if ((di_root = di_init("/", DINFOCPYALL)) == DI_NODE_NIL)
	    return 0;
    }

    if (devlink_hndl == DI_LINK_NIL) {
	if ((devlink_hndl = di_devlink_init(NULL, DI_MAKE_LINK)) == DI_LINK_NIL)
	    return 0;
    }

    if ((n = di_drv_first_node(ksp->ks_module, di_root)) == DI_NODE_NIL) {
	if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
	    fprintf(stderr,"No nodes for %s: %s\n",
		    ksp->ks_name, osstrerror());
	}
	return 0;
    }

    do {
	if (di_instance(n) == ksp->ks_instance) {
	    di_minor_t minor = di_minor_next(n, DI_MINOR_NIL);
	    char *path;
	    char *devlink = NULL;

	    if (minor == DI_MINOR_NIL) {
		if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
		    fprintf (stderr, "No minors of %s: %s\n",
			     ksp->ks_name, osstrerror());
		}
		return 0;
	    }
	    path = di_devfs_minor_path(minor);
	    di_devlink_walk(devlink_hndl, NULL, path, 0, &devlink,
			    get_devlink_path);
	    di_devfs_path_free(path);

	    if (devlink) {
		atom->cp = devlink;
		return 1;
	    }
	    return 0;
	}
	n = di_drv_next_node(n);
    } while (n != DI_NODE_NIL);
    return 0;
}

static int
get_instance_value(pmdaMetric *mdesc, pmInDom dindom, int inst,
		   pmAtomValue *atom)
{
    ctl_t *ctl;
    char *diskname;
    uint64_t ull;
    ptrdiff_t offset = ((metricdesc_t *)mdesc->m_user)->md_offset;
    kstat_ctl_t *kc;

    if ((kc = kstat_ctl_update()) == NULL)
	return 0;

    if (pmdaCacheLookup(dindom, inst, &diskname,
			(void **)&ctl) != PMDA_CACHE_ACTIVE) {
	if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
	    fprintf(stderr,
		    "Unexpected cache result - instance %d "
		    "is not active in disk indom cache\n",
		    inst);
	}
	return 0;
    }

    if (offset == -1) {
	if (pmid_item(mdesc->m_desc.pmid) == 35) { /* hinv.disk.devlink */
	    return fetch_disk_devlink(ctl->ksp, atom);
	}
	if (!fetch_disk_data(kc, mdesc, ctl, diskname))
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
		if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
		    fprintf(stderr, "No %s in %s\n", m, diskname);
		return 0;
	    }

	    return kstat_named_to_pmAtom(kn, atom);
	}
	return 0;
    } else {
	char *iop = ((char *)&ctl->iostat) + offset;
	if (!fetch_disk_data(kc, mdesc, ctl, diskname))
	    return 0;
	if (mdesc->m_desc.type == PM_TYPE_U64) {
	    __uint64_t *ullp = (__uint64_t *)iop;
	    ull = *ullp;
	    if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
		/* desperate */
		fprintf(stderr, "disk_fetch: pmid %s inst %d val %llu\n",
			pmIDStr(mdesc->m_desc.pmid), inst,
			(unsigned long long)*ullp);
	    }
	}
	else {
	    __uint32_t *ulp = (__uint32_t *)iop;
	    ull = *ulp;
	    if (pmDebugOptions.appl0 && pmDebugOptions.appl2) {
		/* desperate */
		fprintf(stderr, "disk_fetch: pmid %s inst %d val %u\n",
		    pmIDStr(mdesc->m_desc.pmid), inst, *ulp);
	    }
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
