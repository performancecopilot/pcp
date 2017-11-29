/*
 * Some functions become per-client (of pmcd) with the introduction
 * of PMDA_INTERFACE_5.
 *
 * Copyright (c) 2010 Ken McDonell.  All Rights Reserved.
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

#include <pcp/pmapi.h>
#include <pcp/pmda.h>
#include "percontext.h"

typedef struct {
    int		state;		/* active or inactive context */
    int		recv_pdu;	/* count of PDUs received from this context */
    int		xmit_pdu;	/* count of PDUs sent to this context */
} perctx_t;

/* values for state */
#define CTX_INACTIVE	0
#define CTX_ACTIVE	1

static perctx_t	*ctxtab;
static int	num_ctx;
static int	num_start;	/* count of new contexts noted */
static int	num_end;	/* count of end context events */
static int	num_recv_pdu;	/* recv count from closed contexts */
static int	num_xmit_pdu;	/* xmit count from closed contexts */

void
sample_clr_recv(int ctx)
{
    if (ctx == CTX_ALL) {
	int	i;
	for (i = 0; i < num_ctx; i++) {
	    if (ctxtab[i].state == CTX_ACTIVE)
		ctxtab[i].recv_pdu = 0;
	}
	num_recv_pdu = 0;
    }
    else if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	fprintf(stderr, "Botch: sample_clr_recv(%d) num_ctx=%d", ctx, num_ctx);
	if (ctx >= 0 && ctx < num_ctx)
	    fprintf(stderr, " ctxtab[] is inactive");
	fputc('\n', stderr);
    }
    else
	ctxtab[ctx].recv_pdu = 0;
}

void
sample_clr_xmit(int ctx)
{
    if (ctx == CTX_ALL) {
	int	i;
	for (i = 0; i < num_ctx; i++) {
	    if (ctxtab[i].state == CTX_ACTIVE)
		ctxtab[i].xmit_pdu = 0;
	}
	num_xmit_pdu = 0;
    }
    else if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	fprintf(stderr, "Botch: sample_clr_xmit(%d) num_ctx=%d", ctx, num_ctx);
	if (ctx >= 0 && ctx < num_ctx)
	    fprintf(stderr, " ctxtab[] is inactive");
	fputc('\n', stderr);
    }
    else
	ctxtab[ctx].xmit_pdu = 0;
}

int
sample_get_recv(int ctx)
{
    if (ctx == CTX_ALL) {
	int	i;
	int	ans = num_recv_pdu;
	for (i = 0; i < num_ctx; i++) {
	    if (ctxtab[i].state == CTX_ACTIVE)
		ans += ctxtab[i].recv_pdu;
	}
	return ans;
    }
    else if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	return PM_ERR_NOCONTEXT;
    }
    else
	return ctxtab[ctx].recv_pdu;
}

int
sample_get_xmit(int ctx)
{
    if (ctx == CTX_ALL) {
	int	i;
	int	ans = num_xmit_pdu;
	for (i = 0; i < num_ctx; i++) {
	    if (ctxtab[i].state == CTX_ACTIVE)
		ans += ctxtab[i].xmit_pdu;
	}
	return ans;
    }
    else if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	return PM_ERR_NOCONTEXT;
    }
    else
	return ctxtab[ctx].xmit_pdu;
}

static void
growtab(int ctx)
{
    ctxtab = (perctx_t *)realloc(ctxtab, (ctx+1)*sizeof(ctxtab[0]));
    if (ctxtab == NULL) {
	pmNoMem("growtab", (ctx+1)*sizeof(ctxtab[0]), PM_FATAL_ERR);
	/*NOTREACHED*/
    }
    while (num_ctx <= ctx) {
	ctxtab[num_ctx].state = CTX_INACTIVE;
	ctxtab[num_ctx].recv_pdu  = 0;
	ctxtab[num_ctx].xmit_pdu  = 0;
	num_ctx++;
    }
    ctxtab[ctx].state = CTX_INACTIVE;
}

