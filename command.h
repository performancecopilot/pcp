/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020, Nick <heronr1 at gmail dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
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

#ifndef __COMMAND_H_
#define __COMMAND_H_

#include <stdint.h>

#include "adlist.h"
#include <hiredis/hiredis.h>

typedef enum cmd_parse_result {
    CMD_PARSE_OK,     /* parsing ok */
    CMD_PARSE_ENOMEM, /* out of memory */
    CMD_PARSE_ERROR,  /* parsing error */
    CMD_PARSE_REPAIR, /* more to parse -> repair parsed & unparsed data */
    CMD_PARSE_AGAIN,  /* incomplete -> parse again */
} cmd_parse_result_t;

#define CMD_TYPE_CODEC(ACTION)                                                 \
    ACTION(UNKNOWN)                                                            \
    ACTION(REQ_REDIS_DEL) /* redis commands - keys */                          \
    ACTION(REQ_REDIS_EXISTS)                                                   \
    ACTION(REQ_REDIS_EXPIRE)                                                   \
    ACTION(REQ_REDIS_EXPIREAT)                                                 \
    ACTION(REQ_REDIS_PEXPIRE)                                                  \
    ACTION(REQ_REDIS_PEXPIREAT)                                                \
    ACTION(REQ_REDIS_PERSIST)                                                  \
    ACTION(REQ_REDIS_PTTL)                                                     \
    ACTION(REQ_REDIS_SORT)                                                     \
    ACTION(REQ_REDIS_TTL)                                                      \
    ACTION(REQ_REDIS_TYPE)                                                     \
    ACTION(REQ_REDIS_APPEND) /* redis requests - string */                     \
    ACTION(REQ_REDIS_BITCOUNT)                                                 \
    ACTION(REQ_REDIS_DECR)                                                     \
    ACTION(REQ_REDIS_DECRBY)                                                   \
    ACTION(REQ_REDIS_DUMP)                                                     \
    ACTION(REQ_REDIS_GET)                                                      \
    ACTION(REQ_REDIS_GETBIT)                                                   \
    ACTION(REQ_REDIS_GETRANGE)                                                 \
    ACTION(REQ_REDIS_GETSET)                                                   \
    ACTION(REQ_REDIS_INCR)                                                     \
    ACTION(REQ_REDIS_INCRBY)                                                   \
    ACTION(REQ_REDIS_INCRBYFLOAT)                                              \
    ACTION(REQ_REDIS_MGET)                                                     \
    ACTION(REQ_REDIS_MSET)                                                     \
    ACTION(REQ_REDIS_PSETEX)                                                   \
    ACTION(REQ_REDIS_RESTORE)                                                  \
    ACTION(REQ_REDIS_SET)                                                      \
    ACTION(REQ_REDIS_SETBIT)                                                   \
    ACTION(REQ_REDIS_SETEX)                                                    \
    ACTION(REQ_REDIS_SETNX)                                                    \
    ACTION(REQ_REDIS_SETRANGE)                                                 \
    ACTION(REQ_REDIS_STRLEN)                                                   \
    ACTION(REQ_REDIS_HDEL) /* redis requests - hashes */                       \
    ACTION(REQ_REDIS_HEXISTS)                                                  \
    ACTION(REQ_REDIS_HGET)                                                     \
    ACTION(REQ_REDIS_HGETALL)                                                  \
    ACTION(REQ_REDIS_HINCRBY)                                                  \
    ACTION(REQ_REDIS_HINCRBYFLOAT)                                             \
    ACTION(REQ_REDIS_HKEYS)                                                    \
    ACTION(REQ_REDIS_HLEN)                                                     \
    ACTION(REQ_REDIS_HMGET)                                                    \
    ACTION(REQ_REDIS_HMSET)                                                    \
    ACTION(REQ_REDIS_HSET)                                                     \
    ACTION(REQ_REDIS_HSETNX)                                                   \
    ACTION(REQ_REDIS_HSCAN)                                                    \
    ACTION(REQ_REDIS_HVALS)                                                    \
    ACTION(REQ_REDIS_LINDEX) /* redis requests - lists */                      \
    ACTION(REQ_REDIS_LINSERT)                                                  \
    ACTION(REQ_REDIS_LLEN)                                                     \
    ACTION(REQ_REDIS_LPOP)                                                     \
    ACTION(REQ_REDIS_LPUSH)                                                    \
    ACTION(REQ_REDIS_LPUSHX)                                                   \
    ACTION(REQ_REDIS_LRANGE)                                                   \
    ACTION(REQ_REDIS_LREM)                                                     \
    ACTION(REQ_REDIS_LSET)                                                     \
    ACTION(REQ_REDIS_LTRIM)                                                    \
    ACTION(REQ_REDIS_PFADD) /* redis requests - hyperloglog */                 \
    ACTION(REQ_REDIS_PFCOUNT)                                                  \
    ACTION(REQ_REDIS_PFMERGE)                                                  \
    ACTION(REQ_REDIS_RPOP)                                                     \
    ACTION(REQ_REDIS_RPOPLPUSH)                                                \
    ACTION(REQ_REDIS_RPUSH)                                                    \
    ACTION(REQ_REDIS_RPUSHX)                                                   \
    ACTION(REQ_REDIS_SADD) /* redis requests - sets */                         \
    ACTION(REQ_REDIS_SCARD)                                                    \
    ACTION(REQ_REDIS_SDIFF)                                                    \
    ACTION(REQ_REDIS_SDIFFSTORE)                                               \
    ACTION(REQ_REDIS_SINTER)                                                   \
    ACTION(REQ_REDIS_SINTERSTORE)                                              \
    ACTION(REQ_REDIS_SISMEMBER)                                                \
    ACTION(REQ_REDIS_SMEMBERS)                                                 \
    ACTION(REQ_REDIS_SMOVE)                                                    \
    ACTION(REQ_REDIS_SPOP)                                                     \
    ACTION(REQ_REDIS_SRANDMEMBER)                                              \
    ACTION(REQ_REDIS_SREM)                                                     \
    ACTION(REQ_REDIS_SUNION)                                                   \
    ACTION(REQ_REDIS_SUNIONSTORE)                                              \
    ACTION(REQ_REDIS_SSCAN)                                                    \
    ACTION(REQ_REDIS_XACK)                                                     \
    ACTION(REQ_REDIS_XADD)                                                     \
    ACTION(REQ_REDIS_XAUTOCLAIM)                                               \
    ACTION(REQ_REDIS_XCLAIM)                                                   \
    ACTION(REQ_REDIS_XDEL)                                                     \
    ACTION(REQ_REDIS_XGROUP)                                                   \
    ACTION(REQ_REDIS_XINFO)                                                    \
    ACTION(REQ_REDIS_XLEN)                                                     \
    ACTION(REQ_REDIS_XPENDING)                                                 \
    ACTION(REQ_REDIS_XRANGE)                                                   \
    ACTION(REQ_REDIS_XREVRANGE)                                                \
    ACTION(REQ_REDIS_XTRIM)                                                    \
    ACTION(REQ_REDIS_ZADD) /* redis requests - sorted sets */                  \
    ACTION(REQ_REDIS_ZCARD)                                                    \
    ACTION(REQ_REDIS_ZCOUNT)                                                   \
    ACTION(REQ_REDIS_ZINCRBY)                                                  \
    ACTION(REQ_REDIS_ZINTERSTORE)                                              \
    ACTION(REQ_REDIS_ZLEXCOUNT)                                                \
    ACTION(REQ_REDIS_ZRANGE)                                                   \
    ACTION(REQ_REDIS_ZRANGEBYLEX)                                              \
    ACTION(REQ_REDIS_ZRANGEBYSCORE)                                            \
    ACTION(REQ_REDIS_ZRANK)                                                    \
    ACTION(REQ_REDIS_ZREM)                                                     \
    ACTION(REQ_REDIS_ZREMRANGEBYRANK)                                          \
    ACTION(REQ_REDIS_ZREMRANGEBYLEX)                                           \
    ACTION(REQ_REDIS_ZREMRANGEBYSCORE)                                         \
    ACTION(REQ_REDIS_ZREVRANGE)                                                \
    ACTION(REQ_REDIS_ZREVRANGEBYSCORE)                                         \
    ACTION(REQ_REDIS_ZREVRANK)                                                 \
    ACTION(REQ_REDIS_ZSCORE)                                                   \
    ACTION(REQ_REDIS_ZUNIONSTORE)                                              \
    ACTION(REQ_REDIS_ZSCAN)                                                    \
    ACTION(REQ_REDIS_EVAL) /* redis requests - eval */                         \
    ACTION(REQ_REDIS_EVALSHA)                                                  \
    ACTION(REQ_REDIS_PING) /* redis requests - ping/quit */                    \
    ACTION(REQ_REDIS_QUIT)                                                     \
    ACTION(REQ_REDIS_AUTH)                                                     \
    ACTION(RSP_REDIS_STATUS) /* redis response */                              \
    ACTION(RSP_REDIS_ERROR)                                                    \
    ACTION(RSP_REDIS_INTEGER)                                                  \
    ACTION(RSP_REDIS_BULK)                                                     \
    ACTION(RSP_REDIS_MULTIBULK)                                                \
    ACTION(SENTINEL)

