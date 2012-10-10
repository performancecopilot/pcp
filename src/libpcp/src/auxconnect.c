/*
 * Copyright (c) 2012 Red Hat.
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
#include <fcntl.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

/* default connect timeout is 5 seconds */
static struct timeval	canwait = { 5, 000000 };

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
__pmConnectTo(int fd, const __pmSockAddrIn *addr, int port)
{
    int sts, fdFlags = __pmGetFileStatusFlags(fd);
    __pmSockAddrIn myAddr;

    myAddr = *addr;
    __pmSetPort(&myAddr, port);

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
    int	so_err;
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

int
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
    __pmSockAddrIn	myAddr;
    __pmHostEnt		servInfo;
    char		*sibuf;
    int			fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    sibuf = __pmAllocHostEntBuffer();
    if (__pmGetHostByName(hostname, &servInfo, sibuf) == NULL) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    fprintf(stderr, "__pmAuxConnectPMCDPort(%s, %d) : hosterror=%d, ``%s''\n",
		    hostname, pmcd_port, hosterror(), hoststrerror());
	}
#endif
	__pmFreeHostEntBuffer(sibuf);
	PM_UNLOCK(__pmLock_libpcp);
	return -EHOSTUNREACH;
    }

    __pmConnectTimeout();

    if ((fd = __pmCreateSocket()) < 0) {
	__pmFreeHostEntBuffer(sibuf);
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    __pmInitSockAddr(&myAddr, htonl(INADDR_ANY), 0);
    __pmSetSockAddr(&myAddr, &servInfo);
    __pmFreeHostEntBuffer(sibuf);
    PM_UNLOCK(__pmLock_libpcp);

    if ((fdFlags = __pmConnectTo(fd, &myAddr, pmcd_port)) >= 0) {
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
    return fdFlags;
}

#if !defined(HAVE_NSS)

int
__pmCreateSocket(void)
{
    int sts, fd;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -neterror();
    if ((sts = __pmInitSocket(fd)) < 0)
        return sts;
    return fd;
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
 
void
__pmInitSockAddr(__pmSockAddrIn *addr, int address, int port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = address;
    addr->sin_port = port;
}

void
__pmSetSockAddr(__pmSockAddrIn *addr, __pmHostEnt *he)
{
    memcpy(&addr->sin_addr, he->h_addr, he->h_length);
}

void
__pmSetPort(__pmSockAddrIn *addr, int port)
{
    addr->sin_port = htons(port);
}

int
__pmListen(int fd, int backlog)
{
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    return accept(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    return bind(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    return connect(fd, (struct sockaddr *)addr, addrlen);
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
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
    return send(socket, buffer, length, flags);
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
    return recv(socket, buffer, length, flags);
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

char *
__pmAllocHostEntBuffer(void)
{
    return NULL;
}

void
__pmFreeHostEntBuffer(char *buffer)
{
    /* No buffer was actually allocated.  Our work here is done. */
}

__pmHostEnt *
__pmGetHostByName(const char *hostName, __pmHostEnt *hostEntry, char *buffer)
{
    __pmHostEnt *he = gethostbyname(hostName);

    if (he == NULL)
	return NULL;
    *hostEntry = *he;
    return hostEntry;
}

__pmHostEnt *
__pmGetHostByAddr(__pmSockAddrIn *address, __pmHostEnt *hostEntry, char *buffer)
{
    __pmHostEnt *he = gethostbyaddr((void *)&address->sin_addr.s_addr, sizeof(address->sin_addr.s_addr), AF_INET);
    if (he == NULL)
	return NULL;
    *hostEntry = *he;
    return hostEntry;
}

__pmIPAddr
__pmHostEntGetIPAddr(const __pmHostEnt *he, int ix)
{
    return ((struct in_addr *)he->h_addr_list[ix])->s_addr;
}

void
__pmSetIPAddr(__pmIPAddr *addr, unsigned int a)
{
    *addr = a;
}

__pmIPAddr *
__pmMaskIPAddr(__pmIPAddr *addr, const __pmIPAddr *mask)
{
    *addr &= *mask;
    return addr;
}

int
__pmCompareIPAddr(const __pmIPAddr *addr1, const __pmIPAddr *addr2)
{
    return *addr1 - *addr2;
}

int
__pmIPAddrIsLoopBack(const __pmIPAddr *addr)
{
    return *addr == htonl(INADDR_LOOPBACK);
}

__pmIPAddr
__pmLoopbackAddress(void)
{
    return htonl(INADDR_LOOPBACK);
}

__pmIPAddr
__pmSockAddrInToIPAddr(const __pmSockAddrIn *inaddr)
{
    return __pmInAddrToIPAddr(&inaddr->sin_addr);
}

__pmIPAddr
__pmInAddrToIPAddr(const __pmInAddr *inaddr)
{
    return inaddr->s_addr;
}

int
__pmIPAddrToInt(const __pmIPAddr *addr)
{
    return *addr;
}

/*
 * Convert an address in network byte order to a string.
 * The caller must free the buffer.
 */
char *
__pmInAddrToString(__pmInAddr *address)
{
    char *buf = inet_ntoa(*address);

    if (buf == NULL)
	return NULL;
    return strdup(buf);
}

int
__pmStringToInAddr(const char *cp, __pmInAddr *inp)
{
#ifdef IS_MINGW
    unsigned long in;
    in = inet_addr(buf);
    inaddr.s_addr = in;
    return in == INADDR_NONE ? 0 : 1;
#else
    return inet_aton(cp, inp);
#endif
}

char *
__pmSockAddrInToString(__pmSockAddrIn *address)
{
    return __pmInAddrToString(&address->sin_addr);
}

#else	/* NSS */
/* NSS/NSPR file descriptors are not integers, however, integral file descriptors are expected
   in many parts of pcp. In order to deal with this assumption, when NSS/NSPR is available, we
   maintain a set of available integral file descriptors. The file descriptor number returned by
   __pmCreateSocket is a reference to this set and must be used for all further I/O operations
   on that socket.

   Since some interfaces (e.g. the IPC table) will use a mix of native file descriptors
   and NSPR ones, we need a way to distinguish them. Obtaining the hard max fd number using
   getrlimit() was considered, but a sysadmin could change this limit arbitrarily while we are
   running. We can't use negative values, since these indicate an error.

   There is a limit on the range of fd's which can be passed to the fd_set API. It is FD_SETSIZE.
   So, consider all fd's >= FD_SETSIZE to be ones which reference our set. Using this threshold will
   also allow us to easily manage mixed sets of native and NSPR fds.

   NB: __pmLock_libpcp must be held when accessing this set, since another thread could modify it
       at any time.
 */
static fd_set nsprFds;

#define NSPR_HANDLE_BASE FD_SETSIZE

static int
newNSPRHandle(void)
{
    int fd;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    for (fd = 0; fd < FD_SETSIZE; ++fd) {
        if (! FD_ISSET(fd, &nsprFds)) {
	    FD_SET(fd, &nsprFds);
	    PM_UNLOCK(__pmLock_libpcp);
	    return NSPR_HANDLE_BASE + fd;
	}
    }
    PM_UNLOCK(__pmLock_libpcp);

    /* No free handles available */
    return -1;
}

static void
freeNSPRHandle(int fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    FD_CLR(fd - NSPR_HANDLE_BASE, &nsprFds);
    PM_UNLOCK(__pmLock_libpcp);
}

int
__pmCreateSocket(void)
{
    int sts;
    int fd;
    PRFileDesc *nsprFd;

    /* Make sure that NSPR has been initialized */
    if (PR_Initialized() != PR_TRUE)
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);

    if ((nsprFd = PR_OpenTCPSocket(PR_AF_INET)) == NULL)
	return -neterror();

    fd = newNSPRHandle();
    __pmSetNSPRFdIPC(fd, nsprFd); /* Must be before __pmInitSocket */

    if ((sts = __pmInitSocket(fd)) < 0)
        return sts;

    return fd;
}

void
__pmCloseSocket(int fd)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    __pmResetIPC(fd);

    if (nsprFd) {
        freeNSPRHandle(fd);
	PR_Close(nsprFd);
	return;
    }

    /* We have a native fd */
#if defined(IS_MINGW)
    closesocket(fd);
#else
    close(fd);
#endif
}

static int
sockOptValue(const void *option_value, __pmSockLen option_len)
{
    switch(option_len) {
    case sizeof(int):
        return *(int *)option_value;
    default:
        __pmNotifyErr(LOG_ERR, "sockOptValue: invalid option length: %d\n", option_len);
	break;
    }
    return 0;
}

int
__pmSetSockOpt(int socket, int level, int option_name, const void *option_value,
	       __pmSockLen option_len)
{
    /* Map the request to the NSPR equivalent, if possible. */
    PRSocketOptionData odata;
    PRStatus prStatus;
    PRFileDesc *nsprFd = __pmNSPRFdIPC(socket);

    if (nsprFd) {
	switch(level) {
	case SOL_SOCKET:
	    switch(option_name) {
#ifdef IS_MINGW
	    case SO_EXCLUSIVEADDRUSE: {
		/* There is no direct mapping of this option in NSPR. The best we can do is to use
		   the native handle and call setsockopt on that handle. */
	        socket = PR_FileDesc2NativeHandle(nsprFd);
		return setsockopt(socket, level, option_name, option_value, option_len);
	    }
#endif
	    case SO_KEEPALIVE:
	        odata.option = PR_SockOpt_Keepalive;
		odata.value.keep_alive = sockOptValue(option_value, option_len);
		break;
	    case SO_LINGER: {
	        struct linger *linger = (struct linger *)option_value;
		odata.option = PR_SockOpt_Linger;
		odata.value.linger.polarity = linger->l_onoff;
		odata.value.linger.linger = linger->l_linger;
		break;
	    }
	    case SO_REUSEADDR:
	        odata.option = PR_SockOpt_Reuseaddr;
		odata.value.reuse_addr = sockOptValue(option_value, option_len);
		break;
	    default:
	        __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			      option_name);
		return -1;
	    }
	    break;
	case IPPROTO_TCP:
	    if (option_name == TCP_NODELAY) {
	        odata.option = PR_SockOpt_NoDelay;
		odata.value.no_delay = sockOptValue(option_value, option_len);
		break;
	    }
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for IPPROTO_TCP: %d\n",
			  option_name);
	    return -1;
	default:
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented level: %d\n", level);
	    return -1;
	}

	prStatus = PR_SetSocketOption(nsprFd, &odata);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }

    /* We have a native socket. */
    return setsockopt(socket, level, option_name, option_value, option_len);
}

int
__pmGetSockOpt(int socket, int level, int option_name, void *option_value,
	       __pmSockLen *option_len)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(socket);

    /* Map the request to the NSPR equivalent, if possible. */
    if (nsprFd) {
	switch(level) {
	case SOL_SOCKET:
	  switch(option_name) {
	  case SO_ERROR: {
	      /* There is no direct mapping of this option in NSPR. The best we can do is to use the
		 native fd and call getsockopt on that handle. */
	      socket = PR_FileDesc2NativeHandle(nsprFd);
	      return getsockopt(socket, level, option_name, option_value, option_len);
	  }
	  default:
	      __pmNotifyErr(LOG_ERR, "__pmGetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			  option_name);
	      return -1;
	  }
	  break;
	default:
	    __pmNotifyErr(LOG_ERR, "__pmGetSockOpt: unimplemented level: %d\n", level);
	    break;
	}
	return -1;
    }

    /* We have a native socket. */
    return getsockopt(socket, level, option_name, option_value, option_len);
}
 
void
__pmInitSockAddr(__pmSockAddrIn *addr, int address, int port)
{
    /* We expect the address and port number to be on network byte order.
       PR_InitializeNetAddr expected the port in host byte order.
       The ip field of __pmSockAddrIn (PRNetAddr) must be in network byte order. */
  PRStatus prStatus = PR_InitializeNetAddr (PR_IpAddrNull, ntohs(port), addr);
  if (prStatus != PR_SUCCESS)
    __pmNotifyErr(LOG_ERR, "__pmInitSockAddr: PR_InitializeNetAddr failure: %d\n",
		  PR_GetError());
  addr->inet.ip = address;
}

void
__pmSetSockAddr(__pmSockAddrIn *addr, __pmHostEnt *he)
{
    PR_EnumerateHostEnt(0, he, 0, addr);
}

void
__pmSetPort(__pmSockAddrIn *addr, int port)
{
    if (addr->raw.family == PR_AF_INET)
        addr->inet.port = htons(port);
    else if (addr->raw.family == PR_AF_INET6)
        addr->ipv6.port = htons(port);
}

int
__pmListen(int fd, int backlog)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        PRStatus prStatus;
	prStatus = PR_Listen(nsprFd, backlog);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }

    /* We have a native fd */
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        PRFileDesc *newSocket;
	newSocket = PR_Accept(nsprFd, (PRNetAddr *)addr, PR_INTERVAL_MIN);
	if (newSocket == NULL)
	  return -1;
	/* Add the accepted socket to the fd table. */
	fd = newNSPRHandle();
	__pmSetNSPRFdIPC(fd, newSocket);
	return fd;
    }

    /* We have a native fd */
    return accept(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        PRStatus prStatus;
	prStatus = PR_Bind(nsprFd, (PRNetAddr *)addr);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }

    /* We have a native fd */
    return bind(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        PRStatus prStatus;
	prStatus = PR_Connect(nsprFd, (PRNetAddr *)addr, PR_INTERVAL_NO_TIMEOUT);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }

    /* We have a native fd */
    return connect(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmGetFileStatusFlags(int fd)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        /* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   native fd and call fcntl on that handle. */
        fd = PR_FileDesc2NativeHandle(nsprFd);
    }
    return fcntl(fd, F_GETFL);
}

