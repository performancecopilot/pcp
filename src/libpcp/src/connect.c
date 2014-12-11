/*
 * Copyright (c) 2012-2014 Red Hat.
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
 * Verify that the requested context flags (ctxflags) are
 * viable given available functionality from pmcd at the
 * other end of a connection (features).
 * Returns appropriate PDU flags for the client to send to
 * pmcd if we can go ahead with the connection, else error.
 */
static int
check_feature_flags(int ctxflags, int features)
{
    int		pduflags = 0;

    if (features & PDU_FLAG_CREDS_REQD)
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
	    if (features & PDU_FLAG_SECURE) {
		pduflags |= PDU_FLAG_SECURE;
		/*
		 * Determine whether the server can send an ACK for a
		 * secure connection request. We can still connect
		 * whether it does or not, but we need to know the
		 * protocol.
		 */
		if (features & PDU_FLAG_SECURE_ACK)
		    pduflags |= PDU_FLAG_SECURE_ACK;
	    } else if (ctxflags & PM_CTXFLAG_SECURE) {
		return -EOPNOTSUPP;
	    }
	}
	if (ctxflags & PM_CTXFLAG_COMPRESS) {
	    if (features & PDU_FLAG_COMPRESS)
		pduflags |= PDU_FLAG_COMPRESS;
	    else
		return -EOPNOTSUPP;
	}
	if (ctxflags & PM_CTXFLAG_AUTH) {
	    if (features & PDU_FLAG_AUTH)
		pduflags |= PDU_FLAG_AUTH;
	    else
		return -EOPNOTSUPP;
	}
	if (ctxflags & PM_CTXFLAG_CONTAINER) {
	    if (features & PDU_FLAG_CONTAINER)
		pduflags |= PDU_FLAG_CONTAINER;
	    else
		return -EOPNOTSUPP;
	}
    }
    return pduflags;
}

static int
container_handshake(int fd, __pmHashCtl *attrs)
{
    __pmHashNode *node;
    const char *name;
    int length;

    if ((node = __pmHashSearch(PCP_ATTR_CONTAINER, attrs)) == NULL)
	return -ESRCH;
    if ((name = (const char *)node->data) == NULL)
	return -ESRCH;
    length = strlen(name);

    if (pmDebug & DBG_TRACE_CONTEXT)
	fprintf(stderr, "%s:__pmConnectHandshake container=\"%s\" [%d]\n",
		__FILE__, name, length);

    return __pmSendAttr(fd, FROM_ANON, PCP_ATTR_CONTAINER, name, length);
}

static int
attributes_handshake(int fd, int flags, const char *host, __pmHashCtl *attrs)
{
    int sts;

    if ((sts = __pmSecureClientHandshake(fd, flags, host, attrs)) < 0)
	return sts;

    /* additional connection attributes, done after secure setup */
    if (((flags & PDU_FLAG_CONTAINER) != 0) &&
	((sts = container_handshake(fd, attrs)) < 0))
	return sts;

    return 0;
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
	    int			pduflags;

	    pduinfo = __ntohpmPDUInfo(*(__pmPDUInfo *)&challenge);
	    pduflags = sts = check_feature_flags(ctxflags, pduinfo.features);
	    if (sts < 0) {
		__pmUnpinPDUBuf(pb);
		return sts;
	    }

	    if ((ok = __pmSetVersionIPC(fd, version)) < 0) {
		__pmUnpinPDUBuf(pb);
		return ok;
	    }

	    /*
	     * Negotiate connection version and features (via creds PDU)
	     */
	    memset(&handshake, 0, sizeof(handshake));
	    handshake.c_type = CVERSION;
	    handshake.c_version = PDU_VERSION;
	    handshake.c_flags = pduflags;
	    sts = __pmSendCreds(fd, (int)getpid(), 1, (__pmCred *)&handshake);

	    /*
	     * At this point we know the caller wants to set channel options
	     * and pmcd supports them so go ahead and update the socket (this
	     * completes the SSL handshake in encrypting mode, authentication
	     * via SASL, enabling compression in NSS, and any other requested
	     * connection attributes).
	     */
	    if (sts >= 0 && pduflags)
		sts = attributes_handshake(fd, pduflags, hostname, attrs);
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

static void
load_pmcd_ports(void)
{
    if (global_portlist == NULL) {
	/* __pmPMCDAddPorts discovers at least one valid port, if it returns. */
	global_nports = __pmPMCDAddPorts(&global_portlist, global_nports);
    }
}

static void
load_proxy_hostspec(pmHostSpec *proxy)
{
    char	errmsg[PM_MAXERRMSGLEN];
    char	*envstr;

    if ((envstr = getenv("PMPROXY_HOST")) != NULL) {
	proxy->name = strdup(envstr);
	if (proxy->name == NULL) {
	    __pmNotifyErr(LOG_WARNING,
			  "__pmConnectPMCD: cannot save PMPROXY_HOST: %s\n",
			  pmErrStr_r(-oserror(), errmsg, sizeof(errmsg)));
	}
	else {
	    /*
	     *__pmProxyAddPorts discovers at least one valid port, if it
	     * returns.
	     */
	    proxy->nports = __pmProxyAddPorts(&proxy->ports, proxy->nports);
	}
    }
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
#ifdef PCP_DEBUG
	    if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
		fprintf(stderr, "__pmConnectPMCD: trying __pmAuxConnectPMCDUnixSocket(%s) ...\n", name);
	    }
#endif
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
#ifdef PCP_DEBUG
		if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
		    fprintf(stderr, "__pmConnectPMCD: trying __pmAuxConnectPMCDPort(%s, %d) ...\n", name, ports[portIx]);
		}
#endif
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
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "__pmConnectPMCD: trying __pmAuxConnectPMCDPort(%s, %d) ...\n", proxyhost->name, proxyport);
	}
#endif
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
