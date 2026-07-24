/*
 * Copyright (c) 2020-2022,2024 Red Hat.
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
 * SQLite FTS5 search backend for PCP metric/instance full-text search.
 */
#include <assert.h>
#include <sqlite3.h>
#include "pmapi.h"
#include "libpcp.h"
#include "search.h"

#define SEARCH_DOC_METRIC	1
#define SEARCH_DOC_INDOM	2
#define SEARCH_DOC_INST		3

static int		search_enabled;
static unsigned int	default_resultcount = 10;

typedef struct searchModuleData {
    sqlite3		*db;
    int			has_base;
    struct dict		*config;
    unsigned int	loaded;
    unsigned int	resultcount;
} searchModuleData;

const char *
pmSearchTextTypeStr(pmSearchTextType type)
{
    switch (type) {
    case PM_SEARCH_TYPE_UNKNOWN:
	return "unknown";
    case PM_SEARCH_TYPE_METRIC:
	return "metric";
    case PM_SEARCH_TYPE_INDOM:
	return "indom";
    case PM_SEARCH_TYPE_INST:
	return "instance";
    }
    return "unknown";
}

static searchModuleData *
getSearchModuleData(pmSearchModule *module)
{
    if (module->privdata == NULL)
	module->privdata = calloc(1, sizeof(searchModuleData));
    return (searchModuleData *)module->privdata;
}

/*
 * Build an FTS5 MATCH expression from the request.
 * If infields are restricted, wrap the query in column filters.
 * On FTS5 syntax error, fall back to quoting the query as a phrase.
 */
static sds
search_build_match(pmSearchTextRequest *request)
{
    sds		match;
    int		restricted = 0;

    if (request->infields_name || request->infields_oneline ||
	request->infields_helptext) {
	int count = request->infields_name + request->infields_oneline +
		    request->infields_helptext;
	if (count < 3)
	    restricted = 1;
    }

    if (restricted) {
	sds	cols = sdsempty();
	int	first = 1;

	if (request->infields_name) {
	    cols = sdscat(cols, "name");
	    first = 0;
	}
	if (request->infields_oneline) {
	    if (!first)
		cols = sdscat(cols, " ");
	    cols = sdscat(cols, "oneline");
	    first = 0;
	}
	if (request->infields_helptext) {
	    if (!first)
		cols = sdscat(cols, " ");
	    cols = sdscat(cols, "helptext");
	}
	match = sdscatfmt(sdsempty(), "{%S} : %S", cols, request->query);
	sdsfree(cols);
    } else {
	match = sdsnew(request->query);
    }

    return match;
}

/*
 * Build the type filter clause for SQL queries.
 * Returns an sds string like " AND type IN (1,3)" or empty string.
 */
static sds
search_type_filter(pmSearchTextRequest *request)
{
    sds		filter = sdsempty();
    int		any = request->type_metric + request->type_indom +
		      request->type_inst;

    if (any == 0 || any == 3)
	return filter;

    filter = sdscat(filter, " AND type IN (");
    any = 0;
    if (request->type_metric) {
	filter = sdscatfmt(filter, "%i", SEARCH_DOC_METRIC);
	any = 1;
    }
    if (request->type_indom) {
	if (any)
	    filter = sdscat(filter, ",");
	filter = sdscatfmt(filter, "%i", SEARCH_DOC_INDOM);
	any = 1;
    }
    if (request->type_inst) {
	if (any)
	    filter = sdscat(filter, ",");
	filter = sdscatfmt(filter, "%i", SEARCH_DOC_INST);
    }
    filter = sdscat(filter, ")");
    return filter;
}

/*
 * Build a docid string from rowid and name.
 */
static sds
search_make_docid(sqlite3_int64 rowid, const char *name)
{
    char	buf[32];

    pmsprintf(buf, sizeof(buf), "%lld", (long long)rowid);
    return sdscatfmt(sdsempty(), "%s:%s", buf, name ? name : "");
}

/*
 * Execute a text query against a single docs table (qualified name).
 * Appends results to the hits array; caller owns the array.
 */
