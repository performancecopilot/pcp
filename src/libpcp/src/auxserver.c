/*
 * Copyright (c) 2013-2019 Red Hat.
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
#include <sys/stat.h>
#define SOCKET_INTERNAL
#include "internal.h"
#if defined(HAVE_GETPEERUCRED)
#include <ucred.h>
#endif
#ifdef HAVE_SYSTEMD_SD_DAEMON_H
#include <systemd/sd-daemon.h>
#endif

#define STRINGIFY(s)	#s
#define TO_STRING(s)	STRINGIFY(s)

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
    __pmServerPresence	*presence;	/* For advertising server presence on the network. */
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

/*
 * The unix domain socket we're willing to listen for clients on,
 * from -s (or env)
 */
static char *localSocketPath;
static int   localSocketFd = -EPROTO;
static const char *serviceSpec;

int
__pmServiceAddPorts(const char *service, int **ports, int nports)
{
    /*
     * The list of ports referenced by *ports may be (re)allocated
     * using calls to realloc(3) with a new size based on nports.
     * For an empty list, *ports must be NULL and nports must be 0.
     * It is the responsibility of the caller to free this memory.
     *
     * If -EOPNOTSUPP is not returned, then this function is
     * guaranteed to return a list containing at least 1 element.
     *
     * The service is a service name (e.g. pmcd).
     */
    if (strcmp(service, PM_SERVER_SERVICE_SPEC) == 0)
	nports = __pmPMCDAddPorts(ports, nports);
    else if (strcmp(service, PM_SERVER_PROXY_SPEC) == 0)
	nports = __pmProxyAddPorts(ports, nports);
    else if (strcmp(service, PM_SERVER_WEBAPI_SPEC) == 0)
	nports = __pmWebAPIAddPorts(ports, nports);
    else
	nports = -EOPNOTSUPP;

    return nports;
}

int
__pmPMCDAddPorts(int **ports, int nports)
{
    /*
     * The list of ports referenced by *ports may be (re)allocated
     * using calls to realloc(3) with a new size based on nports.
     * For an empty list, *ports must be NULL and nports must be 0.
     * It is the responsibility of the caller to free this memory.
     *
     * This function is guaranteed to return a list containing at least
     * 1 element.
     */
    char *env;
    int  new_nports = nports;

    PM_LOCK(__pmLock_extcall);
    if ((env = getenv("PMCD_PORT")) != NULL)		/* THREADSAFE */
	/*
	 * THREADSAFE because __pmAddPorts acquires no locks (other than
	 * on the fatal pmNoMem() path)
	 */
	new_nports = __pmAddPorts(env, ports, nports);		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    /*
     * Add the default port, if no new ports were added or if there was an
     * error.
     */
    if (new_nports <= nports)
	new_nports = __pmAddPorts(TO_STRING(SERVER_PORT), ports, nports);

    return new_nports;
}

int
__pmProxyAddPorts(int **ports, int nports)
{
    /*
     * The list of ports referenced by *ports may be (re)allocated
     * using calls to realloc(3) with a new size based on nports.
     * For an empty list, *ports must be NULL and nports must be 0.
     * It is the responsibility of the caller to free this memory.
     *
     * This function is guaranteed to return a list containing at least
     * 1 element.
     */
    char *env;
    int  new_nports = nports;

    PM_LOCK(__pmLock_extcall);
    if ((env = getenv("PMPROXY_PORT")) != NULL)		/* THREADSAFE */
	/*
	 * THREADSAFE because __pmAddPorts acquires no locks (other than
	 * on the fatal pmNoMem() path)
	 */
	new_nports = __pmAddPorts(env, ports, nports);		/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    /*
     * Add the default port, if no new ports were added or if there was an
     * error.
     */
    if (new_nports <= nports)
	new_nports = __pmAddPorts(TO_STRING(PROXY_PORT), ports, nports);

    return new_nports;
}

int
__pmWebAPIAddPorts(int **ports, int nports)
{
    /*
     * The list of ports referenced by *ports may be (re)allocated
     * using calls to realloc(3) with a new size based on nports.
     * For an empty list, *ports must be NULL and nports must be 0.
     * It is the responsibility of the caller to free this memory.
     *
     * This function is guaranteed to return a list containing at least
     * 1 element.
     */
    char *env;
    int  new_nports = nports;

    PM_LOCK(__pmLock_extcall);
    if ((env = getenv("PMWEBAPI_PORT")) != NULL)	/* THREADSAFE */
	/*
	 * THREADSAFE because __pmAddPorts acquires no locks (other than
	 * on the fatal pmNoMem() path)
	 */
	new_nports = __pmAddPorts(env, ports, nports);	/* THREADSAFE */
    PM_UNLOCK(__pmLock_extcall);

    /*
     * Add the default port, if no new ports were added or if there was an
     * error.
     */
    if (new_nports <= nports)
	new_nports = __pmAddPorts(TO_STRING(WEBAPI_PORT), ports, nports);

    return new_nports;
}

