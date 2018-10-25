/*
 * Copyright (c) 2017-2018 Red Hat.
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include <assert.h>
#include "pmapi.h"
#include "private.h"
#include "batons.h"

static const char *
magic_str(seriesBatonMagic *baton)
{
    switch (baton->magic) {
    case MAGIC_SLOTS:    return "slots";
    case MAGIC_MAPPING:  return "mapping";
    case MAGIC_CONTEXT:  return "context";
    case MAGIC_LOAD:     return "load";
    case MAGIC_STREAM:   return "stream";
    case MAGIC_QUERY:    return "query";
    case MAGIC_SID:      return "sid";
    case MAGIC_NAMES:    return "names";
    case MAGIC_LABELMAP: return "labelmap";
    default:             break;
    }
    return "???";
}

void
initSeriesBatonMagic(void *arg, series_baton_magic magic)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    memset(baton, 0, sizeof(*baton));
    baton->magic = magic;
}

void
seriesBatonCheckMagic(void *arg, series_baton_magic magic, const char *caller)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    if (UNLIKELY(baton->traced || pmDebugOptions.desperate || baton->magic != magic)) {
	fprintf(stderr, "Baton [%s/%p] %s magic - %u (@ %s)\n",
		magic_str(baton), baton, 
		(baton->magic == magic) ? "verified" : "BAD",
		baton->magic, caller);
    }
    assert(baton->magic == magic);
}

void
seriesBatonCheckCount(void *arg, const char *caller)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    if (UNLIKELY(baton->traced || pmDebugOptions.desperate || baton->refcount)) {
	fprintf(stderr, "Baton [%s/%p] %u references - %s (@ %s)\n",
		magic_str(baton), baton, baton->refcount,
		(baton->refcount == 0) ? "verified" : "BAD", caller);
    }
    assert(baton->refcount == 0);
}

void
seriesBatonSetTraced(void *arg, int traced)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    assert(baton->magic < MAGIC_COUNT);
    baton->traced = traced;
}

void
seriesBatonReferences(void *arg, unsigned int refcount, const char *caller)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    if (UNLIKELY(baton->traced || pmDebugOptions.series)) {
	fprintf(stderr,
		"Baton [%s/%p] references: %u -> %u (@ %s)\n",
		magic_str(baton), baton, baton->refcount,
		baton->refcount + refcount, caller);
    }
    assert(baton->magic < MAGIC_COUNT);
    baton->refcount += refcount;
}

void
seriesBatonReference(void *arg, const char *caller)
{
    seriesBatonReferences(arg, 1, caller);
}

/* returns non-zero if this was the final reference being dropped */
int
seriesBatonDereference(void *arg, const char *caller)
{
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    if (UNLIKELY(baton->traced || pmDebugOptions.series)) {
	fprintf(stderr,
		"Baton [%s/%p] references: %u -> %u (@ %s)\n",
		magic_str(baton), baton, baton->refcount, baton->refcount - 1,
		caller);
    }
    assert(baton->magic < MAGIC_COUNT);
    assert(baton->refcount > 0);

    if (baton->refcount)
	if (--baton->refcount == 0)
	    return 1;
    return 0;
}

void
seriesPassBaton(seriesBatonPhase **head, void *arg, const char *caller)
{
    seriesBatonPhase	*next;
    seriesBatonMagic	*baton = (seriesBatonMagic *)arg;

    if (UNLIKELY(baton->traced || pmDebugOptions.series)) {
	fprintf(stderr,
		"Baton [%s/%p] references: %u -> %u (@ %s[%s])\n",
		magic_str(baton), baton, baton->refcount, baton->refcount - 1,
		caller, "seriesPassBaton");
    }
    assert(baton->refcount);

    if (--baton->refcount > 0) {
	/* phase still in-progress so no more to do */
    } else if ((next = (*head)->next) != NULL) {
	*head = next;	/* move onto the next phase */
	next->func(arg);
    } else {
	*head = NULL;	/* all phases are completed */
    }
}

void
seriesBatonPhases(seriesBatonPhase *phases, unsigned int count, void *arg)
{
    seriesBatonPhase	*tmp;
    int			i;

    assert(count > 0);
    for (i = 0; i < count - 1; i++) {
	tmp = &phases[i];
	tmp->next = &phases[i+1];
    }
    phases[i].next = NULL;
    phases[0].func(arg);	/* start phase one! */
}
