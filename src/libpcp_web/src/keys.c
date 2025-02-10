/*
 * Copyright (c) 2017-2020,2024, Red Hat.
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
#include <hiredis/net.h>
#include "keys.h"

const char *
resp_reply_type(respReply *reply)
{
    if (reply == NULL)
        return "none";
    switch (reply->type) {
    case RESP_REPLY_STRING:
        return "string";
    case RESP_REPLY_ARRAY:
        return "array";
    case RESP_REPLY_BOOL:
        return "bool";
    case RESP_REPLY_DOUBLE:
        return "double";
    case RESP_REPLY_INTEGER:
        return "integer";
    case RESP_REPLY_MAP:
        return "map";
    case RESP_REPLY_NIL:
        return "nil";
    case RESP_REPLY_SET:
        return "set";
    case RESP_REPLY_STATUS:
        return "status";
    case RESP_REPLY_ERROR:
        return "error";
    default:
        break;
    }
    return "unknown";
}

/* Enable connection KeepAlive. */
int
keysAsyncEnableKeepAlive(keysAsyncContext *ac)
{
    if (keysKeepAlive(&ac->c, RESP_KEEPALIVE_INTERVAL) != RESP_OK)
        return RESP_ERR;
    return RESP_OK;
}