int
__pmAddPorts(const char *portstr, int **ports, int nports)
{
    /*
     * The list of ports referenced by *ports may be (re)allocated
     * using calls to realloc(3) with a new size based on nports.
     * For an empty list, *ports must be NULL and nports must be 0.
     * It is the responsibility of the caller to free this memory.
     *
     * If sufficient memory cannot be allocated, then this function
     * calls pmNoMem() and does not return.
     */
    char	*endptr, *p = (char *)portstr;
    size_t	size;

    /*
     * one (of possibly several) ports for client requests
     * ... accept a comma separated list of ports here
     */
    for ( ; ; ) {
	int port = (int)strtol(p, &endptr, 0);
	if ((*endptr != '\0' && *endptr != ',') || port < 0)
	    return -EINVAL;

	size = (nports + 1) * sizeof(int);
	if ((*ports = (int *)realloc(*ports, size)) == NULL)
	    pmNoMem("__pmAddPorts: cannot grow port list", size, PM_FATAL_ERR);
	(*ports)[nports++] = port;
	if (*endptr == '\0')
	    break;
	p = &endptr[1];
    }
    return nports;
}

int
__pmServerAddPorts(const char *ports)
{
    nport = __pmAddPorts(ports, &portlist, nport);
    return nport;
}

int
__pmServerAddInterface(const char *address)
{
    size_t size = (nintf+1) * sizeof(char *);
    char *intf;

    /* one (of possibly several) IP addresses for client requests */
    intflist = (char **)realloc(intflist, size);
    if (intflist == NULL)
	pmNoMem("AddInterface: cannot grow interface list", size, PM_FATAL_ERR);
    if ((intf = strdup(address)) == NULL)
	pmNoMem("AddInterface: cannot strdup interface", strlen(address), PM_FATAL_ERR);
    intflist[nintf++] = intf;
    return nintf;
}

void
__pmServerSetLocalSocket(const char *path)
{
    if (localSocketPath != NULL)
	free(localSocketPath);
    if (path != NULL && *path != '\0')
	localSocketPath = strdup(path);
    else
	localSocketPath = strdup(__pmPMCDLocalSocketDefault());
}

void
__pmServerSetServiceSpec(const char *spec)
{
    if (spec != NULL && *spec != '\0')
	serviceSpec = strdup(spec);
    else
	serviceSpec = PM_SERVER_SERVICE_SPEC;
}

static void
pidonexit(void)
{
    char        pidpath[MAXPATHLEN];

    if (serviceSpec) {
	pmsprintf(pidpath, sizeof(pidpath), "%s%c%s.pid",
	    pmGetConfig("PCP_RUN_DIR"), pmPathSeparator(), serviceSpec);
	unlink(pidpath);
    }
}

int
__pmServerCreatePIDFile(const char *spec, int verbose)
{
    char        pidpath[MAXPATHLEN];
    FILE        *pidfile;

    if (!serviceSpec)
	__pmServerSetServiceSpec(spec);

    pmsprintf(pidpath, sizeof(pidpath), "%s%c%s.pid",
	     pmGetConfig("PCP_RUN_DIR"), pmPathSeparator(), spec);

    if ((pidfile = fopen(pidpath, "w")) == NULL) {
	if (verbose)
	    fprintf(stderr, "Error: cannot open PID file %s\n", pidpath);
	return -oserror();
    }
    atexit(pidonexit);
    fprintf(pidfile, "%" FMT_PID, (pid_t)getpid());
#ifdef HAVE_FCHMOD
    (void)fchmod(fileno(pidfile), S_IRUSR | S_IRGRP | S_IROTH);
#else
    (void)chmod(pidpath, S_IRUSR | S_IRGRP | S_IROTH);
#endif
    fclose(pidfile);
    return 0;
}

void
__pmCheckAcceptedAddress(__pmSockAddr *addr)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /*
     * accept(3) doesn't set the peer address for unix domain sockets.
     * We need to do it ourselves. The address family
     * is set, so we can use it to test. There is only one unix domain socket
     * open, so we know its path.
     */
    if (__pmSockAddrGetFamily(addr) == AF_UNIX)
	__pmSockAddrSetPath(addr, localSocketPath);
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
	pmNoMem("GrowRequestPorts: can't grow request port array",
		need, PM_FATAL_ERR);
    }
}

