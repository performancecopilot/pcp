/*
 * Copyright (c) 2014-2018 Red Hat.
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

#include "pmapi.h"
#include "pmda.h"

#include "root.h"
#include "docker.h"

static const char *systemd_cgroup;

static json_metric_desc json_metrics[] = {
    { "State/Pid", 0, 1, {0}, ""},
    { "Name", 0, 1, {0}, ""},
    { "State/Running", CONTAINER_FLAG_RUNNING, 1, {0}, ""},
    { "State/Paused", CONTAINER_FLAG_PAUSED, 1, {0}, ""},
    { "State/Restarting", CONTAINER_FLAG_RESTARTING, 1, {0}, ""},
};
#define JSONMETRICS_SZ	(sizeof(json_metrics)/sizeof(json_metric_desc))

/*
 * Container implementation for the Docker engine
 * Uses /var/lib/docker/container files to discover local containers.
 */

void
docker_setup(container_engine_t *dp)
{
    static const char	*docker_default = "/var/lib/docker";
    const char		*docker = getenv("PCP_DOCKER_DIR");
    char		path[MAXPATHLEN];

    /* determine the default container naming heuristic */
    if (systemd_cgroup == NULL)
	systemd_cgroup = getenv("PCP_SYSTEMD_CGROUP");

    /* determine the location of docker container config.json files */
    if (docker == NULL)
	docker = docker_default;
    pmsprintf(path, sizeof(path), "%s/containers", docker);
    dp->path = strdup(path);

    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "docker_setup: path %s, %s style names\n",
			dp->path, systemd_cgroup ? "systemd" : "default");
}

int
docker_indom_changed(container_engine_t *dp)
{
    static const char	*cgroup_check_default = "/sys/fs/cgroup/memory/docker";
    static const char	*cgroup_check_path;
    static int		lasterrno;
    static struct stat	lastsbuf;
    static struct stat	lastcgroupsbuf;
    struct stat		statbuf;

    if (cgroup_check_path == NULL) {
	if ((cgroup_check_path = getenv("PCP_CGROUP_CHECK_PATH")) == NULL)
	    cgroup_check_path = cgroup_check_default;
    }

    if (stat(cgroup_check_path, &statbuf) == 0 &&
	root_stat_time_differs(&statbuf, &lastcgroupsbuf)) {
	lastcgroupsbuf = statbuf;
	return 1;
    }

    if (stat(dp->path, &statbuf) != 0) {
	if (oserror() == lasterrno)
	    return 0;
	memset(&lastsbuf, 0, sizeof(lastsbuf));
	lasterrno = oserror();
	return 1;
    }
    lasterrno = 0;
    if (!root_stat_time_differs(&statbuf, &lastsbuf))
	return 0;
    lastsbuf = statbuf;
    return 1;
}

static char *
docker_cgroup_find(char *name, int namelen, char *path, int pathlen)
{
    DIR			*finddir;
    int			bytes, childlen, found = 0;
    char		*base, *child;
    struct dirent	*drp;

    /* find candidates in immediate children of path */
    if ((finddir = opendir(path)) == NULL)
	return NULL;

    while ((drp = readdir(finddir)) != NULL) {
	/* handle any special cases that we can cull right away */
	if (drp->d_type == DT_REG)
	    continue;
	if (*(base = &drp->d_name[0]) == '.' ||
	    strcmp(base, "user.slice") == 0)
	    continue;
	child = base;
	bytes = strlen(base);
	if (pathlen + bytes + 2 >= MAXPATHLEN)
	    continue;
	/* accept either a direct name match or docker- prefixed */
	if (strncmp(child, "docker-", sizeof("docker-")-1) == 0)
	    child += sizeof("docker-")-1;
	childlen = pathlen;
	path[childlen++] = '/';
	memcpy(path + childlen, base, bytes + 1);
	if (strncmp(child, name, namelen) == 0) {
	    found = 1;
	    break;
	}
	/* didn't match - descend into this (possible) directory */
	child = docker_cgroup_find(name, namelen, path, childlen + bytes);
	if (child != NULL) {
	    path = child;
	    found = 1;
	    break;
	}
    }
    closedir(finddir);

    return found ? path : NULL;
}

