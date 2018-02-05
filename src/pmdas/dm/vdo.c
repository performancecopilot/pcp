/*
 * Device Mapper PMDA - VDO (Virtual Data Optimizer) Stats
 *
 * Copyright (c) 2018 Red Hat.
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
#include "libpcp.h"
#include "pmda.h"

#include "vdo.h"
#include "indom.h"

static char *dm_vdo_statspath;

static char *
vdo_fetch_oneline(const char *path, const char *name)
{
    static char buffer[MAXPATHLEN];
    FILE *fp;

    pmsprintf(buffer, sizeof(buffer), "%s/%s/statistics/%s",
		   dm_vdo_statspath, name, path);

    if ((fp = fopen(buffer, "r")) != NULL) {
	int i = fscanf(fp, "%63s", buffer);
	fclose(fp);
	if (i == 1)
	    return buffer;
    }
    return NULL;
}

static char *
vdo_fetch_string(const char *file, const char *name)
{
    return vdo_fetch_oneline(file, name);
}

static __uint64_t
vdo_fetch_ull(const char *file, const char *name)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;
    __uint64_t v = strtoull(value, &endnum, 10);

    if (!endnum || *endnum != '\0')
	return 0;
    return v;
}

static __uint32_t
vdo_fetch_ul(const char *file, const char *name)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;
    __uint32_t v = strtoul(value, &endnum, 10);

    if (!endnum || *endnum != '\0')
	return 0;
    return v;
}

static float
vdo_fetch_float(const char *file, const char *name)
{
    char *value = vdo_fetch_oneline(file, name);
    char *endnum = NULL;
    float v = strtof(value, &endnum);

    if (!endnum || *endnum != '\0')
	return 0;
    return v;
}

/*
 * Fetches the value for the given metric instance and assigns to pmAtomValue.
 */
int
dm_vdodev_fetch(pmdaMetric *metric, const char *name, pmAtomValue *atom)
{
    char *file = (char *)metric->m_user;

    if (file) {
	unsigned int	type = metric->m_desc.type;

	switch (type) {
	case PM_TYPE_STRING:
	    atom->cp = vdo_fetch_string(file, name);
	    return 1;
	case PM_TYPE_FLOAT:
	    atom->f = vdo_fetch_float(file, name);
	    return 1;
	case PM_TYPE_U64:
	    atom->ull = vdo_fetch_ull(file, name);
	    return 1;
	case PM_TYPE_U32:
	    atom->ul = vdo_fetch_ul(file, name);
	    return 1;
	default:
	    break;
	}
	fprintf(stderr, "Bad VDO type=%u f=%s dev=%s\n", type, file, name);
    } else {	/* derived metrics */
	unsigned int	item = pmID_item(metric->m_desc.pmid);
	double		calc;

	switch (item) {
	case VDODEV_JOURNAL_BLOCKS_BATCHING:
	    atom->ull = vdo_fetch_ull("journal_blocks_started", name) -
			vdo_fetch_ull("journal_blocks_written", name);
	    return 1;
	case VDODEV_JOURNAL_BLOCKS_WRITING:
	    atom->ull = vdo_fetch_ull("journal_blocks_written", name) -
			vdo_fetch_ull("journal_blocks_committed", name);
	    return 1;
	case VDODEV_JOURNAL_ENTRIES_BATCHING:
	    atom->ull = vdo_fetch_ull("journal_entries_started", name) -
			vdo_fetch_ull("journal_entries_written", name);
	    return 1;
	case VDODEV_JOURNAL_ENTRIES_WRITING:
	    atom->ull = vdo_fetch_ull("journal_entries_written", name) -
			vdo_fetch_ull("journal_entries_committed", name);
	    return 1;
	case VDODEV_CAPACITY:
	    atom->ull = vdo_fetch_ull("physical_blocks", name) *
			vdo_fetch_ull("block_size", name) / 1024;
	    return 1;
	case VDODEV_USED:
	    atom->ull = (vdo_fetch_ull("data_blocks_used", name) +
			 vdo_fetch_ull("overhead_blocks_used", name)) *
			 vdo_fetch_ull("block_size", name) / 1024;
	    return 1;
	case VDODEV_AVAILABLE:
	    atom->ull = (vdo_fetch_ull("physical_blocks", name) -
			 vdo_fetch_ull("data_blocks_used", name) -
			 vdo_fetch_ull("overhead_blocks_used", name)) *
			 vdo_fetch_ull("block_size", name) / 1024;
	    return 1;
	case VDODEV_USED_PERCENTAGE:
	    atom->ull = vdo_fetch_ull("physical_blocks", name);
	    calc = (double)(vdo_fetch_ull("data_blocks_used", name) +
			    vdo_fetch_ull("overhead_blocks_used", name));
	    atom->f = 100.0 * (calc / (double)atom->ull);
	    return 1;
	case VDODEV_SAVINGS_PERCENTAGE:
	    atom->ull = vdo_fetch_ull("logical_blocks_used", name);
	    calc = (double)(atom->ull -
			    vdo_fetch_ull("data_blocks_used", name));
	    atom->f = 100.0 * (calc / (double)atom->ull);
	    return 1;
	default:
	    break;
	}
	fprintf(stderr, "Bad metric item=%u dev=%s\n", item, name);
    }
    return 0;
}

/*
 * Check whether 'name' in 'vdosysdir' is a VDO volume.
 */
static int
dm_vdodev_isvdovolumedir(const char *dir, const char *name)
{
    static char buffer[MAXPATHLEN];

    pmsprintf(buffer, sizeof(buffer), "%s/%s/statistics", dir, name);
    /* This file should exist if this is a VDO volume */
    return (access(buffer, F_OK) != -1);
}

/*
 * Update the VDO device instance domain. This will change as
 * VDO volumes are created, activated and removed.
 *
 * Using the pmdaCache interfaces simplifies things and provides
 * guarantees needed around consistent instance numbering in all
 * of those interesting corner cases.
 */
int
dm_vdodev_instance_refresh(void)
{
    DIR *sysdir;
    char *sysdev;
    struct dirent *sysentry;
    pmInDom indom = dm_indom(DM_VDODEV_INDOM);

    pmdaCacheOp(indom, PMDA_CACHE_INACTIVE);

    if ((sysdir = opendir(dm_vdo_statspath)) == NULL)
	return -oserror();

    while ((sysentry = readdir(sysdir)) != NULL) {
	sysdev = sysentry->d_name;
	if (sysdev[0] == '.')
	    continue;
	if (!dm_vdodev_isvdovolumedir(dm_vdo_statspath, sysdev))
	    continue;
	if (pmDebugOptions.libpmda)
	    fprintf(stderr, "dm_vdodev_instance_refresh: added %s", sysdev);
	pmdaCacheStore(indom, PMDA_CACHE_ADD, sysdev, NULL);
    }
    closedir(sysdir);
    return 0;
}

void
dm_vdo_setup(void)
{
    static char vdo_path[] = "/sys/kvdo";
    char *env_path;

    /* allow override at startup for QA testing */
    if ((env_path = getenv("DM_VDO_STATSPATH")) != NULL)
        dm_vdo_statspath = env_path;
    else
        dm_vdo_statspath = vdo_path;
}
