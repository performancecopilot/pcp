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

#ifdef HAVE_NSS
/* NSS/NSPR file descriptors are not integers, however, integral file descriptors are expected
   in many parts of pcp. In order to deal with this assumption, when NSS/NSPR is available, we
   maintain a table of file descriptors. The file descriptor number returned by __pmCreateSocket
   is an index into this table and must be used for all further I/O operations on that socket.

   Since some interfaces (e.g. the IPC table) will use a mix of native file descriptor numbers
   and indexed ones, we need a way to distinguish them. Obtaining the hard max fd number using
   getrlimit() was considered, but a sysadmin could change this limit arbitrarily while we are
   running. We can't use negative values, since these indicate an error.

   There is a limit on the range of fd's which can be passed to the fd_set API. It is FD_SETSIZE.
   So, consider all fd's >= FD_SETSIZE to be ones which index our table. Using this threshold will
   also allow us to easily manage mixed sets of natrive and indexed fds.

   NB: __pmLock_libpcp must be held when accessing this table, since another thread could grow it
       or claim a NULL entry at any time.
 */
#define PM_FD_TABLE_THRESHOLD FD_SETSIZE
static PRFileDesc **fdTable = NULL;
static size_t fdTableSize = 0;

static int indexedFd(int ix);

static int
newFdTableEntry(PRFileDesc *fd)
{
#define MIN_FDS_ALLOC 8
    int i, j;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    /* Look for an empty entry in the table */
    for (i = 0; i < fdTableSize; ++i) {
        if (fdTable[i] == NULL) {
	    fdTable[i] = fd;
	    PM_UNLOCK(__pmLock_libpcp);
	    return i;
	}
    }

    /* No empty entries, grow the table. */
    fdTableSize = fdTableSize? fdTableSize * 2 : MIN_FDS_ALLOC;
    fdTable = (PRFileDesc **)realloc(fdTable, sizeof(*fdTable) * fdTableSize);
    if (fdTable == NULL)
      return -ENOMEM;
    fdTable[i] = fd;
    for (j = i + 1; j < fdTableSize; ++j)
      fdTable[j] = NULL;
    PM_UNLOCK(__pmLock_libpcp);

    return indexedFd(i);
}

static PRFileDesc *
getFdTableEntry(int ix)
{
    /* A conveniant function for ensuring that __pmLock_libpcp is held when accessing the table. */
    PRFileDesc *entry;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    entry = fdTable[ix];
    PM_UNLOCK(__pmLock_libpcp);
    return entry;
}

static int
fdTableIndex(int fd)
{
    /* Check whether this fd refers to an active open file descriptor.
       if so, return the file fd table index, otherwise return -1. */
    fd = fd - PM_FD_TABLE_THRESHOLD;
    if (fd < 0)
        return -1;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (fd >= fdTableSize)
        return -1;
    if (fdTable[fd] == NULL)
        fd = -1;
    PM_UNLOCK(__pmLock_libpcp);
    return fd;
}

static int
isIndexedFd(int fd)
{
    /* Determine whether the given file descriptor refers to the fd table. */
#ifdef HAVE_NSS
    if (fd >= PM_FD_TABLE_THRESHOLD)
      return 1;
#endif
    return 0;
}

static int
indexedFd(int ix)
{
    /* Convert the given fd table index to an indexed fd. */
    return PM_FD_TABLE_THRESHOLD + ix;
}
#endif /* HAVE_NSS */

int
__pmCreateSocket(void)
{
    int sts;
    int fd;
#ifdef HAVE_NSS
    PRFileDesc *nsprFd;

    if ((nsprFd = PR_OpenTCPSocket(PR_AF_INET)) == NULL)
	return -neterror();

    fd = newFdTableEntry(nsprFd);
#else
    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -neterror();
#endif

    if ((sts = __pmInitSocket(fd)) < 0)
        return sts;

    return fd;
}

int
__pmInitSocket(int fd)
{
    int sts;
    int			nodelay=1;
    struct linger	nolinger = {1, 0};

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

void
__pmCloseSocket(int fd)
{
    __pmResetIPC(fd);

#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmCloseSocket: invalid file descriptor: %d\n", fd);
	    return;
	}
	PR_Close(getFdTableEntry(ix));
	return;
    }
#endif
    /* We have a native fd */
#if defined(IS_MINGW)
    closesocket(fd);
#else
    close(fd);
#endif
}

#ifdef HAVE_NSS
static int
sockOptValue(const void *option_value, mysocklen_t option_len)
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
#endif

