/*
 * Copyright (c) 2012 Red Hat.
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

#include "pmapi.h"
#include "impl.h"
#include <nss.h>
#include <ssl.h>
#include <nspr.h>
#include <keyhi.h>
#include <secder.h>
#include <pk11pub.h>
#include <sys/stat.h>

#define SERVER_CERTIFICATE_NICK	"PCP Collector certificate"
#define MAX_DATABASE_PASSWORD	256

static CERTCertificate		*certificate;
static SECKEYPrivateKey		*private_key;
static const char		*password_file;
static SSLKEAType		certificate_KEA;
static int			certificate_verified;
static int			ssl_session_cache_setup;
static char			database_path[MAXPATHLEN];

int
pmcd_encryption_enabled(void)
{
    return certificate_verified;
}

int
pmcd_compression_enabled(void)
{
    return 1;	/* Will need to check this, does it require cert/key? */
}

static int
secure_server_error(void)
{
    return PM_ERR_NYI + PR_GetError();	/* returned value is negative */
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
    size_t length = MAX_DATABASE_PASSWORD;
    char *password = NULL;
    int sts;

    (void)arg;
    (void)info;

    if (!password_file) {
	__pmNotifyErr(LOG_ERR, "Password sought but no password file given");
	return NULL;
    }
    if (retry) {
	__pmNotifyErr(LOG_ERR, "Retry attempted during password extraction");
	return NULL;	/* no soup^Wretries for you */
    }

    if ((sts = secure_file_contents(password_file, &password, &length)) < 0) {
	__pmNotifyErr(LOG_ERR, "Cannot read password file \"%s\": %s",
		password_file, pmErrStr(sts));
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
	return secure_server_error();

    /* Convert to local time */
    PR_ExplodeTime(itime, PR_GMTParameters, &exploded);
    if (!PR_FormatTime(buffer, size, "%a %b %d %H:%M:%S %Y", &exploded))
	return secure_server_error();
    return 0;
}

static void
__pmDumpCertificate(const char *nickname, CERTCertificate *certificate)
{
    CERTValidity *valid = &certificate->validity;
    char tbuf[256];

    fprintf(stderr, "Certificate: %s", nickname);
    if (__pmCertificateTimestamp(&valid->notBefore, tbuf, sizeof(tbuf)) == 0)
	fprintf(stderr, "  Not Valid Before: %s UTC", tbuf);
    if (__pmCertificateTimestamp(&valid->notAfter, tbuf, sizeof(tbuf)) == 0)
	fprintf(stderr, "  Not Valid After: %s UTC", tbuf);
}

static int
__pmValidCertificate(CERTCertDBHandle *handle, CERTCertificate *cert, PRTime stamp)
{
    SECCertificateUsage usage = certificateUsageSSLServer; /*|certificateUsageObjectSigner*/
    SECStatus secsts = CERT_VerifyCertificate(handle, cert, PR_TRUE, usage,
						stamp, NULL, NULL, &usage);
    return (secsts == SECSuccess);
}

int
pmcd_secure_server_setup(const char *dbpath, const char *passwd)
{
    SECStatus secsts;

    PR_Init(PR_SYSTEM_THREAD, PR_PRIORITY_NORMAL, 1);

    /* Configure optional (cmdline) password file in case DB locked */
    password_file = passwd;
    PK11_SetPasswordFunc(certificate_database_password);

    /* Configure location of the database files with a sane default */
    if (!dbpath) {
	int sep = __pmPathSeparator();
	snprintf(database_path, sizeof(database_path),
		 "%s%c" "config" "%c" "ssl" "%c" "collector",
		 pmGetConfig("PCP_VAR_DIR"), sep, sep, sep);
    } else {
	/* -2 here ensures result is NULL terminated */
	strncat(database_path, dbpath, MAXPATHLEN-2);
    }

    if (access(database_path, R_OK) < 0 && oserror() == ENOENT) {
	/* Handle the common case - pmcd supports secure sockets, */
	/* but no configuration has been performed on the server. */
	return 0;
    }

    secsts = NSS_Init(database_path);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_INFO,
		"Disabling encryption - cannot setup certificate DB (%s): %s",
		database_path, pmErrStr(secure_server_error()));
	return 0;
    }

    secsts = NSS_SetExportPolicy();
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to set NSS export policy: %s",
		pmErrStr(secure_server_error()));
	return -EINVAL;
    }

    /* Configure the SSL session cache for single process server, using defaults */
    secsts = SSL_ConfigServerSessionIDCache(0, 0, 0, NULL);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to configure SSL session ID cache: %s",
		pmErrStr(secure_server_error()));
	return -EINVAL;
    } else {
	ssl_session_cache_setup = 1;
    }

    /* Iterate over all certs for server nickname, ensuring one cert is valid. */
    CERTCertificate *dbcert = PK11_FindCertFromNickname(SERVER_CERTIFICATE_NICK, NULL);
    CERTCertDBHandle *dbhandle = CERT_GetDefaultCertDB();
    CERTCertList *certlist;

    if (dbcert) {
	PRTime now = PR_Now();
	SECItem *name = &dbcert->derSubject;
	CERTCertListNode *node;

	certlist = CERT_CreateSubjectCertList(NULL, dbhandle, name, now, PR_FALSE);
	if (certlist) {
	    for (node = CERT_LIST_HEAD(certlist);
		 !CERT_LIST_END(node, certlist);
		 node = CERT_LIST_NEXT (node)) {
		if (pmDebug & DBG_TRACE_CONTEXT)
		    __pmDumpCertificate(SERVER_CERTIFICATE_NICK, node->cert);
		if (!__pmValidCertificate(dbhandle, node->cert, now))
		    continue;
		certificate_verified = 1;
		break;
	    }
	    CERT_DestroyCertList(certlist);
	}

	if (certificate_verified) {
	    certificate_KEA = NSS_FindCertKEAType(dbcert);
	    private_key = PK11_FindKeyByAnyCert(dbcert, NULL);
	    if (!private_key) {
		__pmNotifyErr(LOG_ERR, "Unable to extract %s private key",
				SERVER_CERTIFICATE_NICK);
		CERT_DestroyCertificate(dbcert);
		certificate_verified = 0;
		return -EINVAL;
	    }
	} else {
	    __pmNotifyErr(LOG_ERR, "Unable to find a valid %s",
			    SERVER_CERTIFICATE_NICK);
	    CERT_DestroyCertificate(dbcert);
	    return -EINVAL;
	}
    }

    if (certificate_verified) {
	certificate = dbcert;
    } else {
	__pmNotifyErr(LOG_INFO,
		"Disabling encryption - no valid \"%s\" in DB: %s",
		SERVER_CERTIFICATE_NICK, database_path);
    }

    return 0;
}

