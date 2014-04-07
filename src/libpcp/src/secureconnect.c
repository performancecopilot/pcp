/*
 * Copyright (c) 2012-2014 Red Hat.
 * Security and Authentication (NSS and SASL) support.  Client side.
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
#include <assert.h>
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
#ifdef PCP_DEBUG
    if (pmDebug & DBG_TRACE_DESPERATE)
        fprintf(stderr, "%s:__pmHostEntFree(hostent=%p) name=%p (%s) addresses=%p\n", __FILE__, hostent, hostent->name, hostent->name, hostent-> addresses);
#endif
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
    sasl_conn_t *saslConn;
    sasl_callback_t *saslCB;
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
__pmSetupSocket(PRFileDesc *fdp, int family)
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
    (void)__pmInitSocket(fd, family);	/* cannot fail after __pmSetDataIPC */
    return fd;
}

int
__pmCreateSocket(void)
{
    PRFileDesc *fdp;

    __pmInitSecureSockets();
    if ((fdp = PR_OpenTCPSocket(PR_AF_INET)) == NULL)
	return -neterror();
    return __pmSetupSocket(fdp, AF_INET);
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
	__pmNotifyErr(LOG_ERR, "%s:__pmCreateIPv6Socket: IPV6 is not supported\n", __FILE__);
	PR_Close(fdp);
	return -EOPNOTSUPP;
    }

    return __pmSetupSocket(fdp, AF_INET6);
}

int
__pmCreateUnixSocket(void)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    PRFileDesc *fdp;

    __pmInitSecureSockets();
    if ((fdp = PR_OpenTCPSocket(PR_AF_LOCAL)) == NULL)
	return -neterror();
    return __pmSetupSocket(fdp, AF_UNIX);
#else
    __pmNotifyErr(LOG_ERR, "%s:__pmCreateUnixSocket: AF_UNIX is not supported\n", __FILE__);
    return -EOPNOTSUPP;
#endif
}

