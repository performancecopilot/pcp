/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 2000,2004,2005 Silicon Graphics, Inc.  All Rights Reserved.
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

#include "pmapi.h"
#include "impl.h"
#define SOCKET_INTERNAL
#include "internal.h"

/* default connect timeout is 5 seconds */
static struct timeval	canwait = { 5, 000000 };

__pmSockAddr *
__pmSockAddrAlloc(void)
{
    return calloc(1, sizeof(__pmSockAddr));
}

__pmSockAddr *
__pmSockAddrDup(const __pmSockAddr *sockaddr)
{
    __pmSockAddr *new = malloc(sizeof(__pmSockAddr));
    if (new)
	*new = *sockaddr;
    return new;
}

size_t
__pmSockAddrSize(void)
{
    return sizeof(__pmSockAddr);
}

int
__pmSockAddrIsLoopBack(const __pmSockAddr *addr)
{
    int rc;
    __pmSockAddr *loopBackAddr = __pmLoopBackAddress();
    if (loopBackAddr == NULL)
        return 0;
    rc = __pmSockAddrCompare(addr, loopBackAddr);
    __pmSockAddrFree(loopBackAddr);
    return rc == 0;
}

void
__pmSockAddrFree(__pmSockAddr *sockaddr)
{
    free(sockaddr);
}

__pmSockAddr *
__pmLoopBackAddress(void)
{
    __pmSockAddr* addr = __pmSockAddrAlloc();
    if (addr != NULL)
        __pmSockAddrInit(addr, INADDR_LOOPBACK, 0);
    return addr;
}

int
__pmInitSocket(int fd)
{
    int sts;
    int	nodelay = 1;
    struct linger nolinger = {1, 0};
    char errmsg[PM_MAXERRMSGLEN];

    if ((sts = __pmSetSocketIPC(fd)) < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    /* avoid 200 ms delay */
    if (__pmSetSockOpt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay,
		   (__pmSockLen)sizeof(nodelay)) < 0) {
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): __pmSetSockOpt TCP_NODELAY: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    /* don't linger on close */
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_LINGER, (char *)&nolinger,
		   (__pmSockLen)sizeof(nolinger)) < 0) {
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): __pmSetSockOpt SO_LINGER: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    return fd;
}

int
__pmConnectTo(int fd, const __pmSockAddr *addr, int port)
{
    int sts, fdFlags = __pmGetFileStatusFlags(fd);
    __pmSockAddr myAddr;

    myAddr = *addr;
    __pmSockAddrSetPort(&myAddr, port);

    if (__pmSetFileStatusFlags(fd, fdFlags | FNDELAY) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];

        __pmNotifyErr(LOG_ERR, "__pmConnectTo: cannot set FNDELAY - "
		      "fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags|FNDELAY , osstrerror_r(errmsg, sizeof(errmsg)));
    }
    
    if (__pmConnect(fd, &myAddr, sizeof(myAddr)) < 0) {
	sts = neterror();
	if (sts != EINPROGRESS) {
	    __pmCloseSocket(fd);
	    return -sts;
	}
    }

    return fdFlags;
}

int
__pmConnectCheckError(int fd)
{
    int	so_err = 0;
    __pmSockLen	olen = sizeof(int);
    char errmsg[PM_MAXERRMSGLEN];

    if (__pmGetSockOpt(fd, SOL_SOCKET, SO_ERROR, (void *)&so_err, &olen) < 0) {
	so_err = neterror();
	__pmNotifyErr(LOG_ERR, 
		"__pmConnectCheckError: __pmGetSockOpt(SO_ERROR) failed: %s\n",
		netstrerror_r(errmsg, sizeof(errmsg)));
    }
    return so_err;
}

