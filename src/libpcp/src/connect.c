/*
 * Copyright (c) 2012-2013 Red Hat.
 * Copyright (c) 1995-2002,2004 Silicon Graphics, Inc.  All Rights Reserved.
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
 * Thread-safe notes
 *
 * Do not need ctxp->c_pmcd->pc_lock lock around __pmSendCreds() call,
 * as the context has not been created, so no-one else could be using
 * the context's fd.
 */

#include "pmapi.h"
#include "impl.h"
#include "internal.h"

/* MY_BUFLEN needs to big enough to hold "hostname port" */
#define MY_BUFLEN (MAXHOSTNAMELEN+10)
#define MY_VERSION "pmproxy-client 1\n"

static int
negotiate_proxy(int fd, const char *hostname, int port)
{
    char	buf[MY_BUFLEN];
    char	*bp;
    int		ok = 0;

    /*
     * version negotiation (converse to pmproxy logic)
     *   __pmSend my client version message
     *   __pmRecv server version message
     *   __pmSend hostname and port
     */

    if (__pmSend(fd, MY_VERSION, strlen(MY_VERSION), 0) != strlen(MY_VERSION)) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send version string to pmproxy failed: %s\n",
	    pmErrStr_r(-neterror(), errmsg, sizeof(errmsg)));
	return PM_ERR_IPC;
    }
    for (bp = buf; bp < &buf[MY_BUFLEN]; bp++) {
	if (__pmRecv(fd, bp, 1, 0) != 1) {
	    *bp = '\0';
	    bp = &buf[MY_BUFLEN];
	    break;
	}
	if (*bp == '\n' || *bp == '\r') {
	    *bp = '\0';
	    break;
	}
    }
    if (bp < &buf[MY_BUFLEN]) {
	if (strcmp(buf, "pmproxy-server 1") == 0)
	    ok = 1;
    }

    if (!ok) {
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: bad version string from pmproxy: \"%s\"\n",
	    buf);
	return PM_ERR_IPC;
    }

    snprintf(buf, sizeof(buf), "%s %d\n", hostname, port);
    if (__pmSend(fd, buf, strlen(buf), 0) != strlen(buf)) {
	char	errmsg[PM_MAXERRMSGLEN];
	__pmNotifyErr(LOG_WARNING,
	     "__pmConnectPMCD: send hostname+port string to pmproxy failed: %s'\n",
	     pmErrStr_r(-neterror(), errmsg, sizeof(errmsg)));
	return PM_ERR_IPC;
    }

    return ok;
}

/*
 * client connects to pmcd handshake
 */
