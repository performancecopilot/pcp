/*
 * Copyright (c) 2018 Red Hat.
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

void
proxylog(pmloglevel level, sds message, void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    const char		*state = proxy->redisetup? "" : "- DISCONNECTED - ";
    int			priority;

    switch (level) {
    case PMLOG_TRACE:
    case PMLOG_DEBUG:
	priority = LOG_DEBUG;
	break;
    case PMLOG_INFO:
	priority = LOG_INFO;
	break;
    case PMLOG_WARNING:
	priority = LOG_WARNING;
	break;
    case PMLOG_CORRUPT:
	priority = LOG_CRIT;
	break;
    default:
	priority = LOG_ERR;
	break;
    }
    pmNotifyErr(priority, "%s%s", state, message);
}

static struct proxy *
server_init(int portcount, const char *localpath)
{
    struct server	*servers;
    struct proxy	*proxy;
    int			count;

    if ((proxy = calloc(1, sizeof(struct proxy))) == NULL) {
	fprintf(stderr, "%s: out-of-memory in proxy server setup\n",
			pmGetProgname());
	return NULL;
    }

    count = portcount + (*localpath ? 1 : 0);
    if (count) {
	/* allocate space for maximum listen port data structures */
	if ((servers = calloc(count, sizeof(struct server))) == NULL) {
	    fprintf(stderr, "%s: out-of-memory allocating for %d ports\n",
			    pmGetProgname(), count);
	    free(proxy);
	    return NULL;
	}
	proxy->servers = servers;
    } else {
	fprintf(stderr, "%s: no ports or local paths specified\n",
			pmGetProgname());
	free(proxy);
	return NULL;
    }

    proxy->redishost = sdsnew("localhost:6379");	/* TODO: config file */
    proxy->events = uv_default_loop();
    uv_loop_init(proxy->events);
    return proxy;
}

static void
on_client_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    struct client	*client = (struct client *)handle;

    if (pmDebugOptions.desperate)
	fprintf(stderr, "%s: client %p buffer allocation of %lld bytes\n",
			pmGetProgname(), client, (long long)suggested_size);

    if ((buf->base = malloc(suggested_size)) != NULL)
	buf->len = suggested_size;
    else
	buf->len = 0;
}

void
on_client_close(uv_handle_t *handle)
{
    struct client	*client = (struct client *)handle;

    if (pmDebugOptions.desperate)
	fprintf(stderr, "%s: client %p connection closed\n",
			pmGetProgname(), client);

#if 0
    struct proxy	*proxy = (struct proxy *)handle->data;
    struct client	*tmp;
    /* remove client from the doubly-linked list */
    tmp = client->prev;
    tmp->next = client->next;
    if (tmp->next == NULL)
	proxy->tail = tmp;
    else
	tmp->next->prev = tmp;
    /* TODO: free any partially accrued read/write buffers here */
    free(client);
#endif
}

static stream_protocol
client_protocol(int key)
{
    switch (key) {
    case 'p':	/* pmproxy */
	return STREAM_PCP;
    case '-':	/* error */
    case '+':	/* status */
    case ':':	/* integer */
    case '$':	/* string */
    case '*':	/* array */
	return STREAM_REDIS;
    case '\0':
	return STREAM_SECURE;
    default:
	break;
    }
    return STREAM_UNKNOWN;
}

static void
on_client_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
    struct proxy	*proxy = (struct proxy *)stream->data;
    struct client	*client = (struct client *)stream;

    if (nread > 0) {
	if (client->protocol == STREAM_UNKNOWN)
	    client->protocol = client_protocol(*buf->base);
	switch (client->protocol) {
	case STREAM_PCP:
	    return on_pcp_client_read(proxy, client, nread, buf);
	case STREAM_REDIS:
	    return on_redis_client_read(proxy, client, nread, buf);
	default:
	    break;
	}
	if (pmDebugOptions.pdu)
	    fprintf(stderr, "%s: unknown protocol key '%c' - disconnecting\n",
			pmGetProgname(), *buf->base);
    } else {
	if (nread < 0)
	    fprintf(stderr, "%s: read error %ld - disconnecting\n",
			pmGetProgname(), (long)nread);
#if 0
	if (buf->base != NULL)
	    free(buf->base);
	buf->base = NULL;
	buf->len = 0;
#endif
    }
    uv_close((uv_handle_t *)stream, on_client_close);
}