void
__pmCloseSocket(int fd)
{
    __pmSecureSocket socket;
    int sts;

    sts = __pmDataIPC(fd, (void *)&socket);
    __pmResetIPC(fd);

    if (sts == 0) {
	if (socket.saslConn) {
	    sasl_dispose(&socket.saslConn);
	    socket.saslConn = NULL;
	}
	if (socket.saslCB) {
	    free(socket.saslCB);
	    socket.saslCB = NULL;
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

static char *
dbpath(char *path, size_t size, char *db_method)
{
    int sep = __pmPathSeparator();
    const char *empty_homedir = "";
    char *homedir = getenv("HOME");
    char *nss_method = getenv("PCP_SECURE_DB_METHOD");

    if (homedir == NULL)
	homedir = (char *)empty_homedir;
    if (nss_method == NULL)
	nss_method = db_method;

    /*
     * Fill in a buffer with the users NSS database specification.
     * Return a pointer to the filesystem path component - without
     * the <method>:-prefix - for other routines to work with.
     */
    snprintf(path, size, "%s%s" "%c" ".pki" "%c" "nssdb",
		nss_method, homedir, sep, sep);
    return path + strlen(nss_method);
}

static char *
dbphrase(PK11SlotInfo *slot, PRBool retry, void *arg)
{
    (void)arg;
    if (retry)
	return NULL;
    assert(PK11_IsInternal(slot));
    return strdup(SECURE_USERDB_DEFAULT_KEY);
}

int
__pmInitCertificates(void)
{
    char nssdb[MAXPATHLEN];
    PK11SlotInfo *slot;
    SECStatus secsts;

    PK11_SetPasswordFunc(dbphrase);

    /*
     * Check for client certificate databases.  We enforce use
     * of the per-user shared NSS database at $HOME/.pki/nssdb
     * For simplicity, we create this directory if we need to.
     * If we cannot, we silently bail out so that users who're
     * not using secure connections (initially everyone) don't
     * have to diagnose / put up with spurious errors.
     */
    if (__pmMakePath(dbpath(nssdb, sizeof(nssdb), "sql:"), 0700) < 0)
	return 0;
    secsts = NSS_InitReadWrite(nssdb);

    if (secsts != SECSuccess) {
	/* fallback, older versions of NSS do not support sql: */
	dbpath(nssdb, sizeof(nssdb), "");
	secsts = NSS_InitReadWrite(nssdb);
    }

    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    if ((slot = PK11_GetInternalKeySlot()) != NULL) {
	if (PK11_NeedUserInit(slot))
	    PK11_InitPin(slot, NULL, SECURE_USERDB_DEFAULT_KEY);
	else if (PK11_NeedLogin(slot))
	    PK11_Authenticate(slot, PR_FALSE, NULL);
	PK11_FreeSlot(slot);
    }

    /* Some NSS versions don't do this correctly in NSS_SetDomesticPolicy. */
    do {
        const PRUint16 *cipher;
        for (cipher = SSL_ImplementedCiphers; *cipher != 0; ++cipher)
            SSL_CipherPolicySet(*cipher, SSL_ALLOWED);
    } while (0);
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
__pmAuthLogCB(void *context, int priority, const char *message)
{
    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthLogCB enter ctx=%p pri=%d\n", __FILE__, context, priority);

    if (!message)
	return SASL_BADPARAM;
    switch (priority) {
    case SASL_LOG_NONE:
	return SASL_OK;
    case SASL_LOG_ERR:
	priority = LOG_ERR;
	break;
    case SASL_LOG_FAIL:
	priority = LOG_ALERT;
	break;
    case SASL_LOG_WARN:
	priority = LOG_WARNING;
	break;
    case SASL_LOG_NOTE:
	priority = LOG_NOTICE;
	break;
    case SASL_LOG_DEBUG:
    case SASL_LOG_TRACE:
    case SASL_LOG_PASS:
	if (pmDebug & DBG_TRACE_AUTH)
	    priority = LOG_DEBUG;
	else
	    return SASL_OK;
	break;
    default:
	priority = LOG_INFO;
	break;
    }
    __pmNotifyErr(priority, "%s", message);
    return SASL_OK;
}

#if !defined(IS_MINGW)
static void echoOff(int fd)
{
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag &= ~ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
}

static void echoOn(int fd)
{
    if (isatty(fd)) {
        struct termios tio;
        tcgetattr(fd, &tio);
        tio.c_lflag |= ECHO;
        tcsetattr(fd, TCSAFLUSH, &tio);
    }
}
#define fgetsQuietly(buf,length,input) fgets(buf,length,input)
#define consoleName "/dev/tty"
#else
#define echoOn(fd) do { } while (0)
#define echoOff(fd) do { } while (0)
#define consoleName "CON:"
static char *fgetsQuietly(char *buf, int length, FILE *input)
{
    int c;
    char *end = buf;

    if (!isatty(fileno(input)))
        return fgets(buf, length, input);
    do {
        c = getch();
        if (c == '\b') {
            if (end > buf)
                end--;
        }
	else if (--length > 0)
            *end++ = c;
        if (!c || c == '\n' || c == '\r')
	    break;
    } while (1);

    return buf;
}
#endif

static char *fgetsPrompt(FILE *in, FILE *out, const char *prompt, int secret)
{
    size_t length;
    int infd  = fileno(in);
    int isTTY = isatty(infd);
    char *value, phrase[256];

    if (isTTY) {
	fprintf(out, "%s", prompt);
	fflush(out);
	if (secret)
	    echoOff(infd);
    }

    memset(phrase, 0, sizeof(phrase));
    value = fgetsQuietly(phrase, sizeof(phrase)-1, in);
    if (!value)
	return strdup("");
    length = strlen(value) - 1;
    while (length && (value[length] == '\n' || value[length] == '\r'))
	value[length] = '\0';

    if (isTTY && secret) {
	fprintf(out, "\n");
	echoOn(infd);
    }

    return strdup(value);
}

/*
 * SASL is calling us looking for the value for a specific attribute;
 * we must respond as best we can:
 * - if user specified it on the command line, its in the given hash
 * - if we can interact, we can ask the user for it (taking care for
 *   sensitive info like passwords, not to echo back to the user)
 * Also take care to handle non-console interactive modes, like the
 * pmchart case.  Further, we should consider a mode where we extract
 * these values from a per-user config file too (ala. libvirt).
 *
 * Return value is a dynamically allocated string, caller must free.
 */

static char *
__pmGetAttrConsole(const char *prompt, int secret)
{
    FILE *input, *output;
    char *value, *console;

    /*
     * Interactive mode: open terminal and discuss with user
     * For graphical tools, we do not want to ever be here.
     * For testing, we want to just error out of here ASAP.
     */
    console = getenv("PCP_CONSOLE");
    if (console) {
	if (strcmp(console, "none") == 0)
	    return NULL;
    } else {
	console = consoleName;
    }

    input = fopen(console, "r");
    if (input == NULL) {
	__pmNotifyErr(LOG_ERR, "opening input terminal for read\n");
	return NULL;
    }
    output = fopen(console, "w");
    if (output == NULL) {
	__pmNotifyErr(LOG_ERR, "opening output terminal for write\n");
	fclose(input);
	return NULL;
    }

    value = fgetsPrompt(input, output, prompt, secret);

    fclose(input);
    fclose(output);

    return value;
}

static char *
__pmGetAttrValue(__pmAttrKey key, __pmHashCtl *attrs, const char *prompt)
{
    __pmHashNode *node;
    char *value;

    if ((node = __pmHashSearch(key, attrs)) != NULL)
	return (char *)node->data;
    value = __pmGetAttrConsole(prompt, key == PCP_ATTR_PASSWORD);
    if (value)	/* must track all our own memory use in SASL */
	__pmHashAdd(key, value, attrs);
    return value;
}


static int
__pmAuthRealmCB(void *context, int id, const char **realms, const char **result)
{
    __pmHashCtl *attrs = (__pmHashCtl *)context;
    char *value = NULL;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthRealmCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (id != SASL_CB_GETREALM)
	return SASL_FAIL;

    value = __pmGetAttrValue(PCP_ATTR_REALM, attrs, "Realm: ");
    *result = (const char *)value;

    if (pmDebug & DBG_TRACE_AUTH) {
	fprintf(stderr, "%s:__pmAuthRealmCB ctx=%p, id=%#x, realms=(", __FILE__, context, id);
	if (realms) {
	    if (*realms)
		fprintf(stderr, "%s", *realms);
	    for (value = (char *) *(realms + 1); value && *value; value++)
		fprintf(stderr, " %s", value);
	}
	fprintf(stderr, ") -> rslt=%s\n", *result ? *result : "(none)");
    }
    return SASL_OK;
}

static int
__pmAuthSimpleCB(void *context, int id, const char **result, unsigned *len)
{
    __pmHashCtl *attrs = (__pmHashCtl *)context;
    char *value = NULL;
    int sts;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSimpleCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (!result)
	return SASL_BADPARAM;

    sts = SASL_OK;
    switch (id) {
    case SASL_CB_USER:
    case SASL_CB_AUTHNAME:
	value = __pmGetAttrValue(PCP_ATTR_USERNAME, attrs, "Username: ");
	break;
    case SASL_CB_LANGUAGE:
	break;
    default:
	sts = SASL_BADPARAM;
	break;
    }

    if (len)
	*len = value ? strlen(value) : 0;
    *result = value;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSimpleCB ctx=%p id=%#x -> sts=%d rslt=%p len=%d\n",
		__FILE__, context, id, sts, *result, len ? *len : -1);
    return sts;
}

static int
__pmAuthSecretCB(sasl_conn_t *saslconn, void *context, int id, sasl_secret_t **secret)
{
    __pmHashCtl *attrs = (__pmHashCtl *)context;
    size_t length = 0;
    char *password;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSecretCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (saslconn == NULL || secret == NULL || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    password = __pmGetAttrValue(PCP_ATTR_PASSWORD, attrs, "Password: ");
    length = password ? strlen(password) : 0;

    *secret = (sasl_secret_t *) calloc(1, sizeof(sasl_secret_t) + length + 1);
    if (!*secret) {
	free(password);
	return SASL_NOMEM;
    }

    if (password) {
	(*secret)->len = length;
	strcpy((char *)(*secret)->data, password);
    }

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSecretCB ctx=%p id=%#x -> data=%s len=%u\n",
		__FILE__, context, id, password, (unsigned)length);
    free(password);

    return SASL_OK;
}

static int
__pmAuthPromptCB(void *context, int id, const char *challenge, const char *prompt,
		 const char *defaultresult, const char **result, unsigned *length)
{
    char *value, message[512];

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthPromptCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (id != SASL_CB_ECHOPROMPT && id != SASL_CB_NOECHOPROMPT)
	return SASL_BADPARAM;
    if (!prompt || !result || !length)
	return SASL_BADPARAM;
    if (defaultresult == NULL)
	defaultresult = "";

    if (!challenge)
	snprintf(message, sizeof(message), "%s [%s]: ", prompt, defaultresult);
    else
	snprintf(message, sizeof(message), "%s [challenge: %s] [%s]: ",
		 prompt, challenge, defaultresult);
    message[sizeof(message)-1] = '\0';

    if (id == SASL_CB_ECHOPROMPT) {
	value = __pmGetAttrConsole(message, 0);
	if (value && value[0] != '\0') {
	    *result = value;
	} else {
	    free(value);
	    *result = defaultresult;
	}
    } else {
	if (fgets(message, sizeof(message), stdin) == NULL || message[0])
	    *result = strdup(message);
	else
	    *result = defaultresult;
    }
    if (!*result)
	return SASL_NOMEM;

    *length = (unsigned) strlen(*result);
    return SASL_OK;
}

static int
__pmSecureClientIPCFlags(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    __pmSecureSocket socket;
    sasl_callback_t *cb;
    SECStatus secsts;
    int sts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;
    if (socket.nsprFd == NULL)
	return -EOPNOTSUPP;

    if ((flags & PDU_FLAG_SECURE) != 0) {
	if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL) {
	    __pmNotifyErr(LOG_ERR, "SecureClientIPCFlags: importing socket into SSL");
	    return PM_ERR_IPC;
	}
	socket.nsprFd = socket.sslFd;

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

    if ((flags & PDU_FLAG_AUTH) != 0) {
	socket.saslCB = calloc(LIMIT_CLIENT_CALLBACKS, sizeof(sasl_callback_t));
	if ((cb = socket.saslCB) == NULL)
	    return -ENOMEM;
	cb->id = SASL_CB_USER;
	cb->proc = (sasl_callback_func)&__pmAuthSimpleCB;
	cb->context = (void *)attrs;
	cb++;
	cb->id = SASL_CB_AUTHNAME;
	cb->proc = (sasl_callback_func)&__pmAuthSimpleCB;
	cb->context = (void *)attrs;
	cb++;
	cb->id = SASL_CB_LANGUAGE;
	cb->proc = (sasl_callback_func)&__pmAuthSimpleCB;
	cb++;
	cb->id = SASL_CB_GETREALM;
	cb->proc = (sasl_callback_func)&__pmAuthRealmCB;
	cb->context = (void *)attrs;
	cb++;
	cb->id = SASL_CB_PASS;
	cb->proc = (sasl_callback_func)&__pmAuthSecretCB;
	cb->context = (void *)attrs;
	cb++;
	cb->id = SASL_CB_ECHOPROMPT;
	cb->proc = (sasl_callback_func)&__pmAuthPromptCB;
	cb++;
	cb->id = SASL_CB_NOECHOPROMPT;
	cb->proc = (sasl_callback_func)&__pmAuthPromptCB;
	cb++;
	cb->id = SASL_CB_LIST_END;
	cb++;
	assert(cb - socket.saslCB <= LIMIT_CLIENT_CALLBACKS);

	sts = sasl_client_new(SECURE_SERVER_SASL_SERVICE,
				hostname,
				NULL, NULL, /*iplocal,ipremote*/
				socket.saslCB,
				0, &socket.saslConn);
	if (sts != SASL_OK && sts != SASL_CONTINUE)
	    return __pmSecureSocketsError(sts);
    }

    /* save changes back into the IPC table (updates client sslFd) */
    return __pmSetDataIPC(fd, (void *)&socket);
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

static void
__pmInitAuthPaths(void)
{
    char *path;

    if ((path = getenv("PCP_SASL2_PLUGIN_PATH")) != NULL)
	sasl_set_path(SASL_PATH_TYPE_PLUGIN, path);
    if ((path = getenv("PCP_SASL2_CONFIG_PATH")) != NULL)
	sasl_set_path(SASL_PATH_TYPE_CONFIG, path);
}

static sasl_callback_t common_callbacks[] = { \
	{ .id = SASL_CB_LOG, .proc = (sasl_callback_func)&__pmAuthLogCB },
	{ .id = SASL_CB_LIST_END }};

int
__pmInitAuthClients(void)
{
    __pmInitAuthPaths();
    if (sasl_client_init(common_callbacks) != SASL_OK)
	return -EINVAL;
    return 0;
}

int
__pmInitAuthServer(void)
{
    __pmInitAuthPaths();
    if (sasl_server_init(common_callbacks, pmProgname) != SASL_OK)
	return -EINVAL;
    return 0;
}

static int
__pmAuthClientSetProperties(sasl_conn_t *saslconn, int ssf)
{
    int sts;
    sasl_security_properties_t props;

    /* set external security strength factor */
    if ((sts = sasl_setprop(saslconn, SASL_SSF_EXTERNAL, &ssf)) != SASL_OK)
	return __pmSecureSocketsError(sts);

    /* set general security properties */
    memset(&props, 0, sizeof(props));
    props.maxbufsize = LIMIT_AUTH_PDU;
    props.max_ssf = UINT_MAX;
    if ((sts = sasl_setprop(saslconn, SASL_SEC_PROPS, &props)) != SASL_OK)
	return __pmSecureSocketsError(sts);

    return 0;
}

static int
__pmAuthClientNegotiation(int fd, int ssf, const char *hostname, __pmHashCtl *attrs)
{
    int sts, zero, saslsts = SASL_FAIL;
    int pinned, length, method_length;
    char *payload, buffer[LIMIT_AUTH_PDU];
    const char *method = NULL;
    sasl_conn_t *saslconn;
    __pmHashNode *node;
    __pmPDU *pb;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthClientNegotiation(fd=%d, ssf=%d, host=%s)\n",
		__FILE__, fd, ssf, hostname);

    if ((saslconn = (sasl_conn_t *)__pmGetUserAuthData(fd)) == NULL)
	return -EINVAL;

    /* setup all the security properties for this connection */
    if ((sts = __pmAuthClientSetProperties(saslconn, ssf)) < 0)
	return sts;

    /* lookup users preferred connection method, if specified */
    if ((node = __pmHashSearch(PCP_ATTR_METHOD, attrs)) != NULL)
	method = (const char *)node->data;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthClientNegotiation requesting \"%s\" method\n",
		__FILE__, method ? method : "default");

    /* get security mechanism list */ 
    sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_AUTH) {
	sts = __pmDecodeAuth(pb, &zero, &payload, &length);
	if (sts >= 0) {
	    strncpy(buffer, payload, length);
	    buffer[length] = '\0';

	    if (pmDebug & DBG_TRACE_AUTH)
		fprintf(stderr, "%s:__pmAuthClientNegotiation got methods: "
				"\"%s\" (%d)\n", __FILE__, buffer, length);
	    /*
	     * buffer now contains the list of server mechanisms -
	     * override using users preference (if any) and proceed.
	     */
	    if (method) {
		strncpy(buffer, method, sizeof(buffer));
		buffer[sizeof(buffer) - 1] = '\0';
		length = strlen(buffer);
	    }

	    payload = NULL;
	    saslsts = sasl_client_start(saslconn, buffer, NULL,
					     (const char **)&payload,
					     (unsigned int *)&length, &method);
	    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
		sts = __pmSecureSocketsError(saslsts);
		if (pmDebug & DBG_TRACE_AUTH)
		    fprintf(stderr, "sasl_client_start failed: %d (%s)\n",
				    saslsts, pmErrStr(sts));
	    }
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

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "sasl_client_start chose \"%s\" method, saslsts=%s\n",
		method, saslsts == SASL_CONTINUE ? "continue" : "ok");

    /* tell server we've made a decision and are ready to move on */
    strncpy(buffer, method, sizeof(buffer));
    buffer[sizeof(buffer) - 1] = '\0';
    method_length = strlen(buffer);
    if (payload) {
	if (LIMIT_AUTH_PDU - method_length - 1 < length)
	    return -E2BIG;
	memcpy(buffer + method_length + 1, payload, length);
	length += method_length + 1;
    } else {
	length = method_length + 1;
    }

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "sasl_client_start sending (%d bytes) \"%s\"\n",
		length, buffer);

    if ((sts = __pmSendAuth(fd, FROM_ANON, 0, buffer, length)) < 0)
	return sts;

    while (saslsts == SASL_CONTINUE) {
	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "%s:__pmAuthClientNegotiation awaiting server reply\n", __FILE__);

	sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
	if (sts == PDU_AUTH) {
	    sts = __pmDecodeAuth(pb, &zero, &payload, &length);
	    if (sts >= 0) {
		saslsts = sasl_client_step(saslconn, payload, length, NULL,
						(const char **)&buffer,
						(unsigned int *)&length);
		if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
		    sts = __pmSecureSocketsError(saslsts);
		    if (pmDebug & DBG_TRACE_AUTH)
			fprintf(stderr, "sasl_client_step failed: %d (%s)\n",
					saslsts, pmErrStr(sts));
		    break;
		}
		if (pmDebug & DBG_TRACE_AUTH) {
		    fprintf(stderr, "%s:__pmAuthClientNegotiation"
				    " step recv (%d bytes)", __FILE__, length);
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
	    sts = __pmSendAuth(fd, FROM_ANON, 0, length ? buffer : "", length);
	if (sts < 0)
	    break;
    }

    if (pmDebug & DBG_TRACE_AUTH) {
	if (sts < 0)
	    fprintf(stderr, "%s:__pmAuthClientNegotiation loop failed\n", __FILE__);
	else {
	    saslsts = sasl_getprop(saslconn, SASL_USERNAME, (const void **)&payload);
	    fprintf(stderr, "%s:__pmAuthClientNegotiation success, username=%s\n",
			    __FILE__, saslsts != SASL_OK ? "?" : payload);
	}
    }

    return sts;
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    int sts, ssf = DEFAULT_SECURITY_STRENGTH;

    if (flags & PDU_FLAG_CREDS_REQD) {
	if (__pmHashSearch(PCP_ATTR_UNIXSOCK, attrs) != NULL)
	    return 0;
	flags |= PDU_FLAG_AUTH;	/* force the use of SASL authentication */
    }
    if ((sts = __pmSecureClientIPCFlags(fd, flags, hostname, attrs)) < 0)
	return sts;
    if (((flags & PDU_FLAG_SECURE) != 0) &&
	((sts = __pmSecureClientNegotiation(fd, &ssf)) < 0))
	return sts;
    if (((flags & PDU_FLAG_AUTH) != 0) &&
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
    return (void *)socket.saslConn;
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    __pmSecureSocket socket;
    SECStatus secsts;
    int saslsts;

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

    if ((flags & PDU_FLAG_AUTH) != 0) {
	saslsts = sasl_server_new(SECURE_SERVER_SASL_SERVICE,
				NULL, NULL, /*localdomain,userdomain*/
				NULL, NULL, NULL, /*iplocal,ipremote,callbacks*/
				0, &socket.saslConn);
	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "%s:__pmSecureServerIPCFlags SASL server: %d\n", __FILE__, saslsts);
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
		/*
		 * These options are not related. They are just both options for which
		 * NSPR has no direct mapping.
		 */
#ifdef IS_MINGW
	    case SO_EXCLUSIVEADDRUSE: /* Only exists on MINGW */
#endif
	    {
		/*
		 * There is no direct mapping of this option in NSPR.
		 * The best we can do is to use the native handle and
		 * call setsockopt on that handle.
		 */
	        fd = PR_FileDesc2NativeHandle(socket.nsprFd);
		return setsockopt(fd, level, option_name, option_value, option_len);
	    }
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
	        __pmNotifyErr(LOG_ERR, "%s:__pmSetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			      __FILE__, option_name);
		return -1;
	    }
	    break;
	case IPPROTO_TCP:
	    if (option_name == TCP_NODELAY) {
	        option_data.option = PR_SockOpt_NoDelay;
		option_data.value.no_delay = sockOptValue(option_value, option_len);
		break;
	    }
	    __pmNotifyErr(LOG_ERR, "%s:__pmSetSockOpt: unimplemented option_name for IPPROTO_TCP: %d\n",
			  __FILE__, option_name);
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
	    __pmNotifyErr(LOG_ERR, "%s:__pmSetSockOpt: unimplemented option_name for IPPROTO_IPV6: %d\n",
			  __FILE__, option_name);
	    return -1;
	default:
	    __pmNotifyErr(LOG_ERR, "%s:__pmSetSockOpt: unimplemented level: %d\n", __FILE__, level);
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

#if defined(HAVE_STRUCT_UCRED)
	    case SO_PEERCRED:
#endif
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
			"%s:__pmGetSockOpt: unimplemented option_name for SOL_SOCKET: %d\n",
			__FILE__, option_name);
	      return -1;
	  }
	  break;

	default:
	    __pmNotifyErr(LOG_ERR, "%s:__pmGetSockOpt: unimplemented level: %d\n", __FILE__, level);
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
	__pmNotifyErr(LOG_ERR, "%s:__pmSockAddrInit: Invalid address %d\n", __FILE__, address);
	return;
    }
    if (prStatus != PR_SUCCESS)
	__pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrInit: PR_InitializeNetAddr failure: %d\n", __FILE__, PR_GetError());
}

