/*
 * Copyright (c) 2012 Red Hat Inc.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * This file provides an abstraction layer for I/O operations used by pmcd.
 */
#include "pmcd.h"
#include "impl.h"
#include "io.h"

#if HAVE_NSS
#include <nspr.h>
#else
#include <stdlib.h>
#endif
#if defined(HAVE_SYS_UN_H)
#include <sys/un.h>
#endif

#if HAVE_NSS
void
ioInit(void)
{
    /* Initialize NSPR. */
    PR_Init (PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);
}

int
ioStringToNetAddr (const char *ipSpec, IpAddress *addr)
{
  PRStatus status = PR_StringToNetAddr (ipSpec, addr);
  return status == PR_SUCCESS ? 1 : 0;
}

int
ioInitializeNetAddr (int addrValue, int port, IpAddress *addr)
{
  PRStatus status;

  memset (addr, 0, sizeof(*addr));
  switch (addrValue) {
  case INADDR_ANY:
    status = PR_InitializeNetAddr (PR_IpAddrAny, port, addr);
    break;
  default:
    status = PR_FAILURE;
    break;
  }
  return status == PR_SUCCESS ? 1 : 0;
}

int
ioCreateSocket(void)
{
  #error __FUNCTION__ not implemented
}

int
ioSetClientSockopts (int fd, int port, IpAddress *ipAddr)
{
  #error __FUNCTION__ not implemented
}

int
ioBindClientSocket (int fd, int port, IpAddress *ipAddr)
{
  #error __FUNCTION__ not implemented
}

int
ioListen (int fd, int maxRequests)
{
  #error __FUNCTION__ not implemented
}

void
ioCloseSocket (int fd)
{
  #error __FUNCTION__ not implemented
}

void
ioFD_SET(int fd, FdSet *set)
{
  #error __FUNCTION__ not implemented
}

int
ioFD_ISSET(int fd, FdSet *set)
{
  #error __FUNCTION__ not implemented
}

void
ioFD_CLR(int fd, FdSet *set)
{
  #error __FUNCTION__ not implemented
}

void
ioFD_ZERO(FdSet *set)
{
  #error __FUNCTION__ not implemented
}

int
ioSelect(int nfds, FdSet *readfds, FdSet *writefds, FdSet *exceptfds, struct timeval *timeout)
{
  #error __FUNCTION__ not implemented
}

int
ioAccept (int socket, ClientAddr *address)
{
  #error __FUNCTION__ not implemented
}

int
ioGetPDU (int fd, int mode, int timeout, __pmPDU **result)
{
  #error __FUNCTION__ not implemented
}

int
ioSendError(int fd, int from, int code)
{
  #error __FUNCTION__ not implemented
}

int
ioSendResult(int, int, const pmResult *)
{
  #error __FUNCTION__ not implemented
}

int
ioAccAddClient(ClientAddr* addr, unsigned int *denyOpsResult)
{
  #error __FUNCTION__ not implemented
}

const char *
ioStrError(void)
{
  #error __FUNCTION__ not implemented
}

#else /* ! HAVE_NSS */

void
ioInit(void)
{
}

int
ioStringToNetAddr (const char *ipSpec, IpAddress *addr)
{
    int			i;
    const char		*sp = ipSpec;
    char		*endp;
    unsigned long	part;

    for (i = 0; i < 4; i++) {
        part = strtoul(sp, &endp, 10);
	if (*endp != ((i < 3) ? '.' : '\0'))
	    return 0;
	if (part > 255)
	    return 0;
	*addr |= part << (8 * (3 - i));
	if (i < 3)
	    sp = endp + 1;
    }
    *addr = htonl(*addr);
    return 1; /* success */
}

int
ioInitializeNetAddr (int addrValue, int port, IpAddress *addr)
{
  *addr = addrValue;
  return 1; /* Success */
}

int
ioCreateSocket(void)
{
  return __pmCreateSocket();
}

int
ioSetClientSockopts (int fd, int port, IpAddress *ipAddr)
{
    int one;
    /* Ignore dead client connections */
    one = 1;
#ifndef IS_MINGW
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		(mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, 0x%x) setsockopt(SO_REUSEADDR): %s\n",
		port, *ipAddr, ioStrError());
	return 0; /* failure */
    }
#else
    if (setsockopt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		(mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d,0x%x) setsockopt(EXCLUSIVEADDRUSE): %s\n",
		port, *ipAddr, ioStrError());
	return 0; /* failure */
    }
#endif

    /* and keep alive please - pv 916354 bad networks eat fds */
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one,
		(mysocklen_t)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, 0x%x) setsockopt(SO_KEEPALIVE): %s\n",
		port, *ipAddr, ioStrError());
	return 0; /* failure */
    }

    return 1; /* success */
}

int
ioBindClientSocket (int fd, int port, IpAddress *ipAddr)
{
    int	sts;
    struct sockaddr_in myAddr;

    memset(&myAddr, 0, sizeof(myAddr));
    myAddr.sin_family = AF_INET;
    myAddr.sin_addr.s_addr = *ipAddr;
    myAddr.sin_port = htons(port);
    sts = bind(fd, (struct sockaddr*)&myAddr, sizeof(myAddr));
    if (sts < 0) {
	sts = neterror();
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, 0x%x) bind: %s\n",
		port, *ipAddr, ioStrError());
	if (sts == EADDRINUSE)
	    __pmNotifyErr(LOG_ERR, "pmcd may already be running\n");
	return 0; /* failure */
    }

    return 1; /* success */
}

int
ioListen (int fd, int maxRequests)
{
  return listen(fd, maxRequests);
}

void
ioCloseSocket (int fd)
{
  __pmCloseSocket(fd);
}

void
ioFD_SET(int fd, FdSet *set)
{
  FD_SET(fd, set);
}

int
ioFD_ISSET(int fd, FdSet *set)
{
  return FD_ISSET(fd, set);
}

void
ioFD_CLR(int fd, FdSet *set)
{
  FD_CLR(fd, set);
}

void
ioFD_ZERO(FdSet *set)
{
  FD_ZERO(set);
}

int
ioSelect(int nfds, FdSet *readfds, FdSet *writefds, FdSet *exceptfds, struct timeval *timeout)
{
  return select(nfds, readfds, writefds, exceptfds, timeout);
}

int
ioAccept (int socket, ClientAddr *address)
{
  mysocklen_t addrlen = sizeof(*address);
  return accept(socket, (struct sockaddr *)address, &addrlen);
}

int
ioGetPDU (int fd, int mode, int timeout, __pmPDU **result)
{
  return __pmGetPDU(fd, mode, timeout, result);
}

int
ioSendError(int fd, int from, int code)
{
  return __pmSendError (fd, from, code);
}

int
ioSendResult(int fd, int from, const pmResult *result)
{
  return __pmSendResult (fd, from, result);
}

int
ioAccAddClient(ClientAddr* addr, unsigned int *denyOpsResult)
{
  return __pmAccAddClient (&addr->sin_addr, denyOpsResult);
}

const char *
ioStrError(void)
{
  return netstrerror();
}

#endif /* ! HAVE_NSS */
