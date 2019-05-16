/*
 * Copyright (c) 2019 Red Hat.
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2011, Pieter Noordhuis <pcnoordhuis at gmail dot com>
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
#include "sslio.h"
#include "redis.h"

/**
 * Callback used for debugging
 */
static void sslLogCallback(const SSL *ssl, int where, int ret) {
    const char *retstr = "";
    int should_log = 1;
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

void redisFreeSsl(redisSsl *ssl){
    if (ssl->ctx) {
        SSL_CTX_free(ssl->ctx);
    }
    if (ssl->ssl) {
        SSL_free(ssl->ssl);
    }
    free(ssl);
}

int redisSslCreate(redisContext *c, const char *capath, const char *certpath,
                   const char *keypath, const char *servername) {
    static int isInit = 0;
    redisSsl *s;

    if (!isInit) {
        isInit = 1;
        SSL_library_init();
    }
    assert(!c->ssl);
    s = c->ssl = calloc(1, sizeof(*c->ssl));
    s->ctx = SSL_CTX_new(SSLv23_client_method());
    SSL_CTX_set_info_callback(s->ctx, sslLogCallback);
    SSL_CTX_set_mode(s->ctx, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
    SSL_CTX_set_options(s->ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3);
    SSL_CTX_set_verify(s->ctx, SSL_VERIFY_PEER, NULL);

    if ((certpath != NULL && keypath == NULL) || (keypath != NULL && certpath == NULL)) {
        __redisSetError(c, REDIS_ERR, "certpath and keypath must be specified together");
        return REDIS_ERR;
    }

    if (capath) {
        if (!SSL_CTX_load_verify_locations(s->ctx, capath, NULL)) {
            __redisSetError(c, REDIS_ERR, "Invalid CA certificate");
            return REDIS_ERR;
        }
    }
    if (certpath) {
        if (!SSL_CTX_use_certificate_chain_file(s->ctx, certpath)) {
            __redisSetError(c, REDIS_ERR, "Invalid client certificate");
            return REDIS_ERR;
        }
        if (!SSL_CTX_use_PrivateKey_file(s->ctx, keypath, SSL_FILETYPE_PEM)) {
            __redisSetError(c, REDIS_ERR, "Invalid client key");
            return REDIS_ERR;
        }
    }

    s->ssl = SSL_new(s->ctx);
    if (!s->ssl) {
        __redisSetError(c, REDIS_ERR, "Couldn't create new SSL instance");
        return REDIS_ERR;
    }
    if (servername) {
        if (!SSL_set_tlsext_host_name(s->ssl, servername)) {
            __redisSetError(c, REDIS_ERR, "Couldn't set server name indication");
            return REDIS_ERR;
        }
    }

    SSL_set_fd(s->ssl, c->fd);
    SSL_set_connect_state(s->ssl);

    c->flags |= REDIS_SSL;
    int rv = SSL_connect(c->ssl->ssl);
    if (rv == 1) {
        return REDIS_OK;
    }

    rv = SSL_get_error(s->ssl, rv);
    if (((c->flags & REDIS_BLOCK) == 0) &&
        (rv == SSL_ERROR_WANT_READ || rv == SSL_ERROR_WANT_WRITE)) {
        return REDIS_OK;
    }

    if (c->err == 0) {
        __redisSetError(c, REDIS_ERR_IO, "SSL_connect() failed");
    }
    return REDIS_ERR;
}

static int maybeCheckWant(redisSsl *rssl, int rv) {
    /**
     * If the error is WANT_READ or WANT_WRITE, the appropriate flags are set
     * and true is returned. False is returned otherwise
     */
    if (rv == SSL_ERROR_WANT_READ) {
        rssl->wantRead = 1;
        return 1;
    } else if (rv == SSL_ERROR_WANT_WRITE) {
        rssl->pendingWrite = 1;
        return 1;
    } else {
        return 0;
    }
}

int redisSslRead(redisContext *c, char *buf, size_t bufcap) {
    int nread = SSL_read(c->ssl->ssl, buf, bufcap);
    if (nread > 0) {
        return nread;
    } else if (nread == 0) {
        __redisSetError(c, REDIS_ERR_EOF, "Server closed the connection");
        return -1;
    } else {
        int err = SSL_get_error(c->ssl->ssl, nread);
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
                    msg = "Timed out";
                }
                __redisSetError(c, REDIS_ERR_IO, msg);
                return -1;
            }
        }

        /**
         * We can very well get an EWOULDBLOCK/EAGAIN, however
         */
        if (maybeCheckWant(c->ssl, err)) {
            return 0;
        } else {
            __redisSetError(c, REDIS_ERR_IO, NULL);
            return -1;
        }
    }
}

int redisSslWrite(redisContext *c) {
    size_t len = c->ssl->lastLen ? c->ssl->lastLen : sdslen(c->obuf);
    int rv = SSL_write(c->ssl->ssl, c->obuf, len);

    if (rv > 0) {
        c->ssl->lastLen = 0;
    } else if (rv < 0) {
        c->ssl->lastLen = len;

        int err = SSL_get_error(c->ssl->ssl, rv);
        if ((c->flags & REDIS_BLOCK) == 0 && maybeCheckWant(c->ssl, err)) {
            return 0;
        } else {
            __redisSetError(c, REDIS_ERR_IO, NULL);
            return -1;
        }
    }
    return rv;
}
