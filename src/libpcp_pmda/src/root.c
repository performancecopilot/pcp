/*
 * Service routines for elevated privilege services (pmdaroot).
 *
 * Copyright (c) 2014 Red Hat.
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

static int clientfd = -1;
static char socketpath[MAXPATHLEN];
static int priv_fdset[PMDA_NAMESPACE_COUNT];
static int self_fdset[PMDA_NAMESPACE_COUNT];

/*
 * Connect to the pmdaroot socket as a client, perform version exchange
 */
int
pmdaRootConnect(void)
{
    __pmSockAddr	*addr;
    char		errmsg[PM_MAXERRMSGLEN];
    int			i, fd, sts;

    /* Ensure we start from an initially-invalid fd state */
    for (i = 0; i < PMDA_NAMESPACE_COUNT; i++)
	self_fdset[i] = priv_fdset[i] = -1;

    /* Initialize the socket address. */
    if ((addr = __pmSockAddrAlloc()) == NULL)
	return -ENOMEM;

    snprintf(socketpath, MAXPATHLEN, "%s/pmcd/root.socket",
		pmGetConfig("PCP_TMP_DIR"));

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
	__pmNotifyErr(LOG_ERR, "pmdaRootConnect: cannot connect to %s: %s\n",
			socketpath, osstrerror_r(errmsg, sizeof(errmsg)));
	__pmCloseSocket(fd);
	return sts;
    }
    clientfd = fd;
    return 0;
}

void
pmdaRootShutdown(void)
{
    if (clientfd != -1)
	shutdown(clientfd, SHUT_RDWR);
    clientfd = -1;
}

#if defined(HAVE_SETNS)
int
__pmdaOpenNameSpaceFds(int nsflags, int pid, int *fdset)
{
    int		fd;

    (void)pid;	/* TODO: generalise this routine */

    if (nsflags & PMDA_NAMESPACE_IPC) {
	if ((fd = open("/proc/self/ns/ipc", O_RDONLY)) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_IPC_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_UTS) {
	if ((fd = open("/proc/self/ns/uts", O_RDONLY)) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_UTS_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_NET) {
	if ((fd = open("/proc/self/ns/net", O_RDONLY)) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_NET_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_MNT) {
	if ((fd = open("/proc/self/ns/mnt", O_RDONLY)) < 0)
	    return fd;
	fdset[PMDA_NAMESPACE_MNT_INDEX] = fd;
    }
    if (nsflags & PMDA_NAMESPACE_USER) {
	if ((fd = open("/proc/self/ns/user", O_RDONLY)) < 0)
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
 */
int
pmdaEnterContainerNameSpace(const char *container, int nsflags)
{
    int		fdset[PMDA_NAMESPACE_COUNT];	/* NB: packed, ordered */
    int		flags, count, sts, i;

    if (clientfd < 0)
	return PM_ERR_NOTCONN;

    /* setup: open my own namespace fds, stash 'em */
    if ((sts = __pmdaOpenNameSpaceFds(nsflags, getpid(), self_fdset)) < 0)
	return sts;

    /* sendmsg to pmdaroot, await results */
    if ((sts = __pmdaSendRootNameSpaceFdsReq(clientfd, nsflags, container, strlen(container), 0)) < 0)
	return sts;

    /* recvmsg from pmdaroot, error or results */
    if ((sts = __pmdaRecvRootNameSpaceFds(clientfd, &fdset, &count)) < 0)
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

/*
 * And another setns(2) to switch back to the original namespace
 */
int
pmdaLeaveContainerNameSpace(int nsflags)
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
pmdaEnterContainerNameSpace(const char *container, int nsflags)
{
    (void)nsflags;
    (void)container;
    return -ENOTSUP;
}
int
pmdaLeaveContainerNameSpace(int nsflags)
{
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
