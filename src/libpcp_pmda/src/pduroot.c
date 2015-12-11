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
#include "pmdaroot.h"

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
    return send(fd, (const char *)&pduinfo, sizeof(pduinfo), 0);
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
    if (len > 0)
	strncpy(pdu.name, name, len);
    else
	memset(pdu.name, 0, sizeof(pdu.name));

    __pmIgnoreSignalPIPE();
    return send(fd, (const char *)&pdu, length, 0);
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

#ifndef IS_MINGW
/* PMCD sends __pmdaRootPDUStart PDUs */
int
__pmdaSendRootPDUStart(int fd, int status,
		int pdutype, int ipctype, int infd, int outfd,
		const char *label, int labellen, const char* argv, int argvlen)
{
    __pmdaRootPDUStart	pdu;
    struct iovec	iov;
    struct msghdr	msgh;
    int			length;
    int			iofds[2];
    int			*ioptr = 0;
    union {
        struct cmsghdr	cmh;
        char		control[CMSG_SPACE(sizeof(iofds))];
    } control_un;
    struct cmsghdr	*cmhp;

    if (labellen < 0 || argvlen < 0)
	return -EINVAL;
    if (labellen >= MAXPATHLEN || argvlen >= MAXPATHLEN)
	return -E2BIG;

    length = sizeof(__pmdaRootPDUStart) - sizeof(pdu.label) + labellen + argvlen;
    pdu.hdr.type = pdutype;
    pdu.hdr.length = length;
    pdu.hdr.status = status;
    pdu.hdr.version = ROOT_PDU_VERSION;

    pdu.labellen = labellen;
    if (labellen > 0)
	strncpy(pdu.label, label, labellen);
    else
	memset(pdu.label, 0, sizeof(pdu.label));

    if (argvlen > 0)
	strncpy(pdu.argv, argv, argvlen);
    else
	memset(pdu.argv, 0, sizeof(pdu.argv));
    pdu.argvlen = argvlen;
    pdu.ipctype = ipctype;

    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_base = &pdu;
    iov.iov_len = length;

    msgh.msg_name = NULL;
    msgh.msg_namelen = 0;
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    cmhp = CMSG_FIRSTHDR(&msgh);
    cmhp->cmsg_len = CMSG_LEN(sizeof(iofds));
    cmhp->cmsg_level = SOL_SOCKET;
    cmhp->cmsg_type = SCM_RIGHTS;

    iofds[0] = infd;
    iofds[1] = outfd;
    ioptr = (int *)CMSG_DATA(cmhp);
    memcpy(ioptr, iofds, sizeof(iofds));

    __pmIgnoreSignalPIPE();
    return sendmsg(fd, &msgh, 0);
}

