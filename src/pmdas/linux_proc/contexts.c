/*
 * Copyright (c) 2013,2015,2017,2024 Red Hat.
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
#include "contexts.h"
#include <ctype.h>
#include <pwd.h>

static char **ctxusers;
static proc_perctx_t *ctxtab;
static int ctx_passwd_mapped;
static int num_ctx;
static uid_t baseuid;
static gid_t basegid;

static void
proc_ctx_clear(int ctx)
{
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].uid = -1;
    ctxtab[ctx].gid = -1;
    ctxtab[ctx].threads = 1;
    ctxtab[ctx].cgroups = NULL;
    memset(&ctxtab[ctx].container, 0, sizeof(proc_container_t));
}

void
proc_ctx_end(int ctx)
{
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE)
	return;
    if (ctxtab[ctx].state & CTX_CONTAINER)
	free((void *)ctxtab[ctx].container.name);
    if (ctxtab[ctx].state & CTX_CGROUPS)
	free((void *)ctxtab[ctx].cgroups);
    proc_ctx_clear(ctx);
}

static void
proc_ctx_growtab(int ctx)
{
    size_t need;

    if (ctx < num_ctx)
        return;

    need = (ctx + 1) * sizeof(ctxtab[0]);
    ctxtab = (proc_perctx_t *)realloc(ctxtab, need);
    if (ctxtab == NULL)
        pmNoMem("proc ctx table", need, PM_FATAL_ERR);
    while (num_ctx <= ctx)
	proc_ctx_clear(num_ctx++);
}

static void
proc_ctx_set_userid(int ctx, const char *value)
{
    proc_ctx_growtab(ctx);
    ctxtab[ctx].uid = atoi(value);
    ctxtab[ctx].state |= (CTX_ACTIVE | CTX_USERID);
}

static int
proc_ctx_allow_username(const char *value)
{
    char *username;

    if (!ctxusers)
	return 0; /* no allowed usernames, denied */
    for (username = *ctxusers; username; username++)
	if (strcmp(username, value) == 0)
	    return 1;
    return 0; /* user not in allowed list, denied */
}

static void
proc_ctx_set_username(int ctx, const char *value)
{
    if (ctx_passwd_mapped) {
	struct passwd *entry = getpwnam(value);
	if (!entry)
	    return; /* no such user, denied */
	if (!proc_ctx_allow_username(value))
	    return; /* not allowed, denied */
	proc_ctx_growtab(ctx);
	ctxtab[ctx].uid = entry->pw_uid;
	ctxtab[ctx].gid = entry->pw_gid;
	ctxtab[ctx].state |= (CTX_ACTIVE | CTX_USERID | CTX_GROUPID);
    } else {
	if (!proc_ctx_allow_username(value))
	    return; /* not allowed, denied */
	proc_ctx_growtab(ctx);
	ctxtab[ctx].state |= (CTX_ACTIVE | CTX_USERNAME);
    }
}

static void
proc_ctx_set_groupid(int ctx, const char *value)
{
    proc_ctx_growtab(ctx);
    ctxtab[ctx].gid = atoi(value);
    ctxtab[ctx].state |= (CTX_ACTIVE | CTX_GROUPID);
}

static void
proc_ctx_set_container(int ctx, const char *value, int length)
{
    char	*name = length > 1 ? strndup(value, length) : NULL;

    proc_ctx_growtab(ctx);
    ctxtab[ctx].container.pid = 0;
    if (name) {
	ctxtab[ctx].container.name = name;
	ctxtab[ctx].container.length = length;
	ctxtab[ctx].state |= CTX_CONTAINER;
    } else {
	if (ctxtab[ctx].container.name != NULL)
	    free((void *)ctxtab[ctx].container.name);
	ctxtab[ctx].container.name = NULL;
	ctxtab[ctx].container.length = 0;
	ctxtab[ctx].state &= ~CTX_CONTAINER;
    }
    ctxtab[ctx].state |= CTX_ACTIVE;
}

int
proc_ctx_attrs(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    int	sts;

    if ((sts = pmdaAttribute(ctx, attr, value, length, pmda)) < 0)
	return sts;

    switch (attr) {
    case PMDA_ATTR_USERID:
	proc_ctx_set_userid(ctx, value);
	break;
    case PMDA_ATTR_GROUPID:
	proc_ctx_set_groupid(ctx, value);
	break;
    case PMDA_ATTR_USERNAME:
	proc_ctx_set_username(ctx, value);
	break;
    case PMDA_ATTR_CONTAINER:
	proc_ctx_set_container(ctx, value, length);
	break;
    default:
	break;
    }
    return 0;
}

static char *
proc_ctx_add_username(char *value)
{
    size_t length, count, bytes = strlen(value);
    char **p = NULL, **q;

    if (!bytes)
	return value;

    if (!ctxusers)
	count = 1;
    else
	for (count = 1, p = ctxusers; *p; p++)
	    count++;

    length = (count + 1) * sizeof(char *);
    if ((q = realloc(ctxusers, length)) == NULL) {
	pmNoMem("proc_ctx_add_username table", length, PM_RECOV_ERR);
	return value + bytes;
    }

    if (ctxusers)
	p = q + (p - ctxusers);
    else
	p = q;

    if ((*p = strndup(value, bytes)) == NULL) {
	pmNoMem("proc_ctx_add_username value", bytes, PM_RECOV_ERR);
	free(q);
    } else {
	*(++p) = NULL; /* sentinel */
	ctxusers = q;
    }
    return value + bytes;
}