int
__pmSetSockOpt(int socket, int level, int option_name, const void *option_value,
	       mysocklen_t option_len)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    PRSocketOptionData odata;
    PRStatus prStatus;

    if (isIndexedFd(socket)) {
	int ix;
	if ((ix = fdTableIndex(socket) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: invalid file descriptor: %d\n", socket);
	    return -1;
	}

	switch(level) {
	case SOL_SOCKET:
	    switch(option_name) {
#ifdef IS_MINGW
	    case SO_EXCLUSIVEADDRUSE: {
	        int fd;
		/* There is no direct mapping of this option in NSPR. The best we can do is to use
		   the (deprecated) function PR_FileDesc2NativeHandle to get the native fd and call
		   setsockopt on that handle. */
		if ((fd = PR_FileDesc2NativeHandle(getFdTableEntry(ix))) < 0) {
		  __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: invalid file descriptor: %d\n", socket);
		  return -1;
		}
		return setsockopt(fd, level, option_name, option_value, option_len);
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

	prStatus = PR_SetSocketOption(getFdTableEntry(ix), &odata);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }
#endif
    /* We have native socket. */
    return setsockopt(socket, level, option_name, option_value, option_len);
}

int
__pmGetSockOpt(int socket, int level, int option_name, void *option_value,
	       mysocklen_t *option_len)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    if (isIndexedFd(socket)) {
	int ix;
	if ((ix = fdTableIndex(socket) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmGetSockOpt: invalid file descriptor: %d\n", socket);
	    return -1;
	}

	switch(level) {
	case SOL_SOCKET:
	  switch(option_name) {
	  case SO_ERROR: {
	      int fd;
	      /* There is no direct mapping of this option in NSPR. The best we can do is to use the
		 (deprecated) function PR_FileDesc2NativeHandle to get the native fd and call
		 getsockopt on that handle. */
	      if ((fd = PR_FileDesc2NativeHandle(getFdTableEntry(ix))) < 0) {
	          __pmNotifyErr(LOG_ERR, "__pmGetSockOpt: invalid file descriptor: %d\n", socket);
		  return -1;
	      }
	      return getsockopt(fd, level, option_name, option_value, option_len);
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
#endif
    /* We have native socket. */
    return getsockopt(socket, level, option_name, option_value, option_len);
}
 
void
__pmInitSockAddr(__pmSockAddrIn *addr, int address, int port)
{
#ifdef HAVE_NSS
    /* We expect the address and port number to be on network byte order, but NSPR expects them
       it in host byte order. */
  PRStatus prStatus = PR_InitializeNetAddr (PR_IpAddrNull, ntohs(port), addr);
  if (prStatus != PR_SUCCESS)
    __pmNotifyErr(LOG_ERR, "__pmInitSockAddr: PR_InitializeNetAddr failure: %d\n",
		  PR_GetError());
  addr->inet.ip = ntohl(address);
#else
  memset(addr, 0, sizeof(*addr));
  addr->sin_family = AF_INET;
  addr->sin_addr.s_addr = address;
  addr->sin_port = port;
#endif
}

void
__pmSetSockAddr(__pmSockAddrIn *addr, __pmHostEnt *he)
{
#ifdef HAVE_NSS
    PR_EnumerateHostEnt(0, he, 0, addr);
#else
    memcpy(&addr->sin_addr, he->h_addr, he->h_length);
#endif
}

void
__pmSetPort(__pmSockAddrIn *addr, int port)
{
#ifdef HAVE_NSS
    if (addr->raw.family == PR_AF_INET)
        addr->inet.port = port;
    else if (addr->raw.family == PR_AF_INET6)
        addr->ipv6.port = port;
#else
    addr->sin_port = htons(port);
#endif
}


int
__pmListen(int fd, int backlog)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
        PRStatus prStatus;
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmListen: invalid file descriptor: %d\n", fd);
	    return -1;
	}
	prStatus = PR_Listen(getFdTableEntry(ix), backlog);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }
#endif
    /* We have a native fd */
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, mysocklen_t *addrlen)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
        PRFileDesc *newSocket;
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmAccept: invalid file descriptor: %d\n", fd);
	    return -1;
	}
	newSocket = PR_Accept(getFdTableEntry(ix), (PRNetAddr *)addr, PR_INTERVAL_MIN);
	if (newSocket == NULL)
	  return -1;
	/* Add the accepted socket to the fd table. */
	fd = newFdTableEntry(newSocket);
	return fd;
    }
#endif
    /* We have a native fd */
    return accept(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmBind(int fd, void *addr, mysocklen_t addrlen)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
        PRStatus prStatus;
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmBind: invalid file descriptor: %d\n", fd);
	    return -1;
	}
	prStatus = PR_Bind(getFdTableEntry(ix), (PRNetAddr *)addr);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }
#endif
    /* We have a native fd */
    return bind(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnect(int fd, void *addr, mysocklen_t addrlen)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
        PRStatus prStatus;
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmConnect: invalid file descriptor: %d\n", fd);
	    return -1;
	}
	prStatus = PR_Connect(getFdTableEntry(ix), (PRNetAddr *)addr, PR_INTERVAL_NO_TIMEOUT);
	return prStatus == PR_SUCCESS ? 0 : -1;
    }
