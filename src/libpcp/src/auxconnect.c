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
    __pmFD		fd;
    int			sts;
    int			nodelay=1;
    struct linger	nolinger = {1, 0};

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -neterror();

    if ((sts = __pmSetSocketIPC(fd)) < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    /* avoid 200 ms delay */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nodelay,
		   (mysocklen_t)sizeof(nodelay)) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): setsockopt TCP_NODELAY: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    /* don't linger on close */
    if (setsockopt(fd, SOL_SOCKET, SO_LINGER, (char *)&nolinger,
		   (mysocklen_t)sizeof(nolinger)) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_ERR, 
		      "__pmCreateSocket(%d): setsockopt SO_LINGER: %s\n",
		      fd, netstrerror_r(errmsg, sizeof(errmsg)));
    }

    return fd;
}

__pmFD
__pmSocket(int domain, int type, int protocol)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  return socket(domain, type, protocol);
#endif
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
__pmConnect(__pmFD fd, __pmSockAddr *addr, mysocklen_t addrlen)
{
#ifdef HAVE_NSS
  PRStatus prStatus = PR_Connect(fd, addr, PR_INTERVAL_NO_TIMEOUT);
  return prStatus == PR_SUCCESS ? 0 : -1;
#else
  return connect(fd, addr, addrlen);
#endif
}

int
__pmConnectTo(int fd, const __pmSockAddr *addr, int port)
{
    int sts, fdFlags = fcntl(fd, F_GETFL);
    __pmSockAddrIn myAddr;

    memcpy(&myAddr, addr, sizeof (myAddr));
    myAddr.sin_port = htons(port);

    if (fcntl(fd, F_SETFL, fdFlags | FNDELAY) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
        __pmNotifyErr(LOG_ERR, "__pmConnectTo: cannot set FNDELAY - "
		      "fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags|FNDELAY , osstrerror_r(errmsg, sizeof(errmsg)));
    }
    
    if (__pmConnect(fd, (__pmSockAddr*)&myAddr, sizeof(myAddr)) < 0) {
	sts = neterror();
	if (sts != EINPROGRESS) {
	    close(fd);
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
	close(fd);
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
    __pmHostEnt*	servInfo;
    __pmFD		fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((servInfo = __pmGetHostByName(hostname)) == NULL) {
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

    if ((fd = __pmCreateSocket()) == PM_ERROR_FD) {
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    memset(&myAddr, 0, sizeof(myAddr));	/* Arrgh! &myAddr, not myAddr */
    myAddr.sin_family = AF_INET;
    memcpy(&myAddr.sin_addr, servInfo->h_addr, servInfo->h_length);
    PM_UNLOCK(__pmLock_libpcp);

    if ((fdFlags = __pmConnectTo(fd, (const __pmSockAddr *)&myAddr,
				 pmcd_port)) < 0) {
	return (fdFlags);
    } else { /* FNDELAY and we're in progress - wait on __pmSelectWrite */
	int	rc;
	__pmFdSet wfds;
	// TODO hope this goes away with ASYNC API stuff else need lock
	// and more tricky logic to make sure canwait has indeed been
	// initialized
	struct timeval stv = canwait;
	struct timeval *pstv = (stv.tv_sec || stv.tv_usec) ? &stv : NULL;

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
	    close(fd);
	    return -sts;
	}
    }
    
    /* If we're here, it means we have valid connection, restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called */
    return __pmConnectRestoreFlags (fd, fdFlags);
}

int
__pmListen(__pmFD fd, int backlog)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  return listen(fd, backlog);
#endif
}

__pmFD
__pmAccept(__pmFD reqfd, __pmSockAddr *addr, mysocklen_t *addrlen)
{
#ifdef HAVE_NSS
  return PR_Accept(reqfd, addr, PR_INTERVAL_MIN);
#else
  return accept(reqfd, addr, addrlen);
#endif
}

int
__pmBind(__pmFD fd, __pmSockAddr *addr, mysocklen_t addrlen)
{
#ifdef HAVE_NSS
  PRStatus prStatus = PR_Bind(fd, addr);
  return prStatus == PR_SUCCESS ? 0 : -1;
#else
  return bind(fd, addr, addrlen);
#endif
}

char *
__pmNetAddrToString(__pmInAddr *address) {
#ifdef HAVE_NSS
  static char buf[PM_NET_ADDR_STRING_SIZE];
  PR_NetAddrToString(address, buf, sizeof (buf));
  return buf;
#else
  return inet_ntoa(*address);
#endif
}

__pmHostEnt *
__pmGetHostByName(const char *hostName)
{
#ifdef HAVE_NSS
  static __pmHostEnt hostEntry;
  static char buf[PR_NETDB_BUF_SIZE];
  PRStatus prStatus = PR_GetHostByName(hostname, buf, sizeof(buf), & hostEntry);
  return prStatus == PR_SUCCESS ? & hostEntry : NULL;
#else
  return gethostbyname(hostName);
#endif
}

__pmHostEnt *
__pmGetHostByAddr(__pmSockAddrIn *address)
{
#ifdef HAVE_NSS
  static __pmHostEnt hostEntry;
  static char buf[PR_NETDB_BUF_SIZE];
  PRStatus prStatus = PR_GetHostByName(address, buf, sizeof(buf), & hostEntry);
  return prStatus == PR_SUCCESS ? & hostEntry : NULL;
#else
  return gethostbyaddr((void *)&address->sin_addr.s_addr, sizeof(address->sin_addr.s_addr), AF_INET);
#endif
}

__pmHostEnt *
__pmGetHostByInAddr(__pmInAddr *address)
{
#ifdef HAVE_NSS
  return __pmGetHostByAddr(address);
#else
  return gethostbyaddr((void *)&address->s_addr, sizeof(address->s_addr), AF_INET);
#endif
}

void
__pmFD_CLR(__pmFD fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  FD_CLR(fd, set);
#endif
}

int
__pmFD_ISSET(__pmFD fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  return FD_ISSET(fd, set);
#endif
}

void
__pmFD_SET(__pmFD fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  FD_SET(fd, set);
#endif
}

void
__pmFD_ZERO(__pmFdSet *set)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  FD_ZERO(set);
#endif
}

__pmFD
__pmUpdateMaxFD(__pmFD fd, int maxFd)
{
#ifdef HAVE_NSS
  /* The NSPR select API (PR_Poll) does not use max fd, so leave it alone. */
  return maxFd;
#else
  if (fd > maxFd)
    maxFd = fd;
  return maxFd;
#endif
}

__pmFD
__pmIncrFD(__pmFD fd)
{
#ifdef HAVE_NSS
  /* The NSPR select API (PR_Poll) does not use max fd, so leave it alone. */
  return fd;
#else
  return fd + 1;
#endif
}

int
__pmSelectRead(int nfds, __pmFdSet *readfds, struct timeval *timeout)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  return select(nfds, readfds, NULL, NULL, timeout);
#endif
}

int
__pmSelectWrite(int nfds, __pmFdSet *writefds, struct timeval *timeout)
{
#ifdef HAVE_NSS
  #error __FUNCTION__ is not implemented for NSS
#else
  return select(nfds, NULL, writefds, NULL, timeout);
#endif
}
