/*
 * Copyright (c) 2015-2017, Ieshen Zheng <ieshen.zheng at 163 dot com>
 * Copyright (c) 2020, Nick <heronr1 at gmail dot com>
 * Copyright (c) 2020-2021, Bjorn Svensson <bjorn.a.svensson at est dot tech>
 * Copyright (c) 2020-2021, Viktor Söderqvist <viktor.soderqvist at est dot tech>
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

#ifndef __HIUTIL_H_
#define __HIUTIL_H_

#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#define HI_OK 0
#define HI_ERROR -1
#define HI_EAGAIN -2

typedef int rstatus_t; /* return type */

#define LF (uint8_t)10
#define CR (uint8_t)13
#define CRLF "\x0d\x0a"
#define CRLF_LEN (sizeof("\x0d\x0a") - 1)

#define NELEMS(a) ((sizeof(a)) / sizeof((a)[0]))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define SQUARE(d) ((d) * (d))
#define VAR(s, s2, n) (((n) < 2) ? 0.0 : ((s2)-SQUARE(s) / (n)) / ((n)-1))
#define STDDEV(s, s2, n) (((n) < 2) ? 0.0 : sqrt(VAR((s), (s2), (n))))

#define HI_INET4_ADDRSTRLEN (sizeof("255.255.255.255") - 1)
#define HI_INET6_ADDRSTRLEN                                                    \
    (sizeof("ffff:ffff:ffff:ffff:ffff:ffff:255.255.255.255") - 1)
#define HI_INET_ADDRSTRLEN MAX(HI_INET4_ADDRSTRLEN, HI_INET6_ADDRSTRLEN)
#define HI_UNIX_ADDRSTRLEN                                                     \
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

#define HI_MAXHOSTNAMELEN 256

/*
 * Length of 1 byte, 2 bytes, 4 bytes, 8 bytes and largest integral
 * type (uintmax_t) in ascii, including the null terminator '\0'
 *
 * From stdint.h, we have:
 * # define UINT8_MAX   (255)
 * # define UINT16_MAX  (65535)
 * # define UINT32_MAX  (4294967295U)
 * # define UINT64_MAX  (__UINT64_C(18446744073709551615))
 */
#define HI_UINT8_MAXLEN (3 + 1)
#define HI_UINT16_MAXLEN (5 + 1)
#define HI_UINT32_MAXLEN (10 + 1)
#define HI_UINT64_MAXLEN (20 + 1)
#define HI_UINTMAX_MAXLEN HI_UINT64_MAXLEN

/*
 * Make data 'd' or pointer 'p', n-byte aligned, where n is a power of 2
 * of 2.
 */
#define HI_ALIGNMENT sizeof(unsigned long) /* platform word */
#define HI_ALIGN(d, n) (((d) + (n - 1)) & ~(n - 1))
#define HI_ALIGN_PTR(p, n)                                                     \
    (void *)(((uintptr_t)(p) + ((uintptr_t)n - 1)) & ~((uintptr_t)n - 1))

/*
 * Wrapper to workaround well known, safe, implicit type conversion when
 * invoking system calls.
 */
#define hi_gethostname(_name, _len) gethostname((char *)_name, (size_t)_len)

#define hi_atoi(_line, _n) _hi_atoi((uint8_t *)_line, (size_t)_n)
#define hi_itoa(_line, _n) _hi_itoa((uint8_t *)_line, (int)_n)

#define uint_len(_n) _uint_len((uint32_t)_n)

#ifndef WIN32
int hi_set_blocking(int sd);
int hi_set_nonblocking(int sd);
int hi_set_reuseaddr(int sd);
int hi_set_tcpnodelay(int sd);
int hi_set_linger(int sd, int timeout);
int hi_set_sndbuf(int sd, int size);
int hi_set_rcvbuf(int sd, int size);
int hi_get_soerror(int sd);
int hi_get_sndbuf(int sd);
int hi_get_rcvbuf(int sd);
#endif

int _hi_atoi(uint8_t *line, size_t n);
void _hi_itoa(uint8_t *s, int num);

int hi_valid_port(int n);

int _uint_len(uint32_t num);

#ifndef WIN32
/*
 * Wrappers to send or receive n byte message on a blocking
 * socket descriptor.
 */
#define hi_sendn(_s, _b, _n) _hi_sendn(_s, _b, (size_t)(_n))

#define hi_recvn(_s, _b, _n) _hi_recvn(_s, _b, (size_t)(_n))
#endif

/*
 * Wrappers to read or write data to/from (multiple) buffers
 * to a file or socket descriptor.
 */
#define hi_read(_d, _b, _n) read(_d, _b, (size_t)(_n))

#define hi_readv(_d, _b, _n) readv(_d, _b, (int)(_n))

#define hi_write(_d, _b, _n) write(_d, _b, (size_t)(_n))

#define hi_writev(_d, _b, _n) writev(_d, _b, (int)(_n))

#ifndef WIN32
ssize_t _hi_sendn(int sd, const void *vptr, size_t n);
ssize_t _hi_recvn(int sd, void *vptr, size_t n);
#endif

/*
 * Wrappers for defining custom assert based on whether macro
 * HI_ASSERT_PANIC or HI_ASSERT_LOG was defined at the moment
 * ASSERT was called.
 */
#ifdef HI_ASSERT_PANIC

#define ASSERT(_x)                                                             \
    do {                                                                       \
        if (!(_x)) {                                                           \
            hi_assert(#_x, __FILE__, __LINE__, 1);                             \
        }                                                                      \
    } while (0)

#define NOT_REACHED() ASSERT(0)

#elif HI_ASSERT_LOG

#define ASSERT(_x)                                                             \
    do {                                                                       \
        if (!(_x)) {                                                           \
            hi_assert(#_x, __FILE__, __LINE__, 0);                             \
        }                                                                      \
    } while (0)

#define NOT_REACHED() ASSERT(0)

#else

#define ASSERT(_x)

#define NOT_REACHED()

#endif

void hi_assert(const char *cond, const char *file, int line, int panic);
void hi_stacktrace(int skip_count);
void hi_stacktrace_fd(int fd);

int _scnprintf(char *buf, size_t size, const char *fmt, ...);
int _vscnprintf(char *buf, size_t size, const char *fmt, va_list args);
int64_t hi_usec_now(void);
int64_t hi_msec_now(void);

void print_string_with_length(char *s, size_t len);
void print_string_with_length_fix_CRLF(char *s, size_t len);

uint16_t crc16(const char *buf, int len);

#endif
