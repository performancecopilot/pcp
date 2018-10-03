/*
 * Copyright (c) 2018 Red Hat.
 * Copyright (c) 2018 Challa Venkata Naga Prajwal <cvnprajwal at gmail dot com>
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
#include "discover.h"

static int series_queries = 1;		/* TODO: config file */
static int redis_protocol = 1;		/* TODO: config file */
static int archive_discovery = 1;	/* TODO: config file */

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
on_redis_client_write(uv_write_t *wrreq, int status)
{
    struct client	*client = (struct client *)wrreq->data;
    uv_buf_t		*wrbuf = &client->u.redis.writebuf;

    sdsfree(wrbuf->base);
    wrbuf->base = NULL;
    wrbuf->len = 0;
}

static void
on_redis_server_reply(redisAsyncContext *c, redisReply *reply, void *arg)
{
    struct client	*client = (struct client *)arg;
    uv_stream_t		*stream = (uv_stream_t *)&client->stream;
    uv_write_t		*wrreq = &client->u.redis.writereq;
    uv_buf_t		*wrbuf = &client->u.redis.writebuf;
    sds			command;
    int			sts;

    (void)c;
    command = redisfmt(reply);
    wrbuf->base = command;
    wrbuf->len = sdslen(command);
    wrreq->data = (void *)client;
    sts = uv_write(wrreq, stream, wrbuf, 1, on_redis_client_write);
    if (sts) {
	fprintf(stderr, "%s: out-of-memory for redis client reader\n",
			pmGetProgname());
	uv_close((uv_handle_t *)stream, on_client_close);
    }
}

void
on_redis_client_read(struct proxy *proxy, struct client *client,
		ssize_t nread, const uv_buf_t *buf)
{
    if (!redis_protocol ||
        redisSlotsProxy(proxy->slots, proxylog, &client->u.redis.reader,
		nread, buf->base, on_redis_server_reply, client) < 0)
	uv_close((uv_handle_t *)&client->stream, on_client_close);
}

static void
on_redis_connected(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    sds			message;

    message = sdsnew("slots");
    if (redis_protocol)
	message = sdscat(message, ", command keys");
    if (archive_discovery | series_queries)
	message = sdscat(message, ", schema version");
    pmNotifyErr(LOG_INFO,
		"%s: %s setup from redis-server on %s\n",
		pmGetProgname(), message, proxy->redishost);
    sdsfree(message);

    redis_discover.module.events = proxy->events;
    redis_discover.module.slots = proxy->slots;
    pmDiscoverSetup(&redis_discover, proxy);
    proxy->redisetup = 1;
}

void
setup_redis_proxy(struct proxy *proxy)
{
    redisSlotsFlags	flags = SLOTS_NONE;

    if (redis_protocol)
	flags |= SLOTS_KEYMAP;
    if (archive_discovery | series_queries)
	flags |= SLOTS_VERSION;

    proxy->slots = redisSlotsConnect(proxy->redishost, flags,
	    proxylog, on_redis_connected, proxy, proxy->events, proxy);
}
