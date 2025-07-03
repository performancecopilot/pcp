/*
 * Copyright (c) 2025 Red Hat.
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
#include <limits.h>

typedef enum pmLoggerRestKey {
    RESTKEY_LABEL = 1,	/* archive log header */
    RESTKEY_META,	/* metadata records */
    RESTKEY_INDEX,	/* temporal index */
    RESTKEY_VOLUME,	/* data volumes */
    RESTKEY_PING,	/* live check */
} pmLoggerRestKey;

typedef struct pmLoggerRestCommand {
    const char		*name;
    unsigned int	namelen : 16;
    unsigned int	options : 16;
    pmLoggerRestKey	key;
} pmLoggerRestCommand;

typedef struct pmLoggerBaton {
    struct client	*client;
    sds			clientid;
    pmLoggerRestKey	restkey;
    unsigned int 	options;
    unsigned int 	volume;
    unsigned int 	logger;
    sds			body;
} pmLoggerBaton;

static pmLoggerRestCommand commands[] = {
    { .key = RESTKEY_LABEL, .options = HTTP_OPTIONS_POST,
	    .name = "label", .namelen = sizeof("label")-1 },
    { .key = RESTKEY_META, .options = HTTP_OPTIONS_POST,
	    .name = "meta", .namelen = sizeof("meta")-1 },
    { .key = RESTKEY_INDEX, .options = HTTP_OPTIONS_POST,
	    .name = "index", .namelen = sizeof("index")-1 },
    { .key = RESTKEY_VOLUME, .options = HTTP_OPTIONS_POST,
	    .name = "volume", .namelen = sizeof("volume")-1 },
    { .key = RESTKEY_PING, .options = HTTP_OPTIONS_GET,
	    .name = "ping", .namelen = sizeof("ping")-1 },
    { .name = NULL }	/* sentinel */
};

#define MAX_BODY_LENGTH	(1024 * 1024 * 64) /* 64Mb */

/* constant string keys (initialized during servlet setup) */
static sds PARAM_CLIENT;

/* constant global strings (read-only) */
static const char pmlogger_success[] = "\"success\":true";
static const char pmlogger_failure[] = "\"success\":false";

static void
on_pmlogger_archive(int archive, void *arg)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)arg;
    struct client	*client = baton->client;
    http_options_t	options = baton->options;
    http_flags_t	flags = client->u.http.flags | HTTP_FLAG_JSON;
    sds			result;

    if (pmDebugOptions.http || pmDebugOptions.log)
	fprintf(stderr, "%s: arg=%p archive=%d\n", __FUNCTION__, arg, archive);

    baton->logger = (unsigned int)archive;

    result = http_get_buffer(client);
    result = sdscatfmt(result, "{\"archive\":%u", baton->logger);
    if (baton->clientid)
	result = sdscatfmt(result, ",\"client\":%S", baton->clientid);
    result = sdscatfmt(result, ",%s}\r\n", pmlogger_success);

    http_reply(client, result, HTTP_STATUS_OK, flags, options);

    /* release lock of pmlogger_request_done */
    client_put(client);
}

static void
on_pmlogger_done(int status, void *arg)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)arg;
    struct client	*client = baton->client;
    http_options_t	options = baton->options;
    http_flags_t	flags = client->u.http.flags | HTTP_FLAG_JSON;
    http_code_t		code;
    const char		*body;
    sds			msg;

    if (pmDebugOptions.http || pmDebugOptions.log)
	fprintf(stderr, "%s: arg=%p status=%d\n", __FUNCTION__, arg, status);

    if (status >= 0) {
	code = HTTP_STATUS_OK;
	body = pmlogger_success;
    } else {
	if (status == -EEXIST)
	    code = HTTP_STATUS_CONFLICT;
	else if (status == -EINVAL)
	    code = HTTP_STATUS_BAD_REQUEST;
	else if (status == PM_ERR_LABEL)
	    code = HTTP_STATUS_UNPROCESSABLE_ENTITY;
	else if (status == -ESRCH || status == -ENOTCONN)
	    code = HTTP_STATUS_GONE;
	else
	    code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	client->u.http.parser.status_code = code;
	body = pmlogger_failure;
    }
    msg = sdsnewlen("{", 1);
    if (baton->clientid)
	msg = sdscatfmt(msg, "\"client\":%S,", baton->clientid);
    msg = sdscatfmt(msg, "%s}\r\n", body);

    http_reply(client, msg, code, flags, options);

    /* release lock of pmlogger_request_done */
    client_put(client);
}

