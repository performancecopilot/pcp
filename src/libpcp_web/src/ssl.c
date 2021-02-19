/*
 * Copyright (c) 2019-2020 Red Hat.
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2011, Pieter Noordhuis <pcnoordhuis at gmail dot com>
 * Copyright (c) 2019 Redis Labs.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Redis nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <pthread.h>
#include <assert.h>
#include <errno.h>
#include "pmapi.h"
#include "redis.h"

#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct redisSSLContext {
    /**
     * OpenSSL SSL_CTX; It is optional and will not be set when using
     * user-supplied SSL.
     */
    SSL_CTX		*ctx;

    /**
     * OpenSSL SSL object.
     */
    SSL			*ssl;

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
} redisSSLContext;

/* Forward declaration */
redisContextFuncs redisContextSSLFuncs;

#ifdef SSL_TRACE
/**
 * Callback used for debugging
 */
static void
sslLogCallback(const SSL *ssl, int where, int ret)
{
    const char	*retstr;
    int		should_log = 0;
    /* Ignore low-level SSL stuff */

    if (where & SSL_CB_ALERT) {
        should_log = 1;
    }
    if (where == SSL_CB_HANDSHAKE_START || where == SSL_CB_HANDSHAKE_DONE) {
        should_log = 1;
    }
    if ((where & SSL_CB_EXIT) && ret == 0) {
        should_log = 1;
    }

    if (!should_log) {
        return;
    }

    retstr = SSL_alert_type_string(ret);
    printf("ST(0x%x). %s. R(0x%x)%s\n", where, SSL_state_string_long(ssl), ret, retstr);

    if (where == SSL_CB_HANDSHAKE_DONE) {
        printf("Using SSL version %s. Cipher=%s\n", SSL_get_version(ssl), SSL_get_cipher_name(ssl));
    }
}
#endif

