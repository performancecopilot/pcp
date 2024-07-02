/*
 * Copyright (c) 2019-2021 Red Hat.
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
#include <assert.h>
#include <ctype.h>
#include "openmetrics.h"
#include "server.h"
#include "util.h"

typedef enum pmWebRestKey {
    RESTKEY_CONTEXT	= 1,
    RESTKEY_METRIC,
    RESTKEY_FETCH,
    RESTKEY_INDOM,
    RESTKEY_PROFILE,
    RESTKEY_CHILD,
    RESTKEY_STORE,
    RESTKEY_DERIVE,
    RESTKEY_SCRAPE,
} pmWebRestKey;

typedef struct pmWebRestCommand {
    const char		*name;
    unsigned int	namelen : 16;
    unsigned int	options : 16;
    pmWebRestKey	key;
} pmWebRestCommand;

typedef struct pmWebGroupBaton {
    struct client	*client;
    pmWebRestKey	restkey;
    sds			context;
    dict		*labels;
    sds			suffix;		/* response trailer (stack) */
    sds			clientid;	/* user-supplied identifier */
    sds			username;	/* from basic auth header */
    sds			password;	/* from basic auth header */
    unsigned int	times : 1;
    unsigned int	compat : 1;
    unsigned int	options : 16;
    unsigned int	numpmids;
    unsigned int	numvsets;
    unsigned int	numinsts;
    unsigned int	numindoms;
    sds			name;		/* metric currently being processed */
    pmID		pmid;		/* metric currently being processed */
    pmInDom		indom;		/* indom currently being processed */
} pmWebGroupBaton;

static pmWebRestCommand commands[] = {
    { .key = RESTKEY_CONTEXT, .options = HTTP_OPTIONS_GET,
	    .name = "context", .namelen = sizeof("context")-1 },
    { .key = RESTKEY_PROFILE, .options = HTTP_OPTIONS_GET,
	    .name = "profile", .namelen = sizeof("profile")-1 },
    { .key = RESTKEY_SCRAPE, .options = HTTP_OPTIONS_GET,
	    .name = "metrics", .namelen = sizeof("metrics")-1 },
    { .key = RESTKEY_METRIC, .options = HTTP_OPTIONS_GET,
	    .name = "metric", .namelen = sizeof("metric")-1 },
    { .key = RESTKEY_DERIVE, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "derive", .namelen = sizeof("derive")-1 },
    { .key = RESTKEY_FETCH, .options = HTTP_OPTIONS_GET,
	    .name = "fetch", .namelen = sizeof("fetch")-1 },
    { .key = RESTKEY_INDOM, .options = HTTP_OPTIONS_GET,
	    .name = "indom", .namelen = sizeof("indom")-1 },
    { .key = RESTKEY_STORE, .options = HTTP_OPTIONS_GET | HTTP_OPTIONS_POST,
	    .name = "store", .namelen = sizeof("store")-1 },
    { .key = RESTKEY_CHILD, .options = HTTP_OPTIONS_GET,
	    .name = "children", .namelen = sizeof("children")-1 },
    { .name = NULL }	/* sentinel */
};

static pmWebRestCommand openmetrics[] = {
    { .key = RESTKEY_SCRAPE, .options = HTTP_OPTIONS_GET,
	    .name = "/metrics", .namelen = sizeof("/metrics")-1 },
    { .name = NULL }	/* sentinel */
};

static sds PARAM_NAMES, PARAM_NAME, PARAM_PMIDS, PARAM_PMID,
	   PARAM_INDOM, PARAM_EXPR, PARAM_VALUE, PARAM_TIMES,
	   PARAM_CONTEXT, PARAM_CLIENT;


