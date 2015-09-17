/*
 * Copyright (c) 2012-2015 Red Hat.
 *
 * Server side security features - via Network Security Services (NSS) and
 * the Simple Authentication and Security Layer (SASL).
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
#include <keyhi.h>
#include <secder.h>
#include <pk11pub.h>
#include <sys/stat.h>

#define MAX_NSSDB_PASSWORD_LENGTH	256

static struct {
    /* NSS certificate management */
    CERTCertificate	*certificate;
    SECKEYPrivateKey	*private_key;
    const char		*password_file;
    SSLKEAType		certificate_KEA;
    char		database_path[MAXPATHLEN];

    /* status flags (bitfields) */
    unsigned int	initialized : 1;
    unsigned int	init_failed : 1;
    unsigned int	certificate_verified : 1;	/* NSS */
    unsigned int	ssl_session_cache_setup : 1;	/* NSS */
} secure_server;

int
__pmSecureServerSetFeature(__pmServerFeature wanted)
{
    (void)wanted;
    return 0;	/* nothing dynamically enabled at this stage */
}

int
__pmSecureServerClearFeature(__pmServerFeature clear)
{
    (void)clear;
    return 0;	/* nothing dynamically disabled at this stage */
}

int
__pmSecureServerHasFeature(__pmServerFeature query)
{
    int sts = 0;

    switch (query) {
    case PM_SERVER_FEATURE_SECURE:
	return ! secure_server.init_failed;
    case PM_SERVER_FEATURE_COMPRESS:
    case PM_SERVER_FEATURE_AUTH:
	sts = 1;
	break;
    default:
	break;
    }
    return sts;
}

static int
secure_file_contents(const char *filename, char **passwd, size_t *length)
{
    struct stat	stat;
    size_t	size = *length;
    char	*pass = NULL;
    FILE	*file = NULL;
    int		sts;

    if ((file = fopen(filename, "r")) == NULL)
	goto fail;
    if (fstat(fileno(file), &stat) < 0)
	goto fail;
    if (stat.st_size > size) {
	setoserror(E2BIG);
	goto fail;
    }
    if ((pass = (char *)PORT_Alloc(stat.st_size)) == NULL) {
	setoserror(ENOMEM);
	goto fail;
    }
    sts = fread(pass, 1, stat.st_size, file);
    if (sts < 1) {
	setoserror(EINVAL);
	goto fail;
    }
    while (sts > 0 && (pass[sts-1] == '\r' || pass[sts-1] == '\n'))
	pass[--sts] = '\0';
    *passwd = pass;
    *length = sts;
    fclose(file);
    return 0;

fail:
    sts = -oserror();
    if (file)
	fclose(file);
    if (pass)
	PORT_Free(pass);
    return sts;
}

static char *
certificate_database_password(PK11SlotInfo *info, PRBool retry, void *arg)
{
    size_t length = MAX_NSSDB_PASSWORD_LENGTH;
    char *password = NULL;
    char passfile[MAXPATHLEN];
    int sts;

    (void)arg;
    (void)info;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    passfile[0] = '\0';
    if (secure_server.password_file)
	strncpy(passfile, secure_server.password_file, MAXPATHLEN-1);
    passfile[MAXPATHLEN-1] = '\0';
    PM_UNLOCK(__pmLock_libpcp);

    if (passfile[0] == '\0') {
	__pmNotifyErr(LOG_ERR, "Password sought but no password file given");
	return NULL;
    }
    if (retry) {
	__pmNotifyErr(LOG_ERR, "Retry attempted during password extraction");
	return NULL;	/* no soup^Wretries for you */
    }

    sts = secure_file_contents(passfile, &password, &length);
    if (sts < 0) {
	__pmNotifyErr(LOG_ERR, "Cannot read password file \"%s\": %s",
			passfile, pmErrStr(sts));
	return NULL;
    }
    return password;
}

static int
__pmCertificateTimestamp(SECItem *vtime, char *buffer, size_t size)
{
    PRExplodedTime exploded;
    SECStatus secsts;
    int64 itime;

    switch (vtime->type) {
    case siUTCTime:
	secsts = DER_UTCTimeToTime(&itime, vtime);
	break;
    case siGeneralizedTime:
	secsts = DER_GeneralizedTimeToTime(&itime, vtime);
	break;
    default:
	return -EINVAL;
    }
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    /* Convert to local time */
    PR_ExplodeTime(itime, PR_GMTParameters, &exploded);
    if (!PR_FormatTime(buffer, size, "%a %b %d %H:%M:%S %Y", &exploded))
	return __pmSecureSocketsError(PR_GetError());
    return 0;
}

