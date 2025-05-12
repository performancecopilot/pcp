/*
 * Copyright (c) 2012-2015 Red Hat.
 * Copyright (c) 2000,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 *
 */

#include "pmapi.h"
#include "libpcp.h"
#include "internal.h"

#include <limits.h>
#include <sys/stat.h>

/*
 * Return timeout (in seconds) to be used when pmlc is communicating
 * with pmlogger ... used externally from pmlc and internally from
 * __pmConnectLogger() and __pmControlLog()
 */
int
__pmLoggerTimeout(void)
{
    static int		timeout = TIMEOUT_NEVER;
    static int		done_default = 0;

    PM_LOCK(__pmLock_extcall);
    if (!done_default) {
	char	*timeout_str;
	char	*end_ptr;
	timeout_str = getenv("PMLOGGER_REQUEST_TIMEOUT");	/* THREADSAFE */
	done_default = 1;
	if (timeout_str != NULL) {
	    /*
	     * Only a positive integer (the unit is seconds) is OK
	     */
	    timeout = strtol(timeout_str, &end_ptr, 10);
	    if (*end_ptr != '\0' || timeout < 0) {
		char	*fromenv = strdup(timeout_str);
		timeout = TIMEOUT_NEVER;
		PM_UNLOCK(__pmLock_extcall);
		pmNotifyErr(LOG_WARNING,
			      "ignored bad PMLOGGER_REQUEST_TIMEOUT = '%s'\n",
			      fromenv);
		free(fromenv);
	    }
	    else
		PM_UNLOCK(__pmLock_extcall);
	}
	else
	    PM_UNLOCK(__pmLock_extcall);
    }
    else
	PM_UNLOCK(__pmLock_extcall);

    return timeout;
}

#if defined(HAVE_STRUCT_SOCKADDR_UN)
/*
 * Return the path to the default PMLOGGER local unix domain socket.
 * in the buffer propvided.
 * Return the path regardless of whether unix domain sockets are
 * supported by our build. Other functions can then print reasonable
 * messages if an attempt is made to use one.
 */
const char *
__pmLogLocalSocketDefault(int pid, char *buf, size_t bufSize)
{
    /* pmsprintf guarantees a terminating nul, even if the output is truncated. */
    if (pid == PM_LOG_PRIMARY_PID) { /* primary */
	pmsprintf(buf, bufSize, "%s/pmlogger.primary.socket",
		 pmGetConfig("PCP_RUN_DIR"));
    }
    else {
	pmsprintf(buf, bufSize, "%s/pmlogger.%d.socket",
		 pmGetConfig("PCP_RUN_DIR"), pid);
    }

    return buf;
}

/*
 * Return the path to the user's own PMLOGGER local unix domain socket
 * in the buffer provided.
 * Return the path regardless of whether unix domain sockets are
 * supported by our build. Other functions can then print reasonable
 * messages if an attempt is made to use one.
 */
const char *
__pmLogLocalSocketUser(int pid, char *buf, size_t bufSize)
{
    char home[MAXPATHLEN];
    char *homeResult;

    homeResult = __pmHomedirFromID(getuid(), home, sizeof(home));
    if (homeResult == NULL)
	return NULL;

    /* pmsprintf guarantees a terminating nul, even if the output is truncated. */
    pmsprintf(buf, bufSize, "%s/.pcp/run/pmlogger.%d.socket",
	     homeResult, pid);

    return buf;
}
#endif

/*
 * Common function for attempting connections to pmlogger.
 */