static pmWebRestCommand *
pmwebapi_lookup_rest_command(sds url, unsigned int *compat, sds *context)
{
    pmWebRestCommand	*cp;
    const char		*name, *ctxid = NULL;

    if (sdslen(url) >= (sizeof("/pmapi/") - 1) &&
	strncmp(url, "/pmapi/", sizeof("/pmapi/") - 1) == 0) {
	name = (const char *)url + sizeof("/pmapi/") - 1;
	/* extract (optional) context identifier */
	if (isdigit((int)(*name))) {
	    ctxid = name;
	    do {
		name++;
	    } while (isdigit((int)(*name)));
	    if (*name++ != '/')
		return NULL;
	    *context = sdsnewlen(ctxid, name - ctxid - 1);
	}
	if (*name == '_') {
	    name++;		/* skip underscore designating */
	    *compat = 1;	/* backward-compatibility mode */
	}
	for (cp = &commands[0]; cp->name; cp++)
	    if (strncmp(cp->name, name, cp->namelen) == 0)
		return cp;
    }
    for (cp = &openmetrics[0]; cp->name; cp++)
	if (strncmp(cp->name, url, cp->namelen) == 0)
	    return cp;
    return NULL;
}

static void
pmwebapi_data_release(struct client *client)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)client->u.http.data;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: baton %p for client %p\n", "pmwebapi_data_release",
			baton, client);

    sdsfree(baton->name);
    sdsfree(baton->suffix);
    sdsfree(baton->context);
    sdsfree(baton->clientid);
    if (baton->labels)
	dictRelease(baton->labels);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static void
pmwebapi_set_context(pmWebGroupBaton *baton, sds context)
{
    if (baton->context == NULL) {
	baton->context = sdsdup(context);
    } else if (sdscmp(baton->context, context) != 0) {
	sdsfree(baton->context);
	baton->context = sdsdup(context);
    }
}

static void
on_pmwebapi_context(sds context, pmWebSource *source, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    struct client	*client = baton->client;
    sds			result;

    pmwebapi_set_context(baton, context);

    result = http_get_buffer(client);
    result = sdscatfmt(result, "{\"context\":%S", context);
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
    if (baton->compat == 0) {
	if (baton->clientid)
	    result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
	result = sdscatfmt(result,
			",\"source\":\"%S\",\"hostspec\":\"%S\",\"labels\":",
			source->source, source->hostspec);
	if (source->labels)
	    result = sdscatsds(result, source->labels);
	else
	    result = sdscatlen(result, "{}", 2);
    }
    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
}

static void
on_pmwebapi_metric(sds context, pmWebMetric *metric, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    struct client	*client = baton->client;
    char		pmidstr[20], indomstr[20];
    sds			quoted, result = http_get_buffer(client);
    int			first = (baton->numpmids == 0);

    pmwebapi_set_context(baton, context);

    if (first) {
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
	baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);
	result = sdscatfmt(result, "{\"context\":%S", context);
	if (baton->clientid)
	    result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
	result = sdscatlen(result, ",\"metrics\":[", 12);
    } else { /* next metric */
	result = sdscatlen(result, ",", 1);
    }

    baton->numpmids++;

    if (metric->pmid == PM_ID_NULL) {
	quoted = sdscatrepr(sdsempty(), metric->name, sdslen(metric->name));
	result = sdscatfmt(result,
			"{\"name\":%S,\"message\":\"%S\",\"success\":false}",
			quoted, metric->oneline);
	sdsfree(quoted);
	goto transfer;
    }

    result = sdscatfmt(result, "{\"name\":\"%S\",\"series\":\"%S\"",
			metric->name, metric->series);
    if (baton->compat == 0) {
	pmIDStr_r(metric->pmid, pmidstr, sizeof(pmidstr));
	result = sdscatfmt(result, ",\"pmid\":\"%s\"", pmidstr);
    } else {
	result = sdscatfmt(result, ",\"pmid\":%u", metric->pmid);
    }
    if (metric->indom != PM_INDOM_NULL) {
	if (baton->compat == 0) {
	    pmInDomStr_r(metric->indom, indomstr, sizeof(indomstr));
	    result = sdscatfmt(result, ",\"indom\":\"%s\"", indomstr);
	} else {
	    result = sdscatfmt(result, ",\"indom\":%u", metric->indom);
	}
    }
    result = sdscatfmt(result,
		",\"type\":\"%s\",\"sem\":\"%s\",\"units\":\"%s\",\"labels\":",
			metric->type, metric->sem, metric->units);
    if (metric->labels)
	result = sdscatsds(result, metric->labels);
    else
	result = sdscatlen(result, "{}", 2);
    if (metric->oneline &&
		(quoted = json_string(metric->oneline)) != NULL) {
	result = sdscatfmt(result, ",\"text-oneline\":%S", quoted);
	sdsfree(quoted);
    }
    quoted = (metric->helptext && metric->helptext[0] != '\0') ?
		metric->helptext : metric->oneline;
    if (quoted &&
		(quoted = json_string(quoted)) != NULL) {
	result = sdscatfmt(result, ",\"text-help\":%S", quoted);
	sdsfree(quoted);
    }
    result = sdscatlen(result, "}", 1);

