/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2001 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 */

#ifndef _INTERNAL_H
#define _INTERNAL_H

/*
 * Routines and data structures used within libpcp source files,
 * but which we do not want to expose via impl.h or pmapi.h.
 */

#ifdef __cplusplus
extern "C" {
#endif

extern int __pmFetchLocal(__pmContext *, int, pmID *, pmResult **);

#ifdef PM_MULTI_THREAD
#ifdef HAVE___THREAD
/*
 * C compiler is probably gcc and supports __thread declarations
 */
#define PM_TPD(x) x
#else
/*
 * Roll-your-own Thread Private Data support
 */
extern pthread_key_t __pmTPDKey;

typedef struct {
    int		curcontext;	/* current context */
    char	*derive_errmsg;	/* derived metric parser error message */
} __pmTPD;

static inline __pmTPD *
__pmTPDGet(void)
{
    return (__pmTPD *)pthread_getspecific(__pmTPDKey);
}

#define PM_TPD(x)  __pmTPDGet()->x
#endif
#else
/* No threads - just access global variables as is */
#define PM_TPD(x) x
#endif

#ifdef SOCKET_INTERNAL
#ifdef HAVE_SECURE_SOCKETS
#include <nss.h>
#include <ssl.h>
#include <nspr.h>
#include <private/pprio.h>

#define SECURE_SERVER_CERTIFICATE "PCP Collector certificate"

struct __pmSockAddr {
    PRNetAddr		sockaddr;
};
struct __pmHostEnt {
    PRHostEnt		hostent;
    char		buffer[PR_NETDB_BUF_SIZE];
};

/* internal NSS implementation details */
extern int __pmSecureSocketsError(void);

#else
struct __pmSockAddr {
    union {
	__uint16_t		family;
	struct sockaddr_in	inet;
	struct sockaddr_in6	ipv6;
    } sockaddr;
};
struct __pmHostEnt {
    struct hostent	hostent;
};
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* _INTERNAL_H */
