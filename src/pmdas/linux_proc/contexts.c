/*
 * Copyright (c) 2013 Red Hat.
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
#include "contexts.h"

static proc_perctx_t *ctxtab;
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
}

void
proc_ctx_end(int ctx)
{
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE)
	return;
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
        __pmNoMem("proc ctx table", need, PM_FATAL_ERR);
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

static void
proc_ctx_set_groupid(int ctx, const char *value)
{
    proc_ctx_growtab(ctx);
    ctxtab[ctx].gid = atoi(value);
    ctxtab[ctx].state |= (CTX_ACTIVE | CTX_GROUPID);
}

int
proc_ctx_attrs(int ctx, int attr, const char *value, int length, pmdaExt *pmda)
{
    if (pmDebug & DBG_TRACE_AUTH) {
	char buffer[256];

	if (!__pmAttrStr_r(attr, value, buffer, sizeof(buffer))) {
	    __pmNotifyErr(LOG_ERR, "Bad Attribute: ctx=%d, attr=%d\n", ctx, attr);
	} else {
	    buffer[sizeof(buffer)-1] = '\0';
	    __pmNotifyErr(LOG_INFO, "Attribute: ctx=%d %s", ctx, buffer);
	}
    }

    switch (attr) {
    case PCP_ATTR_USERID:
	proc_ctx_set_userid(ctx, value);
	break;
    case PCP_ATTR_GROUPID:
	proc_ctx_set_groupid(ctx, value);
	break;
    default:
	break;
    }
    return 0;
}

void
proc_ctx_init(void)
{
    baseuid = getuid();
    basegid = getgid();
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

    if (pp->state & CTX_GROUPID) {
	accessible++;
	if (basegid != pp->gid) {
	    if (setegid(pp->gid) < 0) {
		__pmNotifyErr(LOG_ERR, "setegid(%d) access failed: %s\n",
			      pp->gid, osstrerror());
		accessible--;
	    }
	}
    }
    if (pp->state & CTX_USERID) {
	accessible++;
	if (baseuid != pp->uid) {
	    if (seteuid(pp->uid) < 0) {
		__pmNotifyErr(LOG_ERR, "seteuid(%d) access failed: %s\n",
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

    if ((pp->state & CTX_USERID) && baseuid != pp->uid) {
	if (seteuid(baseuid) < 0)
	    __pmNotifyErr(LOG_ERR, "seteuid(%d) revert failed: %s\n",
			  baseuid, osstrerror());
    }
    if ((pp->state & CTX_GROUPID) && basegid != pp->gid) {
	if (setegid(basegid) < 0)
	    __pmNotifyErr(LOG_ERR, "setegid(%d) revert failed: %s\n",
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
	return PM_ERR_CONV;

    pp->state |= CTX_THREADS;
    pp->threads = threads;
    return 0;
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
	return PM_ERR_CONV;

    pp->state |= CTX_CGROUPS;
    pp->cgroups = cgroups;
    return 0;
}
