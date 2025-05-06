/*
 * Copyright (c) 2012-2015,2022 Red Hat.
 *
 * Server side security features - OpenSSL encryption and SASL
 * (Simple Authentication and Security Layer) authentication.
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
#define SOCKET_INTERNAL
#include "internal.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>

static struct {
    /* OpenSSL certificate management */
    __pmSecureConfig	config;
    SSL_CTX		*context;
    unsigned int	features;
    unsigned int	setup;
} secure_server;

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	secureserver_lock;
#else
void			*secureserver_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == secureserver_lock
 */
int
__pmIsSecureserverLock(void *lock)
{
    return lock == (void *)&secureserver_lock;
}
#endif

void
init_secureserver_lock(void)
{
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&secureserver_lock);
#endif
}

int
__pmSecureServerSetFeature(__pmServerFeature wanted)
{
    if (wanted == PM_SERVER_FEATURE_CERT_REQD){
        secure_server.features |= (1 << wanted);
        return 1;
    }
    return 0;
}

int
__pmSecureServerClearFeature(__pmServerFeature clear)
{
    if (clear == PM_SERVER_FEATURE_CERT_REQD){
    	secure_server.features &= ~(1<<clear);
	return 1;
    }
    return 0;
}

int
__pmSecureServerHasFeature(__pmServerFeature query)
{
    int sts = 0;

    switch (query) {
    case PM_SERVER_FEATURE_SECURE:
	return secure_server.context != NULL;
    case PM_SERVER_FEATURE_COMPRESS:
	break; /* deprecated */
    case PM_SERVER_FEATURE_AUTH:
	sts = 1;
	break;
    case PM_SERVER_FEATURE_CERT_REQD:
	sts = ((secure_server.features & (1 << PM_SERVER_FEATURE_CERT_REQD)) != 0);
	break;
    default:
	break;
    }
    return sts;
}

int
__pmSecureServerCertificateSetup(const char *db, const char *p, const char *c)
{
    /* deprecated function with the transition to OpenSSL */
    (void)db; (void)p; (void)c;
    return -ENOTSUP;
}

int
__pmSecureServerSetup(void)
{
    PM_INIT_LOCKS();
    PM_LOCK(secureserver_lock);
    if (secure_server.setup == 1) {
	PM_UNLOCK(secureserver_lock);
	return 1;
    }

    SSL_library_init();
    SSL_load_error_strings();

    secure_server.setup = 1;
    PM_UNLOCK(secureserver_lock);
    return 0;
}