static int
docker_cgroup_search(char *name, int namelen, char *cgroups, container_t *cp)
{
    char		path[MAXPATHLEN], *p;
    int			pathlen;

    /* Using "unified" (cgroup2) or "memory" (cgroup1) as starting
     * points, recursively descend looking for a named container.
     */
    pathlen = pmsprintf(path, sizeof(path), "%s/%s", cgroups, "unified");
    if ((p = docker_cgroup_find(name, namelen, path, pathlen)) != NULL) {
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "%s", p + pathlen);
	return 0;
    }
    pathlen = pmsprintf(path, sizeof(path), "%s/%s", cgroups, "memory");
    if ((p = docker_cgroup_find(name, namelen, path, pathlen)) != NULL) {
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "%s", p + pathlen);
	return 0;
    }
    /* Else fallback to using a default naming convention. */
    if (systemd_cgroup)
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "%s/docker-%s.scope",
			systemd_cgroup, name);
    else
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "/docker/%s", name);
    return 1;
}

void
docker_insts_refresh(container_engine_t *dp, pmInDom indom)
{
    static const char	*cgroup_default = "/sys/fs/cgroup";
    static char		*cgroup;
    DIR			*rundir;
    char		*path;
    struct dirent	*drp;
    container_t		*cp;
    int			sts;

    if (cgroup == NULL && (cgroup = getenv("PCP_CGROUP_DIR")) == NULL)
	cgroup = (char *)cgroup_default;

    if ((rundir = opendir(dp->path)) == NULL) {
	if (pmDebugOptions.attr)
	    fprintf(stderr, "%s: skipping docker path %s\n",
		    pmGetProgname(), dp->path);
	return;
    }

    while ((drp = readdir(rundir)) != NULL) {
	if (*(path = &drp->d_name[0]) == '.')
	    continue;
	sts = pmdaCacheLookupName(indom, path, NULL, (void **)&cp);
	switch (sts) {
	case PMDA_CACHE_ACTIVE:
	    break;

	case PMDA_CACHE_INACTIVE:
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, cp);
	    break;

	default:
	    /* allocate space for values for this container and update indom */
	    if (pmDebugOptions.attr)
		fprintf(stderr, "%s: adding docker container %s\n",
			pmGetProgname(), path);
	    if ((cp = calloc(1, sizeof(container_t))) == NULL)
		break;
	    cp->engine = dp;
	    pmdaCacheStore(indom, PMDA_CACHE_ADD, path, cp);
	}
	if (cp == NULL || (cp->uptodate & CONTAINERS_UPTODATE_CGROUP))
	    continue;
	sts = docker_cgroup_search(path, strlen(path), cgroup, cp);
	/* container may not be running, so heuristic may be wrong */
	if (sts == 0)
	    cp->uptodate |= CONTAINERS_UPTODATE_CGROUP;
	else if (pmDebugOptions.attr)
	    fprintf(stderr, "%s: unverified container cgroup used for %s\n",
		    pmGetProgname(), path);
    }
    closedir(rundir);
}

static int
docker_values_changed(const char *path, container_t *values)
{
    struct stat		statbuf;

    if (stat(path, &statbuf) != 0) {
	memset(&values->stat, 0, sizeof(values->stat));
	return 1;
    }
    if (!root_stat_time_differs(&statbuf, &values->stat))
	return 0;
    values->stat = statbuf;
    return 1;
}

static int
docker_fread(char *buffer, int buflen, void *data)
{
    FILE	*fp = (FILE *)data;
    int		sts;

    if ((sts = fread(buffer, 1, buflen, fp)) > 0) {
	if (pmDebugOptions.attr && pmDebugOptions.desperate)
	    pmNotifyErr(LOG_DEBUG, "docker_fread[%d bytes]: %.*s\n",
			sts, sts, buffer);
	return sts;
    }
    if (feof(fp))
	return 0;
    return -ferror(fp);
}

