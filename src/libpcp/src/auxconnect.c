/*
 * Copyright (c) 2000,2004,2005 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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
__pmCreateSocket(void)
{
    int			fd;
    int			sts;
    int			nodelay=1;
    struct linger	nolinger = {1, 0};

    if ((fd = __pmSocket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -neterror();

    if ((sts = __pmSetSocketIPC(fd)) < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    /* avoid 200 ms delay */
    if (__pmSetSockOpt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay,
		   (mysocklen_t)sizeof(nodelay)) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): __pmSetSockOpt TCP_NODELAY: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    /* don't linger on close */
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_LINGER, (char *)&nolinger,
		   (mysocklen_t)sizeof(nolinger)) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): __pmSetSockOpt SO_LINGER: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    return fd;
}

int
__pmSocket(int domain, int type, int protocol)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return socket(domain, type, protocol);
#endif
}

void
__pmCloseSocket(int fd)
{
    __pmResetIPC(fd);

#ifdef HAVE_NSS
    //#error __FUNCTION__ is not implemented for NSS
#elif defined(IS_MINGW)
    closesocket(fd);
#else
    close(fd);
#endif
}

int
__pmSetSockOpt(int socket, int level, int option_name, const void *option_value,
	       mysocklen_t option_len)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return setsockopt(socket, level, option_name, option_value, option_len);
#endif
}

void
__pmInitSockAddr(__pmSockAddrIn *addr, int address, int port)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
#else
  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = address;
  addr->sin_port = port;
#endif
}

int
__pmListen(int fd, int backlog)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return listen(fd, backlog);
#endif
}

int
__pmAccept(int reqfd, __pmSockAddr *addr, mysocklen_t *addrlen)
{
#ifdef HAVE_NSS
  return PR_Accept(reqfd, addr, PR_INTERVAL_MIN);
#else
  return accept(reqfd, addr, addrlen);
#endif
}

int
__pmBind(int fd, __pmSockAddr *addr, mysocklen_t addrlen)
{
#ifdef HAVE_NSS
  PRStatus prStatus = PR_Bind(fd, addr);
  return prStatus == PR_SUCCESS ? 0 : -1;
#else
  return bind(fd, addr, addrlen);
#endif
}

int
__pmConnectTo(int fd, const struct sockaddr *addr, int port)
{
    int sts, fdFlags = fcntl(fd, F_GETFL);
    struct sockaddr_in myAddr;

    memcpy(&myAddr, addr, sizeof (struct sockaddr_in));
    myAddr.sin_port = htons(port);

    if (fcntl(fd, F_SETFL, fdFlags | FNDELAY) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
        __pmNotifyErr(LOG_ERR, "__pmConnectTo: cannot set FNDELAY - "
		      "fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags|FNDELAY , osstrerror_r(errmsg, sizeof(errmsg)));
    }
    
    if (connect(fd, (struct sockaddr*)&myAddr, sizeof(myAddr)) < 0) {
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
    mysocklen_t	olen = sizeof(int);

    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, (void *)&so_err, &olen) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	so_err = neterror();
	__pmNotifyErr(LOG_ERR, 
		"__pmConnectCheckError: getsockopt(SO_ERROR) failed: %s\n",
		netstrerror_r(errmsg, sizeof(errmsg)));
    }
    return so_err;
}