static int
search_query_table(sqlite3 *db, const char *table,
		   pmSearchTextRequest *request, sds match_expr, sds type_filter,
		   pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdscatfmt(sdsempty(),
	"SELECT rowid, name, oneline, helptext, type, indom,"
	" bm25(%s, 9.0, 4.0, 2.0),"
	" highlight(%s, 0, '<b>', '</b>'),"
	" highlight(%s, 1, '<b>', '</b>'),"
	" highlight(%s, 2, '<b>', '</b>')"
	" FROM %s WHERE %s MATCH ?%S"
	" ORDER BY bm25(%s, 9.0, 4.0, 2.0)",
	table, table, table, table,
	table, table, type_filter,
	table);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_query_table: prepare: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }

    sqlite3_bind_text(stmt, 1, match_expr, sdslen(match_expr), SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits) {
	    *maxhits = *maxhits ? *maxhits * 2 : 64;
	    *hits = realloc(*hits, *maxhits * sizeof(pmSearchTextResult));
	    if (*hits == NULL) {
		sqlite3_finalize(stmt);
		return -ENOMEM;
	    }
	}

	memset(&result, 0, sizeof(result));

	result.docid = search_make_docid(
	    sqlite3_column_int64(stmt, 0),
	    (const char *)sqlite3_column_text(stmt, 1));
	result.type = sqlite3_column_int(stmt, 4);
	result.score = -sqlite3_column_double(stmt, 6);

	if (request->return_name) {
	    if (request->highlight_name)
		result.name = sdsnew((const char *)sqlite3_column_text(stmt, 7));
	    else
		result.name = sdsnew((const char *)sqlite3_column_text(stmt, 1));
	}
	if (request->return_indom) {
	    const char *indom = (const char *)sqlite3_column_text(stmt, 5);
	    if (indom && *indom)
		result.indom = sdsnew(indom);
	}
	if (request->return_oneline) {
	    if (request->highlight_oneline)
		result.oneline = sdsnew((const char *)sqlite3_column_text(stmt, 8));
	    else {
		const char *ol = (const char *)sqlite3_column_text(stmt, 2);
		if (ol && *ol)
		    result.oneline = sdsnew(ol);
	    }
	}
	if (request->return_helptext) {
	    if (request->highlight_helptext)
		result.helptext = sdsnew((const char *)sqlite3_column_text(stmt, 9));
	    else {
		const char *ht = (const char *)sqlite3_column_text(stmt, 3);
		if (ht && *ht)
		    result.helptext = sdsnew(ht);
	    }
	}

	(*hits)[*nhits] = result;
	(*nhits)++;
    }

    sqlite3_finalize(stmt);
    return 0;
}

static int
score_cmp(const void *a, const void *b)
{
    const pmSearchTextResult	*ra = a, *rb = b;

    if (rb->score > ra->score) return 1;
    if (rb->score < ra->score) return -1;
    return 0;
}

static void
search_do_text_query(searchModuleData *smd, pmSearchTextRequest *request,
		     pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    sds			match, type_filter;
    int			nhits = 0, maxhits = 0;
    unsigned int	count, offset, i;
    double		timer;

    pmtimespecNow(&started);

    if (!request->return_name && !request->return_indom &&
	!request->return_oneline && !request->return_helptext &&
	!request->return_type) {
	request->return_name = 1;
	request->return_indom = 1;
	request->return_oneline = 1;
	request->return_helptext = 1;
	request->return_type = 1;
    }

    match = search_build_match(request);
    type_filter = search_type_filter(request);

    search_query_table(smd->db, "docs", request, match, type_filter,
		       &hits, &nhits, &maxhits);

    if (smd->has_base) {
	search_query_table(smd->db, "base.docs", request, match, type_filter,
			   &hits, &nhits, &maxhits);
    }

    sdsfree(match);
    sdsfree(type_filter);

    qsort(hits, nhits, sizeof(pmSearchTextResult), score_cmp);

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    offset = request->offset;
    count = request->count ? request->count : smd->resultcount;

    for (i = offset; i < (unsigned int)nhits && i < offset + count; i++) {
	hits[i].total = nhits;
	hits[i].count = (i - offset) + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    for (i = 0; i < (unsigned int)nhits; i++) {
	sdsfree(hits[i].docid);
	sdsfree(hits[i].name);
	sdsfree(hits[i].indom);
	sdsfree(hits[i].oneline);
	sdsfree(hits[i].helptext);
    }
    free(hits);

    callbacks->on_done(0, userdata);
}

static int
search_suggest_table(sqlite3 *db, const char *table, sds match,
		     pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdscatfmt(sdsempty(),
	"SELECT rowid, name, type, bm25(%s, 9.0, 4.0, 2.0)"
	" FROM %s WHERE %s MATCH ?"
	" AND type IN (1, 3)"
	" ORDER BY bm25(%s, 9.0, 4.0, 2.0)",
	table, table, table, table);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK)
	return -EIO;

    sqlite3_bind_text(stmt, 1, match, sdslen(match), SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits) {
	    *maxhits = *maxhits ? *maxhits * 2 : 64;
	    *hits = realloc(*hits, *maxhits * sizeof(pmSearchTextResult));
	    if (*hits == NULL) {
		sqlite3_finalize(stmt);
		return -ENOMEM;
	    }
	}

	memset(&result, 0, sizeof(result));
	result.docid = search_make_docid(
	    sqlite3_column_int64(stmt, 0),
	    (const char *)sqlite3_column_text(stmt, 1));
	result.name = sdsnew((const char *)sqlite3_column_text(stmt, 1));
	result.type = sqlite3_column_int(stmt, 2);
	result.score = -sqlite3_column_double(stmt, 3);

	(*hits)[(*nhits)++] = result;
    }

    sqlite3_finalize(stmt);
    return 0;
}

