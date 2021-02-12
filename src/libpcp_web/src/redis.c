/*
 * Copyright (c) 2017-2020, Red Hat.
 * Copyright (c) 2009-2011, Salvatore Sanfilippo <antirez at gmail dot com>
 * Copyright (c) 2010-2014, Pieter Noordhuis <pcnoordhuis at gmail dot com>
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
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include "zmalloc.h"
#include "pmapi.h"
#include "redis.h"
#include "util.h"
#include "dict.h"
#include "net.h"
#include "sds.h"

const char *
redis_reply_type(redisReply *reply)
{
    if (reply == NULL)
        return "none";
    switch (reply->type) {
    case REDIS_REPLY_STRING:
        return "string";
    case REDIS_REPLY_ARRAY:
        return "array";
    case REDIS_REPLY_INTEGER:
        return "integer";
    case REDIS_REPLY_NIL:
        return "nil";
    case REDIS_REPLY_STATUS:
        return "status";
    case REDIS_REPLY_ERROR:
        return "error";
    default:
        break;
    }
    return "unknown";
}

static void
__redisReaderSetError(redisReader *r, int type, const char *str)
{
    size_t		len;

    if (r->reply != NULL && r->fn && r->fn->freeObject) {
        r->fn->freeObject(r->reply);
        r->reply = NULL;
    }

    /* Clear input buffer on errors. */
    if (r->buf != NULL) {
        sdsfree(r->buf);
        r->buf = NULL;
        r->pos = r->len = 0;
    }

    /* Reset task stack. */
    r->ridx = -1;

    /* Set error. */
    r->err = type;
    len = strlen(str);
    len = len < (sizeof(r->errstr)-1) ? len : (sizeof(r->errstr)-1);
    memcpy(r->errstr, str, len);
    r->errstr[len] = '\0';
}

static size_t
chrtos(char *buf, size_t size, char byte)
{
    size_t		len = 0;

    switch (byte) {
    case '\\':
    case '"':  len = pmsprintf(buf, size, "\"\\%c\"", byte); break;
    case '\n': len = pmsprintf(buf, size, "\"\\n\""); break;
    case '\r': len = pmsprintf(buf, size, "\"\\r\""); break;
    case '\t': len = pmsprintf(buf, size, "\"\\t\""); break;
    case '\a': len = pmsprintf(buf, size, "\"\\a\""); break;
    case '\b': len = pmsprintf(buf, size, "\"\\b\""); break;
    default:
        if (isprint((int)byte))
            len = pmsprintf(buf, size, "\"%c\"", byte);
        else
            len = pmsprintf(buf, size, "\"\\x%02x\"", (unsigned char)byte);
        break;
    }
    return len;
}

static void
__redisReaderSetErrorProtocolByte(redisReader *r, char byte)
{
    char		cbuf[8], sbuf[128];

    chrtos(cbuf, sizeof(cbuf), byte);
    pmsprintf(sbuf, sizeof(sbuf), "Protocol error, got %s as reply type byte",
             cbuf);
    __redisReaderSetError(r, REDIS_ERR_PROTOCOL, sbuf);
}

static void
__redisReaderSetErrorOOM(redisReader *r)
{
    __redisReaderSetError(r, REDIS_ERR_OOM, "Out of memory");
}

static char *
readBytes(redisReader *r, unsigned int bytes)
{
    char		*p;

    if (r->len - r->pos >= bytes) {
        p = r->buf + r->pos;
        r->pos += bytes;
        return p;
    }
    return NULL;
}

/* Find pointer to \r\n. */
static char *
seekNewline(char *s, size_t len)
{
    int			pos = 0;
    int			_len = len - 1;

    /*
     * Position should be < len-1 because the character at "pos" should be
     * followed by a \n. Note that strchr cannot be used because it doesn't
     * allow to search a limited length and the buffer that is being searched
     * might not have a trailing NULL character.
     */
    while (pos < _len) {
        while (pos < _len && s[pos] != '\r')
            pos++;
        if (pos == _len) {
            /* Not found. */
            return NULL;
        } else {
            if (s[pos+1] == '\n') {
                /* Found. */
                return s+pos;
            } else {
                /* Continue searching. */
                pos++;
            }
        }
    }
    return NULL;
}

/*
 * Convert a string into a long long. Returns REDIS_OK if the string could be
 * parsed into a (non-overflowing) long long, REDIS_ERR otherwise. The value
 * will be set to the parsed value when appropriate.
 *
 * Note that this function demands that the string strictly represents
 * a long long: no spaces or other characters before or after the string
 * representing the number are accepted, nor zeroes at the start if not
 * for the string "0" representing the zero number.
 *
 * Because of its strictness, it is safe to use this function to check if
 * you can convert a string into a long long, and obtain back the string
 * from the number without any loss in the string representation.
 */
static int
string2ll(const char *s, size_t slen, long long *value)
{
    const char		*p = s;
    size_t		plen = 0;
    int			negative = 0;
    unsigned long long	v;

    if (plen == slen)
        return REDIS_ERR;

    /* Special case: first and only digit is 0. */
    if (slen == 1 && p[0] == '0') {
        if (value != NULL) *value = 0;
        return REDIS_OK;
    }

    if (p[0] == '-') {
        negative = 1;
        p++; plen++;

        /* Abort on only a negative sign. */
        if (plen == slen)
            return REDIS_ERR;
    }
    /* First digit should be 1-9, otherwise the string should just be 0. */
    if (p[0] >= '1' && p[0] <= '9') {
        v = p[0]-'0';
        p++; plen++;
    } else if (p[0] == '0' && slen == 1) {
        *value = 0;
        return REDIS_OK;
    } else {
        return REDIS_ERR;
    }

    while (plen < slen && p[0] >= '0' && p[0] <= '9') {
        if (v > (ULLONG_MAX / 10)) /* Overflow. */
            return REDIS_ERR;
        v *= 10;

        if (v > (ULLONG_MAX - (p[0]-'0'))) /* Overflow. */
            return REDIS_ERR;
        v += p[0]-'0';

        p++; plen++;
    }

    /* Return if not all bytes were used. */
    if (plen < slen)
        return REDIS_ERR;

    if (negative) {
        if (v > ((unsigned long long)(-(LLONG_MIN+1))+1)) /* Overflow. */
            return REDIS_ERR;
        if (value != NULL) *value = -v;
    } else {
        if (v > LLONG_MAX) /* Overflow. */
            return REDIS_ERR;
        if (value != NULL) *value = v;
    }
    return REDIS_OK;
}

static char *
readLine(redisReader *r, int *_len)
{
    char		*p, *s;
    int			len;

    p = r->buf + r->pos;
    s = seekNewline(p, (r->len - r->pos));
    if (s != NULL) {
        len = s - (r->buf + r->pos);
        r->pos += len + 2; /* skip \r\n */
        if (_len)
            *_len = len;
        return p;
    }
    return NULL;
}

static void
moveToNextTask(redisReader *r)
{
    redisReadTask	*cur, *prv;

    while (r->ridx >= 0) {
        /* Return a.s.a.p. when the stack is now empty. */
        if (r->ridx == 0) {
            r->ridx--;
            return;
        }

        cur = &(r->rstack[r->ridx]);
        prv = &(r->rstack[r->ridx-1]);
        assert(prv->type == REDIS_REPLY_ARRAY ||
               prv->type == REDIS_REPLY_MAP ||
               prv->type == REDIS_REPLY_SET);
        if (cur->idx == prv->elements-1) {
            r->ridx--;
        } else {
            /* Reset the type because the next item can be anything */
            assert(cur->idx < prv->elements);
            cur->type = REDIS_REPLY_UNKNOWN;
            cur->elements = -1;
            cur->idx++;
            return;
        }
    }
}