int
__pmSetFileStatusFlags(int fd, int flags)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        /* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   native fd and call fcntl on that handle. */
        fd = PR_FileDesc2NativeHandle(nsprFd);
    }
    return fcntl(fd, F_SETFL, flags);
}

int
__pmGetFileDescriptorFlags(int fd)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        /* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   native fd and call fcntl on that handle. */
        fd = PR_FileDesc2NativeHandle(nsprFd);
    }
    return fcntl(fd, F_GETFD);
}

int
__pmSetFileDescriptorFlags(int fd, int flags)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        /* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   native fd and call fcntl on that handle. */
        fd = PR_FileDesc2NativeHandle(nsprFd);
    }
    return fcntl(fd, F_SETFD, flags);
}

ssize_t
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
    /* Map the request to the NSPR equivalent, if possible. */
    PRFileDesc *nsprFd = __pmNSPRFdIPC(socket);
    if (nsprFd) {
	return PR_Write (nsprFd, buffer, length);
    }

    /* We have a native fd */
    return send(socket, buffer, length, flags);
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
    /* Map the request to the NSPR equivalent, if possible. */
    PRFileDesc *nsprFd = __pmNSPRFdIPC(socket);
    if (nsprFd) {
	return PR_Read (nsprFd, buffer, length);
    }

    /* We have a native fd */
    return recv(socket, buffer, length, flags);
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	FD_CLR(fd, &set->nspr_set);
	/* Reset the max fd, if necessary. */
	if (fd + 1 >= set->num_nspr_fds) {
	  for (--fd; fd >= 0; --fd) {
	    if (FD_ISSET(fd, &set->nspr_set))
	      break;
	  }
	  set->num_nspr_fds = fd + 1;
	}
	return;
    }

    FD_CLR(fd, &set->native_set);
    /* Reset the max fd, if necessary. */
    if (fd + 1 >= set->num_native_fds) {
        for (--fd; fd >= 0; --fd) {
	    if (FD_ISSET(fd, &set->native_set))
	        break;
	}
	set->num_native_fds = fd + 1;
    }
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	return FD_ISSET(fd, &set->nspr_set);
    }
    return FD_ISSET(fd, &set->native_set);
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
    PRFileDesc *nsprFd = __pmNSPRFdIPC(fd);
    if (nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	FD_SET(fd, &set->nspr_set);
	/* Reset the max fd, if necessary. */
	if (fd >= set->num_nspr_fds)
	  set->num_nspr_fds = fd + 1;
	return;
    }

    FD_SET(fd, &set->native_set);
    /* Reset the max fd, if necessary. */
    if (fd >= set->num_native_fds)
        set->num_native_fds = fd + 1;
}

