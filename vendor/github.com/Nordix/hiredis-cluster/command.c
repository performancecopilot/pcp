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
#include <ctype.h>
#include <errno.h>
#include <hiredis/alloc.h>
#include <strings.h>

#include "command.h"
#include "hiarray.h"
#include "hiutil.h"

static uint64_t cmd_id = 0; /* command id counter */

/*
 * Return true, if the redis command take no key, otherwise
 * return false
 * Format: command
 */
static int redis_argz(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_PING:
    case CMD_REQ_REDIS_QUIT:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts no arguments, otherwise
 * return false
 * Format: command key
 */
static int redis_arg0(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_PERSIST:
    case CMD_REQ_REDIS_PTTL:
    case CMD_REQ_REDIS_SORT:
    case CMD_REQ_REDIS_TTL:
    case CMD_REQ_REDIS_TYPE:
    case CMD_REQ_REDIS_DUMP:

    case CMD_REQ_REDIS_DECR:
    case CMD_REQ_REDIS_GET:
    case CMD_REQ_REDIS_INCR:
    case CMD_REQ_REDIS_STRLEN:

    case CMD_REQ_REDIS_HGETALL:
    case CMD_REQ_REDIS_HKEYS:
    case CMD_REQ_REDIS_HLEN:
    case CMD_REQ_REDIS_HVALS:

    case CMD_REQ_REDIS_LLEN:
    case CMD_REQ_REDIS_LPOP:
    case CMD_REQ_REDIS_RPOP:

    case CMD_REQ_REDIS_SCARD:
    case CMD_REQ_REDIS_SMEMBERS:
    case CMD_REQ_REDIS_SPOP:

    case CMD_REQ_REDIS_XLEN:
    case CMD_REQ_REDIS_ZCARD:
    case CMD_REQ_REDIS_PFCOUNT:
    case CMD_REQ_REDIS_AUTH:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 1 argument, otherwise
 * return false
 * Format: command key arg
 */
static int redis_arg1(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_EXPIRE:
    case CMD_REQ_REDIS_EXPIREAT:
    case CMD_REQ_REDIS_PEXPIRE:
    case CMD_REQ_REDIS_PEXPIREAT:

    case CMD_REQ_REDIS_APPEND:
    case CMD_REQ_REDIS_DECRBY:
    case CMD_REQ_REDIS_GETBIT:
    case CMD_REQ_REDIS_GETSET:
    case CMD_REQ_REDIS_INCRBY:
    case CMD_REQ_REDIS_INCRBYFLOAT:
    case CMD_REQ_REDIS_SETNX:

    case CMD_REQ_REDIS_HEXISTS:
    case CMD_REQ_REDIS_HGET:

    case CMD_REQ_REDIS_LINDEX:
    case CMD_REQ_REDIS_LPUSHX:
    case CMD_REQ_REDIS_RPOPLPUSH:
    case CMD_REQ_REDIS_RPUSHX:

    case CMD_REQ_REDIS_SISMEMBER:

    case CMD_REQ_REDIS_ZRANK:
    case CMD_REQ_REDIS_ZREVRANK:
    case CMD_REQ_REDIS_ZSCORE:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 2 arguments, otherwise
 * return false
 * Format: command key arg1 arg2
 */
static int redis_arg2(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_GETRANGE:
    case CMD_REQ_REDIS_PSETEX:
    case CMD_REQ_REDIS_SETBIT:
    case CMD_REQ_REDIS_SETEX:
    case CMD_REQ_REDIS_SETRANGE:

    case CMD_REQ_REDIS_HINCRBY:
    case CMD_REQ_REDIS_HINCRBYFLOAT:
    case CMD_REQ_REDIS_HSET:
    case CMD_REQ_REDIS_HSETNX:

    case CMD_REQ_REDIS_LRANGE:
    case CMD_REQ_REDIS_LREM:
    case CMD_REQ_REDIS_LSET:
    case CMD_REQ_REDIS_LTRIM:

    case CMD_REQ_REDIS_SMOVE:

    case CMD_REQ_REDIS_ZCOUNT:
    case CMD_REQ_REDIS_ZLEXCOUNT:
    case CMD_REQ_REDIS_ZINCRBY:
    case CMD_REQ_REDIS_ZREMRANGEBYLEX:
    case CMD_REQ_REDIS_ZREMRANGEBYRANK:
    case CMD_REQ_REDIS_ZREMRANGEBYSCORE:

    case CMD_REQ_REDIS_RESTORE:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts exactly 3 arguments, otherwise
 * return false
 * Format: command key arg1 arg2 arg3
 */
static int redis_arg3(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_LINSERT:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command accepts 0 or more arguments, otherwise
 * return false
 * Format: command key [ arg ... ]
 */
static int redis_argn(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_BITCOUNT:

    case CMD_REQ_REDIS_SET:
    case CMD_REQ_REDIS_HDEL:
    case CMD_REQ_REDIS_HMGET:
    case CMD_REQ_REDIS_HMSET:
    case CMD_REQ_REDIS_HSCAN:

    case CMD_REQ_REDIS_LPUSH:
    case CMD_REQ_REDIS_RPUSH:

    case CMD_REQ_REDIS_SADD:
    case CMD_REQ_REDIS_SDIFF:
    case CMD_REQ_REDIS_SDIFFSTORE:
    case CMD_REQ_REDIS_SINTER:
    case CMD_REQ_REDIS_SINTERSTORE:
    case CMD_REQ_REDIS_SREM:
    case CMD_REQ_REDIS_SUNION:
    case CMD_REQ_REDIS_SUNIONSTORE:
    case CMD_REQ_REDIS_SRANDMEMBER:
    case CMD_REQ_REDIS_SSCAN:

    case CMD_REQ_REDIS_PFADD:
    case CMD_REQ_REDIS_PFMERGE:
    case CMD_REQ_REDIS_XACK:
    case CMD_REQ_REDIS_XADD:
    case CMD_REQ_REDIS_XAUTOCLAIM:
    case CMD_REQ_REDIS_XCLAIM:
    case CMD_REQ_REDIS_XDEL:
    case CMD_REQ_REDIS_XPENDING:
    case CMD_REQ_REDIS_XRANGE:
    case CMD_REQ_REDIS_XREVRANGE:
    case CMD_REQ_REDIS_XTRIM:
    case CMD_REQ_REDIS_ZADD:
    case CMD_REQ_REDIS_ZINTERSTORE:
    case CMD_REQ_REDIS_ZRANGE:
    case CMD_REQ_REDIS_ZRANGEBYSCORE:
    case CMD_REQ_REDIS_ZREM:
    case CMD_REQ_REDIS_ZREVRANGE:
    case CMD_REQ_REDIS_ZRANGEBYLEX:
    case CMD_REQ_REDIS_ZREVRANGEBYSCORE:
    case CMD_REQ_REDIS_ZUNIONSTORE:
    case CMD_REQ_REDIS_ZSCAN:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is a vector command accepting one or
 * more keys, otherwise return false
 * Format: command key [ key ... ]
 */
static int redis_argx(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_EXISTS:
    case CMD_REQ_REDIS_MGET:
    case CMD_REQ_REDIS_DEL:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is a vector command accepting one or
 * more key-value pairs, otherwise return false
 * Format: command key value [ key value ... ]
 */
static int redis_argkvx(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_MSET:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Check if command type expects a sub-command before the key
 * Format: command subcommand key [ arg ... ]
 */
static int redis_argsub(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_XGROUP:
    case CMD_REQ_REDIS_XINFO:
        return 1;

    default:
        break;
    }

    return 0;
}

/*
 * Return true, if the redis command is either EVAL or EVALSHA. These commands
 * have a special format with exactly 2 arguments, followed by one or more keys,
 * followed by zero or more arguments (the documentation online seems to suggest
 * that at least one argument is required, but that shouldn't be the case).
 */
static int redis_argeval(struct cmd *r) {
    switch (r->type) {
    case CMD_REQ_REDIS_EVAL:
    case CMD_REQ_REDIS_EVALSHA:
        return 1;

    default:
        break;
    }

    return 0;
}

static inline cmd_type_t redis_parse_cmd_verb(const char *m, int len) {
    // clang-format off
    switch (len) {
    case 3:
        return !strncasecmp(m, "get", 3) ? CMD_REQ_REDIS_GET :
               !strncasecmp(m, "set", 3) ? CMD_REQ_REDIS_SET :
               !strncasecmp(m, "ttl", 3) ? CMD_REQ_REDIS_TTL :
               !strncasecmp(m, "del", 3) ? CMD_REQ_REDIS_DEL :
                                           CMD_UNKNOWN;
    case 4:
        return !strncasecmp(m, "pttl", 4) ? CMD_REQ_REDIS_PTTL :
               !strncasecmp(m, "decr", 4) ? CMD_REQ_REDIS_DECR :
               !strncasecmp(m, "dump", 4) ? CMD_REQ_REDIS_DUMP :
               !strncasecmp(m, "hdel", 4) ? CMD_REQ_REDIS_HDEL :
               !strncasecmp(m, "hget", 4) ? CMD_REQ_REDIS_HGET :
               !strncasecmp(m, "hlen", 4) ? CMD_REQ_REDIS_HLEN :
               !strncasecmp(m, "hset", 4) ? CMD_REQ_REDIS_HSET :
               !strncasecmp(m, "incr", 4) ? CMD_REQ_REDIS_INCR :
               !strncasecmp(m, "llen", 4) ? CMD_REQ_REDIS_LLEN :
               !strncasecmp(m, "lpop", 4) ? CMD_REQ_REDIS_LPOP :
               !strncasecmp(m, "lrem", 4) ? CMD_REQ_REDIS_LREM :
               !strncasecmp(m, "lset", 4) ? CMD_REQ_REDIS_LSET :
               !strncasecmp(m, "rpop", 4) ? CMD_REQ_REDIS_RPOP :
               !strncasecmp(m, "sadd", 4) ? CMD_REQ_REDIS_SADD :
               !strncasecmp(m, "spop", 4) ? CMD_REQ_REDIS_SPOP :
               !strncasecmp(m, "srem", 4) ? CMD_REQ_REDIS_SREM :
               !strncasecmp(m, "type", 4) ? CMD_REQ_REDIS_TYPE :
               !strncasecmp(m, "mget", 4) ? CMD_REQ_REDIS_MGET :
               !strncasecmp(m, "mset", 4) ? CMD_REQ_REDIS_MSET :
               !strncasecmp(m, "xack", 4) ? CMD_REQ_REDIS_XACK :
               !strncasecmp(m, "xadd", 4) ? CMD_REQ_REDIS_XADD :
               !strncasecmp(m, "xdel", 4) ? CMD_REQ_REDIS_XDEL :
               !strncasecmp(m, "xlen", 4) ? CMD_REQ_REDIS_XLEN :
               !strncasecmp(m, "zadd", 4) ? CMD_REQ_REDIS_ZADD :
               !strncasecmp(m, "zrem", 4) ? CMD_REQ_REDIS_ZREM :
               !strncasecmp(m, "eval", 4) ? CMD_REQ_REDIS_EVAL :
               !strncasecmp(m, "sort", 4) ? CMD_REQ_REDIS_SORT :
               !strncasecmp(m, "ping", 4) ? CMD_REQ_REDIS_PING :
               !strncasecmp(m, "quit", 4) ? CMD_REQ_REDIS_QUIT :
               !strncasecmp(m, "auth", 4) ? CMD_REQ_REDIS_AUTH :
                                            CMD_UNKNOWN;
    case 5:
        return !strncasecmp(m, "hkeys", 5) ? CMD_REQ_REDIS_HKEYS :
               !strncasecmp(m, "hmget", 5) ? CMD_REQ_REDIS_HMGET :
               !strncasecmp(m, "hmset", 5) ? CMD_REQ_REDIS_HMSET :
               !strncasecmp(m, "hvals", 5) ? CMD_REQ_REDIS_HVALS :
               !strncasecmp(m, "hscan", 5) ? CMD_REQ_REDIS_HSCAN :
               !strncasecmp(m, "lpush", 5) ? CMD_REQ_REDIS_LPUSH :
               !strncasecmp(m, "ltrim", 5) ? CMD_REQ_REDIS_LTRIM :
               !strncasecmp(m, "rpush", 5) ? CMD_REQ_REDIS_RPUSH :
               !strncasecmp(m, "scard", 5) ? CMD_REQ_REDIS_SCARD :
               !strncasecmp(m, "sdiff", 5) ? CMD_REQ_REDIS_SDIFF :
               !strncasecmp(m, "setex", 5) ? CMD_REQ_REDIS_SETEX :
               !strncasecmp(m, "setnx", 5) ? CMD_REQ_REDIS_SETNX :
               !strncasecmp(m, "smove", 5) ? CMD_REQ_REDIS_SMOVE :
               !strncasecmp(m, "sscan", 5) ? CMD_REQ_REDIS_SSCAN :
               !strncasecmp(m, "xinfo", 5) ? CMD_REQ_REDIS_XINFO :
               !strncasecmp(m, "xtrim", 5) ? CMD_REQ_REDIS_XTRIM :
               !strncasecmp(m, "zcard", 5) ? CMD_REQ_REDIS_ZCARD :
               !strncasecmp(m, "zrank", 5) ? CMD_REQ_REDIS_ZRANK :
               !strncasecmp(m, "zscan", 5) ? CMD_REQ_REDIS_ZSCAN :
               !strncasecmp(m, "pfadd", 5) ? CMD_REQ_REDIS_PFADD :
                                             CMD_UNKNOWN;
    case 6:
        return !strncasecmp(m, "append", 6) ? CMD_REQ_REDIS_APPEND :
               !strncasecmp(m, "decrby", 6) ? CMD_REQ_REDIS_DECRBY :
               !strncasecmp(m, "exists", 6) ? CMD_REQ_REDIS_EXISTS :
               !strncasecmp(m, "expire", 6) ? CMD_REQ_REDIS_EXPIRE :
               !strncasecmp(m, "getbit", 6) ? CMD_REQ_REDIS_GETBIT :
               !strncasecmp(m, "getset", 6) ? CMD_REQ_REDIS_GETSET :
               !strncasecmp(m, "psetex", 6) ? CMD_REQ_REDIS_PSETEX :
               !strncasecmp(m, "hsetnx", 6) ? CMD_REQ_REDIS_HSETNX :
               !strncasecmp(m, "incrby", 6) ? CMD_REQ_REDIS_INCRBY :
               !strncasecmp(m, "lindex", 6) ? CMD_REQ_REDIS_LINDEX :
               !strncasecmp(m, "lpushx", 6) ? CMD_REQ_REDIS_LPUSHX :
               !strncasecmp(m, "lrange", 6) ? CMD_REQ_REDIS_LRANGE :
               !strncasecmp(m, "rpushx", 6) ? CMD_REQ_REDIS_RPUSHX :
               !strncasecmp(m, "setbit", 6) ? CMD_REQ_REDIS_SETBIT :
               !strncasecmp(m, "sinter", 6) ? CMD_REQ_REDIS_SINTER :
               !strncasecmp(m, "strlen", 6) ? CMD_REQ_REDIS_STRLEN :
               !strncasecmp(m, "sunion", 6) ? CMD_REQ_REDIS_SUNION :
               !strncasecmp(m, "xclaim", 6) ? CMD_REQ_REDIS_XCLAIM :
               !strncasecmp(m, "xgroup", 6) ? CMD_REQ_REDIS_XGROUP :
               !strncasecmp(m, "xrange", 6) ? CMD_REQ_REDIS_XRANGE :
               !strncasecmp(m, "zcount", 6) ? CMD_REQ_REDIS_ZCOUNT :
               !strncasecmp(m, "zrange", 6) ? CMD_REQ_REDIS_ZRANGE :
               !strncasecmp(m, "zscore", 6) ? CMD_REQ_REDIS_ZSCORE :
                                              CMD_UNKNOWN;
    case 7:
        return !strncasecmp(m, "persist", 7) ? CMD_REQ_REDIS_PERSIST :
               !strncasecmp(m, "pexpire", 7) ? CMD_REQ_REDIS_PEXPIRE :
               !strncasecmp(m, "hexists", 7) ? CMD_REQ_REDIS_HEXISTS :
               !strncasecmp(m, "hgetall", 7) ? CMD_REQ_REDIS_HGETALL :
               !strncasecmp(m, "hincrby", 7) ? CMD_REQ_REDIS_HINCRBY :
               !strncasecmp(m, "linsert", 7) ? CMD_REQ_REDIS_LINSERT :
               !strncasecmp(m, "zincrby", 7) ? CMD_REQ_REDIS_ZINCRBY :
               !strncasecmp(m, "evalsha", 7) ? CMD_REQ_REDIS_EVALSHA :
               !strncasecmp(m, "restore", 7) ? CMD_REQ_REDIS_RESTORE :
               !strncasecmp(m, "pfcount", 7) ? CMD_REQ_REDIS_PFCOUNT :
               !strncasecmp(m, "pfmerge", 7) ? CMD_REQ_REDIS_PFMERGE :
                                               CMD_UNKNOWN;
    case 8:
        return !strncasecmp(m, "expireat", 8) ? CMD_REQ_REDIS_EXPIREAT :
               !strncasecmp(m, "bitcount", 8) ? CMD_REQ_REDIS_BITCOUNT :
               !strncasecmp(m, "getrange", 8) ? CMD_REQ_REDIS_GETRANGE :
               !strncasecmp(m, "setrange", 8) ? CMD_REQ_REDIS_SETRANGE :
               !strncasecmp(m, "smembers", 8) ? CMD_REQ_REDIS_SMEMBERS :
               !strncasecmp(m, "xpending", 8) ? CMD_REQ_REDIS_XPENDING :
               !strncasecmp(m, "zrevrank", 8) ? CMD_REQ_REDIS_ZREVRANK :
                                                CMD_UNKNOWN;
    case 9:
        return !strncasecmp(m, "pexpireat", 9) ? CMD_REQ_REDIS_PEXPIREAT :
               !strncasecmp(m, "rpoplpush", 9) ? CMD_REQ_REDIS_RPOPLPUSH :
               !strncasecmp(m, "sismember", 9) ? CMD_REQ_REDIS_SISMEMBER :
               !strncasecmp(m, "xrevrange", 9) ? CMD_REQ_REDIS_XREVRANGE :
               !strncasecmp(m, "zrevrange", 9) ? CMD_REQ_REDIS_ZREVRANGE :
               !strncasecmp(m, "zlexcount", 9) ? CMD_REQ_REDIS_ZLEXCOUNT :
                                                 CMD_UNKNOWN;
    case 10:
        return !strncasecmp(m, "sdiffstore", 10) ? CMD_REQ_REDIS_SDIFFSTORE :
               !strncasecmp(m, "xautoclaim", 10) ? CMD_REQ_REDIS_XAUTOCLAIM :
                                                   CMD_UNKNOWN;
    case 11:
        return !strncasecmp(m, "incrbyfloat", 11) ? CMD_REQ_REDIS_INCRBYFLOAT :
               !strncasecmp(m, "sinterstore", 11) ? CMD_REQ_REDIS_SINTERSTORE :
               !strncasecmp(m, "srandmember", 11) ? CMD_REQ_REDIS_SRANDMEMBER :
               !strncasecmp(m, "sunionstore", 11) ? CMD_REQ_REDIS_SUNIONSTORE :
               !strncasecmp(m, "zinterstore", 11) ? CMD_REQ_REDIS_ZINTERSTORE :
               !strncasecmp(m, "zunionstore", 11) ? CMD_REQ_REDIS_ZUNIONSTORE :
               !strncasecmp(m, "zrangebylex", 11) ? CMD_REQ_REDIS_ZRANGEBYLEX :
                                                    CMD_UNKNOWN;
    case 12:
        return !strncasecmp(m, "hincrbyfloat", 12) ?
                   CMD_REQ_REDIS_HINCRBYFLOAT :
                   CMD_UNKNOWN;
    case 13:
        return !strncasecmp(m, "zrangebyscore", 13) ?
                   CMD_REQ_REDIS_ZRANGEBYSCORE :
                   CMD_UNKNOWN;
    case 14:
        return !strncasecmp(m, "zremrangebylex", 14) ?
                   CMD_REQ_REDIS_ZREMRANGEBYLEX :
                   CMD_UNKNOWN;
    case 15:
        return !strncasecmp(m, "zremrangebyrank", 15) ?
                   CMD_REQ_REDIS_ZREMRANGEBYRANK :
                   CMD_UNKNOWN;
    case 16:
        return !strncasecmp(m, "zremrangebyscore", 16) ?
                   CMD_REQ_REDIS_ZREMRANGEBYSCORE :
               !strncasecmp(m, "zrevrangebyscore", 16) ?
                   CMD_REQ_REDIS_ZREVRANGEBYSCORE :
                   CMD_UNKNOWN;
    default:
        return CMD_UNKNOWN;
    }
    // clang-format on
}

/*
 * Reference: http://redis.io/topics/protocol
 *
 * Redis >= 1.2 uses the unified protocol to send requests to the Redis
 * server. In the unified protocol all the arguments sent to the server
 * are binary safe and every request has the following general form:
 *
 *   *<number of arguments> CR LF
 *   $<number of bytes of argument 1> CR LF
 *   <argument data> CR LF
 *   ...
 *   $<number of bytes of argument N> CR LF
 *   <argument data> CR LF
 *
 * Before the unified request protocol, redis protocol for requests supported
 * the following commands
 * 1). Inline commands: simple commands where arguments are just space
 *     separated strings. No binary safeness is possible.
 * 2). Bulk commands: bulk commands are exactly like inline commands, but
 *     the last argument is handled in a special way in order to allow for
 *     a binary-safe last argument.
 *
 * only supports the Redis unified protocol for requests.
 */
void redis_parse_cmd(struct cmd *r) {
    int len;
    char *p, *m, *token = NULL;
    char *cmd_end;
    char ch;
    uint32_t rlen = 0;  /* running length in parsing fsa */
    uint32_t rnarg = 0; /* running # arg used by parsing fsa */
    enum {
        SW_START,
        SW_NARG,
        SW_NARG_LF,
        SW_CMD_TYPE_LEN,
        SW_CMD_TYPE_LEN_LF,
        SW_CMD_TYPE,
        SW_CMD_TYPE_LF,
        SW_KEY_LEN,
        SW_KEY_LEN_LF,
        SW_KEY,
        SW_KEY_LF,
        SW_ARG1_LEN,
        SW_ARG1_LEN_LF,
        SW_ARG1,
        SW_ARG1_LF,
        SW_ARG2_LEN,
        SW_ARG2_LEN_LF,
        SW_ARG2,
        SW_ARG2_LF,
        SW_ARG3_LEN,
        SW_ARG3_LEN_LF,
        SW_ARG3,
        SW_ARG3_LF,
        SW_ARGN_LEN,
        SW_ARGN_LEN_LF,
        SW_ARGN,
        SW_ARGN_LF,
        SW_SENTINEL
    } state;

    state = SW_START;
    cmd_end = r->cmd + r->clen;

    ASSERT(state >= SW_START && state < SW_SENTINEL);
    ASSERT(r->cmd != NULL && r->clen > 0);

    for (p = r->cmd; p < cmd_end; p++) {
        ch = *p;

        switch (state) {

        case SW_START:
        case SW_NARG:
            if (token == NULL) {
                if (ch != '*') {
                    goto error;
                }
                token = p;
                r->narg_start = p;
                rnarg = 0;
                state = SW_NARG;
            } else if (isdigit(ch)) {
                rnarg = rnarg * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (rnarg == 0) {
                    goto error;
                }
                r->narg = rnarg;
                r->narg_end = p;
                token = NULL;
                state = SW_NARG_LF;
            } else {
                goto error;
            }

            break;

        case SW_NARG_LF:
            switch (ch) {
            case LF:
                state = SW_CMD_TYPE_LEN;
                break;

            default:
                goto error;
            }

            break;

        case SW_CMD_TYPE_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                token = p;
                rlen = 0;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if (rlen == 0 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_CMD_TYPE_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_CMD_TYPE_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_CMD_TYPE;
                break;

            default:
                goto error;
            }

            break;

        case SW_CMD_TYPE:
            if (token == NULL) {
                token = p;
            }

            m = token + rlen;
            if (m >= cmd_end) {
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            m = token;
            token = NULL;
            r->type = redis_parse_cmd_verb(m, p - m);
            if (r->type == CMD_UNKNOWN) {
                goto error;
            }

            state = SW_CMD_TYPE_LF;
            break;

        case SW_CMD_TYPE_LF:
            switch (ch) {
            case LF:
                if (redis_argz(r)) {
                    goto done;
                } else if (redis_argeval(r) || redis_argsub(r)) {
                    state = SW_ARG1_LEN;
                } else {
                    state = SW_KEY_LEN;
                }
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                token = p;
                rlen = 0;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {

                if (rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_KEY_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_KEY_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_KEY;
                break;

            default:
                goto error;
            }

            break;

        case SW_KEY:
            if (token == NULL) {
                token = p;
            }

            m = token + rlen;
            if (m >= cmd_end) {
                goto error;
            }

            if (*m != CR) {
                goto error;
            } else { /* got a key */
                struct keypos *kpos;

                p = m; /* move forward by rlen bytes */
                rlen = 0;
                m = token;
                token = NULL;

                kpos = hiarray_push(r->keys);
                if (kpos == NULL) {
                    goto oom;
                }
                kpos->start = m;
                kpos->end = p;

                state = SW_KEY_LF;
            }

            break;

        case SW_KEY_LF:
            switch (ch) {
            case LF:
                if (redis_arg0(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg1(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_arg2(r)) {
                    if (rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_arg3(r)) {
                    if (rnarg != 3) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_KEY_LEN;
                } else if (redis_argkvx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    if (r->narg % 2 == 0) {
                        goto error;
                    }
                    state = SW_ARG1_LEN;
                } else if (redis_argeval(r) || redis_argsub(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG1_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;

                /*
                //for mset value length
                if(redis_argkvx(r))
                {
                    struct keypos *kpos;
                    uint32_t array_len = array_n(r->keys);
                    if(array_len == 0)
                    {
                        goto error;
                    }

                    kpos = array_n(r->keys, array_len-1);
                    if (kpos == NULL || kpos->v_len != 0) {
                        goto error;
                    }

                    kpos->v_len = rlen;
                }
                */
                state = SW_ARG1_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG1_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG1;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG1:
            m = p + rlen;
            if (m >= cmd_end) {
                // Moving past the end, not good..
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;

            state = SW_ARG1_LF;

            break;

        case SW_ARG1_LF:
            // Check that the command parser has enough
            // arguments left to be acceptable
            // rnarg is the number of arguments after the first argument
            switch (ch) {
            case LF:
                if (redis_arg1(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg2(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_arg3(r)) {
                    if (rnarg != 2) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else if (redis_argeval(r)) {
                    // EVAL command layout:
                    // eval <script> <no of keys> <keys..> <args..>
                    //              ^
                    // The Redis docs specifies that EVAL dont require a key
                    // (i.e rnarg < 1) but also that a key would be
                    // required for it to work in Redis Cluster.
                    // Hiredis-cluster requires at least one key in eval to know
                    // which instance to use.
                    if (rnarg < 2) {
                        goto error;
                    }
                    state = SW_ARG2_LEN;
                } else if (redis_argsub(r)) {
                    // Command layout:  cmd <sub-cmd> <key> <args..>
                    // Current position:             ^
                    if (rnarg < 1) {
                        goto error;
                    }
                    state = SW_KEY_LEN;
                } else if (redis_argkvx(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_KEY_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARG2_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG2_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG2;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG2:
            if (token == NULL && redis_argeval(r)) {
                /*
                 * For EVAL/EVALSHA, ARG2 represents the # key/arg pairs which
                 * must be tokenized and stored in contiguous memory.
                 */
                token = p;
            }

            m = p + rlen;
            if (m >= cmd_end) {
                // rlen -= (uint32_t)(b->last - p);
                // m = b->last - 1;
                // p = m;
                // break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;

            if (redis_argeval(r)) {
                uint32_t nkey;
                char *chp;

                /*
                 * For EVAL/EVALSHA, we need to find the integer value of this
                 * argument. It tells us the number of keys in the script, and
                 * we need to error out if number of keys is 0. At this point,
                 * both p and m point to the end of the argument and r->token
                 * points to the start.
                 */
                if (p - token < 1) {
                    goto error;
                }

                for (nkey = 0, chp = token; chp < p; chp++) {
                    if (isdigit(*chp)) {
                        nkey = nkey * 10 + (uint32_t)(*chp - '0');
                    } else {
                        goto error;
                    }
                }
                if (nkey == 0) {
                    goto error;
                }

                token = NULL;
            }

            state = SW_ARG2_LF;

            break;

        case SW_ARG2_LF:
            switch (ch) {
            case LF:
                if (redis_arg2(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_arg3(r)) {
                    if (rnarg != 1) {
                        goto error;
                    }
                    state = SW_ARG3_LEN;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else if (redis_argeval(r)) {
                    if (rnarg < 1) {
                        goto error;
                    }
                    state = SW_KEY_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARG3_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARG3_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARG3;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARG3:
            m = p + rlen;
            if (m >= cmd_end) {
                // rlen -= (uint32_t)(b->last - p);
                // m = b->last - 1;
                // p = m;
                // break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            state = SW_ARG3_LF;

            break;

        case SW_ARG3_LF:
            switch (ch) {
            case LF:
                if (redis_arg3(r)) {
                    if (rnarg != 0) {
                        goto error;
                    }
                    goto done;
                } else if (redis_argn(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN_LEN:
            if (token == NULL) {
                if (ch != '$') {
                    goto error;
                }
                rlen = 0;
                token = p;
            } else if (isdigit(ch)) {
                rlen = rlen * 10 + (uint32_t)(ch - '0');
            } else if (ch == CR) {
                if ((p - token) <= 1 || rnarg == 0) {
                    goto error;
                }
                rnarg--;
                token = NULL;
                state = SW_ARGN_LEN_LF;
            } else {
                goto error;
            }

            break;

        case SW_ARGN_LEN_LF:
            switch (ch) {
            case LF:
                state = SW_ARGN;
                break;

            default:
                goto error;
            }

            break;

        case SW_ARGN:
            m = p + rlen;
            if (m >= cmd_end) {
                // rlen -= (uint32_t)(b->last - p);
                // m = b->last - 1;
                // p = m;
                // break;
                goto error;
            }

            if (*m != CR) {
                goto error;
            }

            p = m; /* move forward by rlen bytes */
            rlen = 0;
            state = SW_ARGN_LF;

            break;

        case SW_ARGN_LF:
            switch (ch) {
            case LF:
                if (redis_argn(r) || redis_argeval(r) || redis_argsub(r)) {
                    if (rnarg == 0) {
                        goto done;
                    }
                    state = SW_ARGN_LEN;
                } else {
                    goto error;
                }

                break;

            default:
                goto error;
            }

            break;

        case SW_SENTINEL:
        default:
            NOT_REACHED();
            break;
        }
    }

    ASSERT(p == cmd_end);

    return;

done:
    ASSERT(r->type > CMD_UNKNOWN && r->type < CMD_SENTINEL);
    r->result = CMD_PARSE_OK;
    return;

error:
    r->result = CMD_PARSE_ERROR;
    errno = EINVAL;
    if (r->errstr == NULL) {
        r->errstr = hi_malloc(100 * sizeof(*r->errstr));
        if (r->errstr == NULL) {
            goto oom;
        }
    }

    len = _scnprintf(
        r->errstr, 100,
        "Parse command error. Cmd type: %d, state: %d, break position: %d.",
        r->type, state, (int)(p - r->cmd));
    r->errstr[len] = '\0';
    return;

oom:
    r->result = CMD_PARSE_ENOMEM;
}

struct cmd *command_get() {
    struct cmd *command;
    command = hi_malloc(sizeof(struct cmd));
    if (command == NULL) {
        return NULL;
    }

    command->id = ++cmd_id;
    command->result = CMD_PARSE_OK;
    command->errstr = NULL;
    command->type = CMD_UNKNOWN;
    command->cmd = NULL;
    command->clen = 0;
    command->keys = NULL;
    command->narg_start = NULL;
    command->narg_end = NULL;
    command->narg = 0;
    command->quit = 0;
    command->noforward = 0;
    command->slot_num = -1;
    command->frag_seq = NULL;
    command->reply = NULL;
    command->sub_commands = NULL;
    command->node_addr = NULL;

    command->keys = hiarray_create(1, sizeof(struct keypos));
    if (command->keys == NULL) {
        hi_free(command);
        return NULL;
    }

    return command;
}

void command_destroy(struct cmd *command) {
    if (command == NULL) {
        return;
    }

    if (command->cmd != NULL) {
        hi_free(command->cmd);
        command->cmd = NULL;
    }

    if (command->errstr != NULL) {
        hi_free(command->errstr);
        command->errstr = NULL;
    }

    if (command->keys != NULL) {
        command->keys->nelem = 0;
        hiarray_destroy(command->keys);
        command->keys = NULL;
    }

    if (command->frag_seq != NULL) {
        hi_free(command->frag_seq);
        command->frag_seq = NULL;
    }

    freeReplyObject(command->reply);

    if (command->sub_commands != NULL) {
        listRelease(command->sub_commands);
    }

    if (command->node_addr != NULL) {
        sdsfree(command->node_addr);
        command->node_addr = NULL;
    }

    hi_free(command);
}