static int
processLineItem(redisReader *r)
{
    redisReadTask	*cur = &(r->rstack[r->ridx]);
    void		*obj;
    char		*p;
    int			len;

    if ((p = readLine(r, &len)) != NULL) {
        if (cur->type == REDIS_REPLY_INTEGER) {
            if (r->fn && r->fn->createInteger) {
                long long v;
                if (string2ll(p, len, &v) == REDIS_ERR) {
                    __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                            "Bad integer value");
                    return REDIS_ERR;
                }
                obj = r->fn->createInteger(cur, v);
	    } else {
                obj = (void*)REDIS_REPLY_INTEGER;
	    }
        } else if (cur->type == REDIS_REPLY_DOUBLE) {
            if (r->fn && r->fn->createDouble) {
                char buf[326], *eptr;
                double d;

                if ((size_t)len >= sizeof(buf)) {
                    __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                            "Double value is too large");
                    return REDIS_ERR;
                }

                memcpy(buf,p,len);
                buf[len] = '\0';

                if (strcasecmp(buf, ",inf") == 0) {
                    d = INFINITY; /* Positive infinite. */
                } else if (strcasecmp(buf, ",-inf") == 0) {
                    d = -INFINITY; /* Negative infinite. */
                } else {
                    d = strtod((char*)buf, &eptr);
                    if (buf[0] == '\0' || eptr[0] != '\0' || isnan(d)) {
                        __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                                "Bad double value");
                        return REDIS_ERR;
                    }
                }
                obj = r->fn->createDouble(cur, d, buf, len);
            } else {
                obj = (void*)REDIS_REPLY_DOUBLE;
            }
        } else if (cur->type == REDIS_REPLY_NIL) {
            if (r->fn && r->fn->createNil)
                obj = r->fn->createNil(cur);
            else
                obj = (void*)REDIS_REPLY_NIL;
        } else if (cur->type == REDIS_REPLY_BOOL) {
            int bval = p[0] == 't' || p[0] == 'T';

            if (r->fn && r->fn->createBool)
                obj = r->fn->createBool(cur, bval);
            else
                obj = (void*)REDIS_REPLY_BOOL;
        } else {
            /* Type will be error or status. */
            if (r->fn && r->fn->createString)
                obj = r->fn->createString(cur, p, len);
            else
                obj = (void*)(size_t)(cur->type);
        }

        if (obj == NULL) {
            __redisReaderSetErrorOOM(r);
            return REDIS_ERR;
        }

        /* Set reply if this is the root object. */
        if (r->ridx == 0)
            r->reply = obj;
        moveToNextTask(r);
        return REDIS_OK;
    }

    return REDIS_ERR;
}

static int
processBulkItem(redisReader *r)
{
    redisReadTask	*cur = &(r->rstack[r->ridx]);
    void		*obj = NULL;
    char		*p, *s;
    long long		len;
    unsigned long	bytelen;
    int			success = 0;

    p = r->buf + r->pos;
    s = seekNewline(p, r->len - r->pos);
    if (s != NULL) {
        p = r->buf + r->pos;
        bytelen = s - (r->buf + r->pos) + 2; /* include \r\n */

        if (string2ll(p, bytelen - 2, &len) == REDIS_ERR) {
            __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                    "Bad bulk string length");
            return REDIS_ERR;
        }

        if (len < -1 || (LLONG_MAX > SIZE_MAX && len > (long long)SIZE_MAX)) {
            __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                    "Bulk string length out of range");
            return REDIS_ERR;
        }

        if (len == -1) {
            /* The nil object can always be created. */
            if (r->fn && r->fn->createNil)
                obj = r->fn->createNil(cur);
            else
                obj = (void*)REDIS_REPLY_NIL;
            success = 1;
        } else {
            /* Only continue when the buffer contains the entire bulk item. */
            bytelen += len + 2; /* include \r\n */
            if (r->pos+bytelen <= r->len) {
                if (r->fn && r->fn->createString)
                    obj = r->fn->createString(cur, s + 2, len);
                else
                    obj = (void*)REDIS_REPLY_STRING;
                success = 1;
            }
        }

        /* Proceed when obj was created. */
        if (success) {
            if (obj == NULL) {
                __redisReaderSetErrorOOM(r);
                return REDIS_ERR;
            }

            r->pos += bytelen;

            /* Set reply if this is the root object. */
            if (r->ridx == 0)
                r->reply = obj;
            moveToNextTask(r);
            return REDIS_OK;
        }
    }

    return REDIS_ERR;
}

/* Process the array, map and set types. */
static int
processAggregateItem(redisReader *r)
{
    redisReadTask	*cur = &(r->rstack[r->ridx]);
    void		*obj;
    char		*p;
    long long		elements;
    int			root = 0, len;

    /* Set error for nested multi bulks with depth > 7 */
    if (r->ridx == 8) {
        __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
            "No support for nested multi bulk replies with depth > 7");
        return REDIS_ERR;
    }

    if ((p = readLine(r, &len)) != NULL) {
        if (string2ll(p, len, &elements) == REDIS_ERR) {
            __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                    "Bad multi-bulk length");
            return REDIS_ERR;
        }

        root = (r->ridx == 0);

        if (elements < -1 || (LLONG_MAX > SIZE_MAX && elements > SIZE_MAX)) {
            __redisReaderSetError(r, REDIS_ERR_PROTOCOL,
                    "Multi-bulk length out of range");
            return REDIS_ERR;
        }

        if (elements == -1) {
            if (r->fn && r->fn->createNil)
                obj = r->fn->createNil(cur);
            else
                obj = (void *)REDIS_REPLY_NIL;

            if (obj == NULL) {
                __redisReaderSetErrorOOM(r);
                return REDIS_ERR;
            }

            moveToNextTask(r);
        } else {
            if (cur->type == REDIS_REPLY_MAP) elements *= 2;

            if (r->fn && r->fn->createArray)
                obj = r->fn->createArray(cur, elements);
            else
                obj = (void *)(long)cur->type;

            if (obj == NULL) {
                __redisReaderSetErrorOOM(r);
                return REDIS_ERR;
            }

            /* Modify task stack when there are more than 0 elements. */
            if (elements > 0) {
                cur->elements = elements;
                cur->obj = obj;
                r->ridx++;
                r->rstack[r->ridx].type = REDIS_REPLY_UNKNOWN;
                r->rstack[r->ridx].elements = -1;
                r->rstack[r->ridx].idx = 0;
                r->rstack[r->ridx].obj = NULL;
                r->rstack[r->ridx].parent = cur;
                r->rstack[r->ridx].privdata = r->privdata;
            } else {
                moveToNextTask(r);
            }
        }

        /* Set reply if this is the root object. */
        if (root)
            r->reply = obj;
        return REDIS_OK;
    }

    return REDIS_ERR;
}