static int
connectLogger(int fd, __pmSockAddr *myAddr)
{
    /* Attempt the connection. */
    int sts = __pmConnect(fd, myAddr, __pmSockAddrSize());

    /* Successful connection? */
    if (sts >= 0)
	return sts;

    sts = neterror();
    if (sts == EINPROGRESS) {
	/* We're in progress - wait on select. */
	struct timeval stv = { 0, 000000 };
	struct timeval *pstv;
	__pmFdSet rfds;
	int rc;
	stv.tv_sec = __pmLoggerTimeout();
	pstv = stv.tv_sec ? &stv : NULL;

	__pmFD_ZERO(&rfds);
	__pmFD_SET(fd, &rfds);
	if ((rc = __pmSelectRead(fd+1, &rfds, pstv)) == 1) {
	    sts = __pmConnectCheckError(fd);
	}
	else if (rc == 0) {
	    sts = ETIMEDOUT;
	}
	else {
	    sts = (rc < 0) ? neterror() : EINVAL;
	}
    }
    sts = -sts;

    /* Successful connection? */
    if (sts >= 0)
	return sts;

    /* Unsuccessful connection. */
    __pmCloseSocket(fd);
    return sts;
}

#if defined(HAVE_STRUCT_SOCKADDR_UN)
/*
 * Attempt connection to pmlogger via a local socket.
 */
static int
connectLoggerLocal(const char *local_socket)
{
    char		socket_path[MAXPATHLEN];
    int			fd;
    int			sts;
    __pmSockAddr	*myAddr;

    /* Create a socket */
    fd = __pmCreateUnixSocket();
    if (fd < 0)
	return -ECONNREFUSED;

    /* Set up the socket address. */
    myAddr = __pmSockAddrAlloc();
    if (myAddr == NULL) {
	pmNotifyErr(LOG_ERR, "__pmConnectLogger: out of memory\n");
	__pmCloseSocket(fd);
	return -ENOMEM;
    }
    __pmSockAddrSetFamily(myAddr, AF_UNIX);

    /*
     * Set the socket path. All socket paths are absolute, but strip off any redundant
     * initial path separators.
     * pmsprintf is guaranteed to add a nul byte.
     */
    while (*local_socket == pmPathSeparator())
	++local_socket;
    pmsprintf(socket_path, sizeof(socket_path), "%c%s", pmPathSeparator(), local_socket);
    __pmSockAddrSetPath(myAddr, socket_path);

    /* Attempt to connect */
    sts = connectLogger(fd, myAddr);
    __pmSockAddrFree(myAddr);

    if (sts < 0) {
	__pmCloseSocket(fd);
	return sts;
    }

    return fd;
}
#endif

/*
 * Determine how to connect based on connectionSpec, pid and port:
 *
 * If hostname is "local:[//][path]", then try the socket at
 * /path, if specified and the socket at PCP_RUN_DIR/pmlogger.<pid>.socket otherwise,
 * where <pid> is "primary" if pid is PM_LOG_PRIMARY_PID.
 * If this fails then set connectionSpec to "localhost" and then
 *
 * ConnectionSpec is a host name.
 * If port is set, use hostname:port, otherwise
 *
 * Use hostname+pid to find port, assuming pmcd is running there
 */
