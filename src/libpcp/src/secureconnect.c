/*
 * Copyright (c) 2012-2013 Red Hat.
 * Network Security Services (NSS) support.  Client side.
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
#define SOCKET_INTERNAL
#include "internal.h"
#include <hasht.h>
#include <certdb.h>
#include <secerr.h>
#include <sslerr.h>
#include <pk11pub.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

void
__pmHostEntFree(__pmHostEnt *hostent)
{
    if (hostent->name != NULL)
        free(hostent->name);
    if (hostent->addresses != NULL)
        PR_FreeAddrInfo(hostent->addresses);
    free(hostent);
}

/*
 * We shift NSS/NSPR/SSL/SASL errors below the valid range for other
 * PCP error codes, in order to avoid conflicts.  pmErrStr can then
 * detect and decode.  PM_ERR_NYI is the PCP error code sentinel.
 */
int
__pmSecureSocketsError(int code)
{
    int sts = (PM_ERR_NYI + code);	/* encode, negative value */
    setoserror(-sts);
    return sts;
}

int
__pmSocketClosed(void)
{
    int	error = oserror();

    if (PM_ERR_NYI > -error)
	error = -(error + PM_ERR_NYI);

    switch (error) {
	/*
	 * Treat this like end of file on input.
	 *
	 * failed as a result of pmcd exiting and the connection
	 * being reset, or as a result of the kernel ripping
	 * down the connection (most likely because the host at
	 * the other end just took a dive)
	 *
	 * from IRIX BDS kernel sources, seems like all of the
	 * following are peers here:
	 *  ECONNRESET (pmcd terminated?)
	 *  ETIMEDOUT ENETDOWN ENETUNREACH EHOSTDOWN EHOSTUNREACH
	 *  ECONNREFUSED
	 * peers for BDS but not here:
	 *  ENETRESET ENONET ESHUTDOWN (cache_fs only?)
	 *  ECONNABORTED (accept, user req only?)
	 *  ENOTCONN (udp?)
	 *  EPIPE EAGAIN (nfs, bds & ..., but not ip or tcp?)
	 */
	case ECONNRESET:
	case EPIPE:
	case ETIMEDOUT:
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
	case ECONNREFUSED:
	case PR_IO_TIMEOUT_ERROR:
	case PR_NETWORK_UNREACHABLE_ERROR:
	case PR_CONNECT_TIMEOUT_ERROR:
	case PR_NOT_CONNECTED_ERROR:
	case PR_CONNECT_RESET_ERROR:
	case PR_PIPE_ERROR:
	case PR_NETWORK_DOWN_ERROR:
	case PR_SOCKET_SHUTDOWN_ERROR:
	case PR_HOST_UNREACHABLE_ERROR:
	    return 1;
    }
    return 0;
}

/*
 * For every connection when operating under secure socket mode, we need
 * the following auxillary structure associated with the socket.  It holds
 * critical information that each piece of the security pie can make use
 * of (NSS/SSL/NSPR/SASL).  This is allocated once when initial connection
 * is being established.
 */
typedef struct { 
    PRFileDesc	*nsprFd;
    PRFileDesc	*sslFd;
    struct sasl {
	sasl_conn_t *conn;
	__pmHashCtl *attrs;
    } sasl;
} __pmSecureSocket;

int
__pmDataIPCSize(void)
{
    return sizeof(__pmSecureSocket);
}

/*
 * NSS/NSPR file descriptors are not integers, however, integral file
 * descriptors are expected in many parts of pcp. In order to deal with
 * this assumption, when NSS/NSPR is available, we maintain a set of
 * available integral file descriptors. The file descriptor number
 * returned by __pmCreateSocket is a reference to this set and must be
 * used for all further I/O operations on that socket.
 *
 * Since some interfaces (e.g. the IPC table) will use a mix of native
 * file descriptors * and NSPR ones, we need a way to distinguish them.
 * Obtaining the hard max fd number using getrlimit() was considered,
 * but a sysadmin could change this limit arbitrarily while we are
 * running. We can't use negative values, since these indicate an error.
 *
 * There is a limit on the range of fd's which can be passed to the
 * fd_set API. It is FD_SETSIZE.  So, consider all fd's >= FD_SETSIZE
 * to be ones which reference our set. Using this threshold will also
 * allow us to easily manage mixed sets of native and NSPR fds.
 *
 * NB: __pmLock_libpcp must be held when accessing this set, since
 * another thread could modify it at any time.
 */
static fd_set nsprFds;
#define NSPR_HANDLE_BASE FD_SETSIZE

static int
newNSPRHandle(void)
{
    int fd;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    for (fd = 0; fd < FD_SETSIZE; ++fd) {
        if (! FD_ISSET(fd, &nsprFds)) {
	    FD_SET(fd, &nsprFds);
	    PM_UNLOCK(__pmLock_libpcp);
	    return NSPR_HANDLE_BASE + fd;
	}
    }
    PM_UNLOCK(__pmLock_libpcp);

    /* No free handles available */
    return -1;
}

static void
freeNSPRHandle(int fd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    FD_CLR(fd - NSPR_HANDLE_BASE, &nsprFds);
    PM_UNLOCK(__pmLock_libpcp);
}

int
__pmInitSecureSockets(void)
{
    /* Make sure that NSPR has been initialized */
    if (PR_Initialized() != PR_TRUE)
        PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 0);
    return 0;
}

int
__pmShutdownSecureSockets(void)
{
    if (PR_Initialized())
	PR_Cleanup();
    return 0;
}

static int
__pmSetupSocket(PRFileDesc *fdp)
{
    __pmSecureSocket socket = { 0 };
    int fd, sts;

    socket.nsprFd = fdp;
    if ((fd = newNSPRHandle()) < 0) {
	PR_Close(socket.nsprFd);
	return fd;
    }
    if ((sts = __pmSetDataIPC(fd, (void *)&socket)) < 0) {
	PR_Close(socket.nsprFd);
	freeNSPRHandle(fd);
	return sts;
    }
    (void)__pmInitSocket(fd);	/* cannot fail after __pmSetDataIPC */
    return fd;
}