static int
processItem(redisReader *r)
{
    redisReadTask	*cur = &(r->rstack[r->ridx]);
    char		*p;

    /* check if we need to read type */
    if (cur->type < 0) {
        if ((p = readBytes(r, 1)) != NULL) {
            switch (p[0]) {
            case '-':
                cur->type = REDIS_REPLY_ERROR;
                break;
            case '+':
                cur->type = REDIS_REPLY_STATUS;
                break;
            case ':':
                cur->type = REDIS_REPLY_INTEGER;
                break;
            case ',':
                cur->type = REDIS_REPLY_DOUBLE;
                break;
            case '_':
                cur->type = REDIS_REPLY_NIL;
                break;
            case '$':
                cur->type = REDIS_REPLY_STRING;
                break;
            case '*':
                cur->type = REDIS_REPLY_ARRAY;
                break;
            case '%':
                cur->type = REDIS_REPLY_MAP;
                break;
            case '~':
                cur->type = REDIS_REPLY_SET;
                break;
            case '#':
                cur->type = REDIS_REPLY_BOOL;
                break;
            default:
                __redisReaderSetErrorProtocolByte(r, *p);
                return REDIS_ERR;
            }
        } else {
            /* could not consume 1 byte */
            return REDIS_ERR;
        }
    }

    /* process typed item */
    switch (cur->type) {
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_INTEGER:
    case REDIS_REPLY_DOUBLE:
    case REDIS_REPLY_NIL:
    case REDIS_REPLY_BOOL:
        return processLineItem(r);
    case REDIS_REPLY_STRING:
        return processBulkItem(r);
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
        return processAggregateItem(r);
    default:
        assert(NULL);
        return REDIS_ERR; /* Avoid warning. */
    }
}

redisReader *
redisReaderCreateWithFunctions(redisReplyObjectFunctions *fn)
{
    redisReader		*r;

    if ((r = calloc(1, sizeof(redisReader))) == NULL)
        return NULL;

    r->fn = fn;
    r->buf = sdsempty();
    r->maxbuf = REDIS_READER_MAX_BUF;
    if (r->buf == NULL) {
        free(r);
        return NULL;
    }

    r->ridx = -1;
    return r;
}

void
redisReaderFree(redisReader *r)
{
    if (r == NULL)
        return;
    if (r->reply != NULL && r->fn && r->fn->freeObject)
        r->fn->freeObject(r->reply);
    if (r->buf != NULL)
        sdsfree(r->buf);
    r->buf = NULL;
    free(r);
}

int
redisReaderFeed(redisReader *r, const char *buf, size_t len)
{
    sds			newbuf;

    /* Return early when this reader is in an erroneous state. */
    if (r->err)
        return REDIS_ERR;

    /* Copy the provided buffer. */
    if (buf != NULL && len >= 1) {
        /* Destroy internal buffer when it is empty and is quite large. */
        if (r->len == 0 && r->maxbuf != 0 && sdsavail(r->buf) > r->maxbuf) {
            sdsfree(r->buf);
            r->buf = sdsempty();
            r->pos = 0;

            /* r->buf should not be NULL since we just free'd a larger one. */
            assert(r->buf != NULL);
        }

        if ((newbuf = sdscatlen(r->buf, buf, len)) == NULL) {
            __redisReaderSetErrorOOM(r);
            return REDIS_ERR;
        }

        r->buf = newbuf;
        r->len = sdslen(r->buf);
    }

    return REDIS_OK;
}

int
redisReaderGetReply(redisReader *r, void **reply)
{
    /* Default target pointer to NULL. */
    if (reply != NULL)
        *reply = NULL;

    /* Return early when this reader is in an erroneous state. */
    if (r->err)
        return REDIS_ERR;

    /* When the buffer is empty, there will never be a reply. */
    if (r->len == 0)
        return REDIS_OK;

    /* Set first item to process when the stack is empty. */
    if (r->ridx == -1) {
        r->rstack[0].type = REDIS_REPLY_UNKNOWN;
        r->rstack[0].elements = -1;
        r->rstack[0].idx = -1;
        r->rstack[0].obj = NULL;
        r->rstack[0].parent = NULL;
        r->rstack[0].privdata = r->privdata;
        r->ridx = 0;
    }

    /* Process items in reply. */
    while (r->ridx >= 0)
        if (processItem(r) != REDIS_OK)
            break;

    /* Return ASAP when an error occurred. */
    if (r->err)
        return REDIS_ERR;

    /* Discard part of the buffer when we've consumed at least 1k, to avoid
     * doing unnecessary calls to memmove() in sds.c. */
    if (r->pos >= 1024) {
        sdsrange(r->buf, r->pos, -1);
        r->pos = 0;
        r->len = sdslen(r->buf);
    }

    /* Emit a reply when there is one. */
    if (r->ridx == -1) {
        if (reply != NULL) {
            *reply = r->reply;
        } else if (r->reply != NULL && r->fn && r->fn->freeObject) {
            r->fn->freeObject(r->reply);
        }
        r->reply = NULL;
    }
    return REDIS_OK;
}

static redisReply *createReplyObject(enum redisReplyType);
static void *createStringObject(const redisReadTask *, char *, size_t);
static void *createArrayObject(const redisReadTask *, int);
static void *createIntegerObject(const redisReadTask *, long long);
static void *createDoubleObject(const redisReadTask *, double, char *, size_t);
static void *createNilObject(const redisReadTask *);
static void *createBoolObject(const redisReadTask *, int);

/* Default set of functions to build the reply. Keep in mind that such a
 * function returning NULL is interpreted as OOM. */
static redisReplyObjectFunctions defaultFunctions = {
    createStringObject,
    createArrayObject,
    createIntegerObject,
    createDoubleObject,
    createNilObject,
    createBoolObject,
    freeReplyObject
};
static redisContextFuncs redisContextDefaultFuncs;

static redisReply *
createReplyObject(enum redisReplyType type)
{
    redisReply		*reply;

    if ((reply = calloc(1, sizeof(redisReply))) != NULL)
        reply->type = type;
    return reply;
}

/* Free a reply object */
void
freeReplyObject(void *r)
{
    redisReply		*reply;
    size_t		j;

    if ((reply = (redisReply *)r) == NULL)
        return;

    switch (reply->type) {
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
        if (reply->element != NULL) {
            for (j = 0; j < reply->elements; j++)
                if (reply->element[j] != NULL)
                    freeReplyObject(reply->element[j]);
            free(reply->element);
        }
        break;

    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_DOUBLE:
        if (reply->str != NULL)
            free(reply->str);
        break;

    case REDIS_REPLY_INTEGER:
    default:
        break; /* Nothing to free */
    }
    free(reply);
}

static void *
createStringObject(const redisReadTask *task, char *str, size_t len)
{
    redisReply		*reply, *parent;
    char		*buf;

    if ((reply = createReplyObject(task->type)) == NULL)
        return NULL;

    if ((buf = malloc(len + 1)) == NULL) {
        freeReplyObject(reply);
        return NULL;
    }

    assert(task->type == REDIS_REPLY_ERROR  ||
           task->type == REDIS_REPLY_STATUS ||
           task->type == REDIS_REPLY_STRING);

    /* Copy string value */
    memcpy(buf, str, len);
    buf[len] = '\0';
    reply->str = buf;
    reply->len = len;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;
}

static void *
createArrayObject(const redisReadTask *task, int elements)
{
    redisReply		*reply, *parent;

    if ((reply = createReplyObject(task->type)) == NULL)
        return NULL;

    if (elements > 0) {
        reply->element = calloc(elements,sizeof(redisReply*));
        if (reply->element == NULL) {
            freeReplyObject(reply);
            return NULL;
        }
    }

    reply->elements = elements;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;
}

