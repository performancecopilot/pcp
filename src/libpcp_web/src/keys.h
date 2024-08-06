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

#include <hiredis/hiredis.h>
#include <hiredis/async.h>

#define RESP_OK			REDIS_OK
#define RESP_ERR		REDIS_ERR
#define RESP_ERR_IO		REDIS_ERR_IO
#define RESP_CONN_UNIX		REDIS_CONN_UNIX

/*
 * Unfortunately there is no error code for these errors to use.
 * In LOADING case, the full error contains the key server name -
 * so here we split the check into the two guaranteed substrings.
 */
#define RESP_ELOADING	"LOADING"
#define RESP_ELOADDATA	"loading the dataset in memory"
#define RESP_ENOCLUSTER	"ERR This instance has cluster support disabled"

#define RESP_KEEPALIVE_INTERVAL REDIS_KEEPALIVE_INTERVAL

/*
 * Protocol reply types
 */
#define RESP_REPLY_STRING	REDIS_REPLY_STRING
#define RESP_REPLY_ARRAY 	REDIS_REPLY_ARRAY
#define RESP_REPLY_BOOL 	REDIS_REPLY_BOOL
#define RESP_REPLY_DOUBLE	REDIS_REPLY_DOUBLE
#define RESP_REPLY_INTEGER	REDIS_REPLY_INTEGER
#define RESP_REPLY_MAP		REDIS_REPLY_MAP
#define RESP_REPLY_NIL		REDIS_REPLY_NIL
#define RESP_REPLY_SET		REDIS_REPLY_SET
#define RESP_REPLY_STATUS	REDIS_REPLY_STATUS
#define RESP_REPLY_ERROR	REDIS_REPLY_ERROR

#define respReply redisReply
#define respReader redisReader
#define respReaderCreate redisReaderCreate
#define respReaderFeed redisReaderFeed
#define respReaderFree redisReaderFree
#define respReaderGetReply redisReaderGetReply

#define keysAsyncContext redisAsyncContext
#define keysAsyncEnableKeepAlive redisAsyncEnableKeepAlive
#define keysKeepAlive redisKeepAlive

#define keyClusterAsyncFree redisClusterAsyncFree
#define keyClusterAsyncContext redisClusterAsyncContext
#define keyClusterAsyncContextInit redisClusterAsyncContextInit
#define keyClusterCallbackFn redisClusterCallbackFn
#define keyClusterConnect2 redisClusterConnect2
#define keyClusterAsyncDisconnect redisClusterAsyncDisconnect
#define keyClusterSetOptionAddNodes redisClusterSetOptionAddNodes
#define keyClusterSetOptionPassword redisClusterSetOptionPassword
#define keyClusterSetOptionUsername redisClusterSetOptionUsername
#define keyClusterSetOptionConnectTimeout redisClusterSetOptionConnectTimeout
#define keyClusterSetOptionTimeout redisClusterSetOptionTimeout
#define keyClusterLibuvAttach redisClusterLibuvAttach
#define keyClusterAsyncSetConnectCallback redisClusterAsyncSetConnectCallback
#define keyClusterAsyncSetDisconnectCallback redisClusterAsyncSetDisconnectCallback
#define keyClusterAsyncFormattedCommand redisClusterAsyncFormattedCommand
#define keyClusterAsyncFormattedCommandToNode redisClusterAsyncFormattedCommandToNode

extern const char *resp_reply_type(respReply *);
extern int keysAsyncEnableKeepAlive(keysAsyncContext *);

#define RESP_ENOCLUSTER	"ERR This instance has cluster support disabled"
#define RESP_ESTREAMXADD "ERR The ID specified in XADD is equal or smaller than the target stream top item"
#define RESP_EDROPINDEX	"Index already exists. Drop it first!"	/* search module */

#endif /* RESP_KEYS_H */