#endif
    /* We have a native fd */
    return connect(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnectTo(int fd, const __pmSockAddrIn *addr, int port)
{
    int sts, fdFlags = __pmFcntlGetFlags(fd);
    __pmSockAddrIn myAddr;

    myAddr = *addr;
    __pmSetPort(&myAddr, htons(port));

    if (__pmFcntlSetFlags(fd, fdFlags | FNDELAY) < 0) {
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
    mysocklen_t	olen = sizeof(int);

    if (__pmGetSockOpt(fd, SOL_SOCKET, SO_ERROR, (void *)&so_err, &olen) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
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

    if (__pmFcntlSetFlags(fd, fdFlags) < 0) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_WARNING,"__pmConnectRestoreFlags: cannot restore "
		      "flags fcntl(%d,F_SETFL,0x%x) failed: %s\n",
		      fd, fdFlags, osstrerror_r(errmsg, sizeof(errmsg)));
    }

    if ((fdFlags = __pmFcntlGetFlags(fd)) >= 0)
        sts = __pmFcntlSetFlags(fd, fdFlags | FD_CLOEXEC);
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
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    __pmInitSockAddr(&myAddr, htonl(INADDR_ANY), 0);
    __pmSetSockAddr(&myAddr, &servInfo);
    __pmFreeHostEntBuffer(sibuf);
    PM_UNLOCK(__pmLock_libpcp);

    if ((fdFlags = __pmConnectTo(fd, &myAddr, pmcd_port)) < 0) {
	return (fdFlags);
    } else { /* FNDELAY and we're in progress - wait on select */
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
	    __pmCloseSocket(fd);
	    return -sts;
	}
    }
    
    /* If we're here, it means we have valid connection, restore the
     * flags and make sure this file descriptor is closed if exec() is
     * called */
    return __pmConnectRestoreFlags (fd, fdFlags);
}

int
__pmFcntlGetFlags(int fd)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmFcntlGetFlags: invalid file descriptor: %d\n", fd);
	    return -1;
	}

	/* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   (deprecated) function PR_FileDesc2NativeHandle to get the native fd and call
	   fcntl on that handle. */
	if ((fd = PR_FileDesc2NativeHandle(getFdTableEntry(ix))) < 0) {
	  __pmNotifyErr(LOG_ERR, "__pmFcntlGetFlags: invalid file descriptor: %d\n", fd);
	  return -1;
	}
    }
#endif
  return fcntl(fd, F_GETFL);
}

int
__pmFcntlSetFlags(int fd, int flags)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmFcntlGetFlags: invalid file descriptor: %d\n", fd);
	    return -1;
	}

	/* There is no direct mapping of this function in NSPR. The best we can do is to use the
	   (deprecated) function PR_FileDesc2NativeHandle to get the native fd and call
	   fcntl on that handle. */
	if ((fd = PR_FileDesc2NativeHandle(getFdTableEntry(ix))) < 0) {
	  __pmNotifyErr(LOG_ERR, "__pmFcntlGetFlags: invalid file descriptor: %d\n", fd);
	  return -1;
	}
    }
#endif
    return fcntl(fd, F_SETFL, flags);
}

ssize_t
__pmSend(int socket, const void *buffer, size_t length, int flags)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    if (isIndexedFd(socket)) {
	int ix;
	if ((ix = fdTableIndex(socket) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmSend: invalid file descriptor: %d\n", socket);
	    return -1;
	}
	return PR_Write (getFdTableEntry(ix), buffer, length);
    }
#endif
    /* We have a native fd */
    return send(socket, buffer, length, flags);
}

ssize_t
__pmRecv(int socket, void *buffer, size_t length, int flags)
{
#ifdef HAVE_NSS
    /* Map the request to the NSPR equivalent, if possible. */
    if (isIndexedFd(socket)) {
	int ix;
	if ((ix = fdTableIndex(socket) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmRecv: invalid file descriptor: %d\n", socket);
	    return -1;
	}
	return PR_Read (getFdTableEntry(ix), buffer, length);
    }
#endif
    /* We have a native fd */
    return recv(socket, buffer, length, flags);
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmFD_CLR: invalid file descriptor: %d\n", fd);
	    return;
	}
	FD_CLR(ix, &set->indexedSet);
	return;
    }
    FD_CLR(fd, &set->nativeSet);
