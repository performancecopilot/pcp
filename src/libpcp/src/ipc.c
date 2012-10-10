/*
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#ifdef HAVE_SYS_CONFIG_H
#include <sys/config.h>
#endif

/*
 * We keep a table of connection state for each interesting file descriptor here.
 * The version field holds the version of the software at the other end of the
 * connection end point (0 is unknown, 1 or 2 are also valid).
 * The socket field is used to tell whether this is a socket or pipe (or a file)
 * connection, which is most important for the Windows port, as socket interfaces
 * are braindead and do not use the usual file descriptor read/write/close calls,
 * but must rather use recv/send/closesocket.
 *
 * If NSS/NSPR is available, then we also keep NSPR file descriptor information
 * here. This allows us to handle NSPR sockets which in turn allows us to support
 * SSL/TLS.
 * 
 * NSPR file descriptors are not integers, however, integral file descriptors are
 * expected in many parts of pcp. In order to deal with this assumption, when
 * NSS/NSPR is available, for sockets using NSPR, we must assign an integral fd
 * number. However this must not collide with the native fd numbers also in use.
 * Obtaining the hard max fd number using getrlimit() was considered, but a
 * sysadmin could change this limit arbitrarily while we are running. We can't use
 * negative values, since these indicate an error.
 *
 * There is a limit on the range of fd's which can be passed to the fd_set API.
 * It is FD_SETSIZE. So, consider all fd's >= FD_SETSIZE to be ones which
 * represent NSPR sockets. Using this threshold will also allow us to easily
 * manage mixed sets of native and indexed fds.
 */
typedef struct {
    int		version;	/* one or two */
    int		socket;		/* true or false */
#if defined(HAVE_NSS)
    PRFileDesc *nsprFD;
#endif
} __pmIPC;

static int	__pmLastUsedFd = -INT_MAX;
static __pmIPC	*__pmIPCTablePtr;
static int	ipctablesize;

/*
 * always called with __pmLock_libpcp held
 */
static int
__pmResizeIPC(int fd)
{
    int	oldsize;

    if (__pmIPCTablePtr == NULL || fd >= ipctablesize) {
	oldsize = ipctablesize;
	while (fd >= ipctablesize) {
	    if (ipctablesize == 0) {
		ipctablesize = 4;
	    }
	    else
		ipctablesize *= 2;
	}
	if ((__pmIPCTablePtr = (__pmIPC *)realloc(__pmIPCTablePtr,
				sizeof(__pmIPC)*ipctablesize)) == NULL)
	    return -oserror();
	memset((__pmIPCTablePtr+oldsize), 0, sizeof(__pmIPC)*(ipctablesize-oldsize));
    }
    return 0;
}

int
__pmSetVersionIPC(int fd, int version)
{
    int sts;

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSetVersionIPC: fd=%d version=%d\n", fd, version);

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

    __pmIPCTablePtr[fd].version = version;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmSetSocketIPC(int fd)
{
    int sts;

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSetSocketIPC: fd=%d\n", fd);

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

    __pmIPCTablePtr[fd].socket = 1;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmVersionIPC(int fd)
{
    int		sts;

    if (fd == PDU_OVERRIDE2)
	return PDU_VERSION2;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize) {
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr,
		"IPC protocol botch: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTablePtr, fd, ipctablesize);
	PM_UNLOCK(__pmLock_libpcp);
	return UNKNOWN_VERSION;
    }
    sts = __pmIPCTablePtr[fd].version;

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmLastVersionIPC()
{
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    sts = __pmVersionIPC(__pmLastUsedFd);
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmSocketIPC(int fd)
{
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize) {
	PM_UNLOCK(__pmLock_libpcp);
	return 0;
    }
    sts = __pmIPCTablePtr[fd].socket;

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

#if defined(HAVE_NSS)
int
__pmSetNSPRFdIPC(int fd, PRFileDesc *nsprFD)
{
    int sts;

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSetSocketIPC: fd=%d\n", fd);

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

    __pmIPCTablePtr[fd].nsprFD = nsprFD;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

PRFileDesc *
__pmNSPRFdIPC(int fd)
{
    PRFileDesc *sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize) {
	PM_UNLOCK(__pmLock_libpcp);
	return 0;
    }
    sts = __pmIPCTablePtr[fd].nsprFD;

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}
#endif /* defined(HAVE_NSS) */

/*
 * Called by log readers who need version info for result decode,
 * but don't have a socket fd (have a FILE* & fileno though).
 * Also at start of version exchange before version is known
 * (when __pmDecodeError is called before knowing version).
 */
void
__pmOverrideLastFd(int fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    __pmLastUsedFd = fd;
    PM_UNLOCK(__pmLock_libpcp);
}

void
__pmResetIPC(int fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize) {
	PM_UNLOCK(__pmLock_libpcp);
	return;
    }
    memset(&__pmIPCTablePtr[fd], 0, sizeof(__pmIPC));
    PM_UNLOCK(__pmLock_libpcp);
}

void
__pmPrintIPC(void)
{
    int	i;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    fprintf(stderr, "IPC table fd(PDU version):");
    for (i = 0; i < ipctablesize; i++) {
	if (__pmIPCTablePtr[i].version != UNKNOWN_VERSION)
	    fprintf(stderr, " %d(%d,%d)", i, __pmIPCTablePtr[i].version,
					     __pmIPCTablePtr[i].socket);
    }
    fputc('\n', stderr);
    PM_UNLOCK(__pmLock_libpcp);
}
