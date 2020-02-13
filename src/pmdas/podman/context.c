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

#include "podman.h"

enum {
    CTX_INACTIVE = 0, 
    CTX_ACTIVE   = (1<<0),
    CTX_CONTAINER= (1<<1),
};

typedef struct context {
    unsigned int        state;
    struct container	*container;
} context_t;

static context_t *ctxtab;
static int num_ctx;

static void
podman_context_clear(int ctx)
{
    ctxtab[ctx].state = CTX_INACTIVE;
    ctxtab[ctx].container = NULL;
}

void
podman_context_end(int ctx)
{
    if (ctx < 0 || ctx >= num_ctx || ctxtab[ctx].state == CTX_INACTIVE)
        return;
    podman_context_clear(ctx);
}

static void
podman_context_grow_table(int ctx)
{
    size_t	need;

    if (ctx < num_ctx)
        return;

    need = (ctx + 1) * sizeof(ctxtab[0]);
    ctxtab = (context_t *)realloc(ctxtab, need);
    if (ctxtab == NULL)
	pmNoMem("podman context table", need, PM_FATAL_ERR);
    while (num_ctx <= ctx)
	podman_context_clear(num_ctx++);
}

struct container *
podman_context_container(int ctx)
{
    context_t *pp;

    if (ctx < 0 || ctx >= num_ctx)
        return NULL;
    pp = &ctxtab[ctx];
    if (pp->state == CTX_INACTIVE)
        return NULL;
    if (pp->state & CTX_CONTAINER)
        return pp->container;
    return NULL;
}

void
podman_context_set_container(int ctx, pmInDom indom, const char *value, int length)
{
    struct container	*cp = NULL;
    char		name[64+1];
    int			sts;

    if (length < sizeof(name)) {
	pmsprintf(name, sizeof(name), "%s", value);
	sts = pmdaCacheLookupName(indom, name, NULL, (void **)&cp);
	if (sts < 0)
	    cp = NULL;
    }

    podman_context_grow_table(ctx);
    if (cp) {
	ctxtab[ctx].container = cp;
	ctxtab[ctx].state |= CTX_CONTAINER;
    } else {
	ctxtab[ctx].container = NULL;
	ctxtab[ctx].state &= ~CTX_CONTAINER;
    }
    ctxtab[ctx].state |= CTX_ACTIVE;
}