void
__pmSockAddrSetFamily(__pmSockAddr *addr, int family)
{
    if (family == AF_INET)
        addr->sockaddr.raw.family = PR_AF_INET;
    else if (family == AF_INET6)
        addr->sockaddr.raw.family = PR_AF_INET6;
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (family == AF_UNIX)
        addr->sockaddr.raw.family = PR_AF_LOCAL;
#endif
    else
	__pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrSetFamily: Invalid address family: %d\n", __FILE__, family);
}

int
__pmSockAddrGetFamily(const __pmSockAddr *addr)
{
    if (addr->sockaddr.raw.family == PR_AF_INET)
        return AF_INET;
    if (addr->sockaddr.raw.family == PR_AF_INET6)
        return AF_INET6;
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr->sockaddr.raw.family == PR_AF_LOCAL)
        return AF_UNIX;
#endif
    __pmNotifyErr(LOG_ERR, "%s:__pmSockAddrGetFamily: Invalid address family: %d\n",
		  __FILE__, addr->sockaddr.raw.family);
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
		"%s:__pmSockAddrSetPort: Invalid address family: %d\n", __FILE__, addr->sockaddr.raw.family);
}

int
__pmSockAddrGetPort(const __pmSockAddr *addr)
{
    if (addr->sockaddr.raw.family == PR_AF_INET)
        return ntohs(addr->sockaddr.inet.port);
    if (addr->sockaddr.raw.family == PR_AF_INET6)
        return ntohs(addr->sockaddr.ipv6.port);
    __pmNotifyErr(LOG_ERR,
		  "__pmSockAddrGetPort: Invalid address family: %d\n",
		  addr->sockaddr.raw.family);
    return 0; /* not set */
}

