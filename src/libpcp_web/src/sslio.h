/*
 * Copyright (c) 2019 Red Hat.
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
#ifndef SERIES_SSLIO_H
#define SERIES_SSLIO_H

#include "pmapi.h"
#include "redis.h"

#if defined(HAVE_OPENSSL)
#include <openssl/ssl.h>

typedef struct redisSsl {
    SSL			*ssl;
    SSL_CTX		*ctx;

    /**
     * SSL_write() requires to be called again with the same arguments it was
     * previously called with in the event of an SSL_read/SSL_write situation
     */
    size_t		lastLen;

    /** Whether the SSL layer requires read (possibly before a write) */
    unsigned int	wantRead;

    /**
     * Whether a write was requested prior to a read. If set, the write()
     * should resume whenever a read takes place, if possible
     */
    unsigned int	pendingWrite;
} redisSsl;

extern int redisSslCreate(struct redisContext *, const char *,
			const char *, const char *, const char *);

extern int redisSslRead(struct redisContext *, char *, size_t);
extern int redisSslWrite(struct redisContext *);
extern void redisFreeSsl(struct redisSsl *);

#else
typedef struct redisSsl {
    unsigned int	wantRead;
    unsigned int	pendingWrite;
} redisSsl;
#define redisSslCreate(c, ca, cert, key, servername) (REDIS_ERR)
#define redisSslRead(c, s, n) (-1)
#define redisSslWrite(c) (-1)
#define redisFreeSsl(ssl) do { } while (0)
#endif

#endif /* SERIES_SSLIO_H */
