/*
 * Copyright (c) 2012-2015,2022 Red Hat.
 * Security and Authentication (OpenSSL and SASL) support.  Client side.
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
#include <ctype.h>
#include <assert.h>
#include <openssl/opensslv.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#else
#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif
#endif

#ifdef PM_MULTI_THREAD
static pthread_mutex_t	secureclient_lock;
#else
void			*secureclient_lock;
#endif

#if defined(PM_MULTI_THREAD) && defined(PM_MULTI_THREAD_DEBUG)
/*
 * return true if lock == secureclient_lock
 */
int
__pmIsSecureclientLock(void *lock)
{
    return lock == (void *)&secureclient_lock;
}
#endif

void
init_secureclient_lock(void)
{
#ifdef PM_MULTI_THREAD
    __pmInitMutex(&secureclient_lock);
#endif
}

/*
 * For every connection when operating under secure socket mode, we need
 * the following auxillary structure associated with the socket.  It has
 * critical information that each piece of the security pie can make use
 * of (OpenSSL + SASL).  This is allocated once a connection is upgraded
 * from insecure to secure.
 */
typedef struct { 
    SSL			*ssl;
    sasl_conn_t		*saslConn;
    sasl_callback_t	*saslCB;
} __pmSecureSocket;

int
__pmDataIPCSize(void)
{
    return sizeof(__pmSecureSocket);
}

/*
 * We shift SASL errors below the valid range for all other PCP
 * error codes, in order to avoid conflicts.  pmErrStr can then
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
	 */
	case ECONNRESET:
	case EPIPE:
	case ETIMEDOUT:
	case ENETDOWN:
	case ENETUNREACH:
	case EHOSTDOWN:
	case EHOSTUNREACH:
	case ECONNREFUSED:
	    return 1;
    }
    return 0;
}

void
__pmCloseSocket(int fd)
{
    __pmSecureSocket	ss;
    int			sts;

    sts = __pmDataIPC(fd, (void *)&ss);

    __pmResetIPC(fd);

    if (sts == 0) {
	if (ss.saslConn) {
	    sasl_dispose(&ss.saslConn);
	    ss.saslConn = NULL;
	}
	if (ss.saslCB) {
	    free(ss.saslCB);
	    ss.saslCB = NULL;
	}
	if (ss.ssl) {
	    SSL_shutdown(ss.ssl);
	    SSL_free(ss.ssl);
	    ss.ssl = NULL;
	}
    }

    if (fd >= 0) {
#if defined(IS_MINGW)
	closesocket(fd);
#else
	close(fd);
#endif
    }
}

static int
__pmAuthLogCB(void *context, int priority, const char *message)
{
    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s enter ctx=%p pri=%d\n",
			__FILE__, "__pmAuthLogCB", context, priority);

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
	if (pmDebugOptions.auth)
	    priority = LOG_DEBUG;
	else
	    return SASL_OK;
	break;
    default:
	priority = LOG_INFO;
	break;
    }
    pmNotifyErr(priority, "%s", message);
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
	pmNotifyErr(LOG_ERR, "opening input terminal for read\n");
	return NULL;
    }
    output = fopen(console, "w");
    if (output == NULL) {
	pmNotifyErr(LOG_ERR, "opening output terminal for write\n");
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

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s enter ctx=%p id=%#x\n",
			__FILE__, "__pmAuthRealmCB", context, id);

    if (id != SASL_CB_GETREALM)
	return SASL_FAIL;

    value = __pmGetAttrValue(PCP_ATTR_REALM, attrs, "Realm: ");
    *result = value;

    if (pmDebugOptions.auth) {
	fprintf(stderr, "%s:%s ctx=%p, id=%#x, realms=(",
			__FILE__, "__pmAuthRealmCB" ,context, id);
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

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s enter ctx=%p id=%#x\n",
			__FILE__, "__pmAuthSimpleCB", context, id);

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

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s ctx=%p id=%#x -> sts=%d rslt=%p len=%d\n",
		__FILE__, "__pmAuthSimpleCB",
		context, id, sts, *result, len ? *len : -1);
    return sts;
}