static void
on_pmlogger_info(pmLogLevel level, sds message, void *arg)
{
    pmLoggerBaton      *baton = (pmLoggerBaton *)arg;

    proxylog(level, message, baton->client->proxy);
}

static pmLogGroupSettings pmlogger_settings = {
    .callbacks.on_archive	= on_pmlogger_archive,
    .callbacks.on_done          = on_pmlogger_done,
    .module.on_info             = on_pmlogger_info,
};

static pmLoggerRestCommand *
pmlogger_lookup_rest_command(sds url)
{
    pmLoggerRestCommand	*cp;
    const char		*name;

    if (sdslen(url) >= (sizeof("/logger/") - 1) &&
	strncmp(url, "/logger/", sizeof("/logger/") - 1) == 0) {
	name = (const char *)url + sizeof("/logger/") - 1;
	for (cp = &commands[0]; cp->name; cp++) {
	    if (strncmp(cp->name, name, cp->namelen) == 0)
		return cp;
	}
    }
    return NULL;
}

static void
pmlogger_data_release(struct client *client)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)client->u.http.data;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: %p for client %p\n", "pmlogger_data_release",
			baton, client);

    sdsfree(baton->body);
    sdsfree(baton->clientid);
    memset(baton, 0, sizeof(*baton));
    free(baton);
}

static void
pmlogger_setup_request_parameters(struct client *client,
		pmLoggerBaton *baton, dict *parameters)
{
    enum http_method	method = client->u.http.parser.method;
    dictEntry		*entry;
    sds			logger;

    if (parameters) {
	/* allow all APIs to pass(-through) a 'client' parameter */
	if ((entry = dictFind(parameters, PARAM_CLIENT)) != NULL) {
	    logger = dictGetVal(entry);   /* leave sds value, dup'd below */
	    baton->clientid = sdscatrepr(sdsempty(), logger, sdslen(logger));
	}
    }

    switch (baton->restkey) {
    case RESTKEY_LABEL:
    case RESTKEY_META:
    case RESTKEY_INDEX:
    case RESTKEY_VOLUME:
	if (method != HTTP_POST)
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;

    case RESTKEY_PING:
	if (method != HTTP_GET)
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;

    default:
	client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	break;
    }
}

/*
 * Test if this is a pmlogger REST API command, and if so which one.
 * If this servlet is handling this URL, ensure space for state exists
 * and indicate acceptance for processing this URL via the return code.
 */
static int
pmlogger_request_url(struct client *client, sds url, dict *parameters)
{
    char		*identity;
    unsigned int	volume = 0, logger = 0;
    pmLoggerBaton	*baton;
    pmLoggerRestCommand	*command;

    if ((command = pmlogger_lookup_rest_command(url)) == NULL)
	return 0;

    identity = url + sizeof("/logger/") + command->namelen;

    switch (command->key) {
    case RESTKEY_VOLUME:
	/* extract the archive volume number to use and log identifier */
	if (sscanf(identity, "%u/%u", &volume, &logger) != 2 ||
	    volume > INT_MAX || logger == 0 || logger > INT_MAX) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    return 1;
	}
	break;
    case RESTKEY_META:
    case RESTKEY_INDEX:
	/* extract the log identifier only */
	if (sscanf(identity, "%u", &logger) != 1 ||
	    logger == 0 || logger > INT_MAX) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    return 1;
	}
	break;
    default:	/* RESTKEY_LABEL, RESTKEY_PING */
	break;
    }

    if ((baton = calloc(1, sizeof(*baton))) != NULL) {
	client->u.http.data = baton;
	baton->client = client;
	baton->volume = volume;
	baton->logger = logger;
	baton->restkey = command->key;
	baton->options = command->options;
	pmlogger_setup_request_parameters(client, baton, parameters);
    } else {
	client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
    }
    return 1;
}

