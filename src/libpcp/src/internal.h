/*
 * Copyright (c) 2012-2018 Red Hat.
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
#ifndef _LIBPCP_INTERNAL_H
#define _LIBPCP_INTERNAL_H

/*
 * Routines and data structures used within libpcp source files,
 * but which we do not want to expose via pmapi.h or libpcp.h.
 */

#include <stdarg.h>
#include "compiler.h"
#include "derive.h"

extern int __pmConvertTimeout(int) _PCP_HIDDEN;
extern int __pmConnectWithFNDELAY(int, void *, __pmSockLen) _PCP_HIDDEN;

extern int __pmPtrToHandle(__pmContext *) _PCP_HIDDEN;

extern int __pmGetDate(struct timespec *, char const *, struct timespec const *)  _PCP_HIDDEN;

#ifdef HAVE_NETWORK_BYTEORDER
/*
 * no-ops if already in network byte order but
 * the value may be used in an expression.
 */
#define __htonpmUnits(a)	(a)
#define __ntohpmUnits(a)	(a)
#define __htonpmID(a)		(a)
#define __ntohpmID(a)		(a)
#define __htonpmInDom(a)	(a)
#define __ntohpmInDom(a)	(a)
#define __htonpmPDUInfo(a)	(a)
#define __ntohpmPDUInfo(a)	(a)
#define __htonpmCred(a)		(a)
#define __ntohpmCred(a)		(a)

/*
 * For network byte order, the following are noops,
 * but otherwise the function is void, so they are
 * defined as comments to catch code that tries to
 * use them in an expression or assignment.
 */
#define __htonpmLabel(a)	/* noop */
#define __ntohpmLabel(a)	/* noop */
#define __htonpmValueBlock(a)	/* noop */
#define __ntohpmValueBlock(a)	/* noop */
#define __htonf(a)		/* noop */
#define __ntohf(a)		/* noop */
#define __htond(a)		/* noop */
#define __ntohd(a)		/* noop */
#define __htonll(a)		/* noop */
#define __ntohll(a)		/* noop */

#else
/*
 * Functions to convert to/from network byte order
 * for little-endian platforms (e.g. Intel).
 */
#define __htonpmID(a)		htonl(a)
#define __ntohpmID(a)		ntohl(a)
#define __htonpmInDom(a)	htonl(a)
#define __ntohpmInDom(a)	ntohl(a)


extern pmUnits __htonpmUnits(pmUnits) _PCP_HIDDEN;
extern pmUnits __ntohpmUnits(pmUnits) _PCP_HIDDEN;
extern __pmPDUInfo __htonpmPDUInfo(__pmPDUInfo) _PCP_HIDDEN;
extern __pmPDUInfo __ntohpmPDUInfo(__pmPDUInfo) _PCP_HIDDEN;
extern __pmCred __htonpmCred(__pmCred) _PCP_HIDDEN;
extern __pmCred __ntohpmCred(__pmCred) _PCP_HIDDEN;

/* insitu swab for these */
extern void __htonpmLabel(pmLabel * const) _PCP_HIDDEN;
extern void __ntohpmLabel(pmLabel * const) _PCP_HIDDEN;
extern void __htonpmValueBlock(pmValueBlock * const) _PCP_HIDDEN;
extern void __ntohpmValueBlock(pmValueBlock * const) _PCP_HIDDEN;
extern void __htonf(char *) _PCP_HIDDEN;	/* float */
#define __ntohf(v) __htonf(v)
#define __htond(v) __htonll(v)			/* double */
#define __ntohd(v) __ntohll(v)
extern void __htonll(char *) _PCP_HIDDEN;	/* 64bit int */
#define __ntohll(v) __htonll(v)

#endif /* HAVE_NETWORK_BYTEORDER */

#ifdef PM_MULTI_THREAD
extern void __pmInitMutex(pthread_mutex_t *) _PCP_HIDDEN;	/* mutex initializer */
extern void __pmDestroyMutex(pthread_mutex_t *) _PCP_HIDDEN;	/* mutex destroyer */

/* local lock initilizer methods */
extern void init_pmns_lock(void) _PCP_HIDDEN;
extern void init_AF_lock(void) _PCP_HIDDEN;
extern void init_secureserver_lock(void) _PCP_HIDDEN;
extern void init_connect_lock(void) _PCP_HIDDEN;
extern void init_exec_lock(void) _PCP_HIDDEN;

#ifdef HAVE___THREAD
/*
 * C compiler is probably gcc and supports __thread declarations
 */