static int
docker_values_parse(FILE *fp, const char *name, container_t *values)
{
    int     sts = 0;
    int     i;

    static const int JSONMETRICS_BYTES = JSONMETRICS_SZ * sizeof(*json_metrics);
    static json_metric_desc *local_metrics;

    if (!local_metrics)
	local_metrics = (json_metric_desc *)malloc(JSONMETRICS_BYTES);

    memcpy(local_metrics, json_metrics, JSONMETRICS_BYTES);
    for (i = 0; i < JSONMETRICS_SZ; i++)
	local_metrics[i].json_pointer = strdup(json_metrics[i].json_pointer);

    local_metrics[0].dom = strdup(name);

    clearerr(fp);
    if ((sts = pmjsonGet(local_metrics, JSONMETRICS_SZ, PM_INDOM_NULL,
			 docker_fread, (void *)fp)) < 0)
	return sts;

    if (local_metrics[0].values.l)
	values->pid = local_metrics[0].values.l;
    else
	values->pid = -1;
    if (local_metrics[1].values.cp)
	values->name = strdup(local_metrics[1].values.cp);
    else
	values->name = strdup("?");
    if (values->name)
	values->uptodate |= CONTAINERS_UPTODATE_NAME;
    if (local_metrics[2].values.ul)
	values->flags = CONTAINER_FLAG_RUNNING;
    else if (local_metrics[3].values.ul)
	values->flags = CONTAINER_FLAG_PAUSED;
    else if (local_metrics[4].values.ul)
	values->flags = CONTAINER_FLAG_RESTARTING;
    else
	values->flags = 0;
    values->uptodate |= CONTAINERS_UPTODATE_STATE;
    return 0;
}

/* docker decided to not only change the config file json pointer layout, but
 * also the filename in which the config resides.  Check which version is being used.
 */
int determine_docker_version(container_engine_t *dp, const char *name, char *buf, size_t bufsize)
{
    FILE *fp;

    /* we go with version two first, because v1 config files may not
     * always be cleaned up after the upgrade to v2.
     */
    pmsprintf(buf, bufsize, "%s/%s/config.v2.json", dp->path, name);
    if ((fp = fopen(buf, "r")) != NULL) {
	fclose(fp);
	return 2;
    }
    pmsprintf(buf, bufsize, "%s/%s/config.json", dp->path, name);
    if ((fp = fopen(buf, "r")) != NULL) {
	fclose(fp);
	return 1;
    }
    return -oserror();
}

/*
 * Extract critical information (PID1, state) for a named container.
 * Name here is the unique identifier we've chosen to use for Docker
 * container external instance names (i.e. long hash names).
 */
int
docker_value_refresh(container_engine_t *dp,
		     const char *name, container_t *values)
{
    int		sts;
    FILE	*fp;
    char	path[MAXPATHLEN];

    sts = determine_docker_version(dp, name, path, sizeof(path));
    if (sts < 0)
	return sts;

    if (!docker_values_changed(path, values))
	return 0;

    values->uptodate &= ~(CONTAINERS_UPTODATE_NAME|CONTAINERS_UPTODATE_STATE);

    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "%s: file=%s\n", "docker_value_refresh", path);
    if ((fp = fopen(path, "r")) == NULL)
	return -oserror();
    sts = docker_values_parse(fp, name, values);
    fclose(fp);
    if (sts < 0)
	return sts;

    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "%s: uptodate=0x%x\n", "docker_value_refresh",
		    values->uptodate);

    /* did we manage to extract both container name and status? */
    return (values->uptodate & CONTAINERS_UPTODATE_STATE)? 0 : PM_ERR_AGAIN;
}

/*
 * Given two strings, determine if they identify the same container.
 * This is called iteratively, passing over all container instances.
 *
 * For Docker we need to match user-supplied names (which Docker will
 * have assigned itself, if none given - see 'docker ps'), optionally
 * slash-prefixed (for some reason Docker does this? *shrug*).  Also,
 * we want to allow for matching on the container hash identifiers we
 * use for the external names (or unique-prefix components thereof).
 *
 * Use a simple ranking scheme - the closer the match, the higher the
 * return value, up to a maximum of 100% (zero => no match).
 *
 * 'query' - the name supplied by the PCP monitoring tool.
 * 'username' - the name from the container_t -> name field.
 * 'instname' - the external instance identifier, lengthy hash.
 */
int
docker_name_matching(struct container_engine *dp, const char *query,
	const char *username, const char *instname)
{
    unsigned int ilength, qlength, limit;
    int i, fuzzy = 0;

    if (username) {
	if (strcmp(query, username) == 0)
	    return 100;
	if (username[0] == '/' && strcmp(query, username+1) == 0)
	    return 99;
    }
    if (strcmp(query, instname) == 0)
	return 98;
    qlength = strlen(query);
    ilength = strlen(instname);
    /* find the shortest of the three boundary conditions */
    if ((limit = (qlength < ilength) ? qlength : ilength) > 95)
	limit = 95;
    for (i = 0; i < limit; i++) {
	if (query[i] != instname[i])
	    break;
	fuzzy++;	/* bump for each matching character */
    }
    return fuzzy;
}