void
__pmFD_ZERO(__pmFdSet *set)
{
    FD_ZERO(&set->nspr_set);
    FD_ZERO(&set->native_set);
    set->num_nspr_fds = 0;
    set->num_native_fds = 0;
}

void
__pmFD_COPY(__pmFdSet *s1, const __pmFdSet *s2)
{
    memcpy(s1, s2, sizeof(*s1));
}

static int
nsprSelect(int rwflag, __pmFdSet *fds, struct timeval *timeout)
{
    int nsprReady, nativeReady;
    PRIntervalTime nsprTimeout;
    PRPollDesc *pollfds;
    int fd;

    /* Convert the fd_set for the nspr fds to the form required by NSPR. */
    pollfds = malloc(fds->num_nspr_fds * sizeof(*pollfds));
    for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
      if (FD_ISSET(fd, &fds->nspr_set)) {
	pollfds[fd].fd = __pmNSPRFdIPC(NSPR_HANDLE_BASE + fd);
	pollfds[fd].in_flags = rwflag;
      }
      else
	pollfds[fd].fd = NULL;
    }

    /* Convert the given timeout to the form required by NSPR. */
    if (timeout != NULL) {
      nsprTimeout = PR_SecondsToInterval(timeout->tv_sec);
      nsprTimeout += PR_MicrosecondsToInterval(timeout->tv_usec);
    }
    else
      nsprTimeout = PR_INTERVAL_NO_TIMEOUT;

    /* Poll the given fds. */
    nsprReady = PR_Poll(pollfds, fds->num_nspr_fds, nsprTimeout);
    if (nsprReady < 0) {
      __pmNotifyErr(LOG_ERR, "nsprSelect: error polling nspr file descriptors\n");
      free(pollfds);
      return -1;
    }

    /* Convert the result back to the native format. */
    FD_ZERO(&fds->nspr_set);
    if (nsprReady != 0) {
        for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
	    if (pollfds[fd].fd != NULL && (pollfds[fd].out_flags & rwflag) != 0) {
	        FD_SET(fd, &fds->nspr_set);
		fds->num_nspr_fds = fd + 1;
	    }
	}
    }
    free(pollfds);

    nativeReady = 0;
    if (fds->num_native_fds > 0) {
        /* Use the native 'select' function on the native fds. Ignore the nfds
	   passed to us and use the number from the set itself. */
        if (rwflag == PR_POLL_READ)
	    nativeReady = select(fds->num_native_fds, &fds->native_set, NULL, NULL, timeout);
	else
	    nativeReady = select(fds->num_native_fds, NULL, &fds->native_set, NULL, timeout);
	if (nativeReady < 0) {
	    __pmNotifyErr(LOG_ERR, "nsprSelect: error polling native file descriptors\n");
	    return -1;
	}

	/* Reset the max fd. */
	for (fd = fds->num_native_fds - 1; fd >= 0; --fd) {
	    if (FD_ISSET(fd, &fds->native_set))
	        break;
	}
	fds->num_native_fds = fd + 1;
    }

    /* Return the total number of ready fds. */
    return nsprReady + nativeReady;
}