transfer:
    http_set_buffer(client, result, HTTP_FLAG_JSON);
    http_transfer(client);
}

static int
on_pmwebapi_fetch(sds context, pmWebResult *fetch, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    sds			result = http_get_buffer(baton->client);

    pmwebapi_set_context(baton, context);

    baton->numvsets = baton->numinsts = 0;
    if (baton->compat == 0) {
	result = sdscatfmt(result, "{\"context\":%S", context);
	if (baton->clientid)
	    result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
	result = sdscatfmt(result, ",\"timestamp\":%I.%I,",
			fetch->seconds, fetch->nanoseconds);
    } else {
	result = sdscatfmt(result, "{\"timestamp\":{\"s\":%I,\"us\":%I},",
			fetch->seconds, fetch->nanoseconds / 1000);
    }
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
    result = sdscatfmt(result, "\"values\":[");
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

static int
on_pmwebapi_fetch_values(sds context, pmWebValueSet *valueset, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    sds			s, result = http_get_buffer(baton->client);
    char		pmidstr[20];

    pmwebapi_set_context(baton, context);

    /* pmID insufficient to determine uniqueness, use metric name too */
    if (baton->name == NULL)
	baton->name = sdsempty();

    s = baton->name;
    if (valueset->pmid != baton->pmid || sdscmp(valueset->name, s) != 0) {
	sdsclear(s);	/* new metric */
	baton->name = sdscpylen(s, valueset->name, sdslen(valueset->name));
	baton->pmid = valueset->pmid;
	if (baton->numvsets > 0) {
	    result = sdscatlen(result, "]},", 3);
	    baton->suffix = json_pop_suffix(baton->suffix);	/* '[' */
	    baton->suffix = json_pop_suffix(baton->suffix); /* '{' */
	}
	baton->numvsets = 0;
	baton->numinsts = 0;
    } else if (baton->numvsets != 0) {
	result = sdscatlen(result, ",", 1);
    }
    baton->numvsets++;

    if (baton->compat == 0) {
	pmIDStr_r(valueset->pmid, pmidstr, sizeof(pmidstr));
	result = sdscatfmt(result, "{\"pmid\":\"%s\"", pmidstr);
    } else {
	result = sdscatfmt(result, "{\"pmid\":%u", valueset->pmid);
    }
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
    result = sdscatfmt(result, ",\"name\":\"%S\",\"instances\":[",
				valueset->name);
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

static int
on_pmwebapi_fetch_value(sds context, pmWebValue *value, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    sds			result = http_get_buffer(baton->client);

    assert(value->pmid == baton->pmid);
    pmwebapi_set_context(baton, context);

    if (baton->numinsts != 0)
	result = sdscatlen(result, ",", 1);
    baton->numinsts++;

    if (value->inst == PM_IN_NULL) {
	result = sdscatfmt(result, "{\"instance\":%s,\"value\":%s}",
				baton->compat ? "-1" : "null",
				sdslen(value->value) ? value->value : "null");
    } else {
	result = sdscatfmt(result, "{\"instance\":%u,\"value\":%s}",
				value->inst,
				sdslen(value->value) ? value->value : "null");
    }

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

static int
on_pmwebapi_indom(sds context, pmWebInDom *indom, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    char		indomstr[20];
    sds			quoted, result = http_get_buffer(baton->client);

    pmwebapi_set_context(baton, context);

    if (indom->indom != baton->indom) {	/* new indom */
	baton->indom = indom->indom;
	baton->numindoms = 0;
	baton->numinsts = 0;
    }

    if (baton->numindoms != 0)
	result = sdscatlen(result, ",", 1);
    baton->numindoms++;

    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
    if (baton->compat == 0) {
	pmInDomStr_r(indom->indom, indomstr, sizeof(indomstr));
	result = sdscatfmt(result, "{\"context\":%S", context);
	if (baton->clientid)
	    result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
	result = sdscatfmt(result, ",\"indom\":\"%s\"", indomstr);
    } else {
	result = sdscatfmt(result, "{\"indom\":%u", indom->indom);
    }
    result = sdscatfmt(result, ",\"labels\":");
    if (indom->labels)
	result = sdscatsds(result, indom->labels);
    else
	result = sdscatlen(result, "{}", 2);
    if (indom->oneline && (quoted = json_string(indom->oneline))) {
	result = sdscatfmt(result, ",\"text-oneline\":%S", quoted);
	sdsfree(quoted);
    }
    quoted = (indom->helptext && indom->helptext[0] != '\0') ?
		indom->helptext : indom->oneline;
    if (quoted && (quoted = json_string(quoted))) {
	result = sdscatfmt(result, ",\"text-help\":%S", quoted);
	sdsfree(quoted);
    }
    result = sdscatfmt(result, ",\"instances\":[");
    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_ARRAY);

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

static int
on_pmwebapi_instance(sds context, pmWebInstance *instance, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    sds			quoted, result = http_get_buffer(baton->client);

    assert(instance->indom == baton->indom);
    pmwebapi_set_context(baton, context);

    if (baton->numinsts != 0)
	result = sdscatlen(result, ",", 1);
    baton->numinsts++;

    quoted = json_string(instance->name);
    result = sdscatfmt(result, "{\"instance\":%u,\"name\":%S,\"labels\":",
			instance->inst, quoted);
    sdsfree(quoted);
    if (instance->labels)
	result = sdscatsds(result, instance->labels);
    else
	result = sdscatlen(result, "{}", 2);
    result = sdscatlen(result, "}", 1);

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

static int
on_pmwebapi_children(sds context, pmWebChildren *children, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    sds			result = http_get_buffer(baton->client);
    unsigned int	i;

    pmwebapi_set_context(baton, context);

    baton->suffix = json_push_suffix(baton->suffix, JSON_FLAG_OBJECT);
    result = sdscatfmt(result, "{\"context\":%S", context);
    if (baton->clientid)
	result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
    result = sdscatfmt(result, ",\"name\":\"%S\"", children->name);
    result = sdscatlen(result, ",\"leaf\":[", 9);
    for (i = 0; i < children->numleaf; i++)
	result = (i > 0) ? sdscatfmt(result, ",\"%S\"", children->leaf[i]):
			    sdscatfmt(result, "\"%S\"", children->leaf[i]);
    result = sdscatlen(result, "],\"nonleaf\":[", 13);
    for (i = 0; i < children->numnonleaf; i++)
	result = (i > 0) ? sdscatfmt(result, ",\"%S\"", children->nonleaf[i]):
			    sdscatfmt(result, "\"%S\"", children->nonleaf[i]);
    result = sdscatlen(result, "]", 1);

    http_set_buffer(baton->client, result, HTTP_FLAG_JSON);
    http_transfer(baton->client);
    return 0;
}

/*
 * https://openmetrics.io/
 *
 * metric_name [
 *    "{" label_name "=" `"` label_value `"` { "," label_name "=" `"` label_value `"` } [ "," ] "}"
 * ] value [ timestamp ]
 */
static int
on_pmwebapi_scrape(sds context, pmWebScrape *scrape, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    pmWebInstance	*instance = &scrape->instance;
    pmWebMetric		*metric = &scrape->metric;
    pmWebValue		*value = &scrape->value;
    long long		milliseconds;
    char		pmidstr[20], indomstr[20];
    sds			name = NULL, semantics = NULL, labels = NULL;
    sds			s, result;

    pmwebapi_set_context(baton, context);
    if (open_metrics_type_check(metric->type) < 0)
	return 0;

    result = http_get_buffer(baton->client);
    name = open_metrics_name(metric->name, baton->compat);

    if (baton->name == NULL)
	baton->name = sdsempty();

    s = baton->name;
    if (metric->pmid != baton->pmid || sdscmp(metric->name, s) != 0) {
	sdsclear(s);	/* new metric */
	baton->name = sdscpylen(s, metric->name, sdslen(metric->name));
	baton->pmid = metric->pmid;
    } else {    
	goto value;	/* metric header already done */
    }

    if (baton->compat == 0) {	/* include pmid, indom and type */
	pmIDStr_r(metric->pmid, pmidstr, sizeof(pmidstr));
	pmInDomStr_r(metric->indom, indomstr, sizeof(indomstr));
	result = sdscatfmt(result, "# PCP5 %S %s %S %s %S %S\n",
			metric->name, pmidstr, metric->type,
			indomstr, metric->sem, metric->units);
    } else {
	result = sdscatfmt(result, "# PCP %S %S %S\n",
			metric->name, metric->sem, metric->units);
    }

    if (metric->oneline)
	result = sdscatfmt(result, "# HELP %S %S\n", name, metric->oneline);
    semantics = open_metrics_semantics(metric->sem);
    result = sdscatfmt(result, "# TYPE %S %S\n", name, semantics);

value:
    if (metric->indom != PM_INDOM_NULL)
	labels = instance->labels;
    if (labels == NULL)
	labels = metric->labels;
    if (labels)
	result = sdscatfmt(result, "%S{%S}", name, labels);
    else /* no labels */
	result = sdscatsds(result, name);

    /* append the value */
    result = sdscatfmt(result, " %S", value->value);

    if (baton->times) {
	/* append the timestamp string */
	milliseconds = (scrape->seconds * 1000) + (scrape->nanoseconds / 1000);
	result = sdscatfmt(result, " %I\n", milliseconds);
    } else {
	result = sdscatfmt(result, "\n");
    }

    sdsfree(semantics);
    sdsfree(name);

    http_set_buffer(baton->client, result, HTTP_FLAG_TEXT);
    http_transfer(baton->client);
    return 0;
}

/*
 * Given an array of labelset pointers produce Open Metrics format labels.
 * The labelset structure provided contains a pre-allocated result buffer.
 *
 * This function is comparable to pmMergeLabelSets(3), however it produces
 * labels in Open Metrics form instead of the native PCP JSONB style.
 */
static void
on_pmwebapi_scrape_labels(sds context, pmWebLabelSet *labelset, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    struct client	*client = (struct client *)baton->client;

    if (pmDebugOptions.labels || pmDebugOptions.series) {
	fprintf(stderr, "%s: client=%p (ctx=%s)\n",
		"on_pmwebapi_scrape_labels", client, context);
    }
    if (baton->labels == NULL)
	baton->labels = dictCreate(&sdsOwnDictCallBacks, NULL);
    open_metrics_labels(labelset, baton->labels);
    dictRelease(baton->labels);	/* reset for next caller */
    baton->labels = NULL;
}

static int
on_pmwebapi_check(sds context, pmWebAccess *access,
		int *status, sds *message, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    struct client	*client = (struct client *)baton->client;

    if (pmDebugOptions.auth || pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p (ctx=%s) user=%s pass=*** realm=%s\n",
		"on_pmwebapi_check", client, context,
		access->username, access->realm);

    /* Does this context require username/password authentication? */
    if (access->username != NULL ||
		__pmServerHasFeature(PM_SERVER_FEATURE_CREDS_REQD)) {
	if (access->username == NULL || access->password == NULL ||
		client->u.http.username == NULL || client->u.http.password == NULL) {
	    *message = sdsnew("authentication required");
	    *status = -EAGAIN;
	    return -1;
	}
	if (sdscmp(access->username, client->u.http.username) != 0 ||
		sdscmp(access->password, client->u.http.password) != 0) {
	    *message = sdsnew("authentication failed");
	    *status = -EPERM;
	    return -1;
	}
    }

    return 0;
}

static void
on_pmwebapi_done(sds context, int status, sds message, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;
    struct client	*client = (struct client *)baton->client;
    http_options_t	options = baton->options;
    http_flags_t	flags = client->u.http.flags;
    http_code_t		code;
    sds			quoted, msg;

    if (pmDebugOptions.series)
	fprintf(stderr, "%s: client=%p (sts=%d,msg=%s)\n", "on_pmwebapi_done",
			client, status, message ? message : "");

    if (status == 0) {
	code = HTTP_STATUS_OK;
	/* complete current response with JSON suffix if needed */
	if ((msg = baton->suffix) == NULL) {	/* empty OK response */
	    if (flags & HTTP_FLAG_NO_BODY) {
		msg = sdsempty();
	    } else if (flags & HTTP_FLAG_JSON) {
		msg = sdsnewlen("{", 1);
		if (context)
		    msg = sdscatfmt(msg, "\"context\":%S,", context);
		msg = sdscat(msg, "\"success\":true}\r\n");
	    } else {
		msg = sdsnewlen("# EOF", 5);
	    }
	}
	baton->suffix = NULL;
    } else {
	flags |= HTTP_FLAG_JSON;	/* all errors in JSON */
	if (((code = client->u.http.parser.status_code)) == 0) {
	    if (status == -EPERM)
		code = HTTP_STATUS_FORBIDDEN;
	    else if (status == -EAGAIN)
		code = HTTP_STATUS_UNAUTHORIZED;
	    else
		code = HTTP_STATUS_BAD_REQUEST;
	}
	if (message)
	    quoted = json_string(message);
	else
	    quoted = sdsnew("\"(none)\"");
	msg = sdsnewlen("{", 1);
	if (context)
	    msg = sdscatfmt(msg, "\"context\":%S,", context);
	msg = sdscatfmt(msg, "\"message\":%S,", quoted);
	msg = sdscat(msg, "\"success\":false}\r\n");
	sdsfree(quoted);
    }

    http_reply(client, msg, code, flags, options);
    client_put(client);
}

static void
on_pmwebapi_info(pmLogLevel level, sds message, void *arg)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)arg;

    proxylog(level, message, baton->client->proxy);
}

static pmWebGroupSettings pmwebapi_settings = {
    .callbacks.on_context	= on_pmwebapi_context,
    .callbacks.on_metric	= on_pmwebapi_metric,
    .callbacks.on_fetch		= on_pmwebapi_fetch,
    .callbacks.on_fetch_values	= on_pmwebapi_fetch_values,
    .callbacks.on_fetch_value	= on_pmwebapi_fetch_value,
    .callbacks.on_indom		= on_pmwebapi_indom,
    .callbacks.on_instance	= on_pmwebapi_instance,
    .callbacks.on_children	= on_pmwebapi_children,
    .callbacks.on_scrape	= on_pmwebapi_scrape,
    .callbacks.on_scrape_labels	= on_pmwebapi_scrape_labels,
    .callbacks.on_check		= on_pmwebapi_check,
    .callbacks.on_done		= on_pmwebapi_done,
    .module.on_info		= on_pmwebapi_info,
};

/*
 * Finish processing of individual request parameters, preparing
 * for (later) submission of the requested PMWEBAPI(3) command.
 */
static void
pmwebapi_setup_request_parameters(struct client *client,
		pmWebGroupBaton *baton, dict *parameters)
{
    dictEntry	*entry;
    sds		value;

    if (parameters) {
	/* allow all APIs to pass(-through) a 'client' parameter */
	if ((entry = dictFind(parameters, PARAM_CLIENT)) != NULL) {
	    value = dictGetVal(entry);   /* leave sds value, dup'd below */
	    baton->clientid = json_string(value);
	}
	/* allow all APIs to request specific context via params */
	if ((entry = dictFind(parameters, PARAM_CONTEXT)) != NULL)
	    pmwebapi_set_context(baton, dictGetVal(entry));
    }

    switch (baton->restkey) {
    case RESTKEY_CONTEXT:
    case RESTKEY_METRIC:
    case RESTKEY_CHILD:
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    case RESTKEY_SCRAPE:
	if (parameters && (entry = dictFind(parameters, PARAM_TIMES)))
	     baton->times = (strcmp(dictGetVal(entry), "true") == 0);
	client->u.http.flags |= HTTP_FLAG_TEXT;
	break;

    case RESTKEY_FETCH:
	if (parameters == NULL ||
	    (dictFind(parameters, PARAM_NAME) == NULL &&
	     dictFind(parameters, PARAM_NAMES) == NULL &&
	     dictFind(parameters, PARAM_PMID) == NULL &&
	     dictFind(parameters, PARAM_PMIDS) == NULL))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    case RESTKEY_INDOM:
	if (parameters == NULL ||
	    (dictFind(parameters, PARAM_INDOM) == NULL &&
	     dictFind(parameters, PARAM_NAME) == NULL))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    case RESTKEY_PROFILE:
	if (parameters == NULL ||
	    dictFind(parameters, PARAM_EXPR) == NULL)
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    case RESTKEY_STORE:
	/*
	 * Metric name always in parameters, value optionally there or
	 * via a POST (see pmwebapi_request_body() for further detail).
	 */
	if ((parameters == NULL) &&
	    (client->u.http.parser.method != HTTP_POST))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	else if (dictFind(parameters, PARAM_NAME) == NULL &&
		(dictFind(parameters, PARAM_PMID) == NULL))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	else if (client->u.http.parser.method != HTTP_POST &&
		(dictFind(parameters, PARAM_VALUE) == NULL))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    case RESTKEY_DERIVE:
	/*
	 * Expect metric name and an expression string OR configuration
	 * via a POST (see pmwebapi_request_body() for further detail).
	 */
	if (parameters == NULL && 
	    (client->u.http.parser.method != HTTP_POST))
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	else if (client->u.http.parser.method != HTTP_POST &&
	    dictFind(parameters, PARAM_NAME) == NULL &&
	    dictFind(parameters, PARAM_EXPR) == NULL)
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	client->u.http.flags |= HTTP_FLAG_JSON;
	break;

    default:
	client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;
    }
}

/*
 * Test if this is a pmwebapi REST request, and if so, which one.
 * If this servlet is handling this URL, ensure space for state exists
 * and indicate acceptance for processing this URL via the return code.
 */
static int
pmwebapi_request_url(struct client *client, sds url, dict *parameters)
{
    pmWebGroupBaton	*baton;
    pmWebRestCommand	*command;
    unsigned int	compat = 0;
    sds			context = NULL;

    if (!(command = pmwebapi_lookup_rest_command(url, &compat, &context))) {
	sdsfree(context);
	return 0;
    }

    if ((baton = calloc(1, sizeof(*baton))) != NULL) {
	client->u.http.data = baton;
	baton->client = client;
	baton->restkey = command->key;
	baton->options = command->options;
	baton->compat = compat;
	baton->context = context;
	pmwebapi_setup_request_parameters(client, baton, parameters);
    } else {
	client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	sdsfree(context);
    }
    return 1;
}

static int
pmwebapi_request_body(struct client *client, const char *content, size_t length)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)client->u.http.data;

    if (baton->restkey == RESTKEY_DERIVE &&
	client->u.http.parser.method == HTTP_POST) {
	if (client->u.http.parameters == NULL)
	    client->u.http.parameters = dictCreate(&sdsOwnDictCallBacks, NULL);
	dictAdd(client->u.http.parameters,
			sdsnewlen(PARAM_EXPR, sdslen(PARAM_EXPR)),
			sdsnewlen(content, length));
    }
    if (baton->restkey == RESTKEY_STORE &&
	client->u.http.parser.method == HTTP_POST) {
	if (client->u.http.parameters == NULL)
	    client->u.http.parameters = dictCreate(&sdsOwnDictCallBacks, NULL);
	dictAdd(client->u.http.parameters,
			sdsnewlen(PARAM_VALUE, sdslen(PARAM_VALUE)),
			sdsnewlen(content, length));
    }
    return 0;
}

