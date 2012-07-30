/*
 * Copyright (c) 2012 Red Hat.  All Rights Reserved.
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

#ifndef PMIO_H
#define PMIO_H

#ifdef HAVE_NSS
#include <nspr.h>
#include <private/pprio.h>
#endif

/* SSL/TLS/IPv6 support via NSS/NSPR. */
#ifdef HAVE_NSS
/* Socket I/O types */
typedef PRNetAddr __pmSockAddr;
typedef PRNetAddr __pmSockAddrIn;
typedef PRNetAddr __pmInAddr;
typedef unsigned long __pmIPAddr;
typedef PRHostEnt __pmHostEnt;

/* Handling of sets of sockets */
typedef struct __pmFdSet
{
  size_t size;
  PRPollDesc *elements;
} __pmFdSet;
#else /* ! HAVE_NSS */
/* Socket I/O types */
typedef struct sockaddr __pmSockAddr;
typedef struct sockaddr_in __pmSockAddrIn;
typedef struct in_addr __pmInAddr;
typedef unsigned int __pmIPAddr;
typedef struct hostent __pmHostEnt;

/* Handling of sets of sockets */
typedef fd_set __pmFdSet;
#endif /* ! HAVE_NSS */

#endif /* PMIO_H */
