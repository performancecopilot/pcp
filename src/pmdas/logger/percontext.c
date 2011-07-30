/*
 * Some functions become per-client (of pmcd) with the introduction
 * of PMDA_INTERFACE_5.
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
 * Copyright (c) 2011 Red Hat Inc.
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
#include "percontext.h"
#include <regex.h>

typedef struct {
    unsigned int	state;		/* active|inactive|access context */
    void		*user_data;	/* user data */
    void		*filter_data;
} perctx_t;

/* values for state */
#define CTX_INACTIVE	0
#define CTX_ACTIVE	0x1
#define CTX_ACCESS	0x2

static perctx_t	*ctxtab;
static int	num_ctx;		/* number of active contexts */
static int	num_ctx_allocated;	/* number of allocated contexts */
static int	last_ctx = -1;

static ctxStartContextCallBack ctx_start_cb;
static ctxEndContextCallBack ctx_end_cb;

static void
growtab(int ctx)
{
    ctxtab = (perctx_t *)realloc(ctxtab, (ctx+1)*sizeof(ctxtab[0]));
    if (ctxtab == NULL) {
	__pmNoMem("growtab", (ctx+1)*sizeof(ctxtab[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    while (num_ctx_allocated <= ctx) {
	ctxtab[num_ctx_allocated].state = CTX_INACTIVE;
	ctxtab[num_ctx_allocated].user_data = NULL;
	ctxtab[num_ctx_allocated].filter_data = NULL;
	num_ctx_allocated++;
    }
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].user_data = NULL;
    ctxtab[ctx].filter_data = NULL;
}

int
ctx_active(int ctx)
{
    if (ctx < 0)
	return -1;
    if (ctx >= num_ctx_allocated)
	growtab(ctx);
    last_ctx = ctx;
    if (ctxtab[ctx].state == 0) {
	num_ctx++;
	ctxtab[ctx].state = CTX_ACTIVE;
	if (ctx_start_cb) {
	    ctxtab[ctx].user_data = ctx_start_cb(ctx);
	    ctxtab[ctx].filter_data = NULL;
	}
	if (pmDebug & DBG_TRACE_APPL2)
	    __pmNotifyErr(LOG_INFO, "%s: saw new context %d (num_ctx=%d)\n",
		      __FUNCTION__, ctx, num_ctx);
    }
    return 0;
}

void
ctx_end(int ctx)
{
    if (pmDebug & DBG_TRACE_APPL2) {
	fprintf(stderr, "sample_ctx_end(%d) [context is ", ctx);
	if (ctx < 0 || ctx >= num_ctx_allocated)
	    fprintf(stderr, "unknown, num_ctx=%d", num_ctx);
	else {
	    if (ctxtab[ctx].state & CTX_ACCESS)
		fprintf(stderr, "accessible+");
	    if (ctxtab[ctx].state & CTX_ACTIVE)
		fprintf(stderr, "active");
	    if (ctxtab[ctx].state == 0)
		fprintf(stderr, "inactive");
	}
	fprintf(stderr, "]\n");
    }

    if (ctx < 0 || ctx >= num_ctx_allocated || ctxtab[ctx].state == 0) {
	/*
	 * This is expected ... when a context is closed in pmcd
	 * (or for a local context or for dbpmda or ...) all the
	 * PMDAs with a registered pmdaEndContextCallBack will be
	 * called end some of the PMDAs may not have not serviced
	 * any previous requests for that context.
	 */
	return;
    }
    if (ctx_end_cb) {
	ctx_end_cb(ctx, ctxtab[ctx].user_data);
    }
    num_ctx--;
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].user_data = NULL;
    ctxtab[ctx].filter_data = NULL;
}

int
ctx_get_num(void)
{
    return num_ctx;
}

void
ctx_register_callbacks(ctxStartContextCallBack start,
		       ctxEndContextCallBack end)
{
    ctx_start_cb = start;
    ctx_end_cb = end;
}

/* Returns the user data associated with the current client context. */
void *
ctx_get_user_data(void)
{
    if (last_ctx < 0 || last_ctx >= num_ctx_allocated
	|| ctxtab[last_ctx].state == 0)
	return NULL;
    return ctxtab[last_ctx].user_data;
}

/*
 * Marks context as having been pmStore'd into (access allowed),
 * adds optional filtering data for the current client context.
 */
void
ctx_set_filter_data(void *filter)
{
    if (last_ctx < 0 || last_ctx >= num_ctx_allocated)
	return;
    if (ctxtab[last_ctx].state == 0)	/* inactive */
	return;
    ctxtab[last_ctx].state |= CTX_ACCESS;
    ctxtab[last_ctx].filter_data = filter;
}

/* Returns any filtering data for the current client context. */
void *
ctx_get_filter_data(void)
{
    if (last_ctx < 0 || last_ctx >= num_ctx_allocated
	|| ctxtab[last_ctx].state == 0)
	return NULL;
    return ctxtab[last_ctx].filter_data;
}

/* Returns true/false access for the current client context. */
int
ctx_get_user_access(void)
{
    if (last_ctx < 0 || last_ctx >= num_ctx_allocated)
	return 0;
    if (ctxtab[last_ctx].state == 0)	/* inactive */
	return 0;
    return ((ctxtab[last_ctx].state & CTX_ACCESS) == CTX_ACCESS);
}

/* Visit each active context and run a supplied callback routine */
void
ctx_iterate(ctxVisitContextCallBack visit, int id, void *call_data)
{
    int ctx;

    for (ctx = 0; ctx < num_ctx_allocated; ctx++) {
	if (ctxtab[ctx].state == CTX_INACTIVE)
	    continue;
	visit(ctx, id, ctxtab[ctx].user_data, call_data);
    }
}
