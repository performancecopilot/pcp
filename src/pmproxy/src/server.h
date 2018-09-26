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
#ifndef PROXY_SERVER_H
#define PROXY_SERVER_H

#include <uv.h>
#include "pmapi.h"
#include "pmwebapi.h"
#include "pmproxy.h"
#include "libpcp.h"
#include "slots.h"

typedef enum stream_protocol {
    STREAM_UNKNOWN	= 0,
    STREAM_SECURE	= 0x1,
    STREAM_REDIS	= 0x2,
    STREAM_PCP		= 0x8,
} stream_protocol;

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

typedef struct redis_client {
    redisReader		*reader;
    uv_write_t		writereq;
    uv_buf_t		writebuf;
    uv_buf_t		readbuf;
} redis_client;

typedef struct client {
    struct stream	stream;
    stream_protocol	protocol;
    union {
	redis_client	redis;
    } u;
    struct client	*next;
    struct client	*prev;
} client;

typedef struct server {
    struct stream	stream;
    __pmServerPresence	*presence;
} server;

typedef struct proxy {
    uv_loop_t		*events;
    struct client	*head;		/* doubly linked list of clients */
    struct client	*tail;
    struct server	*servers;	/* array of tcp/pipe socket servers */
    int			nservers;	/* count of entries in server array */
    int			redisetup;	/* is Redis slots information setup */
    sds			redishost;	/* initial Redis host specification */
    redisSlots		*slots;
} proxy;

extern void proxylog(pmLogLevel, sds, void *);
extern void on_client_close(uv_handle_t *);

extern void setup_redis_proxy(struct proxy *);
extern void on_redis_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);

extern void on_pcp_client_read(struct proxy *, struct client *,
				ssize_t, const uv_buf_t *);

#endif	/* PROXY_SERVER_H */