int
__pmConnectLogger(const char *connectionSpec, int *pid, int *port)
{
    int			n, sts = 0;
    __pmLogPort		*lpp;
    int			fd;	/* Fd for socket connection to pmcd */
    __pmPDU		*pb;
    __pmPDUHdr		*php;
    int			pinpdu;
    __pmHostEnt		*servInfo;
    __pmSockAddr	*myAddr;
    void		*enumIx;
    const char		*prefix_end;
    size_t		prefix_len;
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    char		path[MAXPATHLEN];
#endif
    int			originalPid;
    int			wasLocal;

    if (pmDebugOptions.pmlc)
	fprintf(stderr, "__pmConnectLogger(host=%s, pid=%d, port=%d)\n",
		connectionSpec, *pid, *port);

    if (*pid == PM_LOG_NO_PID && *port == PM_LOG_PRIMARY_PORT) {
	/*
	 * __pmLogFindPort and __pmLogLocalSocketDefault can only lookup
	 * based on pid, so xlate the request
	 */
	*pid = PM_LOG_PRIMARY_PID;
	*port = PM_LOG_NO_PORT;
    }

    /*
     * If the prefix is "local:[path]", we may try the connection more than once using
     * "unix:[path]" followed by "localhost".
     */
    for (originalPid = *pid; /**/; *pid = originalPid) {
	fd = -1;
	/* Look for a "local:" or a "unix:" prefix. */
	wasLocal = 0;
	prefix_end = strchr(connectionSpec, ':');
	if (prefix_end != NULL) {
	    prefix_len = prefix_end - connectionSpec + 1;
	    if ((wasLocal = (prefix_len == 6 && strncmp(connectionSpec, "local:", prefix_len) == 0)) ||
		(prefix_len == 5 && strncmp(connectionSpec, "unix:", prefix_len) == 0)) {
#if defined(HAVE_STRUCT_SOCKADDR_UN)
		if (connectionSpec[prefix_len] != '\0') {
		    /* Try the specified local socket directly. */
		    fd = connectLoggerLocal(connectionSpec + prefix_len);
		}
		else if (*pid != PM_LOG_NO_PID) {
		    /* Try the socket indicated by the pid. */
		    connectionSpec = __pmLogLocalSocketDefault(*pid, path, sizeof(path));
		    fd = connectLoggerLocal(connectionSpec);
		    if (fd < 0) {
			/* Try the socket in the user's home directory. */
			connectionSpec = __pmLogLocalSocketUser(*pid, path, sizeof(path));
			if (connectionSpec != NULL)
			    fd = connectLoggerLocal(connectionSpec);
		    }
		}
#endif
	    }
	    if (fd >= 0)
		sts = 0;
	    else
		sts = fd;
	}
	else {
	    /*
	     * If not a url, then connectionSpec is a host name.
	     *
	     * Catch pid == PM_LOG_ALL_PIDS ... this tells __pmLogFindPort
	     * to get all ports
	     */
	    if (*pid == PM_LOG_ALL_PIDS) {
		if (pmDebugOptions.pmlc)
		    fprintf(stderr, "__pmConnectLogger: pid == PM_LOG_ALL_PIDS makes no sense here\n");
		return -ECONNREFUSED;
	    }

	    if (*port == PM_LOG_NO_PORT) {
		if ((n = __pmLogFindPort(connectionSpec, *pid, &lpp)) < 0) {
		    if (pmDebugOptions.pmlc) {
			char	errmsg[PM_MAXERRMSGLEN];
			fprintf(stderr, "__pmConnectLogger: __pmLogFindPort: %s\n", pmErrStr_r(n, errmsg, sizeof(errmsg)));
		    }
		    return n;
		}
		else if (n != 1) {
		    if (pmDebugOptions.pmlc)
			fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> 1, cannot contact pmlogger\n");
		    return -ECONNREFUSED;
		}
		*port = lpp->port;
		if (pmDebugOptions.pmlc)
		    fprintf(stderr, "__pmConnectLogger: __pmLogFindPort -> pid = %d\n", lpp->port);
	    }

	    if ((servInfo = __pmGetAddrInfo(connectionSpec)) == NULL) {
		if (pmDebugOptions.pmlc) {
		    const char	*errmsg;
		    PM_LOCK(__pmLock_extcall);
		    errmsg = hoststrerror();		/* THREADSAFE */
		    fprintf(stderr, "__pmConnectLogger: __pmGetAddrInfo: %s\n",
			    errmsg);
		    PM_UNLOCK(__pmLock_extcall);
		}
		return -EHOSTUNREACH;
	    }

	    /*
	     * Loop over the addresses resolved for this host name until one of them
	     * connects.
	     */
	    enumIx = NULL;
	    for (myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx);
		 myAddr != NULL;
		 myAddr = __pmHostEntGetSockAddr(servInfo, &enumIx)) {
		/* Create a socket */
		if (__pmSockAddrIsInet(myAddr))
		    fd = __pmCreateSocket();
		else if (__pmSockAddrIsIPv6(myAddr))
		    fd = __pmCreateIPv6Socket();
		else {
		    pmNotifyErr(LOG_ERR, 
				  "__pmConnectLogger : invalid address family %d\n",
				  __pmSockAddrGetFamily(myAddr));
		    fd = -1;
		}
		if (fd < 0) {
		    __pmSockAddrFree(myAddr);
		    continue; /* Try the next address */
		}

		/* Attempt to connect */
		__pmSockAddrSetPort(myAddr, *port);
		sts = connectLogger(fd, myAddr);
		__pmSockAddrFree(myAddr);

		/* Successful connection? */
		if (sts >= 0)
		    break;
	    }
	    __pmHostEntFree(servInfo);
	}
	
	if (sts < 0) {
	    if (pmDebugOptions.pmlc) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmConnectLogger: connect: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
	}
	else {
	    /* Expect an error PDU back: ACK/NACK for connection */
	    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, __pmLoggerTimeout(), &pb);
	    if (sts == PDU_ERROR) {
		__pmOverrideLastFd(PDU_OVERRIDE2);	/* don't dink with the value */
		__pmDecodeError(pb, &sts);
		php = (__pmPDUHdr *)pb;
		if (*pid != PM_LOG_NO_PID && *pid != PM_LOG_PRIMARY_PID && php->from != *pid) {
		    if (pmDebugOptions.pmlc)
			fprintf(stderr, "__pmConnectLogger: ACK response from pid %d, expected pid %d\n",
				php->from, *pid);
		    sts = -ECONNREFUSED;
		}
		*pid = php->from;
	    }
	    else if (sts < 0) {
		if (pmDebugOptions.pmlc) {
		    if (sts == PM_ERR_TIMEOUT)
			fprintf(stderr, "__pmConnectLogger: timeout (after %d secs)\n", __pmLoggerTimeout());
		    else {
			char	errmsg[PM_MAXERRMSGLEN];
			fprintf(stderr, "__pmConnectLogger: Error: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		    }
		}
		;	/* fall through */
	    }
	    else {
		/* wrong PDU type! */
		if (pmDebugOptions.pmlc)
		    fprintf(stderr, "__pmConnectLogger: ACK botch PDU type=%d not PDU_ERROR?\n", sts);
		sts = PM_ERR_IPC;
	    }

	    if (pinpdu > 0)
		__pmUnpinPDUBuf(pb);

	    if (sts >= 0) {
		/*
		 * choose min of sts (highest version the client supports)
		 * and LOG_PDU_VERSION (highest version pmlogger supports)
		 */
		__pmCred	handshake[1];
		int		choose = sts < LOG_PDU_VERSION ? sts : LOG_PDU_VERSION;
		if (choose != LOG_PDU_VERSION && pmDebugOptions.pmlc)
		    fprintf(stderr, "__pmConnectLogger: downgrading LOG PDU version from %d to %d\n", LOG_PDU_VERSION, choose);
		__pmSetVersionIPC(fd, choose);
		handshake[0].c_type = CVERSION;
		handshake[0].c_vala = choose;
		handshake[0].c_valb = 0;
		handshake[0].c_valc = 0;
		sts = __pmSendCreds(fd, (int)getpid(), 1, handshake);
		if (sts >= 0) {
		    if (pmDebugOptions.pmlc)
			fprintf(stderr, "__pmConnectLogger: LOG PDU version=%d fd=%d\n",
				__pmVersionIPC(fd), fd);
		    return fd;
		}
		else {
		    if (pmDebugOptions.pmlc)
			fprintf(stderr, "__pmConnectLogger: __pmSendCreds error %d\n", sts);
		}
	    }

	    /* Error if we get here */
	    __pmCloseSocket(fd);
	}

	/*
	 * If the prefix was "local:" and we have a port or a pid, try the
	 * connection as "localhost". Otherwise, we can't connect.
	 */
	if (wasLocal && (*port != PM_LOG_NO_PORT || *pid != PM_LOG_NO_PID)) {
	    connectionSpec = "localhost";
	    continue;
	}

	/* No more ways to connect. */
	break;
    } /* Loop over connect specs. */

    return sts;
}
