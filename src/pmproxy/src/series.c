/*
 * Copyright (c) 2019 Red Hat.
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
#include "server.h"
#include <assert.h>

typedef enum pmSeriesRestKey {
    RESTKEY_NONE	= 0,
    RESTKEY_SOURCE,
    RESTKEY_DESC,
    RESTKEY_INSTS,
    RESTKEY_LABELS,
    RESTKEY_METRIC,
    RESTKEY_LOAD,
    RESTKEY_QUERY,
} pmSeriesRestKey;

typedef struct pmSeriesRestCommand {
    const char		*name;
    unsigned int	size;
    pmSeriesRestKey	key;
} pmSeriesRestCommand;

typedef struct pmSeriesBaton {
    struct client	*client;
    pmSeriesRestKey	restkey;
    pmSeriesFlags	flags;
    uv_work_t		loading;
    unsigned int	working;
    int			nsids;
    pmSID		*sids;
    pmSID		sid;
    sds			match;
    sds			suffix;
    unsigned int	series;
    unsigned int	values;
    sds			info;
    sds			query;
} pmSeriesBaton;

static pmSeriesRestCommand commands[] = {
    { .key = RESTKEY_QUERY, .name = "query", .size = sizeof("query")-1 },
    { .key = RESTKEY_DESC,  .name = "descs",  .size = sizeof("descs")-1 },
    { .key = RESTKEY_INSTS, .name = "instances", .size = sizeof("instances")-1 },
    { .key = RESTKEY_LABELS, .name = "labels", .size = sizeof("labels")-1 },
    { .key = RESTKEY_METRIC, .name = "metrics", .size = sizeof("metrics")-1 },
    { .key = RESTKEY_SOURCE, .name = "sources", .size = sizeof("sources")-1 },
    { .key = RESTKEY_LOAD,  .name = "load",  .size = sizeof("load")-1 },
    { .key = RESTKEY_NONE }
};

/* constant string keys (initialized during servlet setup) */
static sds PARAM_EXPR, PARAM_MATCH, PARAM_SERIES, PARAM_SOURCE;

/* constant global strings (read-only) */
static const char pmseries_success[] = "{\"success\":true}\r\n";
static const char pmseries_failure[] = "{\"success\":false}\r\n";

static pmSeriesRestKey
pmseries_lookup_restkey(sds url)
{
    pmSeriesRestCommand	*cp;
    const char		*name;

    if (sdslen(url) >= (sizeof("/series/") - 1) &&
	strncmp(url, "/series/", sizeof("/series/") - 1) == 0) {
	name = (const char *)url + sizeof("/series/") - 1;
	for (cp = &commands[0]; cp->name; cp++) {
	    if (strncmp(cp->name, name, cp->size) == 0)
		return cp->key;
	}
    }
    return RESTKEY_NONE;
}

static void
pmseries_free_baton(struct client *client, pmSeriesBaton *baton)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "pmseries_free_baton %p for client %p\n", baton, client);

    if (baton->sid)
	sdsfree(baton->sid);
    if (baton->info)
	sdsfree(baton->info);
    if (baton->query)
	sdsfree(baton->query);
    if (baton->suffix)
	sdsfree(baton->suffix);
    memset(baton, 0, sizeof(*baton));
}

static int
on_pmseries_match(pmSID sid, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			result = http_get_buffer(baton->client);

    if (baton->sid == NULL || sdscmp(baton->sid, sid) != 0) {
	if (baton->series++ == 0) {
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s\"%S\"", prefix, sid);
	http_set_buffer(client, result, HTTP_FLAG_JSON);
	http_transfer(client);
    }
    return 0;
}

