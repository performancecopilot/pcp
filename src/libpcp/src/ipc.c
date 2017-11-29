/*
 * Copyright (c) 2012-2013 Red Hat.
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
#include "libpcp.h"
#include "internal.h"
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#ifdef HAVE_SYS_CONFIG_H
#include <sys/config.h>
#endif

/*
 * We keep a table of connection state for each interesting file
 * descriptor here.
 * The version field holds the version of the software at the other
 * end of the connection end point (0 is unknown, 1 or 2 are also valid).
 * The socket field is used to tell whether this is a socket or pipe
 * (or a file) connection, which is most important for the Windows
 * port as socket interfaces are special and do not use the usual
 * file descriptor read/write/close calls, but must rather use
 * recv/send/closesocket.
 * The features field holds any feature bits received from the other
 * end of the connection, so we can test for the presense/absence of
 * (remote) features.
 *
 * The table entries are of fixed length, but the actual size depends
 * on compile time options used (in particular, the secure sockets
 * setting requires further space allocated to hold the additional
 * security metadata for each socket).
 */
typedef struct {
    int		version : 8;	/* remote version (v1 or v2, so far) */
    unsigned	socket : 1;	/* true or false */
    int		padding : 7;	/* unused zeroes */
    int		features : 16;	/* remote features (i.e. PDU_FLAG_s) */
    char	data[0];	/* opaque data (optional) */
} __pmIPC;

static int	__pmLastUsedFd = -INT_MAX;
static __pmIPC	*__pmIPCTable;
static int	ipctablecount;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	ipc_lock = PTHREAD_MUTEX_INITIALIZER;
#else
void			*ipc_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == ipc_lock
 */
int
__pmIsIpcLock(void *lock)
{
    return lock == (void *)&ipc_lock;
}
#endif
static int	ipcentrysize;

static inline __pmIPC *
__pmIPCTablePtr(int fd)
{
    char *entry = (char *)__pmIPCTable;
    return (__pmIPC *)(entry + fd * ipcentrysize);
}

static int
resize(int fd)
{
    size_t size;
    int	oldcount;
    __pmIPC	*tmp__pmIPCTable;

    PM_ASSERT_IS_LOCKED(ipc_lock);

    if (__pmIPCTable == NULL || fd >= ipctablecount) {
	if (ipcentrysize == 0)
	    ipcentrysize = sizeof(__pmIPC) + __pmDataIPCSize();
	oldcount = ipctablecount;
	if (ipctablecount == 0)
	    ipctablecount = 4;
	while (fd >= ipctablecount)
	    ipctablecount *= 2;
	size = ipcentrysize * ipctablecount;
	tmp__pmIPCTable = (__pmIPC *)realloc(__pmIPCTable, size);
	if (tmp__pmIPCTable == NULL) {
	    ipctablecount = oldcount;
	    return -oserror();
	}
	__pmIPCTable = tmp__pmIPCTable;
	size -= ipcentrysize * oldcount;
	memset(__pmIPCTablePtr(oldcount), 0, size);
    }
    return 0;
}

