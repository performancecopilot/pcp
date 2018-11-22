/*
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

#include "pmapi.h"
#include "pmda.h"
#include "root.h"
#include "jsonsl.h"
#include "libpcp.h"
#include "podman.h"

/*
 * Container implementation for the podman engine
 * Uses /var/{lib,run}/containers files to discover local containers.
 */
#define MAX_RECURSION_DEPTH	256
#define MAX_DESCENT_LEVEL	64
#define CONTAINERS_DATADIR	\
	"/var/lib/containers/storage/overlay-containers"
#define CONTAINERS_RUNDIR	\
	"/var/run/containers/storage/overlay-containers"

static const char	*podman_datadir;
static const char	*podman_rundir;

void
podman_setup(container_engine_t *dp)
{
    const char		*podman_default_datadir = CONTAINERS_DATADIR;
    const char		*podman_default_rundir = CONTAINERS_RUNDIR;
    char		path[MAXPATHLEN];

    podman_datadir = getenv("PCP_PODMAN_DATADIR");
    podman_rundir = getenv("PCP_PODMAN_RUNDIR");

    /* determine the location of podman container config.json files */
    if (podman_datadir == NULL)
	podman_datadir = podman_default_datadir;
    if (podman_rundir == NULL)
	podman_rundir = podman_default_rundir;
    pmsprintf(path, sizeof(path), "%s/containers.json", podman_datadir);
    dp->path = strdup(path);

    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "podman setup %s (rundir %s)\n", dp->path,
				podman_rundir);
}

int
podman_indom_changed(container_engine_t *dp)
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

static char *
podman_cgroup_find(char *name, int namelen, char *path, int pathlen)
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
	if (*(base = &drp->d_name[0]) == '.' ||
	    strcmp(base, "user.slice") == 0)
	    continue;
	child = base;
	bytes = strlen(base);
	if (pathlen + bytes + 2 >= MAXPATHLEN)
	    continue;
	/* accept either a direct name match or libpod- prefixed */
	if (strncmp(child, "libpod-", sizeof("libpod-")-1) == 0)
	    child += sizeof("libpod-")-1;
	childlen = pathlen;
	path[childlen++] = '/';
	strncpy(path + childlen, base, bytes + 1);
	if (strncmp(child, name, namelen) == 0) {
	    found = 1;
	    break;
	}
	/* didn't match - descend into this (possible) directory */
	child = podman_cgroup_find(name, namelen, path, childlen + bytes);
	if (child != NULL) {
	    path = child;
	    found = 1;
	    break;
	}
    }
    closedir(finddir);

    return found ? path : NULL;
}

static void
podman_cgroup_search(char *name, int namelen, char *cgroups, container_t *cp)
{
    char		path[MAXPATHLEN], *p;
    int			pathlen;

    /* Using "unified" (cgroup2) or "memory" (cgroup1) as starting
     * points, recursively descend looking for a named container.
     */
    pathlen = pmsprintf(path, sizeof(path), "%s/%s", cgroups, "unified");
    if ((p = podman_cgroup_find(name, namelen, path, pathlen)) != NULL) {
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "%s", p + pathlen);
	return;
    }
    pathlen = pmsprintf(path, sizeof(path), "%s/%s", cgroups, "memory");
    if ((p = podman_cgroup_find(name, namelen, path, pathlen)) != NULL) {
	pmsprintf(cp->cgroup, sizeof(cp->cgroup), "%s", p + pathlen);
	return;
    }
    /* Else fallback to using a default naming conventions. */
    pmsprintf(cp->cgroup, sizeof(cp->cgroup), "libpod-%s.scope", name);
}

static container_t *
podman_inst_insert(pmInDom indom, char *name, char *cgroup, container_engine_t *dp)
{
    container_t		*cp;

    /* allocate space for values for this container and update indom */
    if (pmDebugOptions.attr)
	fprintf(stderr, "%s: adding podman container %s\n",
		pmGetProgname(), name);
    if ((cp = calloc(1, sizeof(container_t))) != NULL) {
	cp->engine = dp;
	podman_cgroup_search(name, strlen(name), cgroup, cp);
    }
    return cp;
}

enum parse_state {
    STATE_INIT = 0,
    STATE_CONTAINER_ARRAY,
    STATE_CONTAINER_MAP,
    STATE_CONTAINER_MAPID,
    STATE_CONTAINER_NAMES,
    STATE_SKIP
};

