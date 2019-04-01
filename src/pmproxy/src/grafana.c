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
#include <assert.h>
#include "server.h"

typedef enum GrafanaRestKey {
    RESTKEY_NONE	= 0,
    RESTKEY_TEST,
    RESTKEY_QUERY,
    RESTKEY_SEARCH,
    RESTKEY_TAGKEYS,
    RESTKEY_TAGVALUES,
    RESTKEY_ANNOTATIONS,
} GrafanaRestKey;

typedef struct GrafanaRestCommand {
    const char		*name;
    unsigned int	size;
    GrafanaRestKey	key;
} GrafanaRestCommand;

typedef struct GrafanaBaton {
    struct client	*client;
    GrafanaRestKey	restkey;
    unsigned int	series;
    unsigned int	values;
    pmSID		sid;
    sds			refId;
    sds			panelId;
    sds			dashboardId;
    sds			suffix;
    sds			query;
    sds			target;
    sds			start;
    sds			finish;
    sds			maxvalues;
    sds			maxseries;
    sds			interval;
    sds			timezone;
} GrafanaBaton;

static GrafanaRestCommand commands[] = {
    { .key = RESTKEY_QUERY, .name = "query", .size = sizeof("query")-1 },
    { .key = RESTKEY_SEARCH, .name = "search", .size = sizeof("search")-1 },
    { .key = RESTKEY_TAGKEYS, .name = "tagkeys", .size = sizeof("tagkeys")-1 },
    { .key = RESTKEY_TAGVALUES, .name = "tagvalues", .size = sizeof("tagvalues")-1 },
    { .key = RESTKEY_ANNOTATIONS, .name = "annotations", .size = sizeof("annotations")-1 },
    { .key = RESTKEY_NONE }
};

/* constant string keys (initialized during servlet setup) */
static sds PARAM_REFID, PARAM_PANELID, PARAM_DASHBOARDID,
	   PARAM_EXPR, PARAM_TARGET, PARAM_TIMEZONE,
	   PARAM_START, PARAM_FINISH, PARAM_INTERVAL,
	   PARAM_MAXSERIES, PARAM_MAXVALUES;

/* constant global strings (read-only) */
static const char grafana_success[] = "{\"success\":true}\r\n";
static const char grafana_failure[] = "{\"success\":false}\r\n";

static GrafanaRestKey
grafana_lookup_restkey(sds url)
{
    GrafanaRestCommand	*cp;
    const char		*name;
    unsigned int	length = sdslen(url);

    if (length == sizeof("/grafana") - 1 &&
	strncmp("/grafana", url, sizeof("/grafana") - 1) == 0)
	return RESTKEY_TEST;

    if (sdslen(url) >= (sizeof("/grafana/") - 1) &&
	strncmp(url, "/grafana/", sizeof("/grafana/") - 1) == 0) {
	name = (const char *)url + sizeof("/grafana/") - 1;
	for (cp = &commands[0]; cp->name; cp++) {
	    if (strncmp(cp->name, name, cp->size) == 0)
		return cp->key;
	}
    }
    return RESTKEY_NONE;
}

static void
grafana_free_baton(struct client *client, GrafanaBaton *baton)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "grafana_free_baton %p for client %p\n", baton, client);

    client->u.http.data = NULL;	/* remove baton link for this client */

    if (baton->refId)
	sdsfree(baton->refId);
    if (baton->panelId)
	sdsfree(baton->panelId);
    if (baton->dashboardId)
	sdsfree(baton->dashboardId);
    if (baton->suffix)
	sdsfree(baton->suffix);
    if (baton->query)
	sdsfree(baton->query);
    if (baton->target)
	sdsfree(baton->target);
    if (baton->start)
	sdsfree(baton->start);
    if (baton->finish)
	sdsfree(baton->finish);
    if (baton->interval)
	sdsfree(baton->interval);
    if (baton->timezone)
	sdsfree(baton->timezone);
    if (baton->maxvalues)
	sdsfree(baton->maxvalues);
    if (baton->maxseries)
	sdsfree(baton->maxseries);
    memset(baton, 0, sizeof(*baton));
}

