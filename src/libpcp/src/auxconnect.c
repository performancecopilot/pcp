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

/*
 * Network Security Services (NSS) support
 */
#ifdef HAVE_SECURE_SOCKETS
#include <nss.h>
#include <ssl.h>
#include <nspr.h>
#include <private/pprio.h>
#include <sslerr.h>

struct __pmSockAddrIn {
    PRNetAddr		sockaddr;
};
struct __pmInAddr {
    PRNetAddr		inaddr;
};
struct __pmHostEnt {
    PRHostEnt		hostent;
    char		buffer[PR_NETDB_BUF_SIZE];
};
#else /* ! HAVE_SECURE_SOCKETS */
struct __pmSockAddrIn {
    struct sockaddr_in	sockaddr;
};
struct __pmInAddr {
    struct in_addr	inaddr;
};
struct __pmHostEnt {
    struct hostent	hostent;
};
#endif

/* default connect timeout is 5 seconds */
static struct timeval	canwait = { 5, 000000 };

struct __pmHostEnt *
__pmAllocHostEnt(void)
{
    return malloc(sizeof(struct __pmHostEnt));
}

void
__pmFreeHostEnt(struct __pmHostEnt *hostent)
{
    free(hostent);
}

struct __pmInAddr *
__pmAllocInAddr(void)
{
    return malloc(sizeof(struct __pmInAddr));
}

void
__pmFreeInAddr(struct __pmInAddr *inaddr)
{
    free(inaddr);
}

struct __pmSockAddrIn *
__pmAllocSockAddrIn(void)
{
    return malloc(sizeof(struct __pmSockAddrIn));
}

size_t
__pmSockAddrInSize(void)
{
    return sizeof(struct __pmSockAddrIn);
}

void
__pmFreeSockAddrIn(struct __pmSockAddrIn *sockaddr)
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
__pmConnectTo(int fd, const struct __pmSockAddrIn *addr, int port)
{
    int sts, fdFlags = __pmGetFileStatusFlags(fd);
    struct __pmSockAddrIn myAddr;

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
    struct __pmSockAddrIn myAddr;
    struct __pmHostEnt	*servInfo;
    int			fd;	/* Fd for socket connection to pmcd */
    int			sts;
    int			fdFlags;

    if ((servInfo = __pmAllocHostEnt()) == NULL)
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
	__pmFreeHostEnt(servInfo);
	PM_UNLOCK(__pmLock_libpcp);
	return -EHOSTUNREACH;
    }

    __pmConnectTimeout();

    if ((fd = __pmCreateSocket()) < 0) {
	__pmFreeHostEnt(servInfo);
	PM_UNLOCK(__pmLock_libpcp);
	return fd;
    }

    __pmInitSockAddr(&myAddr, htonl(INADDR_ANY), 0);
    __pmSetSockAddr(&myAddr, servInfo);
    __pmFreeHostEnt(servInfo);
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

int
__pmSocketClosed(void)
{
    int	error = oserror();

    if (PM_ERR_NYI > -error)
	error = -(error + PM_ERR_NYI);

    switch (error) {
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
#ifdef HAVE_SECURE_SOCKETS
	case PR_IO_TIMEOUT_ERROR:
	case PR_NETWORK_UNREACHABLE_ERROR:
	case PR_CONNECT_TIMEOUT_ERROR:
	case PR_NOT_CONNECTED_ERROR:
	case PR_CONNECT_RESET_ERROR:
	case PR_PIPE_ERROR:
	case PR_NETWORK_DOWN_ERROR:
	case PR_SOCKET_SHUTDOWN_ERROR:
	case PR_HOST_UNREACHABLE_ERROR:
#endif
	    return 1;
    }
    return 0;
}
#if !defined(HAVE_SECURE_SOCKETS)

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
__pmShutdownSockets(void)
{
    return 0;
}

int
__pmDataIPCSize(void)
{
    return 0;
}

