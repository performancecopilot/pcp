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
#include <prerror.h>
#include <private/pprio.h>
#include <sasl.h>

#define SECURE_SERVER_CERTIFICATE "PCP Collector certificate"
#define SECURE_USERDB_DEFAULT_KEY "\n"

struct __pmSockAddr {
    PRNetAddr		sockaddr;
};

typedef PRAddrInfo __pmAddrInfo;

/* internal NSS/NSPR/SSL/SASL implementation details */
extern int __pmSecureSocketsError(int);

#else /* native sockets only */

#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#endif

struct __pmSockAddr {
    union {
	struct sockaddr	        raw;
	struct sockaddr_in	inet;
	struct sockaddr_in6	ipv6;
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	struct sockaddr_un	local;
#endif
    } sockaddr;
};

typedef struct addrinfo __pmAddrInfo;

#endif

struct __pmHostEnt {
    char		*name;
    __pmAddrInfo	*addresses;
};
#endif

extern int __pmInitSecureSockets(void);
extern int __pmInitCertificates(void);
extern int __pmInitSocket(int, int);
extern int __pmSocketReady(int, struct timeval *);
extern int __pmSocketClosed(void);
extern int __pmConnectCheckError(int);
extern void *__pmGetSecureSocket(int);
extern void *__pmGetUserAuthData(int);
extern int __pmSecureServerIPCFlags(int, int);

#define SECURE_SERVER_SASL_SERVICE "PCP Collector"
#define LIMIT_AUTH_PDU	2048	/* maximum size of a SASL transfer (in bytes) */
#define LIMIT_CLIENT_CALLBACKS 8	/* maximum size of callback array */
#define DEFAULT_SECURITY_STRENGTH 0	/* SASL security strength factor */

typedef int (*sasl_callback_func)(void);
extern int __pmInitAuthClients(void);
extern int __pmInitAuthServer(void);

extern int __pmValidUserID(__pmUserID);
extern int __pmValidGroupID(__pmGroupID);
extern int __pmEqualUserIDs(__pmUserID, __pmUserID);
extern int __pmEqualGroupIDs(__pmGroupID, __pmGroupID);
extern void __pmUserIDFromString(const char *, __pmUserID *);
extern void __pmGroupIDFromString(const char *, __pmGroupID *);
extern char *__pmUserIDToString(__pmUserID, char *, size_t);
extern char *__pmGroupIDToString(__pmGroupID, char *, size_t);
extern int __pmUsernameToID(const char *, __pmUserID *);
extern int __pmGroupnameToID(const char *, __pmGroupID *);
extern char *__pmUsernameFromID(__pmUserID, char *, size_t);
extern char *__pmGroupnameFromID(__pmGroupID, char *, size_t);
extern int __pmUsersGroupIDs(const char *, __pmGroupID **, unsigned int *);
extern int __pmGroupsUserIDs(const char *, __pmUserID **, unsigned int *);
extern int __pmGetUserIdentity(const char *, __pmUserID *, __pmGroupID *, int);

#ifdef __cplusplus
}
#endif

#endif /* _INTERNAL_H */
