/*
 * Copyright (c) 2013 Red Hat.
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

#include <sys/stat.h> 

#include "pmapi.h"
#include "impl.h"
#define SOCKET_INTERNAL
#include "internal.h"

/*
 * Info about a request port that clients may connect to a server on
 */
enum {
    INET_FD     = 0,
    IPV6_FD,
    FAMILIES
};
typedef struct {
    int			fds[FAMILIES];	/* Inet and IPv6 File descriptors */
    int			port;		/* Listening port */
    const char		*address;	/* Network address string (or NULL) */
} ReqPortInfo;

static unsigned         nReqPorts;		/* number of ports */
static unsigned         szReqPorts;		/* capacity of ports array */
static ReqPortInfo      *reqPorts;      	/* ports array */

/*
 * Interfaces we're willing to listen for clients on, from -i
 */
static int	nintf;
static char	**intflist;

/*
 * Ports we're willing to listen for clients on, from -p (or env)
 */
static int	nport;
static int	*portlist;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
/*
 * The unix domain socket we're willing to listen for clients on, from -s (or env)
 */
static char *localSocketPath;
static int   localSocketFd = -EPROTO;
#endif

int
__pmServerAddInterface(const char *address)
{
    size_t size = (nintf+1) * sizeof(char *);

    /* one (of possibly several) IP addresses for client requests */
    intflist = (char **)realloc(intflist, nintf * sizeof(char *));
    if (intflist == NULL)
	__pmNoMem("AddInterface: cannot grow interface list", size, PM_FATAL_ERR);
    intflist[nintf++] = strdup(address);
    return nintf;
}

int
__pmServerAddPorts(const char *ports)
{
    char	*endptr, *p = (char *)ports;

    /*
     * one (of possibly several) ports for client requests
     * ... accept a comma separated list of ports here
     */
    for ( ; ; ) {
	size_t	size = (nport + 1) * sizeof(int);
	int	port = (int)strtol(p, &endptr, 0);

	if ((*endptr != '\0' && *endptr != ',') || port < 0)
	    return -EINVAL;
	if ((portlist = (int *)realloc(portlist, size)) == NULL)
	    __pmNoMem("AddPorts: cannot grow port list", size, PM_FATAL_ERR);
	portlist[nport++] = port;
	if (*endptr == '\0')
	    break;
	p = &endptr[1];
    }
    return nport;
}

void
__pmServerSetLocalSocket(const char *path)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (path != NULL)
	localSocketPath = strdup(path);
    else
	localSocketPath = NULL;
#endif
}

/* Increase the capacity of the reqPorts array (maintain the contents) */
static void
GrowRequestPorts(void)
{
    size_t	need;

    szReqPorts += 4;
    need = szReqPorts * sizeof(ReqPortInfo);
    reqPorts = (ReqPortInfo*)realloc(reqPorts, need);
    if (reqPorts == NULL) {
	__pmNoMem("GrowRequestPorts: can't grow request port array",
		need, PM_FATAL_ERR);
    }
}

/* Add a request port to the reqPorts array */
static int
AddRequestPort(const char *address, int port)
{
    ReqPortInfo	*rp;

    if (address == NULL)
	address = "INADDR_ANY";

    if (nReqPorts == szReqPorts)
	GrowRequestPorts();
    rp = &reqPorts[nReqPorts];
    rp->fds[INET_FD] = -1;
    rp->fds[IPV6_FD] = -1;
    rp->address = address;
    rp->port = port;
    nReqPorts++;

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_APPL0)
        fprintf(stderr, "AddRequestPort: %s port %d\n", rp->address, rp->port);
#endif

    return nReqPorts;   /* success */
}

static int
SetupRequestPorts(void)
{
    int	i, n;

    /* check for duplicate ports, remove duplicates */
    for (i = 0; i < nport; i++) {
	for (n = i + 1; n < nport; n++) {
	    if (portlist[i] == portlist[n])
		break;
	}
	if (n < nport) {
	    __pmNotifyErr(LOG_WARNING,
		"%s: duplicate client request port (%d) will be ignored\n",
                     pmProgname, portlist[n]);
	    portlist[n] = -1;
	}
    }

    if (nintf == 0) {
	/* no interface options specified, allow connections on any address */
	for (n = 0; n < nport; n++) {
	    if (portlist[n] != -1)
		AddRequestPort(NULL, portlist[n]);
	}
    }
    else {
	for (i = 0; i < nintf; i++) {
	    for (n = 0; n < nport; n++) {
		if (portlist[n] == -1 || intflist[i] == NULL)
		    continue;
		AddRequestPort(intflist[i], portlist[n]);
	    }
	}
    }
    return 0;
}

