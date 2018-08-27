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
#include <uv.h>
#include "pmapi.h"
#include "libpcp.h"
#include "series.h"
#include "pmproxy.h"
#include "slots.h"

typedef enum stream_type {
    STREAM_LOCAL,
    STREAM_TCP,
} stream_type;

typedef struct stream {
    stream_type		type;
    unsigned int	active;
    union {
	uv_pipe_t	local;
	uv_tcp_t	tcp;
    } u;
    __pmServerPresence	*presence;
} stream;

typedef struct server {
    uv_loop_t		*events;
    struct stream	*streams;
    int			nstreams;
    int			connected;	/* is Redis slots connections setup */
    sds			hostspec;	/* initial Redis host specification */
    redisSlots		*slots;
} server;

extern void
serverlog(pmloglevel level, sds message, void *arg)
{
    struct server	*server = (struct server *)arg;
    const char		*state = server->connected? NULL : "- DISCONNECTED - ";
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

static void
on_redis_connected(void *arg)
{
    struct server	*server = (struct server *)arg;

    server->connected = 1;
    pmNotifyErr(LOG_INFO, "%s: connected to redis-server on %s\n",
		    pmGetProgname(), server->hostspec);
    /* TODO: issue COMMAND exchange to calculate key positions */
}

static void
setup_redis_slots(uv_timer_t *arg)
{
    struct server	*server = (struct server *)arg;

    server->slots = redisSlotsConnect(
	    server->hostspec, /* no schema version exchange */ 0,
	    serverlog, on_redis_connected, server, server->events, server);
}

static void
close_redis_slots(void *arg)
{
    struct server	*server = (struct server *)arg;

    if (server->connected) {
	server->connected = 0;
	redisSlotsFree(server->slots);
	server->slots = NULL;
    }
}

static struct server *
server_init(int portcount, const char *localpath)
{
    struct stream	*streams;
    struct server	*server;
    int			count;

    if ((server = calloc(1, sizeof(struct server))) == NULL) {
	fprintf(stderr, "%s: out-of-memory in server setup\n", pmGetProgname());
	return NULL;
    }

    count = portcount + (*localpath ? 1 : 0);
    if (count) {
	/* allocate space for listen port data structures */
	if ((streams = calloc(count, sizeof(stream))) == NULL) {
	    fprintf(stderr, "%s: out-of-memory allocating %d streams\n",
			    pmGetProgname(), count);
	    free(server);
	    return NULL;
	}
	server->streams = streams;
    } else {
	fprintf(stderr, "%s: no ports or local paths specified\n",
			pmGetProgname());
	free(server);
	return NULL;
    }

    server->hostspec = sdsnew("localhost:6379");	/* TODO: config file */
    server->events = uv_default_loop();
    uv_loop_init(server->events);
    return server;
}

static void
on_client_connection(uv_stream_t *server, int status)
{
    /* TODO - plugin proxying logic! (PCP & RESP protocols) */
}

static int
OpenRequestPort(struct stream *stream, const char *address, int port, int maxpending)
{
    struct sockaddr_in6	addr;
    int			sts;

    stream->type = STREAM_TCP;

    /*
     * @address is a string representing the Inet/IPv6 address that the port
     * is advertised for.  To allow connections to all of the hosts internet
     * addresses from clients use address == "INADDR_ANY", or for localhost
     * access only use address == "INADDR_LOOPBACK".
     * If the address is NULL or "INADDR_ANY" or "INADDR_LOOPBACK", then we
     * open one socket for each address family (inet, IPv6).
     */
    if (!address || strcmp(address, "INADDR_ANY") == 0)
	uv_ip6_addr("0.0.0.0", port, &addr);
    else if (strcmp(address, "INADDR_LOOPBACK") == 0)
	uv_ip6_addr("127.0.0.1", port, &addr);
    else
	uv_ip6_addr(address, port, &addr);

    uv_tcp_init(uv_default_loop(), &stream->u.tcp);
    uv_tcp_bind(&stream->u.tcp, (const struct sockaddr *) &addr, 0);
    uv_tcp_nodelay(&stream->u.tcp, 1);
    uv_tcp_keepalive(&stream->u.tcp, 1, 50);
    sts = uv_listen((uv_stream_t *)&stream->u.tcp, maxpending, on_client_connection);
    if (sts != 0) {
	fprintf(stderr, "%s: socket listen error %s\n",
			pmGetProgname(), uv_strerror(sts));
        return -ENOTCONN;
    }
    if (__pmServerHasFeature(PM_SERVER_FEATURE_DISCOVERY))
	stream->presence = __pmServerAdvertisePresence(PM_SERVER_PROXY_SPEC, port);
    stream->active = 1;
    return 0;
}

static int
OpenRequestLocal(struct stream *stream, const char *name, int maxpending)
{
    int			sts;

    stream->type = STREAM_LOCAL;

    uv_pipe_init(uv_default_loop(), &stream->u.local, 0);
    uv_pipe_bind(&stream->u.local, name);
    uv_pipe_chmod(&stream->u.local, UV_READABLE);
    sts = uv_listen((uv_stream_t *)&stream->u.local, maxpending, on_client_connection);
    if (sts != 0) {
	fprintf(stderr, "%s: local listen error %s\n",
			pmGetProgname(), uv_strerror(sts));
        return -ENOTCONN;
    }
    __pmServerSetFeature(PM_SERVER_FEATURE_UNIX_DOMAIN);
    stream->active = 1;
    return 0;
}

void *
OpenRequestPorts(const char *localpath, int maxpending)
{
    int			total, count, port, sts, i, n = 0;
    const char		*address;
    struct server	*server;
    struct stream	*stream;

    if ((sts = total = __pmServerSetupRequestPorts()) < 0)
	return NULL;

    if ((server = server_init(total, localpath)) == NULL)
	return NULL;

    count = 0;
    if (*localpath) {
	stream = &server->streams[n++];
	if (OpenRequestLocal(stream, localpath, maxpending) == 0)
	    count++;
    }

    for (i = 0; i < total; i++) {
	if ((sts = __pmServerGetRequestPort(i, &address, &port)) < 0)
	    break;
	stream = &server->streams[n++];
	if (OpenRequestPort(stream, address, port, maxpending) < 0)
	    continue;
	count++;
    }

    if (count == 0) {
	pmNotifyErr(LOG_ERR, "%s: can't open any request ports, exiting\n",
		pmGetProgname());
	free(server);
	return NULL;
    }
    server->nstreams = count;
    return server;
}

extern void
ShutdownPorts(void *arg)
{
    struct server	*server = (struct server *)arg;
    struct stream	*stream;
    int			i;

    for (i = 0; i < server->nstreams; i++) {
	stream = &server->streams[i++];
	if (stream->active == 0)
	    continue;
	if (stream->type == STREAM_LOCAL)
	    uv_close((uv_handle_t *)&stream->u.local, NULL);
	if (stream->type == STREAM_TCP)
	    uv_close((uv_handle_t *)&stream->u.tcp, NULL);
	if (stream->presence)
	    __pmServerUnadvertisePresence(stream->presence);
    }
    server->nstreams = 0;
    free(server->streams);
}

void
DumpRequestPorts(FILE *stream, void *arg)
{
    struct server	*server = (struct server *)arg;

    /* TODO */
    (void)server;
}

void
MainLoop(void *arg)
{
    struct server	*server = (struct server *)arg;
    uv_timer_t		attempt;

    /*
     * Attempt to establish a Redis connection straight away;
     * this is achieved via a timer that expires immediately.
     * If this fails (now or from subsequent connection drop),
     * we try again later during client request processing.
     */
    uv_timer_init(server->events, &attempt);
    uv_timer_start(&attempt, setup_redis_slots, 0, 0);

    uv_run(server->events, UV_RUN_DEFAULT);

    close_redis_slots(server->slots);
}
