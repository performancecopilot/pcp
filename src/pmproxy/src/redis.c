/*
 * Copyright (c) 2018-2020 Red Hat.
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

static int search_queries;
static int series_queries;
static int redis_protocol;
static int archive_discovery;

static pmDiscoverCallBacks redis_series = {
    .on_source		= pmSeriesDiscoverSource,
    .on_closed		= pmSeriesDiscoverClosed,
    .on_labels		= pmSeriesDiscoverLabels,
    .on_metric		= pmSeriesDiscoverMetric,
    .on_values		= pmSeriesDiscoverValues,
    .on_indom		= pmSeriesDiscoverInDom,
    .on_text		= pmSeriesDiscoverText,
};

static pmDiscoverCallBacks redis_search = {
    .on_metric		= pmSearchDiscoverMetric,
    .on_indom		= pmSearchDiscoverInDom,
    .on_text		= pmSearchDiscoverText,
};

static pmDiscoverSettings redis_discover = {
    .module.on_info	= proxylog,
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
	return sdscatfmt(command, "$%U\r\n%s\r\n", (uint64_t)reply->len, reply->str);
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
	c = sdsempty();
	for (i = 0; i < reply->elements; i++)
	    c = sdscat(c, redisfmt(reply->element[i]));
	if (reply->type == REDIS_REPLY_ARRAY)
	    command = sdscatfmt(command, "*%u\r\n%S", reply->elements, c);
	else if (reply->type == REDIS_REPLY_MAP)
	    command = sdscatfmt(command, "%%%u\r\n%S", reply->elements, c);
	else /* (reply->type == REDIS_REPLY_SET) */
	    command = sdscatfmt(command, "~%u\r\n%S", reply->elements, c);
	sdsfree(c);
	return command;
    case REDIS_REPLY_INTEGER:
	return sdscatfmt(command, ":%I\r\n", reply->integer);
    case REDIS_REPLY_DOUBLE:
	return sdscatfmt(command, ",%s\r\n", reply->str);
    case REDIS_REPLY_STATUS:
	return sdscatfmt(command, "+%s\r\n", reply->str);
    case REDIS_REPLY_ERROR:
	return sdscatfmt(command, "-%s\r\n", reply->str);
    case REDIS_REPLY_BOOL:
	return sdscatfmt(command, "#%c\r\n", reply->integer ? 't' : 'f');
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
    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p\n", "on_redis_client_read", client);

    if (redis_protocol == 0 || proxy->redisetup == 0 ||
	redisSlotsProxyConnect(proxy->slots,
		proxylog, &client->u.redis.reader,
		buf->base, nread, on_redis_server_reply, client) < 0) {
	client_close(client);
    }
}

void
on_redis_client_write(struct client *client)
{
    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: client %p\n", "on_redis_client_write", client);
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
    mmv_registry_t	*metric_registry = proxymetrics(proxy, METRICS_REDIS);
    sds			message;

    message = sdsnew("Redis slots");
    if (redis_protocol)
	message = sdscat(message, ", command keys");
    if ((search_queries = pmSearchEnabled(proxy->slots)))
	message = sdscat(message, ", RediSearch");
    if (series_queries)
	message = sdscat(message, ", schema version");
    pmNotifyErr(LOG_INFO, "%s setup\n", message);
    sdsfree(message);

    if (series_queries) {
	if (search_queries)
	    redis_series.next = &redis_search;
	redis_discover.callbacks = redis_series;
    } else if (search_queries) {
	redis_discover.callbacks = redis_search;
    }

    if (archive_discovery && (series_queries || search_queries)) {
	pmDiscoverSetEventLoop(&redis_discover.module, proxy->events);
	pmDiscoverSetConfiguration(&redis_discover.module, proxy->config);
	pmDiscoverSetMetricRegistry(&redis_discover.module, metric_registry);
	pmDiscoverSetup(&redis_discover.module, &redis_discover.callbacks, proxy);
	pmDiscoverSetSlots(&redis_discover.module, proxy->slots);
    }

    proxy->redisetup = 1;
}

/*
 * Attempt to establish a Redis connection straight away;
 * which is achieved via a timer that expires immediately
 * during the startup process.
 */
void
setup_redis_module(struct proxy *proxy)
{
    redisSlotsFlags	flags = SLOTS_NONE;
    sds			option;

    if ((option = pmIniFileLookup(config, "pmproxy", "redis.enabled")))
	redis_protocol = (strncmp(option, "true", sdslen(option)) == 0);
    if ((option = pmIniFileLookup(config, "pmseries", "enabled")))
	series_queries = (strncmp(option, "true", sdslen(option)) == 0);
    if ((option = pmIniFileLookup(config, "pmsearch", "enabled")))
	search_queries = (strncmp(option, "true", sdslen(option)) == 0);
    if ((option = pmIniFileLookup(config, "discover", "enabled")))
	archive_discovery = (strncmp(option, "true", sdslen(option)) == 0);

    if (proxy->slots == NULL) {
	if (redis_protocol)
	    flags |= SLOTS_KEYMAP;
	if (series_queries)
	    flags |= SLOTS_VERSION;
	if (search_queries)
	    flags |= SLOTS_SEARCH;
	proxy->slots = redisSlotsConnect(proxy->config,
			flags, proxylog, on_redis_connected,
			proxy, proxy->events, proxy);
    }
}

void
close_redis_module(struct proxy *proxy)
{
    if (proxy->slots) {
	redisSlotsFree(proxy->slots);
	proxy->slots = NULL;
    }

    if (archive_discovery)
	pmDiscoverClose(&redis_discover.module);

    proxymetrics_close(proxy, METRICS_REDIS);
}
