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
 *
 * Build a SQLite FTS5 search index from help text entries.
 */
#include <stdio.h>
#include <sqlite3.h>
#include "search_sqlite.h"

static sqlite3		*search_db;
static sqlite3_stmt	*search_insert;

int
search_sqlite_open(const char *path)
{
    int		rc;

    rc = sqlite3_open_v2(path, &search_db,
			 SQLITE_OPEN_CREATE | SQLITE_OPEN_READWRITE, NULL);
    if (rc != SQLITE_OK) {
	fprintf(stderr, "search_sqlite_open: %s: %s\n",
		path, sqlite3_errmsg(search_db));
	sqlite3_close(search_db);
	search_db = NULL;
	return -1;
    }

    rc = sqlite3_exec(search_db,
	"CREATE VIRTUAL TABLE IF NOT EXISTS docs USING fts5("
	"  name,"
	"  oneline,"
	"  helptext,"
	"  type UNINDEXED,"
	"  indom UNINDEXED,"
	"  tokenize='porter unicode61',"
	"  prefix='2,3'"
	");",
	NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
	fprintf(stderr, "search_sqlite_open: create table: %s\n",
		sqlite3_errmsg(search_db));
	sqlite3_close(search_db);
	search_db = NULL;
	return -1;
    }

    rc = sqlite3_exec(search_db, "BEGIN", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
	fprintf(stderr, "search_sqlite_open: begin: %s\n",
		sqlite3_errmsg(search_db));
	sqlite3_close(search_db);
	search_db = NULL;
	return -1;
    }

    rc = sqlite3_prepare_v2(search_db,
	"INSERT INTO docs(name, oneline, helptext, type, indom)"
	" VALUES(?, ?, ?, ?, ?)",
	-1, &search_insert, NULL);
    if (rc != SQLITE_OK) {
	fprintf(stderr, "search_sqlite_open: prepare: %s\n",
		sqlite3_errmsg(search_db));
	sqlite3_close(search_db);
	search_db = NULL;
	return -1;
    }

    return 0;
}

void
search_sqlite_add(const char *name, const char *oneline,
		  const char *helptext, const char *indom, int type)
{
    if (search_db == NULL || search_insert == NULL)
	return;

    sqlite3_bind_text(search_insert, 1, name, -1, SQLITE_STATIC);
    sqlite3_bind_text(search_insert, 2, oneline ? oneline : "", -1, SQLITE_STATIC);
    sqlite3_bind_text(search_insert, 3, helptext ? helptext : "", -1, SQLITE_STATIC);
    sqlite3_bind_int(search_insert, 4, type);
    sqlite3_bind_text(search_insert, 5, indom ? indom : "", -1, SQLITE_STATIC);

    if (sqlite3_step(search_insert) != SQLITE_DONE) {
	fprintf(stderr, "search_sqlite_add: %s: %s\n",
		name, sqlite3_errmsg(search_db));
    }

    sqlite3_reset(search_insert);
}

int
search_sqlite_close(void)
{
    int		rc;

    if (search_db == NULL)
	return -1;

    if (search_insert) {
	sqlite3_finalize(search_insert);
	search_insert = NULL;
    }

    /* optimize the FTS index before closing */
    sqlite3_exec(search_db, "INSERT INTO docs(docs) VALUES('optimize')",
		 NULL, NULL, NULL);

    rc = sqlite3_exec(search_db, "COMMIT", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
	fprintf(stderr, "search_sqlite_close: commit: %s\n",
		sqlite3_errmsg(search_db));
	sqlite3_close(search_db);
	search_db = NULL;
	return -1;
    }

    sqlite3_close(search_db);
    search_db = NULL;
    return 0;
}