static void *
createIntegerObject(const redisReadTask *task, long long value)
{
    redisReply		*reply, *parent;

    if ((reply = createReplyObject(REDIS_REPLY_INTEGER)) == NULL)
        return NULL;

    reply->integer = value;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;
}

static void *
createDoubleObject(const redisReadTask *task, double value, char *str, size_t len)
{
    redisReply		*reply, *parent;

    if ((reply = createReplyObject(REDIS_REPLY_DOUBLE)) == NULL)
        return NULL;

    reply->dval = value;
    if ((reply->str = malloc(len + 1)) == NULL) {
        freeReplyObject(reply);
        return NULL;
    }

    /* The double reply also has the original protocol string representing a
     * double as a null terminated string. This way the caller does not need
     * to format back for string conversion, especially since Redis does efforts
     * to make the string more human readable avoiding the calssical double
     * decimal string conversion artifacts.
     */
    memcpy(reply->str, str, len);
    reply->str[len] = '\0';

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;

}

static void *
createNilObject(const redisReadTask *task)
{
    redisReply		*reply, *parent;

    if ((reply = createReplyObject(REDIS_REPLY_NIL)) == NULL)
        return NULL;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;
}

static void *
createBoolObject(const redisReadTask *task, int bval)
{
    redisReply		*reply, *parent;

    if ((reply = createReplyObject(REDIS_REPLY_BOOL)) == NULL)
        return NULL;

    reply->integer = bval != 0;

    if (task->parent) {
        parent = task->parent->obj;
        assert(parent->type == REDIS_REPLY_ARRAY ||
               parent->type == REDIS_REPLY_MAP ||
               parent->type == REDIS_REPLY_SET);
        parent->element[task->idx] = reply;
    }
    return reply;
}

void
__redisSetError(redisContext *c, int type, const char *str)
{
    size_t		len;

    c->err = type;
    if (str != NULL) {
        len = strlen(str);
        len = len < (sizeof(c->errstr)-1) ? len : (sizeof(c->errstr)-1);
        memcpy(c->errstr, str, len);
        c->errstr[len] = '\0';
    } else {
        /* Only REDIS_ERR_IO may lack a description! */
        assert(type == REDIS_ERR_IO);
        __redis_strerror_r(errno, c->errstr, sizeof(c->errstr));
    }
}

redisReader *
redisReaderCreate(void)
{
    return redisReaderCreateWithFunctions(&defaultFunctions);
}

static redisContext *
redisContextInit(const redisOptions *options)
{
    redisContext		*c;

    if ((c = calloc(1, sizeof(redisContext))) == NULL)
        return NULL;

    c->funcs = &redisContextDefaultFuncs;
    c->obuf = sdsempty();
    c->reader = redisReaderCreate();
    c->fd = REDIS_INVALID_FD;

    if (c->obuf == NULL || c->reader == NULL) {
        redisFree(c);
        return NULL;
    }
    (void)options; /* options are used in other functions */
    return c;
}

void
redisFree(redisContext *c)
{
    if (c == NULL)
        return;
    redisNetClose(c);

    sdsfree(c->obuf);
    if (c->reader != NULL)
        redisReaderFree(c->reader);
    if (c->tcp.host)
        free(c->tcp.host);
    if (c->tcp.source_addr)
        free(c->tcp.source_addr);
    if (c->unix_sock.path)
        free(c->unix_sock.path);
    if (c->timeout)
        free(c->timeout);
    if (c->saddr)
        free(c->saddr);
    if (c->funcs->free_privdata)
        c->funcs->free_privdata(c->privdata);
    memset(c, 0, sizeof(*c));
    free(c);
}

redisFD redisFreeKeepFd(redisContext *c)
{
    redisFD		fd = c->fd;

    c->fd = REDIS_INVALID_FD;
    redisFree(c);
    return fd;
}

int
redisReconnect(redisContext *c)
{
    c->err = 0;
    memset(c->errstr, '\0', strlen(c->errstr));

    if (c->privdata && c->funcs->free_privdata) {
        c->funcs->free_privdata(c->privdata);
        c->privdata = NULL;
    }

    redisNetClose(c);

    sdsfree(c->obuf);
    redisReaderFree(c->reader);

    c->obuf = sdsempty();
    c->reader = redisReaderCreate();

    if (c->connection_type == REDIS_CONN_TCP)
        return redisContextConnectBindTcp(c, c->tcp.host, c->tcp.port,
                c->timeout, c->tcp.source_addr);
    else if (c->connection_type == REDIS_CONN_UNIX)
        return redisContextConnectUnix(c, c->unix_sock.path, c->timeout);
    /* Something bad happened here and shouldn't have. */
    /* There isn't enough information in the context to reconnect. */
    __redisSetError(c, REDIS_ERR_OTHER, "Not enough information to reconnect");
    return REDIS_ERR;
}

redisContext *
redisConnectWithOptions(const redisOptions *options)
{
    redisContext	*c = redisContextInit(options);

    if (c == NULL)
        return NULL;
    if (!(options->options & REDIS_OPT_NONBLOCK))
        c->flags |= REDIS_BLOCK;
    if (options->options & REDIS_OPT_REUSEADDR)
        c->flags |= REDIS_REUSEADDR;
    if (options->options & REDIS_OPT_NOAUTOFREE)
        c->flags |= REDIS_NO_AUTO_FREE;

    if (options->type == REDIS_CONN_TCP)
        redisContextConnectBindTcp(c, options->endpoint.tcp.ip,
                                   options->endpoint.tcp.port, options->timeout,
                                   options->endpoint.tcp.source_addr);
    else if (options->type == REDIS_CONN_UNIX)
        redisContextConnectUnix(c, options->endpoint.unix_socket,
                                options->timeout);
    else if (options->type == REDIS_CONN_USERFD) {
        c->fd = options->endpoint.fd;
        c->flags |= REDIS_CONNECTED;
    }
    else
        return NULL;

    if (options->timeout != NULL && (c->flags & REDIS_BLOCK) && c->fd != REDIS_INVALID_FD)
        redisContextSetTimeout(c, *options->timeout);
    return c;
}

/*
 * Connect to a Redis instance. On error the field error in the returned
 * context will be set to the return value of the error function.
 * When no set of reply functions is given, the default set will be used.
 */
redisContext *
redisConnect(const char *ip, int port)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectWithTimeout(const char *ip, int port, const struct timeval tv)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.timeout = &tv;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectNonBlock(const char *ip, int port)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.options |= REDIS_OPT_NONBLOCK;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectBindNonBlock(const char *ip, int port, const char *source_addr)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.endpoint.tcp.source_addr = source_addr;
    options.options |= REDIS_OPT_NONBLOCK;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectBindNonBlockWithReuse(const char *ip, int port, const char *source_addr)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.endpoint.tcp.source_addr = source_addr;
    options.options |= REDIS_OPT_NONBLOCK|REDIS_OPT_REUSEADDR;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectUnix(const char *path)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_UNIX(&options, path);
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectUnixWithTimeout(const char *path, const struct timeval tv)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_UNIX(&options, path);
    options.timeout = &tv;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectUnixNonBlock(const char *path)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_UNIX(&options, path);
    options.options |= REDIS_OPT_NONBLOCK;
    return redisConnectWithOptions(&options);
}

