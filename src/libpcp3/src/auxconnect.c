/*
 * Copyright (c) 2012-2015,2017-2018 Red Hat.
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
#include "libpcp.h"
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_IPHLPAPI_H
#include <iphlpapi.h>
#endif
#ifdef HAVE_NETIOAPI_H
#include <netioapi.h>
#endif
#define SOCKET_INTERNAL
#include "internal.h"

/* default connect timeout is 5 seconds */
static struct timeval	conn_wait = { 5, 0 };
static int		conn_wait_done;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	auxconnect_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*auxconnect_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == auxconnect_lock
 */
int
__pmIsAuxconnectLock(void *lock)
{
    return lock == (void *)&auxconnect_lock;
}
#endif

static __pmHostEnt *
__pmHostEntAlloc(void)
{
    return calloc(1, sizeof(__pmHostEnt));
}

void
__pmHostEntFree(__pmHostEnt *hostent)
{
    if (pmDebugOptions.desperate)
        fprintf(stderr, "%s:__pmHostEntFree(hostent=%p) name=%p (%s) addresses=%p\n", __FILE__, hostent, hostent->name, hostent->name, hostent-> addresses);
    if (hostent->name != NULL)
        free(hostent->name);
    if (hostent->addresses != NULL)
        freeaddrinfo(hostent->addresses);
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

/* Initialize a socket address. The integral address must be INADDR_ANY or
   INADDR_LOOPBACK in host byte order. */
void
__pmSockAddrInit(__pmSockAddr *addr, int family, int address, int port)
{
    memset(addr, 0, sizeof(*addr));
    if (family == AF_INET) {
	addr->sockaddr.inet.sin_family = family;
	addr->sockaddr.inet.sin_addr.s_addr = htonl(address);
	addr->sockaddr.inet.sin_port = htons(port);
    }
    else if (family == AF_INET6) {
	addr->sockaddr.ipv6.sin6_family = family;
	addr->sockaddr.ipv6.sin6_port = htons(port);
	if (address == INADDR_LOOPBACK)
	    addr->sockaddr.ipv6.sin6_addr = in6addr_loopback;
	else
	    addr->sockaddr.ipv6.sin6_addr = in6addr_any;
    }
    else if (family == AF_UNSPEC) {
	/* do nothing */
	;
    }
    else
	pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrInit: Invalid address family: %d\n", __FILE__, addr->sockaddr.raw.sa_family);
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
	pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrSetPort: Invalid address family: %d\n",
		__FILE__, addr->sockaddr.raw.sa_family);
}

int
__pmSockAddrGetPort(const __pmSockAddr *addr)
{
    if (addr->sockaddr.raw.sa_family == AF_INET)
        return ntohs(addr->sockaddr.inet.sin_port);
    if (addr->sockaddr.raw.sa_family == AF_INET6)
        return ntohs(addr->sockaddr.ipv6.sin6_port);
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr->sockaddr.raw.sa_family == AF_UNIX)
	return 0;
#endif
    pmNotifyErr(LOG_ERR,
		  "__pmSockAddrGetPort: Invalid address family: %d\n",
		  addr->sockaddr.raw.sa_family);
    return 0; /* not set */
}

int
__pmSockAddrIsLoopBack(const __pmSockAddr *addr)
{
    int rc;
    int family;
    __pmSockAddr *loopBackAddr;

    family = __pmSockAddrGetFamily(addr);
    loopBackAddr = __pmLoopBackAddress(family);
    if (loopBackAddr == NULL)
        return 0;
    rc = __pmSockAddrCompare(addr, loopBackAddr);
    __pmSockAddrFree(loopBackAddr);
    return rc == 0;
}

void
__pmSockAddrSetScope(__pmSockAddr *addr, int scope)
{
    if (addr->sockaddr.raw.sa_family == AF_INET6)
        addr->sockaddr.ipv6.sin6_scope_id = scope;
}

void
__pmSockAddrSetPath(__pmSockAddr *addr, const char *path)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr->sockaddr.raw.sa_family == AF_UNIX) {
	int buflen = sizeof(addr->sockaddr.local.sun_path);
	strncpy(addr->sockaddr.local.sun_path, path, buflen);
	addr->sockaddr.local.sun_path[buflen-1] = '\0';
    } else {
	pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrSetPath: Invalid address family: %d\n",
		__FILE__, addr->sockaddr.raw.sa_family);
    }
#else
    pmNotifyErr(LOG_ERR, "%s:__pmSockAddrSetPath: AF_UNIX is not supported\n", __FILE__);
#endif
}

__pmSockAddr *
__pmSockAddrMask(__pmSockAddr *addr, const __pmSockAddr *mask)
{
    int i;
    if (addr->sockaddr.raw.sa_family != mask->sockaddr.raw.sa_family) {
	pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrMask: Address family of the address (%d) must match that of the mask (%d)\n",
		__FILE__, addr->sockaddr.raw.sa_family, mask->sockaddr.raw.sa_family);
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
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (addr->sockaddr.raw.sa_family == AF_UNIX) {
	/* Simply truncate the path in the address to the length of the mask. */
	i = strlen(mask->sockaddr.local.sun_path);
	addr->sockaddr.local.sun_path[i] = '\0';
    }
#endif
    else /* not applicable to other address families. */
	pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrMask: Invalid address family: %d\n", __FILE__, addr->sockaddr.raw.sa_family);

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

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr1->sockaddr.raw.sa_family == AF_UNIX) {
        /* Unix Domain: Compare the paths */
	return strncmp(addr1->sockaddr.local.sun_path, addr2->sockaddr.local.sun_path,
		       sizeof(addr1->sockaddr.local.sun_path));
    }
