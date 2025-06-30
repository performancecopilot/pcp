/*
 * Copyright (c) 2018-2021,2024-2025 Red Hat.
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
#include "discover.h"

#define KEY_SERVER_RECONNECT_INTERVAL 2

static int search_queries;
static int series_queries;
static int key_server_resp;
static int archive_discovery;
static int archive_push;

static pmDiscoverCallBacks key_server_series = {
    .on_source		= pmSeriesDiscoverSource,
    .on_closed		= pmSeriesDiscoverClosed,
    .on_labels		= pmSeriesDiscoverLabels,
    .on_metric		= pmSeriesDiscoverMetric,
    .on_values		= pmSeriesDiscoverValues,
    .on_indom		= pmSeriesDiscoverInDom,
    .on_text		= pmSeriesDiscoverText,
};

static pmDiscoverCallBacks key_server_search = {
    .on_metric		= pmSearchDiscoverMetric,
    .on_indom		= pmSearchDiscoverInDom,
    .on_text		= pmSearchDiscoverText,
};

static pmDiscoverSettings key_server_discover = {
    .module.on_info	= proxylog,
};

static sds
replyfmt(respReply *reply)
{
    sds			c, command = sdsempty();
    int			i;

    if (reply == NULL)
	return command;

    switch (reply->type) {
    case RESP_REPLY_STRING:
	command = sdscatfmt(command, "$%U\r\n", (uint64_t)reply->len);
	command = sdscatlen(command, reply->str, reply->len);
	return sdscatlen(command, "\r\n", 2);
    case RESP_REPLY_ARRAY:
    case RESP_REPLY_MAP:
    case RESP_REPLY_SET:
	c = sdsempty();
	for (i = 0; i < reply->elements; i++) {
	    sds e = replyfmt(reply->element[i]);
	    c = sdscatsds(c, e);
	    sdsfree(e);
	}
	if (reply->type == RESP_REPLY_ARRAY)
	    command = sdscatfmt(command, "*%U\r\n%S", (uint64_t)reply->elements, c);
	else if (reply->type == RESP_REPLY_MAP)
	    command = sdscatfmt(command, "%%%U\r\n%S", (uint64_t)reply->elements, c);
	else /* (reply->type == RESP_REPLY_SET) */
	    command = sdscatfmt(command, "~%U\r\n%S", (uint64_t)reply->elements, c);
	sdsfree(c);
	return command;
    case RESP_REPLY_INTEGER:
	return sdscatfmt(command, ":%I\r\n", reply->integer);
    case RESP_REPLY_DOUBLE:
	command = sdscatlen(command, ",", 1);
	command = sdscatlen(command, reply->str, reply->len);
	return sdscatlen(command, "\r\n", 2);
    case RESP_REPLY_STATUS:
	command = sdscatlen(command, "+", 1);
	command = sdscatlen(command, reply->str, reply->len);
	return sdscatlen(command, "\r\n", 2);
    case RESP_REPLY_ERROR:
	command = sdscatlen(command, "-", 1);
	command = sdscatlen(command, reply->str, reply->len);
	return sdscatlen(command, "\r\n", 2);
    case RESP_REPLY_BOOL:
	return sdscatfmt(command, "#%s\r\n", reply->integer ? "t" : "f");
    case RESP_REPLY_NIL:
	return sdscat(command, "$-1\r\n");
    default:
	break;
    }
    return command;
}

static void
on_key_server_reply(
	keyClusterAsyncContext *c, void *r, void *arg)
{
    struct client	*client = (struct client *)arg;
    respReply		*reply = r;

    (void)c;
    client_write(client, replyfmt(reply), NULL);
}

void
on_key_client_read(struct proxy *proxy, struct client *client,
		ssize_t nread, const uv_buf_t *buf)
{
    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p\n", "on_key_client_read", client);

    if (key_server_resp == 0 || proxy->keys_setup == 0 ||
	keySlotsProxyConnect(proxy->slots,
		proxylog, &client->u.keys.reader,
		buf->base, nread, on_key_server_reply, client) < 0) {
	client_close(client);
    }
}

void
on_key_client_write(struct client *client)
{
    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p\n", "on_key_client_write", client);
}

void
on_key_client_close(struct client *client)
{
    keySlotsProxyFree(client->u.keys.reader);
}