/* Add a request port to the reqPorts array */
static int
AddRequestPort(const char *address, int port)
{
    ReqPortInfo	*rp;

    if (address == NULL && port == 0)
	address = "INADDR_LOOPBACK";
    else if (address == NULL)
	address = "INADDR_ANY";

    if (nReqPorts == szReqPorts)
	GrowRequestPorts();
    rp = &reqPorts[nReqPorts];
    rp->fds[INET_FD] = -1;
    rp->fds[IPV6_FD] = -1;
    rp->address = address;
    rp->port = port;
    rp->presence = NULL;
    nReqPorts++;

    if (pmDebugOptions.appl0)
        fprintf(stderr, "AddRequestPort: %s port %d\n", rp->address, rp->port);

    return nReqPorts;   /* success */
}

int
__pmServerSetupRequestPorts(void)
{
    int	i, n;

    /* check for duplicate ports, remove duplicates */
    for (i = 0; i < nport; i++) {
	for (n = i + 1; n < nport; n++) {
	    if (portlist[i] == portlist[n])
		break;
	}
	if (n < nport) {
	    pmNotifyErr(LOG_WARNING,
		"%s: duplicate client request port (%d) will be ignored\n",
                     pmGetProgname(), portlist[n]);
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
    return nport;
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
 * addresses from clients use address == "INADDR_ANY", or for localhost
 * access only use address == "INADDR_LOOPBACK".
 * On input, 'family' is a pointer to the address family to use (AF_INET,
 * AF_INET6) if the address specified is empty.  If the spec is not
 * empty then family is ignored and is set to the actual address family
 * used. 'family' must be initialized to AF_UNSPEC, in this case.
 */
static int
OpenRequestSocket(int port, const char *address, int *family,
		  int backlog, __pmFdSet *fdset, int *maximum)
{
    int			fd = -1;
    int			one, sts;
    int			net_sts;
    __pmSockAddr	*myAddr;
    int			isUnix = 0;
    int			try;
    struct timeval	tick = { 0, 250*1000 };		/* 250msec */
    char 		errmsg[PM_MAXERRMSGLEN];

    /*
     * Using this flag will eliminate the need for more conditional
     * compilation below, hopefully making the code easier to read and maintain.
     */
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (*family == AF_UNIX)
	isUnix = 1;
#endif

    if (isUnix) {
	if ((myAddr = __pmSockAddrAlloc()) == NULL) {
	    pmNoMem("OpenRequestSocket: can't allocate socket address",
		      sizeof(*myAddr), PM_FATAL_ERR);
	}

	/* Initialize the address. */
	__pmSockAddrSetFamily(myAddr, *family);
	__pmSockAddrSetPath(myAddr, address);

	/* Create the socket. */
	fd = __pmCreateUnixSocket();
    }
    else {
	/*
	 * If the address is unspecified, then use the address family we
	 * have been given, otherwise the family will be determined by
	 * __pmStringToSockAddr.
	 */
	if (address == NULL || strcmp(address, "INADDR_ANY") == 0) {
	    if ((myAddr = __pmSockAddrAlloc()) == NULL) {
		pmNoMem("OpenRequestSocket: can't allocate socket address",
			  sizeof(*myAddr), PM_FATAL_ERR);
	    }
	    __pmSockAddrInit(myAddr, *family, INADDR_ANY, 0);
	}
	else if (strcmp(address, "INADDR_LOOPBACK") == 0) {
	    if ((myAddr = __pmSockAddrAlloc()) == NULL) {
		pmNoMem("OpenRequestSocket: can't allocate socket address",
			  sizeof(*myAddr), PM_FATAL_ERR);
	    }
	    __pmSockAddrInit(myAddr, *family, INADDR_LOOPBACK, 0);
	}
	else {
	    if ((myAddr = __pmStringToSockAddr(address)) == NULL) {
		pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s) invalid address\n",
			      port, address);
		goto fail;
	    }
	    *family = __pmSockAddrGetFamily(myAddr);
	}
	__pmSockAddrSetPort(myAddr, port);

	/* Create the socket. */
	if (*family == AF_INET)
	    fd = __pmCreateSocket();
	else if (*family == AF_INET6)
	    fd = __pmCreateIPv6Socket();
	else {
	    pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s) invalid address family: %d\n",
			  port, address, *family);
	    goto fail;
	}
    }

    if (fd < 0) {
	pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmCreateSocket: %s\n",
		port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	goto fail;
    }

    /* Ignore dead client connections. */
    one = 1;
#ifndef IS_MINGW
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
		       (__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_ERR,
		      "OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(SO_REUSEADDR): %s\n",
		      port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	goto fail;
    }
#else
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char *)&one,
		       (__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_ERR,
		      "OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(EXCLUSIVEADDRUSE): %s\n",
		      port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	goto fail;
    }