#endif
    
    if (addr1->sockaddr.raw.sa_family == AF_UNSPEC) {
	/* if the address family is unspecified, assume not equal */
	return 1;
    }

    /* Unknown address family. */
    pmNotifyErr(LOG_ERR,
		  "%s:__pmSockAddrCompare: Invalid address family: %d\n", __FILE__, addr1->sockaddr.raw.sa_family);
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

int
__pmSockAddrIsUnix(const __pmSockAddr *addr)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    return addr->sockaddr.raw.sa_family == AF_UNIX;
#else
    return 0;
#endif
}

#if defined(IS_MINGW)
#define inet_pton win32_inet_pton
#endif

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
	else if (strcmp(cp, "INADDR_LOOPBACK") == 0) {
	    addr->sockaddr.inet.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	    /* Set the address family to 0, meaning "not set". */
	    addr->sockaddr.raw.sa_family = 0;
	}
	else {
	    int sts;
	    /* Determine the address family. */
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	    if (*cp == pmPathSeparator()) {
		if (strlen(cp) >= sizeof(addr->sockaddr.local.sun_path))
		    sts = -1; /* too long */
		else {
		    addr->sockaddr.raw.sa_family = AF_UNIX;
		    strcpy(addr->sockaddr.local.sun_path, cp);
		    sts = 1;
		}
	    }
	    else
#endif
	    if (strchr(cp, ':') != NULL) {
		char *cp1;
		char *scope;
		/*
		 * inet_pton(3) does not support the "%<interface>" extension for specifying the
		 * scope of a link-local address. If one is present, then strip it out and
		 * set the scope_id manually.
		 */
		if ((scope = strchr(cp, '%')) != NULL) {
		    size_t size = scope - cp;
		    ++scope; /* get past the '%' */
		    if ((cp1 = malloc(size + 1)) == NULL)
			sts = -1;
		    else {
			strncpy(cp1, cp, size);
			cp1[size] = '\0';
		    }
		    cp = cp1;
		}
		if (cp != NULL) {
		    addr->sockaddr.raw.sa_family = AF_INET6;
		    sts = inet_pton(AF_INET6, cp, &addr->sockaddr.ipv6.sin6_addr);
		    if (scope != NULL) {
			free(cp1);
			/* Manually set the scope_id */
			if ((addr->sockaddr.ipv6.sin6_scope_id = if_nametoindex(scope)) == 0)
			    sts = -1;
		    }
		}
	    }
	    else {
		addr->sockaddr.raw.sa_family = AF_INET;
		sts = inet_pton(AF_INET, cp, &addr->sockaddr.inet.sin_addr);
	    }
	    if (sts <= 0) {
		__pmSockAddrFree(addr);
		addr = NULL;
	    }
	}
    }
    return addr;
}

#if defined(IS_MINGW)
#define inet_ntop win32_inet_ntop
#endif

/*
 * Convert an address to a string.
 * The caller must free the buffer.
 */
char *
__pmSockAddrToString(const __pmSockAddr *addr)
{
    char str[INET6_ADDRSTRLEN];
    int family;
    const char *sts;

    sts = NULL;
    family = addr->sockaddr.raw.sa_family;
    if (family == AF_INET)
	sts = inet_ntop(family, (void *)&addr->sockaddr.inet.sin_addr, str, sizeof(str));
    else if (family == AF_INET6)
	sts = inet_ntop(family, (void *)&addr->sockaddr.ipv6.sin6_addr, str, sizeof(str));
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (family == AF_UNIX)
	return strdup(addr->sockaddr.local.sun_path);
#endif
    if (sts == NULL)
	return NULL;
    return strdup(str);
}

__pmSockAddr *
__pmSockAddrFirstSubnetAddr(const __pmSockAddr *netAddr, int maskBits)
{
     __pmSockAddr	*addr;
    
    /* Make a copy of the net address for iteration purposes. */
    addr = __pmSockAddrDup(netAddr);
    if (addr) {
	/*
	 * Construct the first address in the subnet based on the given number
	 * of mask bits.
	 */
	if (addr->sockaddr.raw.sa_family == AF_INET) {
	    /* An inet address. The ip address is in network byte order. */
	    unsigned ip = ntohl(addr->sockaddr.inet.sin_addr.s_addr);
	    ip = __pmFirstInetSubnetAddr (ip, maskBits);
	    addr->sockaddr.inet.sin_addr.s_addr = htonl(ip);
	}
	else if (addr->sockaddr.raw.sa_family == AF_INET6) {
	    __pmFirstIpv6SubnetAddr(addr->sockaddr.ipv6.sin6_addr.s6_addr, maskBits);
	}
	else {
	    /* not applicable to other address families, e.g. AF_LOCAL. */
	    pmNotifyErr(LOG_ERR,
			  "%s:__pmSockAddrFirstSubnetAddr: Unsupported address family: %d\n",
			  __FILE__, addr->sockaddr.raw.sa_family);
	    __pmSockAddrFree(addr);
	    return NULL;
	}
    }

    return addr;
}