static int
__pmConnectHandshake(int fd, const char *hostname, int ctxflags, __pmHashCtl *attrs)
{
    __pmPDU	*pb;
    int		ok;
    int		version;
    int		challenge;
    int		sts;
    int		pinpdu;

    /* Expect an error PDU back from PMCD: ACK/NACK for connection */
    pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_ERROR) {
	/*
	 * See comments in pmcd ... we actually get an extended error PDU
	 * from pmcd, of the form
	 *
	 *  :----------:-----------:
	 *  |  status  | challenge |
	 *  :----------:-----------:
	 *
	 *   For a good connection, status is 0, else a PCP error code;
	 *   challenge contains server-side info (e.g. enabled features)
	 */
	version = __pmDecodeXtendError(pb, &sts, &challenge);
	if (version < 0) {
	    __pmUnpinPDUBuf(pb);
	    return version;
	}
	if (sts < 0) {
	    __pmUnpinPDUBuf(pb);
	    return sts;
	}

	if (version == PDU_VERSION2) {
	    __pmPDUInfo		pduinfo;
	    __pmVersionCred	handshake;
	    int			pduflags = 0;

	    pduinfo = __ntohpmPDUInfo(*(__pmPDUInfo *)&challenge);

	    if (pduinfo.features & PDU_FLAG_CREDS_REQD)
		/*
		 * This is a mandatory connection feature - pmcd must be
		 * sent user credential information one way or another -
		 * i.e. via SASL2 authentication, or AF_UNIX peer creds.
		 */
		pduflags |= PDU_FLAG_CREDS_REQD;

	    if (ctxflags) {
		/*
		 * If an optional connection feature (e.g. encryption) is
		 * desired, the pmcd that we're talking to must advertise
		 * support for the feature.  And if it did, the client in
		 * turn must request it be enabled (now, via pduflags).
		 */
		if (ctxflags & (PM_CTXFLAG_SECURE|PM_CTXFLAG_RELAXED)) {
		    if (pduinfo.features & PDU_FLAG_SECURE) {
			pduflags |= PDU_FLAG_SECURE;
		    } else if (ctxflags & PM_CTXFLAG_SECURE) {
			__pmUnpinPDUBuf(pb);
			return -EOPNOTSUPP;
		    }
		}
		if (ctxflags & PM_CTXFLAG_COMPRESS) {
		    if (pduinfo.features & PDU_FLAG_COMPRESS)
			pduflags |= PDU_FLAG_COMPRESS;
		    else {
			__pmUnpinPDUBuf(pb);
			return -EOPNOTSUPP;
		    }
		}
		if (ctxflags & PM_CTXFLAG_AUTH) {
		    if (pduinfo.features & PDU_FLAG_AUTH)
			pduflags |= PDU_FLAG_AUTH;
		    else {
			__pmUnpinPDUBuf(pb);
			return -EOPNOTSUPP;
		    }
		}
	    }

	    /*
	     * Negotiate connection version and features (via creds PDU)
	     */
	    if ((ok = __pmSetVersionIPC(fd, version)) < 0) {
		__pmUnpinPDUBuf(pb);
		return ok;
	    }

	    memset(&handshake, 0, sizeof(handshake));
	    handshake.c_type = CVERSION;
	    handshake.c_version = PDU_VERSION;
	    handshake.c_flags = pduflags;

	    sts = __pmSendCreds(fd, (int)getpid(), 1, (__pmCred *)&handshake);

	    /*
	     * At this point we know caller wants to set channel options and
	     * pmcd supports them so go ahead and update the socket now (this
	     * completes the SSL handshake in encrypting mode, authentication
	     * via SASL, and/or enabling compression in NSS).
	     */
	    if (sts >= 0 && pduflags)
		sts = __pmSecureClientHandshake(fd, pduflags, hostname, attrs);
	}
	else
	    sts = PM_ERR_IPC;
    }
    else if (sts != PM_ERR_TIMEOUT)
	sts = PM_ERR_IPC;

    if (pinpdu > 0)
	__pmUnpinPDUBuf(pb);

    return sts;
}

static int	global_nports;
static int	*global_portlist;
static int	default_portlist[] = { SERVER_PORT };

static void
load_pmcd_ports(void)
{
    static int	first_time = 1;

    if (first_time) {
	char	*envstr;
	char	*endptr;

	first_time = 0;

	if ((envstr = getenv("PMCD_PORT")) != NULL) {
	    char	*p = envstr;

	    for ( ; ; ) {
		int size, port = (int)strtol(p, &endptr, 0);
		if ((*endptr != '\0' && *endptr != ',') || port < 0) {
		    __pmNotifyErr(LOG_WARNING,
				  "ignored bad PMCD_PORT = '%s'", p);
		}
		else {
		    size = ++global_nports * sizeof(int);
		    global_portlist = (int *)realloc(global_portlist, size);
		    if (global_portlist == NULL) {
			__pmNotifyErr(LOG_WARNING,
				     "__pmConnectPMCD: portlist alloc failed (%d bytes), using default PMCD_PORT (%d)\n", size, SERVER_PORT);
			global_nports = 0;
			break;
		    }
		    global_portlist[global_nports-1] = port;
		}
		if (*endptr == '\0')
		    break;
		p = &endptr[1];
	    }
	}

	if (global_nports == 0) {
	    global_portlist = default_portlist;
	    global_nports = sizeof(default_portlist) / sizeof(default_portlist[0]);
	}
    }
}