void
__pmSockAddrSetScope(__pmSockAddr *addr, int scope)
{
    if (addr->sockaddr.raw.family == PR_AF_INET6)
        addr->sockaddr.ipv6.scope_id = scope;
}

void
__pmSockAddrSetPath(__pmSockAddr *addr, const char *path)
{
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr->sockaddr.raw.family == PR_AF_LOCAL) {
	strncpy(addr->sockaddr.local.path, path, sizeof(addr->sockaddr.local.path));
	addr->sockaddr.local.path[sizeof(addr->sockaddr.local.path)-1] = '\0';
    } else {
	__pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrSetPath: Invalid address family: %d\n", __FILE__, addr->sockaddr.raw.family);
    }
#else
    __pmNotifyErr(LOG_ERR, "%s:__pmSockAddrSetPath: AF_UNIX is not supported\n", __FILE__);
#endif
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
    __pmSockAddr *sockAddr = (__pmSockAddr *)addr;

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	PRIntervalTime timer;
	PRFileDesc *nsprFd;
	int msec;

	msec = __pmConvertTimeout(TIMEOUT_CONNECT);
	timer = PR_MillisecondsToInterval(msec);
	nsprFd = PR_Accept(socket.nsprFd, &sockAddr->sockaddr, timer);
	if (nsprFd == NULL)
	    return -1;

	/* Add the accepted socket to the fd table. */
	fd = newNSPRHandle();
	socket.nsprFd = nsprFd;
	__pmSetDataIPC(fd, (void *)&socket);
    }
    else
	fd = accept(fd, (struct sockaddr *)sockAddr, addrlen);

    __pmCheckAcceptedAddress(sockAddr);
    return fd;
}