static int
__pmAuthSecretCB(sasl_conn_t *saslconn, void *context, int id, sasl_secret_t **secret)
{
    __pmHashCtl *attrs = (__pmHashCtl *)context;
    size_t length = 0;
    const char *password;

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s enter ctx=%p id=%#x\n",
			__FILE__, "__pmAuthSecretCB", context, id);

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

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s done ctx=%p id=%#x\n",
			__FILE__, "__pmAuthSecretCB", context, id);

    return SASL_OK;
}

static int
__pmAuthPromptCB(void *context, int id, const char *challenge, const char *prompt,
		 const char *defaultresult, const char **result, unsigned *length)
{
    char *value, message[512];

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s enter ctx=%p id=%#x\n",
			__FILE__, "__pmAuthPromptCB", context, id);

    if (id != SASL_CB_ECHOPROMPT && id != SASL_CB_NOECHOPROMPT)
	return SASL_BADPARAM;
    if (!prompt || !result || !length)
	return SASL_BADPARAM;
    if (defaultresult == NULL)
	defaultresult = "";

    if (!challenge)
	pmsprintf(message, sizeof(message), "%s [%s]: ", prompt, defaultresult);
    else
	pmsprintf(message, sizeof(message), "%s [challenge: %s] [%s]: ",
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

typedef struct __pmSecureContext {
    SSL_CTX		*ctx;
    __pmSecureConfig	cfg;
} __pmSecureContext;
static __pmSecureContext tls;	/* protected by secureclient_lock */

typedef struct {
    const char	*token;
    char	**value;
} config_parser;

int
__pmGetSecureConfig(__pmSecureConfig *config)
{
    const char	*path = pmGetOptionalConfig("PCP_TLSCONF_PATH");
    config_parser keywords[] = {
	{ "tls-cert-file",	&config->certfile },
	{ "tls-key-file",	&config->keyfile },
	{ "tls-ciphers",	&config->ciphers },
	{ "tls-ciphersuites",	&config->ciphersuites },
	{ "tls-ca-cert-file",	&config->cacertfile },
	{ "tls-ca-cert-dir",	&config->cacertdir },
	{ "tls-client-cert-file", &config->clientcertfile },
	{ "tls-client-key-file", &config->clientkeyfile },
	{ "tls-verify-clients",	&config->clientverify },
    };
    size_t	i, n;
    char	*p, *s, *end;
    char	line[BUFSIZ];
    FILE	*file;

    if ((path == NULL) || (file = fopen(path, "r")) == NULL)
	return -ENOENT;

    while ((p = fgets(line, sizeof(line), file)) != NULL) {
	end = NULL;
	for (s = p; *p; p++) {
	    if (isalpha(*p))
		continue;
	    if (s == p && isspace(*p)) {
		s++;		/* skip any preceding whitespace */
		continue;
	    }
	    if (*p == '#')	/* skip full comment lines */
		break;
	    if (*p == '\n')	/* skip empty lines */
		break;
	    if (*p == ':' || *p == '=') {
		*p = '\0';
		if (end == NULL)
		    end = p;	/* end of keyword */
		p++;
		break;
	    }
	    if (isspace(*p)) {
		if (end == NULL)
		    end = p;	/* end of keyword */
		*p = '\0';	/* trim trailing token whitespace */
	    }
	}
	if (end) {
	    /* token is from s -> end and already null terminated */
	    n = end - s;
	    for (i = 0; i < sizeof(keywords)/sizeof(keywords[0]); i++) {
		if (strncmp(s, keywords[i].token, n) != 0)
		    continue;
		/* cleanup value: remove leading & trailing space */
		while (*p) {
		    if (!isspace(*p))
			break;
		    p++;
		}
		s = p;	/* start value */
		if (*p == '#')
		    *p++ = '\0';
		else if (*p != '\0')
		    p++;
		n = strlen(p);
		end = s + n;
		for (; p <= end; p++) {
		    if (*p == '#' || isspace(*p)) {
			*p = '\0';
			break;
		    }
		}
		if (p <= end && *s != '\0') {	/* non-empty */
		    *(keywords[i].value) = strdup(s);
		    if (pmDebugOptions.tls)
			fprintf(stderr, "%s: %s = %s\n", "__pmGetSecureConfig",
					keywords[i].token, s);
		}
		break;
	    }
	}
    }
    fclose(file);
    return 0;
}

void
__pmSecureConfigInit(void)
{
    static int setup;

    PM_INIT_LOCKS();
    PM_LOCK(secureclient_lock);
    if (setup == 1) {
	PM_UNLOCK(secureclient_lock);
	return;
    }

    setup = 1;
    SSL_library_init();
    SSL_load_error_strings();
    PM_UNLOCK(secureclient_lock);
}

void
__pmInitSecureClients(void)
{
    /* all secure socket connections must use at least TLSv1.2 */
    int	flags = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
		SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1;
    int verify = SSL_VERIFY_NONE;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmInitSecureClients");

    __pmSecureConfigInit();

    PM_LOCK(secureclient_lock);
    if (tls.ctx != NULL) {
	PM_UNLOCK(secureclient_lock);
	return;
    }

    /* load optional /etc/pcp/tls.conf configuration file contents */
    __pmGetSecureConfig(&tls.cfg);

    tls.ctx = SSL_CTX_new(TLS_client_method());
    if (tls.ctx == NULL) {
	pmNotifyErr(LOG_ERR, "Cannot create initial secure client context");
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }

    SSL_CTX_set_options(tls.ctx, flags);
    if (tls.cfg.clientverify && (
	strcmp(tls.cfg.clientverify, "yes") == 0 ||
	strcmp(tls.cfg.clientverify, "true") == 0))
	verify |= SSL_VERIFY_PEER;
    SSL_CTX_set_verify(tls.ctx, verify, NULL);

    if (tls.cfg.cacertfile || tls.cfg.cacertdir) {
	if (!SSL_CTX_load_verify_locations(tls.ctx,
			tls.cfg.cacertfile, tls.cfg.cacertdir)) {
	    pmNotifyErr(LOG_ERR, "Cannot load the CA Certificate list from %s",
			tls.cfg.cacertfile ? tls.cfg.cacertfile :
			tls.cfg.cacertdir);
	    if (pmDebugOptions.tls)
		ERR_print_errors_fp(stderr);
	    goto fail;
	}
    } else {
	if (!SSL_CTX_set_default_verify_paths(tls.ctx)) {
	    pmNotifyErr(LOG_ERR, "Cannot set default CA paths");
	    if (pmDebugOptions.tls)
		ERR_print_errors_fp(stderr);
	    goto fail;
	}
    }

    if (tls.cfg.clientcertfile &&
	!SSL_CTX_use_certificate_chain_file(tls.ctx, tls.cfg.clientcertfile)) {
	pmNotifyErr(LOG_ERR, "Cannot load client certificate chain from %s",
		    tls.cfg.clientcertfile);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }
    else if (tls.cfg.certfile &&
	!SSL_CTX_use_certificate_chain_file(tls.ctx, tls.cfg.certfile)) {
	pmNotifyErr(LOG_ERR, "Cannot load certificate chain from %s",
		    tls.cfg.certfile);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }

    if (tls.cfg.clientcertfile && tls.cfg.clientkeyfile &&
	!SSL_CTX_use_PrivateKey_file(tls.ctx,
			    tls.cfg.clientkeyfile, SSL_FILETYPE_PEM)) {
	pmNotifyErr(LOG_ERR, "Cannot load client private key from %s",
		    tls.cfg.clientkeyfile);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }
    else if (tls.cfg.certfile && tls.cfg.keyfile &&
	!SSL_CTX_use_PrivateKey_file(tls.ctx,
			    tls.cfg.keyfile, SSL_FILETYPE_PEM)) {
	pmNotifyErr(LOG_ERR, "Cannot load private key from %s",
		    tls.cfg.keyfile);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }

    /* optional list of ciphers (TLSv1.2 and earlier) */
    if (tls.cfg.ciphers &&
	!SSL_CTX_set_cipher_list(tls.ctx, tls.cfg.ciphers)) {
	pmNotifyErr(LOG_ERR, "Cannot set the cipher list from %s",
		    tls.cfg.ciphers);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
        goto fail;
    }

    /* optional suites of ciphers (TLSv1.3 and later) */
    if (tls.cfg.ciphersuites &&
	!SSL_CTX_set_ciphersuites(tls.ctx, tls.cfg.ciphersuites)) {
	pmNotifyErr(LOG_ERR, "Cannot set the cipher suites from %s",
		    tls.cfg.ciphersuites);
	if (pmDebugOptions.tls)
	    ERR_print_errors_fp(stderr);
	goto fail;
    }

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: complete\n", "__pmInitSecureClients");

    /* success */
    PM_UNLOCK(secureclient_lock);
    return;

fail:
    SSL_CTX_free(tls.ctx);
    tls.ctx = NULL;
    PM_UNLOCK(secureclient_lock);
}

void
__pmFreeSecureConfig(__pmSecureConfig *config)
{
    free(config->certfile);
    free(config->keyfile);
    free(config->ciphers);
    free(config->ciphersuites);
    free(config->cacertfile);
    free(config->cacertdir);
    free(config->clientcertfile);
    free(config->clientkeyfile);
    free(config->clientverify);
    memset(config, 0, sizeof(*config));
}

int
__pmShutdownSecureSockets(void)
{
    PM_LOCK(secureclient_lock);
    if (tls.ctx)
	SSL_CTX_free(tls.ctx);
    __pmFreeSecureConfig(&tls.cfg);
    PM_UNLOCK(secureclient_lock);
    return 0;
}

static int
__pmSecureClientIPCFlags(int fd, int flags, const char *host, __pmHashCtl *attrs)
{
    __pmSecureSocket	ss;
    sasl_callback_t	*cb;
    char		hostname[MAXHOSTNAMELEN];
    int			sts;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmSecureClientIPCFlags");

    if (__pmDataIPC(fd, &ss) < 0)
	return -EOPNOTSUPP;

    if ((flags & (PDU_FLAG_SECURE|PDU_FLAG_AUTH)) != 0) {
	if (!host || host[0] == '/' || strncmp(host, "local", 5) == 0)
	    gethostname(hostname, sizeof(hostname)-1);
	else
	    strncpy(hostname, host, sizeof(hostname)-1);
	hostname[MAXHOSTNAMELEN-1] = '\0';
    }

    if ((flags & PDU_FLAG_SECURE) != 0) {
	__pmInitSecureClients();
	if (tls.ctx == NULL)
	    return PM_ERR_NOTCONN;
	if ((ss.ssl = SSL_new(tls.ctx)) == NULL)
	    return PM_ERR_NOTCONN;
	if (!SSL_set_tlsext_host_name(ss.ssl, hostname)) {
	    pmNotifyErr(LOG_ERR, "%s: setting TLS hostname: %s\n",
			"__pmSecureClientIPCFlags", hostname);
	    SSL_free(ss.ssl);
	}
	SSL_set_mode(ss.ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_set_fd(ss.ssl, fd);
	SSL_set_connect_state(ss.ssl);	/* client */

	if (pmDebugOptions.tls)
	    fprintf(stderr, "%s: switching fd=%d to TLS mode\n",
			    "__pmSecureClientIPCFlags", fd);

	/* save changes back into the IPC table (updates ssl) */
	__pmSetDataIPC(fd, (void *)&ss);
    }

    if ((flags & PDU_FLAG_AUTH) != 0) {
	__pmInitAuthClients();
	ss.saslCB = calloc(LIMIT_CLIENT_CALLBACKS, sizeof(sasl_callback_t));
	if ((cb = ss.saslCB) == NULL)
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
	assert(cb - ss.saslCB <= LIMIT_CLIENT_CALLBACKS);

	sts = sasl_client_new(SECURE_SERVER_SASL_SERVICE,
				hostname,
				NULL, NULL, /*iplocal,ipremote*/
				ss.saslCB,
				0, &ss.saslConn);
	if (sts != SASL_OK && sts != SASL_CONTINUE)
	    return __pmSecureSocketsError(sts);

	/* save changes back into the IPC table (updates saslConn) */
	__pmSetDataIPC(fd, (void *)&ss);
    }

    return 0;
}

int
__pmSecureServerNegotiation(int fd, int *strength)
{
    __pmSecureSocket ss;
    int found = 0;
    int sts, err;

    if (pmDebugOptions.tls)
	fprintf(stderr, "__pmSecureServerNegotiation: entered\n");

    if (__pmDataIPC(fd, &ss) < 0)
	return -EOPNOTSUPP;

    ERR_clear_error();	/* clear/reset for a new handshake */

    /*
     * one-trip initialization to trace TLS messages send/received
     * from ssl library when -Dtls,desperate in play
     */
    if (pmDebugOptions.tls && pmDebugOptions.desperate) {
#ifdef HAVE_SSL_TRACE
	static BIO	*mybio = NULL;
	PM_INIT_LOCKS();
	PM_LOCK(secureclient_lock);
	if (mybio == NULL) {
	    /* hook into library's default callback */
	    SSL_set_msg_callback(ss.ssl, SSL_trace);
	    /* make diags from SSL_trace() go to our stderr */
	    mybio = BIO_new_fp(stderr, 0);
	    SSL_set_msg_callback_arg(ss.ssl, mybio);
	}
	PM_UNLOCK(secureclient_lock);
#else
	fprintf(stderr, "__pmSecureServerNegotiation: Warning: no SSL_trace() support in libssl\n");
#endif
    }

    if ((sts = SSL_accept(ss.ssl)) <= 0) {
	/* handshake failed, return an appropriate error */
	err = SSL_get_error(ss.ssl, sts);
	if (pmDebugOptions.tls)
	    fprintf(stderr, "__pmSecureServerNegotiation: SSL_accept -> %d (SSL_get_error -> %d)\n", sts, err);
	switch (err) {
	case SSL_ERROR_ZERO_RETURN:
	    return -ENOTCONN;
	case SSL_ERROR_NONE:
	default:
	    break;
	}
	return PM_ERR_TLS;
    }

    found = SSL_get_cipher_bits(ss.ssl, strength);
    if (found == 0)
	*strength = DEFAULT_SECURITY_STRENGTH;

    return 0;
}

int
__pmSecureClientNegotiation(int fd, int *strength)
{
    __pmSecureSocket ss;
    int found = 0;
    int sts, err;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmSecureClientNegotiation");

    if (__pmDataIPC(fd, &ss) < 0)
	return -EOPNOTSUPP;

    ERR_clear_error();	/* clear/reset for a new handshake */

    if ((sts = SSL_connect(ss.ssl)) <= 0) {
	/* handshake failed, return an appropriate error */
	switch ((err = SSL_get_error(ss.ssl, sts))) {
	case SSL_ERROR_ZERO_RETURN:
	    return -ENOTCONN;
	case SSL_ERROR_NONE:
	default:
	    break;
	}
	return PM_ERR_TLS;
    }

    found = SSL_get_cipher_bits(ss.ssl, strength);
    if (found == 0)
	*strength = DEFAULT_SECURITY_STRENGTH;

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
__pmInitAuthServer(void)
{
    __pmInitAuthPaths();
    if (sasl_server_init(common_callbacks, pmGetProgname()) != SASL_OK) {
	pmNotifyErr(LOG_ERR, "Failed to start authenticating server");
	return -EINVAL;
    }
    return 0;
}

int
__pmInitAuthClients(void)
{
    __pmInitAuthPaths();
    if (sasl_client_init(common_callbacks) != SASL_OK)
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
    __pmSecureSocket ss;
    __pmHashNode *node;
    __pmPDU *pb;

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s(fd=%d, ssf=%d, host=%s)\n",
		__FILE__, "__pmAuthClientNegotiation", fd, ssf, hostname);

    if (__pmDataIPC(fd, &ss) < 0)
	return -EOPNOTSUPP;

    /* setup all the security properties for this connection */
    if ((sts = __pmAuthClientSetProperties(ss.saslConn, ssf)) < 0)
	return sts;

    /* lookup users preferred connection method, if specified */
    if ((node = __pmHashSearch(PCP_ATTR_METHOD, attrs)) != NULL)
	method = (const char *)node->data;

    if (pmDebugOptions.auth)
	fprintf(stderr, "%s:%s requesting \"%s\" method\n",
		__FILE__, "__pmAuthClientNegotiation",
		method ? method : "default");

    /* get security mechanism list */ 
    sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
    if (sts == PDU_AUTH) {
	sts = __pmDecodeAuth(pb, &zero, &payload, &length);
	if (sts >= 0) {
	    strncpy(buffer, payload, length);
	    buffer[length] = '\0';

	    if (pmDebugOptions.auth)
		fprintf(stderr, "%s:%s got methods: \"%s\" (%d)\n",
			__FILE__, "__pmAuthClientNegotiation", buffer, length);
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
	    saslsts = sasl_client_start(ss.saslConn, buffer, NULL,
					     (const char **)&payload,
					     (unsigned int *)&length, &method);
	    if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
		sts = __pmSecureSocketsError(saslsts);
		if (pmDebugOptions.auth)
		    fprintf(stderr, "sasl_client_start failed: %d (%s)\n",
				    saslsts, pmErrStr(sts));
	    }
	}
    }
    else if (sts == PDU_ERROR)
	__pmDecodeError(pb, &sts);
    else if (sts != PM_ERR_TIMEOUT)
	sts = PM_ERR_IPC;

    if (pinned > 0)
	__pmUnpinPDUBuf(pb);
    if (sts < 0)
	return sts;

    if (pmDebugOptions.auth)
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

    if (pmDebugOptions.auth)
	fprintf(stderr, "sasl_client_start sending (%d bytes) \"%s\"\n",
		length, buffer);

    if ((sts = __pmSendAuth(fd, FROM_ANON, 0, buffer, length)) < 0)
	return sts;

    while (saslsts == SASL_CONTINUE) {
	const char *data = NULL;

	if (pmDebugOptions.auth)
	    fprintf(stderr, "%s:%s awaiting server reply\n",
			    __FILE__, "__pmAuthClientNegotiation");

	sts = pinned = __pmGetPDU(fd, ANY_SIZE, TIMEOUT_DEFAULT, &pb);
	if (sts == PDU_AUTH) {
	    sts = __pmDecodeAuth(pb, &zero, &payload, &length);
	    if (sts >= 0) {
		saslsts = sasl_client_step(ss.saslConn, payload, length, NULL,
					    &data, (unsigned int *)&length);
		if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
		    sts = __pmSecureSocketsError(saslsts);
		    if (pmDebugOptions.auth)
			fprintf(stderr, "sasl_client_step failed: %d (%s)\n",
					saslsts, pmErrStr(sts));
		    break;
		}
		if (pmDebugOptions.auth) {
		    fprintf(stderr, "%s:%s step send (%d bytes)\n",
			    __FILE__, "__pmAuthClientNegotiation", length);
		}
	    }
	}
	else if (sts == PDU_ERROR)
	    __pmDecodeError(pb, &sts);
	else if (sts != PM_ERR_TIMEOUT)
	    sts = PM_ERR_IPC;

	if (pinned > 0)
	    __pmUnpinPDUBuf(pb);
	if (sts >= 0)
	    sts = __pmSendAuth(fd, FROM_ANON, 0, (length && data) ? data : "", length);
	if (sts < 0)
	    break;
    }

    if (pmDebugOptions.auth) {
	if (sts < 0)
	    fprintf(stderr, "%s:%s loop failed\n",
			    __FILE__, "__pmAuthClientNegotiation");
	else {
	    saslsts = sasl_getprop(ss.saslConn, SASL_USERNAME,
				    (const void **)&payload);
	    fprintf(stderr, "%s:%s success, username=%s\n",
			    __FILE__, "__pmAuthClientNegotiation",
			    saslsts != SASL_OK ? "?" : payload);
	}
    }

    return sts;
}