typedef struct parser {
    enum parse_state	state;
    enum parse_state	pushed;
    char		*cgroup;
    char		*token;
    unsigned int	level;
    pmInDom		indom;
    int			inst;
    container_engine_t	*dp;
} parser_t;

/* termination of an element */
static void
podman_parser_pop(jsonsl_t jsn, jsonsl_action_t action,
		struct jsonsl_state_st *state, const char *at)
{
    struct parser	*parser = (struct parser *)jsn->data;
    container_t		*cp;
    pmInDom		indom;
    char		*cgroup;
    int			sts;

    if (state->type == JSONSL_T_LIST) {
	if (parser->state == STATE_CONTAINER_NAMES) {
	    parser->state = STATE_CONTAINER_MAP;
	    parser->inst = -1;
	}
	else if (parser->state == STATE_SKIP)
	    if (--parser->level == 0)
		parser->state = parser->pushed;
    } else if (state->type == JSONSL_T_OBJECT) {
	if (parser->state == STATE_CONTAINER_MAP) {
	    parser->state = STATE_CONTAINER_ARRAY;
	    parser->inst = -1;
	}
	else if (parser->state == STATE_SKIP)
	    if (--parser->level == 0)
		parser->state = parser->pushed;
    } else if (state->type == JSONSL_T_HKEY) {
	if (parser->state == STATE_CONTAINER_MAP && parser->token) {
	    if (strncmp(parser->token, "id\"", 3) == 0) {
		parser->state = STATE_CONTAINER_MAPID;
		parser->token = NULL;
	    } else if (strncmp(parser->token, "names\"", 6) == 0) {
		parser->state = STATE_CONTAINER_NAMES;
		parser->token = NULL;
	    }
	}
    } else if (state->type == JSONSL_T_STRING) {
        if (*at != '"')
            return;
        *(char *)at = '\0';
	indom = parser->indom;
	if (parser->state == STATE_CONTAINER_NAMES) {
	    if (pmDebugOptions.attr)
		fprintf(stderr, "%s: name %s\n", "podman_parser", parser->token);
	    /* insert struct into pmdaCache, save pointer */
	    sts = pmdaCacheLookup(indom, parser->inst, NULL, (void **)&cp);
	    cp->name = strdup(parser->token);
	    parser->token = NULL;
	} else if (parser->state == STATE_CONTAINER_MAPID) {
	    if (pmDebugOptions.attr)
		fprintf(stderr, "%s: ID %s\n", "podman_parser", parser->token);
	    parser->state = STATE_CONTAINER_MAP;
	    /* insert into pmdaCache, keeping track of inst identifier */
	    sts = pmdaCacheLookupName(indom, parser->token, NULL, (void **)&cp);
	    if (sts != PMDA_CACHE_INACTIVE) {
		cgroup = parser->cgroup;
		cp = podman_inst_insert(indom, parser->token, cgroup, parser->dp);
	    }
	    sts = pmdaCacheStore(indom, PMDA_CACHE_ADD, parser->token, cp);
	    if (sts >= 0)
		parser->inst = sts;
	    parser->token = NULL;
	}
    }
}

/* start of a new element */
static
void podman_parser_push(jsonsl_t jsn, jsonsl_action_t action,
		struct jsonsl_state_st *state, const char *at)
{
    struct parser	*parser = (struct parser *)jsn->data;

    if (state->type == JSONSL_T_LIST) {
	if (parser->state == STATE_INIT)
	    parser->state = STATE_CONTAINER_ARRAY;
	else if (parser->state == STATE_SKIP)
	    parser->level++;
	else if (parser->state != STATE_CONTAINER_NAMES) {
	    parser->pushed = parser->state;
	    parser->state = STATE_SKIP;
	    parser->level = 1;
	}
    } else if (state->type == JSONSL_T_OBJECT) {
	if (parser->state == STATE_CONTAINER_ARRAY)
	    parser->state = STATE_CONTAINER_MAP;
	else if (parser->state == STATE_SKIP)
	    parser->level++;
	else {
	    parser->pushed = parser->state;
	    parser->state = STATE_SKIP;
	    parser->level = 1;
	}
    } else if (state->type == JSONSL_T_HKEY) {
	if (parser->state == STATE_CONTAINER_MAP)
	    parser->token = (char *)at + 1;	/* skip opening quote */
    } else if (state->type == JSONSL_T_STRING) {
	if (parser->state == STATE_CONTAINER_NAMES ||
	    parser->state == STATE_CONTAINER_MAPID)
	    parser->token = (char *)at + 1;	/* skip opening quote */
    }
}