static const char *
AddressFamily(int family)
{
    if (family ==  AF_INET)
	return "inet";
    if (family == AF_INET6)
	return "ipv6";
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (family == AF_UNIX)
	return "unix";
#endif
    return "unknown";
}

/*
 * Create socket for incoming connections and bind to it an address for
 * clients to use.  Returns -1 on failure.
 *
 * If '*family' is AF_UNIX and unix domain sockets are supported:
 * 'port' is ignored and 'address' is the path to the socket file in the filesystem.
 *
 * Otherwise:
 * address is a string representing the Inet/IPv6 address that the port
 * is advertised for.  To allow connections to all this host's internet
 * addresses from clients use address = "INADDR_ANY".
 * On input, 'family' is a pointer to the address family to use (AF_INET,
 * AF_INET6) if the address specified is empty.  If the spec is not
 * empty then family is ignored and is set to the actual address family
 * used.
 */
static int
OpenRequestSocket(int port, const char *address, int *family,
		  int backlog, __pmFdSet *fdset, int *maximum)
{
    int			fd = -1;
    int			one, sts;
    __pmSockAddr	*myAddr;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (*family == AF_UNIX) {
	if ((myAddr = __pmSockAddrAlloc()) == NULL) {
	    __pmNoMem("OpenRequestSocket: can't allocate socket address",
		      sizeof(*myAddr), PM_FATAL_ERR);
	}

	/* Initialize the address. */
	__pmSockAddrSetFamily(myAddr, *family);
	__pmSockAddrSetPath(myAddr, address);

	/* Create the socket. */
	fd = __pmCreateUnixSocket();
    }
    else {
#endif
	if ((myAddr = __pmStringToSockAddr(address)) == NULL) {
	    __pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s) invalid address\n",
			  port, address);
	    goto fail;
	}

	/*
	 * If the address is unspecified, then use the address family we
	 * have been given, otherwise the family will be determined by
	 * __pmStringToSockAddr.
	 */
	if (address == NULL || strcmp(address, "INADDR_ANY") == 0)
	    __pmSockAddrSetFamily(myAddr, *family);
	else
	    *family = __pmSockAddrGetFamily(myAddr);
	__pmSockAddrSetPort(myAddr, port);

	/* Create the socket. */
	if (*family == AF_INET)
	    fd = __pmCreateSocket();
	else if (*family == AF_INET6)
	    fd = __pmCreateIPv6Socket();
	else {
	    __pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s) invalid address family: %d\n",
			  port, address, *family);
	    goto fail;
	}
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    }
#endif

    if (fd < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmCreateSocket: %s\n",
		port, address, AddressFamily(*family), netstrerror());
	goto fail;
    }

    /* Ignore dead client connections */
    one = 1;
#ifndef IS_MINGW
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(SO_REUSEADDR): %s\n",
		port, address, AddressFamily(*family), netstrerror());
	goto fail;
    }
#else
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(EXCLUSIVEADDRUSE): %s\n",
		port, address, AddressFamily(*family), netstrerror());
	goto fail;
    }
#endif

    /* and keep alive please - bad networks eat fds */
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	__pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(SO_KEEPALIVE): %s\n",
		port, address, AddressFamily(*family), netstrerror());
	goto fail;
    }

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (*family == AF_UNIX) {
	/* For unix domain sockets, unlink() the socket path before binding. If it
	   exists, then it is either 1) stray and will be deleted or 2) in use
	   and the unlink will (correctly) fail to remove it. We will then (correctly)
	   receive EADDRINUSE. */
	unlink(address);
    }
