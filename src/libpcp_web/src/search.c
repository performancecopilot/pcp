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
#include <sqlite3.h>
#include "pmapi.h"
#include "libpcp.h"
#include "search.h"

/* Type values stored in the SQLite index — must match pmSearchTextType */
#define SEARCH_DOC_METRIC	PM_SEARCH_TYPE_METRIC
#define SEARCH_DOC_INDOM	PM_SEARCH_TYPE_INDOM
#define SEARCH_DOC_INST		PM_SEARCH_TYPE_INST

static int		search_enabled;
static unsigned int	default_resultcount = 10;

typedef struct searchModuleData {
    sqlite3		*db;
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
	match = sdscatfmt(sdsempty(), "{%S} : (%S)", cols, request->query);
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

/* Grow the hits array, doubling capacity (starting at 64). Returns 0 or -ENOMEM. */
static int
search_hits_grow(pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    int			newmax = *maxhits ? *maxhits * 2 : 64;
    pmSearchTextResult	*tmp;

    if (newmax < *maxhits)
	return -ENOMEM;
    tmp = realloc(*hits, (size_t)newmax * sizeof(pmSearchTextResult));
    if (tmp == NULL)
	return -ENOMEM;
    *hits = tmp;
    *maxhits = newmax;
    return 0;
}

static void
search_hits_free(pmSearchTextResult *hits, int nhits)
{
    int		i;

    for (i = 0; i < nhits; i++) {
	sdsfree(hits[i].docid);
	sdsfree(hits[i].name);
	sdsfree(hits[i].indom);
	sdsfree(hits[i].oneline);
	sdsfree(hits[i].helptext);
    }
    free(hits);
}

/* Count matching docs for a text query; returns 0 on failure. */
static int
search_count_table(sqlite3 *db, sds match_expr, sds type_filter)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc, total = 0;

    sql = sdscatfmt(sdsempty(),
	"SELECT count(*) FROM docs WHERE docs MATCH ?%S",
	type_filter);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_count_table: prepare: %s\n",
		    sqlite3_errmsg(db));
	return 0;
    }

    sqlite3_bind_text(stmt, 1, match_expr, sdslen(match_expr), SQLITE_STATIC);

    if (sqlite3_step(stmt) == SQLITE_ROW)
	total = sqlite3_column_int(stmt, 0);

    sqlite3_finalize(stmt);
    return total;
}

/* Execute a text query against docs; appends results to the hits array. */
static int
search_query_table(sqlite3 *db, pmSearchTextRequest *request,
		   sds match_expr, sds type_filter,
		   unsigned int limit, unsigned int offset,
		   pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdscatfmt(sdsempty(),
	"SELECT rowid, name, oneline, helptext, type, indom,"
	" bm25(docs, 9.0, 4.0, 2.0),"
	" highlight(docs, 0, '<b>', '</b>'),"
	" highlight(docs, 1, '<b>', '</b>'),"
	" highlight(docs, 2, '<b>', '</b>')"
	" FROM docs WHERE docs MATCH ?%S"
	" ORDER BY CASE WHEN type = %i"
	" THEN bm25(docs, 9.0, 4.0, 2.0) * 2.0"
	" ELSE bm25(docs, 9.0, 4.0, 2.0) END"
	" LIMIT ? OFFSET ?",
	type_filter, SEARCH_DOC_INDOM);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_query_table: prepare: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }

    sqlite3_bind_text(stmt, 1, match_expr, sdslen(match_expr), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)limit);
    sqlite3_bind_int64(stmt, 3, (sqlite3_int64)offset);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits &&
	    search_hits_grow(hits, nhits, maxhits) < 0) {
	    sqlite3_finalize(stmt);
	    return -ENOMEM;
	}

	memset(&result, 0, sizeof(result));

	result.docid = search_make_docid(
	    sqlite3_column_int64(stmt, 0),
	    (const char *)sqlite3_column_text(stmt, 1));
	if (request->return_type)
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
    if (rc != SQLITE_DONE) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_query_table: step: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }
    return 0;
}