int
__pmSecureServerIPCFlags(int fd)
{
    (void)fd;
    return -EOPNOTSUPP;
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
__pmInitSockAddr(struct __pmSockAddrIn *addr, int address, int port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sockaddr.sin_family = AF_INET;
    addr->sockaddr.sin_addr.s_addr = address;
    addr->sockaddr.sin_port = port;
}

void
__pmSetSockAddr(struct __pmSockAddrIn *addr, struct __pmHostEnt *he)
{
    memcpy(&addr->sockaddr.sin_addr, he->hostent.h_addr, he->hostent.h_length);
}

void
__pmSetPort(struct __pmSockAddrIn *addr, int port)
{
    addr->sockaddr.sin_port = htons(port);
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
__pmSecureDataPending(int fd)
{
    (void)fd;
    return 0;
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

char *
__pmHostEntName(const struct __pmHostEnt *hostEntry)
{
    return hostEntry->hostent.h_name;
}

struct __pmHostEnt *
__pmGetHostByName(const char *hostName, struct __pmHostEnt *hostEntry)
{
    struct hostent *he = gethostbyname(hostName);

    if (he == NULL)
	return NULL;
    memcpy(&hostEntry->hostent, he, sizeof(*he));
    return hostEntry;
}

struct __pmHostEnt *
__pmGetHostByAddr(struct __pmSockAddrIn *address, struct __pmHostEnt *hostEntry)
{
    struct hostent *he = gethostbyaddr((void *)&address->sockaddr.sin_addr.s_addr,
					sizeof(address->sockaddr.sin_addr.s_addr), AF_INET);
    if (he == NULL)
	return NULL;
    memcpy(&hostEntry->hostent, he, sizeof(*he));
    return hostEntry;
}

__pmIPAddr
__pmHostEntGetIPAddr(const struct __pmHostEnt *he, int ix)
{
    return ((struct in_addr *)he->hostent.h_addr_list[ix])->s_addr;
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
__pmSockAddrInToIPAddr(const struct __pmSockAddrIn *inaddr)
{
    return inaddr->sockaddr.sin_addr.s_addr;
}

__pmIPAddr
__pmInAddrToIPAddr(const struct __pmInAddr *inaddr)
{
    return inaddr->inaddr.s_addr;
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
__pmInAddrToString(struct __pmInAddr *address)
{
    char *buf = inet_ntoa(address->inaddr);

    if (buf == NULL)
	return NULL;
    return strdup(buf);
}

int
__pmStringToInAddr(const char *cp, struct __pmInAddr *inp)
{
#ifdef IS_MINGW
    unsigned long in;
    in = inet_addr(buf);
    inaddr.s_addr = in;
    return in == INADDR_NONE ? 0 : 1;
#else
    return inet_aton(cp, &inp->inaddr);
#endif
}

char *
__pmSockAddrInToString(struct __pmSockAddrIn *address)
{
    return __pmInAddrToString((struct __pmInAddr *)&address->sockaddr.sin_addr);
}

#else	/* HAVE_SECURE_SOCKS */
/*
 * We shift NSS/SSL errors below the valid range for PCP error codes,
 * in order to avoid conflicts.  pmErrStr can then detect and decode.
 * PM_ERR_NYI is the PCP error code sentinel.
 */
#define ENCODE_SECURE_SOCKETS_ERROR()	(PM_ERR_NYI + PR_GetError())  /* negative */
static int
__pmSecureSocketsError()
{
    int code = ENCODE_SECURE_SOCKETS_ERROR();
    setoserror(-code);
    return code;
}

/*
 * For every connection when operating under secure socket mode, we need the following
 * auxillary structure associated with the socket.  It holds critical information that
 * each piece of the security pie can make use of (NSS/SSL/NSPR).  Allocated once when
 * initial connection is being established.
 */
typedef struct { 
    PRFileDesc	*nsprFd;
    PRFileDesc	*sslFd;
} __pmSecureSocket;

int
__pmDataIPCSize(void)
{
    return sizeof(__pmSecureSocket);
}

/*
 * NSS/NSPR file descriptors are not integers, however, integral file descriptors are expected
 * in many parts of pcp. In order to deal with this assumption, when NSS/NSPR is available, we
 * maintain a set of available integral file descriptors. The file descriptor number returned by
 * __pmCreateSocket is a reference to this set and must be used for all further I/O operations
 * on that socket.
 *
 * Since some interfaces (e.g. the IPC table) will use a mix of native file descriptors
 * and NSPR ones, we need a way to distinguish them. Obtaining the hard max fd number using
 * getrlimit() was considered, but a sysadmin could change this limit arbitrarily while we are
 * running. We can't use negative values, since these indicate an error.
 *
 * There is a limit on the range of fd's which can be passed to the fd_set API. It is FD_SETSIZE.
 * So, consider all fd's >= FD_SETSIZE to be ones which reference our set. Using this threshold will
 * also allow us to easily manage mixed sets of native and NSPR fds.
 *
 * NB: __pmLock_libpcp must be held when accessing this set, since another thread could modify it
 *     at any time.
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
__pmInitSecureSockets(void)
{
    /* Make sure that NSPR has been initialized */
    if (PR_Initialized() != PR_TRUE)
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    return 0;
}

int
__pmShutdownSecureSockets(void)
{
    if (PR_Initialized())
	PR_Cleanup();
    return 0;
}

int
__pmCreateSocket(void)
{
    int fd, sts;
    __pmSecureSocket socket = { 0 };

    __pmInitSecureSockets();

    /* Open the socket, setup auxillary structs */
    if ((socket.nsprFd = PR_OpenTCPSocket(PR_AF_INET)) == NULL)
	return -neterror();
    fd = newNSPRHandle();
    /* Must set data before __pmInitSocket call */
    __pmSetDataIPC(fd, (void *)&socket);

    if ((sts = __pmInitSocket(fd)) < 0)
        return sts;
    return fd;
}

void
__pmCloseSocket(int fd)
{
    __pmSecureSocket socket;
    int sts;

    sts = __pmDataIPC(fd, (void *)&socket);
    __pmResetIPC(fd);

    if (sts == 0) {
	if (socket.nsprFd) {
	    freeNSPRHandle(fd);
	    PR_Close(socket.nsprFd);
	}
    } else {
#if defined(IS_MINGW)
	closesocket(fd);
#else
	close(fd);
#endif
    }
}

static int
__pmInitCertificateDB(const char *path)
{
    SECStatus secsts;

    if (access(path, R_OK | X_OK) != 0)
	return -ENOENT;
    secsts = NSS_Init(path);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError();
    secsts = NSS_SetExportPolicy();
    if (secsts != SECSuccess)
	return __pmSecureSocketsError();
    SSL_ClearSessionCache();
    return 0;
}

int
__pmInitCertificates(void)
{
    int sts, sep = __pmPathSeparator();
    char dbpath[MAXPATHLEN];
    char *prefix;

    /*
     * Check for client certificate databases, in the following order:
     *    $HOME/.pcp/ssl  
     *    $PCP_VAR_DIR/config/ssl/monitor
     */

    prefix = getenv("HOME");
    if (prefix) {
	snprintf(dbpath, sizeof(dbpath), "%s%c" ".pcp%cssl",
		prefix, sep, sep);
	if ((sts = __pmInitCertificateDB(dbpath)) != -ENOENT)
	    return sts;
	/* Only continue on if no such database path existed */
    }

    snprintf(dbpath, sizeof(dbpath), "%s%c" "config%cssl%cmonitor",
		pmGetConfig("PCP_VAR_DIR"), sep, sep, sep);
    if ((sts = __pmInitCertificateDB(dbpath)) != -ENOENT)
	return sts;

    /* We're still good even if no certificate database paths existed */
    return 0;
}

int
__pmShutdownCertificates(void)
{
    if (NSS_Shutdown() != SECSuccess)
	return __pmSecureSocketsError();
    return 0;
}

static SECStatus
badCert(void *arg, PRFileDesc *sslsocket)
{
    SECItem secitem = { 0 };
    SECStatus secstatus = SECFailure;
    PRArenaPool *arena = NULL;
    CERTCertificate *servercert = NULL;
    char *expected;

    (void)arg;
    switch (PR_GetError()) {
    case SSL_ERROR_BAD_CERT_DOMAIN:
	/*
	 * Propogate a warning through to the client.  Show the expected
	 * host, then list the DNS names from the server certificate.
	 */
	expected = SSL_RevealURL(sslsocket);
	pmprintf("WARNING: "
"The domain name %s does not match the DNS name(s) on the server certificate:\n",
		expected);
	PORT_Free(expected);

	servercert = SSL_PeerCertificate(sslsocket);
	secstatus = CERT_FindCertExtension(servercert,
				SEC_OID_X509_SUBJECT_ALT_NAME, &secitem);
	if (secstatus != SECSuccess || !secitem.data) {
	    pmprintf("Unable to find alt name extension on the server certificate\n");
	} else if ((arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE)) == NULL) {
	    pmprintf("Out of memory while generating name list\n");
	    SECITEM_FreeItem(&secitem, PR_FALSE);
	} else {
	    CERTGeneralName *namelist, *n;

	    namelist = n = CERT_DecodeAltNameExtension(arena, &secitem);
	    SECITEM_FreeItem(&secitem, PR_FALSE);
	    if (!namelist) {
		pmprintf("Unable to decode alt name extension on server certificate\n");
	    } else {
		do {
		    if (n->type == certDNSName)
			pmprintf("  %.*s\n", (int)n->name.other.len, n->name.other.data);
		    n = CERT_GetNextGeneralName(n);
		} while (n != namelist);
	    }
	}
	if (arena)
	    PORT_FreeArena(arena, PR_FALSE);
	if (servercert)
	    CERT_DestroyCertificate(servercert);
	secstatus = SECSuccess;
	pmflush();
	break;
    default:
	break;
    }
    return secstatus;
}

static int
__pmSecureClientIPCFlags(int fd, int flags, const char *hostname)
{
    __pmSecureSocket socket;
    SECStatus secsts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;
    if (socket.nsprFd == NULL)
	return -EOPNOTSUPP;

    if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL) {
	__pmNotifyErr(LOG_ERR, "SecureClientIPCFlags: importing socket into SSL");
	return PM_ERR_IPC;
    }
    socket.nsprFd = socket.sslFd;	/* TODO: remove seperate sslFd? */

    if ((flags & PDU_FLAG_SECURE) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_SECURITY, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_OptionSet(socket.sslFd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_SetURL(socket.sslFd, hostname);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_BadCertHook(socket.sslFd, (SSLBadCertHandler)badCert, NULL);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
    }

    if ((flags & PDU_FLAG_COMPRESS) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
    }

    /* save changes back into the IPC table (updates client sslFd) */
    return __pmSetDataIPC(fd, (void *)&socket);
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname)
{
    PRIntervalTime timer;
    PRFileDesc *sslsocket;
    SECStatus secsts;
    int msec, sts;

    if ((sts = __pmSecureClientIPCFlags(fd, flags, hostname)) < 0)
	return sts;

    sslsocket = (PRFileDesc *)__pmGetSecureSocket(fd);
    if (!sslsocket)
	return -EINVAL;

    secsts = SSL_ResetHandshake(sslsocket, PR_FALSE /*client*/);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError();

    msec = __pmConvertTimeout(TIMEOUT_DEFAULT);
    timer = PR_MillisecondsToInterval(msec);
    secsts = SSL_ForceHandshakeWithTimeout(sslsocket, timer);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError();

    return 0;
}

void *
__pmGetSecureSocket(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) < 0)
	return NULL;
    return (void *)socket.sslFd;
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    __pmSecureSocket socket;
    SECStatus secsts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;
    if (socket.nsprFd == NULL)
	return -EOPNOTSUPP;

    if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL)
	return __pmSecureSocketsError();
    socket.nsprFd = socket.sslFd;	/* TODO: remove seperate sslFd? */

    secsts = SSL_OptionSet(socket.sslFd, SSL_NO_LOCKS, PR_TRUE);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError();

    if ((flags & PDU_FLAG_SECURE) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_SECURITY, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_OptionSet(socket.sslFd, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUEST_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUIRE_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
    }

    if ((flags & PDU_FLAG_COMPRESS) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError();
    }

    /* save changes back into the IPC table (updates server sslFd) */
    return __pmSetDataIPC(fd, (void *)&socket);
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
__pmSetSockOpt(int fd, int level, int option_name, const void *option_value,
	       __pmSockLen option_len)
{
    /* Map the request to the NSPR equivalent, if possible. */
    PRSocketOptionData option_data;
    PRStatus prsts;

    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	switch(level) {
	case SOL_SOCKET:
	    switch(option_name) {
#ifdef IS_MINGW
	    case SO_EXCLUSIVEADDRUSE: {
		/* There is no direct mapping of this option in NSPR. The best we can do is to use
		   the native handle and call setsockopt on that handle. */
	        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
		return setsockopt(fd, level, option_name, option_value, option_len);
	    }
#endif
	    case SO_KEEPALIVE:
	        option_data.option = PR_SockOpt_Keepalive;
		option_data.value.keep_alive = sockOptValue(option_value, option_len);
		break;
	    case SO_LINGER: {
	        struct linger *linger = (struct linger *)option_value;
		option_data.option = PR_SockOpt_Linger;
		option_data.value.linger.polarity = linger->l_onoff;
		option_data.value.linger.linger = linger->l_linger;
		break;
	    }
	    case SO_REUSEADDR:
	        option_data.option = PR_SockOpt_Reuseaddr;
		option_data.value.reuse_addr = sockOptValue(option_value, option_len);
		break;
	    default:
	        __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			      option_name);
		return -1;
	    }
	    break;
	case IPPROTO_TCP:
	    if (option_name == TCP_NODELAY) {
	        option_data.option = PR_SockOpt_NoDelay;
		option_data.value.no_delay = sockOptValue(option_value, option_len);
		break;
	    }
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for IPPROTO_TCP: %d\n",
			  option_name);
	    return -1;
	default:
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented level: %d\n", level);
	    return -1;
	}

	prsts = PR_SetSocketOption(socket.nsprFd, &option_data);
	return prsts == PR_SUCCESS ? 0 : -1;
    }

    /* We have a native socket. */
    return setsockopt(fd, level, option_name, option_value, option_len);
}