static int
on_pmseries_value(pmSID sid, pmSeriesValue *value, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			timestamp, series, quoted;
    sds			result = http_get_buffer(baton->client);

    timestamp = value->timestamp;
    series = value->series;
    quoted = sdscatrepr(sdsempty(), value->data, sdslen(value->data));

    if (baton->series++ == 0) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	prefix = "[";
    } else {
	prefix = ",";
    }
    result = sdscatfmt(result, "%s{\"series\":\"%S\",", prefix, sid);
    if (sdscmp(sid, series) != 0)	/* an instance of a metric */
	result = sdscatfmt(result, "\"instance\":\"%S\",", series);
    result = sdscatfmt(result, "\"timestamp\":%S,", timestamp);
    result = sdscatfmt(result, "\"value\":%S}", quoted);
    sdsfree(quoted);

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_desc(pmSID sid, pmSeriesDesc *desc, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, result = http_get_buffer(baton->client);

    if ((s = baton->sid) == NULL) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	baton->sid = sdsdup(sid);
	prefix = "[";
    }
    else {
	if (sdscmp(s, sid) != 0) {
	    assert(sdslen(s) == sdslen(sid));
	    baton->sid = sdscpylen(s, sid, sdslen(sid));
	}
	prefix = ",";
    }

    result = sdscatfmt(result, "%s{\"series\":\"%S\",\"source\":\"%S\","
		"\"pmid\":\"%S\",\"indom\":\"%S\","
		"\"semantics\":\"%S\",\"type\":\"%S\",\"units\":\"%S\"}",
		prefix, sid, desc->source, desc->pmid, desc->indom,
		desc->semantics, desc->type, desc->units);
    baton->series++;

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_metric(pmSID sid, sds name, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, result = http_get_buffer(client);

    if (sid == NULL) {	/* request for all metric names globally */
	if (baton->values == 0) {
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s\"%S\"", prefix, name);
    } else {
	if ((s = baton->sid) == NULL) {	/* first series seem */
	    baton->sid = sdsdup(sid);
	    baton->series++;
	    assert(baton->values == 0);
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    if (sdscmp(s, sid) != 0) {
		assert(sdslen(s) == sdslen(sid));
		baton->sid = sdscpylen(s, sid, sdslen(sid));
		baton->series++;
		baton->values = 0;
	    }
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s{\"series\":\"%S\",\"name\":\"%S\"}",
				prefix, sid, name);
    }
    baton->values++;	/* count of names for this series/request */

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_context(pmSID sid, sds name, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, quoted, result = http_get_buffer(client);

    quoted = sdscatrepr(sdsempty(), name, sdslen(name));
    if (sid == NULL) {	/* request for all source names */
	if (baton->values == 0) {
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s%S", prefix, quoted);
    } else {
	if ((s = baton->sid) == NULL) {
	    baton->sid = sdsdup(sid);
	    baton->series++;
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    if (sdscmp(s, sid) != 0) {
		assert(sdslen(s) == sdslen(sid));
		baton->sid = sdscpylen(s, sid, sdslen(sid));
		baton->series++;
		baton->values = 0;
	    }
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s{\"source\":\"%S\",\"name\":%S}",
		    prefix, sid, quoted);
    }
    sdsfree(quoted);
    baton->values++;	/* count of names for this source */

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_label(pmSID sid, sds label, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, result = http_get_buffer(client);

    if (baton->nsids != 0)
	return 0;

    if (sid == NULL) {	/* all labels globally requested */
	if (baton->values == 0) {
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s\"%S\"", prefix, label);
    }
    else if ((s = baton->sid) == NULL) {
	baton->sid = sdsdup(sid);
	baton->series++;
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	result = sdscatfmt(result, "[{\"series\":\"%S\",\"labels\":[\"%S\"",
			sid, label);
    }
    else if (sdscmp(s, sid) != 0) {
	assert(sdslen(s) == sdslen(sid));
	baton->sid = sdscpylen(s, sid, sdslen(sid));
	baton->series++;
	baton->values = 0;
	result = sdscatfmt(result, "]},{\"series\":\"%S\",\"labels\":[\"%S\"}",
			sid, label);
    } else {
	result = sdscatfmt(result, ",\"%S\"", label);
    }
    baton->values++;	/* count of labels for this series */

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_labelmap(pmSID sid, pmSeriesLabel *label, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, name, value, result = http_get_buffer(client);

    if (baton->nsids == 0)
	return 0;

    name = label->name;
    value = label->value;

    if ((s = baton->sid) == NULL) {
	baton->sid = sdsdup(sid);
	baton->series++;
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	result = sdscatfmt(result, "[{\"series\":\"%S\",\"labels\":", sid);
	prefix = "{";
    } else if (sdscmp(s, sid) != 0) {
	assert(sdslen(s) == sdslen(sid));
	baton->series++;
	baton->values = 0;
	baton->sid = sdscpylen(s, sid, sdslen(sid));
	result = sdscatfmt(result, "}},{\"series\":\"%S\",\"labels\":", sid);
	prefix = "{";
    } else {
	prefix = ",";
    }
    result = sdscatfmt(result, "%s\"%S\":%S", prefix, name, value);
    baton->values++;

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_instance(pmSID sid, sds name, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			s, quoted, result = http_get_buffer(client);

    quoted = sdscatrepr(sdsempty(), name, sdslen(name));
    if (sid == NULL) {	/* all instances globally requested */
	if (baton->values == 0) {
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s%S", prefix, quoted);
    }
    else if ((s = baton->sid) == NULL) {	/* first series seen */
	baton->sid = sdsdup(sid);
	baton->series++;
	assert(baton->values == 0);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	result = sdscatfmt(result, "[{\"series\":\"%S\",\"instances\":[%S",
			sid, quoted);
    }
    else if (sdscmp(s, sid) != 0) {		/* different series */
	assert(sdslen(s) == sdslen(sid));
	baton->sid = sdscpylen(s, sid, sdslen(sid));
	baton->series++;
	baton->values = 0;
	result = sdscatfmt(result, "]},{\"series\":\"%S\",\"instances\":[%S",
			sid, name);
    } else {				/* repeating series */
	result = sdscatfmt(result, ",%S", quoted);
    }
    baton->values++;	/* count of instances for this series */
    sdsfree(quoted);

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_pmseries_inst(pmSID sid, pmSeriesInst *inst, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			source, series, instid, quoted;
    sds			result = http_get_buffer(baton->client);

    series = inst->series;
    source = inst->source;
    instid = inst->instid;
    quoted = sdscatrepr(sdsempty(), inst->name, sdslen(inst->name));

    if (baton->values++ == 0) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	prefix = "[";
    } else {
	prefix = ",";
    }
    result = sdscatfmt(result,
		    "%s{\"series\":\"%S\",\"source\":\"%S\","
		    "\"instance\":\"%S\",\"id\":%S,\"name\":%S}",
		    prefix, sid, source, series, instid, quoted);
    sdsfree(quoted);

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static void
on_pmseries_done(int status, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    http_flags		flags = client->u.http.flags;
    http_code		code;
    sds			msg;

    if (status == 0) {
	code = HTTP_STATUS_OK;
	/* complete current response with JSON suffix if needed */
	if ((msg = baton->suffix) == NULL)	/* empty OK response */
	    msg = sdsnewlen(pmseries_success, sizeof(pmseries_success) - 1);
	baton->suffix = NULL;
    } else {
	if (((code = client->u.http.parser.status_code)) == 0)
	    code = HTTP_STATUS_NOT_FOUND;
	msg = sdsnewlen(pmseries_failure, sizeof(pmseries_failure) - 1);
	flags |= HTTP_FLAG_JSON;
    }
    http_reply(client, msg, code, flags);

    /* close connection if requested or if HTTP 1.0 and keepalive not set */
    if (http_should_keep_alive(&client->u.http.parser) == 0)
	http_close(client);

    pmseries_free_baton(client, baton);
}

static void
pmseries_setup(void *arg)
{
    if (pmDebugOptions.series)
	fprintf(stderr, "series module setup (arg=%p)\n", arg);
}

static pmSeriesSettings pmseries_settings = {
    .callbacks.on_match		= on_pmseries_match,
    .callbacks.on_desc		= on_pmseries_desc,
    .callbacks.on_inst		= on_pmseries_inst,
    .callbacks.on_labelmap	= on_pmseries_labelmap,
    .callbacks.on_instance	= on_pmseries_instance,
    .callbacks.on_context	= on_pmseries_context,
    .callbacks.on_metric	= on_pmseries_metric,
    .callbacks.on_value		= on_pmseries_value,
    .callbacks.on_label		= on_pmseries_label,
    .callbacks.on_done		= on_pmseries_done,
    .module.on_setup		= pmseries_setup,
    .module.on_info		= proxylog,
};

static void
pmseries_setup_request_parameters(struct client *client,
		pmSeriesBaton *baton, dict *parameters)
{
    enum http_method	method = client->u.http.parser.method;
    dictEntry		*entry;
    size_t		length;
    sds			series;

    switch (baton->restkey) {
    case RESTKEY_QUERY:
    case RESTKEY_LOAD:
	/* expect an expression string for these commands */
	if (parameters == NULL && method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	} else if (parameters != NULL &&
	    (entry = dictFind(parameters, PARAM_EXPR)) != NULL) {
	    baton->query = dictGetVal(entry);   /* get sds value */
	    dictSetVal(parameters, entry, NULL);   /* claim this */
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;

    case RESTKEY_DESC:
	/* expect comma-separated series identifier(s) for these commands */
	if (parameters == NULL && method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	} else if (parameters != NULL &&
	    (entry = dictFind(parameters, PARAM_SERIES)) != NULL) {
	    series = dictGetVal(entry);
	    length = sdslen(series);
	    baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;

    case RESTKEY_INSTS:
    case RESTKEY_LABELS:
    case RESTKEY_METRIC:
	/* optional comma-separated series identifier(s) for these commands */
	if (parameters != NULL) {
	    if ((entry = dictFind(parameters, PARAM_SERIES)) != NULL) {
		series = dictGetVal(entry);
		length = sdslen(series);
		baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	    } else if ((entry = dictFind(parameters, PARAM_MATCH)) != NULL) {
		baton->match = dictGetVal(entry);
		baton->sids = &baton->match;
	    }
	}
	break;

    case RESTKEY_SOURCE:
	/* expect comma-separated source identifier(s) for these commands */
	if (parameters != NULL) {
	    if ((entry = dictFind(parameters, PARAM_SOURCE)) != NULL) {
		series = dictGetVal(entry);
		length = sdslen(series);
		baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	    } else if ((entry = dictFind(parameters, PARAM_MATCH)) != NULL) {
		baton->match = dictGetVal(entry);
		baton->sids = &baton->match;
	    }
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;

    case RESTKEY_NONE:
    default:
	client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;
    }
}

/*
 * Test if this is a pmseries REST API command, and if so which one.
 * If this servlet is handling this URL, ensure space for state exists
 * and indicate acceptance for processing this URL via the return code.
 */
static int
pmseries_request_url(struct client *client, sds url, dict *parameters)
{
    pmSeriesBaton	*baton;
    pmSeriesRestKey	key;

    if ((key = pmseries_lookup_restkey(url)) == RESTKEY_NONE)
	return 0;

    if ((baton = client->u.http.data) == NULL) {
	if ((baton = calloc(1, sizeof(*baton))) == NULL)
	    client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	client->u.http.data = baton;
    }
    if (baton) {
	baton->client = client;
	baton->restkey = key;
	pmseries_setup_request_parameters(client, baton, parameters);
    }
    return 1;
}

static int
pmseries_request_headers(struct client *client, struct dict *headers)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "series servlet headers (client=%p)\n", client);
    return 0;
}

static int
pmseries_request_body(struct client *client, const char *content, size_t length)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)client->u.http.data;
    sds			series;

    if (pmDebugOptions.http)
	fprintf(stderr, "series servlet body (client=%p)\n", client);

    if (client->u.http.parser.method != HTTP_POST)
	return 0;

    switch (baton->restkey) {
    case RESTKEY_LOAD:
    case RESTKEY_QUERY:
	if (baton->query)
	    sdsfree(baton->query);
	baton->query = sdsnewlen(content, length);
	break;

    case RESTKEY_DESC:
    case RESTKEY_INSTS:
    case RESTKEY_LABELS:
    case RESTKEY_METRIC:
    case RESTKEY_SOURCE:
	series = sdsnewlen(content, length);
	baton->sids = sdssplitlen(series, length, "\n", 1, &baton->nsids);
	sdsfree(series);
	break;

    default:
	break;
    }
    return 0;
}

/* worker thread function for performing a background load */
static void
pmseries_load_work(uv_work_t *load)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)load->data;
    int			sts;

    if ((sts = pmSeriesLoad(&pmseries_settings,
				    baton->query, baton->flags, baton)) < 0)
	on_pmseries_done(sts, baton);
}

static void
pmseries_load_done(uv_work_t *load, int status)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)load->data;

    baton->working = 0;
    baton->loading.data = NULL;
}

static void
pmseries_request_load(struct client *client, pmSeriesBaton *baton)
{
    static const char	loading[] = \
		"{\"success\":false,\"message\":\"load in-progress\"}\r\n";
    static const char	failed[] = \
		"{\"success\":false,\"message\":\"no load expression\"}\r\n";
    sds			message;

    if (baton->query == NULL) {
	message = sdsnewlen(failed, sizeof(failed) - 1);
	http_reply(client, message, HTTP_STATUS_BAD_REQUEST, HTTP_FLAG_JSON);
	pmseries_free_baton(client, baton);
    } else if (baton->working) {
	message = sdsnewlen(loading, sizeof(loading) - 1);
	http_reply(client, message, HTTP_STATUS_CONFLICT, HTTP_FLAG_JSON);
    } else {
	uv_queue_work(client->proxy->events, &baton->loading,
			pmseries_load_work, pmseries_load_done);
    }
}

static int
pmseries_request_done(struct client *client)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)client->u.http.data;
    int			sts;

    if (client->u.http.parser.status_code)
	return 0;

    switch (baton->restkey) {
    case RESTKEY_QUERY:
	if ((sts = pmSeriesQuery(&pmseries_settings,
					baton->query, baton->flags, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_DESC:
	if ((sts = pmSeriesDescs(&pmseries_settings,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_INSTS:
	if ((sts = pmSeriesInstances(&pmseries_settings,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_LABELS:
	if ((sts = pmSeriesLabels(&pmseries_settings,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_METRIC:
	if ((sts = pmSeriesMetrics(&pmseries_settings,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_SOURCE:
	if ((sts = pmSeriesSources(&pmseries_settings,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_LOAD:
    default:
	pmseries_request_load(client, baton);
	break;
    }
    return 0;
}

static void
pmseries_servlet_setup(struct proxy *proxy)
{
    if (PARAM_EXPR == NULL)
	PARAM_EXPR = sdsnew("expr");
    if (PARAM_MATCH == NULL)
	PARAM_MATCH = sdsnew("match");
    if (PARAM_SERIES == NULL)
	PARAM_SERIES = sdsnew("series");
    if (PARAM_SOURCE == NULL)
	PARAM_SOURCE = sdsnew("source");

    pmSeriesSetSlots(&pmseries_settings.module, proxy->slots);
    pmSeriesSetEventLoop(&pmseries_settings.module, proxy->events);
    pmSeriesSetConfiguration(&pmseries_settings.module, proxy->config);
    pmSeriesSetMetricRegistry(&pmseries_settings.module, proxy->metrics);
}

static void
pmseries_servlet_close(void)
{
    sdsfree(PARAM_EXPR);
    sdsfree(PARAM_MATCH);
    sdsfree(PARAM_SERIES);
    sdsfree(PARAM_SOURCE);
}

struct servlet pmseries_servlet = {
    .name		= "series",
    .setup 		= pmseries_servlet_setup,
    .close 		= pmseries_servlet_close,
    .on_url		= pmseries_request_url,
    .on_headers		= pmseries_request_headers,
    .on_body		= pmseries_request_body,
    .on_done		= pmseries_request_done,
};