void
pmcd_secure_server_shutdown(void)
{
    if (certificate) {
	CERT_DestroyCertificate(certificate);
	certificate = NULL;
    }
    if (private_key) {
	SECKEY_DestroyPrivateKey(private_key);
	private_key = NULL;
    }
    if (ssl_session_cache_setup) {
	SSL_ShutdownServerSessionIDCache();
	ssl_session_cache_setup = 0;
    }    
    NSS_Shutdown();
}

int
pmcd_secure_handshake(int fd, int flags)
{
    PRIntervalTime timer;
    PRFileDesc	*sslsocket;
    SECStatus	secsts;
    int		msec;
    int		sts;

    /* Protect ourselves from unsupported requests from oddball clients */
    if ((flags & ~(PDU_FLAG_SECURE|PDU_FLAG_COMPRESS)) != 0)
	return PM_ERR_IPC;

    if ((sts = __pmSecureServerIPCFlags(fd, flags)) < 0)
	return sts;

    sslsocket = (PRFileDesc *)__pmGetSecureSocket(fd);
    if (!sslsocket)
	return PM_ERR_IPC;

    secsts = SSL_ConfigSecureServer(sslsocket, certificate, private_key, certificate_KEA);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to configure secure server: %s",
			    pmErrStr(secure_server_error()));
	return PM_ERR_IPC;
    }

    secsts = SSL_ResetHandshake(sslsocket, PR_TRUE /*server*/);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to reset secure handshake: %s",
			    pmErrStr(secure_server_error()));
	return PM_ERR_IPC;
    }

    /* Server initiates handshake now to get early visibility of errors */
    msec = __pmConvertTimeout(TIMEOUT_DEFAULT);
    timer = PR_MillisecondsToInterval(msec);
    secsts = SSL_ForceHandshakeWithTimeout(sslsocket, timer);
    if (secsts != SECSuccess) {
	__pmNotifyErr(LOG_ERR, "Unable to force secure handshake: %s",
			    pmErrStr(secure_server_error()));
	return PM_ERR_IPC;
    }

    return 0;
}