static void
load_proxy_hostspec(pmHostSpec *proxy)
{
    static int	proxy_port;
    char	errmsg[PM_MAXERRMSGLEN];
    char	*envstr;
    char	*endptr;

    if ((envstr = getenv("PMPROXY_HOST")) != NULL) {
	proxy->name = strdup(envstr);
	if (proxy->name == NULL) {
	    __pmNotifyErr(LOG_WARNING,
			  "__pmConnectPMCD: cannot save PMPROXY_HOST: %s\n",
			  pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	}
	else {
	    if ((envstr = getenv("PMPROXY_PORT")) != NULL) {
		proxy_port = (int)strtol(envstr, &endptr, 0);
		if (*endptr != '\0' || proxy_port < 0) {
		    __pmNotifyErr(LOG_WARNING,
			"__pmConnectPMCD: ignored bad PMPROXY_PORT = '%s'\n", envstr);
		    proxy_port = PROXY_PORT;
		}
	    } else {
		proxy_port = PROXY_PORT;
	    }
	    proxy->ports = &proxy_port;
	    proxy->nports = 1;
	}
    }
}

static void
load_secure_runtime(void)
{
    /* Ensure correct security lib initialisation order */
    __pmInitAuthClients();
    __pmInitSecureSockets();

    /*
     * If secure sockets functionality available, iterate over the set of
     * known locations for certificate databases and attempt to initialise
     * one of them for our use.
     */
    if (__pmInitCertificates() < 0)
	__pmNotifyErr(LOG_WARNING, "__pmConnectPMCD: "
		"certificate database exists, but failed initialization");
}

void
__pmConnectGetPorts(pmHostSpec *host)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    load_pmcd_ports();
    if (__pmAddHostPorts(host, global_portlist, global_nports) < 0) {
	__pmNotifyErr(LOG_WARNING,
		"__pmConnectGetPorts: portlist dup failed, "
		"using default PMCD_PORT (%d)\n", SERVER_PORT);
	host->ports[0] = SERVER_PORT;
	host->nports = 1;
    }
    PM_UNLOCK(__pmLock_libpcp);
}

int
__pmConnectPMCD(pmHostSpec *hosts, int nhosts, int ctxflags, __pmHashCtl *attrs)
{
    int		sts = -1;
    int		fd = -1;	/* Fd for socket connection to pmcd */
    int		*ports;
    int		nports;
    int		portIx;
    int		version = -1;
    int		proxyport;
    pmHostSpec	*proxyhost;

    static int first_time = 1;
    static pmHostSpec proxy;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (first_time) {
	/*
	 * One-trip check for use of pmproxy(1) in lieu of pmcd(1),
	 * and to extract the optional environment variables ...
	 * PMCD_PORT, PMPROXY_HOST and PMPROXY_PORT.
	 * We also check for the presense of a certificate database
	 * and load it up if either a user or system (global) DB is
	 * found.
	 */
	first_time = 0;
	load_pmcd_ports();
	load_proxy_hostspec(&proxy);
	load_secure_runtime();
    }

    if (hosts[0].nports == 0) {
	nports = global_nports;
	ports = global_portlist;
    }
    else {
	nports = hosts[0].nports;
	ports = hosts[0].ports;
    }

    if (proxy.name == NULL && nhosts == 1) {
	const char *name = (const char *)hosts[0].name;

	/*
	 * no proxy, connecting directly to pmcd
	 */
	PM_UNLOCK(__pmLock_libpcp);

	sts = -1;
	/* Try connecting via the local unix domain socket, if requested and supported. */
	if (nports == PM_HOST_SPEC_NPORTS_LOCAL || nports == PM_HOST_SPEC_NPORTS_UNIX) {
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	    if ((fd = __pmAuxConnectPMCDUnixSocket(name)) >= 0) {
		if ((sts = __pmConnectHandshake(fd, name, ctxflags, attrs)) < 0) {
		    __pmCloseSocket(fd);
		}
		else
		    sts = fd;
		portIx = -1; /* no port */
	    }
#endif
	    /*
	     * If the connection failed, or is not supported, and the protocol was 'local:',
	     * then try connecting to localhost via the default port(s).
	     */
	    if (sts < 0) {
		if (nports == PM_HOST_SPEC_NPORTS_LOCAL) {
		    name = "localhost";
		    nports = global_nports;
		    ports = global_portlist;
		    sts = -1; /* keep trying */
		}
		else
		    sts = -2; /* no more connection attempts. */
	    }
	}

	/* If still not connected, try via the given host name and ports, if requested. */
	if (sts == -1) {
	    for (portIx = 0; portIx < nports; portIx++) {
		if ((fd = __pmAuxConnectPMCDPort(name, ports[portIx])) >= 0) {
		    if ((sts = __pmConnectHandshake(fd, name, ctxflags, attrs)) < 0) {
			__pmCloseSocket(fd);
		    }
		    else
			/* success */
			break;
		}
		else
		    sts = fd;
	    }
	}

	if (sts < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmConnectPMCD(%s): pmcd connection port=",
		   hosts[0].name);
		for (portIx = 0; portIx < nports; portIx++) {
		    if (portIx == 0) fprintf(stderr, "%d", ports[portIx]);
		    else fprintf(stderr, ",%d", ports[portIx]);
		}
		fprintf(stderr, " failed: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    }
#endif
	    return sts;
	}

#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    if (portIx >= 0) {
		fprintf(stderr, "__pmConnectPMCD(%s): pmcd connection port=%d fd=%d PDU version=%u\n",
			hosts[0].name, ports[portIx], fd, __pmVersionIPC(fd));
	    }
	    else {
		fprintf(stderr, "__pmConnectPMCD(%s): pmcd connection path=%s fd=%d PDU version=%u\n",
			hosts[0].name, name, fd, __pmVersionIPC(fd));
	    }
	    __pmPrintIPC();
	}