#endif

    sts = __pmBind(fd, (void *)myAddr, __pmSockAddrSize());
    __pmSockAddrFree(myAddr);
    myAddr = NULL;
    if (sts < 0) {
	sts = neterror();
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmBind: %s\n",
		port, address, AddressFamily(*family), netstrerror());
	if (sts == EADDRINUSE)
	    __pmNotifyErr(LOG_ERR, "%s may already be running\n", pmProgname);
	goto fail;
    }

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (*family == AF_UNIX) {
	/* For unix domain sockets, grant rw access to the socket for all,
	   otherwise, on linux platforms, connection will not be possible.
	   This must be done AFTER binding the address. See Unix(7) for details. */
	sts = chmod(address, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (sts != 0) {
	    __pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) chmod(%s): %s\n",
		port, address, AddressFamily(*family), address, netstrerror());
	    goto fail;
	}

	/* For unix domain sockets, set the SO_PASSCRED option to allow user credentials
	   to be authenticated. This must be done AFTER binding the address. Otherwise,
	   on modern linux platforms, the socket will be auto-bound to a name in the
	   abstract namespace. See Unix(7) for details. */
	if (__pmSetSockOpt(fd, SOL_SOCKET, SO_PASSCRED, (char *)&one,
	    (__pmSockLen)sizeof(one)) < 0) {
	    __pmNotifyErr(LOG_ERR,
	        "OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(SO_PASSCRED): %s\n",
	        port, address, AddressFamily(*family), netstrerror());
	    goto fail;
        }
    }
#endif

    sts = __pmListen(fd, backlog);	/* Max. pending connection requests */
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmListen: %s\n",
		port, address, AddressFamily(*family), netstrerror());
	goto fail;
    }

    if (fd > *maximum)
	*maximum = fd;
    __pmFD_SET(fd, fdset);
    return fd;

fail:
    if (fd != -1) {
        __pmCloseSocket(fd);
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	/* We must unlink the socket file. */
	if (*family == AF_UNIX)
	    unlink(address);
#endif
    }
    if (myAddr)
        __pmSockAddrFree(myAddr);
    return -1;
}

/*
 * Open request ports for client connections.
 * For each request port, open an inet and IPv6 (if supported) socket
 * for clients. Also open a local unix domain socket, if it has been
 * specified
 */
static int
OpenRequestPorts(__pmFdSet *fdset, int backlog)
{
    int i, fd, family, success = 0, maximum = -1;
    int with_ipv6 = strcmp(__pmGetAPIConfig("ipv6"), "true") == 0;

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo	*rp = &reqPorts[i];

	/*
	 * If the spec is NULL or "INADDR_ANY", then we open one socket
	 * for each address family (inet, IPv6).  Otherwise, the address
	 * family will be determined by OpenRequestSocket.  Reporting of
	 * all errors is left to OpenRequestSocket to avoid doubling up.
	 */
	if (rp->address == NULL || strcmp(rp->address, "INADDR_ANY") == 0) {
	    family = AF_INET;
	    if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					backlog, fdset, &maximum)) >= 0) {
	        rp->fds[INET_FD] = fd;
		success = 1;
	    }
	    if (with_ipv6) {
		family = AF_INET6;
		if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					    backlog, fdset, &maximum)) >= 0) {
		    rp->fds[IPV6_FD] = fd;
		    success = 1;
		}
	    }
	    else
		rp->fds[IPV6_FD] = -EPROTO;
	}
	else {
	    if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					backlog, fdset, &maximum)) >= 0) {
	        if (family == AF_INET) {
		    rp->fds[INET_FD] = fd;
		    success = 1;
		}
		else if (family == AF_INET6) {
		    rp->fds[IPV6_FD] = fd;
		    success = 1;
		}
	    }
	}
    }

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /* Open a local unix domain socket, if specified. */
    if (localSocketPath != NULL) {
	family = AF_UNIX;
	if ((fd = OpenRequestSocket(0, localSocketPath, &family,
				    backlog, fdset, &maximum)) >= 0) {
	    localSocketFd = fd;
	    success = 1;
	}
	else
	    localSocketFd = -1;
    }
#endif

    if (success)
	return maximum;

    __pmNotifyErr(LOG_ERR, "%s: can't open any request ports, exiting\n",
		pmProgname);
    return -1;
}

int
__pmServerOpenRequestPorts(__pmFdSet *fdset, int backlog)
{
    int sts;

    if ((sts = SetupRequestPorts()) < 0)
	return sts;
    return OpenRequestPorts(fdset, backlog);
}

