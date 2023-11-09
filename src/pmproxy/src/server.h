/*
 * Copyright (c) 2018-2019,2021-2022 Red Hat.
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
#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <uv.h>
#include "uv_callback.h"

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#endif

#include <zlib.h>

#include "pmapi.h"
#include "mmv_stats.h"
#include "pmwebapi.h"
#include "pmproxy.h"
#include "libpcp.h"
#include "slots.h"
#include "http.h"
#include "pcp.h"


typedef enum proxy_registry {
    METRICS_NOTUSED	= 0,	/* special "next available" MMV cluster */
    METRICS_SERVER,
    METRICS_REDIS,
    METRICS_HTTP,
    METRICS_PCP,
    METRICS_DISCOVER,
    METRICS_SERIES,
    METRICS_WEBGROUP,
    METRICS_SEARCH,
    NUM_REGISTRY
} proxy_registry_t;

typedef enum proxy_values {
    VALUE_HTTP_COMPRESSED_COUNT, 
    VALUE_HTTP_UNCOMPRESSED_COUNT,
    VALUE_HTTP_COMPRESSED_BYTES,
    VALUE_HTTP_UNCOMPRESSED_BYTES,
    NUM_VALUES
} proxy_values_t;

typedef struct stream_write_baton {
    uv_write_t		writer;
    uv_buf_t		buffer[2];
    unsigned int	nbuffers;
    uv_write_cb		callback;
} stream_write_baton_t;

typedef enum stream_family {
    STREAM_LOCAL	= 1,
    STREAM_TCP4,
    STREAM_TCP6,
} stream_family_t;

typedef struct stream {
    union {
	uv_pipe_t	local;
	uv_tcp_t	tcp;
    } u;
    enum stream_family	family;
    unsigned int	active: 1;
    unsigned int	secure: 1;
    unsigned int	zero : 14;
    unsigned int	port : 16;
    const char		*address;
} stream_t;

typedef enum stream_protocol {
    STREAM_UNKNOWN	= 0,
    STREAM_SECURE	= 0x1,
    STREAM_REDIS	= 0x2,
    STREAM_HTTP		= 0x4,
    STREAM_PCP		= 0x8,
} stream_protocol_t;

typedef struct redis_client {
    redisReader		*reader;	/* RESP request handling state */
} redis_client_t;

typedef struct http_client {
    http_parser		parser;		/* HTTP request parsing state */
    struct servlet	*servlet;	/* servicing current request */
    struct dict		*parameters;	/* URL parameters dictionary */
    struct dict		*headers;	/* request header dictionary */
    sds			username;	/* HTTP Basic Auth user name */
    sds			password;	/* HTTP Basic Auth passphrase */
    sds			realm;		/* optional Basic Auth realm */
    void		*privdata;	/* private HTTP parsing state */
    void		*data;		/* opaque servlet information */
    unsigned int	type : 16;	/* HTTP response content type */
    unsigned int	flags : 16;	/* request status flags field */
    z_stream        strm;
} http_client_t;

typedef struct pcp_client {
    pcp_proxy_state_t	state;
    unsigned int	port : 16;
    unsigned int	certreq : 1;
    unsigned int	connected : 1;
    unsigned int	pad : 14;
    sds			hostname;
    uv_connect_t	pmcd;
    uv_tcp_t		socket;
} pcp_client_t;

#ifdef HAVE_OPENSSL
typedef struct secure_client {
    SSL			*ssl;
    BIO			*read;
    BIO			*write;
    struct secure_client_pending {
	struct client	*next;
	struct client	*prev;
	unsigned int	queued;
	size_t		writes_count;
	uv_buf_t	*writes_buffer;
    } pending;
} secure_client;
#endif

typedef struct client {
    struct stream	stream;
    stream_protocol_t	protocol;
    unsigned int	refcount;
    unsigned int	opened;
    uv_mutex_t		mutex;
#ifdef HAVE_OPENSSL
    secure_client	secure;
#endif
    union {
	redis_client_t	redis;
	http_client_t	http;
	pcp_client_t	pcp;
    } u;
    struct proxy	*proxy;
    sds			buffer;
} client_t;

typedef struct server {
    struct stream	stream;
    __pmServerPresence	*presence;
} server_t;

typedef struct proxy {
    struct client	*first;		/* doubly linked list of clients */
    struct server	*servers;	/* array of tcp/pipe socket servers */
    unsigned int	nservers;	/* count of entries in server array */
    unsigned int	redisetup;	/* is Redis slots information setup */
    struct client	*pending_writes;
#ifdef HAVE_OPENSSL
    SSL_CTX		*ssl;
    __pmSecureConfig	tls;
#endif
    redisSlots		*slots;		/* mapping of Redis keys to servers */
    struct servlet	*servlets;	/* linked list of http URL handlers */
    mmv_registry_t	*metrics[NUM_REGISTRY];	/* performance metrics */
    pmAtomValue     *values[NUM_VALUES]; /* local metric values*/
    void            *map; /* MMV mapped metric values handle */
    struct dict		*config;	/* configuration dictionary */
    uv_loop_t		*events;	/* global, async event loop */
    uv_callback_t	write_callbacks;
    uv_mutex_t		write_mutex;	/* protects pending writes */
} proxy_t;

extern void proxylog(pmLogLevel, sds, void *);
extern mmv_registry_t *proxymetrics(struct proxy *, enum proxy_registry);
extern void proxymetrics_close(struct proxy *, enum proxy_registry);

extern void on_proxy_flush(uv_handle_t *);
extern void on_client_write(uv_write_t *, int);
extern void on_buffer_alloc(uv_handle_t *, size_t, uv_buf_t *);

extern void client_write(struct client *, sds, sds);
extern int client_is_closed(struct client *);
extern void client_close(struct client *);
extern void client_get(struct client *);
extern void client_put(struct client *);

extern void on_protocol_read(uv_stream_t *, ssize_t, const uv_buf_t *);

#ifdef HAVE_OPENSSL
extern void secure_client_write(struct client *, struct stream_write_baton *);
extern void on_secure_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_secure_client_write(struct client *);
extern void on_secure_client_close(struct client *);
#else
#define secure_client_write(c,w)	do { (void)(c); } while (0)
#define on_secure_client_read(p,c,s,b)	do { (void)(p); } while (0)
#define on_secure_client_write(c)	do { (void)(c); } while (0)
#define on_secure_client_close(c)	do { (void)(c); } while (0)
#endif

extern void on_redis_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_redis_client_write(struct client *);
extern void on_redis_client_close(struct client *);

extern void on_http_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_http_client_write(struct client *);
extern void on_http_client_close(struct client *);

extern void on_pcp_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);
extern void on_pcp_client_write(struct client *);
extern void on_pcp_client_close(struct client *);

#ifdef HAVE_OPENSSL
extern void flush_secure_module(struct proxy *);
extern void setup_secure_module(struct proxy *);
extern void close_secure_module(struct proxy *);
#else
#define flush_secure_module(p)	do { (void)(p); } while (0)
#define setup_secure_module(p)	do { (void)(p); } while (0)
#define close_secure_module(p)	do { (void)(p); } while (0)
#endif

extern void setup_redis_module(struct proxy *);
extern void close_redis_module(struct proxy *);

extern void setup_http_module(struct proxy *);
extern void close_http_module(struct proxy *);

extern void setup_pcp_module(struct proxy *);
extern void close_pcp_module(struct proxy *);

extern void setup_modules(struct proxy *);

#endif	/* PROXY_SERVER_H */