__pmSockAddr *
__pmSockAddrNextSubnetAddr(__pmSockAddr *addr, int maskBits)
{
    if (addr) {
	/*
	 * Construct the next address in the subnet based on the given the
	 * previous address and the given number of mask bits.
	 */
	if (addr->sockaddr.raw.sa_family == AF_INET) {
	    /* An inet address. The ip address is in network byte order. */
	    unsigned ip = ntohl(addr->sockaddr.inet.sin_addr.s_addr);
	    unsigned newIp = __pmNextInetSubnetAddr(ip, maskBits);

	    /* Is this the final address? */
	    if (newIp == ip) {
		__pmSockAddrFree(addr);
		return NULL;
	    }
	    addr->sockaddr.inet.sin_addr.s_addr = htonl(newIp);	    
	}
	else if (addr->sockaddr.raw.sa_family == AF_INET6) {
	    unsigned char *newAddr =
		__pmNextIpv6SubnetAddr(addr->sockaddr.ipv6.sin6_addr.s6_addr, maskBits);

	    if (newAddr == NULL) {
		/* This is the final address. */
		__pmSockAddrFree(addr);
		return NULL;
	    }
	}
	else {
	    /* not applicable to other address families, e.g. AF_LOCAL. */
	    pmNotifyErr(LOG_ERR,
			  "%s:__pmSockAddrFirstSubnetAddr: Unsupported address family: %d\n",
			  __FILE__, addr->sockaddr.raw.sa_family);
	    __pmSockAddrFree(addr);
	    return NULL;
	}
    }

    return addr;
}

void
__pmSockAddrFree(__pmSockAddr *sockaddr)
{
    free(sockaddr);
}

__pmSockAddr *
__pmLoopBackAddress(int family)
{
    __pmSockAddr* addr;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /* There is no loopback address for a unix domain socket. */
    if (family == AF_UNIX)
	return NULL;
#endif

    addr = __pmSockAddrAlloc();
    if (addr != NULL)
        __pmSockAddrInit(addr, family, INADDR_LOOPBACK, 0);
    return addr;
}

int
__pmInitSocket(int fd, int family)
{
    int sts;
    int	nodelay = 1;
    struct linger nolinger = {1, 0};
    char errmsg[PM_MAXERRMSGLEN];

    if ((sts = __pmSetSocketIPC(fd)) < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    /* Don't linger on close */
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_LINGER, (char *)&nolinger,
		   (__pmSockLen)sizeof(nolinger)) < 0) {
	pmNotifyErr(LOG_WARNING,
		      "%s:__pmInitSocket(fd=%d, family=%d): __pmSetSockOpt SO_LINGER: %s\n",
		      __FILE__, fd, family, netstrerror_r(errmsg, sizeof(errmsg)));
    }

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (family == AF_UNIX)
	return fd;
#endif

    /* Avoid 200 ms delay. This option is not supported for unix domain sockets. */
    if (__pmSetSockOpt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay,
		       (__pmSockLen)sizeof(nodelay)) < 0) {
	pmNotifyErr(LOG_WARNING,
		      "%s:__pmInitSocket(fd=%d, family=%d): __pmSetSockOpt TCP_NODELAY: %s\n",
		      __FILE__, fd, family, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    return fd;
}

int
__pmCreateSocket(void)
{
    int sts, fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -neterror();
    if ((sts = __pmInitSocket(fd, AF_INET)) < 0)
        return sts;
    return fd;
}

int
__pmCreateIPv6Socket(void)
{
    int sts, fd, on;
    __pmSockLen onlen = sizeof(on);

    if ((fd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
	return -neterror();

    /*
     * Disable IPv4-mapped connections
     * Must explicitly check whether that worked, for ipv6.enabled=false
     * kernels.  Setting then testing is the most reliable way we've found.
     */
    on = 1;
    (void)__pmSetSockOpt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
    on = 0;
    sts = __pmGetSockOpt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, &onlen);
    if (sts < 0 || on != 1) {
	pmNotifyErr(LOG_ERR, "%s:__pmCreateIPv6Socket: IPV6 is not supported\n", __FILE__);
	close(fd);
	return -EOPNOTSUPP;
    }

    if ((sts = __pmInitSocket(fd, AF_INET6)) < 0)
        return sts;
    return fd;
}

int
__pmCreateUnixSocket(void)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    int sts, fd;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
	return -neterror();

    if ((sts = __pmInitSocket(fd, AF_UNIX)) < 0)
        return sts;

    return fd;
#else
    pmNotifyErr(LOG_ERR, "%s:__pmCreateUnixSocket: AF_UNIX is not supported\n", __FILE__);
    return -EOPNOTSUPP;
#endif
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

int
__pmListen(int fd, int backlog)
{
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    __pmSockAddr *sockAddr = (__pmSockAddr *)addr;
    fd = accept(fd, &sockAddr->sockaddr.raw, addrlen);
    __pmCheckAcceptedAddress(sockAddr);
    return fd;
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSockAddr *sock = (__pmSockAddr *)addr;

    if (pmDebugOptions.context && pmDebugOptions.desperate) {
	char *sockname = __pmSockAddrToString(sock);
	fprintf(stderr, "%s:__pmBind(fd=%d, family=%d, port=%d, addr=%s)\n",
	    __FILE__, fd, __pmSockAddrGetFamily(sock), __pmSockAddrGetPort(sock),
	    sockname);
	free(sockname);
    }
    if (sock->sockaddr.raw.sa_family == AF_INET)
        return bind(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.inet));
    if (sock->sockaddr.raw.sa_family == AF_INET6)
        return bind(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.ipv6));
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (sock->sockaddr.raw.sa_family == AF_UNIX)
        return bind(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.local));
