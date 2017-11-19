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

#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL	0
#endif

/* Server sends __pmdaRootPDUInfo PDUs */
int
__pmdaSendRootPDUInfo(int fd, int features, int status)
{
    __pmdaRootPDUInfo	pduinfo;

    pduinfo.hdr.type = PDUROOT_INFO;
    pduinfo.hdr.length = sizeof(__pmdaRootPDUInfo);
    pduinfo.hdr.status = status;
    pduinfo.hdr.version = ROOT_PDU_VERSION;

    pduinfo.features = features;
    pduinfo.zeroed = 0;

    __pmIgnoreSignalPIPE();
    return send(fd, (const char *)&pduinfo, sizeof(pduinfo), MSG_NOSIGNAL);
}

/* Client recvs __pmdaRootPDUInfo PDUs */
int
__pmdaRecvRootPDUInfo(int fd, int *version, int *features)
{
    __pmdaRootPDUInfo	pduinfo;
    int			sts;

    if ((sts = recv(fd, (char *)&pduinfo, sizeof(pduinfo), 0)) < 0)
	return -oserror();
    if (sts != sizeof(__pmdaRootPDUInfo))
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
    return sts;
}

/* Client and server send __pmdaRootPDUContainer PDUs */
int
__pmdaSendRootPDUContainer(int fd, int pdutype,
		int pid, const char *name, int len, int status)
{
    __pmdaRootPDUContainer pdu;
    int length;

    if (len < 0)
	return -EINVAL;
    if (len >= MAXPATHLEN)
	return -E2BIG;

    length = sizeof(__pmdaRootPDUContainer) - sizeof(pdu.name) + len;
    pdu.hdr.type = pdutype;
    pdu.hdr.length = length;
    pdu.hdr.status = status;
    pdu.hdr.version = ROOT_PDU_VERSION;

    pdu.pid = pid;
    pdu.namelen = len;
    memset(pdu.name, 0, sizeof(pdu.name));
    if (len > 0)
	strncpy(pdu.name, name, len);

    __pmIgnoreSignalPIPE();
    return send(fd, (const char *)&pdu, length, MSG_NOSIGNAL);
}