static int
__pmConnectRestoreFlags(int fd, int fdFlags)
{
    int sts;
    char errmsg[PM_MAXERRMSGLEN];

    if (__pmSetFileStatusFlags(fd, fdFlags) < 0) {
	__pmNotifyErr(LOG_WARNING,"__pmConnectRestoreFlags: cannot restore "
		      "flags fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags, osstrerror_r(errmsg, sizeof(errmsg)));
    }

    if ((fdFlags = __pmGetFileDescriptorFlags(fd)) >= 0)
	sts = __pmSetFileDescriptorFlags(fd, fdFlags | FD_CLOEXEC);
    else
        sts = fdFlags;

    if (sts == -1) {
        __pmNotifyErr(LOG_WARNING, "__pmConnectRestoreFlags: "
		      "fcntl(%d) get/set flags failed: %s\n",
		      fd, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    return fd;
}

const struct timeval *
__pmConnectTimeout(void)
{
    static int		first_time = 1;

    /*
     * get optional stuff from environment ...
     * 	PMCD_CONNECT_TIMEOUT
     *	PMCD_PORT
     */
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (first_time) {
	char	*env_str;
	first_time = 0;

	if ((env_str = getenv("PMCD_CONNECT_TIMEOUT")) != NULL) {
	    char	*end_ptr;
	    double	timeout = strtod(env_str, &end_ptr);
	    if (*end_ptr != '\0' || timeout < 0.0)
		__pmNotifyErr(LOG_WARNING, "__pmAuxConnectPMCDPort: "
			      "ignored bad PMCD_CONNECT_TIMEOUT = '%s'\n",
			      env_str);
	    else {
		canwait.tv_sec = (time_t)timeout;
		canwait.tv_usec = (int)((timeout - 
					 (double)canwait.tv_sec) * 1000000);
	    }
	}
    }
    PM_UNLOCK(__pmLock_libpcp);
    return (&canwait);
}
 
/*
 * This interface is private to libpcp (although exposed in impl.h) and
 * deprecated (replaced by __pmAuxConnectPMCDPort()).
 * The implementation here is retained for IRIX and any 3rd party apps
 * that might have called this interface directly ... the implementation
 * is correct when $PMCD_PORT is unset, or set to a single numeric
 * port number, i.e. the old semantics
 */
int
__pmAuxConnectPMCD(const char *hostname)
{
    static int		pmcd_port;
    static int		first_time = 1;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (first_time) {
	char	*env_str;
	char	*end_ptr;

	first_time = 0;

	if ((env_str = getenv("PMCD_PORT")) != NULL) {

	    pmcd_port = (int)strtol(env_str, &end_ptr, 0);
	    if (*end_ptr != '\0' || pmcd_port < 0) {
		__pmNotifyErr(LOG_WARNING,
			      "__pmAuxConnectPMCD: ignored bad PMCD_PORT = '%s'\n",
			      env_str);
		pmcd_port = SERVER_PORT;
	    }
	}
	else
	    pmcd_port = SERVER_PORT;
    }
    PM_UNLOCK(__pmLock_libpcp);

    return __pmAuxConnectPMCDPort(hostname, pmcd_port);
}

int
__pmAuxConnectPMCDPort(const char *hostname, int pmcd_port)
{
    __pmSockAddr *myAddr;
    __pmHostEnt	*servInfo;
    int			fd = -1;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags = 0;
    int			addrIx;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((servInfo = __pmGetAddrInfo(hostname)) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmAuxConnectPMCDPort(%s, %d) : hosterror=%d, ``%s''\n",
		    hostname, pmcd_port, hosterror(), hoststrerror());
	}
#endif
	PM_UNLOCK(__pmLock_libpcp);
	return -EHOSTUNREACH;
    }

    __pmConnectTimeout();

    addrIx = 0;
    for (myAddr = __pmHostEntGetSockAddr(servInfo, &addrIx);
	 myAddr != NULL;
	 myAddr = __pmHostEntGetSockAddr(servInfo, &addrIx)) {
	/* Create a socket */
	if (__pmSockAddrIsInet(myAddr))
	    fd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myAddr))
	    fd = __pmCreateIPv6Socket();
	else {
	    __pmNotifyErr(LOG_ERR, 
			  "__pmAuxConnectPMCDPort(%s, %d) : invalid address family %d\n",
			  hostname, pmcd_port, __pmSockAddrGetFamily(myAddr));
	    fd = -1;
	}
	if (fd < 0) {
	    __pmSockAddrFree(myAddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	fdFlags = __pmConnectTo(fd, myAddr, pmcd_port);
	__pmSockAddrFree(myAddr);
	if (fdFlags < 0)
	    continue; /* Try the next address */

	/* FNDELAY and we're in progress - wait on select */
	struct timeval stv = canwait;
	struct timeval *pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
	__pmFdSet wfds;
	int rc;

	__pmFD_ZERO(&wfds);
	__pmFD_SET(fd, &wfds);
	sts = 0;
	if ((rc = __pmSelectWrite(fd+1, &wfds, pstv)) == 1) {
	    sts = __pmConnectCheckError(fd);
	}
	else if (rc == 0) {
	    sts = ETIMEDOUT;
	}
	else {
	    sts = (rc < 0) ? neterror() : EINVAL;
	}
 
	/* Successful connection? */
	if (sts == 0)
	    break;

	/* Unsuccessful connection. */
	__pmCloseSocket(fd);
	fd = -1;
    }

    __pmHostEntFree(servInfo);
    PM_UNLOCK(__pmLock_libpcp);
    if (fd < 0)
        return -ECONNREFUSED;

    /*
     * If we're here, it means we have a valid connection; restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called
     */
    return __pmConnectRestoreFlags(fd, fdFlags);
}

#if !defined(HAVE_SECURE_SOCKETS)

__pmHostEnt *
__pmHostEntAlloc(void)
{
    return calloc(1, sizeof(__pmHostEnt));
}

void
__pmHostEntFree(__pmHostEnt *hostent)
{
    if (hostent->name != NULL)
        free(hostent->name);
    if (hostent->addresses != NULL)
        freeaddrinfo(hostent->addresses);
    free(hostent);
}

static int
createSocket(int family)
{
    int sts, fd;

    if ((fd = socket(family, SOCK_STREAM, 0)) < 0)
	return -neterror();
    if ((sts = __pmInitSocket(fd)) < 0)
        return sts;
    return fd;
}

int
__pmCreateSocket(void)
{
    return createSocket(AF_INET);
}

int
__pmCreateIPv6Socket(void)
{
    int socket = createSocket(AF_INET6);

    if (socket >= 0) {
	/* Disable IPv4-mapped connections */
	int one = 1;
	__pmSetSockOpt(socket, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one));
    }
    return socket;
}

