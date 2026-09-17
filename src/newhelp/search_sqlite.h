/*
 * Copyright (c) 2026 Red Hat.
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
#ifndef SEARCH_SQLITE_H
#define SEARCH_SQLITE_H

#define SEARCH_DOC_METRIC	1
#define SEARCH_DOC_INDOM	2
#define SEARCH_DOC_INST		3

extern int  search_sqlite_open(const char *path);
extern void search_sqlite_add(const char *name, const char *oneline,
			      const char *helptext, const char *indom, int type);
extern int  search_sqlite_close(void);

#endif /* SEARCH_SQLITE_H */
