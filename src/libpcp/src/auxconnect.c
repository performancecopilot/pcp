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

__pmHostEnt *
__pmHostEntAlloc(void)
{
    return malloc(sizeof(__pmHostEnt));
}

void
__pmHostEntFree(__pmHostEnt *hostent)
{
    free(hostent);
}

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

void
__pmSockAddrFree(__pmSockAddr *sockaddr)
{
    free(sockaddr);
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
    int			fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    if ((servInfo = __pmHostEntAlloc()) == NULL)
	return -ENOMEM;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmGetHostByName(hostname, servInfo) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmAuxConnectPMCDPort(%s, %d) : hosterror=%d, ``%s''\n",
		    hostname, pmcd_port, hosterror(), hoststrerror());
	}
#endif
	__pmHostEntFree(servInfo);
	PM_UNLOCK(__pmLock_libpcp);
	return -EHOSTUNREACH;
    }

    __pmConnectTimeout();

    if ((fd = __pmCreateSocket()) < 0) {
	__pmHostEntFree(servInfo);
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    if ((myAddr = __pmHostEntGetSockAddr(servInfo, 0)) == NULL) {
        __pmHostEntFree(servInfo);
	return -ENOMEM;
    }
    __pmHostEntFree(servInfo);
    PM_UNLOCK(__pmLock_libpcp);

    if ((fdFlags = __pmConnectTo(fd, myAddr, pmcd_port)) >= 0) {
        __pmSockAddrFree(myAddr);

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
	
	if (sts) {
	    __pmCloseSocket(fd);
	    return -sts;
	}

	/*
	 * If we're here, it means we have a valid connection; restore the
	 * flags and make sure this file descriptor is closed if exec() is
	 * called
	 */
	return __pmConnectRestoreFlags(fd, fdFlags);
    }
    __pmSockAddrFree(myAddr);
    return fdFlags;
}

const char *
__pmHostEntGetName(const __pmHostEnt *he)
{
    return he->hostent.h_name;
}

#if !defined(HAVE_SECURE_SOCKETS)

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
__pmShutdownSockets(void)
{
    return 0;
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
    /* TODO: IPv6 */
    memset(addr, 0, sizeof(*addr));
    addr->sockaddr.inet.sin_family = AF_INET;
    addr->sockaddr.inet.sin_addr.s_addr = htonl(address);
    addr->sockaddr.inet.sin_port = htons(port);
}

void
__pmSockAddrSetFamily(__pmSockAddr *addr, int family)
{
    addr->sockaddr.family = family;
}

int
__pmSockAddrGetFamily(const __pmSockAddr *addr)
{
    return addr->sockaddr.family;
}

void
__pmSockAddrSetPort(__pmSockAddr *addr, int port)
{
    /* TODO: IPv6 */
    addr->sockaddr.inet.sin_port = htons(port);
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
    /* TODO: IPv6 */
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    return accept(fd, (struct sockaddr *)&sock->sockaddr.inet, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    /* TODO: IPv6 */
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    return bind(fd, (struct sockaddr *)&sock->sockaddr.inet, sizeof(sock->sockaddr.inet));
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    /* TODO: IPv6 */
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    return connect(fd, (struct sockaddr *)&sock->sockaddr.inet, sizeof(sock->sockaddr.inet));
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

__pmHostEnt *
__pmGetHostByName(const char *hostName, __pmHostEnt *hostEntry)
{
    struct hostent *he = gethostbyname(hostName);

    if (he == NULL)
	return NULL;
    memcpy(&hostEntry->hostent, he, sizeof(*he));
    return hostEntry;
}

__pmHostEnt *
__pmGetHostByAddr(__pmSockAddr *address, __pmHostEnt *hostEntry)
{
    /* TODO: IPv6 */
    struct hostent *he = gethostbyaddr((void *)&address->sockaddr.inet.sin_addr.s_addr,
					sizeof(address->sockaddr.inet.sin_addr.s_addr), AF_INET);
    if (he == NULL)
	return NULL;
    memcpy(&hostEntry->hostent, he, sizeof(*he));
    return hostEntry;
}

__pmSockAddr *
__pmHostEntGetSockAddr(const __pmHostEnt *he, int ix)
{
    /* TODO: IPv6 */
    __pmSockAddr* addr = __pmSockAddrAlloc();
    if (addr) {
        addr->sockaddr.inet.sin_family = he->hostent.h_addrtype;
	addr->sockaddr.inet.sin_addr = *(struct in_addr *)he->hostent.h_addr_list[ix];
    }
    return addr;
}

__pmSockAddr *
__pmSockAddrMask(__pmSockAddr *addr, const __pmSockAddr *mask)
{
    /* TODO: IPv6 */
    addr->sockaddr.inet.sin_addr.s_addr &= mask->sockaddr.inet.sin_addr.s_addr;
    return addr;
}

int
__pmSockAddrCompare(const __pmSockAddr *addr1, const __pmSockAddr *addr2)
{
    /* TODO: IPv6 */
    return addr1->sockaddr.inet.sin_addr.s_addr - addr2->sockaddr.inet.sin_addr.s_addr;
}

int
__pmSockAddrIsLoopBack(const __pmSockAddr *addr)
{
    /* TODO: IPv6 */
    return addr->sockaddr.inet.sin_addr.s_addr == htonl(INADDR_LOOPBACK);
}

int
__pmSockAddrIsInet(const __pmSockAddr *addr)
{
    return addr->sockaddr.family == AF_INET;
}

int
__pmSockAddrIsIPv6(const __pmSockAddr *addr)
{
    return addr->sockaddr.family == AF_INET6;
}

__pmSockAddr *
__pmLoopBackAddress(void)
{
    /* TODO: IPv6 */
    __pmSockAddr* addr = __pmSockAddrAlloc();
    if (addr) {
        addr->sockaddr.inet.sin_family = AF_INET;
	addr->sockaddr.inet.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
    return addr;
}

__pmSockAddr *
__pmStringToSockAddr(const char *cp)
{
    __pmSockAddr *addr = __pmSockAddrAlloc();

    if (addr) {
        /* TODO: IPv6 */
        addr->sockaddr.family = AF_INET;
	if (cp == NULL || strcmp(cp, "INADDR_ANY") == 0)
	    addr->sockaddr.inet.sin_addr.s_addr = INADDR_ANY;
	else {
#ifdef IS_MINGW
	    unsigned long in;
	    in = inet_addr(cp);
	    addr->sockaddr.inet.inaddr.s_addr = in;
#else
	    inet_aton(cp, &addr->sockaddr.inet.sin_addr);
#endif
	}
    }
    return addr;
}

/*
 * Convert an address in network byte order to a string.
 * The caller must free the buffer.
 */
char *
__pmSockAddrToString(__pmSockAddr *addr)
{
    /* TODO: IPv6 */
    char *buf = inet_ntoa(addr->sockaddr.inet.sin_addr);
    if (buf == NULL)
	return NULL;
    return strdup(buf);
}

#endif /* !HAVE_SECURE_SOCKETS */
