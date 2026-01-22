/*
 * Copyright (c) 2017-2020,2024 Red Hat.
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
#ifndef RESP_KEYS_H
#define RESP_KEYS_H

#include <valkey/valkey.h>
#include <valkey/async.h>
#include <valkey/cluster.h>

#define RESP_OK			VALKEY_OK
#define RESP_ERR		VALKEY_ERR
#define RESP_ERR_IO		VALKEY_ERR_IO
#define RESP_CONN_UNIX		VALKEY_CONN_UNIX

/*
 * Unfortunately there is no error code for these errors to use.
 * In LOADING case, the full error contains the key server name -
 * so here we split the check into the two guaranteed substrings.
 */
#define RESP_ELOADING	"LOADING"
#define RESP_ELOADDATA	"loading the dataset in memory"
#define RESP_ENOCLUSTER	"ERR This instance has cluster support disabled"

#define RESP_KEEPALIVE_INTERVAL VALKEY_KEEPALIVE_INTERVAL

/*
 * Protocol reply types
 */
#define RESP_REPLY_STRING	VALKEY_REPLY_STRING
#define RESP_REPLY_ARRAY 	VALKEY_REPLY_ARRAY
#define RESP_REPLY_BOOL 	VALKEY_REPLY_BOOL
#define RESP_REPLY_DOUBLE	VALKEY_REPLY_DOUBLE
#define RESP_REPLY_INTEGER	VALKEY_REPLY_INTEGER
#define RESP_REPLY_MAP		VALKEY_REPLY_MAP
#define RESP_REPLY_NIL		VALKEY_REPLY_NIL
#define RESP_REPLY_SET		VALKEY_REPLY_SET
#define RESP_REPLY_STATUS	VALKEY_REPLY_STATUS
#define RESP_REPLY_ERROR	VALKEY_REPLY_ERROR

#define respReply valkeyReply
#define respReader valkeyReader
#define respReaderCreate valkeyReaderCreate
#define respReaderFeed valkeyReaderFeed
#define respReaderFree valkeyReaderFree
#define respReaderGetReply valkeyReaderGetReply

#define keysAsyncContext valkeyAsyncContext
#define keysAsyncEnableKeepAlive valkeyAsyncEnableKeepAlive
#define keysKeepAlive valkeyKeepAlive

#define keyClusterAsyncFree valkeyClusterAsyncFree
#define keyClusterAsyncContext valkeyClusterAsyncContext
#define keyClusterAsyncContextInit valkeyClusterAsyncConnectWithOptions
#define keyClusterCallbackFn valkeyClusterCallbackFn
#define keyClusterConnect2 valkeyClusterConnectWithOptions
#define keyClusterAsyncDisconnect valkeyClusterAsyncDisconnect
#define keyClusterSetOptionAddNodes valkeyClusterOptions.initial_nodes
#define keyClusterSetOptionPassword valkeyClusterOptions.password
#define keyClusterSetOptionUsername valkeyClusterOptions.username
#define keyClusterSetOptionConnectTimeout valkeyClusterOptions.connect_timeout
#define keyClusterSetOptionTimeout valkeyClusterSetOptionTimeout
#define keyLibuvAttachAdapter valkeyLibuvAttachAdapter
#define keyClusterAsyncSetConnectCallback valkeyClusterOptions.async_connect_callback
#define keyClusterAsyncSetDisconnectCallback valkeyClusterOptions.async_disconnect_callback
#define keyClusterAsyncFormattedCommand valkeyClusterAsyncFormattedCommand
#define keyClusterAsyncFormattedCommandToNode valkeyClusterAsyncFormattedCommandToNode

/* valkey cluster options */
#define KEYOPT_BLOCKING_INITIAL_UPDATE VALKEY_OPT_BLOCKING_INITIAL_UPDATE

extern const char *resp_reply_type(respReply *);
extern int keysAsyncEnableKeepAlive(keysAsyncContext *);

#define RESP_ENOCLUSTER	"ERR This instance has cluster support disabled"
#define RESP_ESTREAMXADD "ERR The ID specified in XADD is equal or smaller than the target stream top item"
#define RESP_EDROPINDEX	"Index already exists. Drop it first!"	/* search module */

#endif /* RESP_KEYS_H */