static int
redisSSLConnect(redisContext *c, SSL_CTX *ctx, SSL *ssl)
{
    redisSSLContext	*rssl;
    int			sts;

    if (c->privdata) {
        __redisSetError(c, REDIS_ERR_OTHER, "redisContext was already associated");
        return REDIS_ERR;
    }
    if ((c->privdata = calloc(1, sizeof(redisSSLContext))) == NULL)
        return REDIS_ERR;

    c->funcs = &redisContextSSLFuncs;
    rssl = c->privdata;

    rssl->ctx = ctx;
    rssl->ssl = ssl;

    SSL_set_mode(rssl->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_set_fd(rssl->ssl, c->fd);
    SSL_set_connect_state(rssl->ssl);

    ERR_clear_error();
    sts = SSL_connect(rssl->ssl);
    if (sts == 1)
        return REDIS_OK;

    sts = SSL_get_error(rssl->ssl, sts);
    if (((c->flags & REDIS_BLOCK) == 0) &&
        (sts == SSL_ERROR_WANT_READ || sts == SSL_ERROR_WANT_WRITE))
        return REDIS_OK;

    if (c->err == 0) {
        char err[512];
        if (sts == SSL_ERROR_SYSCALL)
            pmsprintf(err, sizeof(err)-1, "SSL_connect failed: %s",
                    strerror(errno));
        else {
            unsigned long e = ERR_peek_last_error();
            pmsprintf(err, sizeof(err)-1, "SSL_connect failed: %s",
                    ERR_reason_error_string(e));
        }
        __redisSetError(c, REDIS_ERR_IO, err);
    }
    return REDIS_ERR;
}

int
redisInitiateSSL(redisContext *c, SSL *ssl)
{
    return redisSSLConnect(c, NULL, ssl);
}

int
redisSecureConnection(redisContext *c, const char *capath,
        const char *certpath, const char *keypath, const char *servername)
{
    SSL_CTX		*ctx = NULL;
    SSL			*ssl = NULL;
    static int		isInit = 0;

    /* Initialize global OpenSSL stuff */
    if (!isInit) {
        isInit = 1;
        SSL_library_init();
    }
    ctx = SSL_CTX_new(SSLv23_client_method());
    if (!ctx) {
        __redisSetError(c, REDIS_ERR_OTHER, "Failed to create SSL_CTX");
        goto error;
    }

#ifdef SSL_TRACE
    SSL_CTX_set_info_callback(ctx, sslLogCallback);
#endif
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    if ((certpath != NULL && keypath == NULL) || (keypath != NULL && certpath == NULL)) {
        __redisSetError(c, REDIS_ERR, "certpath and keypath must be specified together");
        goto error;
    }

    if (capath) {
        if (!SSL_CTX_load_verify_locations(ctx, capath, NULL)) {
            __redisSetError(c, REDIS_ERR, "Invalid CA certificate");
	    goto error;
        }
    }
    if (certpath) {
        if (!SSL_CTX_use_certificate_chain_file(ctx, certpath)) {
            __redisSetError(c, REDIS_ERR, "Invalid client certificate");
	    goto error;
        }
        if (!SSL_CTX_use_PrivateKey_file(ctx, keypath, SSL_FILETYPE_PEM)) {
            __redisSetError(c, REDIS_ERR, "Invalid client key");
	    goto error;
        }
    }

    ssl = SSL_new(ctx);
    if (!ssl) {
        __redisSetError(c, REDIS_ERR, "Couldn't create new SSL instance");
        goto error;
    }
    if (servername) {
        if (!SSL_set_tlsext_host_name(ssl, servername)) {
            __redisSetError(c, REDIS_ERR, "Couldn't set server name indication");
	    goto error;
        }
    }

    return redisSSLConnect(c, ctx, ssl);

error:
    if (ssl) SSL_free(ssl);
    if (ctx) SSL_CTX_free(ctx);
    return REDIS_ERR;
}

static int
maybeCheckWant(redisSSLContext *rssl, int sts)
{
    /**
     * If the error is WANT_READ or WANT_WRITE, the appropriate flags are set
     * and true is returned. False is returned otherwise
     */
    if (sts == SSL_ERROR_WANT_READ) {
        rssl->wantRead = 1;
        return 1;
    } else if (sts == SSL_ERROR_WANT_WRITE) {
        rssl->pendingWrite = 1;
        return 1;
    } else {
        return 0;
    }
}

/**
 * Implementation of redisContextFuncs for SSL connections.
 */

static void
redisSSLFreeContext(void *privdata)
{
    redisSSLContext	*rsc = privdata;

    if (!rsc) return;
    if (rsc->ssl) {
        SSL_free(rsc->ssl);
        rsc->ssl = NULL;
    }
    if (rsc->ctx) {
        SSL_CTX_free(rsc->ctx);
        rsc->ctx = NULL;
    }
    free(rsc);
}

static int
redisSSLRead(redisContext *c, char *buf, size_t bufcap)
{
    redisSSLContext	*rssl = c->privdata;
    int			nread = SSL_read(rssl->ssl, buf, bufcap);

    if (nread > 0) {
        return nread;
    } else if (nread == 0) {
        __redisSetError(c, REDIS_ERR_EOF, "Server closed the connection");
        return -1;
    } else {
        int err = SSL_get_error(rssl->ssl, nread);
        if (c->flags & REDIS_BLOCK) {
            /**
             * In blocking mode, we should never end up in a situation where
             * we get an error without it being an actual error, except
             * in the case of EINTR, which can be spuriously received from
             * debuggers or whatever.
             */
            if (errno == EINTR) {
                return 0;
            } else {
                const char *msg = NULL;
                if (errno == EAGAIN) {
                    msg = "Resource temporarily unavailable";
                }
                __redisSetError(c, REDIS_ERR_IO, msg);
                return -1;
            }
        }

        /**
         * We can very well get an EWOULDBLOCK/EAGAIN, however
         */
        if (maybeCheckWant(rssl, err)) {
            return 0;
        } else {
            __redisSetError(c, REDIS_ERR_IO, NULL);
            return -1;
        }
    }
}

static int
redisSSLWrite(redisContext *c)
{
    redisSSLContext	*rssl = c->privdata;
    size_t		len = rssl->lastLen ? rssl->lastLen : sdslen(c->obuf);
    int			sts = SSL_write(rssl->ssl, c->obuf, len);

    if (sts > 0) {
        rssl->lastLen = 0;
    } else if (sts < 0) {
        rssl->lastLen = len;

        int err = SSL_get_error(rssl->ssl, sts);
        if ((c->flags & REDIS_BLOCK) == 0 && maybeCheckWant(rssl, err)) {
            return 0;
        } else {
            __redisSetError(c, REDIS_ERR_IO, NULL);
            return -1;
        }
    }
    return sts;
}

static void
redisSSLAsyncRead(redisAsyncContext *ac)
{
    int			sts;
    redisSSLContext	*rssl = ac->c.privdata;
    redisContext	*c = &ac->c;

    rssl->wantRead = 0;

    if (rssl->pendingWrite) {
        int done;

        /* This is probably just a write event */
        rssl->pendingWrite = 0;
        sts = redisBufferWrite(c, &done);
        if (sts == REDIS_ERR) {
            __redisAsyncDisconnect(ac);
            return;
        } else if (!done) {
            REDIS_EV_ADD_WRITE(ac);
        }
    }

    sts = redisBufferRead(c);
    if (sts == REDIS_ERR) {
        __redisAsyncDisconnect(ac);
    } else {
        REDIS_EV_ADD_READ(ac);
        redisProcessCallBacks(ac);
    }
}

static void
redisSSLAsyncWrite(redisAsyncContext *ac)
{
    int			sts, done = 0;
    redisSSLContext	*rssl = ac->c.privdata;
    redisContext	*c = &ac->c;

    rssl->pendingWrite = 0;
    sts = redisBufferWrite(c, &done);
    if (sts == REDIS_ERR) {
        __redisAsyncDisconnect(ac);
        return;
    }

    if (!done) {
        if (rssl->wantRead) {
            /* Need to read-before-write */
            rssl->pendingWrite = 1;
            REDIS_EV_DEL_WRITE(ac);
        } else {
            /* No extra reads needed, just need to write more */
            REDIS_EV_ADD_WRITE(ac);
        }
    } else {
        /* Already done! */
        REDIS_EV_DEL_WRITE(ac);
    }

    /* Always reschedule a read */
    REDIS_EV_ADD_READ(ac);
}

redisContextFuncs redisContextSSLFuncs = {
    .free_privdata = redisSSLFreeContext,
    .async_read = redisSSLAsyncRead,
    .async_write = redisSSLAsyncWrite,
    .read = redisSSLRead,
    .write = redisSSLWrite
};

