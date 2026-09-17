/*
 * Copyright (c) 2020,2022 Red Hat.
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
 */
#ifndef SEARCH_SCHEMA_H
#define SEARCH_SCHEMA_H

#include <pmapi.h>
#include <pcp/pmwebapi.h>

struct dict;
struct keySlots;

/*
 * The functions below are retained only for link compatibility with
 * schema.c and keys.c.  They are implemented as no-ops now that the
 * search index is built by newhelp(1) into a local SQLite FTS5 file.
 * Do not add new callers.
 */
extern void keysSearchInit(struct dict *);
extern void keysSearchClose(void);

extern void keys_search_text_add(struct keySlots *, pmSearchTextType,
		const char *, const char *, const char *, const char *, void *);

#endif	/* SEARCH_SCHEMA_H */
