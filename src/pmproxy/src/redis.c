/*
 * Copyright (c) 2018-2019 Red Hat.
 * Copyright (c) 2018 Challa Venkata Naga Prajwal <cvnprajwal at gmail dot com>
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
 */
#include "server.h"
#include "discover.h"

static int series_queries;
static int redis_protocol;
static int archive_discovery;

static pmDiscoverSettings redis_discover = {
    .callbacks.on_source	= pmSeriesDiscoverSource,
    .callbacks.on_closed	= pmSeriesDiscoverClosed,
    .callbacks.on_labels	= pmSeriesDiscoverLabels,
    .callbacks.on_metric	= pmSeriesDiscoverMetric,
    .callbacks.on_values	= pmSeriesDiscoverValues,
    .callbacks.on_indom		= pmSeriesDiscoverInDom,
    .callbacks.on_text		= pmSeriesDiscoverText,
    .module.on_info		= proxylog,
};

static sds
redisfmt(redisReply *reply)
{
    sds			c, command = sdsempty();
    int			i;

    if (reply == NULL)
	return command;

    switch (reply->type) {
    case REDIS_REPLY_STRING:
	return sdscatfmt(command, "$%U\r\n%s\r\n", reply->len, reply->str);
    case REDIS_REPLY_ARRAY:
	c = sdsempty();
	for (i = 0; i < reply->elements; i++)
	    c = sdscat(c, redisfmt(reply->element[i]));
	command = sdscatfmt(command, "*%u\r\n%S", reply->elements, c);
	sdsfree(c);
	return command;
    case REDIS_REPLY_INTEGER:
	return sdscatfmt(command, ":%I\r\n", reply->integer);
    case REDIS_REPLY_STATUS:
	return sdscatfmt(command, "+%s\r\n", reply->str);
    case REDIS_REPLY_ERROR:
	return sdscatfmt(command, "-%s\r\n", reply->str);
    case REDIS_REPLY_NIL:
    default:
	break;
    }
    return command;
}

static void
on_redis_server_reply(
	redisAsyncContext *c, redisReply *reply, const sds cmd, void *arg)
{
    struct client	*client = (struct client *)arg;

    (void)c;
    sdsfree(cmd);
    client_write(client, redisfmt(reply), NULL);
}

void
on_redis_client_read(struct proxy *proxy, struct client *client,
		ssize_t nread, const uv_buf_t *buf)
{
    if (redis_protocol == 0 ||
	redisSlotsProxyConnect(proxy->slots,
		proxylog, &client->u.redis.reader,
		buf->base, nread, on_redis_server_reply, client) < 0) {
	uv_close((uv_handle_t *)&client->stream, on_client_close);
    }
}

void
on_redis_client_close(struct client *client)
{
    redisSlotsProxyFree(client->u.redis.reader);
}

static void
on_redis_connected(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    sds			message;

    pmNotifyErr(LOG_INFO, "connected to redis server(s)\n");

    message = sdsnew("slots");
    if (redis_protocol)
	message = sdscat(message, ", command keys");
    if (archive_discovery | series_queries)
	message = sdscat(message, ", schema version");
    pmNotifyErr(LOG_INFO, "%s setup\n", message);
    sdsfree(message);

    pmDiscoverSetSlots(&redis_discover.module, proxy->slots);
}

void
setup_redis_modules(struct proxy *proxy)
{
    redisSlotsFlags	flags = SLOTS_NONE;
    sds			option;

    if ((option = pmIniFileLookup(config, "pmproxy", "redis.enabled")))
	redis_protocol = (strncmp(option, "true", sdslen(option)) == 0);
    if ((option = pmIniFileLookup(config, "pmseries", "enabled")))
	series_queries = (strncmp(option, "true", sdslen(option)) == 0);
    if ((option = pmIniFileLookup(config, "discover", "enabled")))
	archive_discovery = (strncmp(option, "true", sdslen(option)) == 0);

    if (proxy->slots == NULL) {
	if (redis_protocol)
	    flags |= SLOTS_KEYMAP;
	if (archive_discovery | series_queries)
	    flags |= SLOTS_VERSION;
	proxy->slots = redisSlotsConnect(proxy->config,
			flags, proxylog, on_redis_connected,
			proxy, proxy->events, proxy);
	pmDiscoverSetSlots(&redis_discover.module, proxy->slots);
    }

    pmDiscoverSetEventLoop(&redis_discover.module, proxy->events);
    pmDiscoverSetConfiguration(&redis_discover.module, proxy->config);
    pmDiscoverSetMetricRegistry(&redis_discover.module, proxy->metrics);
    pmDiscoverSetup(&redis_discover.module, &redis_discover.callbacks, proxy);
}
