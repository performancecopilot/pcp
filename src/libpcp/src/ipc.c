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
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1500 Crittenden Lane,
 * Mountain View, CA 94043, USA, or: http://www.sgi.com
 */

#include "pmapi.h"
#include "impl.h"
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#ifdef HAVE_SYS_CONFIG_H
#include <sys/config.h>
#endif

#ifndef MAXINT
#ifdef INT_MAX
#define MAXINT INT_MAX
#else
bozo ... need to be able to find MAXINT or equivalent
#endif
#endif

#define PMIPC_TABLE_INIT	4
#define PMIPC_TABLE_BASE	0

int		__pmLastUsedFd   = -MAXINT;
__pmIPC		*__pmIPCTablePtr = NULL;
static int	ipctablesize = 0;

/*
 * This table is used when an explicit version is to be used.
 * A lookup on file descriptor `-PDU_VERSION1' returns 1st entry,
 * similarly with fd `-PDU_VERSION2'.
 */
static __pmIPC	force[] = {
    { PDU_VERSION1, NULL },
    { PDU_VERSION2, NULL }
};

int
__pmAddIPC(int fd, __pmIPC ipc)
{
    int		oldsize;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "__pmAddIPC: fd=%d verion=%d\n", fd, ipc.version);
#endif
    if (__pmIPCTablePtr == NULL || fd >= ipctablesize) {
	oldsize = ipctablesize;
	while (fd >= ipctablesize) {
	    if (ipctablesize == 0) {
		ipctablesize = PMIPC_TABLE_INIT;
	    }
	    else
		ipctablesize *= 2;
	}
	if ((__pmIPCTablePtr = (__pmIPC *)realloc(__pmIPCTablePtr,
		sizeof(__pmIPC)*ipctablesize)) == NULL)
	    return -errno;
	if (oldsize == 0)
	    memset(__pmIPCTablePtr, UNKNOWN_VERSION, sizeof(__pmIPC)*ipctablesize);
	memset((__pmIPCTablePtr+fd), UNKNOWN_VERSION, sizeof(__pmIPC)*(ipctablesize-fd));
    }
    __pmIPCTablePtr[fd] = ipc;
    __pmLastUsedFd = fd;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT)
	__pmPrintIPC();
#endif

    return 0;
}

int
__pmLookupIPC(__pmIPC **ipc)
{
    return __pmFdLookupIPC(__pmLastUsedFd, ipc);
}

int
__pmFdLookupIPC(int fd, __pmIPC **ipc)
{
    if (fd == PDU_OVERRIDE1) {
	*ipc = &force[0];
	return 0;
    }
    else if (fd == PDU_OVERRIDE2) {
	*ipc = &force[1];
	return 0;
    }

    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT)
	    fprintf(stderr, "IPC protocol botch: table->" PRINTF_P_PFX "%p fd=%d sz=%d\n",
		__pmIPCTablePtr, fd, ipctablesize);
#endif
	*ipc = NULL;
	return PM_ERR_IPC;
    }
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmFdLookupIPC: fd=%d PDU version: ", fd);
	if (__pmIPCTablePtr[fd].version == UNKNOWN_VERSION)
	    fprintf(stderr, "?\n");
	else
	    fprintf(stderr, "%d\n", __pmIPCTablePtr[fd].version);
    }
#endif

    *ipc = &__pmIPCTablePtr[fd];
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
    __pmLastUsedFd = fd;
}

void
__pmResetIPC(int fd)
{
    if (__pmIPCTablePtr == NULL || fd < 0 || fd >= ipctablesize)
	return;

    __pmIPCTablePtr[fd].version = UNKNOWN_VERSION;
    __pmIPCTablePtr[fd].ext = NULL;
}

void
__pmPrintIPC(void)
{
    int	i;

    fprintf(stderr, "IPC table fd(PDU version):");
    for (i = 0; i < ipctablesize; i++) {
	if (__pmIPCTablePtr[i].version == UNKNOWN_VERSION)
	    fprintf(stderr, " %d(?)", i);
	else
	    fprintf(stderr, " %d(%d)", i, __pmIPCTablePtr[i].version);
    }
    fputc('\n', stderr);
}