static void
__pmDumpCertificate(FILE *fp, const char *nickname, CERTCertificate *cert)
{
    CERTValidity *valid = &cert->validity;
    char tbuf[256];

    fprintf(fp, "Certificate: %s", nickname);
    if (__pmCertificateTimestamp(&valid->notBefore, tbuf, sizeof(tbuf)) == 0)
	fprintf(fp, "  Not Valid Before: %s UTC", tbuf);
    if (__pmCertificateTimestamp(&valid->notAfter, tbuf, sizeof(tbuf)) == 0)
	fprintf(fp, "  Not Valid After: %s UTC", tbuf);
}

static int
__pmValidCertificate(CERTCertDBHandle *db, CERTCertificate *cert, PRTime stamp)
{
    SECCertificateUsage usage = certificateUsageSSLServer;
    SECStatus secsts = CERT_VerifyCertificate(db, cert, PR_TRUE, usage,
						stamp, NULL, NULL, &usage);
    return (secsts == SECSuccess);
}

static char *
serverdb(char *path, size_t size, char *db_method)
{
    int sep = __pmPathSeparator();
    char *nss_method = getenv("PCP_SECURE_DB_METHOD");

    if (nss_method == NULL)
	nss_method = db_method;

    /*
     * Fill in a buffer with the server NSS database specification.
     * Return a pointer to the filesystem path component - without
     * the <method>:-prefix - for other routines to work with.
     */
    snprintf(path, size, "%s" "%c" "etc" "%c" "pki" "%c" "nssdb",
		nss_method, sep, sep, sep);
    return path + strlen(nss_method);
}

int
__pmSecureServerSetup(const char *db, const char *passwd)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);

    /* Configure optional (cmdline) password file in case DB locked */
    secure_server.password_file = passwd;

    /*
     * Configure location of the NSS database with a sane default.
     * For servers, we default to the shared (sql) system-wide database.
     * If command line db specified, pass it directly through - allowing
     * any old database format, at the users discretion.
     */
    if (db) {
	/* shortened-buffer-size (-2) guarantees null-termination */
	strncpy(secure_server.database_path, db, MAXPATHLEN-2);
    }

    PM_UNLOCK(__pmLock_libpcp);
    return 0;
}

