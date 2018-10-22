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
proxylog(pmLogLevel level, sds message, void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    const char		*state = proxy->slots ? "" : "- DISCONNECTED - ";
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

void
on_buffer_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
    if (pmDebugOptions.desperate)
	fprintf(stderr, "%s: handle %p buffer allocation of %lld bytes\n",
			"on_buffer_alloc", handle, (long long)suggested_size);

    if ((buf->base = sdsnewlen(SDS_NOINIT, suggested_size)) != NULL)
	buf->len = suggested_size;
    else
	buf->len = 0;
}

void
on_client_close(uv_handle_t *handle)
{
    struct client	*client = (struct client *)handle;

    if (pmDebugOptions.desperate)
	fprintf(stderr, "client %p connection closed\n", client);

    switch (client->protocol) {
    case STREAM_PCP:
	on_pcp_client_close(client);
	break;
    case STREAM_REDIS:
	on_redis_client_close(client);
	break;
    default:
	break;
    }

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

static void
on_client_write(uv_write_t *writer, int status)
{
    struct client	*client = (struct client *)writer->handle;
    stream_write_baton	*request = (stream_write_baton *)writer;

    sdsfree(request->buffer[0].base);
    if (request->buffer[1].base)	/* optional second buffer */
	sdsfree(request->buffer[1].base);
    free(request);

    if (status != 0)
	uv_close((uv_handle_t *)&client->stream, on_client_close);
}

void
client_write(struct client *client, sds buffer, sds suffix)
{
    stream_write_baton	*request = calloc(1, sizeof(stream_write_baton));
    unsigned int	nbuffers = 0;

    if (request) {
	if (pmDebugOptions.pdu)
	    fprintf(stderr, "%s: sending %ld bytes to client %p\n",
			"client_write", sdslen(buffer), client);
	request->buffer[nbuffers++] = uv_buf_init(buffer, sdslen(buffer));
	if (suffix != NULL)
	    request->buffer[nbuffers++] = uv_buf_init(suffix, sdslen(suffix));
	uv_write(&request->writer, (uv_stream_t *)&client->stream,
		 request->buffer, nbuffers, on_client_write);
    } else {
	uv_close((uv_handle_t *)&client->stream, on_client_close);
    }
}

static stream_protocol
client_protocol(int key)
{
    switch (key) {
    case 'p':	/* PCP pmproxy */
	return STREAM_PCP;
    case '-':	/* RESP error */
    case '+':	/* RESP status */
    case ':':	/* RESP integer */
    case '$':	/* RESP string */
    case '*':	/* RESP array */
	return STREAM_REDIS;
    case 0x14:	/* TLS ChangeCipherSpec */
    case 0x15:	/* TLS Alert */
    case 0x16:	/* TLS Handshake */
    case 0x17:	/* TLS Application */
    case 0x18:	/* TLS Heartbeat */
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
	case STREAM_SECURE:
	    fprintf(stderr, "%s: SSL/TLS connection initiated by client %p\n",
			"client_protocol", client);
	default:
	    break;
	}
	if (pmDebugOptions.pdu)
	    fprintf(stderr, "%s: unknown protocol key '%c' (0x%x) - disconnecting"
			"client %p\n", "on_client_read", *buf->base, (unsigned int)*buf->base, proxy);
    } else {
	if (pmDebugOptions.pdu && nread < 0)
	    fprintf(stderr, "%s: read error %ld - disconnecting client %p\n",
			"on_client_read", (long)nread, client);
#if 0
	if (buf->base != NULL)
	    sdsfree(buf->base);
	buf->base = NULL;
	buf->len = 0;
#endif
	uv_close((uv_handle_t *)stream, on_client_close);
    }
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
    if (pmDebugOptions.pdu)
	fprintf(stderr, "%s: new client %p\n", "on_client_connection", client);

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
    client->proxy = proxy;

    status = uv_read_start((uv_stream_t *)&client->stream.u.tcp,
			    on_buffer_alloc, on_client_read);
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
    int			sts, flags = 0;

    stream->family = family;
    if (family == STREAM_TCP6)
	flags = UV_TCP_IPV6ONLY;
    stream->port = port;

    uv_tcp_init(proxy->events, &stream->u.tcp);
    handle = (uv_handle_t *)&stream->u.tcp;
    handle->data = (void *)proxy;

    uv_tcp_bind(&stream->u.tcp, addr, flags);
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

typedef struct proxyaddr {
    __pmSockAddr	*addr;
    const char		*address;
    int			port;
} proxyaddr;

void *
OpenRequestPorts(const char *localpath, int maxpending)
{
    int			inaddr, total, count, port, sts, i, n;
    int			with_ipv6 = strcmp(pmGetAPIConfig("ipv6"), "true") == 0;
    const char		*address;
    __pmSockAddr	*addr;
    struct proxyaddr	*addrlist;
    const struct sockaddr *sockaddr;
    stream_family	family;
    struct server	*server;
    struct proxy	*proxy;

    if ((sts = total = __pmServerSetupRequestPorts()) < 0)
	return NULL;

    /* allow for both IPv6 and IPv4 addresses for each port */
    if ((addrlist = calloc(total * 2, sizeof(proxyaddr))) == NULL)
	return NULL;

    /* fill in sockaddr structs for subsequent listen calls */
    for (i = n = 0; i < total; i++) {
	__pmServerGetRequestPort(i, &address, &port);
	addrlist[n].address = address;
	addrlist[n].port = port;

	if (address != NULL &&
	    strcmp(address, "INADDR_ANY") != 0 &&
	    strcmp(address, "INADDR_LOOPBACK") != 0) {
	    addr = __pmStringToSockAddr(address);
	    if (__pmSockAddrGetFamily(addr) == AF_UNSPEC)
		__pmSockAddrFree(addr);
	    else {
		__pmSockAddrSetPort(addr, port);
		addrlist[n++].addr = addr;
		continue;
	    }
	}

	/* address unspecified - create both ipv4 and ipv6 entries */
	if (address == NULL || strcmp(address, "INADDR_ANY") == 0)
	    inaddr = INADDR_ANY;
	else if (strcmp(address, "INADDR_LOOPBACK") == 0)
	    inaddr = INADDR_LOOPBACK;
	else
	    continue;

	addrlist[n].addr = __pmSockAddrAlloc();
	__pmSockAddrInit(addrlist[n].addr, AF_INET, inaddr, port);
	n++;

	if (!with_ipv6)
	     continue;

	addrlist[n].addr = __pmSockAddrAlloc();
	__pmSockAddrInit(addrlist[n].addr, AF_INET6, inaddr, port);
	n++;
    }
    total = n;

    if ((proxy = server_init(total, localpath)) == NULL)
	goto fail;

    count = n = 0;
    if (*localpath) {
	server = &proxy->servers[n++];
	server->stream.address = localpath;
	if (OpenRequestLocal(proxy, server, localpath, maxpending) == 0)
	    count++;
    }

    for (i = 0; i < total; i++) {
	sockaddr = (const struct sockaddr *)addrlist[i].addr;
	family = __pmSockAddrGetFamily(addrlist[i].addr) == AF_INET ?
					STREAM_TCP4 : STREAM_TCP6;
	server = &proxy->servers[n++];
	server->stream.address = addrlist[i].address;
	if (OpenRequestPort(proxy, server, family, sockaddr, port, maxpending) == 0)
	    count++;
	__pmSockAddrFree(addrlist[i].addr);
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
	__pmSockAddrFree(addrlist[i].addr);
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

    if (proxy->slots) {
	redisSlotsFree(proxy->slots);
	proxy->slots = NULL;
    }
    sdsfree(proxy->redishost);
}

void
DumpRequestPorts(FILE *output, void *arg)
{
    struct proxy	*proxy = (struct proxy *)arg;
    struct stream	*stream;
    uv_os_fd_t		uv_fd;
    int			i, fd;

    fprintf(output, "%s request port(s):\n"
		"  sts fd   port  family address\n"
		"  === ==== ===== ====== =======\n", pmGetProgname());

    for (i = 0; i < proxy->nservers; i++) {
	stream = &proxy->servers[i].stream;
	fd = (uv_fileno((uv_handle_t *)stream, &uv_fd) < 0) ? -1 : (int)uv_fd;
	if (stream->family == STREAM_LOCAL)
	    fprintf(output, "  %-3s %4d %5s %-6s %s\n",
		    stream->active ? "ok" : "err", fd, "",
		    "unix", stream->address);
	else
	    fprintf(output, "  %-3s %4d %5d %-6s %s\n",
		    stream->active ? "ok" : "err", fd, stream->port,
		    stream->family == STREAM_TCP4 ? "inet" : "ipv6",
		    stream->address ? stream->address : "INADDR_ANY");
    }
}

/*
 * Attempt to establish a Redis connection straight away;
 * this is achieved via a timer that expires immediately.
 * Once the connection is established (async) modules are
 * again informed via the setup routines.
 */
static void
setup_proxy(uv_timer_t *arg)
{
    uv_handle_t		*handle = (uv_handle_t *)arg;
    struct proxy	*proxy = (struct proxy *)handle->data;

    setup_redis_modules(proxy);
    setup_pcp_modules(proxy);
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