int
__pmBind(int fd, void *addr, __pmSockLen addrlen)
{
    __pmSecureSocket	socket;
    int			family;
    family = __pmSockAddrGetFamily((__pmSockAddr *)addr);

    if (__pmDataIPC(fd, &socket) == 0 && socket.nsprFd) {
	__pmSockAddr *nsprAddr = (__pmSockAddr *)addr;
	PRSocketOptionData socketOption;
#ifdef PCP_DEBUG
	if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    PRStatus	prStatus;
	    char	buf[1024]; // at least PM_NET_ADDR_STRING_SIZE

	    prStatus = PR_NetAddrToString((PRNetAddr *)addr, buf, sizeof(buf));
	    fprintf(stderr, "%s:__pmBind(fd=%d, family=%d, port=%d, addr=%s) using PR_Bind\n",
		__FILE__, fd, family, __pmSockAddrGetPort(nsprAddr),
		prStatus == PR_SUCCESS ? buf : "<unknown addr>");
	}
#endif
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
#ifdef PCP_DEBUG
    if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
	fprintf(stderr, "%s:__pmBind(fd=%d, family=%d, port=%d, addr=%s) using bind\n",
	    __FILE__, fd, family, __pmSockAddrGetPort((__pmSockAddr *)addr),
	    __pmSockAddrToString((__pmSockAddr *)addr));
    }