int
__pmCreateSocket(void)
{
    PRFileDesc *fdp;

    __pmInitSecureSockets();
    if ((fdp = PR_OpenTCPSocket(PR_AF_INET)) == NULL)
	return -neterror();
    return __pmSetupSocket(fdp);
}

int
__pmCreateIPv6Socket(void)
{
    int fd, sts, on;
    __pmSockLen onlen = sizeof(on);
    PRFileDesc *fdp;

    __pmInitSecureSockets();

    /* Open the socket */
    if ((fdp = PR_OpenTCPSocket(PR_AF_INET6)) == NULL)
	return -neterror();
    fd = PR_FileDesc2NativeHandle(fdp);

    /*
     * Disable IPv4-mapped connections.
     * Must explicitly check whether that worked, for ipv6.enabled=false
     * kernels.  Setting then testing is the most reliable way we've found.
     */
    on = 1;
    setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &on, onlen);
    on = 0;
    sts = getsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&on, &onlen);
    if (sts < 0 || on != 1) {
	__pmNotifyErr(LOG_ERR, "__pmCreateIPv6Socket: IPV6 is not supported\n");
	PR_Close(fdp);
	return -EOPNOTSUPP;
    }

    return __pmSetupSocket(fdp);
}

void
__pmCloseSocket(int fd)
{
    __pmSecureSocket socket;
    int sts;

    sts = __pmDataIPC(fd, (void *)&socket);
    __pmResetIPC(fd);

    if (sts == 0) {
	if (socket.sasl.conn) {
	    sasl_dispose(&socket.sasl.conn);
	    socket.sasl.conn = NULL;
	}
	if (socket.nsprFd) {
	    freeNSPRHandle(fd);
	    PR_Close(socket.nsprFd);
	    socket.nsprFd = NULL;
	    socket.sslFd = NULL;
	}
    } else {
#if defined(IS_MINGW)
	closesocket(fd);
#else
	close(fd);
#endif
    }
}

static int
mkpath(const char *dir, mode_t mode)
{
    char path[MAXPATHLEN], *p;
    int sts;

    sts = access(dir, R_OK|W_OK|X_OK);
    if (sts == 0)
	return 0;
    if (sts < 0 && oserror() != ENOENT)
	return -1;

    strncpy(path, dir, sizeof(path));
    path[sizeof(path)-1] = '\0';

    for (p = path+1; *p != '\0'; p++) {
	if (*p == __pmPathSeparator()) {
	    *p = '\0';
	    mkdir2(path, mode);
	    *p = __pmPathSeparator();
	}
    }
    return mkdir2(path, mode);
}

static char *
dbpath(char *path, size_t size)
{
    int sep = __pmPathSeparator();
    const char *empty_homedir = "";
    char *homedir = getenv("HOME");
    char *nss_method = getenv("PCP_SECURE_DB_METHOD");

    if (homedir == NULL)
	homedir = (char *)empty_homedir;
    if (nss_method == NULL)
	nss_method = "sql:";

    /*
     * Fill in a buffer with the users NSS database specification.
     * Return a pointer to the filesystem path component - without
     * the <method>:-prefix - for other routines to work with.
     */
    snprintf(path, size, "%s%s" "%c" ".pki" "%c" "nssdb",
		nss_method, homedir, sep, sep);
    return path + strlen(nss_method);
}

int
__pmInitCertificates(void)
{
    char nssdb[MAXPATHLEN];
    SECStatus secsts;

    /*
     * Check for client certificate databases.  We enforce use
     * of the per-user shared NSS database at $HOME/.pki/nssdb
     * For simplicity, we create this directory if we need to.
     * If we cannot, we silently bail out so that users who're
     * not using secure connections (initially everyone) don't
     * have to diagnose / put up with spurious errors.
     */
    if (mkpath(dbpath(nssdb, sizeof(nssdb)), 0700) < 0)
	return 0;

    secsts = NSS_InitReadWrite(nssdb);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    secsts = NSS_SetExportPolicy();
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    SSL_ClearSessionCache();

    return 0;
}

int
__pmShutdownCertificates(void)
{
    if (NSS_Shutdown() != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());
    return 0;
}

static void
saveUserCertificate(CERTCertificate *cert)
{
    SECStatus secsts;
    PK11SlotInfo *slot = PK11_GetInternalKeySlot();
    CERTCertTrust *trust = NULL;

    secsts = PK11_ImportCert(slot, cert, CK_INVALID_HANDLE,
				SECURE_SERVER_CERTIFICATE, PR_FALSE);
    if (secsts != SECSuccess)
	goto done;

    secsts = SECFailure;
    trust = (CERTCertTrust *)PORT_ZAlloc(sizeof(CERTCertTrust));
    if (!trust)
	goto done;

    secsts = CERT_DecodeTrustString(trust, "P,P,P");
    if (secsts != SECSuccess)
	goto done;

    secsts = CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), cert, trust);

done:
    if (slot)
	PK11_FreeSlot(slot);
    if (trust)
	PORT_Free(trust);

    /*
     * Issue a warning only, but continue, if we fail to save certificate
     * (this is not a fatal condition on setting up the secure socket).
     */
    if (secsts != SECSuccess) {
	char	errmsg[PM_MAXERRMSGLEN];
	pmprintf("WARNING: Failed to save certificate locally: %s\n",
		pmErrStr_r(__pmSecureSocketsError(PR_GetError()),
			errmsg, sizeof(errmsg)));
	pmflush();
    }
}

static int
rejectUserCertificate(const char *message)
{
    pmprintf("%s? (no)\n", message);
    pmflush();
    return 0;
}

#ifdef HAVE_SYS_TERMIOS_H
static int
queryCertificateOK(const char *message)
{
    int c, fd, sts = 0, count = 0;

    fd = fileno(stdin);
    /* if we cannot interact, simply assume the answer to be "no". */
    if (!isatty(fd))
	return rejectUserCertificate(message);

    do {
	struct termios saved, raw;

	pmprintf("%s (y/n)? ", message);
	pmflush();

	/* save terminal state and temporarily enter raw terminal mode */
	if (tcgetattr(fd, &saved) < 0)
	    return 0;
	cfmakeraw(&raw);
	if (tcsetattr(fd, TCSAFLUSH, &raw) < 0)
	    return 0;

	c = getchar();
	if (c == 'y' || c == 'Y')
	    sts = 1;	/* yes */
	else if (c == 'n' || c == 'N')
	    sts = 0;	/* no */
	else
	    sts = -1;	/* dunno, try again (3x) */
	tcsetattr(fd, TCSAFLUSH, &saved);
	pmprintf("\n");
    } while (sts == -1 && ++count < 3);
    pmflush();

    return sts;
}
#else
static int
queryCertificateOK(const char *message)
{
    /* no way implemented to interact to query the user, so decline */
    return rejectUserCertificate(message);
}
#endif

