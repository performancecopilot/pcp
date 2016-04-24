/*
 * Copyright (c) 2012-2016 Red Hat.
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
 * are "special" and do not use the usual file descriptor read/write/close calls,
 * but must rather use recv/send/closesocket.
 *
 * The table entries are of fixed length, but the actual size depends on compile
 * time options used (in particular, the secure sockets setting requires further
 * space allocated to hold the additional security metadata for each socket).
 */
typedef struct {
    int		version;	/* one or two */
    int		socket;		/* true or false */
    char	data[0];	/* opaque data (optional) */
} __pmIPC;

static int	__pmLastUsedFd = -INT_MAX;

/* NB: Protected by a static lock instead of __pmLock_libpcp, to
   eliminate this source lock-ordering deadlocks. */
static __pmIPC	*__pmIPCTable;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t __pmIPCTable_lock = PTHREAD_MUTEX_INITIALIZER;
#else
static void* __pmIPCTable_lock;
#endif

static int	ipctablecount;
static int	ipcentrysize;

static inline __pmIPC *
__pmIPCTablePtr(int fd)
{
    char *entry = (char *)__pmIPCTable;
    return (__pmIPC *)(entry + fd * ipcentrysize);
}

/*
 * always called with __pmIPCTable_lock held
 */
static int
__pmResizeIPC(int fd)
{
    size_t size;
    int	oldcount;

    if (__pmIPCTable == NULL || fd >= ipctablecount) {
	if (ipcentrysize == 0)
	    ipcentrysize = sizeof(__pmIPC) + __pmDataIPCSize();
	oldcount = ipctablecount;
	if (ipctablecount == 0)
	    ipctablecount = 4;
	while (fd >= ipctablecount)
	    ipctablecount *= 2;
	size = ipcentrysize * ipctablecount;
	__pmIPCTable = (__pmIPC *)realloc(__pmIPCTable, size);
	if (__pmIPCTable == NULL)
	    return -oserror();
	size -= ipcentrysize * oldcount;
	memset(__pmIPCTablePtr(oldcount), 0, size);
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
    PM_LOCK(__pmIPCTable_lock);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmIPCTable_lock);
	return sts;
    }

    __pmIPCTablePtr(fd)->version = version;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmSetSocketIPC(int fd)
{
    int sts;

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSetSocketIPC: fd=%d\n", fd);

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmIPCTable_lock);
	return sts;
    }

    __pmIPCTablePtr(fd)->socket = 1;
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmVersionIPC(int fd)
{
    int		sts;

    if (fd == PDU_OVERRIDE2)
	return PDU_VERSION2;
    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if (__pmIPCTable == NULL || fd < 0 || fd >= ipctablecount) {
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr,
		"IPC protocol botch: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTable, fd, ipctablecount);
	PM_UNLOCK(__pmIPCTable_lock);
	return UNKNOWN_VERSION;
    }
    sts = __pmIPCTablePtr(fd)->version;

    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmLastVersionIPC()
{
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    sts = __pmVersionIPC(__pmLastUsedFd);
    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmSocketIPC(int fd)
{
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if (__pmIPCTable == NULL || fd < 0 || fd >= ipctablecount) {
	PM_UNLOCK(__pmIPCTable_lock);
	return 0;
    }
    sts = __pmIPCTablePtr(fd)->socket;

    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmSetDataIPC(int fd, void *data)
{
    char	*dest;
    int		sts;

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if ((sts = __pmResizeIPC(fd)) < 0) {
	PM_UNLOCK(__pmIPCTable_lock);
	return sts;
    }

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmSetDataIPC: fd=%d data=%p(sz=%d)\n",
		fd, data, (int)(ipcentrysize - sizeof(__pmIPC)));

    dest = ((char *)__pmIPCTablePtr(fd)) + sizeof(__pmIPC);
    memcpy(dest, data, ipcentrysize - sizeof(__pmIPC));
    __pmLastUsedFd = fd;

    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();

    PM_UNLOCK(__pmIPCTable_lock);
    return sts;
}

int
__pmDataIPC(int fd, void *data)
{
    char	*source;

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if (fd < 0 || fd >= ipctablecount || __pmIPCTable == NULL ||
	ipcentrysize == sizeof(__pmIPC)) {
	PM_UNLOCK(__pmIPCTable_lock);
	return -ESRCH;
    }
    source = ((char *)__pmIPCTablePtr(fd)) + sizeof(__pmIPC);
    if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE))
	fprintf(stderr, "__pmDataIPC: fd=%d, data=%p(sz=%d)\n",
		fd, source, (int)(ipcentrysize - sizeof(__pmIPC)));
    memcpy(data, source, ipcentrysize - sizeof(__pmIPC));

    PM_UNLOCK(__pmIPCTable_lock);
    return 0;
}

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
    PM_LOCK(__pmIPCTable_lock);
    __pmLastUsedFd = fd;
    PM_UNLOCK(__pmIPCTable_lock);
}

void
__pmResetIPC(int fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    if (__pmIPCTable && fd >= 0 && fd < ipctablecount)
	memset(__pmIPCTablePtr(fd), 0, ipcentrysize);
    PM_UNLOCK(__pmIPCTable_lock);
}

void
__pmPrintIPC(void)
{
    int	i;

    PM_INIT_LOCKS();
    PM_LOCK(__pmIPCTable_lock);
    fprintf(stderr, "IPC table fd(PDU version):");
    for (i = 0; i < ipctablecount; i++) {
	if (__pmIPCTablePtr(i)->version != UNKNOWN_VERSION)
	    fprintf(stderr, " %d(%d,%d)", i, __pmIPCTablePtr(i)->version,
					     __pmIPCTablePtr(i)->socket);
    }
    fputc('\n', stderr);
    PM_UNLOCK(__pmIPCTable_lock);
}
