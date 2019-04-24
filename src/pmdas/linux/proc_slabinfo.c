/*
 * Linux Memory Slab Cluster
 *
 * Copyright (c) 2014,2017 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#include <ctype.h>
#include "linux.h"
#include "proc_slabinfo.h"

int
refresh_proc_slabinfo(pmInDom slab_indom, proc_slabinfo_t *slabinfo)
{
    slab_cache_t sbuf, *s;
    char buf[BUFSIZ];
    char name[128];
    char *w, *p;
    FILE *fp;
    int i, sts = 0, indom_change = 0;
    static int major_version = -1;
    static int minor_version = 0;

    for (pmdaCacheOp(slab_indom, PMDA_CACHE_WALK_REWIND);;) {
	if ((i = pmdaCacheOp(slab_indom, PMDA_CACHE_WALK_NEXT)) < 0)
	    break;
	if (!pmdaCacheLookup(slab_indom, i, NULL, (void **)&s) || !s)
	    continue;
	s->seen = 0;
    }
    pmdaCacheOp(slab_indom, PMDA_CACHE_INACTIVE);

    if ((fp = linux_statsfile("/proc/slabinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    /* skip header */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
    	/* oops, no header! */
	fclose(fp);
	return -oserror();
    }

    if (major_version < 0) {
	major_version = minor_version = 0;
	if (strstr(buf, "slabinfo - version:")) {
	    for (p = buf; *p; p++) {
		if (isdigit((int)*p)) {
		    sscanf(p, "%d.%d", &major_version, &minor_version);
		    break;
		}
	    }
	}
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	/* try to convert whitespace in cache names to underscores, */
	/* by looking for alphabetic chars which follow whitespace. */
	if (buf[0] == '#')
	    continue;
	for (w = NULL, p = buf; *p != '\0'; p++) {
	    if (isspace((int)*p))
		w = p;
	    else if (isdigit((int)*p))
		break;
	    else if (isalpha((int)*p) && w) {
		for (; w && w != p; w++)
		    *w = '_';
		w = NULL;
	    }
	}

	memset(&sbuf, 0, sizeof(slab_cache_t));

	if (major_version == 1 && minor_version == 0) {
	    /*
	     * <name> <active_objs> <num_objs>
	     * (generally 2.2 kernels)
	     */
	    i = sscanf(buf, "%s %lu %lu", name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs);
	    if (i != 3) {
		sts = PM_ERR_APPVERSION;
		break;
	    }
	}
	else if (major_version == 1 && minor_version == 1) {
	    /*
	     * <name> <active_objs> <num_objs> <objsize> <active_slabs> <num_slabs> <pagesperslab>
	     * (generally 2.4 kernels)
	     */
	    i = sscanf(buf, "%s %lu %lu %u %u %u %u", name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs,
			    &sbuf.object_size, 
			    &sbuf.num_active_slabs,
			    &sbuf.total_slabs, 
			    &sbuf.pages_per_slab);
	    if (i != 7) {
		sts = PM_ERR_APPVERSION;
		break;
	    }

	    sbuf.total_size = sbuf.pages_per_slab * sbuf.num_active_slabs;
	    sbuf.total_size <<= _pm_pageshift;
	}
	else if (major_version == 2 && minor_version >= 0 && minor_version <= 1) {
	    /* 
	     * <name> <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>  .. and more
	     * (generally for kernels up to at least 2.6.11)
	     */
	    i = sscanf(buf, "%s %lu %lu %u %u %u", name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs,
			    &sbuf.object_size,
			    &sbuf.objects_per_slab, 
			    &sbuf.pages_per_slab);
	    if (i != 6) {
		sts = PM_ERR_APPVERSION;
		break;
	    }

	    sbuf.total_size = sbuf.pages_per_slab * sbuf.num_active_objs;
	    sbuf.total_size <<= _pm_pageshift;
	    sbuf.total_size /= sbuf.objects_per_slab;
	}
	else {
	    /* no support */
	    sts = PM_ERR_APPVERSION;
	    break;
	}

	sts = pmdaCacheLookupName(slab_indom, name, &i, (void **)&s);
	if (sts < 0 || !s) {
	    /* new cache has appeared */
	    if ((s = calloc(1, sizeof(*s))) == NULL)
		continue;
	    if (pmDebugOptions.libpmda)
		fprintf(stderr, "refresh_slabinfo: added \"%s\"\n", name);
	    indom_change++;
	}

	s->num_active_objs	= sbuf.num_active_objs;
	s->total_objs		= sbuf.total_objs;
	s->object_size		= sbuf.object_size;
	s->num_active_slabs	= sbuf.num_active_slabs;
	s->total_slabs		= sbuf.total_slabs;
	s->pages_per_slab	= sbuf.pages_per_slab;
	s->objects_per_slab	= sbuf.objects_per_slab;
	s->total_size		= sbuf.total_size;

	s->seen = major_version * 10 + minor_version;

	pmdaCacheStore(slab_indom, PMDA_CACHE_ADD, name, s);
    }
    fclose(fp);

    if (indom_change)
	pmdaCacheOp(slab_indom, PMDA_CACHE_SAVE);

    return sts;
}

int
proc_slabinfo_fetch(pmInDom indom, int item, unsigned int inst, pmAtomValue *ap)
{
    slab_cache_t	*slab_cache = NULL;
    int			sts;

    sts = pmdaCacheLookup(indom, inst, NULL, (void **)&slab_cache);
    if (sts < 0)
	return sts;
    if (sts == PMDA_CACHE_INACTIVE)
	return PM_ERR_INST;

    switch (item) {
	case 0:	/* mem.slabinfo.objects.active */
	    ap->ull = slab_cache->num_active_objs;
	    break;
	case 1:	/* mem.slabinfo.objects.total */
	    ap->ull = slab_cache->total_objs;
	    break;
	case 2:	/* mem.slabinfo.objects.size */
	    if (slab_cache->seen < 11)	/* version 1.1 or later only */
		return 0;
	    ap->ul = slab_cache->object_size;
	    break;
	case 3:	/* mem.slabinfo.slabs.active */
	    if (slab_cache->seen < 11)	/* version 1.1 or later only */
		return 0;
	    ap->ul = slab_cache->num_active_slabs;
	    break;
	case 4:	/* mem.slabinfo.slabs.total */
	    if (slab_cache->seen == 11)	/* version 1.1 only */
		return 0;
	    ap->ul = slab_cache->total_slabs;
	    break;
	case 5:	/* mem.slabinfo.slabs.pages_per_slab */
	    if (slab_cache->seen < 11)	/* version 1.1 or later only */
		return 0;
	    ap->ul = slab_cache->pages_per_slab;
	    break;
	case 6:	/* mem.slabinfo.slabs.objects_per_slab */
	    if (slab_cache->seen != 20 && slab_cache->seen != 21)	/* version 2.0 or 2.1 only */
		return 0;
	    ap->ul = slab_cache->objects_per_slab;
	    break;
	case 7:	/* mem.slabinfo.slabs.total_size */
	    if (slab_cache->seen < 11)	/* version 1.1 or later only */
		return 0;
	    ap->ull = slab_cache->total_size;
	    break;

	default:
	    return PM_ERR_PMID;
    }
    return 1;
}