int
__pmSecureClientHandshake(int fd, int flags, const char *hostname, __pmHashCtl *attrs)
{
    int sts, ssf = DEFAULT_SECURITY_STRENGTH;

    if (pmDebugOptions.tls)
	fprintf(stderr, "%s: entered\n", "__pmSecureClientHandshake");

    /* deprecated, insecure */
    if ((flags & PDU_FLAG_COMPRESS))
	return -EOPNOTSUPP;

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
	    return PM_ERR_IPC;
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
__pmGetUserAuthData(int fd)
{
    __pmSecureSocket socket;

    if (__pmDataIPC(fd, &socket) < 0)
	return NULL;
    return (void *)socket.saslConn;
}

static void
sendSecureAck(int fd, int flags, int sts)
{
    /*
     * At this point we've attempted some required initialization for secure
     * sockets. If the client wants a secure-ack then send an error pdu
     * containing our status. The client will then know whether or not to
     * proceed with the secure handshake.
     */
    if (flags & PDU_FLAG_SECURE_ACK)
	__pmSendError(fd, FROM_ANON, sts);
}

int
__pmSecureServerIPCFlags(int fd, int flags, void *ctx)
{
    __pmSecureSocket ss;
    SSL_CTX *context = (SSL_CTX *)ctx;
    char hostname[MAXHOSTNAMELEN];
    int saslsts;
    int verify;
    int sts;

    if (pmDebugOptions.auth)
	fprintf(stderr, "__pmSecureServerIPCFlags: entered\n");

    if ((flags & PDU_FLAG_COMPRESS) != 0)
	return -EOPNOTSUPP;

    if ((flags & (PDU_FLAG_SECURE|PDU_FLAG_AUTH)) != 0) {
	gethostname(hostname, sizeof(hostname)-1);
	hostname[MAXHOSTNAMELEN-1] = '\0';
    }

    if (__pmDataIPC(fd, &ss) < 0)
	return -EOPNOTSUPP;

    if ((flags & PDU_FLAG_SECURE) != 0) {
	if ((ss.ssl = SSL_new(context)) == NULL) {
	    sts = PM_ERR_NOTCONN;
	    sendSecureAck(fd, flags, sts);
	    if (sts < 0 && pmDebugOptions.auth)
		fprintf(stderr, "__pmSecureServerIPCFlags: sendSecureAck -> %d\n", sts);
	    return sts;
	}
	SSL_set_mode(ss.ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
	SSL_set_tlsext_host_name(ss.ssl, hostname);
	SSL_set_fd(ss.ssl, fd);
	SSL_set_accept_state(ss.ssl);	/* server */

	/*
 	 * If called from pmcd, the server may have the feature set by
	 * a command line option.  If called from pmproxy, "flags" is
	 * set if required by an upstream pmcd.  Needs to be forwarded
	 * through to the client.
 	 */
	verify = SSL_VERIFY_PEER;
	if ((__pmServerHasFeature(PM_SERVER_FEATURE_CERT_REQD)) ||
	    (flags & PDU_FLAG_CERT_REQD))
	    verify |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
	SSL_set_verify(ss.ssl, verify, NULL);

	sendSecureAck(fd, flags, 0);

	if (pmDebugOptions.tls)
	    fprintf(stderr, "__pmSecureServerIPCFlags: switching server fd=%d to TLS mode\n", fd);

	/* save ssl changes back into the IPC table */
	__pmSetDataIPC(fd, (void *)&ss);
    }

    if ((flags & PDU_FLAG_AUTH) != 0) {
	sts = __pmInitAuthServer();
	if (sts < 0) {
	    if (pmDebugOptions.auth)
		fprintf(stderr, "__pmSecureServerIPCFlags: __pmInitAuthServer -> %d\n", sts);
	    return sts;
	}

	/**
	 * SASL stores username@hostname in a SASL DB (see sasldblistusers2(8))
	 * saslpasswd2(8) uses gethostname() to determine the hostname
	 * sasl_server_new() uses get_fqhostname() to determine a FQDN
	 * if the hostname parameter is NULL
	 *
	 * Therefore, if the hostname doesn't match the FQDN of the system
	 * running pmcd, the authentication is broken.  As a workaround,
	 * use gethostname() as parameter to sasl_server_new
	 */
	saslsts = sasl_server_new(SECURE_SERVER_SASL_SERVICE,
				hostname, NULL, /*serverFQDN,user_realm*/
				NULL, NULL, NULL, /*iplocal,ipremote,callbacks*/
				0, &ss.saslConn);
	if (pmDebugOptions.auth)
	    fprintf(stderr, "__pmSecureServerIPCFlags: SASL server: %d\n", saslsts);
	if (saslsts != SASL_OK && saslsts != SASL_CONTINUE) {
	    sts = __pmSecureSocketsError(saslsts);
	    if (sts < 0 && pmDebugOptions.auth)
		fprintf(stderr, "__pmSecureServerIPCFlags: __pmSecureSocketsError -> %d\n", sts);
	    return sts;
	}

	/* save saslConn changes back into the IPC table */
	sts = __pmSetDataIPC(fd, (void *)&ss);
	if (sts < 0 && pmDebugOptions.auth)
	    fprintf(stderr, "__pmSecureServerIPCFlags: __pmSetDataIPC -> %d\n", sts);
	return sts;
    }

    return 0;
}

ssize_t
__pmWrite(int fd, const void *buffer, size_t length)
{
    __pmSecureSocket ss;
    int sts, bytes;

    if (__pmDataIPC(fd, &ss) == 0 && ss.ssl) {
	do {
	    bytes = SSL_write(ss.ssl, buffer, length);
	    sts = SSL_get_error(ss.ssl, bytes);
	    if (sts == SSL_ERROR_WANT_READ || sts == SSL_ERROR_WANT_WRITE)
		continue;
	    if (sts == SSL_ERROR_ZERO_RETURN)
		return 0;
	    if (sts == SSL_ERROR_NONE)
		return bytes;
	    return PM_ERR_TLS;
	} while (1);
    }
    return write(fd, buffer, length);
}

ssize_t
__pmRead(int fd, void *buffer, size_t length)
{
    __pmSecureSocket ss;
    int sts, bytes;

    if (fd < 0)
	return -EBADF;

    if (__pmDataIPC(fd, &ss) == 0 && ss.ssl) {
	do {
	    bytes = SSL_read(ss.ssl, buffer, length);
	    sts = SSL_get_error(ss.ssl, bytes);
	    if (sts == SSL_ERROR_WANT_READ || sts == SSL_ERROR_WANT_WRITE)
		continue;
	    if (sts == SSL_ERROR_ZERO_RETURN)
		return 0;
	    if (sts == SSL_ERROR_NONE)
		return bytes;
	    return PM_ERR_TLS;
	} while (1);
    }
    return read(fd, buffer, length);
}

ssize_t
__pmSend(int fd, const void *buffer, size_t length, int flags)
{
    __pmSecureSocket ss;
    int	sts, bytes;

    if (fd < 0)
	return -EBADF;

    if (__pmDataIPC(fd, &ss) == 0 && ss.ssl) {
	do {
	    bytes = SSL_write(ss.ssl, buffer, length);
	    sts = SSL_get_error(ss.ssl, bytes);
	    if (sts == SSL_ERROR_WANT_READ || sts == SSL_ERROR_WANT_WRITE)
		continue;
	    if (sts == SSL_ERROR_ZERO_RETURN)
		return 0;
	    if (sts == SSL_ERROR_NONE)
		return bytes;
	    return PM_ERR_TLS;
	} while (1);
    }
    return send(fd, buffer, length, flags);
}

ssize_t
__pmRecv(int fd, void *buffer, size_t length, int flags)
{
    __pmSecureSocket	ss;
    ssize_t		bytes;
    int			debug;
    int			sts;

    if (fd < 0)
	return -EBADF;

    debug = pmDebugOptions.tls ||
	    (pmDebugOptions.pdu && pmDebugOptions.desperate);

    if (__pmDataIPC(fd, &ss) == 0 && ss.ssl) {
	if (debug)
	    fprintf(stderr, "%s:__pmRecv[secure](", __FILE__);
	do {
	    bytes = SSL_read(ss.ssl, buffer, length);
	    sts = SSL_get_error(ss.ssl, bytes);
	    if (sts == SSL_ERROR_WANT_READ || sts == SSL_ERROR_WANT_WRITE)
		continue;
	    if (debug)
		fprintf(stderr, "%d, ..., %d, 0x%x) -> %d\n",
			fd, (int)length, flags, (int)bytes);
	    if (sts == SSL_ERROR_ZERO_RETURN)
		return 0;
	    if (sts == SSL_ERROR_NONE)
		return bytes;
	    return PM_ERR_TLS;
	} while (1);
    }

    if (debug)
	fprintf(stderr, "%s:__pmRecv(", __FILE__);
    bytes = recv(fd, buffer, length, flags);
    if (debug)
	fprintf(stderr, "%d, ..., %d, 0x%x) -> %d\n",
		fd, (int)length, flags, (int)bytes);
    return bytes;
}

/*
 * In certain situations, we need to allow access to previously-read
 * data on a socket.  This is because, for example, the SSL protocol
 * buffering may have already consumed data that we are now expecting
 * (in this case, its buffered internally and a socket read will give
 * up that data).
 */
int
__pmSocketReady(int fd, struct timeval *timeout)
{
    __pmSecureSocket ss;
    __pmFdSet onefd;

    if (fd < 0)
	return -EBADF;

    if (__pmDataIPC(fd, &ss) == 0 && ss.ssl)
	if (SSL_pending(ss.ssl) > 0)
	    return 1;	/* proceed without blocking */

    FD_ZERO(&onefd);
    FD_SET(fd, &onefd);
    return select(fd+1, &onefd, NULL, NULL, timeout);
}