int
__pmGetSockOpt(int fd, int level, int option_name, void *option_value,
	       __pmSockLen *option_len)
{
    __pmSecureSocket socket;

    /* Map the request to the NSPR equivalent, if possible. */
    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	switch (level) {
	case SOL_SOCKET:
	    switch(option_name) {
	    case SO_ERROR: {
		/*
		 * There is no direct mapping of this option in NSPR.
		 * Best we can do is call getsockopt on the native fd.
		 */
	      fd = PR_FileDesc2NativeHandle(socket.nsprFd);
	      return getsockopt(fd, level, option_name, option_value, option_len);
	  }
	  default:
	      __pmNotifyErr(LOG_ERR,
			"__pmGetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
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
    return getsockopt(fd, level, option_name, option_value, option_len);
}
 
void
__pmInitSockAddr(struct __pmSockAddrIn *addr, int address, int port)
{
    /*
     * We expect the address and port number to be on network byte order.
     * PR_InitializeNetAddr expects the port in host byte order.
     * The ip field of __pmSockAddrIn (PRNetAddr) must be in network byte order.
     */
    PRStatus prsts = PR_InitializeNetAddr(PR_IpAddrNull, ntohs(port), &addr->sockaddr);
    if (prsts != PR_SUCCESS)
	__pmNotifyErr(LOG_ERR,
		"__pmInitSockAddr: PR_InitializeNetAddr failure: %d\n", PR_GetError());
    addr->sockaddr.inet.ip = address;
}

void
__pmSetSockAddr(struct __pmSockAddrIn *addr, struct __pmHostEnt *he)
{
    PRUint16 port = 0;
    /*
     * The port in the address is in network byte forder, but PR_EnumerateHostEnt
     * expects it in host byte order.
     */
    if (addr->sockaddr.raw.family == PR_AF_INET)
        port = ntohs(addr->sockaddr.inet.port);
    else if (addr->sockaddr.raw.family == PR_AF_INET6)
        port = ntohs(addr->sockaddr.ipv6.port);
    PR_EnumerateHostEnt(0, &he->hostent, port, &addr->sockaddr);
}

void
__pmSetPort(struct __pmSockAddrIn *addr, int port)
{
    if (addr->sockaddr.raw.family == PR_AF_INET)
        addr->sockaddr.inet.port = htons(port);
    else if (addr->sockaddr.raw.family == PR_AF_INET6)
        addr->sockaddr.ipv6.port = htons(port);
}

int
__pmListen(int fd, int backlog)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
        return PR_Listen(socket.nsprFd, backlog) == PR_SUCCESS ? 0 : -1;
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        PRFileDesc *newSocket;

	newSocket = PR_Accept(socket.nsprFd, addr, PR_INTERVAL_NO_TIMEOUT);
	if (newSocket == NULL)
	    return -1;
	/* Add the accepted socket to the fd table. */
	fd = newNSPRHandle();
	socket.nsprFd = newSocket;
	__pmSetDataIPC(fd, (void *)&socket);
	return fd;
    }
    return accept(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
        return PR_Bind(socket.nsprFd, (PRNetAddr *)addr) == PR_SUCCESS ? 0 : -1;
    return bind(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
	return (PR_Connect(socket.nsprFd, (PRNetAddr *)addr, PR_INTERVAL_NO_TIMEOUT)
		== PR_SUCCESS) ? 0 : -1;
    return connect(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmGetFileStatusFlags(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_GETFL);
}

int
__pmSetFileStatusFlags(int fd, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_SETFL, flags);
}

int
__pmGetFileDescriptorFlags(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_GETFD);
}

int
__pmSetFileDescriptorFlags(int fd, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_SETFD, flags);
}

ssize_t
__pmWrite(int fd, const void *buffer, size_t length)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t	size = PR_Write(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError();
	return size;
    }
    return write(fd, buffer, length);
}