redisContext *
redisConnectFd(redisFD fd)
{
    redisOptions	options = {0};

    options.type = REDIS_CONN_USERFD;
    options.endpoint.fd = fd;
    return redisConnectWithOptions(&options);
}

/* Set read/write timeout on a blocking socket. */
int
redisSetTimeout(redisContext *c, const struct timeval tv)
{
    if (c->flags & REDIS_BLOCK)
        return redisContextSetTimeout(c, tv);
    return REDIS_ERR;
}

/* Enable connection KeepAlive. */
int
redisAsyncEnableKeepAlive(redisAsyncContext *ac)
{
    if (redisKeepAlive(&ac->c, REDIS_KEEPALIVE_INTERVAL) != REDIS_OK)
        return REDIS_ERR;
    return REDIS_OK;
}

/*
 * Use this function to handle a read event on the descriptor. It will try
 * and read some bytes from the socket and feed them to the reply parser.
 *
 * After this function is called, you may use redisContextReadReply to
 * see if there is a reply available.
 */
int
redisBufferRead(redisContext *c)
{
    char		buf[1024*16];
    int			nread;

    /* Return early when the context has seen an error. */
    if (c->err)
        return REDIS_ERR;

    nread = c->funcs->read(c, buf, sizeof(buf));
    if (nread > 0) {
        if (redisReaderFeed(c->reader, buf, nread) != REDIS_OK) {
            __redisSetError(c, c->reader->err, c->reader->errstr);
            return REDIS_ERR;
        }
    } else if (nread < 0) {
        return REDIS_ERR;
    }
    return REDIS_OK;
}

/*
 * Write the output buffer to the socket.
 *
 * Returns REDIS_OK when the buffer is empty, or (a part of) the buffer was
 * successfully written to the socket. When the buffer is empty after the
 * write operation, "done" is set to 1 (if given).
 *
 * Returns REDIS_ERR if an error occurred trying to write and sets
 * c->errstr to hold the appropriate error string.
 */
int
redisBufferWrite(redisContext *c, int *done)
{
    /* Return early when the context has seen an error. */
    if (c->err)
        return REDIS_ERR;

    if (sdslen(c->obuf) > 0) {
        int nwritten = c->funcs->write(c);
        if (nwritten < 0) {
            return REDIS_ERR;
        } else if (nwritten > 0) {
            if (nwritten == (signed)sdslen(c->obuf)) {
                sdsfree(c->obuf);
                c->obuf = sdsempty();
            } else {
                sdsrange(c->obuf, nwritten, -1);
            }
        }
    }
    if (done != NULL)
        *done = (sdslen(c->obuf) == 0);
    return REDIS_OK;
}

/*
 * Internal helper function to try and get a reply from the reader,
 * or set an error in the context otherwise.
 */
int
redisGetReplyFromReader(redisContext *c, void **reply)
{
    if (redisReaderGetReply(c->reader, reply) == REDIS_ERR) {
        __redisSetError(c, c->reader->err, c->reader->errstr);
        return REDIS_ERR;
    }
    return REDIS_OK;
}

int
redisGetReply(redisContext *c, void **reply)
{
    int			wdone = 0;
    void		*aux = NULL;

    /* Try to read pending replies */
    if (redisGetReplyFromReader(c, &aux) == REDIS_ERR)
        return REDIS_ERR;

    /* For the blocking context, flush output buffer and read reply */
    if (aux == NULL && c->flags & REDIS_BLOCK) {
        /* Write until done */
        do {
            if (redisBufferWrite(c, &wdone) == REDIS_ERR)
                return REDIS_ERR;
        } while (!wdone);

        /* Read until there is a reply */
        do {
            if (redisBufferRead(c) == REDIS_ERR)
                return REDIS_ERR;
            if (redisGetReplyFromReader(c, &aux) == REDIS_ERR)
                return REDIS_ERR;
        } while (aux == NULL);
    }

    /* Set reply or free it if we were passed NULL */
    if (reply != NULL) {
        *reply = aux;
    } else {
        freeReplyObject(aux);
    }
    return REDIS_OK;
}

/* Write a formatted command to the output buffer. */
int
__redisAppendCommand(redisContext *c, const sds cmd)
{
    sds			newbuf;

    if ((newbuf = sdscatsds(c->obuf, cmd)) == NULL) {
        __redisSetError(c, REDIS_ERR_OOM, "Out of memory");
        return REDIS_ERR;
    }
    c->obuf = newbuf;
    return REDIS_OK;
}

/* Functions managing dictionary of callbacks for pub/sub. */
static uint64_t
callbackHash(const void *key)
{
    const unsigned char	*hashkey = (const unsigned char *)key;

    return dictGenHashFunction(hashkey, sdslen((const sds)key));
}

static void *
callbackValDup(void *privdata, const void *src)
{
    redisCallBack	*dup = zmalloc(sizeof(*dup));

    (void)privdata;
    memcpy(dup, src, sizeof(*dup));
    return dup;
}

static int
callbackKeyCompare(void *privdata, const void *key1, const void *key2)
{
    int			l1, l2;

    (void)privdata;
    l1 = sdslen((const sds)key1);
    l2 = sdslen((const sds)key2);
    if (l1 != l2)
        return 0;
    return memcmp(key1, key2, l1) == 0;
}

static void
callbackKeyDestructor(void *privdata, void *key)
{
    (void)privdata;
    sdsfree((sds)key);
}

static void
callbackValDestructor(void *privdata, void *val)
{
    (void)privdata;
    free(val);
}

static dictType callbackDict = {
    .hashFunction	= callbackHash,
    .valDup		= callbackValDup,
    .keyCompare		= callbackKeyCompare,
    .keyDestructor	= callbackKeyDestructor,
    .valDestructor	= callbackValDestructor,
};

static redisAsyncContext *
redisAsyncInitialize(redisContext *c)
{
    redisAsyncContext	*ac;

    if ((ac = realloc(c, sizeof(redisAsyncContext))) == NULL)
        return NULL;

    c = &(ac->c);

    /*
     * The regular connect functions will always set the flag REDIS_CONNECTED.
     * For the async API, we want to wait until the first write event is
     * received up before setting this flag, so reset it here.
     */
    c->flags &= ~REDIS_CONNECTED;

    ac->err = 0;
    ac->errstr = NULL;
    ac->data = NULL;
    ac->onConnect = NULL;
    ac->onDisconnect = NULL;
    memset(&ac->ev, 0, sizeof(ac->ev));
    memset(&ac->replies, 0, sizeof(ac->replies));
    memset(&ac->sub, 0, sizeof(ac->sub));
    ac->sub.channels = dictCreate(&callbackDict, NULL);
    ac->sub.patterns = dictCreate(&callbackDict, NULL);
    return ac;
}

/*
 * We want the error field to be accessible directly instead of requiring
 * an indirection to the redisContext struct.
 * */
static void
__redisAsyncCopyError(redisAsyncContext *ac)
{
    redisContext	*c;

    if (!ac)
        return;
    c = &(ac->c);
    ac->err = c->err;
    ac->errstr = c->errstr;
}

redisAsyncContext *
redisAsyncConnectWithOptions(const redisOptions *options)
{
    redisOptions	myOptions = *options;
    redisContext	*c;
    redisAsyncContext	*ac;

    myOptions.options |= REDIS_OPT_NONBLOCK;
    c = redisConnectWithOptions(&myOptions);
    if (c == NULL)
        return NULL;

    ac = redisAsyncInitialize(c);
    if (ac == NULL) {
        redisFree(c);
        return NULL;
    }

    __redisAsyncCopyError(ac);
    return ac;
}