static int
on_grafana_match(pmSID sid, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s\n", "on_grafana_match",
		baton->client, sid);
    return 0;
}

static int
on_grafana_value(pmSID sid, pmSeriesValue *value, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;
    struct client	*client = baton->client;
    char		*prefix, *s;
    sds			result, timestamp, series, quoted;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s\n", "on_grafana_value", client, sid);

    assert(client != NULL);
    result = http_get_buffer(client);

    quoted = sdscatrepr(sdsempty(), value->data, sdslen(value->data));
    series = value->series;
    timestamp = value->timestamp;
    /* chop off the sub-millisecond component of timestamp */
    if ((s = strchr(timestamp, '.')) != NULL)
	*s = '\0';

    baton->values++;

    if (baton->sid == NULL) {
	baton->sid = sdsnewlen(series, sdslen(series));
	baton->series = 0;
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	prefix = "[";
    } else {
	prefix = ",";
    }
    if (baton->series == 0 || sdscmp(baton->sid, series) != 0) {
	baton->sid = sdscpylen(baton->sid, series, sdslen(series));
	if (baton->series++ != 0) {
	    baton->suffix = json_pop_suffix(baton->suffix);
	    baton->suffix = json_pop_suffix(baton->suffix);
	    result = sdscat(result, "]}");
	}
	baton->values = 0;

	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	result = sdscatfmt(result, "%s{\"target\":\"%S\",\"series\":\"%S\"",
				prefix, baton->target, series);
	if (baton->refId)
	    result = sdscatfmt(result, ",\"refId\":\"%S\"", baton->refId);
	if (baton->panelId)
	    result = sdscatfmt(result, ",\"panelId\":\"%S\"", baton->panelId);
	if (baton->dashboardId)
	    result = sdscatfmt(result, ",\"dashboardId\":\"%S\"",
				baton->dashboardId);
	result = sdscat(result, ",\"datapoints\":");
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	prefix = "[";
    } else {
	prefix = ",";
    }
    baton->values++;
    result = sdscatfmt(result, "%s[%S,%s]", prefix, quoted, timestamp);

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_grafana_desc(pmSID sid, pmSeriesDesc *desc, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s\n", "on_grafana_desc",
			baton->client, sid);
    (void)desc;
    return 0;
}

static int
on_grafana_metric(pmSID sid, sds name, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;
    struct client	*client = baton->client;
    const char		*prefix;
    sds			result = http_get_buffer(client);

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p (name=%s)\n", "on_grafana_metric",
			client, name);

    /* append search query result */
    if (baton->values++ == 0) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	prefix = "[";
    } else {
	prefix = ",";
    }
    result = sdscatfmt(result, "%s\"%S\"", prefix, name);

    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
    return 0;
}

static int
on_grafana_context(pmSID sid, sds name, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s (context=%s)\n", "on_grafana_context",
			baton->client, sid, name);
    return 0;
}

static int
on_grafana_label(pmSID sid, sds label, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s (label=%s)\n", "on_grafana_label",
			baton->client, sid, label);
    (void)baton; (void)sid; (void)label;
    return 0;
}

static int
on_grafana_labelmap(pmSID sid, pmSeriesLabel *label, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s\n", "on_grafana_labelmap",
			baton->client, sid);
    (void)label;
    return 0;
}

static int
on_grafana_instance(pmSID sid, sds name, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s [%s]\n", "on_grafana_instance",
			baton->client, sid, name);
    return 0;
}

static int
on_grafana_inst(pmSID sid, pmSeriesInst *inst, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p %s\n", "on_grafana_instance",
			baton->client, sid);
    (void)inst;
    return 0;
}