#endif

    /* and keep alive please - bad networks eat fds */
    if (__pmSetSockOpt(fd, SOL_SOCKET, SO_KEEPALIVE, (char *)&one,
		(__pmSockLen)sizeof(one)) < 0) {
	pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) __pmSetSockOpt(SO_KEEPALIVE): %s\n",
		port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	goto fail;
    }

    /*
     * Attempts to make daemon restart (especially pmcd) have increased
     * the probability of trying to reuse a socket before the last use
     * has been completely torn down ... so be prepared to try this a
     * few times (4 x 250msec)
     */
    sts = -1;
    try = 0;
    while (sts < 0 && try < 4) {
	sts = __pmBind(fd, (void *)myAddr, __pmSockAddrSize());
	if (sts >= 0)
	    break;
	net_sts = neterror();
	if (pmDebugOptions.desperate)
	    fprintf(stderr, "OpenRequestSocket(%d, %s, %s) __pmBind try %d: %s\n",
		port, address, AddressFamily(*family), try+1, netstrerror_r(errmsg, sizeof(errmsg)));
	__pmtimevalSleep(tick);
	try++;
    }
    __pmSockAddrFree(myAddr);
    myAddr = NULL;
    if (sts < 0) {
	pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmBind: %s\n",
		port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	if (net_sts == EADDRINUSE)
	    pmNotifyErr(LOG_ERR, "%s may already be running\n", pmGetProgname());
	goto fail;
    }

    if (isUnix) {
	/*
	 * For unix domain sockets, grant rw access to the socket for all,
	 * otherwise, on linux platforms, connection will not be possible.
	 * This must be done AFTER binding the address. See Unix(7) for details.
	 */
	sts = chmod(address, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (sts != 0) {
	    pmNotifyErr(LOG_ERR,
		"OpenRequestSocket(%d, %s, %s) chmod(%s): %s\n",
		port, address, AddressFamily(*family), address, netstrerror_r(errmsg, sizeof(errmsg)));
	    goto fail;
	}
    }

    sts = __pmListen(fd, backlog);	/* Max. pending connection requests */
    if (sts < 0) {
	pmNotifyErr(LOG_ERR, "OpenRequestSocket(%d, %s, %s) __pmListen: %s\n",
		port, address, AddressFamily(*family), netstrerror_r(errmsg, sizeof(errmsg)));
	goto fail;
    }

    if (fd > *maximum)
	*maximum = fd;
    __pmFD_SET(fd, fdset);
    return fd;

fail:
    if (fd != -1) {
        __pmCloseSocket(fd);
	/* We must unlink the socket file. */
	if (isUnix)
	    unlink(address);
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
    int with_ipv6 = strcmp(pmGetAPIConfig("ipv6"), "true") == 0;

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo	*rp = &reqPorts[i];
	int		portsOpened = 0;

	/*
	 * If the spec is NULL or "INADDR_ANY" or "INADDR_LOOPBACK", then
	 * we open one socket for each address family (inet, IPv6).
	 * Otherwise, the address family will be determined by
	 * OpenRequestSocket.  Reporting of all errors is left to
	 * OpenRequestSocket to avoid doubling up.
	 */
	if (rp->address == NULL ||
	    strcmp(rp->address, "INADDR_ANY") == 0 ||
	    strcmp(rp->address, "INADDR_LOOPBACK") == 0) {
	    family = AF_INET;
	    if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					backlog, fdset, &maximum)) >= 0) {
	        rp->fds[INET_FD] = fd;
		++portsOpened;
		success = 1;
	    }
	    if (with_ipv6) {
		family = AF_INET6;
		if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					    backlog, fdset, &maximum)) >= 0) {
		    rp->fds[IPV6_FD] = fd;
		    ++portsOpened;
		    success = 1;
		}
	    }
	    else
		rp->fds[IPV6_FD] = -EPROTO;
	}
	else {
	    family = AF_UNSPEC;
	    if ((fd = OpenRequestSocket(rp->port, rp->address, &family,
					backlog, fdset, &maximum)) >= 0) {
	        if (family == AF_INET) {
		    rp->fds[INET_FD] = fd;
		    ++portsOpened;
		    success = 1;
		}
		else if (family == AF_INET6) {
		    rp->fds[IPV6_FD] = fd;
		    ++portsOpened;
		    success = 1;
		}
	    }
	}
	if (portsOpened > 0) {
	    /* Advertise our presence on the network, if requested. */
	    if (serviceSpec != NULL &&
		__pmServerHasFeature(PM_SERVER_FEATURE_DISCOVERY)) {
		rp->presence = __pmServerAdvertisePresence(serviceSpec,
							    rp->port);
	    }
	}
    }

#ifndef IS_MINGW
    /* Open a local unix domain socket, if specified, and supported. */
    if (localSocketPath != NULL) {
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	family = AF_UNIX;
	if ((localSocketFd = OpenRequestSocket(0, localSocketPath, &family,
					       backlog, fdset, &maximum)) >= 0) {
	    __pmServerSetFeature(PM_SERVER_FEATURE_UNIX_DOMAIN);
	    success = 1;
	}
#else
	pmNotifyErr(LOG_ERR, "%s: unix domain sockets are not supported\n",
		      pmGetProgname());
#endif
    }