static void
pmwebapi_fetch(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupFetch(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_indom(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupInDom(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_metric(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupMetric(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_children(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupChildren(&pmwebapi_settings, baton->context, params, baton);
}


static void
pmwebapi_store(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupStore(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_derive(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupDerive(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_profile(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupProfile(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_scrape(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;
    
    pmWebGroupScrape(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_context(uv_work_t *work)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)work->data;
    struct dict		*params = baton->client->u.http.parameters;

    pmWebGroupContext(&pmwebapi_settings, baton->context, params, baton);
}

static void
pmwebapi_work_done(uv_work_t *work, int status)
{
    (void)status;
    free(work);
}

static int
pmwebapi_request_done(struct client *client)
{
    pmWebGroupBaton	*baton = (pmWebGroupBaton *)client->u.http.data;
    uv_loop_t		*loop = client->proxy->events;
    uv_work_t		*work;

    /* take a reference on the client to prevent freeing races on close */
    client_get(client);

    if (client->u.http.parser.status_code) {
	on_pmwebapi_done(NULL, -EINVAL, NULL, baton);
	return 1;
    }

    if (client->u.http.parser.method == HTTP_OPTIONS ||
	client->u.http.parser.method == HTTP_TRACE ||
	client->u.http.parser.method == HTTP_HEAD) {
	on_pmwebapi_done(NULL, 0, NULL, baton);
	return 0;
    }

    if ((work = (uv_work_t *)calloc(1, sizeof(uv_work_t))) == NULL) {
	client_put(client);
	return 1;
    }
    work->data = baton;

    /* submit command request to worker thread */
    switch (baton->restkey) {
    case RESTKEY_CONTEXT:
	uv_queue_work(loop, work, pmwebapi_context, pmwebapi_work_done);
	break;
    case RESTKEY_PROFILE:
	uv_queue_work(loop, work, pmwebapi_profile, pmwebapi_work_done);
	break;
    case RESTKEY_METRIC:
	uv_queue_work(loop, work, pmwebapi_metric, pmwebapi_work_done);
	break;
    case RESTKEY_FETCH:
	uv_queue_work(loop, work, pmwebapi_fetch, pmwebapi_work_done);
	break;
    case RESTKEY_INDOM:
	uv_queue_work(loop, work, pmwebapi_indom, pmwebapi_work_done);
	break;
    case RESTKEY_CHILD:
	uv_queue_work(loop, work, pmwebapi_children, pmwebapi_work_done);
	break;
    case RESTKEY_STORE:
	uv_queue_work(loop, work, pmwebapi_store, pmwebapi_work_done);
	break;
    case RESTKEY_DERIVE:
	uv_queue_work(loop, work, pmwebapi_derive, pmwebapi_work_done);
	break;
    case RESTKEY_SCRAPE:
	uv_queue_work(loop, work, pmwebapi_scrape, pmwebapi_work_done);
	break;
    default:
	pmwebapi_work_done(work, -EINVAL);
	client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	on_pmwebapi_done(NULL, -EINVAL, NULL, baton);
	return 1;
    }
    return 0;
}

static void
pmwebapi_servlet_setup(struct proxy *proxy)
{
    mmv_registry_t	*metric_registry = proxymetrics(proxy, METRICS_WEBGROUP);

    PARAM_NAMES = sdsnew("names");
    PARAM_NAME = sdsnew("name");
    PARAM_PMIDS = sdsnew("pmids");
    PARAM_PMID = sdsnew("pmid");
    PARAM_INDOM = sdsnew("indom");
    PARAM_EXPR = sdsnew("expr");
    PARAM_VALUE = sdsnew("value");
    PARAM_TIMES = sdsnew("times");
    PARAM_CLIENT = sdsnew("client");
    PARAM_CONTEXT = sdsnew("context");

    pmWebGroupSetup(&pmwebapi_settings.module);
    pmWebGroupSetEventLoop(&pmwebapi_settings.module, proxy->events);
    pmWebGroupSetConfiguration(&pmwebapi_settings.module, proxy->config);
    pmWebGroupSetMetricRegistry(&pmwebapi_settings.module, metric_registry);
}

static void
pmwebapi_servlet_close(struct proxy *proxy)
{
    pmWebGroupClose(&pmwebapi_settings.module);
    proxymetrics_close(proxy, METRICS_WEBGROUP);

    sdsfree(PARAM_NAMES);
    sdsfree(PARAM_NAME);
    sdsfree(PARAM_PMIDS);
    sdsfree(PARAM_PMID);
    sdsfree(PARAM_INDOM);
    sdsfree(PARAM_EXPR);
    sdsfree(PARAM_VALUE);
    sdsfree(PARAM_TIMES);
    sdsfree(PARAM_CLIENT);
    sdsfree(PARAM_CONTEXT);
}

struct servlet pmwebapi_servlet = {
    .name		= "webapi",
    .setup		= pmwebapi_servlet_setup,
    .close		= pmwebapi_servlet_close,
    .on_url		= pmwebapi_request_url,
    .on_body		= pmwebapi_request_body,
    .on_done		= pmwebapi_request_done,
    .on_release		= pmwebapi_data_release,
};