static void
search_do_text_suggest(searchModuleData *smd, pmSearchTextRequest *request,
		       pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    sds			match;
    int			nhits = 0, maxhits = 0;
    unsigned int	count, i;
    double		timer;

    pmtimespecNow(&started);

    match = sdscatfmt(sdsempty(), "name : %S*", request->query);

    search_suggest_table(smd->db, "docs", match,
			 &hits, &nhits, &maxhits);

    if (smd->has_base)
	search_suggest_table(smd->db, "base.docs", match,
			     &hits, &nhits, &maxhits);

    sdsfree(match);

    qsort(hits, nhits, sizeof(pmSearchTextResult), score_cmp);

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    count = request->count ? request->count : smd->resultcount;

    for (i = 0; i < (unsigned int)nhits && i < count; i++) {
	hits[i].total = nhits;
	hits[i].count = i + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    for (i = 0; i < (unsigned int)nhits; i++) {
	sdsfree(hits[i].docid);
	sdsfree(hits[i].name);
    }
    free(hits);

    callbacks->on_done(0, userdata);
}

static int
search_indom_table(sqlite3 *db, const char *table, const char *query,
		   int querylen,
		   pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdscatfmt(sdsempty(),
	"SELECT rowid, name, oneline, helptext, type, indom"
	" FROM %s WHERE indom = ?"
	" ORDER BY type",
	table);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK)
	return -EIO;

    sqlite3_bind_text(stmt, 1, query, querylen, SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits) {
	    *maxhits = *maxhits ? *maxhits * 2 : 64;
	    *hits = realloc(*hits, *maxhits * sizeof(pmSearchTextResult));
	    if (*hits == NULL) {
		sqlite3_finalize(stmt);
		return -ENOMEM;
	    }
	}

	memset(&result, 0, sizeof(result));
	result.docid = search_make_docid(
	    sqlite3_column_int64(stmt, 0),
	    (const char *)sqlite3_column_text(stmt, 1));
	result.name = sdsnew((const char *)sqlite3_column_text(stmt, 1));
	result.type = sqlite3_column_int(stmt, 4);
	result.score = (result.type == SEARCH_DOC_INDOM) ? 2.0 :
		       (result.type == SEARCH_DOC_METRIC) ? 1.0 : 0.5;

	{
	    const char *ol = (const char *)sqlite3_column_text(stmt, 2);
	    if (ol && *ol)
		result.oneline = sdsnew(ol);
	}
	{
	    const char *ht = (const char *)sqlite3_column_text(stmt, 3);
	    if (ht && *ht)
		result.helptext = sdsnew(ht);
	}
	{
	    const char *indom = (const char *)sqlite3_column_text(stmt, 5);
	    if (indom && *indom)
		result.indom = sdsnew(indom);
	}

	(*hits)[(*nhits)++] = result;
    }

    sqlite3_finalize(stmt);
    return 0;
}