static void
search_do_text_query(searchModuleData *smd, pmSearchTextRequest *request,
		     pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    sds			match, type_filter;
    int			nhits = 0, maxhits = 0, total, sts;
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

    offset = request->offset;
    if (!request->count)
	request->count = smd->resultcount;
    count = request->count;

    match = search_build_match(request);
    type_filter = search_type_filter(request);

    total = search_count_table(smd->db, match, type_filter);

    sts = search_query_table(smd->db, request, match, type_filter,
		       count, offset, &hits, &nhits, &maxhits);

    sdsfree(match);
    sdsfree(type_filter);

    if (sts < 0) {
	search_hits_free(hits, nhits);
	callbacks->on_done(sts, userdata);
	return;
    }

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    for (i = 0; i < (unsigned int)nhits; i++) {
	hits[i].total = total;
	hits[i].count = i + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    search_hits_free(hits, nhits);

    callbacks->on_done(sts, userdata);
}

static int
search_suggest_table(sqlite3 *db, sds match, unsigned int limit,
		     pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdscatfmt(sdsempty(),
	"SELECT rowid, name, type, bm25(docs, 9.0, 4.0, 2.0)"
	" FROM docs WHERE docs MATCH ?"
	" AND type IN (%i, %i)"
	" ORDER BY bm25(docs, 9.0, 4.0, 2.0)"
	" LIMIT ?",
	SEARCH_DOC_METRIC, SEARCH_DOC_INST);

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_suggest_table: prepare: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }

    sqlite3_bind_text(stmt, 1, match, sdslen(match), SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 2, (sqlite3_int64)limit);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits &&
	    search_hits_grow(hits, nhits, maxhits) < 0) {
	    sqlite3_finalize(stmt);
	    return -ENOMEM;
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
    if (rc != SQLITE_DONE) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_suggest_table: step: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }
    return 0;
}

