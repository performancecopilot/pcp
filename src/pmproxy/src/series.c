/*
 * Copyright (c) 2019-2020 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */
#include "server.h"
#include "util.h"
#include <assert.h>

typedef enum pmSeriesRestKey {
    RESTKEY_SOURCE	= 1,
    RESTKEY_DESC,
    RESTKEY_INSTS,
    RESTKEY_LABELS,
    RESTKEY_METRIC,
    RESTKEY_VALUES,
    RESTKEY_LOAD,
    RESTKEY_QUERY,
    RESTKEY_PING,
} pmSeriesRestKey;

typedef struct pmSeriesRestCommand {
    const char		*name;
    unsigned int	namelen : 16;
    unsigned int	options : 16;
    pmSeriesRestKey	key;
} pmSeriesRestCommand;

typedef struct pmSeriesBaton {
    struct client	*client;
    pmSeriesRestKey	restkey;
    pmSeriesFlags	flags;
    pmSeriesTimeWindow	window;
    uv_work_t		loading;
    unsigned int	working : 1;
    unsigned int	options : 16;
    int			nsids;
    pmSID		*sids;
    pmSID		sid;
    sds			match;
    sds			suffix;
    unsigned int	series;
    unsigned int	values;
    int			nnames;
    sds			*names;
    sds			info;
    sds			query;
    sds			clientid;
} pmSeriesBaton;

static pmSeriesRestCommand commands[] = {
    { .key = RESTKEY_QUERY, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "query", .namelen = sizeof("query")-1 },
    { .key = RESTKEY_DESC, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "descs", .namelen = sizeof("descs")-1 },
    { .key = RESTKEY_INSTS, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "instances", .namelen = sizeof("instances")-1 },
    { .key = RESTKEY_LABELS, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "labels", .namelen = sizeof("labels")-1 },
    { .key = RESTKEY_METRIC, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "metrics", .namelen = sizeof("metrics")-1 },
    { .key = RESTKEY_SOURCE, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "sources", .namelen = sizeof("sources")-1 },
    { .key = RESTKEY_VALUES, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "values", .namelen = sizeof("values")-1 },
    { .key = RESTKEY_LOAD, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "load", .namelen = sizeof("load")-1 },
    { .key = RESTKEY_PING, .options = HTTP_OPTIONS_GET,
	    .name = "ping", .namelen = sizeof("ping")-1 },
    { .name = NULL }	/* sentinel */
};

/* constant string keys (initialized during servlet setup) */
static sds PARAM_EXPR, PARAM_MATCH, PARAM_SERIES, PARAM_SOURCE,
	   PARAM_CLIENT, PARAM_NAME, PARAM_NAMES;
static sds PARAM_ALIGN, PARAM_COUNT, PARAM_DELTA, PARAM_OFFSET,
	   PARAM_SAMPLES, PARAM_INTERVAL, PARAM_START, PARAM_FINISH,
	   PARAM_BEGIN, PARAM_END, PARAM_RANGE, PARAM_ZONE;

/* constant global strings (read-only) */
static const char pmseries_success[] = "\"success\":true";
static const char pmseries_failure[] = "\"success\":false";

static pmSeriesRestCommand *
pmseries_lookup_rest_command(sds url)
{
    pmSeriesRestCommand	*cp;
    const char		*name;

    if (sdslen(url) >= (sizeof("/series/") - 1) &&
	strncmp(url, "/series/", sizeof("/series/") - 1) == 0) {
	name = (const char *)url + sizeof("/series/") - 1;
	for (cp = &commands[0]; cp->name; cp++) {
	    if (strncmp(cp->name, name, cp->namelen) == 0)
		return cp;
	}
    }
    return NULL;
}

static void
pmseries_data_release(struct client *client)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)client->u.http.data;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: %p for client %p\n", "pmseries_data_release",
			baton, client);

    if (baton->nsids)
	sdsfreesplitres(baton->sids, baton->nsids);
    if (baton->names)
	sdsfreesplitres(baton->names, baton->nnames);

    sdsfree(baton->sid);
    sdsfree(baton->info);
    sdsfree(baton->query);
    sdsfree(baton->suffix);
    sdsfree(baton->clientid);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

/*
 * If any request is accompanied by 'client', the client is using
 * this to identify responses.  Wrap the usual response using the
 * identifier - by adding a JSON object at the top level with two
 * fields, 'client' (ID) and 'result' (the rest of the response).
 */