static void
on_client_connection(uv_stream_t *stream, int status)
{
    struct proxy	*proxy = (struct proxy *)stream->data;
    struct client	*client;
    uv_handle_t		*handle;

    if (status != 0) {
	fprintf(stderr, "%s: client connection failed: %s\n",
			pmGetProgname(), uv_strerror(status));
	return;
    }

    if ((client = calloc(1, sizeof(*client))) == NULL) {
	fprintf(stderr, "%s: out-of-memory for new client\n",
			pmGetProgname());
	return;
    }

    status = uv_tcp_init(proxy->events, &client->stream.u.tcp);
    if (status != 0) {
	fprintf(stderr, "%s: client tcp init failed: %s\n",
			pmGetProgname(), uv_strerror(status));
	free(client);
	return;
    }

    status = uv_accept(stream, (uv_stream_t *)&client->stream.u.tcp);
    if (status != 0) {
	fprintf(stderr, "%s: client tcp init failed: %s\n",
			pmGetProgname(), uv_strerror(status));
	free(client);
	return;
    }
    handle = (uv_handle_t *)&client->stream.u.tcp;
    handle->data = (void *)proxy;

    /* insert client into doubly-linked list at the head */
    if (proxy->head != NULL) {
	client->next = proxy->head;
	proxy->head->prev = client;
	proxy->head = client;
    } else {
	proxy->head = proxy->tail = client;
    }

    status = uv_read_start((uv_stream_t *)&client->stream.u.tcp,
			    on_client_alloc, on_client_read);
    if (status != 0) {
	fprintf(stderr, "%s: client read start failed: %s\n",
			pmGetProgname(), uv_strerror(status));
	uv_close((uv_handle_t *)stream, on_client_close);
    }
}

static int
OpenRequestPort(struct proxy *proxy, struct server *server, stream_family family,
		const struct sockaddr *addr, int port, int maxpending)
{
    struct stream	*stream = &server->stream;
    uv_handle_t		*handle;
    int			sts;

    stream->family = family;

    uv_tcp_init(proxy->events, &stream->u.tcp);
    handle = (uv_handle_t *)&stream->u.tcp;
    handle->data = (void *)proxy;

    uv_tcp_bind(&stream->u.tcp, addr, 0);
    uv_tcp_nodelay(&stream->u.tcp, 1);
    uv_tcp_keepalive(&stream->u.tcp, 1, 50);	/* TODO: config file */

    sts = uv_listen((uv_stream_t *)&stream->u.tcp, maxpending, on_client_connection);
    if (sts != 0) {
	fprintf(stderr, "%s: socket listen error %s\n",
			pmGetProgname(), uv_strerror(sts));
        return -ENOTCONN;
    }
    stream->active = 1;
    if (__pmServerHasFeature(PM_SERVER_FEATURE_DISCOVERY))
	server->presence = __pmServerAdvertisePresence(PM_SERVER_PROXY_SPEC, port);
    return 0;
}

static int
OpenRequestLocal(struct proxy *proxy, struct server *server,
		const char *name, int maxpending)
{
    uv_handle_t		*handle;
    struct stream	*stream = &server->stream;
    int			sts;

    stream->family = STREAM_LOCAL;

    uv_pipe_init(proxy->events, &stream->u.local, 0);
    handle = (uv_handle_t *)&stream->u.local;
    handle->data = (void *)proxy;

    uv_pipe_bind(&stream->u.local, name);
    uv_pipe_chmod(&stream->u.local, UV_READABLE);

    sts = uv_listen((uv_stream_t *)&stream->u.local, maxpending, on_client_connection);
    if (sts != 0) {
	fprintf(stderr, "%s: local listen error %s\n",
			pmGetProgname(), uv_strerror(sts));
        return -ENOTCONN;
    }
    stream->active = 1;
    __pmServerSetFeature(PM_SERVER_FEATURE_UNIX_DOMAIN);
    return 0;
}