void *
__pmSecureServerInit(__pmSecureConfig *tls)
{
    /* all secure socket connections must use at least TLSv1.2 */
    int		flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
			SSL_OP_NO_TLSv1 |SSL_OP_NO_TLSv1_1;
    int		verify = SSL_VERIFY_PEER;
    SSL_CTX	*context;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmSecureServerInit");

    PM_INIT_LOCKS();
    PM_LOCK(secureserver_lock);

    __pmSecureConfigInit();

    /* check whether we can proceed or not - misconfiguration is fatal */
    if (!tls->certfile || !tls->keyfile) {
#ifdef OPENSSL_VERSION_STR
	pmNotifyErr(LOG_INFO, "OpenSSL %s - no %s found", OPENSSL_VERSION_STR,
			tls->certfile ? "private key" : "certificate file");
#else /* back-compat and not ideal, includes date */
	pmNotifyErr(LOG_INFO, "%s - no %s found", OPENSSL_VERSION_TEXT,
			tls->certfile ? "private key" : "certificate file");
#endif
	exit(1);
    }

    if ((context = SSL_CTX_new(TLS_server_method())) == NULL) {
	pmNotifyErr(LOG_ERR, "Cannot create initial secure server context");
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	exit(1);
    }

    /* all secure client connections must use at least TLSv1.2 */
    SSL_CTX_set_options(context, flags);
#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
    SSL_CTX_set_options(context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif
#ifdef SSL_OP_NO_COMPRESSION
    SSL_CTX_set_options(context, SSL_OP_NO_COMPRESSION);
#endif

    SSL_CTX_set_mode(context, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

    /* verification mode of client certificate, default is SSL_VERIFY_PEER */
    if (tls->clientverify && strcmp(tls->clientverify, "true") == 0)
	verify |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
    SSL_CTX_set_verify(context, verify, NULL);

    /* Set the key and cert */
    if (SSL_CTX_use_certificate_chain_file(context, tls->certfile) <= 0) {
	pmNotifyErr(LOG_ERR, "Cannot load certificate chain from %s",
		    tls->certfile);
	if (pmDebugOptions.tls)
            ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (!SSL_CTX_use_PrivateKey_file(context, tls->keyfile, SSL_FILETYPE_PEM)) {
	pmNotifyErr(LOG_ERR, "Cannot load private key from %s", tls->keyfile);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	exit(1);
    }

    if (!SSL_CTX_check_private_key(context)) {
	pmNotifyErr(LOG_ERR, "Cannot validate the private key");
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	exit(1);
    }

    if (tls->cacertfile || tls->cacertdir) {
	if (tls->cacertfile)
	    SSL_CTX_set_client_CA_list(context,
				SSL_load_client_CA_file(tls->cacertfile));
	if (!SSL_CTX_load_verify_locations(context,
				tls->cacertfile, tls->cacertdir)) {
	    pmNotifyErr(LOG_ERR, "Cannot load the client CA list from %s",
			tls->cacertfile ? tls->cacertfile : tls->cacertdir);
	    if (pmDebugOptions.tls)
		ERR_print_errors_fp(stderr);
	    exit(1);
	}
    }

    /* optional list of ciphers (TLSv1.2 and earlier) */
    if (tls->ciphers &&
	!SSL_CTX_set_cipher_list(context, tls->ciphers)) {
	pmNotifyErr(LOG_ERR, "Cannot set the cipher list from %s",
			tls->ciphers);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	exit(1);
    }

    /* optional suites of ciphers (TLSv1.3 and later) */
    if (tls->ciphersuites &&
	!SSL_CTX_set_ciphersuites(context, tls->ciphersuites)) {
	pmNotifyErr(LOG_ERR, "Cannot set the cipher suites from %s",
			tls->ciphersuites);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	exit(1);
    }

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: complete\n", "__pmSecureServerInit");

    secure_server.context = context;
    PM_UNLOCK(secureserver_lock);
    return context;
}

void
__pmSecureServerShutdown(void *context, __pmSecureConfig *config)
{
    PM_INIT_LOCKS();
    PM_LOCK(secureserver_lock);
    __pmFreeSecureConfig(&secure_server.config);
    if (secure_server.setup) {
	if (context && context != secure_server.context)
	    SSL_CTX_free(secure_server.context);
	if (config && config != &secure_server.config)
	    __pmFreeSecureConfig(&secure_server.config);
	secure_server.setup = 0;
    }
    if (config)
	__pmFreeSecureConfig(config);
    if (context)
	SSL_CTX_free(context);
    PM_UNLOCK(secureserver_lock);
}

static int
__pmSetUserGroupAttributes(const char *username, __pmHashCtl *attrs)
{
    char name[32];
    char *namep;
    uid_t uid;
    gid_t gid;

    if (__pmGetUserIdentity(username, &uid, &gid, PM_RECOV_ERR) == 0) {
	pmsprintf(name, sizeof(name), "%u", uid);
	name[sizeof(name)-1] = '\0';
	if ((namep = strdup(name)) != NULL)
	    __pmHashAdd(PCP_ATTR_USERID, namep, attrs);
	else
	    return -ENOMEM;

	pmsprintf(name, sizeof(name), "%u", gid);
	name[sizeof(name)-1] = '\0';
	if ((namep = strdup(name)) != NULL)
	    __pmHashAdd(PCP_ATTR_GROUPID, namep, attrs);
	else
	    return -ENOMEM;
	return 0;
    }

    if (pmDebugOptions.auth)
        pmNotifyErr(LOG_INFO, "Authenticated user %s not local\n", username);
    return 0;
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
	int len = strlen(username);

	pmNotifyErr(LOG_INFO,
			"Successful authentication for user \"%s\"\n",
			username);
	if ((username = strdup(username)) == NULL) {
	    pmNoMem("__pmAuthServerSetAttributes",
			len, PM_RECOV_ERR);
	    return -ENOMEM;
	}
    } else {
	pmNotifyErr(LOG_ERR,
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
	pmNotifyErr(LOG_ERR, "SASL setting external SSF to %d: %s",
			ssf, sasl_errstring(saslsts, NULL, NULL));
	return __pmSecureSocketsError(saslsts);
    }

    /* set general security properties */
    memset(&props, 0, sizeof(props));
    props.maxbufsize = LIMIT_AUTH_PDU;
    props.max_ssf = UINT_MAX;
    saslsts = sasl_setprop(conn, SASL_SEC_PROPS, &props);
    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
	pmNotifyErr(LOG_ERR, "SASL setting security properties: %s",
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

    if (pmDebugOptions.auth)
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
	pmNotifyErr(LOG_ERR, "Generating client mechanism list: %s",
			sasl_errstring(saslsts, NULL, NULL));
	return __pmSecureSocketsError(saslsts);
    }
    if (pmDebugOptions.auth)
	fprintf(stderr, "__pmAuthServerNegotiation - sending mechanism list "
		"(%d items, %d bytes): \"%s\"\n", count, length, payload);

    if ((sts = __pmSendAuth(fd, FROM_ANON, 0, payload, length)) < 0)
	return sts;

    if (pmDebugOptions.auth)
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
		if (pmDebugOptions.auth)
		    fprintf(stderr, "sasl_server_start failed: %d (%s)\n",
				    saslsts, pmErrStr(sts));
	    } else {
		if (pmDebugOptions.auth)
		    fprintf(stderr, "sasl_server_start success: sts=%s\n",
			    saslsts == SASL_CONTINUE ? "continue" : "ok");
	    }
	}
    }
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT) {
	if (pmDebugOptions.pdu) {
	    char	strbuf[20];
	    char	errmsg[PM_MAXERRMSGLEN];
	    if (sts < 0)
		fprintf(stderr, "__pmSecureClientHandshake: PM_ERR_IPC: expecting PDU_AUTH but __pmGetPDU returns %d (%s)\n",
		    sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
	    else
		fprintf(stderr, "__pmSecureClientHandshake: PM_ERR_IPC: expecting PDU_AUTH but __pmGetPDU returns %d (type=%s)\n",
		    sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	}
	sts = PM_ERR_IPC;
    }

    if (pinned > 0)
	__pmUnpinPDUBuf(pb);
    if (sts < 0)
	return sts;

    if (pmDebugOptions.auth)
	fprintf(stderr, "__pmAuthServerNegotiation method negotiated\n");

    while (saslsts == SASL_CONTINUE) {
	if (!payload) {
	    pmNotifyErr(LOG_ERR, "No SASL data to send");
	    sts = -EINVAL;
	    break;
	}
	if ((sts = __pmSendAuth(fd, FROM_ANON, 0, payload, length)) < 0)
	    break;

	if (pmDebugOptions.auth)
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
		if (pmDebugOptions.auth) {
		    fprintf(stderr, "__pmAuthServerNegotiation"
				    " step recv (%d bytes)\n", length);
		}
	    }
	}
	else if (sts == PDU_ERROR)
	    __pmDecodeError(pb, &sts);
	else if (sts != PM_ERR_TIMEOUT) {
	    if (pmDebugOptions.pdu) {
		char	strbuf[20];
		char	errmsg[PM_MAXERRMSGLEN];
		if (sts < 0)
		    fprintf(stderr, "__pmAuthServerNegotiation: PM_ERR_IPC: expecting PDU_AUTH but __pmGetPDU returns %d (%s)\n",
			sts, pmErrStr_r(sts, errmsg, sizeof(errmsg)));
		else
		    fprintf(stderr, "__pmAuthServerNegotiation: PM_ERR_IPC: expecting PDU_AUTH but __pmGetPDU returns %d (type=%s)\n",
			sts, __pmPDUTypeStr_r(sts, strbuf, sizeof(strbuf)));
	    }
	    sts = PM_ERR_IPC;
	}

	if (pinned > 0)
	    __pmUnpinPDUBuf(pb);
	if (sts < 0)
	    break;
    }

    if (sts < 0) {
	if (pmDebugOptions.auth)
	    fprintf(stderr, "__pmAuthServerNegotiation loop failed: %d\n", sts);
	return sts;
    }

    return __pmAuthServerSetAttributes(sasl_conn, attrs);
}

