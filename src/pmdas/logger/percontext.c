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

typedef struct {
    int		state;		/* active or inactive context */
    void       *user_data;	/* user data */
} perctx_t;

/* values for state */
#define CTX_INACTIVE	0
#define CTX_ACTIVE	1

static perctx_t	*ctxtab = NULL;
static int	num_ctx = 0;	       /* number of active contexts */
static int	num_ctx_allocated = 0; /* number of allocated contexts */
static int	last_ctx = -1;

static ctxStartContextCallBack ctx_start_cb = NULL;
static ctxEndContextCallBack ctx_end_cb = NULL;

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
	num_ctx_allocated++;
    }
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].user_data = NULL;
}

int
ctx_start(int ctx)
{
    if (ctx < 0) {
	return -1;
    }
    if (ctx >= num_ctx_allocated)
	growtab(ctx);
    last_ctx = ctx;
    if (ctxtab[ctx].state == CTX_INACTIVE) {
	num_ctx++;
	ctxtab[ctx].state = CTX_ACTIVE;
	if (ctx_start_cb) {
	    ctxtab[ctx].user_data = ctx_start_cb(ctx);
	}
	__pmNotifyErr(LOG_INFO, "%s: saw new context %d (num_ctx=%d)\n",
		      __FUNCTION__, ctx, num_ctx);
    }
    return 0;
}

void
ctx_end(int ctx)
{
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL1) {
	fprintf(stderr, "sample_ctx_end(%d) [context is ", ctx);
	if (ctx < 0 || ctx >= num_ctx_allocated)
	    fprintf(stderr, "unknown, num_ctx=%d", num_ctx);
	else if (ctxtab[ctx].state == CTX_ACTIVE)
	    fprintf(stderr, "active");
	else if (ctxtab[ctx].state == CTX_INACTIVE)
	    fprintf(stderr, "inactive");
	else
	    fprintf(stderr, "botched state, %d", ctxtab[ctx].state);
	fprintf(stderr, "]\n");
    }
#endif
    if (ctx < 0 || ctx >= num_ctx_allocated || ctxtab[ctx].state == CTX_INACTIVE) {
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
	|| ctxtab[last_ctx].state == CTX_INACTIVE)
	return NULL;

    return ctxtab[last_ctx].user_data;
}