redisAsyncContext *
redisAsyncConnect(const char *ip, int port)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    return redisAsyncConnectWithOptions(&options);
}

redisAsyncContext *
redisAsyncConnectBind(const char *ip, int port, const char *source_addr)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.endpoint.tcp.source_addr = source_addr;
    return redisAsyncConnectWithOptions(&options);
}

redisAsyncContext *
redisAsyncConnectBindWithReuse(const char *ip, int port, const char *source_addr)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    options.options |= REDIS_OPT_REUSEADDR;
    options.endpoint.tcp.source_addr = source_addr;
    return redisAsyncConnectWithOptions(&options);
}

redisAsyncContext *
redisAsyncConnectUnix(const char *path)
{
    redisOptions	options = {0};

    REDIS_OPTIONS_SET_UNIX(&options, path);
    return redisAsyncConnectWithOptions(&options);
}

int
redisAsyncSetConnectCallBack(redisAsyncContext *ac, redisConnectCallBack *func)
{
    if (ac->onConnect == NULL) {
        ac->onConnect = func;
        /*
         * The common way to detect an established connection is to wait for
         * the first write event to be fired. This assumes the related event
         * library functions are already set.
         */
        REDIS_EV_ADD_WRITE(ac);
        return REDIS_OK;
    }
    return REDIS_ERR;
}

int
redisAsyncSetDisconnectCallBack(redisAsyncContext *ac, redisDisconnectCallBack *func)
{
    if (ac->onDisconnect == NULL) {
        ac->onDisconnect = func;
        return REDIS_OK;
    }
    return REDIS_ERR;
}

/* Helper functions to push/shift callbacks */
static int
__redisPushCallBack(redisCallBackList *list, redisCallBack *source)
{
    redisCallBack	*cb;

    /* Copy callback from stack to heap */
    if ((cb = zcalloc(1, sizeof(*cb))) == NULL)
        return REDIS_ERR_OOM;

    if (source != NULL) {
        memcpy(cb, source, sizeof(*cb));
        cb->next = NULL;
    }

    /* Store callback in list */
    if (list->head == NULL)
        list->head = cb;
    if (list->tail != NULL)
        list->tail->next = cb;
    list->tail = cb;
    return REDIS_OK;
}

static int
__redisShiftCallBack(redisCallBackList *list, redisCallBack *target)
{
    redisCallBack	*cb = list->head;

    if (cb != NULL) {
        list->head = cb->next;
        if (cb == list->tail)
            list->tail = NULL;

        /* Copy callback from heap to stack */
        if (target != NULL)
            memcpy(target,cb,sizeof(*cb));
        free(cb);
        return REDIS_OK;
    }
    return REDIS_ERR;
}

static void
__redisRunCallBack(redisAsyncContext *ac, redisCallBack *cb, redisReply *reply)
{
    redisContext	*c = &(ac->c);

    if (cb->func) {
        c->flags |= REDIS_IN_CALLBACK;
        cb->func(ac, reply, cb->command, cb->privdata);
        c->flags &= ~REDIS_IN_CALLBACK;
    }
}

/* Helper function to free the context. */
static void
__redisAsyncFree(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);
    redisCallBack	cb;
    dictIterator	*it;
    dictEntry		*de;

    /* Execute pending callbacks with NULL reply. */
    while (__redisShiftCallBack(&ac->replies, &cb) == REDIS_OK)
        __redisRunCallBack(ac, &cb, NULL);

    /* Execute callbacks for invalid commands */
    while (__redisShiftCallBack(&ac->sub.invalid, &cb) == REDIS_OK)
        __redisRunCallBack(ac, &cb, NULL);

    /* Run subscription callbacks callbacks with NULL reply */
    it = dictGetIterator(ac->sub.channels);
    while ((de = dictNext(it)) != NULL)
        __redisRunCallBack(ac, dictGetVal(de), NULL);
    dictReleaseIterator(it);
    dictRelease(ac->sub.channels);

    it = dictGetIterator(ac->sub.patterns);
    while ((de = dictNext(it)) != NULL)
        __redisRunCallBack(ac, dictGetVal(de), NULL);
    dictReleaseIterator(it);
    dictRelease(ac->sub.patterns);

    /* Signal event lib to clean up */
    REDIS_EV_CLEANUP(ac);

    /*
     * Execute disconnect callback. When redisAsyncFree() initiated destroying
     * this context, the status will always be REDIS_OK.
     */
    if (ac->onDisconnect && (c->flags & REDIS_CONNECTED)) {
        if (c->flags & REDIS_FREEING) {
            ac->onDisconnect(ac, REDIS_OK);
        } else {
            ac->onDisconnect(ac, (ac->err == 0) ? REDIS_OK : REDIS_ERR);
        }
    }

    /* Cleanup self */
    redisFree(c);
}

/*
 * Free the async context. When this function is called from a callback,
 * control needs to be returned to redisProcessCallBacks() before actual
 * free'ing. To do so, a flag is set on the context which is picked up by
 * redisProcessCallBacks(). Otherwise, the context is immediately free'd.
 */
void
redisAsyncFree(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);

    c->flags |= REDIS_FREEING;
    if (!(c->flags & REDIS_IN_CALLBACK))
        __redisAsyncFree(ac);
}

/* Helper function to make the disconnect happen and clean up. */
void
__redisAsyncDisconnect(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);
    int			sts;

    /* Make sure error is accessible if there is any */
    __redisAsyncCopyError(ac);

    if (ac->err == 0) {
        /* For clean disconnects, there should be no pending callbacks. */
        sts = __redisShiftCallBack(&ac->replies,NULL);
        assert(sts == REDIS_ERR);
    } else {
        /* Disconnection is caused by an error, make sure that pending
         * callbacks cannot call new commands. */
        c->flags |= REDIS_DISCONNECTING;
	if (pmDebugOptions.series) {
	    fprintf(stderr, "%s: ERROR %d \"%s\"\n", "__redisAsyncDisconnect",
	    	ac->err, ac->errstr);
	}
    }

    /* cleanup event library on disconnect.
     * this is safe to call multiple times */
    REDIS_EV_CLEANUP(ac);

    /* For non-clean disconnects, __redisAsyncFree() will execute pending
     * callbacks with a NULL-reply. */
    if (!(c->flags & REDIS_NO_AUTO_FREE)) {
        __redisAsyncFree(ac);
    }
}

/*
 * Tries to do a clean disconnect from Redis, meaning it stops new commands
 * from being issued, but tries to flush the output buffer and execute
 * callbacks for all remaining replies. When this function is called from a
 * callback, there might be more replies and we can safely defer disconnecting
 * to redisProcessCallBacks(). Otherwise, we can only disconnect immediately
 * when there are no pending callbacks.
 */
void
redisAsyncDisconnect(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);

    c->flags |= REDIS_DISCONNECTING;
    /* unset the auto-free flag here, because disconnect undoes this */
    c->flags &= ~REDIS_NO_AUTO_FREE;

    if (!(c->flags & REDIS_IN_CALLBACK) && ac->replies.head == NULL)
        __redisAsyncDisconnect(ac);
}

