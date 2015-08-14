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