int
__pmSelectRead(int nfds, __pmFdSet *readfds, struct timeval *timeout)
{
    return nsprSelect(PR_POLL_READ, readfds, timeout);
}

int
__pmSelectWrite(int nfds, __pmFdSet *writefds, struct timeval *timeout)
{
    return nsprSelect(PR_POLL_WRITE, writefds, timeout);
}

char *
__pmAllocHostEntBuffer(void)
{
    char *buffer = malloc(PR_NETDB_BUF_SIZE);
    if (buffer == NULL)
        __pmNoMem("__pmAllocHostEntBuffer", PR_NETDB_BUF_SIZE, PM_FATAL_ERR);
    return buffer;
}

void
__pmFreeHostEntBuffer(char *buffer)
{
    free(buffer);
}

__pmHostEnt *
__pmGetHostByName(const char *hostName, __pmHostEnt *hostEntry, char *buffer)
{
    PRStatus prStatus = PR_GetHostByName(hostName, buffer, PR_NETDB_BUF_SIZE, hostEntry);
    return prStatus == PR_SUCCESS ? hostEntry : NULL;
}

__pmHostEnt *
__pmGetHostByAddr(__pmSockAddrIn *address, __pmHostEnt *hostEntry, char *buffer)
{
    PRStatus prStatus = PR_GetHostByAddr(address, buffer, PR_NETDB_BUF_SIZE, hostEntry);
    return prStatus == PR_SUCCESS ? hostEntry : NULL;
}