static int
pmlogger_request_headers(struct client *client, struct dict *headers)
{
    if (pmDebugOptions.http)
	fprintf(stderr, "logger servlet headers (client=%p)\n", client);
    return 0;
}

static int
pmlogger_request_body(struct client *client, const char *content, size_t length)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)client->u.http.data;
    size_t		bytes;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: logger servlet body (client=%p,length=%zu)\n",
			__FUNCTION__, client, length);

    if (client->u.http.parser.status_code != 0)
	return 0;

    switch (baton->restkey) {
    case RESTKEY_LABEL:
    case RESTKEY_META:
    case RESTKEY_INDEX:
    case RESTKEY_VOLUME:
	if (client->u.http.parser.method != HTTP_POST)
	    return 0;
	if (length > MAX_BODY_LENGTH) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    return 0;
	}
	bytes = (baton->body ? sdslen(baton->body) : 0) + length;
	if (bytes > MAX_BODY_LENGTH) {
	    client->u.http.parser.status_code = HTTP_STATUS_BAD_REQUEST;
	    return 0;
	}
	if (baton->body == NULL)
	    baton->body = sdsnewlen(content, length);
	else
	    baton->body = sdscatlen(baton->body, content, length);
	break;

    case RESTKEY_PING:
	if (client->u.http.parser.method != HTTP_GET)
	    return 0;
	break;

    default:
	client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	return 1;
    }

    return 0;
}

static int
pmlogger_request_done(struct client *client)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)client->u.http.data;
    struct dict		*param = client->u.http.parameters;

    /* reference to prevent freeing while waiting for a reply callback */
    client_get(client);

    /* error state entered already, message body may not be present */
    if (client->u.http.parser.status_code) {
	on_pmlogger_done(PM_ERR_GENERIC, baton);
	return 0;
    }

    switch (baton->restkey) {
    case RESTKEY_LABEL:
	pmLogGroupLabel(&pmlogger_settings, baton->body, sdslen(baton->body),
			param, baton);
	break;

    case RESTKEY_META:
	pmLogGroupMeta(&pmlogger_settings, baton->logger, baton->body,
			sdslen(baton->body), param, baton);
	break;

    case RESTKEY_INDEX:
	pmLogGroupIndex(&pmlogger_settings, baton->logger, baton->body,
			sdslen(baton->body), param, baton);
	break;

    case RESTKEY_VOLUME:
	pmLogGroupVolume(&pmlogger_settings, baton->logger, baton->volume,
			baton->body, sdslen(baton->body), param, baton);
	break;

    case RESTKEY_PING:
	on_pmlogger_done(0, baton);
	break;

    default:
	client->u.http.parser.status_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
	on_pmlogger_done(PM_ERR_GENERIC, baton);
	return 0;
    }

    return 0;
}

static void
pmlogger_servlet_setup(struct proxy *proxy)
{
    mmv_registry_t	*registry = proxymetrics(proxy, METRICS_LOGGROUP);

    PARAM_CLIENT = sdsnew("client");

    pmlogger_settings.module.discover = get_keys_module(proxy);

    pmLogGroupSetup(&pmlogger_settings.module);
    pmLogGroupSetEventLoop(&pmlogger_settings.module, proxy->events);
    pmLogGroupSetConfiguration(&pmlogger_settings.module, proxy->config);
    pmLogGroupSetMetricRegistry(&pmlogger_settings.module, registry);
}

static void
pmlogger_servlet_close(struct proxy *proxy)
{
    pmLogGroupClose(&pmlogger_settings.module);
    proxymetrics_close(proxy, METRICS_LOGGROUP);

    sdsfree(PARAM_CLIENT);
}

struct servlet pmlogger_servlet = {
    .name		= "logger",
    .setup 		= pmlogger_servlet_setup,
    .close 		= pmlogger_servlet_close,
    .on_url		= pmlogger_request_url,
    .on_headers		= pmlogger_request_headers,
    .on_body		= pmlogger_request_body,
    .on_done		= pmlogger_request_done,
    .on_release		= pmlogger_data_release,
};