static void
on_key_server_connected(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    sds			message;

    message = sdsnew("Key server slots");
    if (key_server_resp)
	message = sdscat(message, ", command keys");
    if ((search_queries = pmSearchEnabled(proxy->slots)))
	message = sdscat(message, ", search");
    if (series_queries)
	message = sdscat(message, ", schema version");
    pmNotifyErr(LOG_INFO, "%s setup\n", message);
    sdsfree(message);

    /* Key server was already connected before */
    if (proxy->keys_setup == 1)
	return;

    if (series_queries) {
	if (search_queries)
	    key_server_series.next = &key_server_search;
	key_server_discover.callbacks = key_server_series;
    } else if (search_queries) {
	key_server_discover.callbacks = key_server_search;
    }

    if ((archive_discovery || archive_push) &&
	(series_queries || search_queries)) {
	mmv_registry_t	*registry = proxymetrics(proxy, METRICS_DISCOVER);

	pmDiscoverSetEventLoop(&key_server_discover.module, proxy->events);
	pmDiscoverSetConfiguration(&key_server_discover.module, proxy->config);
	pmDiscoverSetMetricRegistry(&key_server_discover.module, registry);
	pmDiscoverSetup(&key_server_discover.module, &key_server_discover.callbacks, proxy);
	pmDiscoverSetSlots(&key_server_discover.module, proxy->slots);
    }

    proxy->keys_setup = 1;
}

static keySlotsFlags
get_key_slots_flags()
{
    keySlotsFlags	flags = SLOTS_NONE;

    if (key_server_resp)
	flags |= SLOTS_KEYMAP;
    if (series_queries)
	flags |= SLOTS_VERSION;
    if (search_queries)
	flags |= SLOTS_SEARCH;

    return flags;
}

static void
key_server_reconnect_worker(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    static unsigned int	wait_sec = KEY_SERVER_RECONNECT_INTERVAL;

    /* wait X seconds, because this timer callback is called every second */
    if (wait_sec > 1) {
	wait_sec--;
	return;
    }
    wait_sec = KEY_SERVER_RECONNECT_INTERVAL;

    /*
     * skip if server is disabled or state is not SLOTS_DISCONNECTED
     */
    if (!proxy->slots || proxy->slots->state != SLOTS_DISCONNECTED)
	return;

    if (pmDebugOptions.desperate)
	proxylog(PMLOG_INFO, "Trying to connect to key server ...", arg);

    keySlotsFlags	flags = get_key_slots_flags();
    keySlotsReconnect(proxy->slots, flags, proxylog, on_key_server_connected,
			proxy, proxy->events, proxy);
}

/*
 * Attempt to establish a server connection straight away
 * which is achieved via a timer that expires immediately
 * during the startup process.
 */
void
setup_keys_module(struct proxy *proxy)
{
    sds			option;

    if ((option = pmIniFileLookup(config, "keys", "enabled")) &&
	(strcmp(option, "false") == 0))
	return;
    else if ((option = pmIniFileLookup(config, "redis", "enabled")) &&
	(strcmp(option, "false") == 0))
	return;

    if ((option = pmIniFileLookup(config, "pmproxy", "resp.enabled")))
	key_server_resp = (strcmp(option, "true") == 0);
    else if ((option = pmIniFileLookup(config, "pmproxy", "redis.enabled")))
	key_server_resp = (strcmp(option, "true") == 0);
    if ((option = pmIniFileLookup(config, "pmseries", "enabled")))
	series_queries = (strcmp(option, "true") == 0);
    if ((option = pmIniFileLookup(config, "pmsearch", "enabled")))
	search_queries = (strcmp(option, "true") == 0);
    if ((option = pmIniFileLookup(config, "discover", "enabled")))
	archive_discovery = (strcmp(option, "true") == 0);
    if ((option = pmIniFileLookup(config, "pmlogger", "enabled")))
	archive_push = (strcmp(option, "true") == 0);

    if (proxy->slots == NULL &&
	(key_server_resp || series_queries || search_queries ||
	 archive_discovery || archive_push)) {
	mmv_registry_t	*registry = proxymetrics(proxy, METRICS_KEYS);
	keySlotsFlags	flags = get_key_slots_flags();

	proxy->slots = keySlotsConnect(proxy->config,
			flags, proxylog, on_key_server_connected,
			proxy, proxy->events, proxy);
	keySlotsSetMetricRegistry(proxy->slots, registry);
	keySlotsSetupMetrics(proxy->slots);
	pmWebTimerRegister(key_server_reconnect_worker, proxy);
    }
}

void *
get_keys_module(struct proxy *proxy)
{
    if (proxy->slots == NULL)
	setup_keys_module(proxy);
    return &key_server_discover.module;
}

void
close_keys_module(struct proxy *proxy)
{
    if (proxy->slots) {
	keySlotsFree(proxy->slots);
	proxy->slots = NULL;
    }

    if (archive_discovery || archive_push)
	pmDiscoverClose(&key_server_discover.module);

    proxymetrics_close(proxy, METRICS_KEYS);
    proxymetrics_close(proxy, METRICS_DISCOVER);
}