int
__pmSecureServerInit(void)
{
    const char *nickname = SECURE_SERVER_CERTIFICATE;
    const PRUint16 *cipher;
    SECStatus secsts;
    int pathSpecified;
    int sts = 0;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);

    /* Only attempt this once. */
    if (secure_server.initialized)
	goto done;
    secure_server.initialized = 1;

    if (PR_Initialized() != PR_TRUE)
	PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    /* Configure optional (cmdline) password file in case DB locked */
    PK11_SetPasswordFunc(certificate_database_password);

    /*
     * Configure location of the NSS database with a sane default.
     * For servers, we default to the shared (sql) system-wide database.
     * If command line db specified, pass it directly through - allowing
     * any old database format, at the users discretion.
     */
    if (!secure_server.database_path[0]) {
	const char *path;
	pathSpecified = 0;
	path = serverdb(secure_server.database_path, MAXPATHLEN, "sql:");

	/* this is the default case on some platforms, so no log spam */
	if (access(path, R_OK|X_OK) < 0) {
	    if (pmDebug & DBG_TRACE_CONTEXT)
		__pmNotifyErr(LOG_INFO,
			      "Cannot access system security database: %s",
			      secure_server.database_path);
	    sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
	    secure_server.init_failed = 1;
	    goto done;
	}
    }
    else
	pathSpecified = 1;

    secsts = NSS_Init(secure_server.database_path);
    if (secsts != SECSuccess && !pathSpecified) {
	/* fallback, older versions of NSS do not support sql: */
	serverdb(secure_server.database_path, MAXPATHLEN, "");
	secsts = NSS_Init(secure_server.database_path);
    }

    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Cannot setup certificate DB (%s): %s",
			secure_server.database_path,
			pmErrStr(__pmSecureSocketsError(PR_GetError())));
	sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
	secure_server.init_failed = 1;
	goto done;
    }

    /* Some NSS versions don't do this correctly in NSS_SetDomesticPolicy. */
    for (cipher = SSL_GetImplementedCiphers(); *cipher != 0; ++cipher)
	SSL_CipherPolicySet(*cipher, SSL_ALLOWED);

    /* Configure SSL session cache for multi-process server, using defaults */
    secsts = SSL_ConfigMPServerSIDCache(1, 0, 0, NULL);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to configure SSL session ID cache: %s",
		pmErrStr(__pmSecureSocketsError(PR_GetError())));
	sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
	secure_server.init_failed = 1;
	goto done;
    } else {
	secure_server.ssl_session_cache_setup = 1;
    }

    /*
     * Iterate over any/all PCP Collector nickname certificates,
     * seeking one valid certificate.  No-such-nickname is not an
     * error (not configured by admin at all) but anything else is.
     */
    CERTCertList *certlist;
    CERTCertDBHandle *nssdb = CERT_GetDefaultCertDB();
    CERTCertificate *dbcert = PK11_FindCertFromNickname(nickname, NULL);

    if (dbcert) {
	PRTime now = PR_Now();
	SECItem *name = &dbcert->derSubject;
	CERTCertListNode *node;

	certlist = CERT_CreateSubjectCertList(NULL, nssdb, name, now, PR_FALSE);
	if (certlist) {
	    for (node = CERT_LIST_HEAD(certlist);
		 !CERT_LIST_END(node, certlist);
		 node = CERT_LIST_NEXT (node)) {
		if (pmDebug & DBG_TRACE_CONTEXT)
		    __pmDumpCertificate(stderr, nickname, node->cert);
		if (!__pmValidCertificate(nssdb, node->cert, now))
		    continue;
		secure_server.certificate_verified = 1;
		break;
	    }
	    CERT_DestroyCertList(certlist);
	}

	if (secure_server.certificate_verified) {
	    secure_server.certificate_KEA = NSS_FindCertKEAType(dbcert);
	    secure_server.private_key = PK11_FindKeyByAnyCert(dbcert, NULL);
	    if (!secure_server.private_key) {
		__pmNotifyErr(LOG_ERR, "Unable to extract %s private key",
				nickname);
		CERT_DestroyCertificate(dbcert);
		secure_server.certificate_verified = 0;
		sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
		secure_server.init_failed = 1;
		goto done;
	    }
	} else {
	    __pmNotifyErr(LOG_ERR, "Unable to find a valid %s", nickname);
	    CERT_DestroyCertificate(dbcert);
	    sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
	    secure_server.init_failed = 1;
	    goto done;
	}
    }

    if (! secure_server.certificate_verified) {
	if (pmDebug & DBG_TRACE_CONTEXT) {
	    __pmNotifyErr(LOG_INFO, "No valid %s in security database: %s",
			  nickname, secure_server.database_path);
	}
	sts = -EOPNOTSUPP;	/* not fatal - just no secure connections */
	secure_server.init_failed = 1;
	goto done;
    }

    secure_server.certificate = dbcert;
    secure_server.init_failed = 0;
    sts = 0;

done:
    PM_UNLOCK(__pmLock_libpcp);
    return sts;
}

void
__pmSecureServerShutdown(void)
{
    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    if (secure_server.certificate) {
	CERT_DestroyCertificate(secure_server.certificate);
	secure_server.certificate = NULL;
    }
    if (secure_server.private_key) {
	SECKEY_DestroyPrivateKey(secure_server.private_key);
	secure_server.private_key = NULL;
    }
    if (secure_server.ssl_session_cache_setup) {
	SSL_ShutdownServerSessionIDCache();
	secure_server.ssl_session_cache_setup = 0;
    }    
    if (secure_server.initialized) {
	NSS_Shutdown();
	secure_server.initialized = 0;
    }
    PM_UNLOCK(__pmLock_libpcp);
}

static int
__pmSecureServerNegotiation(int fd, int *strength)
{
    PRIntervalTime timer;
    PRFileDesc *sslsocket;
    SECStatus secsts;
    int enabled, keysize;
    int msec;

    sslsocket = (PRFileDesc *)__pmGetSecureSocket(fd);
    if (!sslsocket)
	return PM_ERR_IPC;

    PM_INIT_LOCKS();
    PM_LOCK(__pmLock_libpcp);
    secsts = SSL_ConfigSecureServer(sslsocket,
			secure_server.certificate,
			secure_server.private_key,
			secure_server.certificate_KEA);
    PM_UNLOCK(__pmLock_libpcp);

    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to configure secure server: %s",
			    pmErrStr(__pmSecureSocketsError(PR_GetError())));
	return PM_ERR_IPC;
    }

    secsts = SSL_ResetHandshake(sslsocket, PR_TRUE /*server*/);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to reset secure handshake: %s",
			    pmErrStr(__pmSecureSocketsError(PR_GetError())));
	return PM_ERR_IPC;
    }

    /* Server initiates handshake now to get early visibility of errors */
    msec = __pmConvertTimeout(TIMEOUT_DEFAULT);
    timer = PR_MillisecondsToInterval(msec);
    secsts = SSL_ForceHandshakeWithTimeout(sslsocket, timer);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to force secure handshake: %s",
			    pmErrStr(__pmSecureSocketsError(PR_GetError())));
	return PM_ERR_IPC;
    }

    secsts = SSL_SecurityStatus(sslsocket, &enabled, NULL, &keysize, NULL, NULL, NULL);
    if (secsts != SECSuccess)
	return __pmSecureSocketsError(PR_GetError());

    *strength = (enabled > 0) ? keysize : DEFAULT_SECURITY_STRENGTH;
    return 0;
}

