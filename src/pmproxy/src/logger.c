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
	fprintf(stderr, "%s: arg=" PRINTF_P_PFX "%p archive=%d\n", __FUNCTION__, arg, archive);

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
	fprintf(stderr, "%s: arg=" PRINTF_P_PFX "%p status=%d\n", __FUNCTION__, arg, status);

    if (status >= 0) {
	code = HTTP_STATUS_OK;
	body = pmlogger_success;
    } else if (client->u.http.parser.status_code) {
	code = client->u.http.parser.status_code;
	body = pmlogger_failure;
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

static int pmlogger_authenticate;
static sds pmlogger_auth_host;		/* pmcd host used to validate credentials */

/*
 * Validate HTTP Basic credentials by authenticating them against pmcd, reusing
 * PCP's SASL machinery.  pmproxy holds no password store of its own; exactly as
 * the pmseries servlet does, the supplied username/password are injected as
 * connection attributes (which set PM_CTXFLAG_AUTH) on a pmcd connection whose
 * SASL exchange is the authority.  We open a short-lived host context purely to
 * authenticate, then discard it.  The pmcd host is the [pmlogger] auth_host
 * config (default "localhost"); a push user must be a valid pmcd SASL user on
 * that host.
 *
 * pmcd defers validating the SASL credentials until the first metadata exchange
 * on the connection, so pmNewContext() alone succeeds even for a bad password;
 * we force the check with a context-labels fetch (the same request whose failure
 * surfaces a rejected credential in qa/1388) and use its result as the verdict.
 *
 * This runs synchronously on the event loop, matching the logger servlet's
 * design (its archive writes are likewise synchronous - see loggroup.c) and the
 * localhost default keeps the round-trip cheap.  The caller caches a successful
 * result on the connection, so a multi-POST push authenticates just once rather
 * than once per record.
 *
 * Returns 0 when the credentials authenticate, or a negative error code (which
 * pmlogger_authenticate_status classifies as denial vs. pmcd unreachable).
 */
static int
pmlogger_authenticate_user(const sds username, const sds password)
{
    __pmHashCtl		attrs;
    __pmHostSpec	*hosts = NULL;
    pmLabelSet		*labels = NULL;
    char		buf[512], *msg = NULL;
    int			sts, numhosts = 0, ctx, saved;

    __pmHashInit(&attrs);
    if ((sts = __pmParseHostAttrsSpec(pmlogger_auth_host,
				&hosts, &numhosts, &attrs, &msg)) < 0) {
	if (msg)
	    free(msg);
	return sts;
    }
    __pmHashAdd(PCP_ATTR_USERNAME, strdup(username), &attrs);
    __pmHashAdd(PCP_ATTR_PASSWORD, strdup(password), &attrs);
    sts = __pmUnparseHostAttrsSpec(hosts, numhosts, &attrs, buf, sizeof(buf));
    __pmFreeHostAttrsSpec(hosts, numhosts, &attrs);
    __pmHashClear(&attrs);
    if (sts < 0)
	return sts;

    saved = pmWhichContext();		/* preserve any caller context */
    if ((ctx = pmNewContext(PM_CONTEXT_HOST, buf)) < 0) {
	sts = ctx;			/* connection to pmcd failed */
    } else {
	/* force the deferred SASL credential check via a metadata request */
	if ((sts = pmGetContextLabels(&labels)) >= 0) {
	    pmFreeLabelSets(labels, sts);
	    sts = 0;			/* credentials accepted */
	}
	pmDestroyContext(ctx);
    }
    if (saved >= 0)
	pmUseContext(saved);
    return sts;
}

/*
 * Map a pmlogger_authenticate_user() failure onto the HTTP status we return.
 * Transport-level failures mean the authenticating pmcd could not be reached,
 * which is a server-side problem the client should retry later (503); anything
 * else is treated as the credentials being rejected (403).  Ambiguous errors
 * fall through to "rejected" so we fail closed and never grant access on doubt.
 */
static http_code_t
pmlogger_authenticate_status(int sts)
{
    switch (sts) {
    case -ECONNREFUSED:
    case -ECONNRESET:
    case -EHOSTDOWN:
    case -EHOSTUNREACH:
    case -ENETUNREACH:
    case -ETIMEDOUT:
    case PM_ERR_TIMEOUT:
    case PM_ERR_IPC:
    case PM_ERR_CONNLIMIT:
	return HTTP_STATUS_SERVICE_UNAVAILABLE;
    default:
	return HTTP_STATUS_FORBIDDEN;
    }
}

/*
 * Enforce [pmlogger] authenticate on an incoming request.  Credentials arrive
 * per-request in the Basic Auth header, but a push reuses one keep-alive
 * connection for many POSTs, so a validated (user, secret) pair is cached on
 * the connection (client->u.http.auth_*, which survive the per-request reset in
 * http_client_release) and only revalidated when the presented credentials
 * differ.  Sets client->u.http.parser.status_code and returns non-zero when the
 * request must be refused.
 */
static int
pmlogger_check_credentials(struct client *client)
{
    sds			username = client->u.http.username;
    sds			password = client->u.http.password;
    int			sts;

    if (username == NULL || password == NULL) {
	/* missing credentials - match the servlet's existing 403 (and the
	 * generic PM_SERVER_FEATURE_CREDS_REQD path in http.c) */
	client->u.http.parser.status_code = HTTP_STATUS_FORBIDDEN;
	return 1;
    }
    if (client->u.http.auth_userid != NULL &&
	sdscmp(client->u.http.auth_userid, username) == 0 &&
	sdscmp(client->u.http.auth_secret, password) == 0)
	return 0;		/* already validated on this connection */

    if ((sts = pmlogger_authenticate_user(username, password)) < 0) {
	if (client->u.http.auth_userid) {
	    sdsfree(client->u.http.auth_userid);
	    client->u.http.auth_userid = NULL;
	}
	if (client->u.http.auth_secret) {
	    sdsfree(client->u.http.auth_secret);
	    client->u.http.auth_secret = NULL;
	}
	client->u.http.parser.status_code = pmlogger_authenticate_status(sts);
	return 1;
    }

    /* cache the validated credentials for the rest of the connection */
    if (client->u.http.auth_userid)
	sdsfree(client->u.http.auth_userid);
    if (client->u.http.auth_secret)
	sdsfree(client->u.http.auth_secret);
    client->u.http.auth_userid = sdsdup(username);
    client->u.http.auth_secret = sdsdup(password);
    return 0;
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
	fprintf(stderr, "%s: " PRINTF_P_PFX "%p for client " PRINTF_P_PFX "%p\n", "pmlogger_data_release",
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
	client->u.http.parser.status_code = 0;
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
	fprintf(stderr, "logger servlet headers (client=" PRINTF_P_PFX "%p)\n", client);
    if (pmlogger_authenticate)
	pmlogger_check_credentials(client);
    return 0;
}

static int
pmlogger_request_body(struct client *client, const char *content, size_t length)
{
    pmLoggerBaton	*baton = (pmLoggerBaton *)client->u.http.data;
    size_t		bytes;

    if (pmDebugOptions.http)
	fprintf(stderr, "%s: logger servlet body (client=" PRINTF_P_PFX "%p,length=%zu)\n",
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
    mmv_registry_t	*logpaths = proxymetrics(proxy, METRICS_LOGPATHS);
    sds			value;

    if ((value = pmIniFileLookup(proxy->config, "pmlogger", "authenticate"))
	&& strcmp(value, "true") == 0)
	pmlogger_authenticate = 1;

    if ((value = pmIniFileLookup(proxy->config, "pmlogger", "auth_host")))
	pmlogger_auth_host = sdsdup(value);
    else
	pmlogger_auth_host = sdsnew("localhost");

    PARAM_CLIENT = sdsnew("client");

    pmlogger_settings.module.discover = get_keys_module(proxy);

    pmLogGroupSetup(&pmlogger_settings.module);
    pmLogGroupSetEventLoop(&pmlogger_settings.module, proxy->events);
    pmLogGroupSetConfiguration(&pmlogger_settings.module, proxy->config);
    pmLogGroupSetMetricRegistry(&pmlogger_settings.module, registry);
    pmLogPathsSetMetricRegistry(&pmlogger_settings.module, logpaths);
}

static void
pmlogger_servlet_reset(struct proxy *proxy)
{
    pmLogPathsReset(&pmlogger_settings.module);
}

static void
pmlogger_servlet_close(struct proxy *proxy)
{
    pmLogGroupClose(&pmlogger_settings.module);
    proxymetrics_close(proxy, METRICS_LOGGROUP);
    proxymetrics_close(proxy, METRICS_LOGPATHS);

    sdsfree(PARAM_CLIENT);
    if (pmlogger_auth_host) {
	sdsfree(pmlogger_auth_host);
	pmlogger_auth_host = NULL;
    }
}

struct servlet pmlogger_servlet = {
    .name		= "logger",
    .setup 		= pmlogger_servlet_setup,
    .reset 		= pmlogger_servlet_reset,
    .close 		= pmlogger_servlet_close,
    .on_url		= pmlogger_request_url,
    .on_headers		= pmlogger_request_headers,
    .on_body		= pmlogger_request_body,
    .on_done		= pmlogger_request_done,
    .on_release		= pmlogger_data_release,
};