__pmIPAddr
__pmHostEntGetIPAddr(const __pmHostEnt *he, int ix)
{
    PRNetAddr address;
    PRIntn rc = PR_EnumerateHostEnt(0, he, 0, &address);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "__pmHostEntGetIPAddr: unable to obtain host address\n");
	return 0;
    }
    return __pmInAddrToIPAddr(&address);
}

void
__pmSetIPAddr(__pmIPAddr *addr, unsigned int a)
{
    *addr = a;
}

__pmIPAddr *
__pmMaskIPAddr(__pmIPAddr *addr, const __pmIPAddr *mask)
{
    *addr &= *mask;
    return addr;
}

int
__pmCompareIPAddr(const __pmIPAddr *addr1, const __pmIPAddr *addr2)
{
    return *addr1 - *addr2;
}

int
__pmIPAddrIsLoopBack(const __pmIPAddr *addr)
{
    return *addr == PR_INADDR_LOOPBACK;
}

__pmIPAddr
__pmLoopbackAddress(void)
{
    return PR_INADDR_LOOPBACK;
}

__pmIPAddr
__pmSockAddrInToIPAddr(const __pmSockAddrIn *inaddr)
{
    return __pmInAddrToIPAddr(inaddr);
}

__pmIPAddr
__pmInAddrToIPAddr(const __pmInAddr *inaddr)
{
    return inaddr->inet.ip;
}

int
__pmIPAddrToInt(const __pmIPAddr *addr)
{
    return *addr;
}

/*
 * Convert an address in network byte order to a string.
 * The caller must free the buffer.
 */
#define PM_NET_ADDR_STRING_SIZE 46 /* from the NSPR API reference */

char *
__pmInAddrToString(__pmInAddr *address)
{
  PRStatus prStatus;
  char     *buf = malloc(PM_NET_ADDR_STRING_SIZE);
  if (buf == NULL)
      return strdup("unknown");
  prStatus = PR_NetAddrToString(address, buf, PM_NET_ADDR_STRING_SIZE);
  if (prStatus != PR_SUCCESS) {
      free(buf);
      return NULL;
  }
  return buf;
}

int
__pmStringToInAddr(const char *cp, __pmInAddr *inp)
{
  PRStatus prStatus = PR_StringToNetAddr(cp, inp);
  return prStatus == PR_SUCCESS ? 1 : 0;
}

char *
__pmSockAddrInToString(__pmSockAddrIn *address)
{
  return __pmInAddrToString(address);
}

#endif	/* NSS */