static void
search_do_text_suggest(searchModuleData *smd, pmSearchTextRequest *request,
		       pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    sds			match;
    int			nhits = 0, maxhits = 0, sts;
    unsigned int	count, i;
    double		timer;

    pmtimespecNow(&started);

    {
	sds	query = request->query;
	int	len = sdslen(query);
	int	j, start;

	match = sdsnew("name : (");
	for (j = 0, start = 0; j <= len; j++) {
	    if (j == len || query[j] == '.') {
		if (j > start) {
		    if (start > 0)
			match = sdscat(match, " ");
		    match = sdscatlen(match, query + start, j - start);
		}
		start = j + 1;
	    }
	}
	match = sdscat(match, "*)");
    }

    if (!request->count)
	request->count = smd->resultcount;
    count = request->count;

    sts = search_suggest_table(smd->db, match, count, &hits, &nhits, &maxhits);

    sdsfree(match);

    if (sts < 0) {
	search_hits_free(hits, nhits);
	callbacks->on_done(sts, userdata);
	return;
    }

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    for (i = 0; i < (unsigned int)nhits && i < count; i++) {
	hits[i].total = nhits;
	hits[i].count = i + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    search_hits_free(hits, nhits);

    callbacks->on_done(sts, userdata);
}

static int
search_indom_table(sqlite3 *db, const char *query, int querylen,
		   pmSearchTextResult **hits, int *nhits, int *maxhits)
{
    sqlite3_stmt	*stmt = NULL;
    sds			sql;
    int			rc;

    sql = sdsnew(
	"SELECT d.rowid, d.name, d.oneline, d.helptext, d.type, d.indom"
	" FROM indom_map m JOIN docs d ON d.rowid = m.docid"
	" WHERE m.indom = ?"
	" ORDER BY d.type");

    rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
    sdsfree(sql);
    if (rc != SQLITE_OK) {
	/* fall back for indexes built without indom_map */
	sql = sdsnew(
	    "SELECT rowid, name, oneline, helptext, type, indom"
	    " FROM docs WHERE indom = ?"
	    " ORDER BY type");
	rc = sqlite3_prepare_v2(db, sql, sdslen(sql), &stmt, NULL);
	sdsfree(sql);
	if (rc != SQLITE_OK) {
	    if (pmDebugOptions.search)
		fprintf(stderr, "search_indom_table: prepare: %s\n",
			sqlite3_errmsg(db));
	    return -EIO;
	}
    }

    sqlite3_bind_text(stmt, 1, query, querylen, SQLITE_STATIC);

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
	pmSearchTextResult	result;

	if (*nhits >= *maxhits &&
	    search_hits_grow(hits, nhits, maxhits) < 0) {
	    sqlite3_finalize(stmt);
	    return -ENOMEM;
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
    if (rc != SQLITE_DONE) {
	if (pmDebugOptions.search)
	    fprintf(stderr, "search_indom_table: step: %s\n",
		    sqlite3_errmsg(db));
	return -EIO;
    }
    return 0;
}

static void
search_do_text_indom(searchModuleData *smd, pmSearchTextRequest *request,
		     pmSearchCallBacks *callbacks, void *userdata)
{
    pmSearchTextResult	*hits = NULL;
    struct timespec	started, finished;
    int			nhits = 0, maxhits = 0, sts;
    unsigned int	count, offset, i;
    double		timer;

    pmtimespecNow(&started);

    sts = search_indom_table(smd->db, request->query,
		       sdslen(request->query),
		       &hits, &nhits, &maxhits);

    if (sts < 0) {
	search_hits_free(hits, nhits);
	callbacks->on_done(sts, userdata);
	return;
    }

    pmtimespecNow(&finished);
    timer = pmtimespecSub(&finished, &started);

    offset = request->offset;
    if (!request->count)
	request->count = smd->resultcount;
    count = request->count;

    for (i = offset; i < (unsigned int)nhits && (i - offset) < count; i++) {
	hits[i].total = nhits;
	hits[i].count = (i - offset) + 1;
	hits[i].timer = timer;
	callbacks->on_text_result(&hits[i], userdata);
    }

    search_hits_free(hits, nhits);

    callbacks->on_done(sts, userdata);
}

/* --- public API --- */

int
pmSearchInfo(pmSearchSettings *settings, sds key, void *arg)
{
    searchModuleData	*smd = (searchModuleData *)settings->module.privdata;
    pmSearchMetrics	metrics;
    sqlite3_stmt	*stmt = NULL;
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

    rc = sqlite3_prepare_v2(smd->db,
	"SELECT COUNT(*), COALESCE(SUM(cnt),0)"
	" FROM docs_vocab", -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
	if (sqlite3_step(stmt) == SQLITE_ROW) {
	    metrics.terms = sqlite3_column_int64(stmt, 0);
	    metrics.records = sqlite3_column_int64(stmt, 1);
	}
	sqlite3_finalize(stmt);
    } else if (pmDebugOptions.search) {
	fprintf(stderr, "pmSearchInfo: docs_vocab: %s\n",
		sqlite3_errmsg(smd->db));
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
    if (request == NULL || request->query == NULL) {
	settings->callbacks.on_done(-EINVAL, arg);
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
    if (request == NULL || request->query == NULL) {
	settings->callbacks.on_done(-EINVAL, arg);
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
    if (request == NULL || request->query == NULL) {
	settings->callbacks.on_done(-EINVAL, arg);
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
	if (option) {
	    char		*endp;
	    unsigned long	value;

	    errno = 0;
	    value = strtoul(option, &endp, 10);
	    if (errno == 0 && *endp == '\0' && value > 0 && value <= UINT_MAX)
		smd->resultcount = (unsigned int)value;
	    else
		pmNotifyErr(LOG_WARNING, "ignoring invalid "
			"pmsearch result.count \"%s\"", option);
	}
    }

    option = smd->config ?
	     pmIniFileLookup(smd->config, "pmsearch", "index.path") : NULL;

    if (option) {
	rc = sqlite3_open_v2(option, &smd->db,
			     SQLITE_OPEN_READONLY, NULL);
	if (rc != SQLITE_OK) {
	    pmNotifyErr(LOG_WARNING,
		    "cannot open pmsearch index %s: %s",
		    option, sqlite3_errmsg(smd->db));
	    sqlite3_close(smd->db);
	    smd->db = NULL;
	}
    } else {
	pmsprintf(nightly, sizeof(nightly), "%s/lib/pmsearch.index",
		  pmGetConfig("PCP_VAR_DIR"));
	pmsprintf(base, sizeof(base), "%s/lib/pmsearch.index",
		  pmGetConfig("PCP_SHARE_DIR"));

	rc = sqlite3_open_v2(nightly, &smd->db,
			     SQLITE_OPEN_READONLY, NULL);
	if (rc == SQLITE_OK) {
	    if (pmDebugOptions.search)
		fprintf(stderr, "pmSearchSetup: loaded nightly index %s\n",
			nightly);
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
		pmNotifyErr(LOG_WARNING,
			"no pmsearch index found "
			"(tried %s and %s); run pmsearch_daily(1)",
			nightly, base);
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

void
keysSearchInit(struct dict *config)
{
    sds		option;
    char	*endp;
    unsigned long value;

    if (config) {
	if ((option = pmIniFileLookup(config, "pmsearch", "result.count"))) {
	    errno = 0;
	    value = strtoul(option, &endp, 10);
	    if (errno == 0 && *endp == '\0' && value > 0 && value <= UINT_MAX)
		default_resultcount = (unsigned int)value;
	    else
		pmNotifyErr(LOG_WARNING, "ignoring invalid "
			"pmsearch result.count \"%s\"", option);
	}
    }
}

void
keysSearchClose(void)
{
    default_resultcount = 10;
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