void
__pmServerCloseRequestPorts(void)
{
    int i, fd;

    for (i = 0; i < nReqPorts; i++) {
	if ((fd = reqPorts[i].fds[INET_FD]) >= 0)
	    __pmCloseSocket(fd);
	if ((fd = reqPorts[i].fds[IPV6_FD]) >= 0)
	    __pmCloseSocket(fd);
    }
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (localSocketFd >= 0) {
        __pmCloseSocket(localSocketFd);

	/* We must remove the socket file. */
	i = unlink(localSocketPath);
	if (i != 0) {
	    __pmNotifyErr(LOG_ERR, "%s: can't unlink %s. errno==%d: %s",
			  pmProgname, localSocketPath, errno, strerror(errno));
	    __pmNotifyErr(LOG_ERR, "%s: uid==%d euid==%d\n",
			  pmProgname, getuid(), geteuid());
	}
    }
#endif
}

/*
 * Accept any new client connections
 */
void
__pmServerAddNewClients(__pmFdSet *fdset, __pmServerCallback NewClient)
{
    int i, fd;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /* Check the local unix domain fd. */
    if (localSocketFd >= 0) {
	NewClient(fdset, localSocketFd);
    }
#endif

    for (i = 0; i < nReqPorts; i++) {
	/* Check the inet and ipv6 fds. */
	if ((fd = reqPorts[i].fds[INET_FD]) >= 0)
	    NewClient(fdset, fd);
	if ((fd = reqPorts[i].fds[IPV6_FD]) >= 0)
	    NewClient(fdset, fd);
    }
}

static const char *
RequestFamilyString(int index)
{
    if (index == INET_FD)
	return "inet";
    if (index == IPV6_FD)
	return "ipv6";
    return "unknown";
}

void
__pmServerDumpRequestPorts(FILE *stream)
{
    int	i, j;

    fprintf(stream, "%s request port(s):\n"
	  "  sts fd   port  family address\n"
	  "  === ==== ===== ====== =======\n", pmProgname);

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (localSocketFd != -EPROTO)
	fprintf(stderr, "  %-3s %4d %5s %-6s %s\n",
		(localSocketFd != -1) ? "ok" : "err",
		localSocketFd, "", "unix",
		localSocketPath);
#endif

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo *rp = &reqPorts[i];
	for (j = 0; j < FAMILIES; j++) {
	    if (rp->fds[j] != -EPROTO)
		fprintf(stderr, "  %-3s %4d %5d %-6s %s\n",
		    (rp->fds[j] != -1) ? "ok" : "err",
		    rp->fds[j], rp->port, RequestFamilyString(j),
		    rp->address ? rp->address : "(any address)");
	}
    }
}

char *
__pmServerRequestPortString(int fd, char *buffer, size_t sz)
{
    int i, j;

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (fd == localSocketFd) {
	snprintf(buffer, sz, "%s unix request socket %s",
		 pmProgname, localSocketPath);
	return buffer;
    }
#endif

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo *rp = &reqPorts[i];
	for (j = 0; j < FAMILIES; j++) {
	    if (fd == rp->fds[j]) {
		snprintf(buffer, sz, "%s %s request socket %s",
			pmProgname, RequestFamilyString(j), rp->address);
		return buffer;
	    }
	}
    }
    return NULL;
}

#if !defined(HAVE_SECURE_SOCKETS)

int
__pmSecureServerSetup(const char *db, const char *passwd)
{
    (void)db;
    (void)passwd;
    return 0;
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    (void)fd;
    (void)flags;
    return -EOPNOTSUPP;
}

void
__pmSecureServerShutdown(void)
{
    /* nothing to do here */
}

int
__pmSecureServerHandshake(int fd, int flags, __pmHashCtl *attrs)
{
    (void)fd;
    (void)flags;
    (void)attrs;
    return -EOPNOTSUPP;
}

int
__pmServerHasFeature(__pmServerFeature query)
{
    if (query == PM_SERVER_FEATURE_IPV6)
	return (strcmp(__pmGetAPIConfig("ipv6"), "true") == 0);
    return 0;
}

#endif /* !HAVE_SECURE_SOCKETS */