#define PM_TPD(x) x
#else
/*
 * Roll-your-own Thread Private Data support
 */
extern pthread_key_t __pmTPDKey _PCP_HIDDEN;

typedef struct {
    int		curr_handle;	/* current context # */
    char	*derive_errmsg;	/* derived metric parser error message */
    __pmnsTree  *curr_pmns;     /* current pmns */
    int         useExtPMNS;     /* ... was the result of a __pmUsePMNS */
    __pmContext	*curr_ctxp;	/* -> current __pmContext */
} __pmTPD;

static inline __pmTPD *
__pmTPDGet(void)
{
    return (__pmTPD *)pthread_getspecific(__pmTPDKey);
}

#define PM_TPD(x)  __pmTPDGet()->x
#endif /* HAVE___THREAD */
#else /* !PM_MULTI_THREAD */
/* No threads - just access global variables as-is */
#define PM_TPD(x) x
#endif /* PM_MULTI_THREAD */

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
extern void __pmDebugLock(int, void *, const char *, int) _PCP_HIDDEN;
extern int __pmIsContextLock(void *) _PCP_HIDDEN;
extern char *__pmIsLogCtlLock(void *) _PCP_HIDDEN;
extern int __pmIsDeriveLock(void *) _PCP_HIDDEN;
extern int __pmIsAuxconnectLock(void *) _PCP_HIDDEN;
extern int __pmIsConfigLock(void *) _PCP_HIDDEN;
extern int __pmIsFaultLock(void *) _PCP_HIDDEN;
extern int __pmIsPduLock(void *) _PCP_HIDDEN;
extern int __pmIsPdubufLock(void *) _PCP_HIDDEN;
extern int __pmIsUtilLock(void *) _PCP_HIDDEN;
extern int __pmIsContextsLock(void *) _PCP_HIDDEN;
extern int __pmIsIpcLock(void *) _PCP_HIDDEN;
extern int __pmIsOptfetchLock(void *) _PCP_HIDDEN;
extern int __pmIsErrLock(void *) _PCP_HIDDEN;
extern int __pmIsLockLock(void *) _PCP_HIDDEN;
extern int __pmIsLogutilLock(void *) _PCP_HIDDEN;
extern int __pmIsPmnsLock(void *) _PCP_HIDDEN;
extern int __pmIsAFLock(void *) _PCP_HIDDEN;
extern int __pmIsSecureserverLock(void *) _PCP_HIDDEN;
extern int __pmIsConnectLock(void *) _PCP_HIDDEN;
extern int __pmIsExecLock(void *) _PCP_HIDDEN;
#endif

/*
 * Generic have / do not have lock macros
 */
#define PM_LOCKED	1
#define PM_NOT_LOCKED	0

#if !defined(PTHREAD_STACK_MIN)
#if defined(IS_SOLARIS)
#define PTHREAD_STACK_MIN       ((size_t)_sysconf(_SC_THREAD_STACK_MIN))
#else
#define PTHREAD_STACK_MIN       16384
#endif
#endif

/* AF_UNIX socket family internals */
#define PM_HOST_SPEC_NPORTS_LOCAL (-1)
#define PM_HOST_SPEC_NPORTS_UNIX  (-2)
extern const char *__pmPMCDLocalSocketDefault(void) _PCP_HIDDEN;
extern void __pmCheckAcceptedAddress(__pmSockAddr *) _PCP_HIDDEN;

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

/* internal NSS/NSPR/SSL/SASL implementation details */
extern int __pmSecureSocketsError(int) _PCP_HIDDEN;
#endif

#define PM_FDSET_SIZE \
	(sizeof(__pmFdSet) > FD_SETSIZE ? sizeof(__pmFdSet) : FD_SETSIZE)

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

struct __pmHostEnt {
    char		*name;
    __pmAddrInfo	*addresses;
};

extern const __pmAddrInfo *__pmHostEntGetAddrInfo(const __pmHostEnt *, const void *) _PCP_HIDDEN;
#endif

extern unsigned __pmFirstInetSubnetAddr(unsigned, int) _PCP_HIDDEN;
extern unsigned __pmNextInetSubnetAddr(unsigned, int) _PCP_HIDDEN;
extern unsigned char *__pmFirstIpv6SubnetAddr(unsigned char *, int maskBits) _PCP_HIDDEN;
extern unsigned char *__pmNextIpv6SubnetAddr(unsigned char *, int maskBits) _PCP_HIDDEN;