static void
on_grafana_done(int status, void *arg)
{
    GrafanaBaton	*baton = (GrafanaBaton *)arg;
    struct client	*client = baton->client;
    http_flags		flags = client->u.http.flags;
    http_code		code;
    sds			msg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p (sts=%d)\n", "on_grafana_done",
			client, status);

    if (status == 0) {
	code = HTTP_STATUS_OK;
	/* complete current response with JSON suffix if needed */
	if ((msg = baton->suffix) == NULL)	/* empty OK response */
	    msg = sdsnewlen(grafana_success, sizeof(grafana_success) - 1);
	baton->suffix = NULL;
    } else {
	if (((code = client->u.http.parser.status_code)) == 0)
	    code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	msg = sdsnewlen(grafana_failure, sizeof(grafana_failure) - 1);
	flags |= HTTP_FLAG_JSON;
    }
    http_reply(client, msg, code, flags);

    /* close connection if requested or if HTTP 1.0 and keepalive not set */
    if (http_should_keep_alive(&client->u.http.parser) == 0)
	http_close(client);

    grafana_free_baton(client, baton);
}

static void
grafana_setup(void *arg)
{
    (void)arg;
}

static pmSeriesSettings grafana_settings = {
    .callbacks.on_match		= on_grafana_match,
    .callbacks.on_desc		= on_grafana_desc,
    .callbacks.on_inst		= on_grafana_inst,
    .callbacks.on_labelmap	= on_grafana_labelmap,
    .callbacks.on_instance	= on_grafana_instance,
    .callbacks.on_context	= on_grafana_context,
    .callbacks.on_metric	= on_grafana_metric,
    .callbacks.on_value		= on_grafana_value,
    .callbacks.on_label		= on_grafana_label,
    .callbacks.on_done		= on_grafana_done,
    .module.on_setup		= grafana_setup,
    .module.on_info		= proxylog,
};

static void
grafana_setup_request_parameters(struct client *client,
		GrafanaBaton *baton, dict *parameters)
{
    enum http_method	method = client->u.http.parser.method;
    dictEntry		*entry;

    switch (baton->restkey) {
    case RESTKEY_TEST:
    case RESTKEY_TAGKEYS:
    case RESTKEY_TAGVALUES:
    case RESTKEY_ANNOTATIONS:
	if (method != HTTP_GET)
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;

    case RESTKEY_SEARCH:
	if (method != HTTP_GET) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	} else if (parameters != NULL &&
	    (entry = dictFind(parameters, PARAM_TARGET)) != NULL) {
	    baton->target = dictGetVal(entry);   /* get sds value */
	    dictSetVal(parameters, entry, NULL);   /* claim this */
	}
	break;

    case RESTKEY_QUERY:
	/* expect an expression string for these commands */
	if (parameters == NULL && method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	} else if (parameters != NULL &&
	    (entry = dictFind(parameters, PARAM_EXPR)) != NULL) {
	    baton->target = dictGetVal(entry);   /* get sds value */
	    dictSetVal(parameters, entry, NULL);   /* claim this */
	    /* extract optional parameters, set defaults */
	    if ((entry = dictFind(parameters, PARAM_REFID)) != NULL) {
		baton->refId = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_PANELID)) != NULL) {
		baton->panelId = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_DASHBOARDID)) != NULL) {
		baton->dashboardId = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_START)) != NULL) {
		baton->start = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    } else {
		baton->start = sdsnew("-6hours");	/* default */
	    }
	    if ((entry = dictFind(parameters, PARAM_FINISH)) != NULL) {
		baton->finish = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_INTERVAL)) != NULL) {
		baton->interval = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_TIMEZONE)) != NULL) {
		baton->timezone = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_MAXSERIES)) != NULL) {
		baton->maxseries = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	    if ((entry = dictFind(parameters, PARAM_MAXVALUES)) != NULL) {
		baton->maxvalues = dictGetVal(entry);
		dictSetVal(parameters, entry, NULL);
	    }
	} else if (method != HTTP_POST) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	}
	break;
    default:
	client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;
    }
}