#endif

	return fd;
    }

    /*
     * connecting to pmproxy, and then to pmcd ... not a direct
     * connection to pmcd
     */
    proxyhost = (nhosts > 1) ? &hosts[1] : &proxy;
    proxyport = (proxyhost->nports > 0) ? proxyhost->ports[0] : PROXY_PORT;

    for (portIx = 0; portIx < nports; portIx++) {
	fd = __pmAuxConnectPMCDPort(proxyhost->name, proxyport);
	if (fd < 0) {
#ifdef PCP_DEBUG
	    if (pmDebug & DBG_TRACE_CONTEXT) {
		char	errmsg[PM_MAXERRMSGLEN];
		fprintf(stderr, "__pmConnectPMCD(%s): proxy to %s port=%d failed: %s \n",
			hosts[0].name, proxyhost->name, proxyport, pmErrStr_r(-neterror(), errmsg, sizeof(errmsg)));
	    }
#endif
	    PM_UNLOCK(__pmLock_libpcp);
	    return fd;
	}
	if ((sts = version = negotiate_proxy(fd, hosts[0].name, ports[portIx])) < 0)
	    __pmCloseSocket(fd);
	else if ((sts = __pmConnectHandshake(fd, proxyhost->name, ctxflags, attrs)) < 0)
	    __pmCloseSocket(fd);
	else
	    /* success */
	    break;
    }

    if (sts < 0) {
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    char	errmsg[PM_MAXERRMSGLEN];
	    fprintf(stderr, "__pmConnectPMCD(%s): proxy connection to %s port=",
			hosts[0].name, proxyhost->name);
	    for (portIx = 0; portIx < nports; portIx++) {
		if (portIx == 0) fprintf(stderr, "%d", ports[portIx]);
		else fprintf(stderr, ",%d", ports[portIx]);
	    }
	    fprintf(stderr, " failed: %s\n", pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	}
#endif
	PM_UNLOCK(__pmLock_libpcp);
	return sts;
    }

#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_CONTEXT) {
	fprintf(stderr, "__pmConnectPMCD(%s): proxy connection host=%s port=%d fd=%d version=%d\n",
	    hosts[0].name, proxyhost->name, ports[portIx], fd, version);
    }
#endif

    PM_UNLOCK(__pmLock_libpcp);
    return fd;
}