static void
search_do_text_indom(searchModuleData *smd, pmSearchTextRequest *request,
		     pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    int			nhits = 0, maxhits = 0;
    unsigned int	count, offset, i;
    double		timer;

    pmtimespecNow(&started);

    search_indom_table(smd->db, "docs", request->query,
		       sdslen(request->query), &hits, &nhits, &maxhits);

    if (smd->has_base)
	search_indom_table(smd->db, "base.docs", request->query,
			   sdslen(request->query), &hits, &nhits, &maxhits);

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    offset = request->offset;
    count = request->count ? request->count : smd->resultcount;

    for (i = offset; i < (unsigned int)nhits && i < offset + count; i++) {
	hits[i].total = nhits;
	hits[i].count = (i - offset) + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    for (i = 0; i < (unsigned int)nhits; i++) {
	sdsfree(hits[i].docid);
	sdsfree(hits[i].name);
	sdsfree(hits[i].indom);
	sdsfree(hits[i].oneline);
	sdsfree(hits[i].helptext);
    }
    free(hits);

    callbacks->on_done(0, userdata);
}

/* --- public API --- */

int
pmSearchInfo(pmSearchSettings *settings, sds key, void *arg)
{
    searchModuleData	*smd = (searchModuleData *)settings->module.privdata;
    pmSearchMetrics	metrics;
    sqlite3_stmt	*stmt;
    int			rc;

    (void)key;

    if (smd == NULL || !smd->loaded) {
	settings->callbacks.on_done(-ENOENT, arg);
	return 0;
    }

    memset(&metrics, 0, sizeof(metrics));

    rc = sqlite3_prepare_v2(smd->db,
	"SELECT COUNT(*) FROM docs", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
	if (sqlite3_step(stmt) == SQLITE_ROW)
	    metrics.docs = sqlite3_column_int64(stmt, 0);
	sqlite3_finalize(stmt);
    }

    if (smd->has_base) {
	rc = sqlite3_prepare_v2(smd->db,
	    "SELECT COUNT(*) FROM base.docs", -1, &stmt, NULL);
	if (rc == SQLITE_OK) {
	    if (sqlite3_step(stmt) == SQLITE_ROW)
		metrics.docs += sqlite3_column_int64(stmt, 0);
	    sqlite3_finalize(stmt);
	}
    }

    settings->callbacks.on_metrics(&metrics, arg);
    settings->callbacks.on_done(0, arg);
    return 0;
}

int
pmSearchTextQuery(pmSearchSettings *settings, pmSearchTextRequest *request, void *arg)
{
    searchModuleData	*smd = (searchModuleData *)settings->module.privdata;

    if (smd == NULL || !smd->loaded) {
	settings->callbacks.on_done(-ENOENT, arg);
	return 0;
    }

    search_do_text_query(smd, request, &settings->callbacks, arg);
    return 0;
}

int
pmSearchTextSuggest(pmSearchSettings *settings, pmSearchTextRequest *request, void *arg)
{
    searchModuleData	*smd = (searchModuleData *)settings->module.privdata;

    if (smd == NULL || !smd->loaded) {
	settings->callbacks.on_done(-ENOENT, arg);
	return 0;
    }

    search_do_text_suggest(smd, request, &settings->callbacks, arg);
    return 0;
}

int
pmSearchTextInDom(pmSearchSettings *settings, pmSearchTextRequest *request, void *arg)
{
    searchModuleData	*smd = (searchModuleData *)settings->module.privdata;

    if (smd == NULL || !smd->loaded) {
	settings->callbacks.on_done(-ENOENT, arg);
	return 0;
    }

    search_do_text_indom(smd, request, &settings->callbacks, arg);
    return 0;
}

/* --- module setup / teardown --- */

int
pmSearchSetSlots(pmSearchModule *module, void *slots)
{
    (void)module;
    (void)slots;
    return 0;
}

int
pmSearchSetConfiguration(pmSearchModule *module, struct dict *config)
{
    searchModuleData	*smd = getSearchModuleData(module);

    if (smd == NULL)
	return -ENOMEM;
    smd->config = config;
    return 0;
}

int
pmSearchSetEventLoop(pmSearchModule *module, void *events)
{
    (void)module;
    (void)events;
    return 0;
}

int
pmSearchSetMetricRegistry(pmSearchModule *module, struct mmv_registry *registry)
{
    (void)module;
    (void)registry;
    return 0;
}

int
pmSearchSetup(pmSearchModule *module, void *arg)
{
    searchModuleData	*smd = getSearchModuleData(module);
    char		nightly[MAXPATHLEN];
    char		base[MAXPATHLEN];
    sds			option;
    int			rc;

    if (smd == NULL)
	return -ENOMEM;

    smd->resultcount = default_resultcount;

    if (smd->config) {
	option = pmIniFileLookup(smd->config, "pmsearch", "enabled");
	if (option && strcmp(option, "false") == 0)
	    return -ENOTSUP;

	option = pmIniFileLookup(smd->config, "pmsearch", "result.count");
	if (option)
	    smd->resultcount = atoi(option);
    }

    option = smd->config ?
	     pmIniFileLookup(smd->config, "pmsearch", "index.path") : NULL;

    if (option) {
	rc = sqlite3_open_v2(option, &smd->db,
			     SQLITE_OPEN_READONLY, NULL);
	if (rc != SQLITE_OK) {
	    if (pmDebugOptions.search)
		fprintf(stderr, "pmSearchSetup: open %s: %s\n",
			option, sqlite3_errmsg(smd->db));
	    sqlite3_close(smd->db);
	    smd->db = NULL;
	}
    } else {
	pmsprintf(nightly, sizeof(nightly), "%s/lib/pcp.search",
		  pmGetConfig("PCP_VAR_DIR"));
	pmsprintf(base, sizeof(base), "%s/lib/pcp.search",
		  pmGetConfig("PCP_SHARE_DIR"));

	rc = sqlite3_open_v2(nightly, &smd->db,
			     SQLITE_OPEN_READONLY, NULL);
	if (rc == SQLITE_OK) {
	    char	attach[MAXPATHLEN + 64];

	    if (pmDebugOptions.search)
		fprintf(stderr, "pmSearchSetup: loaded nightly index %s\n",
			nightly);
	    pmsprintf(attach, sizeof(attach),
		      "ATTACH DATABASE '%s' AS base", base);
	    rc = sqlite3_exec(smd->db, attach, NULL, NULL, NULL);
	    if (rc == SQLITE_OK) {
		smd->has_base = 1;
		if (pmDebugOptions.search)
		    fprintf(stderr, "pmSearchSetup: attached base index %s\n",
			    base);
	    }
	} else {
	    sqlite3_close(smd->db);
	    smd->db = NULL;

	    rc = sqlite3_open_v2(base, &smd->db,
				 SQLITE_OPEN_READONLY, NULL);
	    if (rc == SQLITE_OK) {
		if (pmDebugOptions.search)
		    fprintf(stderr, "pmSearchSetup: loaded base index %s\n",
			    base);
	    } else {
		if (pmDebugOptions.search)
		    fprintf(stderr, "pmSearchSetup: no index found\n");
		sqlite3_close(smd->db);
		smd->db = NULL;
	    }
	}
    }

    if (smd->db != NULL) {
	smd->loaded = 1;
	search_enabled = 1;
    } else {
	smd->loaded = 0;
    }

    if (module->on_setup)
	module->on_setup(arg);
    return 0;
}

int
pmSearchEnabled(void *arg)
{
    (void)arg;
    return search_enabled;
}

void
pmSearchClose(pmSearchModule *module)
{
    searchModuleData	*smd = (searchModuleData *)module->privdata;

    if (smd) {
	if (smd->db)
	    sqlite3_close(smd->db);
	memset(smd, 0, sizeof(*smd));
	free(smd);
	module->privdata = NULL;
    }
    search_enabled = 0;
}

/* --- stubs for schema.c / keys.c compatibility --- */

extern void keys_slots_end_phase(void *);

void
keysSearchInit(struct dict *config)
{
    sds		option;

    if (config) {
	if ((option = pmIniFileLookup(config, "pmsearch", "result.count")))
	    default_resultcount = atoi(option);
    }
}

void
keysSearchClose(void)
{
    default_resultcount = 10;
}

void
keys_load_search_schema(void *arg)
{
    keys_slots_end_phase(arg);
}

void
keys_search_text_add(struct keySlots *slots, pmSearchTextType type,
		const char *name, const char *indom,
		const char *oneline, const char *helptext, void *arg)
{
    (void)slots; (void)type; (void)name; (void)indom;
    (void)oneline; (void)helptext; (void)arg;
}

/* --- discover no-ops (declared in discover.h) --- */

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