#endif
    pmNotifyErr(LOG_ERR,
		"%s:__pmBind: Invalid address family: %d\n", __FILE__, sock->sockaddr.raw.sa_family);
    errno = EAFNOSUPPORT;
    return -1; /* failure */
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSockAddr *sock = (__pmSockAddr *)addr;
    int		sts;
    if (sock->sockaddr.raw.sa_family == AF_INET)
        sts = connect(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.inet));
    else if (sock->sockaddr.raw.sa_family == AF_INET6)
        sts = connect(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.ipv6));
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (sock->sockaddr.raw.sa_family == AF_UNIX)
        sts = connect(fd, &sock->sockaddr.raw, sizeof(sock->sockaddr.local));
#endif
    else {
	pmNotifyErr(LOG_ERR,
		"%s:__pmConnect: Invalid address family: %d\n", __FILE__, sock->sockaddr.raw.sa_family);
	errno = EAFNOSUPPORT;
	return -1; /* failure */
    }

    if (sts >= 0)
	return sts;

    /* something bad happened ... */
    sts = neterror();
    if (sts == EINPROGRESS) {
	/*
	 * not sure if connect() is done, wait on select() ... 5 secs seems
	 * long enough to wait given that for EINPROGRESS it seems safe
	 * to assume select() will return "real soon now"
	 */
	__pmFdSet	fdset;
	struct timeval	wait = { 5,0 };
	int		lsts;

	__pmFD_ZERO(&fdset);
	__pmFD_SET(fd, &fdset);
	if ((lsts = __pmSelectWrite(fd+1, &fdset, &wait)) == 1) {
	    sts = __pmConnectCheckError(fd);
	}
	else if (lsts == 0)
	    sts = ETIMEDOUT;
	else 
	    sts = neterror();
    }

    if (sts == 0)
	/* OK now ... */
	return 0;

    /* persistent badness ... */
    errno = sts;
    return -1;
}

int
__pmConnectWithFNDELAY(int fd, void *addr, __pmSockLen addrlen)
{
    return __pmConnect(fd, addr, addrlen);
}

int
__pmConnectTo(int fd, const __pmSockAddr *addr, int port)
{
    int sts, fdFlags = __pmGetFileStatusFlags(fd);
    __pmSockAddr myAddr;

    if (pmDebugOptions.context && pmDebugOptions.desperate) {
	char	*sockname = __pmSockAddrToString(addr);
	fprintf(stderr, "__pmConnectTo(%d, %s, %d)\n", fd, sockname, port);
	free(sockname);
    }

    myAddr = *addr;
    if (port >= 0)
	__pmSockAddrSetPort(&myAddr, port);

    if (__pmSetFileStatusFlags(fd, fdFlags | FNDELAY) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];

	pmNotifyErr(LOG_ERR, "%s:__pmConnectTo: cannot set FNDELAY - "
		      "fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      __FILE__, fd, fdFlags | FNDELAY,
		      osstrerror_r(errmsg, sizeof(errmsg)));
    }
    
    if (__pmConnectWithFNDELAY(fd, &myAddr, sizeof(myAddr)) < 0) {
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
	pmNotifyErr(LOG_ERR, 
		"%s:__pmConnectCheckError: __pmGetSockOpt(SO_ERROR) failed: %s\n",
		__FILE__, netstrerror_r(errmsg, sizeof(errmsg)));
    }
    return so_err;
}

