/*
 * Copyright (c) 2014-2015 Red Hat.
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

#include <sys/stat.h>
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"

#include "root.h"
#include "jsmn.h"
#include "docker.h"

/*
 * JSMN helper interfaces for efficiently extracting JSON configs
 */

static int
jsmneq(const char *js, jsmntok_t *tok, const char *s)
{
    if (tok->type != JSMN_STRING)
	return -1;
    if (strlen(s) == tok->end - tok->start &&
	strncasecmp(js + tok->start, s, tok->end - tok->start) == 0)
	return 0;
    return -1;
}

static int
jsmnflag(const char *js, jsmntok_t *tok, int *bits, int flag)
{
    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    if (strncmp(js + tok->start, "true", sizeof("true")-1) == 0)
	*bits |= flag;
    else
	*bits &= ~flag;
    return 0;
}

static int
jsmnint(const char *js, jsmntok_t *tok, int *value)
{
    char	buffer[64];

    if (tok->type != JSMN_PRIMITIVE)
	return -1;
    strncpy(buffer, js + tok->start, tok->end - tok->start);
    buffer[tok->end - tok->start] = '\0';
    *value = (int)strtol(buffer, NULL, 0);
    return 0;
}

static int
jsmnstrdup(const char *js, jsmntok_t *tok, char **name)
{
    char	*s = *name;

    if (tok->type != JSMN_STRING)
	return -1;
    if (s)
	free(s);
    s = strndup(js + tok->start, tok->end - tok->start);
    return ((*name = s) == NULL) ? -1 : 0;
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
     const char *docker = getenv("PCP_DOCKER_DIR");

     if (!docker)
	docker = docker_default;
     snprintf(dp->path, sizeof(dp->path), "%s/containers", docker);
     dp->path[sizeof(dp->path)-1] = '\0';

    if (pmDebug & DBG_TRACE_ATTR)
	__pmNotifyErr(LOG_DEBUG, "docker_setup: using path: %s\n", dp->path);
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
	if (pmDebug & DBG_TRACE_ATTR)
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
	    if (pmDebug & DBG_TRACE_ATTR)
		fprintf(stderr, "%s: adding docker container %s\n",
			pmProgname, path);
	    if ((cp = calloc(1, sizeof(container_t))) == NULL)
		continue;
	    cp->engine = dp;
	    snprintf(cp->cgroup, sizeof(cp->cgroup),
			"system.slice/docker-%s.scope", path);
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
docker_values_extract(const char *js, jsmntok_t *t, size_t count,
			int key, container_t *values)
{
    int		i, j;

    if (count == 0)
	return 0;
    switch (t->type) {
    case JSMN_PRIMITIVE:
	return 1;
    case JSMN_STRING:
	/*
	 * We're only interested in a handful of values:
	 * "Name": "/jolly_sinoussi",
	 * "State": { "Running": true, "Paused": false, "Restarting": false,
	 *            "Pid": 32471 }
	 */
	if (key) {
	    jsmntok_t	*value = t + 1;

	    if (t->parent == 0) {	/* top-level: look for Name & State */
		if (jsmneq(js, t, "Name") == 0) {
		    jsmnstrdup(js, value, &values->name);
		    values->uptodate++;
		}
		if (jsmneq(js, t, "State") == 0) {
		    values->state = (value->type == JSMN_OBJECT);
		    values->uptodate++;
		}
	    }
	    else if (values->state) { /* pick out various stateful values */
		int 	flag = values->flags;

		if (pmDebug & DBG_TRACE_ATTR)
		    __pmNotifyErr(LOG_DEBUG, "docker_values_parse: state\n");

		if (jsmneq(js, t, "Running") == 0)
		    jsmnflag(js, value, &flag, CONTAINER_FLAG_RUNNING);
		else if (jsmneq(js, t, "Paused") == 0)
		    jsmnflag(js, value, &flag, CONTAINER_FLAG_PAUSED);
		else if (jsmneq(js, t, "Restarting") == 0)
		    jsmnflag(js, value, &flag, CONTAINER_FLAG_RESTARTING);
		else if (jsmneq(js, t, "Pid") == 0) {
		    if (jsmnint(js, value, &values->pid) < 0)
			values->pid = -1;
		    if (pmDebug & DBG_TRACE_ATTR)
			__pmNotifyErr(LOG_DEBUG, "docker_value PID=%d\n",
					values->pid);
		}
		values->flags = flag;
	    }
	}
	return 1;
    case JSMN_OBJECT:
	for (i = j = 0; i < t->size; i++) {
	    j += docker_values_extract(js, t+1+j, count-j, 1, values); /* key */
	    j += docker_values_extract(js, t+1+j, count-j, 0, values); /*value*/
	}
	values->state = 0;
	return j + 1;
    case JSMN_ARRAY:
	for (i = j = 0; i < t->size; i++)
	    j += docker_values_extract(js, t+1+j, count-j, 0, values);
	return j + 1;
    default:
	return 0;
    }
    return 0;
}

static int
docker_values_parse(FILE *fp, const char *name, container_t *values)
{
    static char		*js;
    static int		jslen;
    static int		tokcount;
    static jsmntok_t	*tok;
    jsmn_parser		p;
    char		buf[BUFSIZ];
    int			n, sts = 0, eof_expected = 0;

    if (pmDebug & DBG_TRACE_ATTR)
	__pmNotifyErr(LOG_DEBUG, "docker_values_parse: name=%s\n", name);

    if (!tok) {
	tokcount = 128;
	if ((tok = calloc(tokcount, sizeof(*tok))) == NULL)
	    return -ENOMEM;
    }
    if (jslen)
	jslen = 0;

    jsmn_init(&p);
    values->uptodate = 0;	/* values for this container not yet visible */
    values->state = -1;		/* reset State key marker for this iteration */

    for (;;) {
	/* Read another chunk */
	n = fread(buf, 1, sizeof(buf), fp);
	if (n < 0) {
	    if (pmDebug & DBG_TRACE_ATTR) {
		fprintf(stderr, "%s: failed read on docker %s config: %s\n",
			pmProgname, name, osstrerror());
		sts = -oserror();
		break;
	    }
	}
	if (n == 0) {
	    if (!eof_expected) {
		if (pmDebug & DBG_TRACE_ATTR)
		    fprintf(stderr, "%s: unexpected EOF on %s config: %s\n",
			    pmProgname, name, osstrerror());
		sts = -EINVAL;
		break;
	    }
	    return 0;
	}

	if ((js = realloc(js, jslen + n + 1)) == NULL) {
	    sts = -ENOMEM;
	    break;
	}
	strncpy(js + jslen, buf, n);
	jslen = jslen + n;

again:
	n = jsmn_parse(&p, js, jslen, tok, tokcount);
	if (n < 0) {
	    if (n == JSMN_ERROR_NOMEM) {
		tokcount = tokcount * 2;
		if ((tok = realloc(tok, sizeof(*tok) * tokcount)) == NULL) {
		    sts = -ENOMEM;
		    break;
		}
		goto again;
	    }
	} else {
	    sts = docker_values_extract(js, tok, p.toknext, 0, values);
	    eof_expected = 1;
	}
    }

    return sts;
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

    snprintf(path, sizeof(path), "%s/%s/config.json", dp->path, name);
    if (!docker_values_changed(path, values))
	return 0;
    if (pmDebug & DBG_TRACE_ATTR)
	__pmNotifyErr(LOG_DEBUG, "docker_value_refresh: file=%s\n", path);
    if ((fp = fopen(path, "r")) == NULL)
	return -oserror();
    sts = docker_values_parse(fp, name, values);
    fclose(fp);
    if (sts < 0)
	return sts;

    if (pmDebug & DBG_TRACE_ATTR)
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

    if (strcmp(query, username) == 0)
	return 100;
    if (username[0] == '/' && strcmp(query, username+1) == 0)
	return 99;
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
