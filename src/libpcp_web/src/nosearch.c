/*
 * Copyright (c) 2026 Red Hat.
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
 *
 * Stub routines for builds without SQLite FTS5 support.
 */
#include "pmwebapi.h"
#include "search.h"
#include "discover.h"

const char *
pmSearchTextTypeStr(pmSearchTextType type)
{
    (void)type;
    return "unknown";
}

int
pmSearchInfo(pmSearchSettings *settings, sds key, void *arg)
{
    (void)settings; (void)key; (void)arg;
    return -EOPNOTSUPP;
}

int
pmSearchTextQuery(pmSearchSettings *settings, pmSearchTextRequest *request,
		void *arg)
{
    (void)settings; (void)request; (void)arg;
    return -EOPNOTSUPP;
}

int
pmSearchTextSuggest(pmSearchSettings *settings, pmSearchTextRequest *request,
		void *arg)
{
    (void)settings; (void)request; (void)arg;
    return -EOPNOTSUPP;
}

int
pmSearchTextInDom(pmSearchSettings *settings, pmSearchTextRequest *request,
		void *arg)
{
    (void)settings; (void)request; (void)arg;
    return -EOPNOTSUPP;
}

int
pmSearchSetSlots(pmSearchModule *module, void *slots)
{
    (void)module; (void)slots;
    return 0;
}

int
pmSearchSetConfiguration(pmSearchModule *module, struct dict *config)
{
    (void)module; (void)config;
    return 0;
}

int
pmSearchSetEventLoop(pmSearchModule *module, void *events)
{
    (void)module; (void)events;
    return 0;
}

int
pmSearchSetMetricRegistry(pmSearchModule *module, struct mmv_registry *registry)
{
    (void)module; (void)registry;
    return 0;
}

int
pmSearchSetup(pmSearchModule *module, void *arg)
{
    (void)module; (void)arg;
    return -EOPNOTSUPP;
}

int
pmSearchEnabled(void *arg)
{
    (void)arg;
    return 0;
}

void
pmSearchClose(pmSearchModule *module)
{
    (void)module;
}

/* --- stubs for schema.c / keys.c compatibility --- */

void
keysSearchInit(struct dict *config)
{
    (void)config;
}

void
keysSearchClose(void)
{
}

void
keys_search_text_add(struct keySlots *slots, pmSearchTextType type,
		const char *name, const char *indom,
		const char *oneline, const char *helptext, void *arg)
{
    (void)slots; (void)type; (void)name; (void)indom;
    (void)oneline; (void)helptext; (void)arg;
}

/* --- discover no-ops --- */

void
pmSearchDiscoverMetric(pmDiscoverEvent *event,
		pmDesc *desc, int numnames, char **names, void *arg)
{
    (void)event; (void)desc; (void)numnames; (void)names; (void)arg;
}

void
pmSearchDiscoverInDom(pmDiscoverEvent *event, pmInResult *in, void *arg)
{
    (void)event; (void)in; (void)arg;
}

void
pmSearchDiscoverText(pmDiscoverEvent *event,
		int ident, int type, char *text, void *arg)
{
    (void)event; (void)ident; (void)type; (void)text; (void)arg;
}