#else
  FD_CLR(fd, set);
#endif
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmFD_CLR: invalid file descriptor: %d\n", fd);
	    return 0;
	}
	return FD_ISSET(ix, &set->indexedSet);
    }
    return FD_ISSET(fd, &set->nativeSet);
#else
    return FD_ISSET(fd, set);
#endif
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
#ifdef HAVE_NSS
    if (isIndexedFd(fd)) {
	int ix;
	if ((ix = fdTableIndex(fd) < 0)) {
	    __pmNotifyErr(LOG_ERR, "__pmFD_SET: invalid file descriptor: %d\n", fd);
	    return;
	}
	FD_SET(ix, &set->indexedSet);
	return;
    }
    FD_SET(fd, &set->nativeSet);
#else
    FD_SET(fd, set);
#endif
}

void
__pmFD_ZERO(__pmFdSet *set)
{
#ifdef HAVE_NSS
    FD_ZERO(&set->indexedSet);
    FD_ZERO(&set->nativeSet);
#else
    FD_ZERO(set);
#endif
}

void
__pmFD_COPY(__pmFdSet *s1, const __pmFdSet *s2)
{
    memcpy(s1, s2, sizeof(*s1));
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

int
__pmSelectWrite(int nfds, __pmFdSet *writefds, struct timeval *timeout)
{
#ifdef HAVE_NSS
  //#error __FUNCTION__ is not implemented for NSS
  return 0;
#else
  return select(nfds, NULL, writefds, NULL, timeout);
#endif
}

char *
__pmAllocHostEntBuffer (void)
{
#ifdef HAVE_NSS
    char *buffer = malloc(PR_NETDB_BUF_SIZE);
    if (buffer == NULL)
        __pmNoMem("__pmAllocHostEntBuffer", PR_NETDB_BUF_SIZE, PM_FATAL_ERR);
    return buffer;
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

__pmHostEnt *
__pmGetHostByAddr(__pmSockAddrIn *address, __pmHostEnt *hostEntry, char *buffer)
{
#ifdef HAVE_NSS
    PRStatus prStatus = PR_GetHostByAddr(address, buffer, PR_NETDB_BUF_SIZE, hostEntry);
    return prStatus == PR_SUCCESS ? hostEntry : NULL;
#else
    __pmHostEnt *he = gethostbyaddr((void *)&address->sin_addr.s_addr, sizeof(address->sin_addr.s_addr), AF_INET);
    if (he == NULL)
        return NULL;
    *hostEntry = *he;
    return hostEntry;
#endif
}

__pmIPAddr
__pmHostEntGetIPAddr(const __pmHostEnt *he, int ix)
{
#ifdef HAVE_NSS
    PRNetAddr address;
    PRIntn rc = PR_EnumerateHostEnt(0, he, 0, &address);
    if (rc < 0) {
        __pmNotifyErr(LOG_ERR, "__pmHostEntGetIPAddr: unable to obtain host address\n");
	return 0;
    }
    return __pmInAddrToIPAddr(&address);
#else
    return ((struct in_addr *)he->h_addr_list[ix])->s_addr;
#endif
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
#ifdef HAVE_NSS
    return *addr == PR_INADDR_LOOPBACK;
#else
    return *addr == htonl(INADDR_LOOPBACK);
#endif
}

const __pmIPAddr
__pmSockAddrInToIPAddr(const __pmSockAddrIn *inaddr)
{
#ifdef HAVE_NSS
    return __pmInAddrToIPAddr(inaddr);
#else
    return __pmInAddrToIPAddr(&inaddr->sin_addr);
#endif
}

const __pmIPAddr
__pmInAddrToIPAddr(const __pmInAddr *inaddr)
{
#ifdef HAVE_NSS
    return inaddr->inet.ip;
#else
    return inaddr->s_addr;
#endif
}

int
__pmIPAddrToInt(const __pmIPAddr *addr)
{
    return *addr;
}

/* Convert an address in network byte order to a string. The caller must free the buffer. */
char *
__pmSockAddrInToString(__pmSockAddrIn *address) {
#ifdef HAVE_NSS
  return __pmInAddrToString(address);
#else
  return __pmInAddrToString(&address->sin_addr);
#endif
}

/* Convert an address in network byte order to a string. The caller must free the buffer. */
char *
__pmInAddrToString(__pmInAddr *address) {
#ifdef HAVE_NSS
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
#else
  char *buf = inet_ntoa(*address);
  if (buf == NULL)
      return NULL;
  return strdup(buf);
#endif
}