extern int __pmInitSecureSockets(void) _PCP_HIDDEN;
extern int __pmInitCertificates(void) _PCP_HIDDEN;
extern int __pmInitSocket(int, int) _PCP_HIDDEN;
extern int __pmSocketReady(int, struct timeval *) _PCP_HIDDEN;
extern void *__pmGetSecureSocket(int) _PCP_HIDDEN;
extern void *__pmGetUserAuthData(int) _PCP_HIDDEN;
extern int __pmSecureServerInit(void) _PCP_HIDDEN;
extern int __pmSecureServerIPCFlags(int, int) _PCP_HIDDEN;
extern int __pmSecureServerHasFeature(__pmServerFeature) _PCP_HIDDEN;
extern int __pmSecureServerSetFeature(__pmServerFeature) _PCP_HIDDEN;
extern int __pmSecureServerClearFeature(__pmServerFeature) _PCP_HIDDEN;

extern int __pmShutdownLocal(void) _PCP_HIDDEN;
extern int __pmShutdownCertificates(void) _PCP_HIDDEN;
extern int __pmShutdownSecureSockets(void) _PCP_HIDDEN;

#define SECURE_SERVER_SASL_SERVICE "PCP Collector"
#define LIMIT_ATTR_PDU	4096	/* maximum size of an attribute value (bytes) */
#define LIMIT_AUTH_PDU	2048	/* maximum size of a SASL transfer (in bytes) */
#define LIMIT_CLIENT_CALLBACKS 8	/* maximum size of callback array */
#define DEFAULT_SECURITY_STRENGTH 0	/* SASL security strength factor */

typedef int (*sasl_callback_func)(void);
extern int __pmInitAuthClients(void) _PCP_HIDDEN;
extern int __pmInitAuthServer(void) _PCP_HIDDEN;

/*
 * Platform independent user/group account manipulation
 */
extern int __pmValidUserID(__pmUserID) _PCP_HIDDEN;
extern int __pmValidGroupID(__pmGroupID) _PCP_HIDDEN;
extern int __pmEqualUserIDs(__pmUserID, __pmUserID) _PCP_HIDDEN;
extern int __pmEqualGroupIDs(__pmGroupID, __pmGroupID) _PCP_HIDDEN;
extern void __pmUserIDFromString(const char *, __pmUserID *) _PCP_HIDDEN;
extern void __pmGroupIDFromString(const char *, __pmGroupID *) _PCP_HIDDEN;
extern char *__pmUserIDToString(__pmUserID, char *, size_t) _PCP_HIDDEN;
extern char *__pmGroupIDToString(__pmGroupID, char *, size_t) _PCP_HIDDEN;
extern int __pmUsernameToID(const char *, __pmUserID *) _PCP_HIDDEN;
extern int __pmGroupnameToID(const char *, __pmGroupID *) _PCP_HIDDEN;
extern char *__pmUsernameFromID(__pmUserID, char *, size_t) _PCP_HIDDEN;
extern char *__pmGroupnameFromID(__pmGroupID, char *, size_t) _PCP_HIDDEN;
extern char *__pmHomedirFromID(__pmUserID, char *, size_t) _PCP_HIDDEN;
extern int __pmUsersGroupIDs(const char *, __pmGroupID **, unsigned int *) _PCP_HIDDEN;
extern int __pmGroupsUserIDs(const char *, __pmUserID **, unsigned int *) _PCP_HIDDEN;
extern int __pmGetUserIdentity(const char *, __pmUserID *, __pmGroupID *, int) _PCP_HIDDEN;

extern int __pmStringListAdd(char *, int, char ***) _PCP_HIDDEN;
extern char *__pmStringListFind(const char *, int, char **) _PCP_HIDDEN;

/*
 * Representations of server presence on the network.
 */
typedef struct __pmServerAvahiPresence __pmServerAvahiPresence;

struct __pmServerPresence {
    /* Common data. */
    char			*serviceSpec;
    int				port;
    /* API-specific data. */
    __pmServerAvahiPresence	*avahi;
};

/* Service discovery internals. */
typedef struct {
    const char		*spec;
    __pmSockAddr	*address;
    const char		*protocol;
} __pmServiceInfo;

typedef struct {
    const volatile sig_atomic_t	*flags;		/* Service discovery flags */
    struct timeval		timeout;	/* Global timeout period */
    volatile int		timedOut;	/* Global timeout occurred */
    int				resolve;	/* Resolve discovered addresses */
} __pmServiceDiscoveryOptions;

extern int __pmAddDiscoveredService(__pmServiceInfo *,
				const __pmServiceDiscoveryOptions *,
				int, char ***) _PCP_HIDDEN;