static sds
push_client_identifier(pmSeriesBaton *baton, sds result)
{
    if (baton->clientid) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	return sdscatfmt(result, "{\"client\":%S,\"result\":", baton->clientid);
    }
    return result;
}

static int
on_pmseries_match(pmSID sid, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			result;

    if (baton->sid == NULL || sdscmp(baton->sid, sid) != 0) {
	result = http_get_buffer(baton->client);
	if (baton->series++ == 0) {
	    result = push_client_identifier(baton, result);
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

    if (pmDebugOptions.query && pmDebugOptions.desperate)
	fprintf(stderr, "on_pmseries_value: arg=%p %s %s %s\n",
	    arg, value->timestamp, value->data, value->series);


    timestamp = value->timestamp;
    series = value->series;
    quoted = sdscatrepr(sdsempty(), value->data, sdslen(value->data));

    if (baton->series++ == 0) {
	result = push_client_identifier(baton, result);
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
	result = push_client_identifier(baton, result);
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
    sds			s, quoted, result = http_get_buffer(client);

    quoted = sdscatrepr(sdsempty(), name, sdslen(name));
    if (sid == NULL) {	/* request for all metric names globally */
	if (baton->values == 0) {
	    result = push_client_identifier(baton, result);
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s%S", prefix, quoted);
    } else {
	if ((s = baton->sid) == NULL) {	/* first series seem */
	    baton->sid = sdsdup(sid);
	    baton->series++;
	    assert(baton->values == 0);
	    result = push_client_identifier(baton, result);
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
	result = sdscatfmt(result, "%s{\"series\":\"%S\",\"name\":%S}",
				prefix, sid, quoted);
    }
    sdsfree(quoted);
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
	    result = push_client_identifier(baton, result);
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
	    result = push_client_identifier(baton, result);
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
    sds			s, result;

    if (baton->nsids != 0)
	return 0;

    result = http_get_buffer(client);

    if (sid == NULL) {	/* all labels globally requested */
	if (baton->values == 0) {
	    result = push_client_identifier(baton, result);
	    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	    prefix = "[";
	} else {
	    prefix = ",";
	}
	result = sdscatfmt(result, "%s\"%S\"", prefix, label);
    }
    else if ((s = baton->sid) == NULL) {
	result = push_client_identifier(baton, result);
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
on_pmseries_labelvalues(pmSeriesBaton *baton, pmSeriesLabel *label)
{
    struct client	*client = baton->client;
    sds			s, name, value, result;

    name = label->name;
    value = label->value;
    result = http_get_buffer(client);

    if ((s = baton->sid) == NULL) {	/* first label name */
	baton->sid = sdsdup(name);	/* (stash name in sid for convenience) */
	result = push_client_identifier(baton, result);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	result = sdscatfmt(result, "{\"%S\":[%S", name, value);
    } else if (sdscmp(s, name) != 0) {	/* next label name */
	sdsclear(baton->sid);
	baton->sid = sdscpylen(s, name, sdslen(name));
	result = sdscatfmt(result, "],\"%S\":[%S", name, value);
    } else {	/* next label value for previous label name */
	result = sdscatfmt(result, ",%S", value);
    }

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
    sds			s, name, value, result;

    if (baton->nsids == 0)
	return on_pmseries_labelvalues(baton, label);

    name = label->name;
    value = label->value;
    result = http_get_buffer(client);

    if ((s = baton->sid) == NULL) {
	baton->sid = sdsdup(sid);
	baton->series++;
	result = push_client_identifier(baton, result);
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
	    result = push_client_identifier(baton, result);
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
	result = push_client_identifier(baton, result);
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
	result = push_client_identifier(baton, result);
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
    http_options_t	options = baton->options;
    http_flags_t	flags = client->u.http.flags;
    http_code_t		code;
    sds			msg;

    if (pmDebugOptions.query && pmDebugOptions.desperate)
	fprintf(stderr, "on_pmseries_done: arg=%p status=%d\n", arg, status);
    if (status == 0) {
	code = HTTP_STATUS_OK;
	/* complete current response with JSON suffix if needed */
	if ((msg = baton->suffix) == NULL) {	/* empty OK response */
	    switch (baton->restkey) {
	    case RESTKEY_LABELS:
		if (baton->names != NULL) {
		    /* the label values API method always returns an */
		    /* object { labelName: [labelValues] } */
		    if (baton->clientid)
			msg = sdscatfmt(sdsempty(),
				    "{\"client\":%S,\"result\":{}}\r\n",
				    baton->clientid);
		    else
			msg = sdsnewlen("{}\r\n", 4);
		    break;
		}
	    case RESTKEY_DESC:
	    case RESTKEY_INSTS:
	    case RESTKEY_METRIC:
	    case RESTKEY_VALUES:
	    case RESTKEY_SOURCE:
	    case RESTKEY_QUERY:		/* result is an empty array */
		if (baton->clientid)
		    msg = sdscatfmt(sdsempty(),
				"{\"client\":%S,\"result\":[]}\r\n",
				baton->clientid);
		else
		    msg = sdsnewlen("[]\r\n", 4);
		break;

	    default:			/* use success:true default */
		msg = sdsnewlen("{", 1);
		if (baton->clientid)
		    msg = sdscatfmt(msg, "\"client\":%S,", baton->clientid);
		msg = sdscatfmt(msg, "%s}\r\n", pmseries_success);
		break;
	    }
	}
	baton->suffix = NULL;
    } else {
	if (((code = client->u.http.parser.status_code)) == 0)
	    code = HTTP_STATUS_BAD_REQUEST;
	msg = sdsnewlen("{", 1);
	if (baton->clientid)
	    msg = sdscatfmt(msg, "\"client\":%S,", baton->clientid);
	msg = sdscatfmt(msg, "%s}\r\n", pmseries_failure);
	flags |= HTTP_FLAG_JSON;
    }
    http_reply(client, msg, code, flags, options);
}

static void
on_pmseries_error(pmLogLevel level, sds message, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;
    struct client	*client = baton->client;
    http_options_t	options = baton->options;
    http_flags_t	flags = client->u.http.flags | HTTP_FLAG_JSON;
    http_code_t		status_code;
    sds			quoted, msg;

    if (((status_code = client->u.http.parser.status_code)) == 0)
	status_code = (level > PMLOG_REQUEST) ?
		HTTP_STATUS_INTERNAL_SERVER_ERROR : HTTP_STATUS_BAD_REQUEST;
    quoted = sdscatrepr(sdsempty(), message, sdslen(message));
    msg = sdsnewlen("{", 1);
    if (baton->clientid)
	msg = sdscatfmt(msg, "\"client\":%S,", baton->clientid);
    msg = sdscatfmt(msg, "\"message\":%S,%s}\r\n", quoted, pmseries_failure);
    sdsfree(quoted);

    http_reply(client, msg, status_code, flags, options);
}

static void
pmseries_setup(void *arg)
{
    if (pmDebugOptions.series)
	fprintf(stderr, "series module setup (arg=%p)\n", arg);
}

static void
pmseries_log(pmLogLevel level, sds message, void *arg)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)arg;

    /* locally log low priority diagnostics or when already responding */
    if (baton == NULL) {
	fprintf(stderr, "pmseries_log: Botch: baton is NULL msg=\"%s\"\n", message);
	__pmDumpStack();
	return;
    }
    if (level <= PMLOG_INFO || baton->suffix) {
	if (baton->client == NULL)
	    proxylog(level, message, NULL);
	else
	    proxylog(level, message, baton->client->proxy);
    }
    else	/* inform client, complete request */
	on_pmseries_error(level, message, baton);
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
    .module.on_info		= pmseries_log,
};

static void
pmseries_setup_request_parameters(struct client *client,
		pmSeriesBaton *baton, dict *parameters)
{
    enum http_method	method = client->u.http.parser.method;
    dictEntry		*entry;
    size_t		length;
    sds			series, names, expr;

    if (parameters) {
	/* allow all APIs to pass(-through) a 'client' parameter */
	if ((entry = dictFind(parameters, PARAM_CLIENT)) != NULL) {
	    series = dictGetVal(entry);   /* leave sds value, dup'd below */
	    baton->clientid = sdscatrepr(sdsempty(), series, sdslen(series));
	}
    }

    if (parameters && baton->restkey == RESTKEY_VALUES) {
	/* not claiming time window dict values, freed the usual way */
	if ((entry = dictFind(parameters, PARAM_ALIGN)) != NULL)
	    baton->window.align = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_SAMPLES)) != NULL ||
	    (entry = dictFind(parameters, PARAM_COUNT)) != NULL)
	    baton->window.count = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_INTERVAL)) != NULL ||
	    (entry = dictFind(parameters, PARAM_DELTA)) != NULL)
	    baton->window.delta = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_START)) != NULL ||
	    (entry = dictFind(parameters, PARAM_BEGIN)) != NULL)
	    baton->window.start = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_FINISH)) != NULL ||
	    (entry = dictFind(parameters, PARAM_END)) != NULL)
	    baton->window.end = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_RANGE)) != NULL)
	    baton->window.range = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_OFFSET)) != NULL)
	    baton->window.offset = dictGetVal(entry);
	if ((entry = dictFind(parameters, PARAM_ZONE)) != NULL)
	    baton->window.zone = dictGetVal(entry);
    }

    switch (baton->restkey) {
    case RESTKEY_QUERY:
    case RESTKEY_LOAD:
	/* expect an expression string for these commands */
	if (parameters == NULL && method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	} else if (parameters != NULL &&
	    (entry = dictFind(parameters, PARAM_EXPR)) != NULL) {
	    expr = dictGetVal(entry);   /* get sds value */
	    if (expr == NULL || sdslen(expr) == 0) {
		client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    } else {
		dictSetVal(parameters, entry, NULL);   /* claim this */
		baton->query = expr;
	    }
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
	    if (series == NULL || (length = sdslen(series)) == 0)
		client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    else
		baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;

    case RESTKEY_INSTS:
    case RESTKEY_LABELS:
    case RESTKEY_METRIC:
    case RESTKEY_VALUES:
	/* optional comma-separated series identifier(s) for these commands */
	if (parameters != NULL) {
	    if ((entry = dictFind(parameters, PARAM_SERIES)) != NULL) {
		series = dictGetVal(entry);
		if (series == NULL || (length = sdslen(series)) == 0)
		    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
		else
		    baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	    } else if ((entry = dictFind(parameters, PARAM_MATCH)) != NULL) {
		baton->match = dictGetVal(entry);
		baton->sids = &baton->match;
	    }
	}
	/* special case: requesting all known values for given label name(s) */
	if (parameters && baton->restkey == RESTKEY_LABELS) {
	    if ((entry = dictFind(parameters, PARAM_NAME)) == NULL)
		entry = dictFind(parameters, PARAM_NAMES);	/* synonym */
	    if (entry != NULL) {
		names = dictGetVal(entry);
		if (names == NULL || (length = sdslen(names)) == 0)
		    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
		else
		    baton->names = sdssplitlen(names, length, ",", 1, &baton->nnames);
	    }
	}
	break;

    case RESTKEY_SOURCE:
	/* expect comma-separated source identifier(s) for these commands */
	if (parameters != NULL) {
	    if ((entry = dictFind(parameters, PARAM_SOURCE)) != NULL) {
		series = dictGetVal(entry);
		if (series == NULL || (length = sdslen(series)) == 0)
		    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
		else
		    baton->sids = sdssplitlen(series, length, ",", 1, &baton->nsids);
	    } else if ((entry = dictFind(parameters, PARAM_MATCH)) != NULL) {
		baton->match = dictGetVal(entry);
		baton->sids = &baton->match;
	    }
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;

    case RESTKEY_PING:
	break;

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
    pmSeriesRestCommand	*command;

    if ((command = pmseries_lookup_rest_command(url)) == NULL)
	return 0;

    if ((baton = calloc(1, sizeof(*baton))) != NULL) {
	client->u.http.data = baton;
	baton->client = client;
	baton->restkey = command->key;
	baton->options = command->options;
	pmseries_setup_request_parameters(client, baton, parameters);
    } else {
	client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
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

    if (pmDebugOptions.http)
	fprintf(stderr, "series servlet body (client=%p)\n", client);

    if (client->u.http.parser.method != HTTP_POST || client->u.http.parameters != NULL)
	return 0;

    switch (baton->restkey) {
    case RESTKEY_LOAD:
    case RESTKEY_QUERY:
    case RESTKEY_DESC:
    case RESTKEY_INSTS:
    case RESTKEY_LABELS:
    case RESTKEY_METRIC:
    case RESTKEY_SOURCE:
    case RESTKEY_VALUES:
	/* parse URL encoded parameters in the request body */
	/* in the same way as the URL query string */
	http_parameters(content, length, &client->u.http.parameters);
	pmseries_setup_request_parameters(client, baton, client->u.http.parameters);
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
	http_reply(client, message, HTTP_STATUS_BAD_REQUEST,
			HTTP_FLAG_JSON, baton->options);
    } else if (baton->working) {
	message = sdsnewlen(loading, sizeof(loading) - 1);
	http_reply(client, message, HTTP_STATUS_CONFLICT,
			HTTP_FLAG_JSON, baton->options);
    } else {
        baton->loading.data = baton;         
	uv_queue_work(client->proxy->events, &baton->loading,
			pmseries_load_work, pmseries_load_done);
    }
}

static int
pmseries_request_done(struct client *client)
{
    pmSeriesBaton	*baton = (pmSeriesBaton *)client->u.http.data;
    int			sts;

    if (client->u.http.parser.status_code) {
	on_pmseries_done(-EINVAL, baton);
	return 1;
    }

    if (client->u.http.parser.method == HTTP_OPTIONS ||
	client->u.http.parser.method == HTTP_TRACE ||
	client->u.http.parser.method == HTTP_HEAD) {
	on_pmseries_done(0, baton);
	return 0;
    }

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
	sts = (baton->names == NULL) ?
	    pmSeriesLabels(&pmseries_settings,
					baton->nsids, baton->sids, baton) :
	    pmSeriesLabelValues(&pmseries_settings,
					baton->nnames, baton->names, baton);
	if (sts < 0)
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

    case RESTKEY_VALUES:
	if ((sts = pmSeriesValues(&pmseries_settings, &baton->window,
					baton->nsids, baton->sids, baton)) < 0)
	    on_pmseries_done(sts, baton);
	break;

    case RESTKEY_LOAD:
	pmseries_request_load(client, baton);
	break;

    default:  /*PING*/
	on_pmseries_done(0, baton);
	break;
    }
    return 0;
}

static void
pmseries_servlet_setup(struct proxy *proxy)
{
    mmv_registry_t	*metric_registry = proxymetrics(proxy, METRICS_SERIES);

    PARAM_EXPR = sdsnew("expr");
    PARAM_MATCH = sdsnew("match");
    PARAM_NAME = sdsnew("name");
    PARAM_NAMES = sdsnew("names");
    PARAM_SERIES = sdsnew("series");
    PARAM_SOURCE = sdsnew("source");
    PARAM_CLIENT = sdsnew("client");

    PARAM_ALIGN = sdsnew("align");
    PARAM_BEGIN = sdsnew("begin");
    PARAM_COUNT = sdsnew("count");
    PARAM_DELTA = sdsnew("delta");
    PARAM_END = sdsnew("end");
    PARAM_INTERVAL = sdsnew("interval");
    PARAM_OFFSET = sdsnew("offset");
    PARAM_RANGE = sdsnew("range");
    PARAM_SAMPLES = sdsnew("samples");
    PARAM_START = sdsnew("start");
    PARAM_FINISH = sdsnew("finish");
    PARAM_ZONE = sdsnew("zone");

    pmSeriesSetSlots(&pmseries_settings.module, proxy->slots);
    pmSeriesSetEventLoop(&pmseries_settings.module, proxy->events);
    pmSeriesSetConfiguration(&pmseries_settings.module, proxy->config);
    pmSeriesSetMetricRegistry(&pmseries_settings.module, metric_registry);

    pmSeriesSetup(&pmseries_settings.module, proxy);
}

static void
pmseries_servlet_close(struct proxy *proxy)
{
    pmSeriesClose(&pmseries_settings.module);
    proxymetrics_close(proxy, METRICS_SERIES);

    sdsfree(PARAM_EXPR);
    sdsfree(PARAM_MATCH);
    sdsfree(PARAM_NAME);
    sdsfree(PARAM_NAMES);
    sdsfree(PARAM_SERIES);
    sdsfree(PARAM_SOURCE);
    sdsfree(PARAM_CLIENT);

    sdsfree(PARAM_ALIGN);
    sdsfree(PARAM_BEGIN);
    sdsfree(PARAM_COUNT);
    sdsfree(PARAM_DELTA);
    sdsfree(PARAM_END);
    sdsfree(PARAM_FINISH);
    sdsfree(PARAM_INTERVAL);
    sdsfree(PARAM_OFFSET);
    sdsfree(PARAM_RANGE);
    sdsfree(PARAM_SAMPLES);
    sdsfree(PARAM_START);
    sdsfree(PARAM_ZONE);
}

struct servlet pmseries_servlet = {
    .name		= "series",
    .setup 		= pmseries_servlet_setup,
    .close 		= pmseries_servlet_close,
    .on_url		= pmseries_request_url,
    .on_headers		= pmseries_request_headers,
    .on_body		= pmseries_request_body,
    .on_done		= pmseries_request_done,
    .on_release		= pmseries_data_release,
};
