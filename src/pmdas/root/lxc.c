/*
 * Copyright (c) 2015 Red Hat.
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
#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "root.h"
#include "lxc.h"

static char *lxc_info = "lxc-info";

/*
 * Container implementation for the LXC engine
 *
 * Uses a combination of (timestamp-based) checking on /var/lib/lxc to
 * discover local containers, and parsing output from lxc-info command
 * to find state and process ID.
 */

void
lxc_setup(container_engine_t *dp)
{
     static const char *lxc_default = "/var/lib/lxc";
     const char *lxc = getenv("PCP_LXC_DIR");
     char *lxc_cmd = getenv("PCP_LXC_INFO");

     if (!lxc)
	lxc = lxc_default;
     if (lxc_cmd)
	lxc_info = lxc_cmd;
     strncpy(dp->path, lxc, sizeof(dp->path));
     dp->path[sizeof(dp->path)-1] = '\0';

    if (pmDebug & DBG_TRACE_ATTR)
	__pmNotifyErr(LOG_DEBUG, "lxc_setup: using path: %s\n", dp->path);
}

int
lxc_indom_changed(container_engine_t *dp)
{
    static int		lasterrno;
    static struct stat	lastsbuf;
    struct stat		statbuf;

    if (stat(dp->path, &statbuf) != 0) {
	if (oserror() == lasterrno)
	    return 0;
	lasterrno = oserror();
	return 1;
    }
    lasterrno = 0;
    if (!root_stat_time_differs(&statbuf, &lastsbuf))
	return 0;
    lastsbuf = statbuf;
    return 1;
}

void
lxc_insts_refresh(container_engine_t *dp, pmInDom indom)
{
    DIR			*rundir;
    char		*path;
    struct dirent	*drp;
    container_t		*cp;
    int			sts;

    if ((rundir = opendir(dp->path)) == NULL) {
	if (pmDebug & DBG_TRACE_ATTR)
	    fprintf(stderr, "%s: skipping lxc path %s\n",
		    pmProgname, dp->path);
	return;
    }

    while ((drp = readdir(rundir)) != NULL) {
	if (*(path = &drp->d_name[0]) == '.')
	    continue;
	sts = pmdaCacheLookupName(indom, path, NULL, (void **)&cp);
	if (sts == PMDA_CACHE_ACTIVE)
	    continue;
	/* allocate space for values for this container and update indom */
	if (sts != PMDA_CACHE_INACTIVE) {
	    if (pmDebug & DBG_TRACE_ATTR)
		fprintf(stderr, "%s: adding lxc container %s\n",
			pmProgname, path);
	    if ((cp = calloc(1, sizeof(container_t))) == NULL)
		continue;
	    cp->engine = dp;
	    cp->name = cp->cgroup + 4;
	    snprintf(cp->cgroup, sizeof(cp->cgroup), "lxc/%s", path);
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, path, cp);
    }
    closedir(rundir);
}

/*
 * Parse output from lxc-info command, along the lines:
 *     Name:           myvm
 *     State:          RUNNING
 *     PID:            17297
 *     CPU use:        0.94 seconds
 */
static int
lxc_values_parse(FILE *pp, const char *name, container_t *values)
{
    char buffer[256];
    char *s, *key, *value;

    values->pid = values->flags = 0;

    while ((s = fgets(buffer, sizeof(buffer)-1, pp)) != NULL) {
	key = s;
	while (*s && *s != ':') s++;
	*s++ = '\0';
	while (isspace(*s)) s++;
	value = s;

	if (strcmp(key, "PID") == 0)
	    values->pid = atoi(value);
	if (strcmp(key, "State") == 0) {
	    if (strncmp(value, "RUNNING", 7) == 0 ||
	        strncmp(value, "STOPPING", 8) == 0 ||
	        strncmp(value, "ABORTING", 8) == 0)
		values->flags |= CONTAINER_FLAG_RUNNING;
	    if (strncmp(value, "STOPPED", 7) == 0)
		values->flags |= CONTAINER_FLAG_PAUSED;
	    if (strncmp(value, "STARTING", 7) == 0)
		values->flags |= CONTAINER_FLAG_RESTARTING;
	}
    }
    values->uptodate = NUM_UPTODATE;

    return 0;
}

/*
 * Extract critical information (PID1, state) for a named container.
 * Name here is the identifier given at LXC container creation time,
 * as discovered below /var/lib/lxc.
 */
int
lxc_value_refresh(container_engine_t *dp, const char *name, container_t *values)
{
    int		sts;
    FILE	*pp;
    char	path[MAXPATHLEN];

    snprintf(path, sizeof(path), "%s -n %s", lxc_info, name);
    if (pmDebug & DBG_TRACE_ATTR)
	__pmNotifyErr(LOG_DEBUG, "lxc_values_refresh: pipe=%s\n", path);
    if ((pp = popen(path, "r")) == NULL)
	return -oserror();
    sts = lxc_values_parse(pp, name, values);
    pclose(pp);
    return sts;
}

/*
 * Given two strings, determine if they identify the same container.
 * This is called iteratively, passing over all container instances.
 *
 * For LXC we simply directly match user-supplied names (see lxc-ls)
 * Return zero for no match, 100 for a perfect match - for the LXC
 * engine, username and instname are equivalent:
 *
 * 'query' - the name supplied by the PCP monitoring tool.
 * 'username' - the name from the container_t -> name field.
 * 'instname' - the external instance identifier.
 */
int
lxc_name_matching(struct container_engine *dp, const char *query,
	const char *username, const char *instname)
{
    if (strcmp(query, username) == 0)
	return 100;
    if (strcmp(query, instname) == 0)
	return 99;
    return 0;
}