static void
reportFingerprint(SECItem *item)
{
    unsigned char fingerprint[SHA1_LENGTH] = { 0 };
    SECItem fitem;
    char *fstring;

    PK11_HashBuf(SEC_OID_SHA1, fingerprint, item->data, item->len);
    fitem.data = fingerprint;
    fitem.len = SHA1_LENGTH;
    fstring = CERT_Hexify(&fitem, 1);
    pmprintf("SHA1 fingerprint is %s\n", fstring);
    PORT_Free(fstring);
}

static SECStatus
queryCertificateAuthority(PRFileDesc *sslsocket)
{
    int sts;
    int secsts = SECFailure;
    char *result;
    CERTCertificate *servercert;

    result = SSL_RevealURL(sslsocket);
    pmprintf("WARNING: "
	     "issuer of certificate received from host %s is not trusted.\n",
	     result);
    PORT_Free(result);

    servercert = SSL_PeerCertificate(sslsocket);
    if (servercert) {
	reportFingerprint(&servercert->derCert);
	sts = queryCertificateOK("Do you want to accept and save this certificate locally anyway");
	if (sts == 1) {
	    saveUserCertificate(servercert);
	    secsts = SECSuccess;
	}
	CERT_DestroyCertificate(servercert);
    } else {
	pmflush();
    }
    return secsts;
}

static SECStatus
queryCertificateDomain(PRFileDesc *sslsocket)
{
    int sts;
    char *result;
    SECItem secitem = { 0 };
    SECStatus secstatus = SECFailure;
    PRArenaPool *arena = NULL;
    CERTCertificate *servercert = NULL;

    /*
     * Propagate a warning through to the client.  Show the expected
     * host, then list the DNS names from the server certificate.
     */
    result = SSL_RevealURL(sslsocket);
    pmprintf("WARNING: "
"The domain name %s does not match the DNS name(s) on the server certificate:\n",
		result);
    PORT_Free(result);

    servercert = SSL_PeerCertificate(sslsocket);
    secstatus = CERT_FindCertExtension(servercert,
				SEC_OID_X509_SUBJECT_ALT_NAME, &secitem);
    if (secstatus != SECSuccess || !secitem.data) {
	pmprintf("Unable to find alt name extension on the server certificate\n");
    } else if ((arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE)) == NULL) {
	pmprintf("Out of memory while generating name list\n");
	SECITEM_FreeItem(&secitem, PR_FALSE);
    } else {
	CERTGeneralName *namelist, *n;

	namelist = n = CERT_DecodeAltNameExtension(arena, &secitem);
	SECITEM_FreeItem(&secitem, PR_FALSE);
	if (!namelist) {
	    pmprintf("Unable to decode alt name extension on server certificate\n");
	} else {
	    do {
		if (n->type == certDNSName)
		    pmprintf("  %.*s\n", (int)n->name.other.len, n->name.other.data);
		n = CERT_GetNextGeneralName(n);
	    } while (n != namelist);
	}
    }
    if (arena)
	PORT_FreeArena(arena, PR_FALSE);
    if (servercert)
	CERT_DestroyCertificate(servercert);

    sts = queryCertificateOK("Do you want to accept this certificate anyway");
    return (sts == 1) ? SECSuccess : SECFailure;
}

static SECStatus
badCertificate(void *arg, PRFileDesc *sslsocket)
{
    (void)arg;
    switch (PR_GetError()) {
    case SSL_ERROR_BAD_CERT_DOMAIN:
	return queryCertificateDomain(sslsocket);
    case SEC_ERROR_UNKNOWN_ISSUER:
	return queryCertificateAuthority(sslsocket);
    default:
	break;
    }
    return SECFailure;
}