extern char *__pmServiceDiscoveryParseTimeout(const char *s,
				struct timeval *timeout) _PCP_HIDDEN;

extern int __pmServiceAddPorts(const char *, int **, int) _PCP_HIDDEN;
extern int __pmPMCDAddPorts(int **, int) _PCP_HIDDEN;
extern int __pmProxyAddPorts(int **, int) _PCP_HIDDEN;
extern int __pmWebdAddPorts(int **, int) _PCP_HIDDEN;
extern int __pmAddPorts(const char *, int **, int) _PCP_HIDDEN;

/*
 * Internal variants for with extra __pmContext parameter to allow
 * for context being locked or not locked
 */
extern int pmLookupName_ctx(__pmContext *, int, int, char **, pmID *) _PCP_HIDDEN;
extern int pmNameAll_ctx(__pmContext *, pmID, char ***) _PCP_HIDDEN;
extern int pmLookupDesc_ctx(__pmContext *, int, pmID, pmDesc *) _PCP_HIDDEN;
extern int pmNameInDom_ctx(__pmContext *, pmInDom, int, char **) _PCP_HIDDEN;
extern int pmLookupInDom_ctx(__pmContext *, pmInDom, const char *) _PCP_HIDDEN;
extern int pmGetInDomArchive_ctx(__pmContext *, pmInDom, int **, char ***) _PCP_HIDDEN;
extern int pmFetch_ctx(__pmContext *, int, pmID *, pmResult **) _PCP_HIDDEN;
extern int pmStore_ctx(__pmContext *, const pmResult *) _PCP_HIDDEN;
extern int __pmDecodeResult_ctx(__pmContext *, __pmPDU *, pmResult **) _PCP_HIDDEN;
extern int __pmSendResult_ctx(__pmContext *, int, int, const pmResult *) _PCP_HIDDEN;
extern void __pmDumpResult_ctx(__pmContext *, FILE *, const pmResult *) _PCP_HIDDEN;
extern int pmGetArchiveEnd_ctx(__pmContext *, struct timeval *) _PCP_HIDDEN;
extern int __pmGetArchiveEnd_ctx(__pmContext *, struct timeval *) _PCP_HIDDEN;
extern int __pmLogGenerateMark_ctx(__pmContext *, int, pmResult **) _PCP_HIDDEN;
extern int __pmLogCheckForNextArchive(__pmLogCtl *, int, pmResult **);

#ifdef BUILD_WITH_LOCK_ASSERTS
#include <assert.h>
#define PM_ASSERT_IS_LOCKED(lock) assert(__pmIsLocked(&(lock)))
#define PM_ASSERT_IS_UNLOCKED(lock) assert(!__pmIsLocked(&(lock)))
#else
#define PM_ASSERT_IS_LOCKED(lock)
#define PM_ASSERT_IS_UNLOCKED(lock)
#endif /* BUILD_WITH_LOCK_ASSERTS */

#ifdef IS_MINGW
extern const char *strerror_r(int, char *, size_t);
extern pid_t __pmProcessWait(pid_t, int, int *, int *) _PCP_HIDDEN;
#endif

extern void notifyerr(int, const char *, va_list) _PCP_HIDDEN;

/*
 * migrated from libpcp.h to here, only used in libpcp
 */
extern int __pmGetInternalState(void) _PCP_HIDDEN;
#ifdef PM_MULTI_THREAD
/* mutex for calls to external routines that are not thread-safe */
extern pthread_mutex_t	__pmLock_extcall;
#else
extern void *__pmLock_extcall;			/* symbol exposure */
#endif /* PM_MULTI_THREAD */
extern int __pmIsLocked(void *) _PCP_HIDDEN;
#define PM_IS_LOCKED(lock) 	__pmIsLocked(&(lock))
#ifdef BUILD_WITH_LOCK_ASSERTS
extern void __pmCheckIsUnlocked(void *, char *, int) _PCP_HIDDEN;
#endif /* BUILD_WITH_LOCK_ASSERTS */

extern int __pmGetPDUCeiling(void) _PCP_HIDDEN;

extern int __pmSetFeaturesIPC(int, int, int) _PCP_HIDDEN;
extern int __pmSetDataIPC(int, void *) _PCP_HIDDEN;
extern int __pmDataIPCSize(void) _PCP_HIDDEN;
extern int __pmLastVersionIPC(void) _PCP_HIDDEN;
extern int __pmFeaturesIPC(int) _PCP_HIDDEN;
extern int __pmDataIPC(int, void *) _PCP_HIDDEN;