ssize_t
__pmRead(int fd, void *buffer, size_t length)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t size = PR_Read(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError();
	return size;
    }
    return read(fd, buffer, length);
}

ssize_t
__pmSend(int fd, const void *buffer, size_t length, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t size = PR_Write(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError();
	return size;
    }
    return send(fd, buffer, length, flags);
}

ssize_t
__pmRecv(int fd, void *buffer, size_t length, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t	size = PR_Read(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError();
	return size;
    }
    return recv(fd, buffer, length, flags);
}

int
__pmSecureDataPending(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.sslFd)
        return SSL_DataPending(socket.sslFd);
    return 0;
}

int
__pmFD(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
        return PR_FileDesc2NativeHandle(socket.nsprFd);
    return fd;
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
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
    } else {
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
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	return FD_ISSET(fd, &set->nspr_set);
    }
    return FD_ISSET(fd, &set->native_set);
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	FD_SET(fd, &set->nspr_set);
	/* Reset the max fd, if necessary. */
	if (fd >= set->num_nspr_fds)
	    set->num_nspr_fds = fd + 1;
    } else {
	FD_SET(fd, &set->native_set);
	/* Reset the max fd, if necessary. */
	if (fd >= set->num_native_fds)
	    set->num_native_fds = fd + 1;
    }
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
    fd_set	combined;
    int		numCombined;
    int		fd;
    int		nativeFD;
    __pmSecureSocket socket;
    int		ready;
    char	errmsg[PM_MAXERRMSGLEN];

    /* fds contains two sets; one of native file descriptors and one of NSPR file
       descriptors. We can't poll them separately, since one may block the other.
       We can either convert the native file descriptors to NSPR or vice-versa.
       The NSPR function PR_Poll does not seem to respond to SIGINT, so we will
       convert the NSPR file descriptors to native ones and use select(3) to
       do the polling.

       First initialize our working set from the set of native file descriptors in
       fds.
    */
    combined = fds->native_set;
    numCombined = fds->num_native_fds;

    /* Now add the native fds associated with the NSPR fds in nspr_set, if any. */
    for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
        if (FD_ISSET(fd, &fds->nspr_set)) {
	    __pmDataIPC(NSPR_HANDLE_BASE + fd, &socket);
	    nativeFD = PR_FileDesc2NativeHandle(socket.nsprFd);
	    FD_SET(nativeFD, &combined);
	    if (nativeFD >= numCombined)
		numCombined = nativeFD + 1;
	}
    }

    /* Use the select(3) function to do the polling. Ignore the nfds passed to us
       and use the number that we have computed. */
    if (rwflag == PR_POLL_READ)
        ready = select(numCombined, &combined, NULL, NULL, timeout);
    else
        ready = select(numCombined, NULL, &combined, NULL, timeout);
    if (ready < 0 && neterror() != EINTR) {
        __pmNotifyErr(LOG_ERR, "nsprSelect: error polling file descriptors: %s\n",
		      netstrerror_r(errmsg, sizeof(errmsg)));
	return -1;
    }

    /* Separate the results into their corresponding sets again. */
    for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
        if (FD_ISSET(fd, &fds->nspr_set)) {
	   __pmDataIPC(NSPR_HANDLE_BASE + fd, &socket);
	   nativeFD = PR_FileDesc2NativeHandle(socket.nsprFd);

	   /* As we copy the result to the nspr set, make sure the bit is cleared in the
	      combined set. That way we can simply copy the resulting combined set to the
	      native set when we're done. */
	   if (! FD_ISSET(nativeFD, &combined))
	       FD_CLR(fd, &fds->nspr_set);
	   else
	       FD_CLR(nativeFD, &combined);
	}
    }
    fds->native_set = combined;

    /* Reset the size of each set. */
    while (fds->num_nspr_fds > 0 && ! FD_ISSET(fds->num_nspr_fds - 1, &fds->nspr_set))
	--fds->num_nspr_fds;
    while (fds->num_native_fds > 0 && ! FD_ISSET(fds->num_native_fds - 1, &fds->native_set))
	--fds->num_native_fds;

    /* Return the total number of ready fds. */
    return ready;
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
__pmHostEntName(const struct __pmHostEnt *he)
{
    return he->hostent.h_name;
}