static int
__pmSetUserGroupAttributes(const char *username, __pmHashCtl *attrs)
{
    char name[32];
    char *namep;
    uid_t uid;
    gid_t gid;

    if (__pmGetUserIdentity(username, &uid, &gid, PM_RECOV_ERR) == 0) {
	snprintf(name, sizeof(name), "%u", uid);
	name[sizeof(name)-1] = '\0';
	if ((namep = strdup(name)) != NULL)
	    __pmHashAdd(PCP_ATTR_USERID, namep, attrs);
	else
	    return -ENOMEM;

	snprintf(name, sizeof(name), "%u", gid);
	name[sizeof(name)-1] = '\0';
	if ((namep = strdup(name)) != NULL)
	    __pmHashAdd(PCP_ATTR_GROUPID, namep, attrs);
	else
	    return -ENOMEM;
	return 0;
    }
    __pmNotifyErr(LOG_ERR, "Authenticated user %s not found\n", username);
    return -ESRCH;
}

static int
__pmAuthServerSetAttributes(sasl_conn_t *conn, __pmHashCtl *attrs)
{
    const void *property = NULL;
    char *username;
    int sts;

    sts = sasl_getprop(conn, SASL_USERNAME, &property);
    username = (char *)property;
    if (sts == SASL_OK && username) {
	__pmNotifyErr(LOG_INFO,
			"Successful authentication for user \"%s\"\n",
			username);
	if ((username = strdup(username)) == NULL) {
	    __pmNoMem("__pmAuthServerSetAttributes",
			strlen(username), PM_RECOV_ERR);
	    return -ENOMEM;
	}
    } else {
	__pmNotifyErr(LOG_ERR,
			"Authentication complete, but no username\n");
	return -ESRCH;
    }

    if ((sts = __pmHashAdd(PCP_ATTR_USERNAME, username, attrs)) < 0)
	return sts;
    return __pmSetUserGroupAttributes(username, attrs);
}

static int
__pmAuthServerSetProperties(sasl_conn_t *conn, int ssf)
{
    int saslsts;
    sasl_security_properties_t props;

    /* set external security strength factor */
    saslsts = sasl_setprop(conn, SASL_SSF_EXTERNAL, &ssf);
    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
	__pmNotifyErr(LOG_ERR, "SASL setting external SSF to %d: %s",
			ssf, sasl_errstring(saslsts, NULL, NULL));
	return __pmSecureSocketsError(saslsts);
    }

    /* set general security properties */
    memset(&props, 0, sizeof(props));
    props.maxbufsize = LIMIT_AUTH_PDU;
    props.max_ssf = UINT_MAX;
    saslsts = sasl_setprop(conn, SASL_SEC_PROPS, &props);
    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
	__pmNotifyErr(LOG_ERR, "SASL setting security properties: %s",
			sasl_errstring(saslsts, NULL, NULL));
	return __pmSecureSocketsError(saslsts);
    }

    return 0;
}