static int
__pmSecureClientIPCFlags(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    __pmSecureSocket socket;
    SECStatus secsts;
    int sts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;
    if (socket.nsprFd == NULL)
	return -EOPNOTSUPP;

    if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL) {
	__pmNotifyErr(LOG_ERR, "SecureClientIPCFlags: importing socket into SSL");
	return PM_ERR_IPC;
    }
    socket.nsprFd = socket.sslFd;

    if ((flags & PDU_FLAG_SECURE) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_SECURITY, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_OptionSet(socket.sslFd, SSL_HANDSHAKE_AS_CLIENT, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_SetURL(socket.sslFd, hostname);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_BadCertHook(socket.sslFd,
				(SSLBadCertHandler)badCertificate, NULL);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
    }

    if ((flags & PDU_FLAG_COMPRESS) != 0) {
	secsts = SSL_OptionSet(socket.sslFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
    }

    if ((flags & PDU_FLAG_USER_AUTH) != 0) {
	sts = sasl_client_new(SECURE_SERVER_SASL_SERVICE,
				hostname,
				NULL, NULL, NULL, /*iplocal,ipremote,callbacks*/
				0, &socket.sasl.conn);
	if (sts != SASL_OK && sts != SASL_CONTINUE)
	    return __pmSecureSocketsError(sts);
	socket.sasl.attrs = attrs;
    }

    /* save changes back into the IPC table (updates client sslFd) */
    return __pmSetDataIPC(fd, (void *)&socket);
}

void
__pmInitSecureCallbacks(void)
{
#if 0	/* XXX: nathans TODO */
    sasl_client_init(callbacks);
    /* -- callback for SASL username -- */
    /* -- callback for SASL passphrase -- */
    /* -- callback for SASL realm -- */
#endif

#if 0	/* XXX: nathans TODO */
    /* -- callback for SSL/TLS certificate acceptance -- */
#endif
}

static int
__pmSecureClientNegotiation(int fd, int *strength)
{
    PRIntervalTime timer;
    PRFileDesc *sslsocket;
    SECStatus secsts;
    int enabled, keysize;
    int msec;

    sslsocket = (PRFileDesc *)__pmGetSecureSocket(fd);
    if (!sslsocket)
	return -EINVAL;

    secsts = SSL_ResetHandshake(sslsocket, PR_FALSE /*client*/);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    msec = __pmConvertTimeout(TIMEOUT_DEFAULT);
    timer = PR_MillisecondsToInterval(msec);
    secsts = SSL_ForceHandshakeWithTimeout(sslsocket, timer);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    secsts = SSL_SecurityStatus(sslsocket, &enabled, NULL, &keysize, NULL, NULL, NULL);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    *strength = (enabled > 0) ? keysize : DEFAULT_SECURITY_STRENGTH;
    return 0;
}

static int
__pmAuthClientSetProperties(sasl_conn_t *conn, int ssf, __pmHashCtl *attrs)
{
#if 0	/* XXX: nathans TODO */
    int sts;
    sasl_security_properties_t props;

    /* set external security strength factor */
    sasl_setprop(conn, SASL_SSF_EXTERNAL, &ssf);

    /* set general security properties */
    memset(&props, 0, sizeof(props));
    props.maxbufsize = LIMIT_USER_AUTH;
    props.max_ssf = UINT_MAX;

    /* props.security_flags |= SASL_SEC_* */
    sasl_setprop(conn, SASL_SEC_PROPS, &props);
#else
    (void)attrs;
    (void)conn;
    (void)ssf;
#endif

    return 0;
}

static int
__pmAuthClientNegotiation(int fd, int ssf, const char *hostname, __pmHashCtl *attrs)
{
    int sts, result = SASL_OK;
    int pinned, length, method_length;
    char *payload, buffer[LIMIT_USER_AUTH];
    const char *method = NULL;
    sasl_conn_t *sasl_conn;
    __pmHashNode *node;
    __pmPDU *pb;

    if (pmDebug & DBG_TRACE_USERAUTH)
	fprintf(stderr, "__pmAuthClientNegotiation(fd=%d, ssf=%d, host=%s)\n",
		fd, ssf, hostname);

    if ((sasl_conn = (sasl_conn_t *)__pmGetUserAuthData(fd)) == NULL)
	return -EINVAL;

    /* setup all the security properties for this connection */
    if ((sts = __pmAuthClientSetProperties(sasl_conn, ssf, attrs)) < 0)
	return sts;

    /* lookup users preferred connection method, if specified */
    if ((node = __pmHashSearch(PCP_ATTR_METHOD, attrs)) != NULL)
	method = (const char *)node->data;

    if (pmDebug & DBG_TRACE_USERAUTH)
	fprintf(stderr, "__pmAuthClientNegotiation - requested \"%s\" method\n",
		method ? method : "default");

    /* get security mechanism list */ 
    sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_USER_AUTH) {
	sts = __pmDecodeUserAuth(pb, &length, &payload);
	if (sts >= 0) {
	    /*
	     * buffer now contains the list of server mechanisms -
	     * override using users preference (if any) and proceed.
	     */
	    if (method) {
		strncpy(buffer, method, sizeof(buffer));
		buffer[sizeof(buffer) - 1] = '\0';
		length = strlen(buffer);
	    } else {
		strncpy(buffer, payload, length);
		buffer[length - 1] = '\0';
	    }

	    payload = NULL;
	    sts = result = sasl_client_start(sasl_conn, buffer, NULL,
					     (const char **)&payload,
					     (unsigned int *)&length, &method);
	    if (sts != SASL_OK && sts != SASL_CONTINUE)
		sts = __pmSecureSocketsError(sts);
	}
    } else if (sts == PDU_ERROR) {
	__pmDecodeError(pb, &sts);
    } else if (sts != PM_ERR_TIMEOUT) {
	sts = PM_ERR_IPC;
    }

    if (pinned)
	__pmUnpinPDUBuf(pb);
    if (sts < 0)
	return sts;

    if (pmDebug & DBG_TRACE_USERAUTH)
	fprintf(stderr, "__pmAuthClientNegotiation chosen method is %s", method);

    /* tell server we've made a decision and are ready to move on */
    strncpy(buffer, method, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    method_length = strlen(buffer);
    if (payload) {
	if (LIMIT_USER_AUTH - method_length - 1 < length)
	    return -E2BIG;
	memcpy(buffer + method_length + 1, payload, length);
	length += method_length + 1;
    } else {
	length = method_length + 1;
    }

    if ((sts = __pmSendUserAuth(fd, FROM_ANON, length, buffer)) < 0)
	return sts;

    while (result == SASL_CONTINUE) {
	if (pmDebug & DBG_TRACE_USERAUTH)
	    fprintf(stderr, "__pmAuthClientNegotiation awaiting server reply");

	sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
	if (sts == PDU_USER_AUTH) {
	    sts = __pmDecodeUserAuth(pb, &length, &payload);
	    if (sts >= 0) {
		sts = result = sasl_client_step(sasl_conn, payload, length, NULL,
						(const char **)&buffer,
						(unsigned int *)&length);
		if (sts != SASL_OK && sts != SASL_CONTINUE) {
		    sts = __pmSecureSocketsError(sts);
		    break;
		}
		if (pmDebug & DBG_TRACE_USERAUTH) {
		    fprintf(stderr, "__pmAuthClientNegotiation"
				    " step recv (%d bytes)", length);
		}
	    }
	} else if (sts == PDU_ERROR) {
	    __pmDecodeError(pb, &sts);
	} else if (sts != PM_ERR_TIMEOUT) {
	    sts = PM_ERR_IPC;
	}

	if (pinned)
	    __pmUnpinPDUBuf(pb);
	if (sts >= 0)
	    sts = __pmSendUserAuth(fd, FROM_ANON, length, length ? buffer : "");
	if (sts < 0)
	    break;
    }

    if (pmDebug & DBG_TRACE_USERAUTH) {
	if (sts < 0)
	    fprintf(stderr, "__pmAuthClientNegotiation loop failed\n");
	else {
	    result = sasl_getprop(sasl_conn, SASL_USERNAME, (const void **)&payload);
	    fprintf(stderr, "__pmAuthClientNegotiation success, username=%s\n",
			    result != SASL_OK ? "?" : payload);
	}
    }

    return sts;
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    int sts, ssf = DEFAULT_SECURITY_STRENGTH;

    if ((sts = __pmSecureClientIPCFlags(fd, flags, hostname, attrs)) < 0)
	return sts;
    if (((flags & PDU_FLAG_SECURE) != 0) &&
	((sts = __pmSecureClientNegotiation(fd, &ssf)) < 0))
	return sts;
    if (((flags & PDU_FLAG_USER_AUTH) != 0) &&
	((sts = __pmAuthClientNegotiation(fd, ssf, hostname, attrs)) < 0))
	return sts;
    return 0;
}

void *
__pmGetSecureSocket(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) < 0)
	return NULL;
    return (void *)socket.sslFd;
}