void
__pmCloseSocket(int fd)
{
    __pmResetIPC(fd);
#if defined(IS_MINGW)
    closesocket(fd);
#else
    close(fd);
#endif
}

int
__pmSetSockOpt(int socket, int level, int option_name, const void *option_value,
	       __pmSockLen option_len)
{
    return setsockopt(socket, level, option_name, option_value, option_len);
}

int
__pmGetSockOpt(int socket, int level, int option_name, void *option_value,
	       __pmSockLen *option_len)
{
    return getsockopt(socket, level, option_name, option_value, option_len);
}

/* Initialize a socket address. The integral address must be INADDR_ANY or
   INADDR_LOOPBACK in host byte order. */
void
__pmSockAddrInit(__pmSockAddr *addr, int address, int port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sockaddr.inet.sin_family = AF_INET;
    addr->sockaddr.inet.sin_addr.s_addr = htonl(address);
    addr->sockaddr.inet.sin_port = htons(port);
}

void
__pmSockAddrSetFamily(__pmSockAddr *addr, int family)
{
    addr->sockaddr.raw.sa_family = family;
}

int
__pmSockAddrGetFamily(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.sa_family;
}

void
__pmSockAddrSetPort(__pmSockAddr *addr, int port)
{
    if (addr->sockaddr.raw.sa_family == AF_INET)
        addr->sockaddr.inet.sin_port = htons(port);
    else if (addr->sockaddr.raw.sa_family == AF_INET6)
        addr->sockaddr.ipv6.sin6_port = htons(port);
    else
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrSetPort: Invalid address family: %d\n", addr->sockaddr.raw.sa_family);
}

int
__pmInitCertificates(void)
{
    return 0;
}