void *
OpenRequestPorts(const char *localpath, int maxpending)
{
    int			total, count, port, sts, i, n;
    const char		*address;
    __pmSockAddr	**addrlist;
    const struct sockaddr *sockaddr;
    stream_family	family;
    struct server	*server;
    struct proxy	*proxy;

    if ((sts = total = __pmServerSetupRequestPorts()) < 0)
	return NULL;
    /* allow for both IPv6 and IPv4 addresses for each port */
    if ((addrlist = calloc(total * 2, sizeof(__pmSockAddr *))) == NULL)
	return NULL;

    for (i = n = 0; i < total; i++) {
	__pmServerGetRequestPort(i, &address, &port);
	if ((addrlist[n] = __pmStringToSockAddr(address)) == NULL)
	    goto fail;
	family = __pmSockAddrGetFamily(addrlist[n]);
	if (!family) { /* attempt both IPv4 and IPv6 */
	    if ((addrlist[n+1] = __pmSockAddrDup(addrlist[n])) == NULL)
		goto fail;
	    __pmSockAddrSetFamily(addrlist[n], AF_INET);
	    __pmSockAddrSetPort(addrlist[n++], port);
	    __pmSockAddrSetFamily(addrlist[n], AF_INET6);
	    __pmSockAddrSetPort(addrlist[n++], port);
	} else {
	    __pmSockAddrSetFamily(addrlist[n], family);
	    __pmSockAddrSetPort(addrlist[n++], port);
	}
    }
    total = n;

    if ((proxy = server_init(total, localpath)) == NULL)
	goto fail;

    count = n = 0;
    if (*localpath) {
	server = &proxy->servers[n++];
	if (OpenRequestLocal(proxy, server, localpath, maxpending) == 0)
	    count++;
    }

    for (i = 0; i < total; i++) {
	sockaddr = (const struct sockaddr *)addrlist[i];
	family = __pmSockAddrGetFamily(addrlist[i]) == AF_INET ?
					STREAM_TCP4 : STREAM_TCP6;
	server = &proxy->servers[n++];
	if (OpenRequestPort(proxy, server, family, sockaddr, port, maxpending) == 0)
	    count++;
	__pmSockAddrFree(addrlist[i]);
    }
    free(addrlist);

    if (count == 0) {
	pmNotifyErr(LOG_ERR, "%s: can't open any request ports, exiting\n",
		pmGetProgname());
	free(proxy);
	return NULL;
    }
    proxy->nservers = count;
    return proxy;

fail:
    for (i = 0; i < n; i++)
	__pmSockAddrFree(addrlist[i]);
    free(addrlist);
    return NULL;
}

extern void
ShutdownPorts(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    struct server	*server;
    struct stream	*stream;
    int			i;

    for (i = 0; i < proxy->nservers; i++) {
	server = &proxy->servers[i++];
	stream = &server->stream;
	if (stream->active == 0)
	    continue;
	uv_close((uv_handle_t *)&stream, NULL);
	if (server->presence)
	    __pmServerUnadvertisePresence(server->presence);
    }
    proxy->nservers = 0;
    free(proxy->servers);
    proxy->servers = NULL;

    if (proxy->redisetup) {
	proxy->redisetup = 0;
	redisSlotsFree(proxy->slots);
    }
    proxy->slots = NULL;
    sdsfree(proxy->redishost);
}

void
DumpRequestPorts(FILE *stream, void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;

    /* TODO */
    (void)proxy;
}

/*
 * Attempt to establish a Redis connection straight away;
 * this is achieved via a timer that expires immediately.
 */
static void
setup_proxy(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct proxy	*proxy = (struct proxy *)handle->data;

    setup_redis_proxy(proxy);
}

void
MainLoop(void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    uv_timer_t		attempt;
    uv_handle_t		*handle;

    uv_timer_init(proxy->events, &attempt);
    handle = (uv_handle_t *)&attempt;
    handle->data = (void *)proxy;
    uv_timer_start(&attempt, setup_proxy, 0, 0);

    uv_run(proxy->events, UV_RUN_DEFAULT);
}