void *
__pmGetUserAuthData(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) < 0)
	return NULL;
    return (void *)socket.sasl.conn;
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    __pmSecureSocket socket;
    SECStatus secsts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;
    if (socket.nsprFd == NULL)
	return -EOPNOTSUPP;

    if ((flags & PDU_FLAG_SECURE) != 0) {
	if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL)
	    return __pmSecureSocketsError(PR_GetError());
	socket.nsprFd = socket.sslFd;

	secsts = SSL_OptionSet(socket.sslFd, SSL_NO_LOCKS, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_OptionSet(socket.sslFd, SSL_SECURITY, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_OptionSet(socket.sslFd, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUEST_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUIRE_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
    }

    if ((flags & PDU_FLAG_COMPRESS) != 0) {
	secsts = SSL_OptionSet(socket.nsprFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
    }

    if ((flags & PDU_FLAG_USER_AUTH) != 0) {
	int saslsts = sasl_server_new(SECURE_SERVER_SASL_SERVICE,
				NULL, NULL, /*localdomain,userdomain*/
				NULL, NULL, NULL, /*iplocal,ipremote,callbacks*/
				0, &socket.sasl.conn);
	if (saslsts != SASL_OK && saslsts != SASL_CONTINUE)
	    return __pmSecureSocketsError(saslsts);
    }

    /* save changes back into the IPC table */
    return __pmSetDataIPC(fd, (void *)&socket);
}

static int
sockOptValue(const void *option_value, __pmSockLen option_len)
{
    switch(option_len) {
    case sizeof(int):
        return *(int *)option_value;
    default:
        __pmNotifyErr(LOG_ERR, "sockOptValue: invalid option length: %d\n", option_len);
	break;
    }
    return 0;
}

int
__pmSetSockOpt(int fd, int level, int option_name, const void *option_value,
	       __pmSockLen option_len)
{
    /* Map the request to the NSPR equivalent, if possible. */
    PRSocketOptionData option_data;
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	switch(level) {
	case SOL_SOCKET:
	    switch(option_name) {
#ifdef IS_MINGW
	    case SO_EXCLUSIVEADDRUSE: {
		/*
		 * There is no direct mapping of this option in NSPR.
		 * The best we can do is to use the native handle and
		 * call setsockopt on that handle.
		 */
	        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
		return setsockopt(fd, level, option_name, option_value, option_len);
	    }
#endif
	    case SO_KEEPALIVE:
	        option_data.option = PR_SockOpt_Keepalive;
		option_data.value.keep_alive = sockOptValue(option_value, option_len);
		break;
	    case SO_LINGER: {
	        struct linger *linger = (struct linger *)option_value;
		option_data.option = PR_SockOpt_Linger;
		option_data.value.linger.polarity = linger->l_onoff;
		option_data.value.linger.linger = linger->l_linger;
		break;
	    }
	    case SO_REUSEADDR:
	        option_data.option = PR_SockOpt_Reuseaddr;
		option_data.value.reuse_addr = sockOptValue(option_value, option_len);
		break;
	    default:
	        __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			      option_name);
		return -1;
	    }
	    break;
	case IPPROTO_TCP:
	    if (option_name == TCP_NODELAY) {
	        option_data.option = PR_SockOpt_NoDelay;
		option_data.value.no_delay = sockOptValue(option_value, option_len);
		break;
	    }
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for IPPROTO_TCP: %d\n",
			  option_name);
	    return -1;
	case IPPROTO_IPV6:
	    if (option_name == IPV6_V6ONLY) {
		/*
		 * There is no direct mapping of this option in NSPR.
		 * The best we can do is to use the native handle and
		 * call setsockopt on that handle.
		 */
	        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
		return setsockopt(fd, level, option_name, option_value, option_len);
	    }
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented option_name for IPPROTO_IPV6: %d\n",
			  option_name);
	    return -1;
	default:
	    __pmNotifyErr(LOG_ERR, "__pmSetSockOpt: unimplemented level: %d\n", level);
	    return -1;
	}

	return (PR_SetSocketOption(socket.nsprFd, &option_data)
		== PR_SUCCESS) ? 0 : -1;
    }

    /* We have a native socket. */
    return setsockopt(fd, level, option_name, option_value, option_len);
}

int
__pmGetSockOpt(int fd, int level, int option_name, void *option_value,
	       __pmSockLen *option_len)
{
    __pmSecureSocket socket;

    /* Map the request to the NSPR equivalent, if possible. */
    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	switch (level) {
	case SOL_SOCKET:
	    switch(option_name) {
	    case SO_ERROR: {
		/*
		 * There is no direct mapping of this option in NSPR.
		 * Best we can do is call getsockopt on the native fd.
		 */
	      fd = PR_FileDesc2NativeHandle(socket.nsprFd);
	      return getsockopt(fd, level, option_name, option_value, option_len);
	  }
	  default:
	      __pmNotifyErr(LOG_ERR,
			"__pmGetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			option_name);
	      return -1;
	  }
	  break;

	default:
	    __pmNotifyErr(LOG_ERR, "__pmGetSockOpt: unimplemented level: %d\n", level);
	    break;
	}
	return -1;
    }

    /* We have a native socket. */
    return getsockopt(fd, level, option_name, option_value, option_len);
}
 