static int
version_locked(int fd)
{
    PM_ASSERT_IS_LOCKED(ipc_lock);

    if (fd == PDU_OVERRIDE2)
	return PDU_VERSION2;
    if (__pmIPCTable == NULL || fd < 0 || fd >= ipctablecount) {
	if (pmDebugOptions.context)
	    fprintf(stderr,
		"IPC protocol botch: version: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTable, fd, ipctablecount);
	return UNKNOWN_VERSION;
    }
    return __pmIPCTablePtr(fd)->version;
}

static int
socket_locked(int fd)
{
    PM_ASSERT_IS_LOCKED(ipc_lock);

    if (__pmIPCTable == NULL || fd < 0 || fd >= ipctablecount) {
	if (pmDebugOptions.context)
	    fprintf(stderr,
		"IPC protocol botch: socket: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTable, fd, ipctablecount);
	return 0;
    }
    return __pmIPCTablePtr(fd)->socket;
}

static int
features_locked(int fd)
{
    PM_ASSERT_IS_LOCKED(ipc_lock);

    if (__pmIPCTable == NULL || fd < 0 || fd >= ipctablecount) {
	if (pmDebugOptions.context)
	    fprintf(stderr,
		"IPC protocol botch: features: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTable, fd, ipctablecount);
	return 0;
    }
    return __pmIPCTablePtr(fd)->features;
}

static void
print(void)
{
    int		i;

    PM_ASSERT_IS_LOCKED(ipc_lock);

    fprintf(stderr, "IPC table fd(PDU version):");
    for (i = 0; i < ipctablecount; i++) {
	if (__pmIPCTablePtr(i)->version != UNKNOWN_VERSION)
	    fprintf(stderr, " %d(%d,%d)", i, __pmIPCTablePtr(i)->version,
					     __pmIPCTablePtr(i)->socket);
    }
    fputc('\n', stderr);
}

int
__pmSetVersionIPC(int fd, int version)
{
    int sts;

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSetVersionIPC: fd=%d version=%d\n", fd, version);

    PM_LOCK(ipc_lock);
    if ((sts = resize(fd)) < 0) {
	PM_UNLOCK(ipc_lock);
	return sts;
    }

    __pmIPCTablePtr(fd)->version = version;
    __pmLastUsedFd = fd;

    if (pmDebugOptions.context)
	print();

    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmSetFeaturesIPC(int fd, int version, int features)
{
    __pmIPC	*ipc;
    int		sts;

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSetFeaturesIPC: fd=%d version=%d features=%d\n",
		fd, version, features);

    PM_LOCK(ipc_lock);
    if ((sts = resize(fd)) < 0) {
	PM_UNLOCK(ipc_lock);
	return sts;
    }

    ipc = __pmIPCTablePtr(fd);
    ipc->features = features;
    ipc->version = version;
    __pmLastUsedFd = fd;

    if (pmDebugOptions.context)
	print();

    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmSetSocketIPC(int fd)
{
    int sts;

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSetSocketIPC: fd=%d\n", fd);

    PM_LOCK(ipc_lock);
    if ((sts = resize(fd)) < 0) {
	PM_UNLOCK(ipc_lock);
	return sts;
    }

    __pmIPCTablePtr(fd)->socket = 1;
    __pmLastUsedFd = fd;

    if (pmDebugOptions.context)
	print();

    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmVersionIPC(int fd)
{
    int		sts;

    PM_LOCK(ipc_lock);
    sts = version_locked(fd);
    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmLastVersionIPC()
{
    int		sts;

    PM_LOCK(ipc_lock);
    sts = version_locked(__pmLastUsedFd);
    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmSocketIPC(int fd)
{
    int		sts;

    PM_LOCK(ipc_lock);
    sts = socket_locked(fd);
    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmFeaturesIPC(int fd)
{
    int		sts;

    PM_LOCK(ipc_lock);
    sts = features_locked(fd);
    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmSetDataIPC(int fd, void *data)
{
    char	*dest;
    int		sts;

    PM_LOCK(ipc_lock);
    if ((sts = resize(fd)) < 0) {
	PM_UNLOCK(ipc_lock);
	return sts;
    }

    if (pmDebugOptions.context)
	fprintf(stderr, "__pmSetDataIPC: fd=%d data=%p(sz=%d)\n",
		fd, data, (int)(ipcentrysize - sizeof(__pmIPC)));

    dest = ((char *)__pmIPCTablePtr(fd)) + sizeof(__pmIPC);
    memcpy(dest, data, ipcentrysize - sizeof(__pmIPC));
    __pmLastUsedFd = fd;

    if (pmDebugOptions.context)
	print();

    PM_UNLOCK(ipc_lock);
    return sts;
}

int
__pmDataIPC(int fd, void *data)
{
    char	*source;

    PM_LOCK(ipc_lock);
    if (fd < 0 || fd >= ipctablecount || __pmIPCTable == NULL ||
	ipcentrysize == sizeof(__pmIPC)) {
	PM_UNLOCK(ipc_lock);
	return -ESRCH;
    }
    source = ((char *)__pmIPCTablePtr(fd)) + sizeof(__pmIPC);
    if (pmDebugOptions.context && pmDebugOptions.desperate)
	fprintf(stderr, "__pmDataIPC: fd=%d, data=%p(sz=%d)\n",
		fd, source, (int)(ipcentrysize - sizeof(__pmIPC)));
    memcpy(data, source, ipcentrysize - sizeof(__pmIPC));

    PM_UNLOCK(ipc_lock);
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
    PM_LOCK(ipc_lock);
    __pmLastUsedFd = fd;
    PM_UNLOCK(ipc_lock);
}

void
__pmResetIPC(int fd)
{
    PM_LOCK(ipc_lock);
    if (__pmIPCTable && fd >= 0 && fd < ipctablecount)
	memset(__pmIPCTablePtr(fd), 0, ipcentrysize);
    PM_UNLOCK(ipc_lock);
}

void
__pmPrintIPC(void)
{
    PM_LOCK(ipc_lock);
    print();
    PM_UNLOCK(ipc_lock);
}