int
__pmSecureServerHandshake(int fd, int flags, __pmHashCtl *attrs)
{
    int sts, ssf = DEFAULT_SECURITY_STRENGTH;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmSecureServerHandshake");

    /* protect from unsupported requests from future/oddball clients */
    if (flags & ~(PDU_FLAG_SECURE | PDU_FLAG_SECURE_ACK | PDU_FLAG_COMPRESS |
		  PDU_FLAG_AUTH | PDU_FLAG_CREDS_REQD | PDU_FLAG_CONTAINER |
		  PDU_FLAG_CERT_REQD)) {
	if (pmDebugOptions.pdu) {
	    fprintf(stderr, "__pmAuthServerNegotiation: PM_ERR_IPC: bad flags %x\n", flags);
	}
	return PM_ERR_IPC;
    }

    if (flags & PDU_FLAG_CREDS_REQD) {
	if (__pmHashSearch(PCP_ATTR_USERID, attrs) != NULL)
	    return 0;	/* unix domain socket */
	else
	    flags |= PDU_FLAG_AUTH;	/* force authentication */
    }

    if ((sts = __pmSecureServerIPCFlags(fd, flags, secure_server.context)) < 0)
	return sts;
    if (((flags & PDU_FLAG_SECURE) != 0) &&
	((sts = __pmSecureServerNegotiation(fd, &ssf)) < 0))
	return sts;
    if (((flags & PDU_FLAG_AUTH) != 0) &&
	((sts = __pmAuthServerNegotiation(fd, ssf, attrs)) < 0))
	return sts;
    return 0;
}
