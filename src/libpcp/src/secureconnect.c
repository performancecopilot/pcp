/*
 * Copyright (c) 2012-2015 Red Hat.
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
 * of (NSS/SSL/NSPR/SASL).  This is allocated once a connection is upgraded
 * from insecure to secure.
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
    if (PR_Initialized() == PR_TRUE)
	PR_Cleanup();
    return 0;
}

static int
__pmSetupSecureSocket(int fd, __pmSecureSocket *socket)
{
    /* Is this socket already set up? */
    if (socket->nsprFd)
	return 0;

    /* Import the fd into NSPR. */
    socket->nsprFd = PR_ImportTCPSocket(fd);
    if (! socket->nsprFd)
	return -1;

    return 0;
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
	    PR_Close(socket.nsprFd);
	    socket.nsprFd = NULL;
	    socket.sslFd = NULL;
	    fd = -1;
	}
    }

    if (fd != -1) {
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
    const PRUint16 *cipher;
    PK11SlotInfo *slot;
    SECStatus secsts;
    static int initialized;

    /* Only attempt this once. */
    if (initialized)
	return 0;
    initialized = 1;

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
    for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher)
	SSL_CipherPolicySet(*cipher, SSL_ALLOWED);
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

static const char * /* don't free()! */ 
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
    const char *value = NULL;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthRealmCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (id != SASL_CB_GETREALM)
	return SASL_FAIL;

    value = __pmGetAttrValue(PCP_ATTR_REALM, attrs, "Realm: ");
    *result = value;

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
    const char *value = NULL;
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
    const char *password;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSecretCB enter ctx=%p id=%#x\n", __FILE__, context, id);

    if (saslconn == NULL || secret == NULL || id != SASL_CB_PASS)
	return SASL_BADPARAM;

    password = __pmGetAttrValue(PCP_ATTR_PASSWORD, attrs, "Password: ");
    length = password ? strlen(password) : 0;

    *secret = (sasl_secret_t *) calloc(1, sizeof(sasl_secret_t) + length + 1);
    if (!*secret) {
	return SASL_NOMEM;
    }

    if (password) {
	(*secret)->len = length;
	strcpy((char *)(*secret)->data, password);
    }

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "%s:__pmAuthSecretCB ctx=%p id=%#x -> data=%s len=%u\n",
		__FILE__, context, id, password, (unsigned)length);

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
__pmSecureClientInit(int flags)
{
    int sts;

    /* Ensure correct security lib initialisation order */
    __pmInitSecureSockets();

    /*
     * If secure sockets functionality available, iterate over the set of
     * known locations for certificate databases and attempt to initialise
     * one of them for our use.
     */
    sts = 0;
    if ((flags & PDU_FLAG_NO_NSS_INIT) == 0) {
	sts = __pmInitCertificates();
	if (sts < 0)
	    __pmNotifyErr(LOG_WARNING, "__pmConnectPMCD: "
			  "certificate database exists, but failed initialization");
    }
    return sts;
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

    if ((flags & PDU_FLAG_SECURE) != 0) {
	sts = __pmSecureClientInit(flags);
	if (sts < 0)
	    return sts;
	sts = __pmSetupSecureSocket(fd, &socket);
	if (sts < 0)
	    return __pmSecureSocketsError(PR_GetError());
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
	/*
	 * The current implementation of compression requires an SSL/TLS
	 * connection.
	 */
	if (socket.sslFd == NULL)
	    return -EOPNOTSUPP;
#ifdef SSL_ENABLE_DEFLATE
	secsts = SSL_OptionSet(socket.sslFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
#else
	/*
	 * On some older platforms (e.g. CentOS 5.5) SSL_ENABLE_DEFLATE
	 * is not defined ...
	 */
	return -EOPNOTSUPP;
#endif /*SSL_ENABLE_DEFLATE*/
    }

    if ((flags & PDU_FLAG_AUTH) != 0) {
	__pmInitAuthClients();
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
    if (sasl_server_init(common_callbacks, pmProgname) != SASL_OK) {
	__pmNotifyErr(LOG_ERR, "Failed to start authenticating server");
	return -EINVAL;
    }
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

    if (pinned > 0)
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

	if (pinned > 0)
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

    /*
     * If the server uses the secure-ack protocol, then expect an error
     * PDU here containing the server's secure status. If the status is zero,
     * then all is ok, otherwise, return the status to the caller.
     */
    if (flags & PDU_FLAG_SECURE_ACK) {
	__pmPDU *rpdu;
	int pinpdu;
	int serverSts;

	pinpdu = sts = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &rpdu);
	if (sts != PDU_ERROR) {
	    if (pinpdu > 0)
		__pmUnpinPDUBuf(&rpdu);
	    return -PM_ERR_IPC;
	}
	sts = __pmDecodeError(rpdu, &serverSts);
	if (pinpdu > 0)
	    __pmUnpinPDUBuf(&rpdu);
	if (sts < 0)
	    return sts;
	if (serverSts < 0)
	    return serverSts;
    }

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

static void
sendSecureAck(int fd, int flags, int sts) {
    /*
     * At this point we've attempted some required initialization for secure
     * sockets. If the client wants a secure-ack then send an error pdu
     * containing our status. The client will then know whether or not to
     * proceed with the secure handshake.
     */
    if (flags & PDU_FLAG_SECURE_ACK)
	__pmSendError (fd, FROM_ANON, sts);
}

int
__pmSecureServerIPCFlags(int fd, int flags)
{
    __pmSecureSocket socket;
    SECStatus secsts;
    int saslsts;
    int sts;

    if (__pmDataIPC(fd, &socket) < 0)
	return -EOPNOTSUPP;

    if ((flags & PDU_FLAG_SECURE) != 0) {
	sts = __pmSecureServerInit();
	if (sts < 0) {
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	sts = __pmSetupSecureSocket(fd, &socket);
	if (sts < 0) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	if ((socket.sslFd = SSL_ImportFD(NULL, socket.nsprFd)) == NULL) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	socket.nsprFd = socket.sslFd;

	secsts = SSL_OptionSet(socket.sslFd, SSL_NO_LOCKS, PR_TRUE);
	if (secsts != SECSuccess) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	secsts = SSL_OptionSet(socket.sslFd, SSL_SECURITY, PR_TRUE);
	if (secsts != SECSuccess) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	secsts = SSL_OptionSet(socket.sslFd, SSL_HANDSHAKE_AS_SERVER, PR_TRUE);
	if (secsts != SECSuccess) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUEST_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	secsts = SSL_OptionSet(socket.sslFd, SSL_REQUIRE_CERTIFICATE, PR_FALSE);
	if (secsts != SECSuccess) {
	    sts = __pmSecureSocketsError(PR_GetError());
	    sendSecureAck(fd, flags, sts);
	    return sts;
	}
	sendSecureAck(fd, flags, sts);
    }

    if ((flags & PDU_FLAG_COMPRESS) != 0) {
	/*
	 * The current implementation of compression requires an SSL/TLS
	 * connection.
	 */
	if (socket.sslFd == NULL)
	    return -EOPNOTSUPP;
#ifdef SSL_ENABLE_DEFLATE
	secsts = SSL_OptionSet(socket.sslFd, SSL_ENABLE_DEFLATE, PR_TRUE);
	if (secsts != SECSuccess)
	    return __pmSecureSocketsError(PR_GetError());
#else
	/*
	 * On some older platforms (e.g. CentOS 5.5) SSL_ENABLE_DEFLATE
	 * is not defined ...
	 */
	return -EOPNOTSUPP;
#endif /*SSL_ENABLE_DEFLATE*/
    }

    if ((flags & PDU_FLAG_AUTH) != 0) {
	sts = __pmInitAuthServer();
	if (sts < 0)
	    return sts;
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

    FD_ZERO(&onefd);
    FD_SET(fd, &onefd);
    return select(fd+1, &onefd, NULL, NULL, timeout);
}