#endif

    if (success)
	return maximum;

    pmNotifyErr(LOG_ERR, "%s: can't open any request ports, exiting\n",
		pmGetProgname());
    return -1;
}

int
__pmServerGetRequestPort(int index, const char **address, int *port)
{
    if (index < 0 || index >= nReqPorts)
	return -EINVAL;
    *address = reqPorts[index].address;
    *port = reqPorts[index].port;
    return 0;
}

int
__pmServerOpenRequestPorts(__pmFdSet *fdset, int backlog)
{
    int sts;

    if ((sts = __pmServerSetupRequestPorts()) < 0)
	return sts;
    return OpenRequestPorts(fdset, backlog);
}

void
__pmServerCloseRequestPorts(void)
{
    int i, fd;

    for (i = 0; i < nReqPorts; i++) {
	/* No longer advertise our presence on the network. */
	if (reqPorts[i].presence != NULL)
	    __pmServerUnadvertisePresence(reqPorts[i].presence);
	if ((fd = reqPorts[i].fds[INET_FD]) >= 0)
	    __pmCloseSocket(fd);
	if ((fd = reqPorts[i].fds[IPV6_FD]) >= 0)
	    __pmCloseSocket(fd);
    }
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (localSocketFd >= 0) {
        __pmCloseSocket(localSocketFd);
	localSocketFd = -EPROTO;

	/* We must remove the socket file. */
	if (unlink(localSocketPath) != 0 && oserror() != ENOENT) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    pmNotifyErr(LOG_ERR, "%s: can't unlink %s (uid=%d,euid=%d): %s",
			  pmGetProgname(), localSocketPath, getuid(), geteuid(),
			  osstrerror_r(errmsg, sizeof(errmsg)));
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
    if (localSocketFd >= 0)
	NewClient(fdset, localSocketFd, AF_UNIX);
#endif

    for (i = 0; i < nReqPorts; i++) {
	/* Check the inet and ipv6 fds. */
	if ((fd = reqPorts[i].fds[INET_FD]) >= 0)
	    NewClient(fdset, fd, AF_INET);
	if ((fd = reqPorts[i].fds[IPV6_FD]) >= 0)
	    NewClient(fdset, fd, AF_INET6);
    }
}

#if defined(HAVE_STRUCT_UCRED) || defined(HAVE_GETPEEREID) || defined(HAVE_GETPEERUCRED)
static int
SetCredentialAttrs(__pmHashCtl *attrs, unsigned int pid, unsigned int uid, unsigned int gid)
{
    char name[32], *namep;

    pmsprintf(name, sizeof(name), "%u", uid);
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
        __pmHashAdd(PCP_ATTR_USERID, namep, attrs);

    pmsprintf(name, sizeof(name), "%u", gid);
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
        __pmHashAdd(PCP_ATTR_GROUPID, namep, attrs);

    if (!pid)	/* not available on all platforms */
	return 0;

    pmsprintf(name, sizeof(name), "%u", pid);
    name[sizeof(name)-1] = '\0';
    if ((namep = strdup(name)) != NULL)
        __pmHashAdd(PCP_ATTR_PROCESSID, namep, attrs);

    return 0;
}
#endif

/*
 * Set local connection credentials into given hash structure
 */
int
__pmServerSetLocalCreds(int fd, __pmHashCtl *attrs)
{
#if defined(HAVE_STRUCT_UCRED)		/* Linux */
    struct ucred ucred;
    __pmSockLen length = sizeof(ucred);

    if (__pmGetSockOpt(fd, SOL_SOCKET, SO_PEERCRED, &ucred, &length) < 0)
	return -oserror();
    return SetCredentialAttrs(attrs, ucred.pid, ucred.uid, ucred.gid);

#elif defined(HAVE_GETPEEREID)		/* MacOSX */
    uid_t uid;
    gid_t gid;

    if (getpeereid(__pmFD(fd), &uid, &gid) < 0)
	return -oserror();
    return SetCredentialAttrs(attrs, 0, uid, gid);

#elif defined(HAVE_GETPEERUCRED)	/* Solaris */
    unsigned int uid, gid, pid;
    ucred_t *ucred = NULL;

    if (getpeerucred(__pmFD(fd), &ucred) < 0)
	return -oserror();
    pid = ucred_getpid(ucred);
    uid = ucred_geteuid(ucred);
    gid = ucred_getegid(ucred);
    ucred_free(ucred);
    return SetCredentialAttrs(attrs, pid, uid, gid);

#else
    return -EOPNOTSUPP;
#endif
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
	  "  === ==== ===== ====== =======\n", pmGetProgname());

    if (localSocketFd != -EPROTO)
	fprintf(stream, "  %-3s %4d %5s %-6s %s\n",
		(localSocketFd != -1) ? "ok" : "err",
		localSocketFd, "", "unix",
		localSocketPath);

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo *rp = &reqPorts[i];
	for (j = 0; j < FAMILIES; j++) {
	    if (rp->fds[j] != -EPROTO)
		fprintf(stream, "  %-3s %4d %5d %-6s %s\n",
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

    if (fd == localSocketFd) {
	pmsprintf(buffer, sz, "%s unix request socket %s",
		 pmGetProgname(), localSocketPath);
	return buffer;
    }

    for (i = 0; i < nReqPorts; i++) {
	ReqPortInfo *rp = &reqPorts[i];
	for (j = 0; j < FAMILIES; j++) {
	    if (fd == rp->fds[j]) {
		pmsprintf(buffer, sz, "%s %s request socket %s",
			pmGetProgname(), RequestFamilyString(j), rp->address);
		return buffer;
	    }
	}
    }
    return NULL;
}

unsigned int
__pmServerGetFeaturesFromPDU(__pmPDU *pb)
{
    int         version;
    int         challenge;
    __pmPDUInfo         pduinfo;
    int		sts;
    unsigned int server_features=0;

    version = __pmDecodeXtendError(pb, &sts, &challenge);

    if( version >= 0 && version == PDU_VERSION2  && sts >=0 ){
	pduinfo = __ntohpmPDUInfo(*(__pmPDUInfo *)&challenge);
	server_features = pduinfo.features;
    }
    
    return server_features;
}

static unsigned int server_features;

#if !defined(HAVE_SECURE_SOCKETS)

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == secureserver_lock || secureclient_lock
 * ... false because we're not building secure sockets
 */
int __pmIsSecureclientLock(void *lock) { return 0; }
int __pmIsSecureserverLock(void *lock) { return 0; }
#endif

/* nothing to initialize because we're not building secure sockets */
void init_secureclient_lock(void) { }
void init_secureserver_lock(void) { }

void __pmSecureConfigInit(void) { }

int
__pmGetSecureConfig(__pmSecureConfig *config)
{
    (void)config;
    return -EOPNOTSUPP;
}

void *
__pmSecureServerInit(__pmSecureConfig *tls)
{
    (void)tls;
    return NULL;
}

void
__pmFreeSecureConfig(__pmSecureConfig *config)
{
    (void)config;
}

int
__pmSecureServerIPCFlags(int fd, int flags, void *context)
{
    (void)fd;
    (void)flags;
    (void)context;
    return -EOPNOTSUPP;
}

void
__pmSecureServerShutdown(void *context, __pmSecureConfig *config)
{
    (void)context;
    (void)config;
}

int
__pmSecureServerHandshake(int fd, int flags, __pmHashCtl *attrs)
{
    (void)fd;

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:__pmSecureServerHandshake: flags=%d: ", __FILE__, flags);

    /* for things that require a secure server, return -EOPNOTSUPP */
    if ((flags & (PDU_FLAG_SECURE | PDU_FLAG_SECURE_ACK | PDU_FLAG_COMPRESS
		   | PDU_FLAG_AUTH)) != 0) {
	if (pmDebugOptions.auth)
	    fprintf(stderr, "not allowed\n");
	return -EOPNOTSUPP;
    }

    /*
     * CREDS_REQD is a special case that does not need a secure server
     * provided we've connected on a unix domain socket
     */
    if ((flags & PDU_FLAG_CREDS_REQD) != 0 && __pmHashSearch(PCP_ATTR_USERID, attrs) != NULL) {
	if (pmDebugOptions.auth)
	    fprintf(stderr, "ok\n");
	return 0;
    }
    /* remove all of the known good flags */
    flags &= ~(PDU_FLAG_SECURE | PDU_FLAG_SECURE_ACK | PDU_FLAG_COMPRESS |
	       PDU_FLAG_AUTH | PDU_FLAG_CREDS_REQD | PDU_FLAG_CONTAINER);
    if (!flags)
	return 0;

    /* any remaining flags are unexpected */
    if (pmDebugOptions.auth)
	fprintf(stderr, "bad\n");
    return PM_ERR_IPC;
}

/*
 * PM_SERVER_FEATURE_CERT_REQD is a special case that does not need a
 * secure server, all other server features are not supported
 */
int
__pmSecureServerHasFeature(__pmServerFeature query)
{
    if (query == PM_SERVER_FEATURE_CERT_REQD){
	return (server_features & (1 << query)) != 0;
    }
    return 0;
}

int
__pmSecureServerSetFeature(__pmServerFeature wanted)
{
    if (wanted == PM_SERVER_FEATURE_CERT_REQD){
        server_features |= (1 << wanted);
        return 1;
    }

    return 0;
}

int
__pmSecureServerClearFeature(__pmServerFeature clear)
{
    if (clear == PM_SERVER_FEATURE_CERT_REQD){
    	server_features &= ~(1<<clear);
	return 1;
    }
    return 0;
}

#endif /* !HAVE_SECURE_SOCKETS */

int
__pmServerClearFeature(__pmServerFeature clear)
{
    if (clear == PM_SERVER_FEATURE_DISCOVERY) {
	server_features &= ~(1<<clear);
	return 1;
    }
    return __pmSecureServerClearFeature(clear);
}

int
__pmServerSetFeature(__pmServerFeature wanted)
{
    if (wanted == PM_SERVER_FEATURE_LOCAL ||
	wanted == PM_SERVER_FEATURE_DISCOVERY ||
	wanted == PM_SERVER_FEATURE_CREDS_REQD ||
	wanted == PM_SERVER_FEATURE_UNIX_DOMAIN) {
	server_features |= (1 << wanted);
	return 1;
    }
    if (wanted == PM_SERVER_FEATURE_CONTAINERS) {
#if defined(HAVE_SETNS)
	server_features |= (1 << wanted);
	return 1;
#else
	return 0;
#endif
    }
    return __pmSecureServerSetFeature(wanted);
}

int
__pmServerHasFeature(__pmServerFeature query)
{
    int sts = 0;

    switch (query) {
    case PM_SERVER_FEATURE_IPV6:
	sts = (strcmp(pmGetAPIConfig("ipv6"), "true") == 0);
	break;
    case PM_SERVER_FEATURE_LOCAL:
    case PM_SERVER_FEATURE_DISCOVERY:
    case PM_SERVER_FEATURE_CONTAINERS:
    case PM_SERVER_FEATURE_CREDS_REQD:
    case PM_SERVER_FEATURE_UNIX_DOMAIN:
	if (server_features & (1 << query))
	    sts = 1;
	break;
    default:
	sts = __pmSecureServerHasFeature(query);
	break;
    }
    return sts;
}

#if !defined(IS_MINGW)
/* Based on Stevens (Unix Network Programming, p.83) */
void
__pmServerStart(int argc, char **argv, int flags)
{
    pid_t childpid;

    (void)argc; (void)argv;
    fflush(stdout);
    fflush(stderr);

#if defined(HAVE_TERMIO_SIGNALS)
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
#endif

    if ((childpid = fork()) < 0)
	pmNotifyErr(LOG_ERR, "__pmServerStart: fork");
	/* but keep going */
    else if (childpid > 0) {
	/* parent, let her exit, but avoid ugly "Log finished" messages */
	fclose(stderr);
	exit(0);
    }

    /* not a process group leader, lose controlling tty */
    if (setsid() == -1)
	pmNotifyErr(LOG_WARNING, "__pmServerStart: setsid");
	/* but keep going */

    if (flags & 1)
	close(0);
    /* don't close other fd's -- we know that only good ones are open! */
    /* don't chdir("/") -- we may still need to call pmOpenLog() */
}
#endif

#ifdef HAVE_SYSTEMD
/*
 * Portable sd_notify(). Some platforms use systemd but do not
 * readily have the devel headers/libs needed to call sd_notify,
 * so on those platforms send to a UNIX domain socket directly.
 * Returns negative on failure or >= 0 on success.
 */
static int
__pm_sd_notify(int clear, const char *notify_socket, const char *msg)
{
    int sts = 0;
    int fd = -1;

    if (notify_socket == NULL) {
    	/* no-op - warning issued by caller */
	return 0;
    }

#ifdef HAVE_SYSTEMD_SD_DAEMON_H
    /* use the devel library call and return it's status */
    sts = sd_notify(clear, msg);
    if (pmDebugOptions.services)
	fprintf(stderr, "__pm_sd_notify: using libsystemd:sd_notify(). NOTIFY_SOCKET=\"%s\". msg=\"%s\" sts=%d\n", 
		notify_socket, msg, sts);
    goto out;
#else

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    /* send the message via a UNIX domain socket */
    {
	struct sockaddr_un su = { 0 };
	struct iovec iov = { 0 };
	struct msghdr hdr = { 0 };
	int sendto_flags = 0;

	if (pmDebugOptions.services)
	    fprintf(stderr, "__pm_sd_notify: using AF_UNIX socket for notification\n");

	if ((strchr("@/", notify_socket[0])) == NULL || strlen(notify_socket) < 2) {
	    pmNotifyErr(LOG_ERR, "__pm_sd_notify: invalid notify_socket=\"%s\"\n", notify_socket);
	    sts = PM_ERR_GENERIC;
	    goto out;
	}

	if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) == -1) {
	    pmNotifyErr(LOG_ERR, "__pm_sd_notify: failed to create AF_UNIX socket: %s\n", strerror(errno));
	    sts = PM_ERR_GENERIC;
	    goto out;
	}

	su.sun_family = AF_UNIX;
	strncpy(su.sun_path, notify_socket, sizeof(su.sun_path)-1);
	su.sun_path[sizeof(su.sun_path)-1] = '\0';

	if (notify_socket[0] == '@')
	    su.sun_path[0] = '\0';

	iov.iov_base = (char *)msg;
	iov.iov_len = strlen(msg);

	hdr.msg_name = &su;
	hdr.msg_namelen = offsetof(struct sockaddr_un, sun_path) + strlen(notify_socket);
	hdr.msg_iov = &iov;
	hdr.msg_iovlen = 1;

	if (clear)
	    unsetenv("NOTIFY_SOCKET");
	sendto_flags |= MSG_NOSIGNAL; /* no sigpipes etc */
	if (sendmsg(fd, &hdr, sendto_flags) < 0) {
	    /* FAILED to send */
	    pmNotifyErr(LOG_ERR, "Error __pm_sd_notify: sendmsg \"%s\" failed: %s\n", msg, strerror(errno));
	    sts = PM_ERR_GENERIC;
	    goto out;
	}
    }
#else
    pmNotifyErr(LOG_ERR, "Warning __pm_sd_notify: failed, no sd_notify() or AF_UNIX sockets on this platform\n");
    sts = PM_ERR_NYI;
#endif /* HAVE_STRUCT_SOCKADDR_UN */
#endif /* HAVE_SYSTEMD_SD_DAEMON_H */

out:
    if (fd >= 0)
    	close(fd);
    return sts;
}
#endif /* HAVE_SYSTEMD */