#endif
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
	if ((pmDebug & DBG_TRACE_CONTEXT) && (pmDebug & DBG_TRACE_DESPERATE)) {
	    PRStatus	prStatus;
	    char	buf[1024]; // at least PM_NET_ADDR_STRING_SIZE

	    prStatus = PR_NetAddrToString((PRNetAddr *)addr, buf, sizeof(buf));
	    fprintf(stderr, "%s:__pmConnect(fd=%d(nsprFd=%p), %s) ->",
		__FILE__, fd, socket.nsprFd,
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
    PRStatus prStatus;

    if (address->sockaddr.raw.family == PR_AF_INET ||
	address->sockaddr.raw.family == PR_AF_INET6) {
	prStatus = PR_GetHostByAddr(&address->sockaddr, &buffer[0], sizeof(buffer), &he);
#ifdef PCP_DEBUG
	if (pmDebug & DBG_TRACE_DESPERATE) {
	    if (prStatus != PR_SUCCESS) {
		fprintf(stderr, "%s:PR_GetHostByAddr(%s) returns %d (%s)\n", __FILE__,
			__pmSockAddrToString(address), PR_GetError(),
			PR_ErrorToString(PR_GetError(), PR_LANGUAGE_I_DEFAULT));
	    }
	}
#endif
	name = (prStatus == PR_SUCCESS ? strdup(he.h_name) : NULL);
	return name;
    }

    if (address->sockaddr.raw.family == PR_AF_LOCAL)
	return strdup(address->sockaddr.local.path);

    __pmNotifyErr(LOG_ERR,
		  "%s:__pmGetNameInfo: Invalid address family: %d\n", __FILE__,
		  address->sockaddr.raw.family);
    return NULL;
}

__pmHostEnt *
__pmGetAddrInfo(const char *hostName)
{
    __pmHostEnt *he = __pmHostEntAlloc();

    if (he != NULL) {
        he->addresses = PR_GetAddrInfoByName(hostName, PR_AF_UNSPEC, PR_AI_ADDRCONFIG | PR_AI_NOCANONNAME);
	if (he->addresses == NULL) {
	    __pmHostEntFree(he);
	    return NULL;
	}
	/* Leave the host name NULL. It will be looked up on demand in __pmHostEntGetName(). */
    }

    return he;
}

__pmSockAddr *
__pmHostEntGetSockAddr(const __pmHostEnt *he, void **ei)
{
  __pmSockAddr* addr;

    addr = __pmSockAddrAlloc();
    if (addr == NULL) {
        __pmNotifyErr(LOG_ERR, "%s:__pmHostEntGetSockAddr: out of memory\n", __FILE__);
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
		"%s:__pmSockAddrMask: Address family of the address (%d) must match that of the mask (%d)\n",
		__FILE__, addr->sockaddr.raw.family, mask->sockaddr.raw.family);
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
#if defined(HAVE_STRUCT_SOCKADDR_UN)
    else if (addr->sockaddr.raw.family == PR_AF_LOCAL) {
	/* Simply truncate the path in the address to the length of the mask. */
	i = strlen(mask->sockaddr.local.path);
	addr->sockaddr.local.path[i] = '\0';
    }
#endif
    else /* not applicable to other address families, e.g. PR_AF_LOCAL. */
	__pmNotifyErr(LOG_ERR,
		"%s:__pmSockAddrMask: Invalid address family: %d\n", __FILE__, addr->sockaddr.raw.family);
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

#if defined(HAVE_STRUCT_SOCKADDR_UN)
    if (addr1->sockaddr.raw.family == PR_AF_LOCAL) {
        /* Unix Domain: Compare the paths */
	return strncmp(addr1->sockaddr.local.path, addr2->sockaddr.local.path,
		       sizeof(addr1->sockaddr.local.path));
    }
#endif

    /* not applicable to other address families, e.g. PR_AF_LOCAL. */
    __pmNotifyErr(LOG_ERR,
		  "%s:__pmSockAddrCompare: Invalid address family: %d\n", __FILE__, addr1->sockaddr.raw.family);
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

int
__pmSockAddrIsUnix(const __pmSockAddr *addr)
{
    return addr->sockaddr.raw.family == PR_AF_LOCAL;
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
#if defined(HAVE_STRUCT_SOCKADDR_UN)
	else if (*cp == __pmPathSeparator()) {
	    if (strlen(cp) >= sizeof(addr->sockaddr.local.path))
		prStatus = PR_FAILURE; /* too long */
	    else {
		addr->sockaddr.raw.family = PR_AF_LOCAL;
		strcpy(addr->sockaddr.local.path, cp);
		prStatus = PR_SUCCESS;
	    }
	}
#endif
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
    char	*buf;

    /* PR_NetAddrToString() does not handle PR_AF_LOCAL. Handle it ourselves. */
    if (addr->sockaddr.raw.family == PR_AF_LOCAL)
	return strdup (addr->sockaddr.local.path);

    /* Otherwise, let NSPR construct the string. */
    buf = malloc(PM_NET_ADDR_STRING_SIZE);
    if (buf) {
	prStatus = PR_NetAddrToString(&addr->sockaddr, buf, PM_NET_ADDR_STRING_SIZE);
	if (prStatus != PR_SUCCESS) {
	    free(buf);
	    return NULL;
	}
    }
    return buf;
}