static char *
proc_ctx_read_allowed(char *value)
{
    char *token, *p = value;

    if ((token = strchr(p, '#')) != NULL)
	*token = '\0';
    /* extract list of user names and append to table */
    while ((token = strsep(&p, " ,\n")) != NULL)
	proc_ctx_add_username(token);
    return p;
}

static char *
proc_ctx_read_mapped(char *value)
{
    char *token, *p = value;

    if ((token = strchr(p, '#')) != NULL)
	*token = '\0';
    /* extract boolean setting: getpwnam users or not */
    while ((token = strsep(&p, " \n")) != NULL)
	if (strcmp(token, "1") == 0 || strcmp(token, "true") == 0)
	    ctx_passwd_mapped = 1;
    return p;
}

static void
proc_ctx_config_read(void)
{
    char        filename[MAXPATHLEN], buffer[128], *p;
    FILE        *file;
    int         skip = 0;

    pmsprintf(filename, sizeof(filename), "%s/proc/access.conf",
			pmGetConfig("PCP_SYSCONF_DIR"));
    if ((file = fopen(filename, "r")) != NULL) {
	while (fgets(buffer, sizeof(buffer), file)) {
	    for (p = buffer; *p; p++) {
		if (*p == '#')              /* skip over comments */
		    skip = 1;
		else if (*p == '\n')        /* finish the comment */
		    skip = 0;
		if (skip || isspace(*p))
		    continue;
		if (strncmp(buffer, "mapped:", 7) == 0)
		    p = proc_ctx_read_mapped(p + 8);
		else if (strncmp(buffer, "allowed:", 8) == 0)
		    p = proc_ctx_read_allowed(p + 9);
		if (!p) {
		    skip = 0;
		    break;
		}
	    }
	}
	fclose(file);
    }
}

void
proc_context_init(void)
{
    /* UID and GID of the PMDA process (typically 'root' or 'pcp') */
    baseuid = getuid();
    basegid = getgid();

    /* parse configuration file; setup ctxusers, ctx_passwd_mapped */
    proc_ctx_config_read();
}

int
proc_ctx_getuid(int ctx)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return -1;

    pp = &ctxtab[ctx];

    if ((pp->state & CTX_ACTIVE) && (pp->state & CTX_USERID))
	return pp->uid;
    else
	return -1;
}

int
proc_ctx_access(int ctx)
{
    proc_perctx_t *pp;
    int accessible = 0;

    if (ctx < 0 || ctx >= num_ctx)
	return accessible;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return accessible;

    if (pp->state & CTX_USERNAME)
	return 1;
    if (pp->state & CTX_GROUPID) {
	accessible++;
	if (basegid != pp->gid) {
	    if (setresgid(pp->gid, pp->gid, -1) < 0) {
		pmNotifyErr(LOG_ERR, "set*gid(%d) access failed: %s\n",
			      pp->gid, osstrerror());
		accessible--;
	    }
	}
    }
    if (pp->state & CTX_USERID) {
	accessible++;
	if (baseuid != pp->uid) {
	    if (setresuid(pp->uid, pp->uid, -1) < 0) {
		pmNotifyErr(LOG_ERR, "set*uid(%d) access failed: %s\n",
			      pp->uid, osstrerror());
		accessible--;
	    }
	}
    }
    return (accessible > 1);
}

int
proc_ctx_revert(int ctx)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return 0;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return 0;
    if (pp->state & CTX_USERNAME)
	return 0;

    if ((pp->state & CTX_USERID) && baseuid != pp->uid) {
	if (setresuid(baseuid, baseuid, -1) < 0)
	    pmNotifyErr(LOG_ERR, "set*uid(%d) revert failed: %s\n",
			  baseuid, osstrerror());
    }
    if ((pp->state & CTX_GROUPID) && basegid != pp->gid) {
	if (setresgid(basegid, basegid, -1) < 0)
	    pmNotifyErr(LOG_ERR, "set*gid(%d) revert failed: %s\n",
			  basegid, osstrerror());
    }
    return 0;
}

unsigned int
proc_ctx_threads(int ctx, unsigned int threads)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return threads;	/* fallback to default */
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return threads;	/* fallback to default */

    if (pp->state & CTX_THREADS)
	return pp->threads;	/* client setting */

    return threads;	/* fallback to default */
}

int
proc_ctx_set_threads(int ctx, unsigned int threads)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return PM_ERR_NOCONTEXT;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return PM_ERR_NOCONTEXT;
    if (threads > 1)
	return PM_ERR_BADSTORE;

    pp->state |= CTX_THREADS;
    pp->threads = threads;
    return 0;
}

proc_container_t *
proc_ctx_container(int ctx)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return NULL;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return NULL;
    if (pp->state & CTX_CONTAINER)
	return &pp->container;
    return NULL;
}

const char *
proc_ctx_cgroups(int ctx, const char *cgroups)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return cgroups;	/* fallback to default */
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return cgroups;	/* fallback to default */

    if (pp->state & CTX_CGROUPS)
	return pp->cgroups;	/* client setting */

    return cgroups;	/* fallback to default */
}

int
proc_ctx_set_cgroups(int ctx, const char *cgroups)
{
    proc_perctx_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
	return PM_ERR_NOCONTEXT;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
	return PM_ERR_NOCONTEXT;
    if (cgroups == NULL || cgroups[0] == '\0')
	return PM_ERR_BADSTORE;

    pp->state |= CTX_CGROUPS;
    pp->cgroups = cgroups;
    return 0;
}