static int
__redisGetSubscribeCallBack(redisAsyncContext *ac, redisReply *reply, redisCallBack *dstcb)
{
    redisContext	*c = &(ac->c);
    dict		*callbacks;
    redisCallBack	*cb;
    dictEntry		*de;
    int			pvariant;
    char		*stype;
    sds			sname;

    /*
     * Custom reply functions are not supported for pub/sub.
     * This will fail very hard when they are used...
     */
    if (reply->type == REDIS_REPLY_ARRAY) {
        assert(reply->elements >= 2);
        assert(reply->element[0]->type == REDIS_REPLY_STRING);
        stype = reply->element[0]->str;
        pvariant = (tolower((int)stype[0]) == 'p') ? 1 : 0;

        if (pvariant)
            callbacks = ac->sub.patterns;
        else
            callbacks = ac->sub.channels;

        /* Locate the right callback */
        assert(reply->element[1]->type == REDIS_REPLY_STRING);
        sname = sdsnewlen(reply->element[1]->str, reply->element[1]->len);
        de = dictFind(callbacks, sname);
        if (de != NULL) {
            cb = dictGetVal(de);

            /* If this is an subscribe reply decrease pending counter. */
            if (strcasecmp(stype+pvariant,"subscribe") == 0) {
                cb->pending_subs -= 1;
            }

            memcpy(dstcb, dictGetVal(de), sizeof(*dstcb));

            /* If this is an unsubscribe message, remove it. */
            if (strcasecmp(stype+pvariant, "unsubscribe") == 0) {
                if (cb->pending_subs == 0)
                    dictDelete(callbacks, sname);

                /*
                 * If this was the last unsubscribe message, revert to
                 * non-subscribe mode.
                 */
                assert(reply->element[2]->type == REDIS_REPLY_INTEGER);
                if (reply->element[2]->integer == 0)
                    c->flags &= ~REDIS_SUBSCRIBED;
            }
        }
        sdsfree(sname);
        return REDIS_OK;
    }
    /* Shift callback for invalid commands. */
    return __redisShiftCallBack(&ac->sub.invalid, dstcb);
}

void
redisPrintReply(FILE *fp, redisReply *reply, int level)
{
    size_t	i;
    char	hashbuf[42];

    if (reply == NULL)
    	return;
    for(i=0; i < level; i++)
    	fputc('|', fp);
    fprintf(fp, "REPLY type=%s ", redis_reply_type(reply));
    switch (reply->type) {
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_ERROR:
    	fprintf(fp, "len=%d ", (int)reply->len);
	if (reply->len == 20) {
	    pmwebapi_hash_str((unsigned char *)reply->str, hashbuf, sizeof(hashbuf));
	    fprintf(fp, "SID %s\n", hashbuf);
	}
	else 
	    fprintf(fp, "\"%s\"\n", reply->str);
	break;
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
	fprintf(fp, "elements=%zu\n", reply->elements);
    	for (i = 0; i < reply->elements; i++) {
	    fprintf(fp, "[%zu]", i);
	    redisPrintReply(fp, reply->element[i], level+1);
	}
	break;
    default:
    	fprintf(fp, "\n");
	break;
    }
}

void
redisProcessCallBacks(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);
    redisCallBack	cb;
    redisReply		*reply = NULL;
    int			status;

    if (pmDebugOptions.series)
	fprintf(stderr, "CALLBACK %s: flags=0x%04x replies=%p\n", "redisProcessCallBacks",
		c->flags, ac->replies.head);

    memset(&cb, 0, sizeof(cb));
    while ((status = redisGetReply(c, (void **)&reply)) == REDIS_OK) {
	if (pmDebugOptions.series)
	    redisPrintReply(stderr, reply, 1);
        if (reply == NULL) {
            /*
             * When the connection is being disconnected and there are
             * no more replies, this is the cue to really disconnect.
             */
            if (c->flags & REDIS_DISCONNECTING &&
        	sdslen(c->obuf) == 0 &&
        	ac->replies.head == NULL) {
        	__redisAsyncDisconnect(ac);
        	return;
            }

            /* If monitor mode, repush callback */
            if (c->flags & REDIS_MONITORING)
                __redisPushCallBack(&ac->replies, &cb);

            /*
             * When the connection is not being disconnected, simply stop
             * trying to get replies and wait for the next loop tick.
             */
            break;
        }

        /*
         * Even if the context is subscribed, pending regular callbacks will
         * get a reply before pub/sub messages arrive.
         */
        if (__redisShiftCallBack(&ac->replies, &cb) != REDIS_OK) {
            /*
             * A spontaneous reply in a not-subscribed context can be the error
             * reply that is sent when a new connection exceeds the maximum
             * number of allowed connections on the server side.
             *
             * This is seen as an error instead of a regular reply because the
             * server closes the connection after sending it.
             *
             * To prevent the error from being overwritten by an EOF error the
             * connection is closed here. See issue #43.
             *
             * Another possibility is that the server is loading its dataset.
             * In this case we also want to close the connection, and have the
             * user wait until the server is ready to take our request.
             */
            if (reply->type == REDIS_REPLY_ERROR) {
                c->err = REDIS_ERR_OTHER;
                pmsprintf(c->errstr, sizeof(c->errstr), "%s", reply->str);
                c->reader->fn->freeObject(reply);
                __redisAsyncDisconnect(ac);
                return;
            }
            /* No more regular callbacks and no errors, the context *must* be subscribed or monitoring. */
            assert((c->flags & REDIS_SUBSCRIBED || c->flags & REDIS_MONITORING));
            if (c->flags & REDIS_SUBSCRIBED)
                __redisGetSubscribeCallBack(ac, reply, &cb);
        }

        if (cb.func) {
            __redisRunCallBack(ac, &cb, reply);
            c->reader->fn->freeObject(reply);

            /* Proceed with free'ing when redisAsyncFree() was called. */
            if (c->flags & REDIS_FREEING) {
                __redisAsyncFree(ac);
                return;
            }
        } else {
            /*
             * No callback for this reply. This can either be a NULL callback,
             * or there were no callbacks to begin with. Either way, don't
             * abort with an error, but simply ignore it because the client
             * doesn't know what the server will spit out over the wire.
             */
            c->reader->fn->freeObject(reply);
        }
    }

    /* Disconnect when there was an error reading the reply */
    if (status != REDIS_OK)
        __redisAsyncDisconnect(ac);
}

/*
 * Internal helper function to detect socket status the first time a read or
 * write event fires. When connecting was not successful, the connect callback
 * is called with a REDIS_ERR status and the context is free'd.
 */
static int
__redisAsyncHandleConnect(redisAsyncContext *ac)
{
    int			completed = 0;
    redisContext	*c = &(ac->c);

    if (redisCheckConnectDone(c, &completed) == REDIS_ERR) {
        /* Error! */
        redisCheckSocketError(c);
        if (ac->onConnect) ac->onConnect(ac, REDIS_ERR);
        __redisAsyncDisconnect(ac);
        return REDIS_ERR;
    } else if (completed == 1) {
        /* connected! */
        if (ac->onConnect) ac->onConnect(ac, REDIS_OK);
        c->flags |= REDIS_CONNECTED;
        return REDIS_OK;
    } else {
        return REDIS_OK;
    }
}

void
redisAsyncRead(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);

    if (redisBufferRead(c) == REDIS_ERR) {
        __redisAsyncDisconnect(ac);
    } else {
        /* Always re-schedule reads */
        REDIS_EV_ADD_READ(ac);
        redisProcessCallBacks(ac);
    }
}

