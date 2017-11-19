/*
 * Service routines for elevated privilege services (pmdaroot).
 *
 * Copyright (c) 2014-2015 Red Hat.
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
#include "pmda.h"
#include "pmdaroot.h"
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

/*
 * Connect to the pmdaroot socket as a client, perform version exchange
 */
int
pmdaRootConnect(const char *path)
{
    __pmSockAddr	*addr;
    char		*tmpdir;
    char		socketpath[MAXPATHLEN];
    char		errmsg[PM_MAXERRMSGLEN];
    int			fd, sts, version, features;

    /* Initialize the socket address. */
    if ((addr = __pmSockAddrAlloc()) == NULL)
	return -ENOMEM;

    if (path == NULL) {
	if ((tmpdir = pmGetOptionalConfig("PCP_TMP_DIR")) == NULL) {
	    __pmSockAddrFree(addr);
	    return PM_ERR_GENERIC;
	}
	pmsprintf(socketpath, sizeof(socketpath),
			"%s/pmcd/root.socket", tmpdir);
    } else {
	strncpy(socketpath, path, sizeof(socketpath));
	socketpath[sizeof(socketpath)-1] = '\0';
    }

    __pmSockAddrSetFamily(addr, AF_UNIX);
    __pmSockAddrSetPath(addr, socketpath);

    /* Create client socket connection */
    if ((fd = __pmCreateUnixSocket()) < 0) {
	pmNotifyErr(LOG_ERR, "pmdaRootConnect: cannot create socket %s: %s\n",
			socketpath, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmSockAddrFree(addr);
	return fd;
    }

    sts = __pmConnect(fd, addr, -1);
    __pmSockAddrFree(addr);
    if (sts < 0) {
	if (sts != -EPERM || pmDebugOptions.libpmda)
	    pmNotifyErr(LOG_INFO,
			"pmdaRootConnect: cannot connect to %s: %s\n",
			socketpath, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    /* Check server connection information */
    if ((sts = __pmdaRecvRootPDUInfo(fd, &version, &features)) < 0) {
	pmNotifyErr(LOG_ERR,
			"pmdaRootConnect: cannot verify %s server: %s\n",
			socketpath, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    if (pmDebugOptions.libpmda)
	pmNotifyErr(LOG_INFO,
		"pmdaRootConnect: %s server fd=%d version=%d features=0x%x\n",
			socketpath, fd, version, features);
    return fd;
}

void
pmdaRootShutdown(int clientfd)
{
    if (clientfd >= 0)
	shutdown(clientfd, SHUT_RDWR);
}

static int
container_access(int clientfd, const char *name, int *namelen, int *pid)
{
    if (clientfd < 0)
	return -ENOTCONN;
    if (name) {
	*pid = 0;
    } else {
	/* direct by-PID access mechanism for QA testing */
	*pid = *namelen;
	namelen = 0;
    }
    return 0;
}

int
pmdaRootContainerHostName(int clientfd, const char *name, int namelen,
			char *buffer, int buflen)
{
    char pdubuf[sizeof(__pmdaRootPDUContainer) + MAXPATHLEN];
    int	sts, pid;

    if ((sts = container_access(clientfd, name, &namelen, &pid)) < 0)
	return sts;
    if ((sts = __pmdaSendRootPDUContainer(clientfd, PDUROOT_HOSTNAME_REQ,
			pid, name, namelen, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUContainer(clientfd, PDUROOT_HOSTNAME,
			pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    return __pmdaDecodeRootPDUContainer(pdubuf, sts, &pid, buffer, buflen);
}

int
pmdaRootContainerProcessID(int clientfd, const char *name, int namelen)
{
    char pdubuf[sizeof(__pmdaRootPDUContainer)];
    int sts, pid;

    if ((sts = container_access(clientfd, name, &namelen, &pid)) < 0)
	return sts;
    if ((sts = __pmdaSendRootPDUContainer(clientfd, PDUROOT_PROCESSID_REQ,
			pid, name, namelen, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUContainer(clientfd, PDUROOT_PROCESSID,
			pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    if ((sts = __pmdaDecodeRootPDUContainer(pdubuf, sts, &pid, NULL, 0)) < 0)
	return sts;
    return pid;
}

int
pmdaRootContainerCGroupName(int clientfd, const char *name, int namelen,
			char *buffer, int buflen)
{
    char pdubuf[sizeof(__pmdaRootPDUContainer) + MAXPATHLEN];
    int	sts, pid;

    if ((sts = container_access(clientfd, name, &namelen, &pid)) < 0)
	return sts;
    if ((sts = __pmdaSendRootPDUContainer(clientfd, PDUROOT_CGROUPNAME_REQ,
			pid, name, namelen, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUContainer(clientfd, PDUROOT_CGROUPNAME,
			pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    return __pmdaDecodeRootPDUContainer(pdubuf, sts, &pid, buffer, buflen);
}

int
pmdaRootProcessStart(int clientfd, int ipctype, const char *name,
			int namelen, const char *args, int argslen,
			int *pid, int *infd, int *outfd)
{
    char pdubuf[sizeof(__pmdaRootPDUStart) + MAXPATHLEN];
    int sts;

    if ((sts = __pmdaSendRootPDUStartReq(clientfd, ipctype, name, namelen,
				args, argslen)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUStart(clientfd, pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    return __pmdaDecodeRootPDUStart(pdubuf, sts, pid, infd, outfd,
				NULL, NULL, 0, NULL, 0);
}

int
pmdaRootProcessWait(int clientfd, int pid, int *status)
{
    char pdubuf[sizeof(__pmdaRootPDUStop)];
    int	sts;

    if ((sts = __pmdaSendRootPDUStop(clientfd, PDUROOT_STOPPMDA_REQ,
			pid, 0, 0, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUStop(clientfd, PDUROOT_STOPPMDA,
			pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    if ((sts = __pmdaDecodeRootPDUStop(pdubuf, sts, &pid, status, NULL)) < 0)
	return sts;
    return pid;
}

int
pmdaRootProcessTerminate(int clientfd, int pid)
{
    char pdubuf[sizeof(__pmdaRootPDUStop)];
    int	sts;

    if ((sts = __pmdaSendRootPDUStop(clientfd, PDUROOT_STOPPMDA_REQ,
			pid, 0, 1, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUStop(clientfd, PDUROOT_STOPPMDA,
			pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    return __pmdaDecodeRootPDUStop(pdubuf, sts, NULL, NULL, NULL);
}