/* Client and server recv __pmdaRootPDUContainer PDUs */
int
__pmdaRecvRootPDUContainer(int fd, int type, void *buffer, int buflen)
{
    __pmdaRootPDUContainer *pdu = (__pmdaRootPDUContainer *)buffer;
    size_t minlength = sizeof(*pdu) - sizeof(pdu->name);
    int sts;

    if ((sts = recv(fd, (char *)buffer, buflen, 0)) < 0)
	return -oserror();
    if (sts < minlength)
	return -EINVAL;
    if (pdu->hdr.type != type)
	return -ESRCH;
    if (pdu->hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;
    if (pdu->hdr.length < minlength + pdu->namelen)
	return -E2BIG;
    return sts;
}

/* Client and server decode __pmdaRootPDUContainer PDUs */
int
__pmdaDecodeRootPDUContainer(void *buf, int blen, int *pid, char *name, int nlen)
{
    __pmdaRootPDUContainer *pdu = (__pmdaRootPDUContainer *)buf;
    int length;

    length = pdu->namelen;
    if (blen < (length + 1) + sizeof(__pmdaRootPDUHdr))
	return -EINVAL;

    if (pid)
	*pid = pdu->pid;
    if (!name)
	return length;

    if (length) {
	if (length >= nlen)
	    return -E2BIG;
	strncpy(name, pdu->name, length);
	name[length] = '\0';
    }
    return length;
}

/* PMCD sends __pmdaRootPDUStartReq PDUs */
int
__pmdaSendRootPDUStartReq(int fd, int ipctype,
		const char *name, int namelen, const char* args, int argslen)
{
    __pmdaRootPDUStart	pdu;
    size_t		length;

    memset(&pdu, 0, sizeof(pdu));
    if (namelen <= 0 || argslen <= 0)
	return -EINVAL;
    if (namelen >= MAXPMDALEN || argslen >= MAXPATHLEN)
	return -E2BIG;
    length = sizeof(__pmdaRootPDUStart) - sizeof(pdu.args) + argslen;

    pdu.hdr.type = PDUROOT_STARTPMDA_REQ;
    pdu.hdr.length = length;
    pdu.hdr.version = ROOT_PDU_VERSION;

    pdu.ipctype = ipctype;
    pdu.namelen = namelen;
    if (namelen > 0)
	strncpy(pdu.name, name, namelen);
    pdu.argslen = argslen;
    if (argslen > 0)
	strncpy(pdu.args, args, argslen);

    __pmIgnoreSignalPIPE();
    return send(fd, (const char *)&pdu, length, MSG_NOSIGNAL);
}

/* Server recvs __pmdaRootPDUStartReq PDUs */
int
__pmdaRecvRootPDUStartReq(int fd, void *buffer, int buflen)
{
    __pmdaRootPDUStart *pdu = (__pmdaRootPDUStart *)buffer;
    size_t		minlength = sizeof(*pdu) - sizeof(pdu->args);
    int			sts;

    if ((sts = recv(fd, (char *)&pdu, sizeof(pdu), 0)) < 0)
	return -oserror();
    if (sts < minlength)
	return -EINVAL;
    if (pdu->hdr.type != PDUROOT_STARTPMDA_REQ)
	return -ESRCH;
    if (pdu->hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;
    if (pdu->hdr.length < minlength + pdu->argslen)
	return -E2BIG;
    if (pdu->namelen > MAXPMDALEN)
	return -E2BIG;
    return sts;
}

#ifndef IS_MINGW
/* Server sends __pmdaRootPDUStart PDUs (with PID and open FDs) */
int
__pmdaSendRootPDUStart(int fd, int pid, int infd, int outfd,
		const char *name, int namelen, int status)
{
    __pmdaRootPDUStart	pdu;
    struct iovec	iov;
    struct msghdr	msgh;
    int			iofds[2];
    int			*ioptr = 0;
    union {
        struct cmsghdr	cmh;
        char		control[CMSG_SPACE(sizeof(iofds))];
    } control_un;
    struct cmsghdr	*cmhp;

    if (namelen <= 0)
	return -EINVAL;
    if (namelen >= MAXPMDALEN)
	return -E2BIG;

    memset(&pdu, 0, sizeof(pdu));
    pdu.hdr.type = PDUROOT_STARTPMDA;
    pdu.hdr.length = sizeof(pdu) - sizeof(pdu.args);
    pdu.hdr.status = status;
    pdu.hdr.version = ROOT_PDU_VERSION;

    pdu.pid = pid;
    /*
     * FDs passed via control message, PDU fields are filled in by
     * the __pmdaRecvRootPDUStart code (i.e. client/pmcd side).
     */
    pdu.namelen = namelen;
    if (namelen > 0)
	strncpy(pdu.name, name, namelen);

    memset(&msgh, 0, sizeof(msgh));
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &pdu;
    iov.iov_len = sizeof(pdu) - sizeof(pdu.args);

    memset(&control_un, 0, sizeof(control_un));
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    cmhp = CMSG_FIRSTHDR(&msgh);
    cmhp->cmsg_level = SOL_SOCKET;
    cmhp->cmsg_type = SCM_RIGHTS;

    if (infd < 0 || outfd < 0) {
	cmhp->cmsg_len = CMSG_LEN(0);
    } else {
	cmhp->cmsg_len = CMSG_LEN(sizeof(iofds));
	iofds[0] = infd;
	iofds[1] = outfd;
	ioptr = (int *)CMSG_DATA(cmhp);
	memcpy(ioptr, iofds, sizeof(iofds));
    }
    __pmIgnoreSignalPIPE();
    return sendmsg(fd, &msgh, MSG_NOSIGNAL);
}

/* PMCD recvs __pmdaRootPDUStart PDUs (with PID and open FDs) */
int
__pmdaRecvRootPDUStart(int fd, void *buffer, int buflen)
{
    __pmdaRootPDUStart	*pdu = (__pmdaRootPDUStart *)buffer;
    size_t		minlength = sizeof(*pdu) - sizeof(pdu->args);
    struct msghdr	msgh;
    struct iovec	iov;
    int			sts;
    int			iofds[2];
    int			*ioptr;
    union {
        struct cmsghdr	cmh;
        char  		control[CMSG_SPACE(sizeof(iofds))];
    } control_un;
    struct cmsghdr	*cmhp;

    control_un.cmh.cmsg_len = CMSG_LEN(sizeof(iofds));
    control_un.cmh.cmsg_level = SOL_SOCKET;
    control_un.cmh.cmsg_type = SCM_RIGHTS;

    /*
     * Set 'msgh' control fields to describe 'control_un'
     * Set 'msgh' iov (I/O vector) to point to buffer used
     * to receive the "real" data read by recvmsg()
     */
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_len = sizeof(__pmdaRootPDUStart);
    iov.iov_base = buffer;
    msgh.msg_name = NULL;               /* We don't need address of peer */
    msgh.msg_namelen = 0;
 
    if ((sts = recvmsg(fd, &msgh, MSG_NOSIGNAL)) < 0) {
	pmNotifyErr(LOG_DEBUG, "recvmsg: %d %s\n", errno, strerror(errno));
	return -oserror();
    }

    if (pdu->hdr.type != PDUROOT_STARTPMDA)
	return -ESRCH;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;
    if (sts < minlength)
	return -EINVAL;
    if (pdu->hdr.length < minlength)
	return -E2BIG;

    cmhp = CMSG_FIRSTHDR(&msgh);
    if (cmhp == NULL ||
	cmhp->cmsg_len != CMSG_LEN(sizeof(iofds)) ||
	cmhp->cmsg_level != SOL_SOCKET ||
	cmhp->cmsg_type != SCM_RIGHTS) {
	pdu->infd = -1;
	pdu->outfd = -1;
    } else {
	ioptr = (int *)CMSG_DATA(cmhp);
	memcpy(iofds, ioptr, sizeof(iofds));
	pdu->infd = iofds[0];
	pdu->outfd = iofds[1];
    }
    return sts;
}
#else
int
__pmdaSendRootPDUStart(int fd, int pid, int infd, int outfd,
		const char *name, int namelen, int status)
{
    (void)fd;
    (void)pid;
    (void)infd;
    (void)outfd;
    (void)name;
    (void)namelen;
    (void)status;
    return -EOPNOTSUPP;
}
int
__pmdaRecvRootPDUStart(int fd, void *buffer, int buflen)
{
    (void)fd;
    (void)buffer;
    (void)buflen;
    return -EOPNOTSUPP;
}
#endif

/* Server and PMCD decode __pmdaRootPDUStart PDUs */
int
__pmdaDecodeRootPDUStart(void *buf, int blen, int *pid, int *infd, int *outfd,
		int *ipctype, char *name, int namelen, char *args, int argslen)
{
    __pmdaRootPDUStart	*pdu = (__pmdaRootPDUStart *)buf;

    if (pid)
	*pid = pdu->pid;
    if (infd)
	*infd = pdu->infd;
    if (outfd)
	*outfd = pdu->outfd;
    if (ipctype)
	*ipctype = pdu->ipctype;
    if (namelen) {
	strncpy(name, pdu->name, pdu->namelen);
	name[pdu->namelen] = '\0';
    }
    if (argslen) {
	strncpy(args, pdu->args, pdu->argslen);
	args[pdu->argslen] = '\0';
    }
    return 0;
}

/* PMCD sends __pmdaRootPDUStop PDUs */
int
__pmdaSendRootPDUStop(int fd, int pdutype, int pid, int code, int force, int status)
{
    __pmdaRootPDUStop	pdu;

    pdu.hdr.type = pdutype;
    pdu.hdr.length = sizeof(__pmdaRootPDUStop);
    pdu.hdr.status = status;
    pdu.hdr.version = ROOT_PDU_VERSION;
    pdu.pid = pid;
    pdu.code = code;
    pdu.force = force;
    pdu.zeroed = 0;

    __pmIgnoreSignalPIPE();
    return send(fd, (const char *)&pdu, sizeof(pdu), MSG_NOSIGNAL);
}

/* Server recvs __pmdaRootPDUStop PDUs */
int
__pmdaRecvRootPDUStop(int fd, int pdutype, void* buffer, int buflen)
{
    __pmdaRootPDUStop	*pdu = (__pmdaRootPDUStop *)buffer;
    int			sts;

    if ((sts = recv(fd, (char *)buffer, buflen, 0)) < 0)
	return -oserror();

    if (sts < sizeof(*pdu))
	return -EINVAL;
    if (pdu->hdr.type != pdutype)
	return -ESRCH;
    if (pdu->hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;
    return sts;
}

/* Server decodes __pmdaRootPDUStop PDUs */
int
__pmdaDecodeRootPDUStop(void *buf, int buflen, int *pid, int *code, int *force)
{
    __pmdaRootPDUStop *pdu = (__pmdaRootPDUStop *)buf;

    if (pid)
	*pid = pdu->pid;
    if (code)
	*code = pdu->code;
    if (force)
	*force = pdu->force;
    return 0;
}