int
__pmConnectRestoreFlags(int fd, int fdFlags)
{
    int sts;
    char errmsg[PM_MAXERRMSGLEN];

    if (__pmSetFileStatusFlags(fd, fdFlags) < 0) {
	pmNotifyErr(LOG_WARNING, "%s:__pmConnectRestoreFlags: cannot restore "
		      "flags fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      __FILE__, fd, fdFlags, osstrerror_r(errmsg, sizeof(errmsg)));
    }

    if ((fdFlags = __pmGetFileDescriptorFlags(fd)) >= 0)
	sts = __pmSetFileDescriptorFlags(fd, fdFlags | FD_CLOEXEC);
    else
        sts = fdFlags;

    if (sts == -1) {
        pmNotifyErr(LOG_WARNING, "%s:__pmConnectRestoreFlags: "
		      "fcntl(%d) get/set flags failed: %s\n",
		      __FILE__, fd, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    return fd;
}

int
__pmSetConnectTimeout(double timeout)
{
    if (timeout < 0.0)
	return -EINVAL;

    PM_LOCK(auxconnect_lock);
    /* THREADSAFE - no locks acquired in pmtimevalFromReal() */
    pmtimevalFromReal(timeout, &conn_wait);
    conn_wait_done = 1;
    PM_UNLOCK(auxconnect_lock);
    return 0;
}

double
__pmConnectTimeout(void)
{
    char	*timeout_str, *end_ptr;
    double	timeout;

    /* get optional PMCD connection timeout from the environment */
    PM_LOCK(auxconnect_lock);
    if (!conn_wait_done) {
	conn_wait_done = 1;
	PM_LOCK(__pmLock_extcall);
	timeout_str = getenv("PMCD_CONNECT_TIMEOUT");		/* THREADSAFE */
	if (timeout_str != NULL) {
	    timeout = strtod(timeout_str, &end_ptr);
	    if (*end_ptr != '\0' || timeout < 0.0) {
		char	*fromenv = strdup(timeout_str);
		PM_UNLOCK(__pmLock_extcall);
		PM_UNLOCK(auxconnect_lock);
		pmNotifyErr(LOG_WARNING, "%s:__pmAuxConnectPMCDPort: "
			      "ignored bad PMCD_CONNECT_TIMEOUT = '%s'\n",
			      __FILE__, fromenv);
		PM_LOCK(auxconnect_lock);
		free(fromenv);
	    }
	    else {
		PM_UNLOCK(__pmLock_extcall);
		/* THREADSAFE - no locks acquired in pmtimevalFromReal() */
		pmtimevalFromReal(timeout, &conn_wait);
	    }
	}
	else
	    PM_UNLOCK(__pmLock_extcall);
    }

    /* THREADSAFE - no locks acquired in pmtimevalToReal() */
    timeout = pmtimevalToReal(&conn_wait);
    PM_UNLOCK(auxconnect_lock);

    return timeout;
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

/*
 * This interface is old and mouldy (exposed via impl.h many years ago)
 * and very much deprecated.  It was replaced by __pmAuxConnectPMCDPort.
 *
 * The implementation here is retained for any (out-of-tree) application
 * that might have called this interface directly ... the implementation
 * is correct when $PMCD_PORT is unset, or set to a single numeric port
 * number, i.e. the old semantics
 */
int
__pmAuxConnectPMCD(const char *hostname)
{
    static int		*pmcd_ports = NULL;

    PM_LOCK(auxconnect_lock);
    if (pmcd_ports == NULL) {
	/*
	 * THREADSAFE - only __pmLock_extcall acquired during call to
	 * __pmPMCDAddPorts()
	 */
	__pmPMCDAddPorts(&pmcd_ports, 0);
    }
    PM_UNLOCK(auxconnect_lock);

    /* __pmPMCDAddPorts discovers at least one valid port, if it returns. */
    return __pmAuxConnectPMCDPort(hostname, pmcd_ports[0]);
}

int
__pmAuxConnectPMCDPort(const char *hostname, int pmcd_port)
{
    __pmSockAddr	*myAddr;
    __pmHostEnt		*servInfo;
    int			fd;
    int			sts;
    int			fdFlags[PM_FDSET_SIZE];
    __pmFdSet		allFds;
    __pmFdSet		readyFds;
    int			maxFd;
    void		*enumIx;
    struct timeval	stv;
    struct timeval	*pstv;
    int			rc;
    int			i;

    if ((servInfo = __pmGetAddrInfo(hostname)) == NULL) {
	if (pmDebugOptions.context) {
	    PM_LOCK(__pmLock_extcall);
	    fprintf(stderr, "%s:__pmAuxConnectPMCDPort(%s, %d) : "
			    "hosterror=%d, ``%s''\n",
		    __FILE__, hostname, pmcd_port, hosterror(),
		    hoststrerror());		/* THREADSAFE */
	    PM_UNLOCK(__pmLock_extcall);
	}
	return -EHOSTUNREACH;
    }

    /*
     * We want to respect the connect timeout that has been configured, but we
     * may have more than one address to try. Do this by creating a socket for
     * each address and then using __pmSelectWrite() to wait for one of them to
     * respond. That way, the timeout is applied to all of the addresses
     * simultaneously. First, create the sockets, add them to the fd set
     * and try to connect.
     */
    __pmConnectTimeout();
    __pmFD_ZERO(&allFds);
    maxFd = -1;
    enumIx = NULL;
    for (myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx);
	 myAddr != NULL;
	 myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
#if !defined(IS_SOLARIS) && !defined(IS_MINGW)
	const __pmAddrInfo	*ai;
	/*
	 * We should be ignoring any entry without SOCK_STREAM (= 1),
	 * since we're connecting with TCP only. The other entries
	 * (SOCK_RAW, SOCK_DGRAM) are not relevant.
	 */
	ai = __pmHostEntGetAddrInfo(servInfo, enumIx);
	if (ai->ai_socktype != SOCK_STREAM) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate) {
		char *sockname = __pmSockAddrToString(myAddr);
		fprintf(stderr, "__pmAuxConnectPMCDPort(%s, %d): skip socket %s because type=%d not %d\n",
			  hostname, pmcd_port, sockname, ai->ai_socktype, SOCK_STREAM);
		free(sockname);
	    }
	    __pmSockAddrFree(myAddr);
	    continue;
	}
#endif
	/* Create a socket */
	if (__pmSockAddrIsInet(myAddr))
	    fd = __pmCreateSocket();
	else if (__pmSockAddrIsIPv6(myAddr))
	    fd = __pmCreateIPv6Socket();
	else {
	    pmNotifyErr(LOG_ERR, 
			  "%s:__pmAuxConnectPMCDPort(%s, %d) : invalid address family %d\n",
			  __FILE__, hostname, pmcd_port, __pmSockAddrGetFamily(myAddr));
	    fd = -EINVAL;
	}
	if (fd < 0 || fd >= PM_FDSET_SIZE) {
	    if (pmDebugOptions.context && pmDebugOptions.desperate) {
		char *sockname = __pmSockAddrToString(myAddr);
		fprintf(stderr, "__pmAuxConnectPMCDPort: failed to create socket %s, errno=%d\n", sockname, errno);
		free(sockname);
	    }
	    __pmSockAddrFree(myAddr);
	    continue; /* Try the next address */
	}

	/* Attempt to connect */
	fdFlags[fd] = __pmConnectTo(fd, myAddr, pmcd_port);
	__pmSockAddrFree(myAddr);
	if (fdFlags[fd] < 0) {
	    /* __pmConnectTo() has closed the fd, try next address. */
	    continue;
	}

	/* Add it to the fd set. */
	__pmFD_SET(fd, &allFds);
	if (fd > maxFd)
	    maxFd = fd;
    }
    __pmHostEntFree(servInfo);

    /* If we were unable to open any sockets, then give up. */
    if (maxFd == -1)
	return -ECONNREFUSED;

    /* FNDELAY and we're in progress - wait on select */
    __pmFD_COPY(&readyFds, &allFds);
    /*
     * THREADSAFE - conn_wait only changed in one-trip code in
     * __pmConnectTimeout()
     */
    stv = conn_wait;
    pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
    rc = __pmSelectWrite(maxFd+1, &readyFds, pstv);

    /* Figure out what happened. */
    if (rc == 0)
	fd = -ETIMEDOUT;
    else if (rc < 0)
	fd = -neterror();
    else {
	/*
	 * Examine the fd set and choose the first successfully connected
	 * socket, if any.
	 * Note that because rc > 0, at least one fd is ready and 'fd' will
	 * definitely get set. However, initialize it to keep coverity happy.
	 */
	fd = -EINVAL;
	for (i = 0; i <= maxFd; ++i) {
	    if (__pmFD_ISSET(i, &readyFds)) {
		/* Successful connection? */
		sts = __pmConnectCheckError(i);
		if (sts == 0) {
		    fd = i;
		    break;
		}
		fd = -sts;
	    }
	}
    }

    /* Clean up the unused fds. */
    for (i = 0; i <= maxFd; ++i) {
	if (i != fd && __pmFD_ISSET(i, &allFds))
	    __pmCloseSocket(i);
    }

    /* Unsuccessful? */
    if (fd < 0)
        return fd;

    /*
     * If we're here, it means we have a valid connection; restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called
     */
    return __pmConnectRestoreFlags(fd, fdFlags[fd]);
}

/*
 * Return the path to the default PMCD local unix domain socket.
 * Returns a pointer to a static buffer which can be used directly.
 * Return the path regardless of whether unix domain sockets are
 * supported by our build. Other functions can then print reasonable
 * messages if an attempt is made to use one.
 */
const char *
__pmPMCDLocalSocketDefault(void)
{
    static char 		pmcd_socket[MAXPATHLEN];

    PM_LOCK(auxconnect_lock);
    if (pmcd_socket[0] == '\0') {
	char *envstr;
	PM_LOCK(__pmLock_extcall);
	if ((envstr = getenv("PMCD_SOCKET")) != NULL) {		/* THREADSAFE */
	    pmsprintf(pmcd_socket, sizeof(pmcd_socket), "%s", envstr);
	    PM_UNLOCK(__pmLock_extcall);
	}
	else {
	    PM_UNLOCK(__pmLock_extcall);
	    pmsprintf(pmcd_socket, sizeof(pmcd_socket), "%s%c" "pmcd.socket",
		     pmGetConfig("PCP_RUN_DIR"), pmPathSeparator());
	}
    }
    PM_UNLOCK(auxconnect_lock);

    return pmcd_socket;
}

int
__pmAuxConnectPMCDUnixSocket(const char *sock_path)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    __pmSockAddr	*myAddr;
    int			fd = -1;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags = 0;
    struct timeval	stv;
    struct timeval	*pstv;
    __pmFdSet		wfds;
    int			rc;

    __pmConnectTimeout();

    /* Initialize the socket address. */
    myAddr = __pmSockAddrAlloc();
    if (myAddr == NULL) {
        pmNotifyErr(LOG_ERR, "%s:__pmAuxConnectPMCDUnixSocket(%s): out of memory\n", __FILE__, sock_path);
	return -1;
    }
    __pmSockAddrSetFamily(myAddr, AF_UNIX);
    __pmSockAddrSetPath(myAddr, sock_path);

    /* Create a socket */
    fd = __pmCreateUnixSocket();
    if (fd < 0) {
	char	errmsg[PM_MAXERRMSGLEN];

	pmNotifyErr(LOG_ERR, 
		      "%s:__pmAuxConnectPMCDUnixSocket(%s): unable to create socket: %s\n",
		      __FILE__, sock_path, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmSockAddrFree(myAddr);
	return fd;
    }

    /* Attempt to connect */
    fdFlags = __pmConnectTo(fd, myAddr, -1);
    __pmSockAddrFree(myAddr);
    if (fdFlags < 0)
	return -ECONNREFUSED;

    /*
     * FNDELAY and we're in progress - wait on select
     *
     * THREADSAFE - conn_wait only changed in one-trip code in
     * __pmConnectTimeout()
     */
    stv = conn_wait;
    pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;
    __pmFD_ZERO(&wfds);
    __pmFD_SET(fd, &wfds);
    if ((rc = __pmSelectWrite(fd+1, &wfds, pstv)) == 1) {
	sts = __pmConnectCheckError(fd);
    }
    else if (rc == 0) {
	sts = ETIMEDOUT;
    }
    else {
	sts = (rc < 0) ? neterror() : EINVAL;
    }
 
    if (sts != 0) {
	/* Unsuccessful connection. */
	if (sts == ENOENT)
	    sts = ECONNREFUSED;
	__pmCloseSocket(fd);
	fd = -sts;
    }

    if (fd < 0)
        return fd;

    /*
     * If we're here, it means we have a valid connection; restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called
     */
    return __pmConnectRestoreFlags(fd, fdFlags);
#else
    pmNotifyErr(LOG_ERR, 
		  "%s:__pmAuxConnectPMCDUnixSocket(%s) is not supported\n", __FILE__, sock_path);
    return -1;
#endif
}

char *
__pmHostEntGetName(__pmHostEnt *he)
{
    __pmSockAddr	*addr;
    void		*enumIx;

    if (he->name == NULL) {
	/* Try to reverse lookup the host name.
	 * Check each address until the reverse lookup succeeds.
	 */
	enumIx = NULL;
	for (addr = __pmHostEntGetSockAddr(he, &enumIx);
	     addr != NULL;
	     addr = __pmHostEntGetSockAddr(he, &enumIx)) {
	    he->name = __pmGetNameInfo(addr);
	    if (pmDebugOptions.desperate) {
		char *sockname = __pmSockAddrToString(addr);
		fprintf(stderr, "%s:__pmHostEntGetName: __pmGetNameInfo(%s) returns %s\n", __FILE__, sockname, he->name);
		free(sockname);
	    }
	    __pmSockAddrFree(addr);
	    if (he->name != NULL)
		break;
	}
	if (he->name == NULL)
	    return NULL;
    }
    if (pmDebugOptions.desperate)
        fprintf(stderr, "%s:__pmHostEntGetName -> %s\n", __FILE__, he->name);

    return strdup(he->name);
}

__pmSockAddr *
__pmHostEntGetSockAddr(const __pmHostEnt *he, void **ei)
{
    __pmAddrInfo *ai;
    __pmSockAddr *addr;

    /* The enumerator index (*ei) is actually a pointer to the current address info. */
    if (*ei == NULL)
        *ei = ai = he->addresses;
    else {
        ai = *ei;
        *ei = ai = ai->ai_next;
    }
    if (*ei == NULL)
        return NULL; /* no (more) addresses in the chain. */

    /* Now allocate a socket address and copy the data. */
    addr = __pmSockAddrAlloc();
    if (addr == NULL) {
        pmNotifyErr(LOG_ERR, "%s:__pmHostEntGetSockAddr: out of memory\n", __FILE__);
        *ei = NULL;
	return NULL;
    }
    memcpy(&addr->sockaddr.raw, ai->ai_addr, ai->ai_addrlen);

    return addr;
}

const __pmAddrInfo *
__pmHostEntGetAddrInfo(const __pmHostEnt *he, const void *ei)
{
    const __pmAddrInfo *ai;

    /*
     * If it is not NULL, the enumerator index, ei, is a pointer to the current
     * address info.
     */
    if (ei == NULL)
        ai = he->addresses;
    else
        ai = ei;

    return ai;
}

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
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (address->sockaddr.raw.sa_family == AF_UNIX) {
	/* The name info is the socket path. */
	return strdup(address->sockaddr.local.sun_path);
    }
#endif
    else {
	pmNotifyErr(LOG_ERR,
		"%s:__pmGetNameInfo: Invalid address family: %d\n", __FILE__, address->sockaddr.raw.sa_family);
        sts = EAI_FAMILY;
    } 

    if (pmDebugOptions.desperate) {
        if (sts != 0) {
            fprintf(stderr, "%s:__pmGetNameInfo: family=%d getnameinfo()-> %d %s\n", __FILE__, address->sockaddr.raw.sa_family, sts, gai_strerror(sts));
        }
    }


    return sts == 0 ? strdup(buf) : NULL;
}