/*
 * Test if this is a grafana REST API command, and if so which one.
 * If this servlet is handling this URL, ensure space for state exists
 * and indicate acceptance for processing this URL via the return code.
 */
static int
grafana_request_url(struct client *client, sds url, dict *parameters)
{
    GrafanaBaton	*baton;
    GrafanaRestKey	key;

    if ((key = grafana_lookup_restkey(url)) == RESTKEY_NONE)
	return 0;

    if ((baton = client->u.http.data) == NULL) {
	if ((baton = calloc(1, sizeof(*baton))) == NULL)
	    client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	client->u.http.data = baton;
	baton->client = client;
    }

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: client=%p grafana url %s (baton=%p bc=%p)\n",
			"grafana_request_url", client, url, baton, baton->client);

    if (baton) {
	baton->restkey = key;
	grafana_setup_request_parameters(client, baton, parameters);
    }
    return 1;
}

static int
grafana_request_headers(struct client *client, struct dict *headers)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "grafana servlet headers (client=%p)\n", client);
    return 0;
}

static int
grafana_request_body(struct client *client, const char *content, size_t length)
{
    GrafanaBaton	*baton = (GrafanaBaton *)client->u.http.data;

    if (pmDebugOptions.http)
	fprintf(stderr, "grafana servlet body (client=%p):\n%.*s",
			client, (int)length, content);

    if (client->u.http.parser.method != HTTP_POST)
	return 0;

    if (pmDebugOptions.http)
	fprintf(stderr, "grafana servlet POST (client=%p):\n%.*s",
			client, (int)length, content);

    switch (baton->restkey) {
    case RESTKEY_QUERY:
	if (baton->target)
	    sdsfree(baton->target);
	baton->target = sdsnewlen(content, length);
	break;

    case RESTKEY_SEARCH:
    case RESTKEY_TAGKEYS:
    case RESTKEY_TAGVALUES:
    case RESTKEY_ANNOTATIONS:
    default:
	break;
    }
    return 0;
}

static void
grafana_request_annotations(struct client *client, GrafanaBaton *baton)
{
    static const char	failed[] = \
"{\"success\":false,\"message\":\"Grafana annotations not yet implemented\"}\r\n";
    sds			message;

    message = sdsnewlen(failed, sizeof(failed) - 1);
    http_reply(client, message, HTTP_STATUS_OK, HTTP_FLAG_JSON);
}

static void
grafana_request_search(struct client *client, GrafanaBaton *baton)
{
    sds			message, *target = NULL;
    int			sts;

    /* search metric names by default */
    if (baton->target != NULL) {
	target = &baton->target;
	if ((sts = pmSeriesMetrics(&grafana_settings, 0, target, baton)) < 0)
	    on_grafana_done(sts, baton);
    } else {
	message = sdsnewlen("[]", 2);
	http_reply(client, message, HTTP_STATUS_OK, HTTP_FLAG_JSON);
    }
}

static void
grafana_request_query(struct client *client, GrafanaBaton *baton)
{
    sds			query = sdsdup(baton->target);
    int			sts;

    query = sdscatfmt(query, "[start:\"%S\"", baton->start);
    if (baton->finish)
	query = sdscatfmt(query, ",finish:\"%S\"", baton->finish);
    if (baton->interval)
	query = sdscatfmt(query, ",interval:\"%S\"", baton->interval);
    if (baton->timezone)
	query = sdscatfmt(query, ",timezone:\"%S\"", baton->timezone);
    else
	query = sdscatfmt(query, ",timezone:\"UTC\"");
    if (baton->maxvalues)
	query = sdscatfmt(query, ",samples:%S", baton->maxvalues);
    if (baton->maxseries)
	query = sdscatfmt(query, ",maxseries:%S", baton->maxseries);
    baton->query = sdscatlen(query, "]", 1);

    if ((sts = pmSeriesQuery(&grafana_settings, baton->query, 0, baton)) < 0)
	on_grafana_done(sts, baton);
}

