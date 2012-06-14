/*
 * Copyright (c) 1995,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 */
typedef struct {
    __pmFD      fd;
    int		version;	/* one or two */
    int		socket;		/* true or false */
} __pmIPC;

static __pmFD	__pmLastUsedFd = PM_ERROR_FD;
static __pmIPC	*__pmIPCTablePtr;
static int	ipctablesize;

static int
__pmFindFdIPC(__pmFD fd)
{
  int i;
  for (i = 0; i < ipctablesize; ++i) {
    if (__pmIPCTablePtr[i].fd == fd)
      return i;
  }
  return -1; /* not found */
}

/*
 * always called with __pmLock_libpcp held
 */
static int
__pmResizeIPC(__pmFD fd)
{
    int	oldsize;
    int ix;

    if ((ix = __pmFindFdIPC(fd)) < 0) {
	oldsize = ipctablesize;
	if (ipctablesize == 0)
	    ipctablesize = 4;
	else
	    ipctablesize *= 2;

	if ((__pmIPCTablePtr = (__pmIPC *)realloc(__pmIPCTablePtr,
				sizeof(__pmIPC)*ipctablesize)) == NULL)
	    return -oserror();
	memset((__pmIPCTablePtr+oldsize), 0, sizeof(__pmIPC)*(ipctablesize-oldsize));
	__pmIPCTablePtr[oldsize].fd = fd;
	ix = oldsize;
    }
    return ix;
}

int
__pmSetVersionIPC(__pmFD fd, int version)
{
    int sts;
    int ix;

    if (pmDebug & DBG_TRACE_CONTEXT)
        fprintf(stderr, "__pmSetVersionIPC: fd=%d version=%d\n", __pmFdRef(fd), version);

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

    ix = sts;
    sts = 0;
    __pmIPCTablePtr[ix].version = version;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmSetSocketIPC(__pmFD fd)
{
    int sts;
    int ix;

    if (pmDebug & DBG_TRACE_CONTEXT)
        fprintf(stderr, "__pmSetSocketIPC: fd=%d\n", __pmFdRef(fd));

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

    ix = sts;
    sts = 0;
    __pmIPCTablePtr[ix].socket = 1;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

int
__pmVersionIPC(__pmFD fd)
{
    int		sts;
    int		ix;

    if (fd == PDU_OVERRIDE2)
	return PDU_VERSION2;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((ix = __pmFindFdIPC(fd)) < 0) {
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr,
		"IPC protocol botch: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTablePtr, __pmFdRef(fd), ipctablesize);
	PM_UNLOCK(__pmLock_libpcp);
	return UNKNOWN_VERSION;
    }
    sts = __pmIPCTablePtr[ix].version;

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
__pmSocketIPC(__pmFD fd)
{
    int		sts;
    int		ix;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((ix = __pmFindFdIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return 0;
    }
    sts = __pmIPCTablePtr[ix].socket;

    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

/*
 * Called by log readers who need version info for result decode,
 * but don't have a socket fd (have a FILE* & fileno though).
 * Also at start of version exchange before version is known
 * (when __pmDecodeError is called before knowing version).
 */
void
__pmOverrideLastFd(__pmFD fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    __pmLastUsedFd = fd;
    PM_UNLOCK(__pmLock_libpcp);
}

void
__pmResetIPC(__pmFD fd)
{
    int ix;
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if ((ix = __pmFindFdIPC(fd)) < 0) {
	PM_UNLOCK(__pmLock_libpcp);
	return;
    }
    /* Clear the table entry but retain the fd for searching purposes. */
    memset(&__pmIPCTablePtr[ix], 0, sizeof(__pmIPC));
    __pmIPCTablePtr[ix].fd = fd;
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
