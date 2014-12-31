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
    *features = pduinfo.features;
    return 0;
}

/* Client sends __pmdaRootPDUNameSpaceFdsReq PDUs */
int
__pmdaSendRootNameSpaceFdsReq(int socket, int flags, const char *name, int namelen, int status)
{
    char buffer[sizeof(__pmdaRootPDUNameSpaceFdsReq) + MAXPATHLEN];
    __pmdaRootPDUNameSpaceFdsReq *pdu = (__pmdaRootPDUNameSpaceFdsReq *)buffer;
    int length;

    if (namelen < 1)
	return -EINVAL;
    if (namelen > MAXPATHLEN)
	return -E2BIG;

    length = sizeof(__pmdaRootPDUNameSpaceFdsReq) + namelen;
    pdu->hdr.type = PDUROOT_NS_FDS_REQ;
    pdu->hdr.length = length;
    pdu->hdr.status = status;
    pdu->hdr.version = ROOT_PDU_VERSION;
    pdu->flags = flags;
    return send(socket, pdu, length, 0);
}

/* Server decodes __pmdaRootPDUNameSpaceFdsReq PDUs */
int
__pmdaDecodeRootNameSpaceFdsReq(void *buf, int *flags, char **name, int *len)
{
    __pmdaRootPDUNameSpaceFdsReq *pdu = (__pmdaRootPDUNameSpaceFdsReq *)buf;
    int length;

    if (pdu->hdr.type != PDUROOT_NS_FDS_REQ)
        return -ESRCH;
    length = pdu->namelen;
    if (*len < (length + 1) - sizeof(__pmdaRootPDUHdr))
	return -E2BIG;

    *flags = pdu->flags;
    strncpy(*name, pdu->name, length);
    *name[length] = '\0';
    *len = length;
    return 0;
}

/* Server sends __pmdaRootPDUNameSpaceFds PDUs */
int
__pmdaSendRootNameSpaceFds(int socket, int pid, int *fdset, int count, int status)
{
    __pmdaRootPDUNameSpaceFds fdspdu;
    char cmsgbuf[CMSG_SPACE(sizeof(int) * PMDA_NAMESPACE_COUNT + 1)];
    struct cmsghdr *cmsg = NULL;
    struct msghdr hdr = { 0 };
    struct iovec data;
    ssize_t bytes = sizeof(int) * count;

    /*
     * used to be initialized in the declaration above, but this was
     * too tricky for the NetBSD gcc, so ...
     */
    memset(cmsgbuf, 0, sizeof(cmsgbuf));

    if (count < 1 || fdset == NULL)
	return -EINVAL;

    fdspdu.hdr.type = PDUROOT_NS_FDS;
    fdspdu.hdr.length = sizeof(__pmdaRootPDUNameSpaceFds);
    fdspdu.hdr.status = status;
    fdspdu.hdr.version = ROOT_PDU_VERSION;
    fdspdu.pid = pid;

    data.iov_len = sizeof(int);
    data.iov_base = &fdspdu;

    hdr.msg_iov = &data;
    hdr.msg_iovlen = 1;
    hdr.msg_control = cmsgbuf;
    hdr.msg_controllen = CMSG_LEN(bytes);

    if ((cmsg = CMSG_FIRSTHDR(&hdr)) == NULL)
	return -EINVAL;

    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(bytes);
    memcpy((int *)CMSG_DATA(cmsg), fdset, bytes);

    if ((bytes = sendmsg(socket, &hdr, 0)) < 0) {
	__pmNotifyErr(LOG_ERR, "__pmdaSendRootNameSpaceFds: sendmsg: %s\n",
			strerror(errno));
	return -oserror();
    }
    return 0;
}

static int
extract_fds(struct cmsghdr *cmsg, struct msghdr *msg, int *fds, int maxfds)
{
    int *recvfds, i, count = 0;

    if (!cmsg || !msg)
	return count;
    if (cmsg->cmsg_type != SCM_RIGHTS)
	return count;
    if (msg->msg_controllen > 0) {
	recvfds = (int *)CMSG_DATA(cmsg);
	count = (cmsg->cmsg_len - sizeof(*cmsg)) / sizeof(int);
	for (i = 0; i < count; i++) {
	    if (i < maxfds) {
		*fds++ = *recvfds++;
	    } else {
		close(*recvfds++);
		count--;
	    }
	}
    }
    return count;
}

/* Client recvs __pmdaRootPDUNameSpaceFds PDUs */
int
__pmdaRecvRootNameSpaceFds(int socket, int *fdset, int *count)
{
    __pmdaRootPDUNameSpaceFds *fdspdu, fdsbuf;
    char buf[CMSG_SPACE(sizeof(int) * PMDA_NAMESPACE_COUNT + 1)];
    struct cmsghdr *cmsg = NULL;
    struct msghdr msg = { 0 };
    struct iovec vec = { 0 };
    int sts;

    vec.iov_base = (void *)&fdsbuf;
    vec.iov_len = sizeof(fdsbuf);
    msg.msg_iov = &vec;
    msg.msg_iovlen = 1;
    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    if ((sts = recvmsg(socket, &msg, 0)) < 0)
	return -oserror();

    if (msg.msg_iovlen != 1)
	return -EINVAL;
    fdspdu = (__pmdaRootPDUNameSpaceFds *)msg.msg_iov;
    if (sts != sizeof(*fdspdu))
	return -EINVAL;
    if (fdspdu->hdr.type != PDUROOT_NS_FDS)
	return -ESRCH;
    if (fdspdu->hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (fdspdu->hdr.status != 0)
	return fdspdu->hdr.status;
    if (fdspdu->hdr.length != sizeof(__pmdaRootPDUNameSpaceFds))
	return -E2BIG;

    /* extract the open file namespace descriptors */
    if ((cmsg = CMSG_FIRSTHDR(&msg)) == NULL)
	return -EINVAL;
    do {
	*count += extract_fds(cmsg, &msg, fdset, PMDA_NAMESPACE_COUNT);
	cmsg = CMSG_NXTHDR(&msg, cmsg);
    } while (cmsg != NULL);

    return 0;
}
