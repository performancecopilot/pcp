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

/* Server sends __pmdaRootPDUInfo PDUs */
int
__pmdaSendRootPDUInfo(int socket, int features, int status)
{
    __pmdaRootPDUInfo	pduinfo;

    pduinfo.hdr.type = PDUROOT_INFO;
    pduinfo.hdr.length = sizeof(__pmdaRootPDUInfo);
    pduinfo.hdr.status = status;
    pduinfo.hdr.version = ROOT_PDU_VERSION;

    pduinfo.features = features;
    pduinfo.zeroed = 0;

    __pmIgnoreSignalPIPE();
    return send(socket, &pduinfo, sizeof(pduinfo), 0);
}

/* Client recvs __pmdaRootPDUInfo PDUs */
int
__pmdaRecvRootPDUInfo(int socket, int *version, int *features)
{
    __pmdaRootPDUInfo	pduinfo;
    int			sts;

    if ((sts = recv(socket, &pduinfo, sizeof(pduinfo), 0)) < 0)
	return -oserror();
    if (sts != sizeof(pduinfo))
	return -EINVAL;
    if (pduinfo.hdr.type != PDUROOT_INFO)
	return -ESRCH;
    if (pduinfo.hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (pduinfo.hdr.status != 0)
	return pduinfo.hdr.status;
    if (pduinfo.hdr.length != sizeof(__pmdaRootPDUInfo))
	return -E2BIG;
    *version = pduinfo.hdr.version;
    *features = pduinfo.features;
    return 0;
}

/* Client sends __pmdaRootPDUNameSpaceFdsReq PDUs */
int
__pmdaSendRootNameSpaceFdsReq(int socket, int flags, const char *name, int namelen, int pid, int status)
{
    char buffer[sizeof(__pmdaRootPDUNameSpaceFdsReq) + MAXPATHLEN];
    __pmdaRootPDUNameSpaceFdsReq *pdu = (__pmdaRootPDUNameSpaceFdsReq *)buffer;
    int length;

    if (namelen < 0)
	return -EINVAL;
    if (namelen >= MAXPATHLEN)
	return -E2BIG;

    length = sizeof(__pmdaRootPDUNameSpaceFdsReq) + namelen;
    pdu->hdr.type = PDUROOT_NS_FDS_REQ;
    pdu->hdr.length = length;
    pdu->hdr.status = status;
    pdu->hdr.version = ROOT_PDU_VERSION;

    pdu->flags = flags;
    pdu->zeroed = 0;
    pdu->pid = pid;
    pdu->namelen = namelen;
    if (namelen > 0)
	strncpy(pdu->name, name, namelen);

    __pmIgnoreSignalPIPE();
    return send(socket, pdu, length, 0);
}

/* Server decodes __pmdaRootPDUNameSpaceFdsReq PDUs */
int
__pmdaDecodeRootNameSpaceFdsReq(void *buf,
		int *flags, char **name, int *len, int *pid)
{
    __pmdaRootPDUNameSpaceFdsReq *pdu = (__pmdaRootPDUNameSpaceFdsReq *)buf;
    char *buffer = *name;
    int length;

    if (pdu->hdr.type != PDUROOT_NS_FDS_REQ)
        return -ESRCH;
    length = pdu->namelen;
    if (*len < (length + 1) + sizeof(__pmdaRootPDUHdr))
	return -E2BIG;

    *flags = pdu->flags;
    *pid = pdu->pid;
    if (length) {
	strncpy(buffer, pdu->name, length);
	buffer[length] = '\0';
    } else {
	buffer = NULL;
    }
    *name = buffer;
    *len = length;
    return 0;
}

static int *
collapse_fdset(int *fdset, int flags, int *buffer, int *count)
{
    int index = 0;

    if (flags & PMDA_NAMESPACE_IPC)
	buffer[index++] = fdset[PMDA_NAMESPACE_IPC_INDEX];
    if (flags & PMDA_NAMESPACE_UTS)
	buffer[index++] = fdset[PMDA_NAMESPACE_UTS_INDEX];
    if (flags & PMDA_NAMESPACE_NET)
	buffer[index++] = fdset[PMDA_NAMESPACE_NET_INDEX];
    if (flags & PMDA_NAMESPACE_MNT)
	buffer[index++] = fdset[PMDA_NAMESPACE_MNT_INDEX];
    if (flags & PMDA_NAMESPACE_USER)
	buffer[index++] = fdset[PMDA_NAMESPACE_USER_INDEX];

    *count = index;
    return buffer;
}

/* Server sends __pmdaRootPDUNameSpaceFds PDUs */
int
__pmdaSendRootNameSpaceFds(int socket, int pid, int *fdset, int flags, int status)
{
    __pmdaRootPDUNameSpaceFds fdspdu;
    int densefds[PMDA_NAMESPACE_COUNT], count;
    char cmsgbuf[CMSG_SPACE(sizeof(int) * PMDA_NAMESPACE_COUNT + 1)];
    struct cmsghdr *cmsg;
    struct msghdr msghdr;
    struct iovec pduptr;
    ssize_t bytes;

    fdset = collapse_fdset(fdset, flags, densefds, &count);
    bytes = sizeof(int) * count;

    if (count < 1 || fdset == NULL)
	return -EINVAL;

    fdspdu.hdr.type = PDUROOT_NS_FDS;
    fdspdu.hdr.length = sizeof(__pmdaRootPDUNameSpaceFds);
    fdspdu.hdr.status = status;
    fdspdu.hdr.version = ROOT_PDU_VERSION;
    fdspdu.pid = pid;
    fdspdu.flags = flags;
    pduptr.iov_base = &fdspdu;
    pduptr.iov_len = sizeof(fdspdu);

    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &pduptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = cmsgbuf;
    msghdr.msg_controllen = CMSG_LEN(bytes);

    if ((cmsg = CMSG_FIRSTHDR(&msghdr)) == NULL)
	return -EINVAL;
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_level = SOL_SOCKET;
    memcpy((int *)CMSG_DATA(cmsg), fdset, bytes);

    __pmIgnoreSignalPIPE();
    if ((bytes = sendmsg(socket, &msghdr, 0)) < 0) {
	__pmNotifyErr(LOG_ERR, "__pmdaSendRootNameSpaceFds: sendmsg: %s\n",
			osstrerror());
	return -oserror();
    }
    return 0;
}

/* Client recvs __pmdaRootPDUNameSpaceFds PDUs */
int
__pmdaRecvRootNameSpaceFds(int socket, int *fdset, int count)
{
    char buf[CMSG_SPACE(sizeof(int) * PMDA_NAMESPACE_COUNT)];
    __pmdaRootPDUNameSpaceFds fdspdu;
    struct cmsghdr *cmsg;
    struct msghdr msghdr;
    struct iovec pduptr;
    int i, sts;

    if (count > PMDA_NAMESPACE_COUNT)
	return -EINVAL;

    pduptr.iov_base = &fdspdu;
    pduptr.iov_len = sizeof(fdspdu);
    msghdr.msg_name = NULL;
    msghdr.msg_namelen = 0;
    msghdr.msg_iov = &pduptr;
    msghdr.msg_iovlen = 1;
    msghdr.msg_flags = 0;
    msghdr.msg_control = buf;
    msghdr.msg_controllen = CMSG_SPACE(sizeof(int) * count);
    cmsg = CMSG_FIRSTHDR(&msghdr);
    cmsg->cmsg_len = msghdr.msg_controllen;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_level = SOL_SOCKET;
    memset((int *)CMSG_DATA(cmsg), -1, sizeof(int) * count);

    if ((sts = recvmsg(socket, &msghdr, 0)) < 0)
	return -oserror();

    if (sts != sizeof(fdspdu))
        return -EINVAL;
    if (fdspdu.hdr.type != PDUROOT_NS_FDS)
        return -ESRCH;
    if (fdspdu.hdr.version > ROOT_PDU_VERSION)
        return -ENOTSUP;
    if (fdspdu.hdr.status != 0)
        return fdspdu.hdr.status;
    if (fdspdu.hdr.length != sizeof(__pmdaRootPDUNameSpaceFds))
        return -E2BIG;

    /* extract the open file namespace descriptors */
    for (i = 0; i < count; i++)
	fdset[i] = ((int *)CMSG_DATA(cmsg))[i];
    return 0;
}
