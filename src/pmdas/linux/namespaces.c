/*
 * Copyright (c) 2014-2015 Red Hat.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
#include "pmapi.h"
#include "impl.h"
#include "pmda.h"
#include "namespaces.h"

#if defined(HAVE_SETNS)

static int root_fdset[LINUX_NAMESPACE_COUNT];
static int self_fdset[LINUX_NAMESPACE_COUNT];

static int
namespace_open(const char *process, const char *namespace)
{
    char path[MAXPATHLEN];

    snprintf(path, sizeof(path), "/proc/%s/ns/%s", process, namespace);
    path[sizeof(path)-1] = '\0';
    return open(path, O_RDONLY);
}

static int
open_namespace_fds(int nsflags, int pid, int *fdset)
{
    int		fd;
    char	process[32];

    if (pid)
	snprintf(process, sizeof(process), "%d", pid);
    else
	strcpy(process, "self");

    if ((nsflags & LINUX_NAMESPACE_IPC) &&
	(pid || !fdset[LINUX_NAMESPACE_IPC_INDEX])) {
	if ((fd = namespace_open(process, "ipc")) < 0)
		return fd;
	fdset[LINUX_NAMESPACE_IPC_INDEX] = fd;
    }
    if ((nsflags & LINUX_NAMESPACE_UTS) &&
	(pid || !fdset[LINUX_NAMESPACE_UTS_INDEX])) {
	if ((fd = namespace_open(process, "uts")) < 0)
	    return fd;
	fdset[LINUX_NAMESPACE_UTS_INDEX] = fd;
    }
    if ((nsflags & LINUX_NAMESPACE_NET) &&
	(pid || !fdset[LINUX_NAMESPACE_NET_INDEX])) {
	if ((fd = namespace_open(process, "net")) < 0)
	    return fd;
	fdset[LINUX_NAMESPACE_NET_INDEX] = fd;
    }
    if ((nsflags & LINUX_NAMESPACE_MNT) &&
	(pid || !fdset[LINUX_NAMESPACE_MNT_INDEX])) {
	if ((fd = namespace_open(process, "mnt")) < 0)
	    return fd;
	fdset[LINUX_NAMESPACE_MNT_INDEX] = fd;
    }
    if ((nsflags & LINUX_NAMESPACE_USER) &&
	(pid || !fdset[LINUX_NAMESPACE_USER_INDEX])) {
	if ((fd = namespace_open(process, "user")) < 0)
	    return fd;
	fdset[LINUX_NAMESPACE_USER_INDEX] = fd;
    }
    return 0;
}

static int
set_namespace_fds(int nsflags, int *fdset)
{
    int		sts = 0;

    if (nsflags & LINUX_NAMESPACE_IPC)
	sts |= setns(fdset[LINUX_NAMESPACE_IPC_INDEX], 0);
    if (nsflags & LINUX_NAMESPACE_UTS)
	sts |= setns(fdset[LINUX_NAMESPACE_UTS_INDEX], 0);
    if (nsflags & LINUX_NAMESPACE_NET)
	sts |= setns(fdset[LINUX_NAMESPACE_NET_INDEX], 0);
    if (nsflags & LINUX_NAMESPACE_MNT)
	sts |= setns(fdset[LINUX_NAMESPACE_MNT_INDEX], 0);
    if (nsflags & LINUX_NAMESPACE_USER)
	sts |= setns(fdset[LINUX_NAMESPACE_USER_INDEX], 0);

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
process_enter_namespaces(int pid, int nsflags)
{
    int sts;

    /* open my own namespace fds, stash 'em for LeaveNameSpaces */
    if ((sts = open_namespace_fds(nsflags, 0, self_fdset)) < 0)
	return sts;

    /* open namespace fds for pid */
    if ((sts = open_namespace_fds(nsflags, pid, root_fdset)) < 0)
	return sts;

    /* finally switch local namespaces */
    return set_namespace_fds(nsflags, root_fdset);
}

int
container_enter_namespaces(int fd, linux_container_t *cp, int nsflags)
{
    char pdubuf[BUFSIZ], name[MAXPATHLEN], *np;
    int	sts, pid = 0;

    if (fd < 0)
	return PM_ERR_NOTCONN;

    /* get container process identifier from pmdaroot */
    if ((sts = __pmdaSendRootPDUContainer(fd, PDUROOT_PROCESSID_REQ,
			pid, cp->name, cp->length, 0)) < 0)
	return sts;
    if ((sts = __pmdaRecvRootPDUContainer(fd, PDUROOT_PROCESSID,
                        pdubuf, sizeof(pdubuf))) < 0)
	return sts;
    if ((sts = __pmdaDecodeRootPDUContainer(pdubuf, sts,
			&pid, name, sizeof(name))) < 0)
	return sts;

    /* process the results - stash current container details */
    if (sts > cp->length && (np = strdup(name)) != NULL) {
	cp->length = sts;
	free(cp->name);
	cp->name = np;
    }
    cp->pid = pid;
    if ((sts = process_enter_namespaces(pid, nsflags)) < 0)
	return sts;
    return 0;
}

static int
close_namespace_fds(int nsflags, int *fdset)
{
    if (nsflags & LINUX_NAMESPACE_IPC) {
	close(fdset[LINUX_NAMESPACE_IPC_INDEX]);
	fdset[LINUX_NAMESPACE_IPC_INDEX] = -1;
    }
    if (nsflags & LINUX_NAMESPACE_UTS) {
	close(fdset[LINUX_NAMESPACE_UTS_INDEX]);
	fdset[LINUX_NAMESPACE_UTS_INDEX] = -1;
    }
    if (nsflags & LINUX_NAMESPACE_NET) {
	close(fdset[LINUX_NAMESPACE_NET_INDEX]);
	fdset[LINUX_NAMESPACE_NET_INDEX] = -1;
    }
    if (nsflags & LINUX_NAMESPACE_MNT) {
	close(fdset[LINUX_NAMESPACE_MNT_INDEX]);
	fdset[LINUX_NAMESPACE_MNT_INDEX] = -1;
    }
    if (nsflags & LINUX_NAMESPACE_USER) {
	close(fdset[LINUX_NAMESPACE_USER_INDEX]);
	fdset[LINUX_NAMESPACE_USER_INDEX] = -1;
    }
    return 0;
}

/*
 * And another setns(2) to switch back to the original namespace
 */
int
container_leave_namespaces(int fd, int nsflags)
{
    int		sts;

    if (fd < 0)
	return PM_ERR_NOTCONN;

    sts = set_namespace_fds(nsflags, self_fdset);
    close_namespace_fds(nsflags, root_fdset);
    return sts;
}

#else
int
container_enter_namespaces(int fd, linux_container_t *lcp, int nsflags)
{
    (void)fd;
    (void)lcp;
    (void)nsflags;
    return PM_ERR_APPVERSION;
}

int
container_leave_namespaces(int fd, int nsflags)
{
    (void)fd;
    (void)nsflags;
    return PM_ERR_APPVERSION;
}
#endif

int
container_open_network(linux_container_t *cp)
{
    if (cp->netfd < 0)
	cp->netfd = socket(AF_INET, SOCK_DGRAM, 0);
    return cp->netfd;
}

void
container_close_network(linux_container_t *cp)
{
    if (cp->netfd != -1)
	close(cp->netfd);
    cp->netfd = -1;
}
