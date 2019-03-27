/*
 * Copyright (c) 2018-2019 Red Hat.
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
#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <uv.h>
#include "pmapi.h"
#include "mmv_stats.h"
#include "pmwebapi.h"
#include "pmproxy.h"
#include "libpcp.h"
#include "slots.h"
#include "http.h"
#include "pcp.h"

typedef struct stream_write_baton {
    uv_write_t		writer;
    uv_buf_t		buffer[2];
} stream_write_baton;

typedef enum stream_family {
    STREAM_LOCAL	= 1,
    STREAM_TCP4,
    STREAM_TCP6,
} stream_family;

typedef struct stream {
    union {
	uv_pipe_t	local;
	uv_tcp_t	tcp;
    } u;
    stream_family	family;
    unsigned int	active: 1;
    unsigned int	zero : 15;
    unsigned int	port : 16;
    const char		*address;
} stream;

typedef enum stream_protocol {
    STREAM_UNKNOWN	= 0,
    STREAM_SECURE	= 0x1,
    STREAM_REDIS	= 0x2,
    STREAM_HTTP		= 0x4,
    STREAM_PCP		= 0x8,
} stream_protocol;

typedef struct redis_client {
    redisReader		*reader;	/* RESP request handling state */
} redis_client;

typedef struct http_client {
    http_parser		parser;		/* HTTP request parsing state */
    struct servlet	*servlet;	/* servicing current request */
    struct dict		*parameters;	/* URL parameters dictionary */
    struct dict		*headers;	/* request header dictionary */
    void		*privdata;	/* private HTTP parsing state */
    void		*data;		/* opaque servlet information */
    unsigned int	type : 16;	/* HTTP response content type */
    unsigned int	flags : 16;	/* request status flags field */
} http_client;

typedef struct pcp_client {
    pcp_proxy_state	state;
    sds			hostname;
    unsigned int	port : 16;
    unsigned int	certreq : 1;
    unsigned int	connected : 1;
    unsigned int	pad : 14;
    uv_connect_t	pmcd;
    uv_tcp_t		socket;
} pcp_client;

typedef struct client {
    struct stream	stream;
    stream_protocol	protocol;
    union {
	redis_client	redis;
	http_client	http;
	pcp_client	pcp;
    } u;
    struct proxy	*proxy;
    struct client	*next;
    struct client	**prev;
    sds			buffer;
} client;

typedef struct server {
    struct stream	stream;
    __pmServerPresence	*presence;
} server;

typedef struct proxy {
    struct client	*first;		/* doubly linked list of clients */
    struct server	*servers;	/* array of tcp/pipe socket servers */
    unsigned int	nservers;	/* count of entries in server array */
    unsigned int	redisetup;	/* is Redis slots information setup */
    redisSlots		*slots;		/* mapping of Redis keys to servers */
    struct servlet	*servlets;	/* linked list of http URL handlers */
    struct mmv_registry	*metrics;	/* internal performance metrics */
    struct dict		*config;	/* configuration dictionary */
    uv_loop_t		*events;	/* global, async event loop */
} proxy;

extern void on_client_close(uv_handle_t *);
extern void on_buffer_alloc(uv_handle_t *, size_t, uv_buf_t *);
extern void client_write(struct client *, sds, sds);
extern void proxylog(pmLogLevel, sds, void *);

extern void on_redis_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_redis_client_close(struct client *);

extern void on_http_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_http_client_close(struct client *);

extern void on_pcp_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_pcp_client_close(struct client *);

extern void setup_redis_modules(struct proxy *);
extern void setup_http_modules(struct proxy *);
extern void setup_pcp_modules(struct proxy *);
extern void setup_modules(struct proxy *);

#endif	/* PROXY_SERVER_H */