static void
grafana_request_tagkeys(struct client *client, GrafanaBaton *baton)
{
    const char		failed[] = \
"{\"success\":false,\"message\":\"Grafana tagkeys not yet implemented\"}\r\n";
    sds			message;

    message = sdsnewlen(failed, sizeof(failed) - 1);
    http_reply(client, message, HTTP_STATUS_OK, HTTP_FLAG_JSON);
}

static void
grafana_request_tagvalues(struct client *client, GrafanaBaton *baton)
{
    static const char	failed[] = \
"{\"success\":false,\"message\":\"Grafana tagvalues not yet implemented\"}\r\n";
    sds			message;

    message = sdsnewlen(failed, sizeof(failed) - 1);
    http_reply(client, message, HTTP_STATUS_OK, HTTP_FLAG_JSON);
}

static int
grafana_request_done(struct client *client)
{
    GrafanaBaton	*baton = (GrafanaBaton *)client->u.http.data;
    sds			result;

    switch (baton->restkey) {
    case RESTKEY_QUERY:
	grafana_request_query(client, baton);
	break;

    case RESTKEY_SEARCH:
	grafana_request_search(client, baton);
	break;

    case RESTKEY_ANNOTATIONS:
	grafana_request_annotations(client, baton);
	break;

    case RESTKEY_TAGVALUES:
	grafana_request_tagvalues(client, baton);
	break;

    case RESTKEY_TAGKEYS:
	grafana_request_tagkeys(client, baton);
	break;

    case RESTKEY_TEST:
    default:
	result = sdsnewlen(grafana_success, sizeof(grafana_success) - 1);
	http_reply(client, result, HTTP_STATUS_OK, HTTP_FLAG_JSON);
	break;
    }
    return 0;
}

static void
grafana_servlet_setup(struct proxy *proxy)
{
    if (PARAM_REFID == NULL)
	PARAM_REFID = sdsnew("refId");
    if (PARAM_PANELID == NULL)
	PARAM_PANELID = sdsnew("panelId");
    if (PARAM_DASHBOARDID == NULL)
	PARAM_DASHBOARDID = sdsnew("dashboardId");
    if (PARAM_TARGET == NULL)
	PARAM_TARGET = sdsnew("target");
    if (PARAM_EXPR == NULL)
	PARAM_EXPR = sdsnew("expr");
    if (PARAM_START == NULL)
	PARAM_START = sdsnew("start");
    if (PARAM_FINISH == NULL)
	PARAM_FINISH = sdsnew("finish");
    if (PARAM_INTERVAL == NULL)
	PARAM_INTERVAL = sdsnew("interval");
    if (PARAM_TIMEZONE == NULL)
	PARAM_TIMEZONE = sdsnew("timezone");
    if (PARAM_MAXSERIES == NULL)
	PARAM_MAXSERIES = sdsnew("maxseries");
    if (PARAM_MAXVALUES == NULL)
	PARAM_MAXVALUES = sdsnew("maxdatapoints");

    pmSeriesSetSlots(&grafana_settings.module, proxy->slots);
    pmSeriesSetEventLoop(&grafana_settings.module, proxy->events);
    pmSeriesSetConfiguration(&grafana_settings.module, proxy->config);
    pmSeriesSetMetricRegistry(&grafana_settings.module, proxy->metrics);
}

struct servlet grafana_servlet = {
    .name		= "grafana",
    .setup 		= grafana_servlet_setup,
    .on_url		= grafana_request_url,
    .on_headers		= grafana_request_headers,
    .on_body		= grafana_request_body,
    .on_done		= grafana_request_done,
};