__pmHostEnt *
__pmGetAddrInfo(const char *hostName)
{
    __pmHostEnt *hostEntry;
    struct addrinfo hints;
    int sts;

    hostEntry = __pmHostEntAlloc();
    if (hostEntry != NULL) {
        memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
#ifdef HAVE_AI_ADDRCONFIG
	hints.ai_flags = AI_ADDRCONFIG; /* Only return configured address types */
#endif

	PM_LOCK(__pmLock_extcall);
	sts = getaddrinfo(hostName, NULL, &hints, &hostEntry->addresses);
	PM_UNLOCK(__pmLock_extcall);
	if (sts != 0) {
	    if (pmDebugOptions.desperate)
		fprintf(stderr, "%s:__pmGetAddrInfo: getaddrinfo(%s, ...) -> %d %s\n", __FILE__, hostName, sts, gai_strerror(sts));
	    __pmHostEntFree(hostEntry);
	    hostEntry = NULL;
	}
	/* Leave the host name NULL. It will be looked up on demand in __pmHostEntGetName(). */
    }
    if (pmDebugOptions.desperate) {
	if (hostEntry == NULL)
	    fprintf(stderr, "%s:__pmGetAddrInfo(%s) -> NULL\n", __FILE__, hostName);
	else {
	    __pmSockAddr	*addr;
	    void		*enumIx = NULL;
	    char		*str;
	    int			found = 0;
	    fprintf(stderr, "%s:__pmGetAddrInfo(%s) -> ", __FILE__, hostName);
	    for (addr = __pmHostEntGetSockAddr(hostEntry, &enumIx);
		 addr != NULL;
		 addr = __pmHostEntGetSockAddr(hostEntry, &enumIx)) {
		str = __pmSockAddrToString(addr);
		if (str != NULL) {
		    if (found == 0)
			fprintf(stderr, "%s", str);
		    else
			fprintf(stderr, ", %s", str);
		    found++;
		    free(str);
		}
		__pmSockAddrFree(addr);
	    }
	    if (found)
		fprintf(stderr, "\n");
	    else
		fprintf(stderr, "no ip addrs?\n");
	}
    }
    return hostEntry;
}