int
__pmShutdownCertificates(void)
{
    return 0;
}

int
__pmInitSecureSockets(void)
{
    return 0;
}

int
__pmShutdownSecureSockets(void)
{
    return 0;
}

int
__pmDataIPCSize(void)
{
    return 0;
}

int
__pmSecureServerSetup(const char *db, const char *passwd)
{
    (void)db;
    (void)passwd;
    return 0;
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    (void)fd;
    (void)flags;
    return -EOPNOTSUPP;
}

void
__pmSecureServerShutdown(void)
{
    /* nothing to do here */
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname)
{
    (void)fd;
    (void)flags;
    (void)hostname;
    return -EOPNOTSUPP;
}

int
__pmSecureServerHandshake(int fd, int flags)
{
    (void)fd;
    (void)flags;
    return -EOPNOTSUPP;
}

int
__pmSecureServerHasFeature(__pmSecureServerFeature query)
{
    if (query == PM_SERVER_FEATURE_IPV6)
	return 1;
    return 0;
}

int
__pmSocketClosed(void)
{
    switch (oserror()) {
	/*
	 * Treat this like end of file on input.
	 *
	 * failed as a result of pmcd exiting and the connection
	 * being reset, or as a result of the kernel ripping
	 * down the connection (most likely because the host at
	 * the other end just took a dive)
	 *
	 * from IRIX BDS kernel sources, seems like all of the
	 * following are peers here:
	 *  ECONNRESET (pmcd terminated?)
	 *  ETIMEDOUT ENETDOWN ENETUNREACH EHOSTDOWN EHOSTUNREACH
	 *  ECONNREFUSED
	 * peers for BDS but not here:
	 *  ENETRESET ENONET ESHUTDOWN (cache_fs only?)
	 *  ECONNABORTED (accept, user req only?)
	 *  ENOTCONN (udp?)
	 *  EPIPE EAGAIN (nfs, bds & ..., but not ip or tcp?)
	 */
	case ECONNRESET:
	case EPIPE:
	case ETIMEDOUT:
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
	case ECONNREFUSED:
	    return 1;
    }
    return 0;
}

int
__pmListen(int fd, int backlog)
{
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    return accept(fd, &sock->sockaddr.raw, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    if (sock->sockaddr.raw.sa_family == AF_INET)
        return bind(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.inet));
    if (sock->sockaddr.raw.sa_family == AF_INET6)
        return bind(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.ipv6));
    __pmNotifyErr(LOG_ERR,
		"__pmBind: Invalid address family: %d\n", sock->sockaddr.raw.sa_family);
    errno = EAFNOSUPPORT;
    return -1; /* failure */
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    if (sock->sockaddr.raw.sa_family == AF_INET)
        return connect(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.inet));
    if (sock->sockaddr.raw.sa_family == AF_INET6)
        return connect(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.ipv6));
    __pmNotifyErr(LOG_ERR,
		"__pmConnect: Invalid address family: %d\n", sock->sockaddr.raw.sa_family);
    errno = EAFNOSUPPORT;
    return -1; /* failure */
}

int
__pmGetFileStatusFlags(int fd)
{
    return fcntl(fd, F_GETFL);
}

int
__pmSetFileStatusFlags(int fd, int flags)
{
    return fcntl(fd, F_SETFL, flags);
}

int
__pmGetFileDescriptorFlags(int fd)
{
    return fcntl(fd, F_GETFD);
}

int
__pmSetFileDescriptorFlags(int fd, int flags)
{
    return fcntl(fd, F_SETFD, flags);
}

ssize_t
__pmWrite(int socket, const void *buffer, size_t length)
{
    return write(socket, buffer, length);
}

ssize_t
__pmRead(int socket, void *buffer, size_t length)
{
    return read(socket, buffer, length);
}

ssize_t
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
    return send(socket, buffer, length, flags);
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
    return recv(socket, buffer, length, flags);
}

int
__pmFD(int fd)
{
    return fd;
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
    FD_CLR(fd, set);
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
    return FD_ISSET(fd, set);
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
    FD_SET(fd, set);
}