static int
__pmAuthServerNegotiation(int fd, int ssf, __pmHashCtl *attrs)
{
    int sts, saslsts;
    int pinned, length, count;
    char *payload, *offset;
    sasl_conn_t *sasl_conn;
    __pmPDU *pb;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "__pmAuthServerNegotiation(fd=%d, ssf=%d)\n",
		fd, ssf);

    if ((sasl_conn = (sasl_conn_t *)__pmGetUserAuthData(fd)) == NULL)
        return -EINVAL;

    /* setup all the security properties for this connection */
    if ((sts = __pmAuthServerSetProperties(sasl_conn, ssf)) < 0)
	return sts;

    saslsts = sasl_listmech(sasl_conn,
			    NULL, NULL, " ", NULL,
                            (const char **)&payload,
                            (unsigned int *)&length,
                            &count);
    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
	__pmNotifyErr(LOG_ERR, "Generating client mechanism list: %s",
			sasl_errstring(saslsts, NULL, NULL));
	return __pmSecureSocketsError(saslsts);
    }
    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "__pmAuthServerNegotiation - sending mechanism list "
		"(%d items, %d bytes): \"%s\"\n", count, length, payload);

    if ((sts = __pmSendAuth(fd, FROM_ANON, 0, payload, length)) < 0)
	return sts;

    if (pmDebug & DBG_TRACE_AUTH)
	fprintf(stderr, "__pmAuthServerNegotiation - wait for mechanism\n");

    sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_AUTH) {
        sts = __pmDecodeAuth(pb, &count, &payload, &length);
        if (sts >= 0) {
	    for (count = 0; count < length; count++) {
		if (payload[count] == '\0')
		    break;
	    }
	    if (count < length)	{  /* found an initial response */
		length = length - count - 1;
		offset = payload + count + 1;
	    } else {
		length = 0;
		offset = NULL;
	    }

	    saslsts = sasl_server_start(sasl_conn, payload,
				offset, length,
				(const char **)&payload,
				(unsigned int *)&length);
	    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
		sts = __pmSecureSocketsError(saslsts);
		if (pmDebug & DBG_TRACE_AUTH)
		    fprintf(stderr, "sasl_server_start failed: %d (%s)\n",
				    saslsts, pmErrStr(sts));
	    } else {
		if (pmDebug & DBG_TRACE_AUTH)
		    fprintf(stderr, "sasl_server_start success: sts=%s\n",
			    saslsts == SASL_CONTINUE ? "continue" : "ok");
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
	fprintf(stderr, "__pmAuthServerNegotiation method negotiated\n");

    while (saslsts == SASL_CONTINUE) {
	if (!payload) {
	    __pmNotifyErr(LOG_ERR, "No SASL data to send");
	    sts = -EINVAL;
	    break;
	}
	if ((sts = __pmSendAuth(fd, FROM_ANON, 0, payload, length)) < 0)
	    break;

	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "__pmAuthServerNegotiation awaiting response\n");

	sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
	if (sts == PDU_AUTH) {
	    sts = __pmDecodeAuth(pb, &count, &payload, &length);
	    if (sts >= 0) {
		sts = saslsts = sasl_server_step(sasl_conn, payload, length,
                                                 (const char **)&payload,
                                                 (unsigned int *)&length);
		if (sts != SASL_OK && sts != SASL_CONTINUE) {
		    sts = __pmSecureSocketsError(sts);
		    break;
		}
		if (pmDebug & DBG_TRACE_AUTH) {
		    fprintf(stderr, "__pmAuthServerNegotiation"
				    " step recv (%d bytes)\n", length);
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
	    break;
    }

    if (sts < 0) {
	if (pmDebug & DBG_TRACE_AUTH)
	    fprintf(stderr, "__pmAuthServerNegotiation loop failed: %d\n", sts);
	return sts;
    }

    return __pmAuthServerSetAttributes(sasl_conn, attrs);
}

int
__pmSecureServerHandshake(int fd, int flags, __pmHashCtl *attrs)
{
    int sts, ssf = DEFAULT_SECURITY_STRENGTH;

    /* protect from unsupported requests from future/oddball clients */
    if (flags & ~(PDU_FLAG_SECURE | PDU_FLAG_SECURE_ACK | PDU_FLAG_COMPRESS |
		  PDU_FLAG_AUTH | PDU_FLAG_CREDS_REQD | PDU_FLAG_CONTAINER))
	return PM_ERR_IPC;

    if (flags & PDU_FLAG_CREDS_REQD) {
	if (__pmHashSearch(PCP_ATTR_USERID, attrs) != NULL)
	    return 0;	/* unix domain socket */
	else
	    flags |= PDU_FLAG_AUTH;	/* force authentication */
    }

    if ((sts = __pmSecureServerIPCFlags(fd, flags)) < 0)
	return sts;
    if (((flags & PDU_FLAG_SECURE) != 0) &&
	((sts = __pmSecureServerNegotiation(fd, &ssf)) < 0))
	return sts;
    if (((flags & PDU_FLAG_AUTH) != 0) &&
	((sts = __pmAuthServerNegotiation(fd, ssf, attrs)) < 0))
	return sts;
    return 0;
}