/*
 * Initialize a socket address.  The integral address must be INADDR_ANY or
 * INADDR_LOOPBACK in host byte order.
 */
void
__pmSockAddrInit(__pmSockAddr *addr, int family, int address, int port)
{
    PRStatus prStatus;
    switch(address) {
    case INADDR_ANY:
        prStatus = PR_SetNetAddr(PR_IpAddrAny, family, port, &addr->sockaddr);
	break;
    case INADDR_LOOPBACK:
        prStatus = PR_SetNetAddr(PR_IpAddrLoopback, family, port, &addr->sockaddr);
	break;
    default:
	__pmNotifyErr(LOG_ERR, "__pmSockAddrInit: Invalid address %d\n", address);
	return;
    }
    if (prStatus != PR_SUCCESS)
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrInit: PR_InitializeNetAddr failure: %d\n", PR_GetError());
}

void
__pmSockAddrSetFamily(__pmSockAddr *addr, int family)
{
    if (family == AF_INET)
        addr->sockaddr.raw.family = PR_AF_INET;
    else if (family == AF_INET6)
        addr->sockaddr.raw.family = PR_AF_INET6;
    else
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrSetFamily: Invalid address family: %d\n", family);
}

int
__pmSockAddrGetFamily(const __pmSockAddr *addr)
{
    if (addr->sockaddr.raw.family == PR_AF_INET)
        return AF_INET;
    if (addr->sockaddr.raw.family == PR_AF_INET6)
        return AF_INET6;
    __pmNotifyErr(LOG_ERR, "__pmSockAddrGetFamily: Invalid address family: %d\n",
		  addr->sockaddr.raw.family);
    return 0; /* not set */
}

void
__pmSockAddrSetPort(__pmSockAddr *addr, int port)
{
    if (addr->sockaddr.raw.family == PR_AF_INET)
        addr->sockaddr.inet.port = htons(port);
    else if (addr->sockaddr.raw.family == PR_AF_INET6)
        addr->sockaddr.ipv6.port = htons(port);
    else
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrSetPort: Invalid address family: %d\n", addr->sockaddr.raw.family);
}

int
__pmListen(int fd, int backlog)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
        return PR_Listen(socket.nsprFd, backlog) == PR_SUCCESS ? 0 : -1;
    return listen(fd, backlog);
}

int
__pmAccept(int fd, void *addr, __pmSockLen *addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	__pmSockAddr *nsprAddr = (__pmSockAddr *)addr;
	PRIntervalTime timer;
	PRFileDesc *nsprFd;
	int msec;

	msec = __pmConvertTimeout(TIMEOUT_CONNECT);
	timer = PR_MillisecondsToInterval(msec);
	nsprFd = PR_Accept(socket.nsprFd, &nsprAddr->sockaddr, timer);
	if (nsprFd == NULL)
	    return -1;

	/* Add the accepted socket to the fd table. */
	fd = newNSPRHandle();
	socket.nsprFd = nsprFd;
	__pmSetDataIPC(fd, (void *)&socket);
	return fd;
    }
    return accept(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	__pmSockAddr *nsprAddr = (__pmSockAddr *)addr;
	PRSocketOptionData socketOption;

	/*
	 * Allow the socket address to be reused, in case we want the
	 * same port across a service restart.
	 */
	socketOption.option = PR_SockOpt_Reuseaddr;
	socketOption.value.reuse_addr = PR_TRUE;
	PR_SetSocketOption(socket.nsprFd, &socketOption);

	return (PR_Bind(socket.nsprFd, &nsprAddr->sockaddr)
		== PR_SUCCESS) ? 0 : -1;
    }
    return bind(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmConnect(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	PRIntervalTime timer;
	int msec;
	PRStatus sts;

	msec = __pmConvertTimeout(TIMEOUT_CONNECT);
	timer = PR_MillisecondsToInterval(msec);
	sts = PR_Connect(socket.nsprFd, (PRNetAddr *)addr, timer);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    PRStatus	prStatus;
	    char	buf[1024]; // at least PM_NET_ADDR_STRING_SIZE

	    prStatus = PR_NetAddrToString((PRNetAddr *)addr, buf, sizeof(buf));
	    fprintf(stderr, "__pmConnect(fd=%d(nsprFd=%p), %s) ->",
		fd, socket.nsprFd,
		prStatus == PR_SUCCESS ? buf : "<unknown addr>");
	    if (sts == PR_SUCCESS)
		fprintf(stderr, " OK\n");
	    else {
		int code = __pmSecureSocketsError(PR_GetError());
		pmErrStr_r(code, buf, sizeof(buf));
		fprintf(stderr, " %s\n", buf);
	    }
	}
#endif
	return (sts == PR_SUCCESS) ? 0 : -1;
    }
    return connect(fd, (struct sockaddr *)addr, addrlen);
}

int
__pmGetFileStatusFlags(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_GETFL);
}

int
__pmSetFileStatusFlags(int fd, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_SETFL, flags);
}

int
__pmGetFileDescriptorFlags(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_GETFD);
}

int
__pmSetFileDescriptorFlags(int fd, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	/*
	 * There is no direct mapping of this function in NSPR.
	 * Best we can do is to call fcntl on the native fd.
	 */
        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
    }
    return fcntl(fd, F_SETFD, flags);
}

ssize_t
__pmWrite(int fd, const void *buffer, size_t length)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t	size = PR_Write(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError(PR_GetError());
	return size;
    }
    return write(fd, buffer, length);
}

ssize_t
__pmRead(int fd, void *buffer, size_t length)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t size = PR_Read(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError(PR_GetError());
	return size;
    }
    return read(fd, buffer, length);
}

ssize_t
__pmSend(int fd, const void *buffer, size_t length, int flags)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	ssize_t size = PR_Write(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError(PR_GetError());
	return size;
    }
    return send(fd, buffer, length, flags);
}

ssize_t
__pmRecv(int fd, void *buffer, size_t length, int flags)
{
    __pmSecureSocket	socket;
    ssize_t		size;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "%s:__pmRecv[secure](", __FILE__);
	}