extern int __pmGetSockOpt(int, int, int, void *, __pmSockLen *) _PCP_HIDDEN;
extern __pmSockAddr *__pmSockAddrMask(__pmSockAddr *, const __pmSockAddr *) _PCP_HIDDEN;
extern int	     __pmSockAddrGetPort(const __pmSockAddr *) _PCP_HIDDEN;
extern void	     __pmSockAddrSetScope(__pmSockAddr *, int) _PCP_HIDDEN;
extern __pmSockAddr *__pmSockAddrFirstSubnetAddr(const __pmSockAddr *, int) _PCP_HIDDEN;
extern __pmSockAddr *__pmSockAddrNextSubnetAddr(__pmSockAddr *, int) _PCP_HIDDEN;

extern int __pmHashPreAlloc(int, __pmHashCtl *) _PCP_HIDDEN;

extern int __pmConnectPMCD(pmHostSpec *, int, int, __pmHashCtl *) _PCP_HIDDEN;

extern int __pmConnectLocal(__pmHashCtl *) _PCP_HIDDEN;
extern int __pmAuxConnectPMCD(const char *) _PCP_HIDDEN;
extern int __pmAuxConnectPMCDUnixSocket(const char *) _PCP_HIDDEN;
extern int __pmAddHostPorts(pmHostSpec *, int *, int) _PCP_HIDDEN;
extern void __pmDropHostPort(pmHostSpec *) _PCP_HIDDEN;
extern void __pmConnectGetPorts(pmHostSpec *) _PCP_HIDDEN;

extern int __pmLogOpen(const char *, __pmContext *) _PCP_HIDDEN;
extern int __pmLogReopen(const char *, __pmContext *) _PCP_HIDDEN;
extern const char *__pmLogName_r(const char *, int, char *, int) _PCP_HIDDEN;
extern const char *__pmLogName(const char *, int) _PCP_HIDDEN;	/* NOT thread-safe */
extern int __pmLogGenerateMark(__pmLogCtl *, int, pmResult **) _PCP_HIDDEN;
extern int __pmLogFetchInterp(__pmContext *, int, pmID *, pmResult **) _PCP_HIDDEN;
extern int __pmGetArchiveLabel(__pmLogCtl *, pmLogLabel *) _PCP_HIDDEN;
extern pmTimeval *__pmLogStartTime(__pmArchCtl *) _PCP_HIDDEN;
extern void __pmLogSetTime(__pmContext *) _PCP_HIDDEN;
extern void __pmLogResetInterp(__pmContext *) _PCP_HIDDEN;
extern void __pmArchCtlFree(__pmArchCtl *) _PCP_HIDDEN;
extern int __pmLogChangeArchive(__pmContext *, int) _PCP_HIDDEN;
extern int __pmLogChangeToNextArchive(__pmLogCtl **) _PCP_HIDDEN;
extern int __pmLogChangeToPreviousArchive(__pmLogCtl **) _PCP_HIDDEN;

/* DSO PMDA helpers */
struct __pmDSO;			/* opaque, real definition in pmda.h */
extern struct __pmDSO *__pmLookupDSO(int) _PCP_HIDDEN;
/*
 * Adding/deleting/clearing the list of DSO PMDAs supported for
 * PM_CONTEXT_LOCAL contexts
 */
#define PM_LOCAL_ADD	1
#define PM_LOCAL_DEL	2
#define PM_LOCAL_CLEAR	3

extern int __pmTimevalCmp(const pmTimeval *, const pmTimeval *) _PCP_HIDDEN;

extern int __pmSecureServerSetup(const char *, const char *) _PCP_HIDDEN;

extern pmInDomProfile *__pmFindProfile(pmInDom, const pmProfile *) _PCP_HIDDEN;

extern void __pmFreeInterpData(__pmContext *) _PCP_HIDDEN;

extern void __pmDumpNameAndStatusList(FILE *, int, char **, int *) _PCP_HIDDEN;

#define MAXLABELNAMELEN		((1<<8)-1)
#define MAXLABELVALUELEN	((1<<16)-1)
extern void __pmDumpLabelSet(FILE *, const pmLabelSet *) _PCP_HIDDEN;
extern int __pmRecvLabel(int, __pmContext *, int, int *, int *,
			 pmLabelSet **, int *) _PCP_HIDDEN;
extern char *__pmLabelFlagString(int, char *, int) _PCP_HIDDEN;

#endif /* _LIBPCP_INTERNAL_H */