#ifdef HAVE_SYSTEMD
/*
 * Notify the service manager of status. On platforms using systemd
 * AND the service unit is using Type=notify, then ${NOTIFY_SOCKET} will
 * be set in the environment and this call will notify the service manager
 * via that socket that the service is either READY or STOPPING, and provide
 * the pid of the main daemon process so that systemd can track it.
 * See sd_notify(3) for more details.
 *
 * On systemd platforms that are NOT using Type=notify, and on non-systemd
 * platforms, these calls do nothing. i.e. On platforms using systemd that
 * are NOT using Type=notify (e.g. if the service unit is Type=forking)
 * then ${NOTIFY_SOCKET} will NOT be set in the environment and calls to
 * __pmServerNotifySystemd() will do nothing (it's a cheap no-op that avoids
 * messy conditional code in the caller).  The same is true on non-systemd
 * platforms - these functions can safely be called unconditionally and
 * they will simply return PM_ERR_NYI, which can (and should be) ignored on
 * those non-systemd platforms.
 */
static int
__pmServerNotifySystemd(const char *msg)
{
    const char *notify_socket = getenv("NOTIFY_SOCKET"); /* may be NULL */
    int sts = 0;

    if (notify_socket) {
	sts = __pm_sd_notify(0, notify_socket, msg);
	if (pmDebugOptions.services)
	    fprintf(stderr, "__pmServerNotifySystemd: NOTIFY_SOCKET=\"%s\" msg=\"%s\" sts=%d\n",
		    notify_socket, msg, sts);
    } else {
	/*
	 * we were not launched by systemd ... this is sort of expected,
	 * e.g. pmlogger launched by record mode, stand alone logging or
	 * (especially) the QA suite
	 */
    	if (pmDebugOptions.services)
	    fprintf(stderr, "__pmServerNotifySystemd: NOTIFY_SOCKET not set:"
	    	    " not launched by systemd, or service unit is not using Type=notify\n");
	sts = PM_ERR_GENERIC;
    }
    /* negative return indicates error */
    return sts;
}
#endif /* HAVE_SYSTEMD */

int
__pmServerNotifyServiceManagerReady(pid_t pid)
{
#ifdef HAVE_SYSTEMD
    char msg[64];

    snprintf(msg, sizeof(msg), "READY=1\nMAINPID=%" FMT_PID, pid);
    return __pmServerNotifySystemd((const char *)msg);
#else
    /* no systemd - not fatal but log a debug message */
    if (pmDebugOptions.services)
	fprintf(stderr, "__pmServerNotifyServiceManagerReady: no service manager on this platform\n");
    return PM_ERR_NYI;
#endif
}

int
__pmServerNotifyServiceManagerStopping(pid_t pid)
{
#ifdef HAVE_SYSTEMD
    return __pmServerNotifySystemd("STOPPING=1");
#else
    /* no systemd - not fatal but log a debug message */
    if (pmDebugOptions.services)
	fprintf(stderr, "__pmServerNotifyServiceManagerStopping: no service manager on this platform\n");
    return PM_ERR_NYI;
#endif
}