#endif
	size = PR_Read(socket.nsprFd, buffer, length);
	if (size < 0)
	    __pmSecureSocketsError(PR_GetError());
    }
    else {
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    fprintf(stderr, "%s:__pmRecv(", __FILE__);
	}
#endif
	size = recv(fd, buffer, length, flags);
    }
#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_PDU) && (pmDebug & DBG_TRACE_DESPERATE)) {
	fprintf(stderr, "%d, ..., %d, " PRINTF_P_PFX "%x) -> %d\n",
	    fd, (int)length, flags, (int)size);
    }
#endif
    return size;
}

int
__pmFD(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd)
        return PR_FileDesc2NativeHandle(socket.nsprFd);
    return fd;
}

void
__pmFD_CLR(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	FD_CLR(fd, &set->nspr_set);
	/* Reset the max fd, if necessary. */
	if (fd + 1 >= set->num_nspr_fds) {
	    for (--fd; fd >= 0; --fd) {
		if (FD_ISSET(fd, &set->nspr_set))
		    break;
	    }
	    set->num_nspr_fds = fd + 1;
	}
    } else {
	FD_CLR(fd, &set->native_set);
	/* Reset the max fd, if necessary. */
	if (fd + 1 >= set->num_native_fds) {
	    for (--fd; fd >= 0; --fd) {
		if (FD_ISSET(fd, &set->native_set))
		    break;
	    }
	    set->num_native_fds = fd + 1;
	}
    }
}

int
__pmFD_ISSET(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	return FD_ISSET(fd, &set->nspr_set);
    }
    return FD_ISSET(fd, &set->native_set);
}

void
__pmFD_SET(int fd, __pmFdSet *set)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
        fd -= NSPR_HANDLE_BASE;
	FD_SET(fd, &set->nspr_set);
	/* Reset the max fd, if necessary. */
	if (fd >= set->num_nspr_fds)
	    set->num_nspr_fds = fd + 1;
    } else {
	FD_SET(fd, &set->native_set);
	/* Reset the max fd, if necessary. */
	if (fd >= set->num_native_fds)
	    set->num_native_fds = fd + 1;
    }
}

void
__pmFD_ZERO(__pmFdSet *set)
{
    FD_ZERO(&set->nspr_set);
    FD_ZERO(&set->native_set);
    set->num_nspr_fds = 0;
    set->num_native_fds = 0;
}

void
__pmFD_COPY(__pmFdSet *s1, const __pmFdSet *s2)
{
    memcpy(s1, s2, sizeof(*s1));
}

static int
nsprSelect(int rwflag, __pmFdSet *fds, struct timeval *timeout)
{
    __pmSecureSocket socket;
    fd_set	combined;
    int		numCombined;
    int		fd;
    int		ready;
    int		nativeFD;
    char	errmsg[PM_MAXERRMSGLEN];

    /*
     * fds contains two sets; one of native file descriptors and one of
     * NSPR file descriptors. We can't poll them separately, since one
     * may block the other.  We can either convert the native file
     * descriptors to NSPR or vice-versa.  The NSPR function PR_Poll
     * does not seem to respond to SIGINT, so we will convert the NSPR
     * file descriptors to native ones and use select(2) to do the
     * polling.
     * First initialize our working set from the set of native file
     * descriptors in fds.
     */
    combined = fds->native_set;
    numCombined = fds->num_native_fds;

    /* Now add the native fds associated with the NSPR fds in nspr_set, if any. */
    for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
        if (FD_ISSET(fd, &fds->nspr_set)) {
	    __pmDataIPC(NSPR_HANDLE_BASE + fd, &socket);
	    nativeFD = PR_FileDesc2NativeHandle(socket.nsprFd);
	    FD_SET(nativeFD, &combined);
	    if (nativeFD >= numCombined)
		numCombined = nativeFD + 1;
	}
    }

    /* Use the select(2) function to do the polling. Ignore the nfds passed to us
       and use the number that we have computed. */
    if (rwflag == PR_POLL_READ)
        ready = select(numCombined, &combined, NULL, NULL, timeout);
    else
        ready = select(numCombined, NULL, &combined, NULL, timeout);
    if (ready < 0 && neterror() != EINTR) {
        __pmNotifyErr(LOG_ERR, "nsprSelect: error polling file descriptors: %s\n",
		      netstrerror_r(errmsg, sizeof(errmsg)));
	return -1;
    }

    /* Separate the results into their corresponding sets again. */
    for (fd = 0; fd < fds->num_nspr_fds; ++fd) {
        if (FD_ISSET(fd, &fds->nspr_set)) {
	   __pmDataIPC(NSPR_HANDLE_BASE + fd, &socket);
	   nativeFD = PR_FileDesc2NativeHandle(socket.nsprFd);

	   /* As we copy the result to the nspr set, make sure the bit is cleared in the
	      combined set. That way we can simply copy the resulting combined set to the
	      native set when we're done. */
	   if (! FD_ISSET(nativeFD, &combined))
	       FD_CLR(fd, &fds->nspr_set);
	   else
	       FD_CLR(nativeFD, &combined);
	}
    }
    fds->native_set = combined;

    /* Reset the size of each set. */
    while (fds->num_nspr_fds > 0 && ! FD_ISSET(fds->num_nspr_fds - 1, &fds->nspr_set))
	--fds->num_nspr_fds;
    while (fds->num_native_fds > 0 && ! FD_ISSET(fds->num_native_fds - 1, &fds->native_set))
	--fds->num_native_fds;

    /* Return the total number of ready fds. */
    return ready;
}

int
__pmSelectRead(int nfds, __pmFdSet *readfds, struct timeval *timeout)
{
    return nsprSelect(PR_POLL_READ, readfds, timeout);
}

int
__pmSelectWrite(int nfds, __pmFdSet *writefds, struct timeval *timeout)
{
    return nsprSelect(PR_POLL_WRITE, writefds, timeout);
}

/*
 * In certain situations, we need to allow access to previously-read
 * data on a socket.  This is because, for example, the SSL protocol
 * buffering may have already consumed data that we are now expecting
 * (in this case, its buffered internally and a socket read will give
 * up that data).
 *
 * PR_Poll does not seem to play well here and so we need to use the
 * native select-based mechanism to block and/or query the state of
 * pending data.
 */