/*
 * This function should be called when the socket is readable.
 * It processes all replies that can be read and executes their callbacks.
 */
void
redisAsyncHandleRead(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);

    if (!(c->flags & REDIS_CONNECTED)) {
        /* Abort connect was not successful. */
        if (__redisAsyncHandleConnect(ac) != REDIS_OK)
            return;
        /* Try again later when the context is still not connected. */
        if (!(c->flags & REDIS_CONNECTED))
            return;
    }

    c->funcs->async_read(ac);
}

void
redisAsyncWrite(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);
    int			done = 0;

    if (redisBufferWrite(c, &done) == REDIS_ERR) {
        __redisAsyncDisconnect(ac);
    } else {
        /* Continue writing when not done, stop writing otherwise */
        if (!done)
            REDIS_EV_ADD_WRITE(ac);
        else
            REDIS_EV_DEL_WRITE(ac);

        /* Always schedule reads after writes */
        REDIS_EV_ADD_READ(ac);
    }
}

void
redisAsyncHandleWrite(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);

    if (!(c->flags & REDIS_CONNECTED)) {
        /* Abort connect was not successful. */
        if (__redisAsyncHandleConnect(ac) != REDIS_OK)
            return;
        /* Try again later when the context is still not connected. */
        if (!(c->flags & REDIS_CONNECTED))
            return;
    }

    c->funcs->async_write(ac);
}

void
redisAsyncHandleTimeout(redisAsyncContext *ac)
{
    redisContext	*c = &(ac->c);
    redisCallBack	cb;

    if ((c->flags & REDIS_CONNECTED) && ac->replies.head == NULL) {
        /* Nothing to do - just an idle timeout */
        return;
    }

    if (!c->err) {
        __redisSetError(c, REDIS_ERR_TIMEOUT, "Timeout");
    }

    if (!(c->flags & REDIS_CONNECTED) && ac->onConnect) {
        ac->onConnect(ac, REDIS_ERR);
    }

    while (__redisShiftCallBack(&ac->replies, &cb) == REDIS_OK) {
        __redisRunCallBack(ac, &cb, NULL);
    }

    /**
     * TODO: Don't automatically sever the connection,
     * rather, allow to ignore <x> responses before the queue is clear
     */
    __redisAsyncDisconnect(ac);
}

/*
 * Sets a pointer to the first argument and its length starting at p.
 * Returns the number of bytes to skip to get to the following argument.
 */
static const char *
nextArgument(const char *start, const char **str, size_t *len)
{
    const char		*p = start;

    if (p[0] != '$') {
        p = strchr(p, '$');
        if (p == NULL)
            return NULL;
    }

    *len = (int)strtol(p+1, NULL, 10);
    p = strchr(p, '\r');
    assert(p);
    *str = p + 2;
    return p + 2 + (*len) + 2;
}

/*
 * Helper function for the redisAsyncCommand* family of functions.
 * Writes a formatted command to the output buffer and registers the
 * provided callback function with the context.
 */
static int
__redisAsyncCommand(redisAsyncContext *ac, redisAsyncCallBack *func,
               const sds cmd, void *privdata)
{
    redisContext	*c = &(ac->c);
    redisCallBack	cb = {0};
    struct dict		*cbdict;
    dictEntry		*de;
    redisCallBack	*existcb;
    int			pvariant, hasnext;
    const char		*cstr, *astr;
    size_t		clen, alen;
    const char		*p;
    sds			sname;
    int			sts;

    /* Don't accept new commands when the connection is about to be closed. */
    if (c->flags & (REDIS_DISCONNECTING | REDIS_FREEING))
        return REDIS_ERR;

    /* Setup callback */
    cb.func = func;
    cb.privdata = privdata;
    cb.command = (sds)cmd;
    cb.pending_subs = 1;

    /* Find out which command will be appended. */
    p = nextArgument(cmd, &cstr, &clen);
    assert(p != NULL);
    hasnext = (p[0] == '$');
    pvariant = (tolower((int)cstr[0]) == 'p') ? 1 : 0;
    cstr += pvariant;
    clen -= pvariant;

    if (hasnext && strncasecmp(cstr, "subscribe\r\n", 11) == 0) {
        c->flags |= REDIS_SUBSCRIBED;

        /* Add every channel/pattern to the list of subscription callbacks. */
        while ((p = nextArgument(p, &astr, &alen)) != NULL) {
            sname = sdsnewlen(astr, alen);
            if (pvariant)
                cbdict = ac->sub.patterns;
            else
                cbdict = ac->sub.channels;

            de = dictFind(cbdict,sname);
            if (de != NULL) {
                existcb = dictGetVal(de);
        	cb.pending_subs = existcb->pending_subs + 1;
            }

            sts = dictReplace(cbdict, sname, &cb);
            if (sts == 0)
                sdsfree(sname);
        }
    } else if (strncasecmp(cstr, "unsubscribe\r\n", 13) == 0) {
        /*
         * It is only useful to call (P)UNSUBSCRIBE when the context is
         * subscribed to one or more channels or patterns.
         */
        if (!(c->flags & REDIS_SUBSCRIBED))
            return REDIS_ERR;

        /*
         * (P)UNSUBSCRIBE does not have its own response: every channel or
         * pattern that is unsubscribed will receive a message. This means
         * we should not append a callback function for this command.
         */
    } else if (strncasecmp(cstr, "monitor\r\n", 9) == 0) {
        /* Set monitor flag and push callback */
        c->flags |= REDIS_MONITORING;
        __redisPushCallBack(&ac->replies, &cb);
    } else {
        if (c->flags & REDIS_SUBSCRIBED)
            /*
             * This will likely result in an error reply, but it needs to be
             * received and passed to the callback.
             */
            __redisPushCallBack(&ac->sub.invalid, &cb);
        else
            __redisPushCallBack(&ac->replies, &cb);
    }

    __redisAppendCommand(c, cmd);

    /* Always schedule a write when the write buffer is non-empty */
    REDIS_EV_ADD_WRITE(ac);

    return REDIS_OK;
}

int
redisAsyncFormattedCommand(redisAsyncContext *ac, redisAsyncCallBack *func,
                const sds command, void *privdata)
{
    return __redisAsyncCommand(ac, func, command, privdata);
}

void
redisAsyncSetTimeout(redisAsyncContext *ac, struct timeval tv)
{
    if (!ac->c.timeout)
        ac->c.timeout = zcalloc(1, sizeof(tv));

    if (tv.tv_sec == ac->c.timeout->tv_sec &&
        tv.tv_usec == ac->c.timeout->tv_usec)
        return;

    *ac->c.timeout = tv;
}

static redisContextFuncs redisContextDefaultFuncs = {
    .free_privdata = NULL,
    .async_read = redisAsyncRead,
    .async_write = redisAsyncWrite,
    .read = redisNetRead,
    .write = redisNetWrite
};

#if !defined(HAVE_OPENSSL)
int redisSecureConnection(redisContext *c, const char *a, const char *b,
                          const char *d, const char *e)
{
    (void)a; (void)b; (void)c; (void)d; (void)e;
    return REDIS_ERR;
}
int redisInitiateSSL(redisContext *c, struct ssl_st *s)
{
    (void)c; (void)s;
    return REDIS_ERR;
}
#endif
