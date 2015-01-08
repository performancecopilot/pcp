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
#include "impl.h"
#include "pmda.h"
#include "pduroot.h"
#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

static int priv_fdset[PMDA_NAMESPACE_COUNT];
static int self_fdset[PMDA_NAMESPACE_COUNT];

/*
 * Connect to the pmdaroot socket as a client, perform version exchange
 */
int
pmdaRootConnect(const char *path)
{
    __pmSockAddr	*addr;
    char		socketpath[MAXPATHLEN];
    char		errmsg[PM_MAXERRMSGLEN];
    int			fd, sts, version, features;

    /* Ensure we start from an initially-invalid fd state */
    memset(self_fdset, -1, sizeof(self_fdset));
    memset(priv_fdset, -1, sizeof(priv_fdset));

    /* Initialize the socket address. */
    if ((addr = __pmSockAddrAlloc()) == NULL)
	return -ENOMEM;

    if (path == NULL)
	snprintf(socketpath, sizeof(socketpath), "%s/pmcd/root.socket",
		pmGetConfig("PCP_TMP_DIR"));
    else
	strncpy(socketpath, path, sizeof(socketpath));
    socketpath[sizeof(socketpath)-1] = '\0';

    __pmSockAddrSetFamily(addr, AF_UNIX);
    __pmSockAddrSetPath(addr, socketpath);

    /* Create client socket connection */
    if ((fd = __pmCreateUnixSocket()) < 0) {
	__pmNotifyErr(LOG_ERR, "pmdaRootConnect: cannot create socket %s: %s\n",
			socketpath, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmSockAddrFree(addr);
	return fd;
    }

    sts = __pmConnect(fd, addr, -1);
    __pmSockAddrFree(addr);
    if (sts < 0) {
	if (sts != -EPERM || (pmDebug & DBG_TRACE_LIBPMDA))
	    __pmNotifyErr(LOG_INFO,
			"pmdaRootConnect: cannot connect to %s: %s\n",
			socketpath, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    /* Check server connection information */
    if ((sts = __pmdaRecvRootPDUInfo(fd, &version, &features)) < 0) {
	__pmNotifyErr(LOG_ERR,
			"pmdaRootConnect: cannot verify %s server: %s\n",
			socketpath, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }

    if (pmDebug & DBG_TRACE_LIBPMDA)
	__pmNotifyErr(LOG_INFO,
			"pmdaRootConnect: %s server version=%d features=0x%x\n",
			socketpath, version, features);
    return fd;
}

void
pmdaRootShutdown(int clientfd)
{
    if (clientfd >= 0)
	shutdown(clientfd, SHUT_RDWR);
}

#if defined(HAVE_SETNS)
static int
nsopen(const char *process, const char *namespace)
{
    char path[MAXPATHLEN];

    snprintf(path, sizeof(path), "/proc/%s/ns/%s", process, namespace);
    path[sizeof(path)-1] = '\0';
    return open(path, O_RDONLY);
}

int
__pmdaOpenNameSpaceFds(int nsflags, int pid, int *fdset)
{
    int		fd;
    char	process[32];

    if (pid > 0)
	snprintf(process, sizeof(process), "%d", pid);
    else
	strcpy(process, "self");

    if (nsflags & PMDA_NAMESPACE_IPC) {
	if ((fd = nsopen(process, "ipc")) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_IPC_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_UTS) {
	if ((fd = nsopen(process, "uts")) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_UTS_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_NET) {
	if ((fd = nsopen(process, "net")) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_NET_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_MNT) {
	if ((fd = nsopen(process, "mnt")) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_MNT_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_USER) {
	if ((fd = nsopen(process, "user")) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_USER_INDEX] = fd;
    }
    return 0;
}

int
__pmdaSetNameSpaceFds(int nsflags, int *fdset)
{
    int		sts = 0;

    if (nsflags & PMDA_NAMESPACE_IPC)
	sts |= setns(fdset[PMDA_NAMESPACE_IPC_INDEX], 0);
    if (nsflags & PMDA_NAMESPACE_UTS)
	sts |= setns(fdset[PMDA_NAMESPACE_UTS_INDEX], 0);
    if (nsflags & PMDA_NAMESPACE_NET)
	sts |= setns(fdset[PMDA_NAMESPACE_NET_INDEX], 0);
    if (nsflags & PMDA_NAMESPACE_MNT)
	sts |= setns(fdset[PMDA_NAMESPACE_MNT_INDEX], 0);
    if (nsflags & PMDA_NAMESPACE_USER)
	sts |= setns(fdset[PMDA_NAMESPACE_USER_INDEX], 0);

    if (sts)
	return -oserror();
    return sts;
}

/*
 * Use setns(2) syscall to switch temporarily to a different namespace.
 * On the first call for each namespace we stash away a file descriptor
 * that will get us back to where we started.
 * Note: the NameSpaceFdsReq PDU is sent by the caller (contents depend
 * on whether we switch for a specific process ID or a container name).
 */
static int
__pmdaEnterNameSpaces(int clientfd, int nsflags)
{
    int		fdset[PMDA_NAMESPACE_COUNT];	/* NB: packed, ordered */
    int		flags, count, sts, i;

    /* recvmsg from pmdaroot, error or results */
    if ((sts = __pmdaRecvRootNameSpaceFds(clientfd, fdset, &count)) < 0)
	return sts;

    /* open my own namespace fds, stash 'em for LeaveNameSpaces */
    if ((sts = __pmdaOpenNameSpaceFds(nsflags, -1, self_fdset)) < 0)
	return sts;

    /* finish: unpack the result fds, and call setns(2) */
    sts = 0;
    flags = nsflags;
    for (i = sts = 0; i < count; i++) {
	if (flags & PMDA_NAMESPACE_IPC) {
	    priv_fdset[PMDA_NAMESPACE_IPC_INDEX] = fdset[i];
	    flags &= ~PMDA_NAMESPACE_IPC;
	}
	if (flags & PMDA_NAMESPACE_UTS) {
	    priv_fdset[PMDA_NAMESPACE_UTS_INDEX] = fdset[i];
	    flags &= ~PMDA_NAMESPACE_UTS;
	}
	if (flags & PMDA_NAMESPACE_NET) {
	    priv_fdset[PMDA_NAMESPACE_NET_INDEX] = fdset[i];
	    flags &= ~PMDA_NAMESPACE_NET;
	}
	if (flags & PMDA_NAMESPACE_MNT) {
	    priv_fdset[PMDA_NAMESPACE_MNT_INDEX] = fdset[i];
	    flags &= ~PMDA_NAMESPACE_MNT;
	}
	if (flags & PMDA_NAMESPACE_USER) {
	    priv_fdset[PMDA_NAMESPACE_USER_INDEX] = fdset[i];
	    flags &= ~PMDA_NAMESPACE_USER;
	}
    }
    if (count && !sts)
	sts = __pmdaSetNameSpaceFds(nsflags, priv_fdset);

    if (sts)
	return -oserror();
    return 0;
}

int
pmdaEnterContainerNameSpaces(int clientfd, const char *container, int nsflags)
{
    int	sts;

    if (clientfd < 0)
	return PM_ERR_NOTCONN;

    /* sendmsg to pmdaroot */
    if ((sts = __pmdaSendRootNameSpaceFdsReq(clientfd, nsflags,
			container, strlen(container), 0, 0)) < 0)
	return sts;
    /* process the results */
    return __pmdaEnterNameSpaces(clientfd, nsflags);
}

int
pmdaEnterProcessNameSpaces(int clientfd, int pid, int nsflags)
{
    int	sts;

    if (clientfd < 0)
	return PM_ERR_NOTCONN;

    /* sendmsg to pmdaroot */
    if ((sts = __pmdaSendRootNameSpaceFdsReq(clientfd, nsflags,
			NULL, 0, pid, 0)) < 0)
	return sts;
    /* process the results */
    return __pmdaEnterNameSpaces(clientfd, nsflags);
}

/*
 * And another setns(2) to switch back to the original namespace
 */
int
pmdaLeaveNameSpaces(int clientfd, int nsflags)
{
    int		sts;

    if (clientfd < 0)
	return PM_ERR_NOTCONN;

    sts = __pmdaSetNameSpaceFds(nsflags, self_fdset);
    __pmdaCloseNameSpaceFds(nsflags, priv_fdset);
    return sts;
}

int
__pmdaCloseNameSpaceFds(int nsflags, int *fdset)
{
    if (nsflags & PMDA_NAMESPACE_IPC) {
	close(fdset[PMDA_NAMESPACE_IPC_INDEX]);
	fdset[PMDA_NAMESPACE_IPC_INDEX] = -1;
    }
    if (nsflags & PMDA_NAMESPACE_UTS) {
	close(fdset[PMDA_NAMESPACE_UTS_INDEX]);
	fdset[PMDA_NAMESPACE_UTS_INDEX] = -1;
    }
    if (nsflags & PMDA_NAMESPACE_NET) {
	close(fdset[PMDA_NAMESPACE_NET_INDEX]);
	fdset[PMDA_NAMESPACE_NET_INDEX] = -1;
    }
    if (nsflags & PMDA_NAMESPACE_MNT) {
	close(fdset[PMDA_NAMESPACE_MNT_INDEX]);
	fdset[PMDA_NAMESPACE_MNT_INDEX] = -1;
    }
    if (nsflags & PMDA_NAMESPACE_USER) {
	close(fdset[PMDA_NAMESPACE_USER_INDEX]);
	fdset[PMDA_NAMESPACE_USER_INDEX] = -1;
    }
    return 0;
}

#else	/* no support on this platform */
int
pmdaEnterContainerNameSpaces(int clientfd, const char *container, int nsflags)
{
    (void)clientfd;
    (void)nsflags;
    (void)container;
    return -ENOTSUP;
}
int
pmdaEnterProcessNameSpaces(int clientfd, int process, int nsflags)
{
    (void)clientfd;
    (void)nsflags;
    (void)process;
    return -ENOTSUP;
}
int
pmdaLeaveNameSpaces(int clientfd, int nsflags)
{
    (void)clientfd;
    (void)nsflags;
    return -ENOTSUP;
}
int
__pmdaOpenNameSpaceFds(int nsflags, int pid, int *fdset)
{
    (void)nsflags;
    (void)pid;
    (void)fdset;
    return -ENOTSUP;
}
int
__pmdaSetNameSpaceFds(int nsflags, int *fdset)
{
    (void)nsflags;
    (void)fdset;
    return -ENOTSUP;
}
int
__pmdaCloseNameSpaceFds(int nsflags, int *fdset)
{
    (void)nsflags;
    (void)fdset;
    return -ENOTSUP;
}
#endif
