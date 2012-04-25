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
 * This header provides an abstraction layer for I/O operations.
 */

#ifndef _IO_H
#define _IO_H

#if HAVE_NSS
#include <nspr.h>

typedef PRNetAddr IpAddress;
typedef PR_??? FdSet;
typedef PR_NetAddr ClientAddr;

#else /* ! HAVE_NSS */
typedef __uint32_t IpAddress;
typedef fd_set FdSet;
typedef struct sockaddr_in ClientAddr;
typedef struct sockaddr_in myAddr;

#endif /* ! HAVE_NSS */

extern void ioInit(void);
extern int ioStringToNetAddr (const char *ipSpec, IpAddress *addr);
extern int ioInitializeNetAddr (int addrValue, int port, IpAddress *addr);
extern int ioCreateSocket(void);
extern int ioSetClientSockopts (int fd, int port, IpAddress *ipAddr);
extern int ioBindClientSocket (int fd, int port, IpAddress *ipAddr);
extern int ioListen (int fd, int maxRequests);
extern int ioSelect(int nfds, FdSet *readfds, FdSet *writefds, FdSet *exceptfds, struct timeval *timeout);
extern int ioAccept (int socket, ClientAddr *address);
extern int ioGetPDU (int fd, int mode, int timeout, __pmPDU **result);
extern void ioCloseSocket (int fd);
extern void ioFD_SET(int fd, FdSet *set);
extern int ioFD_ISSET(int fd, FdSet *set);
extern void ioFD_CLR(int fd, FdSet *set);
extern void ioFD_ZERO(FdSet *set);
extern int ioSendError(int fd, int from, int code);
extern int ioSendResult(int fd, int from, const pmResult *result);
extern int ioAccAddClient(ClientAddr* addr, unsigned int *denyOpsResult);
extern const char *ioStrError(void);

#endif /* _IO_H */