int
__pmSocketReady(int fd, struct timeval *timeout)
{
    __pmSecureSocket socket;
    __pmFdSet onefd;

    if (__pmDataIPC(fd, &socket) == 0 && socket.sslFd)
        if (SSL_DataPending(socket.sslFd))
	    return 1;	/* proceed without blocking */

    __pmFD_ZERO(&onefd);
    __pmFD_SET(fd, &onefd);
    return nsprSelect(PR_POLL_READ, &onefd, timeout);
}

char *
__pmGetNameInfo(__pmSockAddr *address)
{
    char buffer[PR_NETDB_BUF_SIZE];
    char *name;
    PRHostEnt he;
    PRStatus prStatus = PR_GetHostByAddr(&address->sockaddr, &buffer[0], sizeof(buffer), &he);
    name = (prStatus == PR_SUCCESS ? strdup(he.h_name) : NULL);
    return name;
}

__pmHostEnt *
__pmGetAddrInfo(const char *hostName)
{
    void *null;
    __pmSockAddr *addr;
    __pmHostEnt *he = __pmHostEntAlloc();

    if (he != NULL) {
        he->addresses = PR_GetAddrInfoByName(hostName, PR_AF_UNSPEC, PR_AI_ADDRCONFIG);
	if (he->addresses == NULL) {
	    __pmHostEntFree(he);
	    return NULL;
	}
    }

    /* Try to reverse lookup the host name. */
    null = NULL;
    addr = __pmHostEntGetSockAddr(he, &null);
    if (addr != NULL) {
        he->name = __pmGetNameInfo(addr);
	__pmSockAddrFree(addr);
	if (he->name == NULL)
	    he->name = strdup(hostName);
    }
    else
        he->name = strdup(hostName);

    return he;
}

char *
__pmHostEntGetName(const __pmHostEnt *he)
{
     if (he->name == NULL)
        return NULL;
     return strdup(he->name);
}

__pmSockAddr *
__pmHostEntGetSockAddr(const __pmHostEnt *he, void **ei)
{
  __pmSockAddr* addr;

    addr = __pmSockAddrAlloc();
    if (addr == NULL) {
        __pmNotifyErr(LOG_ERR, "__pmHostEntGetSockAddr: out of memory\n");
        *ei = NULL;
	return NULL;
    }

    *ei = PR_EnumerateAddrInfo(*ei, he->addresses, 0, &addr->sockaddr);
    if (*ei == NULL) {
        /* End of the address chain or an error. No address to return. */
	__pmSockAddrFree(addr);
	return NULL;
    }
    return addr;
}

__pmSockAddr *
__pmSockAddrMask(__pmSockAddr *addr, const __pmSockAddr *mask)
{
    int i;
    if (addr->sockaddr.raw.family != mask->sockaddr.raw.family) {
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrMask: Address family of the address (%d) must match that of the mask (%d)\n",
		addr->sockaddr.raw.family, mask->sockaddr.raw.family);
    }
    else if (addr->sockaddr.raw.family == PR_AF_INET)
        addr->sockaddr.inet.ip &= mask->sockaddr.inet.ip;
    else if (addr->sockaddr.raw.family == PR_AF_INET6) {
        /* IPv6: Mask it byte by byte */
        char *addrBytes = (char *)&addr->sockaddr.ipv6.ip;
	const char *maskBytes = (const char *)&mask->sockaddr.ipv6.ip;
	for (i = 0; i < sizeof(addr->sockaddr.ipv6.ip); ++i)
            addrBytes[i] &= maskBytes[i];
    }
    else
	__pmNotifyErr(LOG_ERR,
		"__pmSockAddrMask: Invalid address family: %d\n", addr->sockaddr.raw.family);
    return addr;
}

int
__pmSockAddrCompare(const __pmSockAddr *addr1, const __pmSockAddr *addr2)
{
    if (addr1->sockaddr.raw.family != addr2->sockaddr.raw.family)
        return addr1->sockaddr.raw.family - addr2->sockaddr.raw.family;

    if (addr1->sockaddr.raw.family == PR_AF_INET)
        return addr1->sockaddr.inet.ip - addr2->sockaddr.inet.ip;

    if (addr1->sockaddr.raw.family == PR_AF_INET6) {
        /* IPv6: Compare it byte by byte */
      return memcmp(&addr1->sockaddr.ipv6.ip, &addr2->sockaddr.ipv6.ip,
		    sizeof(addr1->sockaddr.ipv6.ip));
    }

    __pmNotifyErr(LOG_ERR,
		  "__pmSockAddrCompare: Invalid address family: %d\n", addr1->sockaddr.raw.family);
    return 1; /* not equal */
}

int
__pmSockAddrIsInet(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.family == PR_AF_INET;
}

int
__pmSockAddrIsIPv6(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.family == PR_AF_INET6;
}

__pmSockAddr *
__pmStringToSockAddr(const char *cp)
{
    PRStatus prStatus;
    __pmSockAddr *addr = __pmSockAddrAlloc();

    if (addr) {
        if (cp == NULL || strcmp(cp, "INADDR_ANY") == 0) {
	    prStatus = PR_InitializeNetAddr (PR_IpAddrAny, 0, &addr->sockaddr);
	    /* Set the address family to 0, meaning "not set". */
	    addr->sockaddr.raw.family = 0;
	}
	else
	    prStatus = PR_StringToNetAddr(cp, &addr->sockaddr);

	if (prStatus != PR_SUCCESS) {
	    __pmSockAddrFree(addr);
	    addr = NULL;
	}
    }

    return addr;
}

/*
 * Convert an address to a string.
 * The caller must free the buffer.
 */
#define PM_NET_ADDR_STRING_SIZE 46 /* from the NSPR API reference */

char *
__pmSockAddrToString(__pmSockAddr *addr)
{
    PRStatus	prStatus;
    char	*buf = malloc(PM_NET_ADDR_STRING_SIZE);

    if (buf) {
	prStatus = PR_NetAddrToString(&addr->sockaddr, buf, PM_NET_ADDR_STRING_SIZE);
	if (prStatus != PR_SUCCESS) {
	    free(buf);
	    return NULL;
	}
    }
    return buf;
}