static int
podman_error_callback(jsonsl_t jsn, jsonsl_error_t err,
		struct jsonsl_state_st *state, char *at)
{
    struct parser	*parser = (struct parser *)jsn->data;

    fprintf(stderr, "%s: error at offset %lu of %s: %s\n", "podman_parser",
		    (unsigned long)jsn->pos, parser->dp->path, jsonsl_strerror(err));
    (void)state;
    (void)at;
    return 0;
}

/*
 * Scan /var/lib/containers/storage/overlay-containers/containers.json names,
 * then /var/run/containers/storage/overlay-containers/ID/userdata/pidfile to
 * find which containers are running.
 */
void
podman_insts_refresh(container_engine_t *dp, pmInDom indom)
{
    static const char	*cgroup_default = "/sys/fs/cgroup";
    static char		*cgroup;
    static jsonsl_t	jsn;
    parser_t		parse = {0};
    ssize_t		bytes;
    char		buffer[BUFSIZ];
    int			fd;

    if (cgroup == NULL && (cgroup = getenv("PCP_CGROUP_DIR")) == NULL)
	cgroup = (char *)cgroup_default;

    if ((fd = open(dp->path, O_RDONLY)) < 0)
	return;

    if (jsn == NULL)
	jsn = jsonsl_new(MAX_RECURSION_DEPTH);
    else
	jsonsl_reset(jsn);

    jsonsl_enable_all_callbacks(jsn);
    jsn->action_callback = NULL;
    jsn->action_callback_PUSH = podman_parser_push;
    jsn->action_callback_POP = podman_parser_pop;
    jsn->error_callback = podman_error_callback;
    jsn->max_callback_level = MAX_DESCENT_LEVEL;

    parse.dp = dp;
    parse.inst = -1;
    parse.indom = indom;
    parse.cgroup = cgroup;
    jsn->data = &parse;

    while ((bytes = read(fd, buffer, sizeof(buffer))) > 0) {
	jsonsl_feed(jsn, buffer, bytes);
        if (bytes < sizeof(buffer))
            break;
    }
    close(fd);
}

static int
podman_values_changed(const char *path, container_t *values)
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
podman_container_process(const char *path)
{
    FILE	*fp = fopen(path, "r");
    int		pid, count;

    if (fp) {
	count = fscanf(fp, "%d", &pid);
	fclose(fp);
	if (count == 1 && __pmProcessExists(pid))
	    return pid;
    }
    return 0;
}

/*
 * Extract critical information (PID1, state) for a named container.
 * Name here is the unique identifier we've chosen to use for podman
 * container external instance names (i.e. long hash names).
 */
int
podman_value_refresh(container_engine_t *dp,
		     const char *name, container_t *values)
{
    char	path[MAXPATHLEN];

    values->uptodate = 0;

    /* /var/run/containers/storage/overlay-containers/[HASH]/userdata/pidfile */
    pmsprintf(path, sizeof(path), "%s/%s/userdata/pidfile", podman_rundir, name);
    if (!podman_values_changed(path, values))
	return 0;
    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "podman_value_refresh: file=%s\n", path);

    values->pid = podman_container_process(path);
    if (values->name)
	values->uptodate++;
    if (values->pid > 0)
	values->flags = CONTAINER_FLAG_RUNNING;
    else
	values->flags = 0;
    values->uptodate++;

    if (pmDebugOptions.attr)
	pmNotifyErr(LOG_DEBUG, "podman_value_refresh: pid=%d uptodate=%d of %d\n",
	    values->pid, values->uptodate, NUM_UPTODATE);

    return values->uptodate == NUM_UPTODATE ? 0 : PM_ERR_AGAIN;
}

/*
 * Given two strings, determine if they identify the same container.
 * This is called iteratively, passing over all container instances.
 *
 * For podman we need to match user-supplied names (which podman will
 * have assigned itself, if none given - see 'podman ps').  Also, we
 * want to allow for matching on the container hash identifiers we
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
podman_name_matching(struct container_engine *dp, const char *query,
	const char *username, const char *instname)
{
    unsigned int ilength, qlength, limit;
    int i, fuzzy = 0;

    if (username) {
	if (strcmp(query, username) == 0)
	    return 100;
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
