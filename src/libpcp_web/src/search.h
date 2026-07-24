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

extern void keysSearchInit(struct dict *);
extern void keysSearchClose(void);

extern void keys_load_search_schema(void *);
extern void keys_search_text_add(struct keySlots *, pmSearchTextType,
		const char *, const char *, const char *, const char *, void *);

#endif	/* SEARCH_SCHEMA_H */
