/*
 * Linux zoneinfo Cluster
 *
 * Copyright (c) 2016 Fujitsu.
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
#include "linux.h"
#include "proc_zoneinfo.h"

int
refresh_proc_zoneinfo(pmInDom indom)
{
    int node, values;
    zoneinfo_entry_t *info;
    unsigned long long value;
    char zonetype[32];
    char instname[64];
    char buf[BUFSIZ];
    static int setup;
    int changed = 0;
    FILE *fp;

    if (!setup) {
	pmdaCacheOp(indom, PMDA_CACHE_LOAD);
	setup = 1;
    }

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);
    if ((fp = linux_statsfile("/proc/zoneinfo", buf, sizeof(buf))) == NULL)
	return -oserror();

    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (strncmp(buf, "Node", 4) != 0)
	    continue;
	if (sscanf(buf, "Node %d, zone   %s", &node, zonetype) != 2)
	    continue;
	snprintf(instname, sizeof(instname), "%s::node%u", zonetype, node);
	instname[sizeof(instname)-1] = '\0';
	values = 0;
	info = NULL;
	if (pmdaCacheLookupName(indom, instname, NULL, (void **)&info) < 0 ||
	    info == NULL) {
	    /* not found: allocate and add a new entry */
	    info = (zoneinfo_entry_t *)calloc(1, sizeof(zoneinfo_entry_t));
	    changed = 1;
	}
	/* inner loop to extract all values for this node */
	while (values < ZONE_VALUES && fgets(buf, sizeof(buf), fp) != NULL) {
	    if ((sscanf(buf, "  pages free %llu", &value)) == 1) {
		info->values[ZONE_FREE] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        min %llu", &value)) == 1) {
		info->values[ZONE_MIN] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        low %llu", &value)) == 1) {
		info->values[ZONE_LOW] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        high %llu", &value)) == 1) {
		info->values[ZONE_HIGH] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        scanned %llu", &value)) == 1) {
		info->values[ZONE_SCANNED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        spanned %llu", &value)) == 1) {
		info->values[ZONE_SPANNED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        present %llu", &value)) == 1) {
		info->values[ZONE_PRESENT] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	    else if ((sscanf(buf, "        managed %llu", &value)) == 1) {
		info->values[ZONE_MANAGED] = (value << _pm_pageshift) / 1024;
		values++;
		continue;
	    }
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, instname, (void *)info);

#if PCP_DEBUG
	if (pmDebug & DBG_TRACE_LIBPMDA)
	    fprintf(stderr, "refresh_proc_zoneinfo: instance %s\n", instname);
#endif
    }
    fclose(fp);

    if (changed)
	pmdaCacheOp(indom, PMDA_CACHE_SAVE);

    return 0;
}
