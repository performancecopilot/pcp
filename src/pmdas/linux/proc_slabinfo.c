/*
 * Linux Memory Slab Cluster
 *
 * Copyright (c) 2014 Red Hat.
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
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "indom.h"
#include "proc_slabinfo.h"

int
refresh_proc_slabinfo(proc_slabinfo_t *slabinfo)
{
    char buf[1024];
    slab_cache_t sbuf;
    slab_cache_t *s;
    FILE *fp;
    int i, n;
    int instcount;
    char *w, *p;
    int old_cache;
    int err = 0;
    static int next_id = -1;
    static int major_version = -1;
    static int minor_version = 0;

    if (next_id < 0) {
	/* one trip initialization */
	next_id = 0;

	slabinfo->ncaches = 0;
	slabinfo->caches = (slab_cache_t *)malloc(sizeof(slab_cache_t));
	slabinfo->indom->it_numinst = 0;
	slabinfo->indom->it_set = (pmdaInstid *)malloc(sizeof(pmdaInstid));
    }

    if ((fp = linux_statsfile("/proc/slabinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    for (i=0; i < slabinfo->ncaches; i++)
	slabinfo->caches[i].seen = 0;

    /* skip header */
    if (fgets(buf, sizeof(buf), fp) == NULL) {
    	/* oops, no header! */
	err = -oserror();
	goto out;
    }

    if (major_version < 0) {
	major_version = minor_version = 0;
	if (strstr(buf, "slabinfo - version:")) {
	    char *p;
	    for (p=buf; *p; p++) {
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
	    n = sscanf(buf, "%s %lu %lu", sbuf.name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs);
	    if (n != 3) {
		err = PM_ERR_APPVERSION;
		goto out;
	    }
	}
	else if (major_version == 1 && minor_version == 1) {
	    /*
	     * <name> <active_objs> <num_objs> <objsize> <active_slabs> <num_slabs> <pagesperslab>
	     * (generally 2.4 kernels)
	     */
	    n = sscanf(buf, "%s %lu %lu %u %u %u %u", sbuf.name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs,
			    &sbuf.object_size, 
			    &sbuf.num_active_slabs,
			    &sbuf.total_slabs, 
			    &sbuf.pages_per_slab);
	    if (n != 7) {
		err = PM_ERR_APPVERSION;
		goto out;
	    }

	    sbuf.total_size = sbuf.pages_per_slab * sbuf.num_active_slabs * _pm_system_pagesize;
	}
	else if (major_version == 2 && minor_version >= 0 && minor_version <= 1) {
	    /* 
	     * <name> <active_objs> <num_objs> <objsize> <objperslab> <pagesperslab>  .. and more
	     * (generally for kernels up to at least 2.6.11)
	     */
	    n = sscanf(buf, "%s %lu %lu %u %u %u", sbuf.name,
			    (unsigned long *)&sbuf.num_active_objs,
			    (unsigned long *)&sbuf.total_objs,
			    &sbuf.object_size,
			    &sbuf.objects_per_slab, 
			    &sbuf.pages_per_slab);
	    if (n != 6) {
		err = PM_ERR_APPVERSION;
		goto out;
	    }

	    sbuf.total_size = sbuf.pages_per_slab * sbuf.num_active_objs * _pm_system_pagesize / sbuf.objects_per_slab;
	}
	else {
	    /* no support */
	    err = PM_ERR_APPVERSION;
	    goto out;
	}

	old_cache = -1;
	for (i=0; i < slabinfo->ncaches; i++) {
	    if (strcmp(slabinfo->caches[i].name, sbuf.name) == 0) {
		if (slabinfo->caches[i].valid)
		    break;
		else
		    old_cache = i;
	    }
	}

	if (i == slabinfo->ncaches) {
	    /* new cache has appeared */
	    if (old_cache >= 0) {
		/* same cache as last time : reuse the id */ 
	    	i = old_cache;
	    }
	    else {
		slabinfo->ncaches++;
	    	slabinfo->caches = (slab_cache_t *)realloc(slabinfo->caches,
		    slabinfo->ncaches * sizeof(slab_cache_t));
		slabinfo->caches[i].id = next_id++;
	    }
	    slabinfo->caches[i].valid = 1;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_slabinfo: add \"%s\"\n", sbuf.name);
	    }
#endif
	}

	s = &slabinfo->caches[i];
	strcpy(s->name, sbuf.name);
	s->num_active_objs	= sbuf.num_active_objs;
	s->total_objs		= sbuf.total_objs;
	s->object_size		= sbuf.object_size;
	s->num_active_slabs	= sbuf.num_active_slabs;
	s->total_slabs		= sbuf.total_slabs;
	s->pages_per_slab	= sbuf.pages_per_slab;
	s->objects_per_slab	= sbuf.objects_per_slab;
	s->total_size		= sbuf.total_size;

	s->seen = major_version * 10 + minor_version;
    }

    /* check for caches that have been deleted (eg. by rmmod) */
    for (i=0, instcount=0; i < slabinfo->ncaches; i++) {
	if (slabinfo->caches[i].valid) {
	    if (slabinfo->caches[i].seen == 0) {
		slabinfo->caches[i].valid = 0;
#if PCP_DEBUG
		if (pmDebug & DBG_TRACE_LIBPMDA) {
		    fprintf(stderr, "refresh_slabinfo: drop \"%s\"\n", slabinfo->caches[i].name);
		}
#endif
	    }
	    else {
		instcount++;
	    }
    	}
    }

    /* refresh slabinfo indom */
    if (slabinfo->indom->it_numinst != instcount) {
        slabinfo->indom->it_numinst = instcount;
        slabinfo->indom->it_set = (pmdaInstid *)realloc(slabinfo->indom->it_set,
		instcount * sizeof(pmdaInstid));
	memset(slabinfo->indom->it_set, 0, instcount * sizeof(pmdaInstid));
    }
    for (n=0, i=0; i < slabinfo->ncaches; i++) {
        if (slabinfo->caches[i].valid) {
	    slabinfo->indom->it_set[n].i_inst = slabinfo->caches[i].id;
	    slabinfo->indom->it_set[n].i_name = slabinfo->caches[i].name;
#if PCP_DEBUG
	    if (pmDebug & DBG_TRACE_LIBPMDA) {
		fprintf(stderr, "refresh_slabinfo: cache[%d] = \"%s\"\n",
		    n, slabinfo->indom->it_set[n].i_name);
	    }
#endif
            n++;
        }
    }

out:
    fclose(fp);
    return err;
}
