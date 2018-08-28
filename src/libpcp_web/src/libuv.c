/*
 * Copyright (c) 2017-2018 Red Hat.
 * Copyright (c) 2018 Challa Venkata Naga Prajwal <cvnprajwal at gmail dot com>
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

#include "libuv.h"
#include "uv.h"

typedef struct redisLibuvEvents {
    redisAsyncContext       *context;
    uv_poll_t               handle;
    int                     events;
} redisLibuvEvents;

static void
redisLibuvPoll(uv_poll_t *handle, int status, int events)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)handle->data;

    if (status != 0)
        return;
    if (p->context != NULL && (events & UV_READABLE))
        redisAsyncHandleRead(p->context);
    if (p->context != NULL && (events & UV_WRITABLE))
        redisAsyncHandleWrite(p->context);
}

static void
redisLibuvAddRead(void *privdata)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)privdata;

    p->events |= UV_READABLE;
    uv_poll_start(&p->handle, p->events, redisLibuvPoll);
}

static void
redisLibuvDelRead(void *privdata)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)privdata;

    p->events &= ~UV_READABLE;
    if (p->events)
        uv_poll_start(&p->handle, p->events, redisLibuvPoll);
    else
        uv_poll_stop(&p->handle);
}

static void
redisLibuvAddWrite(void *privdata)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)privdata;

    p->events |= UV_WRITABLE;
    uv_poll_start(&p->handle, p->events, redisLibuvPoll);
}

static void
redisLibuvDelWrite(void *privdata)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)privdata;

    p->events &= ~UV_WRITABLE;
    if (p->events)
        uv_poll_start(&p->handle, p->events, redisLibuvPoll);
    else
        uv_poll_stop(&p->handle);
}

static void
on_close(uv_handle_t *handle)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)handle->data;

    free(p);
}

static void
redisLibuvCleanup(void *privdata)
{
    redisLibuvEvents	*p = (redisLibuvEvents *)privdata;

    p->context = NULL; /* indicates the context might no longer exist */
    uv_close((uv_handle_t *)&p->handle, on_close);
}

static int
redisLibuvAttach(redisAsyncContext *ac, uv_loop_t *loop)
{
    redisLibuvEvents	*p;

    if (ac->ev.data != NULL)
        return REDIS_ERR;

    ac->ev.addRead  = redisLibuvAddRead;
    ac->ev.delRead  = redisLibuvDelRead;
    ac->ev.addWrite = redisLibuvAddWrite;
    ac->ev.delWrite = redisLibuvDelWrite;
    ac->ev.cleanup  = redisLibuvCleanup;

    if ((p = (redisLibuvEvents *)calloc(1, sizeof(*p))) == NULL)
        return REDIS_ERR;

    if (uv_poll_init(loop, &p->handle, ac->c.fd) != 0)
        return REDIS_ERR;

    ac->ev.data    = p;
    p->handle.data = p;
    p->context     = ac;
    return REDIS_OK;
}

int
redisEventAttach(redisAsyncContext *ac, void *privdata)
{
    return redisLibuvAttach(ac, (uv_loop_t *)privdata);
}