#define DEFINE_ACTION(_name) CMD_##_name,
typedef enum cmd_type { CMD_TYPE_CODEC(DEFINE_ACTION) } cmd_type_t;
#undef DEFINE_ACTION

struct keypos {
    char *start;         /* key start pos */
    char *end;           /* key end pos */
    uint32_t remain_len; /* remain length after keypos->end for more key-value
                            pairs in command, like mset */
};

struct cmd {

    uint64_t id; /* command id */

    cmd_parse_result_t result; /* command parsing result */
    char *errstr;              /* error info when the command parse failed */

    cmd_type_t type; /* command type */

    char *cmd;
    uint32_t clen; /* command length */

    struct hiarray *keys; /* array of keypos, for req */

    char *narg_start; /* narg start (redis) */
    char *narg_end;   /* narg end (redis) */
    uint32_t narg;    /* # arguments (redis) */

    unsigned quit : 1;      /* quit request? */
    unsigned noforward : 1; /* not need forward (example: ping) */

    /* Command destination */
    int slot_num;    /* Command should be sent to slot.
                      * Set to -1 if command is sent to a given node,
                      * or if a slot can not be found or calculated,
                      * or if its a multi-key command cross different
                      * nodes (cross slot) */
    char *node_addr; /* Command sent to this node address */

    struct cmd *
        *frag_seq; /* sequence of fragment command, map from keys to fragments*/

    redisReply *reply;

    hilist *sub_commands; /* just for pipeline and multi-key commands */
};

void redis_parse_cmd(struct cmd *r);

struct cmd *command_get(void);
void command_destroy(struct cmd *command);

#endif