struct __pmHostEnt *
__pmGetHostByName(const char *hostName, struct __pmHostEnt *he)
{
    PRStatus prStatus = PR_GetHostByName(hostName, &he->buffer[0],
					 PR_NETDB_BUF_SIZE, &he->hostent);
    return prStatus == PR_SUCCESS ? he : NULL;
}

struct __pmHostEnt *
__pmGetHostByAddr(struct __pmSockAddrIn *address, struct __pmHostEnt *he)
{
    PRStatus prStatus = PR_GetHostByAddr(&address->sockaddr, &he->buffer[0],
					 PR_NETDB_BUF_SIZE, &he->hostent);
    return prStatus == PR_SUCCESS ? he : NULL;
}

__pmIPAddr
__pmHostEntGetIPAddr(const struct __pmHostEnt *he, int ix)
{
    PRNetAddr address;
    PRIntn rc = PR_EnumerateHostEnt(0, &he->hostent, 0, &address);
    if (rc < 0) {
	__pmNotifyErr(LOG_ERR, "__pmHostEntGetIPAddr: unable to obtain host address\n");
	return 0;
    }
    return address.inet.ip;
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
    return *addr == htonl(PR_INADDR_LOOPBACK);
}

__pmIPAddr
__pmLoopbackAddress(void)
{
    return htonl(PR_INADDR_LOOPBACK);
}

__pmIPAddr
__pmSockAddrInToIPAddr(const struct __pmSockAddrIn *inaddr)
{
    return inaddr->sockaddr.inet.ip;
}

__pmIPAddr
__pmInAddrToIPAddr(const struct __pmInAddr *inaddr)
{
    return inaddr->inaddr.inet.ip;
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
__pmInAddrToString(struct __pmInAddr *address)
{
    PRStatus	prStatus;
    char	*buf = malloc(PM_NET_ADDR_STRING_SIZE);

    if (buf) {
	prStatus = PR_NetAddrToString(&address->inaddr, buf, PM_NET_ADDR_STRING_SIZE);
	if (prStatus != PR_SUCCESS) {
	    free(buf);
	    return NULL;
	}
    }
    return buf;
}

int
__pmStringToInAddr(const char *cp, struct __pmInAddr *inp)
{
    PRStatus prStatus = PR_StringToNetAddr(cp, &inp->inaddr);
    return (prStatus == PR_SUCCESS);
}

char *
__pmSockAddrInToString(struct __pmSockAddrIn *address)
{
    return __pmInAddrToString((struct __pmInAddr *)address);
}

#endif	/* HAVE_SECURE_SOCKETS */