void
__pmFD_ZERO(__pmFdSet *set)
{
    FD_ZERO(set);
}

void
__pmFD_COPY(__pmFdSet *s1, const __pmFdSet *s2)
{
    memcpy(s1, s2, sizeof(*s1));
}

int
__pmSelectRead(int nfds, __pmFdSet *readfds, struct timeval *timeout)
{
    return select(nfds, readfds, NULL, NULL, timeout);
}

int
__pmSelectWrite(int nfds, __pmFdSet *writefds, struct timeval *timeout)
{
    return select(nfds, NULL, writefds, NULL, timeout);
}

int
__pmSocketReady(int fd, struct timeval *timeout)
{
    __pmFdSet	onefd;

    FD_ZERO(&onefd);
    FD_SET(fd, &onefd);
    return select(fd+1, &onefd, NULL, NULL, timeout);
}

/* TODO: Make this interface more like the native getnameinfo. i.e. caller provides buffer and size */
char *
__pmGetNameInfo(__pmSockAddr *address)
{
    int sts;
    char buf[NI_MAXHOST];

    if (address->sockaddr.raw.sa_family == AF_INET) {
        sts = getnameinfo(&address->sockaddr.raw, sizeof(address->sockaddr.inet),
			  buf, sizeof(buf), NULL, 0, 0);
    }
    else if (address->sockaddr.raw.sa_family == AF_INET6) {
        sts = getnameinfo(&address->sockaddr.raw, sizeof(address->sockaddr.ipv6),
			  buf, sizeof(buf), NULL, 0, 0);
    }
    else
        sts = EAI_FAMILY;

    return sts == 0 ? strdup(buf) : NULL;
}

__pmHostEnt *
__pmGetAddrInfo(const char *hostName)
{
    __pmSockAddr *addr;
    __pmHostEnt *hostEntry;
    struct addrinfo hints;
    int zero;
    int sts;

    hostEntry = __pmHostEntAlloc();
    if (hostEntry != NULL) {
        memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
	hints.ai_flags = AI_ADDRCONFIG; /* Only return configured address types */

	sts = getaddrinfo(hostName, NULL, &hints, &hostEntry->addresses);
	if (sts != 0) {
	    __pmHostEntFree(hostEntry);
	    return NULL;
	}

	/* Try to reverse lookup the host name. */
	zero = 0;
	addr = __pmHostEntGetSockAddr(hostEntry, &zero);
	if (addr != NULL) {
	    hostEntry->name = __pmGetNameInfo(addr);
	    __pmSockAddrFree(addr);
	}
	else
	    hostEntry->name = strdup(hostName);
    }
    return hostEntry;
}

char *
__pmHostEntGetName(const __pmHostEnt *he)
{
    if (he->name == NULL)
        return NULL;
    return strdup(he->name);
}

__pmSockAddr *
__pmHostEntGetSockAddr(const __pmHostEnt *he, int *ix)
{
    struct addrinfo *ai;
    __pmSockAddr* addr = __pmSockAddrAlloc();
    int i;

    /* Make sure the given index references an actual address in the chain. */
    for (i = 0, ai = he->addresses; i < *ix && ai != NULL; ++i, ai = ai->ai_next)
        ;
    if (i != *ix)
        __pmNotifyErr(LOG_ERR, "__pmHostEntGetSockAddr: unable to obtain host address\n");

    if (i != *ix || ai == NULL) {
        /* End of the address chain. No address to return. */
	*ix = 0;
	return NULL;
    }

    /* Now allocate a socket address and copy the data. */
    __pmSockAddr* addr = __pmSockAddrAlloc();
    if (addr == NULL) {
        __pmNotifyErr(LOG_ERR, "__pmHostEntGetSockAddr: out of memory\n");
        *ix = 0;
	return NULL;
    }
    memcpy(&addr->sockaddr.raw, ai->ai_addr, ai->ai_addrlen);

    /* Update the enumerator index. */
    ++*ix;
    return addr;
}