/* Server recvs __pmdaRootPDUStart PDUs */
int
__pmdaRecvRootPDUStart(int fd, int type, void *buffer, int buflen)
{
    __pmdaRootPDUStart	*pdu = (__pmdaRootPDUStart *)buffer;
    struct msghdr	msgh;
    struct iovec	iov;
    int			sts;
//  size_t			minlength;	/* TODO - length? */
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

    /* Set 'msgh' fields to describe 'control_un' */
    msgh.msg_control = control_un.control;
    msgh.msg_controllen = sizeof(control_un.control);

    /* Set fields of 'msgh' to point to buffer used to receive (real)
     * data read by recvmsg()
     */
    msgh.msg_iov = &iov;
    msgh.msg_iovlen = 1;
    iov.iov_len = sizeof(__pmdaRootPDUStart);
    iov.iov_base = buffer;

    msgh.msg_name = NULL;               /* We don't need address of peer */
    msgh.msg_namelen = 0;
 
    if ((sts = recvmsg(fd, &msgh, 0)) < 0) {
	__pmNotifyErr(LOG_DEBUG, "from: %d %s\n", errno, strerror(errno));
	return -oserror();
    }

/* TODO:
    minlength = sizeof(*pdu) - sizeof(pdu->pmDomainLabel) - sizeof(pdu->argv);
    if (pdu->hdr.type != type)
	return -ESRCH;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;
    if (sts < minlength)
	return -EINVAL;
    if (pdu->hdr.length < minlength + pdu->pmDomainLabelLength + pdu->argvLength)
	return -E2BIG;
*/
    cmhp = CMSG_FIRSTHDR(&msgh);
    if (cmhp == NULL || cmhp->cmsg_len != CMSG_LEN(sizeof(iofds))) {
	__pmNotifyErr(LOG_DEBUG, "bad cmsg header / message length %ld",
			cmhp->cmsg_len);	/* TODO */
    }
    if (cmhp->cmsg_level != SOL_SOCKET)
        __pmNotifyErr(LOG_DEBUG,"cmsg_level != SOL_SOCKET");	/* TODO */
    if (cmhp->cmsg_type != SCM_RIGHTS)
        __pmNotifyErr(LOG_DEBUG,"cmsg_type != SCM_RIGHTS");	/* TODO */

    ioptr = (int *) CMSG_DATA(cmhp);
    memcpy(iofds, ioptr, sizeof(iofds));
    pdu->infd = iofds[0];
    pdu->outfd = iofds[1];

    return sts;
}
#else
int
__pmdaSendRootPDUStart(int fd, int status,
		int pdutype, int ipctype, int infd, int outfd,
		const char *label, int labellen, const char* argv, int argvlen)
{
    (void)fd;
    (void)status;
    (void)pdutype;
    (void)ipctype;
    (void)infd;
    (void)outfd;
    (void)label;
    (void)labellen;
    (void)argv;
    (void)argvlen;
    return -EOPNOTSUPP;
}
int
__pmdaRecvRootPDUStart(int fd, int type, void *buffer, int buflen)
{
    (void)fd;
    (void)type;
    (void)buffer;
    (void)buflen;
    return -EOPNOTSUPP;
}
#endif

/* Server decodes __pmdaRootPDUStart PDUs */
int
__pmdaDecodeRootPDUStart(void *buf, int blen, int *ipctype,
	int *infd, int *outfd, char *label, int labellen,
	char* argv, int argvlen)
{
    __pmdaRootPDUStart	*pdu = (__pmdaRootPDUStart *)buf;

/* TODO - labellen? argvlen? */
    /* TODO: check length */

    if (ipctype)
	*ipctype = pdu->ipctype;
    if (infd)
	*infd = pdu->infd;
    if (outfd)
	*outfd = pdu->outfd;
    if (argvlen) {
	strncpy(argv, pdu->argv, pdu->argvlen);
	argv[pdu->argvlen] = '\0';
    }
    if (labellen) {
	strncpy(label, pdu->label, pdu->labellen);
	label[pdu->labellen] = '\0';
    }
    return (sizeof(int) * 4) + pdu->labellen + pdu->argvlen;
}

/* PMCD sends __pmdaRootPDUStop PDUs */
int
__pmdaSendRootPDUStop(int fd, int pdutype, int status, int pid, int code, int force)
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
    return send(fd, (const char *)&pdu, sizeof(pdu), 0);
}

/* Server recvs __pmdaRootPDUStop PDUs */
int
__pmdaRecvRootPDUStop(int fd, int pdutype, void* buffer, int buflen)
{
//    __pmdaRootPDUStop	*pdu = (__pmdaRootPDUStop *)buffer;
//    int			minlength = sizeof(*pdu);
    int			sts;

    if ((sts = recv(fd, (char *)buffer, buflen, 0)) < 0)
	return -oserror();
    /* TODO:
    if (sts < minlength)
	return -EINVAL;
    if (pdu->hdr.type != pdutype)
	return -ESRCH;
    if (pdu->hdr.version > ROOT_PDU_VERSION)
	return -ENOTSUP;
    if (pdu->hdr.status != 0)
	return pdu->hdr.status;*/
    return sts;
}

/* Server decodes __pmdaRootPDUStop PDUs */
int
__pmdaDecodeRootPDUStop(void *buf, int buflen, int *pid, int *code, int *force)
{
    __pmdaRootPDUStop *pdu = (__pmdaRootPDUStop *)buf;

    /* TODO: check length */
    if (pid)
	*pid = pdu->pid;
    if (code)
	*code = pdu->code;
    if (force)
	*force = pdu->force;
    return sizeof(__pmdaRootPDUStop);
}