unsigned
__pmFirstInetSubnetAddr(unsigned ip, int maskBits)
{
    unsigned mask = ~((1 << (32 - maskBits)) - 1);
    return ip & mask;
}

unsigned
__pmNextInetSubnetAddr(unsigned ip, int maskBits)
{
    unsigned mask = (1 << (32 - maskBits)) - 1;

    /* Is this the final address? If so then return the address unchanged.*/
    if ((ip & mask) == mask)
	return ip;

    /* Bump up the address. */
    return ++ip;
}

unsigned char *
__pmFirstIpv6SubnetAddr(unsigned char *addr, int maskBits)
{
    unsigned mask;
    int ix;
    /*
     * Manipulate the ipv6 address one byte at a time. There is no
     * host/network byte order.
     * Mask the byte at the subnet mask boundary. Leave the higher order bytes
     * alone and clear the lower order bytes.
     */
    ix = maskBits / 8;
    if (ix < 16) {
	maskBits %= 8;
	mask = ~((1 << (8 - maskBits)) - 1);
	addr[ix] &= mask;
	for (++ix; ix < 16; ++ix)
	    addr[ix] = 0;
    }

    return addr;
}

unsigned char *
__pmNextIpv6SubnetAddr(unsigned char *addr, int maskBits)
{
    unsigned mask;
    int ix, ix1;
    /*
     * Manipulate the ipv6 address one byte at a time. There is no
     * host/network byte order.
     * First determine whether this is the final address. Do this by
     * comparing the high order bits of the subnet against the maximum.
     */
    ix = maskBits / 8;
    if (ix < 16) {
	maskBits %= 8;
	mask = (1 << (8 - maskBits)) - 1;
	if ((addr[ix] & mask) == mask) {
	    /* The highest order bits are maxed out. Check the remaining bits. */
	    for (++ix; ix < 16; ++ix) {
		if (addr[ix] != 0xff)
		    break;
	    }
	}
    }
    if (ix >= 16) {
	/* This is the final address. */
	return NULL;
    }

    /* Bump up the address. Don't forget to carry into the higher order bits
       when necessary. */
    for (ix1 = 15; ix1 >= ix; --ix1) {
	++addr[ix1];
	if (addr[ix1] != 0)
	    break; /* no carry */
    }

    return addr;
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

#if !defined(HAVE_SECURE_SOCKETS)

void
__pmCloseSocket(int fd)
{
    if (fd >= 0) {
	__pmResetIPC(fd);
#if defined(IS_MINGW)
	closesocket(fd);
#else
	close(fd);
#endif
    }
}

int
__pmShutdownSecureSockets(void)
{
    return 0;
}

int
__pmInitAuthClients(void)
{
    return 0;
}

int
__pmDataIPCSize(void)
{
    return 0;
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    (void)fd;
    (void)hostname;

    /*
     * We cannot handle many flags here (no support), in particular:
     * PDU_FLAG_SECURE (OpenSSL)
     * PDU_FLAG_SECURE_ACK (OpenSSL)
     * PDU_FLAG_NO_NSS_INIT (OpenSSL)
     * PDU_FLAG_AUTH (SASL2)
     *
     * But we can still talk to a pmcd that requires credentials, provided
     * we are using unix domain sockets (the kernel provides the auth info
     * to pmcd in this case, with no other special sauce required).  We're
     * also able to communicate container attributes.
     */
    if (flags & PDU_FLAG_CREDS_REQD)
	if (__pmHashSearch(PCP_ATTR_UNIXSOCK, attrs) == NULL)
	    return -EOPNOTSUPP;
    if (flags & (PDU_FLAG_SECURE|PDU_FLAG_AUTH|PDU_FLAG_COMPRESS))
	return -EOPNOTSUPP;
    return 0;
}

int
__pmSocketClosed(void)
{
    int		sts;

    switch ((sts = oserror())) {
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
#ifdef IS_MINGW
	case WSAECONNRESET:
	case WSAETIMEDOUT:
	case WSAENETDOWN:
	case WSAENETUNREACH:
	/* case WSAEHOSTDOWN: same as EHOSTDOWN above */
	case WSAEHOSTUNREACH:
	case WSAECONNREFUSED:
#endif
	    return 1;
    }
    if (pmDebugOptions.pdu && pmDebugOptions.desperate)
	fprintf(stderr, "__pmSocketClosed: unmatched error=%d\n", sts);
    return 0;
}

ssize_t
__pmWrite(int socket, const void *buffer, size_t length)
{
    if (socket < 0)
	return -EBADF;

    return write(socket, buffer, length);
}

ssize_t
__pmRead(int socket, void *buffer, size_t length)
{
    if (socket < 0)
	return -EBADF;

    return read(socket, buffer, length);
}

ssize_t
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
    if (socket < 0)
	return -EBADF;

    return send(socket, buffer, length, flags);
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
    ssize_t	size;

    if (socket < 0)
	return -EBADF;

    size = recv(socket, buffer, length, flags);
    if (pmDebugOptions.pdu && pmDebugOptions.desperate) {
	fprintf(stderr, "%s:__pmRecv(%d, ..., %d, 0x%x) -> %d",
		__FILE__, socket, (int)length, flags, (int)size);
	if (size < 0) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, " %s", pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	}
	fputc('\n', stderr);
    }
    return size;
}

int
__pmSocketReady(int fd, struct timeval *timeout)
{
    __pmFdSet	onefd;

    if (fd < 0)
	return -EBADF;

    FD_ZERO(&onefd);
    FD_SET(fd, &onefd);
    return select(fd+1, &onefd, NULL, NULL, timeout);
}

#endif /* !HAVE_SECURE_SOCKETS */