int
__pmConnectRestoreFlags(int fd, int fdFlags)
{
    int sts;

    if (fcntl(fd, F_SETFL, fdFlags) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_WARNING,"__pmConnectRestoreFlags: cannot restore "
		      "flags fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags, osstrerror_r(errmsg, sizeof(errmsg)));
    }

    if ((fdFlags = fcntl(fd, F_GETFD)) >= 0)
        sts = fcntl(fd, F_SETFD, fdFlags | FD_CLOEXEC);
    else
        sts = fdFlags;

    if (sts == -1) {
	char	errmsg[PM_MAXERRMSGLEN];
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
    struct sockaddr_in	myAddr;
    struct hostent*	servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((servInfo = gethostbyname(hostname)) == NULL) {
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

    if ((fd = __pmCreateSocket()) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    PM_UNLOCK(__pmLock_libpcp);

    if ((fdFlags = __pmConnectTo(fd, (const struct sockaddr *)&myAddr,
				 pmcd_port)) < 0) {
	return (fdFlags);
    } else { /* FNDELAY and we're in progress - wait on select */
	int	rc;
	fd_set wfds;
	// TODO hope this goes away with ASYNC API stuff else need lock
	// and more tricky logic to make sure canwait has indeed been
	// initialized
	struct timeval stv = canwait;
	struct timeval *pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;

	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	sts = 0;
	if ((rc = select(fd+1, NULL, &wfds, NULL, pstv)) == 1) {
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
    }
    
    /* If we're here, it means we have valid connection, restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called */
    return __pmConnectRestoreFlags (fd, fdFlags);
}

ssize_t
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return send(socket, buffer, length, flags);
#endif
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return recv(socket, buffer, length, flags);
#endif
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
#else
  FD_CLR(fd, set);
#endif
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return 0;
#else
  return FD_ISSET(fd, set);
#endif
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
#else
  FD_SET(fd, set);
#endif
}

void
__pmFD_ZERO(__pmFdSet *set)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
#else
  FD_ZERO(set);
#endif
}

int
__pmSelectRead(int nfds, __pmFdSet *readfds, struct timeval *timeout)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return -1;
#else
  return select(nfds, readfds, NULL, NULL, timeout);
#endif
}

char *
__pmAllocHostEntBuffer (void)
{
#ifdef HAVE_NSS
  // TODO NSS: check for malloc failure
  return malloc(PR_NETDB_BUF_SIZE);
#else
  /* No buffer needed */
  return NULL;
#endif
}

void
__pmFreeHostEntBuffer (char *buffer)
{
#ifdef HAVE_NSS
  free(buffer);
#else
  /* No buffer was actually allocated. */
#endif
}

__pmHostEnt *
__pmGetHostByName(const char *hostName, __pmHostEnt *hostEntry, char *buffer)
{
#ifdef HAVE_NSS
    PRStatus prStatus = PR_GetHostByName(hostName, buffer, PR_NETDB_BUF_SIZE, hostEntry);
    return prStatus == PR_SUCCESS ? hostEntry : NULL;
#else
    __pmHostEnt *he = gethostbyname(hostName);
    if (he == NULL)
        return NULL;
    *hostEntry = *he;
    return hostEntry;
#endif
}

__pmIPAddr *
__pmHostEntGetIPAddr(const __pmHostEnt *he, int ix)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return NULL;
#else
  return & ((struct in_addr *)he->h_addr_list[ix])->s_addr;
#endif
}

void
__pmSetIPAddr(__pmIPAddr *addr, unsigned int a)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
#else
  *addr = a;
#endif
}

__pmIPAddr *
__pmMaskIPAddr(__pmIPAddr *addr, const __pmIPAddr *mask)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return addr;
#else
  *addr &= *mask;
  return addr;
#endif
}

int
__pmCompareIPAddr(const __pmIPAddr *addr1, const __pmIPAddr *addr2)
{
#ifdef HAVE_NSS
  return memcmp(addr1, addr2, sizeof(*addr1));
#else
  return *addr1 - *addr2;
#endif
}

int
__pmIPAddrIsLoopBack(const __pmIPAddr *addr)
{
#ifdef HAVE_NSS
  //  #error __FUNCTION__ is not implemented for NSS
  return 0;
#else
  return *addr == htonl(INADDR_LOOPBACK);
#endif
}

const __pmIPAddr
__pmSockAddrInToIPAddr(const __pmSockAddrIn *inaddr)
{
#ifdef HAVE_NSS
  return 0;
#else
  return __pmInAddrToIPAddr(&inaddr->sin_addr);
#endif
}

const __pmIPAddr
__pmInAddrToIPAddr(const __pmInAddr *inaddr)
{
#ifdef HAVE_NSS
  /* For NSPR, these point to the same type. */
  return 0;
#else
  return inaddr->s_addr;
#endif
}

int
__pmIPAddrToInt(const __pmIPAddr *addr)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return 0;
#else
  return *addr;
#endif
}