__pmSockAddr *
__pmSockAddrMask(__pmSockAddr *addr, const __pmSockAddr *mask)
{
    int i;
    if (addr->sockaddr.raw.sa_family != mask->sockaddr.raw.sa_family) {
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrMask: Address family of the address (%d) must match that of the mask (%d)\n",
		addr->sockaddr.raw.sa_family, mask->sockaddr.raw.sa_family);
    }
    else if (addr->sockaddr.raw.sa_family == AF_INET)
        addr->sockaddr.inet.sin_addr.s_addr &= mask->sockaddr.inet.sin_addr.s_addr;
    else if (addr->sockaddr.raw.sa_family == AF_INET6) {
        /* IPv6: Mask it byte by byte */
        unsigned char *addrBytes = addr->sockaddr.ipv6.sin6_addr.s6_addr;
	const unsigned char *maskBytes = mask->sockaddr.ipv6.sin6_addr.s6_addr;
	for (i = 0; i < sizeof(addr->sockaddr.ipv6.sin6_addr.s6_addr); ++i)
            addrBytes[i] &= maskBytes[i];
    }
    else
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrMask: Invalid address family: %d\n", addr->sockaddr.raw.sa_family);

    return addr;
}

int
__pmSockAddrCompare(const __pmSockAddr *addr1, const __pmSockAddr *addr2)
{
    if (addr1->sockaddr.raw.sa_family != addr2->sockaddr.raw.sa_family)
        return addr1->sockaddr.raw.sa_family - addr2->sockaddr.raw.sa_family;

    if (addr1->sockaddr.raw.sa_family == AF_INET)
        return addr1->sockaddr.inet.sin_addr.s_addr - addr2->sockaddr.inet.sin_addr.s_addr;

    if (addr1->sockaddr.raw.sa_family == AF_INET6) {
        /* IPv6: Compare it byte by byte */
        return memcmp(&addr1->sockaddr.ipv6.sin6_addr.s6_addr, &addr2->sockaddr.ipv6.sin6_addr.s6_addr,
		      sizeof(addr1->sockaddr.ipv6.sin6_addr.s6_addr));
    }

    __pmNotifyErr(LOG_ERR,
		  "__pmSockAddrCompare: Invalid address family: %d\n", addr1->sockaddr.raw.sa_family);
    return 1; /* not equal */
}

int
__pmSockAddrIsInet(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.sa_family == AF_INET;
}

int
__pmSockAddrIsIPv6(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.sa_family == AF_INET6;
}

__pmSockAddr *
__pmStringToSockAddr(const char *cp)
{
    __pmSockAddr *addr = __pmSockAddrAlloc();
    if (addr) {
        if (cp == NULL || strcmp(cp, "INADDR_ANY") == 0) {
	    addr->sockaddr.inet.sin_addr.s_addr = INADDR_ANY;
	    /* Set the address family to 0, meaning "not set". */
	    addr->sockaddr.raw.sa_family = 0;
	}
	else {
	    int family = (strchr(cp, ':') == NULL) ? AF_INET : AF_INET6;
	    int sts;
	    addr->sockaddr.raw.sa_family = family;
	    if (family == AF_INET)
	        sts = inet_pton(family, cp, &addr->sockaddr.inet.sin_addr);
	    else
	        sts = inet_pton(family, cp, &addr->sockaddr.ipv6.sin6_addr);
	    if (sts <= 0) {
	        __pmSockAddrFree(addr);
		addr = NULL;
	    }
	}
    }
    return addr;
}

/*
 * Convert an address to a string.
 * The caller must free the buffer.
 */
char *
__pmSockAddrToString(__pmSockAddr *addr)
{
    char str[INET6_ADDRSTRLEN];
    int family;
    const char *sts;

    family = addr->sockaddr.raw.sa_family;
    if (family == AF_INET)
	sts = inet_ntop(family, &addr->sockaddr.inet.sin_addr, str, sizeof(str));
    else
	sts = inet_ntop(family, &addr->sockaddr.ipv6.sin6_addr, str, sizeof(str));
    if (sts == NULL)
	return NULL;
    return strdup(str);
}

#endif /* !HAVE_SECURE_SOCKETS */
