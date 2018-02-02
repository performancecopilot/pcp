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

    if (!endnum && *endnum != '\0')
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

/*
 * Fetches the value for the given metric instance and assigns to pmAtomValue.
 */
int
dm_vdodev_fetch(pmdaMetric *metric, const char *name, pmAtomValue *atom)
{
    unsigned int type = metric->m_desc.type;
    char *file = (char *)metric->m_user;

    switch (type) {
	case PM_TYPE_STRING:
	    atom->cp = vdo_fetch_string(file, name);
	    break;
	case PM_TYPE_U64:
	    atom->ull = vdo_fetch_ull(file, name);
	    break;
	case PM_TYPE_U32:
	    atom->ul = vdo_fetch_ul(file, name);
	    break;
	default:
	    fprintf(stderr, "Bad VDO type=%u f=%s dev=%s\n", type, file, name);
	    return 0;
    }
    return 1;
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
	if (strncmp(sysdev, "vdo", 3) != 0 || !isdigit(sysdev[3]))
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
