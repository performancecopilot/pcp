/*
 * Copyright (c) 2014-2017 Red Hat.
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
#include "impl.h"
#include "pmda.h"

#include "root.h"
#include "docker.h"

/*
 * Heuristic to determine if systemd cgroup names will be used,
 * or not.  Uglee, but appears to be no better way (previously,
 * we tested for /sys/fs/cgroup/systemd existance but turns out
 * the partial systemd installations of some distributions also
 * satisfy that test).
 */
static int
test_systemd_init(void)
{
    FILE *fp = fopen("/proc/1/cmdline", "r");
    char line[64];
    int	count;

    if (!fp)
	return 0;
    count = fscanf(fp, "%63s", line);
    fclose(fp);
    if (count == 1 && strstr(line, "systemd") != NULL)
	return 1;
    return 0;
}

/*
 * Container implementation for the Docker engine
 *
 * Currently uses direct access to the /var/lib/docker/container state
 * to discover local containers.  We may want to switch over to using
 * the Docker daemon (Unix socket) JSON API at some point though if we
 * can get everything needed there.  That'd also mean the daemon must
 * be running to get at the data, which also may be a problem (we can
 * fallback to using the code below in that case though?).  Anyway, it
 * is early days still...
 */

void
docker_setup(container_engine_t *dp)
{
    static const char *docker_default = "/var/lib/docker";
    const char *systemd_cgroup = getenv("PCP_SYSTEMD_CGROUP");
    const char *docker = getenv("PCP_DOCKER_DIR");

    /* determine the location of docker container config.json files */
    if (!docker)
	docker = docker_default;
    pmsprintf(dp->path, sizeof(dp->path), "%s/containers", docker);
    dp->path[sizeof(dp->path)-1] = '\0';

    /* heuristic to determine which cgroup naming convention in use */
    if (systemd_cgroup || test_systemd_init())
	dp->state |= CONTAINER_STATE_SYSTEMD;

    if (pmDebugOptions.attr)
	__pmNotifyErr(LOG_DEBUG, "docker_setup: path %s, %s suffix\n", dp->path,
			(dp->state & CONTAINER_STATE_SYSTEMD)? "systemd" : "default");
}

int
docker_indom_changed(container_engine_t *dp)
{
    static int		lasterrno;
    static struct stat	lastsbuf;
    struct stat		statbuf;

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

void
docker_insts_refresh(container_engine_t *dp, pmInDom indom)
{
    DIR			*rundir;
    char		*path;
    struct dirent	*drp;
    container_t		*cp;
    int			sts;

    if ((rundir = opendir(dp->path)) == NULL) {
	if (pmDebugOptions.attr)
	    fprintf(stderr, "%s: skipping docker path %s\n",
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
	    if (pmDebugOptions.attr)
		fprintf(stderr, "%s: adding docker container %s\n",
			pmProgname, path);
	    if ((cp = calloc(1, sizeof(container_t))) == NULL)
		continue;
	    cp->engine = dp;
	    pmsprintf(cp->cgroup, sizeof(cp->cgroup),
			(dp->state & CONTAINER_STATE_SYSTEMD) ?
			"/system.slice/docker-%s.scope" : "/docker/%s",
			path);
	}
	pmdaCacheStore(indom, PMDA_CACHE_ADD, path, cp);
	
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
	    __pmNotifyErr(LOG_DEBUG, "docker_fread[%d bytes]: %.*s\n",
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
    values->uptodate = 0;

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
	values->uptodate++;
    if (local_metrics[2].values.ul)
	values->flags = CONTAINER_FLAG_RUNNING;
    else if (local_metrics[3].values.ul)
	values->flags = CONTAINER_FLAG_PAUSED;
    else if (local_metrics[4].values.ul)
	values->flags = CONTAINER_FLAG_RESTARTING;
    else
	values->flags = 0;
    values->uptodate++;
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
    if (pmDebugOptions.attr)
	__pmNotifyErr(LOG_DEBUG, "docker_value_refresh: file=%s\n", path);
    if ((fp = fopen(path, "r")) == NULL)
	return -oserror();
    sts = docker_values_parse(fp, name, values);
    fclose(fp);
    if (sts < 0)
	return sts;

    if (pmDebugOptions.attr)
	__pmNotifyErr(LOG_DEBUG, "docker_value_refresh: uptodate=%d of %d\n",
	    values->uptodate, NUM_UPTODATE);

    return values->uptodate == NUM_UPTODATE ? 0 : PM_ERR_AGAIN;
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