void
sample_inc_recv(int ctx)
{
    if (ctx < 0) {
	fprintf(stderr, "Botch: sample_inc_recv(%d)!\n", ctx);
	return;
    }
    if (ctx >= num_ctx) growtab(ctx);
    if (ctxtab[ctx].state == CTX_INACTIVE) {
	num_start++;
	ctxtab[ctx].state = CTX_ACTIVE;
	ctxtab[ctx].recv_pdu = 0;
	ctxtab[ctx].xmit_pdu = 0;
	if (pmDebugOptions.appl1) {
	    fprintf(stderr, "sample_inc_recv(%d) [new context, num_ctx=%d]\n", ctx, num_ctx);
	}
    }
    ctxtab[ctx].recv_pdu++;
}

void
sample_inc_xmit(int ctx)
{
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	fprintf(stderr, "Botch: sample_inc_xmit(%d) num_ctx=%d", ctx, num_ctx);
	if (ctx >= 0 && ctx < num_ctx)
	    fprintf(stderr, " ctxtab[] is inactive");
	fputc('\n', stderr);
	return;
    }
    if (ctx >= num_ctx)
	growtab(ctx);
    ctxtab[ctx].xmit_pdu++;
}

int
sample_ctx_fetch(int ctx, int item)
{
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	fprintf(stderr, "Botch: sample_ctx_fetch(%d, %d) num_ctx=%d", ctx, item, num_ctx);
	if (ctx >= 0 && ctx < num_ctx)
	    fprintf(stderr, " ctxtab[] is inactive");
	fputc('\n', stderr);
	return PM_ERR_NOCONTEXT;
    }

    if (item == 43) {
	/* percontext.pdu */
	return ctxtab[ctx].recv_pdu + ctxtab[ctx].xmit_pdu;
    }
    else if (item == 44) {
	/* percontext.recv-pdu */
	return ctxtab[ctx].recv_pdu;
    }
    else if (item == 45) {
	/* percontext.xmit-pdu */
	return ctxtab[ctx].xmit_pdu;
    }
    else if (item == 122) {
	/* percontext.control.ctx */
	return num_ctx;
    }
    else if (item == 123) {
	/* percontext.control.active */
	int	i;
	int	ans = 0;
	for (i = 0; i < num_ctx; i++) {
	    if (ctxtab[i].state == CTX_ACTIVE)
		ans++;
	}
	return ans;
    }
    else if (item == 124) {
	/* percontext.control.start */
	return num_start;
    }
    else if (item == 125) {
	/* percontext.control.end */
	return num_end;
    }

    fprintf(stderr, "Botch: sample_ctx_fetch(%d, %d): item bad!\n", ctx, item);
    return PM_ERR_PMID;
}

void
sample_ctx_end(int ctx)
{
    if (pmDebugOptions.appl1) {
	fprintf(stderr, "sample_ctx_end(%d) [context is ", ctx);
	if (ctx < 0 || ctx >= num_ctx)
	    fprintf(stderr, "unknown, num_ctx=%d", num_ctx);
	else if (ctxtab[ctx].state == CTX_ACTIVE)
	    fprintf(stderr, "active");
	else if (ctxtab[ctx].state == CTX_INACTIVE)
	    fprintf(stderr, "inactive");
	else
	    fprintf(stderr, "botched state, %d", ctxtab[ctx].state);
	fprintf(stderr, "]\n");
    }
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE) {
	/*
	 * This is expected ... when a context is closed in pmcd
	 * (or for a local context or for dbpmda or ...) all the
	 * PMDAs with a registered pmdaEndContextCallBack will be
	 * called and some of the PMDAs may not have not serviced
	 * any previous requests for that context.
	 */
	return;
    }
    num_end++;
    num_recv_pdu += ctxtab[ctx].recv_pdu;
    num_xmit_pdu += ctxtab[ctx].xmit_pdu;
    ctxtab[ctx].state = CTX_INACTIVE;
}

