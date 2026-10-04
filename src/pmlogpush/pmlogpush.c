/*
 * Copyright (c) 2025 Red Hat.
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

#define PMAPI_VERSION 3

#include "pmapi.h"
#include "pmhttp.h"
#include "libpcp.h"

#define AJ_STR "application/json"
#define AJ_LEN (sizeof(AJ_STR)-1)
#define AO_STR "application/octet-stream"

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Options"),
    PMOPT_DEBUG,
    { "host", 1, 'h', "HOST", "pmproxy HTTP connection host name" },
    { "port", 1, 'p', "PORT", "pmproxy HTTP connection port number" },
    { "secure", 0, 'S', NULL, "use a secure (TLS) HTTPS connection" },
    { "username", 1, 'U', "USER", "user name for HTTP Basic authentication" },
    { "password-file", 1, 'P', "FILE", "file holding the HTTP Basic auth password" },
    { "unix", 1, 's', "PATH", "pmproxy HTTP Unix domain socket path" },
    { "verbose", 0, 'v', NULL, "verbose progress diagnostics" },
    PMOPT_VERSION,
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static int
overrides(int opt, pmOptions *opts)
{
    switch (opt) {
    case 'h': case 'p': case 's': case 'S': case 'U': case 'P':
	return 1;
    }
    return 0;
}

static pmOptions opts = {
    .version = PMAPI_VERSION_3,
    .flags = PM_OPTFLAG_DONE,
    .short_options = "D:h:p:s:SU:P:Vv?",
    .long_options = longopts,
    .short_usage = "[options] archive",
    .override = overrides,
};

static int verbose;
static int secure;		/* use https (TLS) instead of http */
static char *hostname = "localhost";
static char *unix_socket;
static char *username;		/* HTTP Basic auth user name (optional) */
static char *passfile;		/* file to read the Basic auth password from */
static int port = 44322;
static char *body, *type;
static size_t body_bytes, type_bytes;

/*
 * Resolve the HTTP Basic auth password: from the --password-file if given,
 * otherwise from the PCP_PUSH_PASSWORD environment variable.  The password is
 * never accepted on the command line, so it does not appear in the process
 * argument list.  Returns a malloc'd string, or NULL if none was provided.
 */
static char *
resolve_password(void)
{
    char	buf[BUFSIZ], *value;
    size_t	n;
    FILE	*fp;

    if (passfile == NULL) {
	if ((value = getenv("PCP_PUSH_PASSWORD")) == NULL)
	    return NULL;
	return strdup(value);
    }
    if ((fp = fopen(passfile, "r")) == NULL) {
	fprintf(stderr, "%s: cannot open password file \"%s\": %s\n",
		pmGetProgname(), passfile, osstrerror());
	exit(1);
    }
    if (fgets(buf, sizeof(buf), fp) == NULL) {
	fprintf(stderr, "%s: cannot read password from \"%s\"\n",
		pmGetProgname(), passfile);
	fclose(fp);
	exit(1);
    }
    fclose(fp);
    for (n = strlen(buf); n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r'); )
	buf[--n] = '\0';
    return strdup(buf);
}

/*
 * Wire up HTTP Basic authentication when a user name was supplied.  The
 * credential is a cleartext secret, so it is only sent over a secure (-S)
 * connection - refuse to proceed otherwise rather than risk leaking it.
 */
static void
setup_credentials(struct http_client *client)
{
    char	*password;

    if (username == NULL)
	return;
    if (unix_socket != NULL) {
	/*
	 * A Unix domain socket never carries a TLS session, so credentials
	 * could not be transmitted (http_client_auth refuses to send them in
	 * the clear) - reject the combination up front rather than fail later.
	 */
	fprintf(stderr, "%s: authentication (-U) is not supported over a "
		"Unix domain socket (-s)\n", pmGetProgname());
	exit(1);
    }
    if (!secure) {
	fprintf(stderr, "%s: authentication (-U) requires a secure (-S) "
		"connection\n", pmGetProgname());
	exit(1);
    }
    if ((password = resolve_password()) == NULL) {
	fprintf(stderr, "%s: no password for user \"%s\" - set "
		"$PCP_PUSH_PASSWORD or use -P/--password-file\n",
		pmGetProgname(), username);
	exit(1);
    }
    pmhttpClientSetCredentials(client, username, password);
}

/*
 * Build the pmproxy connection URL - a unix domain socket if one was given,
 * otherwise http:// or (with -S) an encrypted https:// endpoint.
 */
static void
build_conn(char *conn, size_t size)
{
    if (unix_socket != NULL)
	pmsprintf(conn, size, "unix:/%s", unix_socket);
    else
	pmsprintf(conn, size, "%s://%s:%u",
			secure ? "https" : "http", hostname, port);
}

static int
httpError(int code, char *buf, size_t buflen)
{
    switch (code) {
    case 400:	/* BAD_REQUEST */
	pmsprintf(buf, buflen, "bad request (%d)", code);
	return code;
    case 409:	/* CONFLICT */
	pmsprintf(buf, buflen, "conflict (%d) - archive already exists", code);
	return code;
    case 422:	/* UNPROCESSABLE_ENTITY */
	pmsprintf(buf, buflen, "unprocessable (%d) - bad archive label", code);
	return code;
    case 500: /* INTERNAL_SERVER_ERROR */
    default:
	if (code >= 200 && code < 300)	/* OK */
	    break;
	pmsprintf(buf, buflen, "server error (%d)", code);
	return code;
    }
    return 0;
}

static size_t
pushLabel(struct http_client *client, __pmLogLabel *lp, int *archive)
{
    /*
     * +32 comes from
     *     https://<host>:<port>
     *     12345678      9012345^\0 [at most +16]
     */
    char		conn[MAXHOSTNAMELEN+32], path[64];
    void		*buffer;
    size_t		bytes;
    int			sts;

    if ((sts = __pmLogEncodeLabel(lp, &buffer, &bytes)) < 0) {
	fprintf(stderr, "%s: Cannot encode label: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    if (verbose)
	fprintf(stderr, "HTTP POST v%d label log from host %s [%zu bytes]\n",
			lp->magic & 0xff, lp->hostname, bytes);

    build_conn(conn, sizeof(conn));
    pmsprintf(path, sizeof(path), "/logger/label");

    sts = pmhttpClientPost(client, conn, path, buffer, bytes, AO_STR,
			    &body, &body_bytes, &type, &type_bytes);
    free(buffer);

    if (sts < 0) {
	fprintf(stderr, "%s: %s %s POST failed: %s\n",
			pmGetProgname(), conn, path, pmErrStr(sts));
	exit(1);
    }
    if (httpError(sts, conn, sizeof(conn))) {
	fprintf(stderr, "%s: HTTP error: %s\n", pmGetProgname(), conn);
	exit(1);
    }
    if (type_bytes != AJ_LEN || strcmp(type, AJ_STR) != 0) {
	fprintf(stderr, "%s: %s unexpected response type: %s (%zu)\n",
			pmGetProgname(), path, type, type_bytes);
	exit(1);
    }
    if (body_bytes < 10) {
	fprintf(stderr, "%s: %s unexpected body size\n", pmGetProgname(), path);
	exit(1);
    }

    if (verbose)
	fprintf(stderr, "%s: label POST result: %s\n", __FUNCTION__, body);

    sts = sscanf(body, "{\"archive\":%u,\"success\":true}", archive);
    if (sts != 1) {
	fprintf(stderr, "%s: %s bad result (expected 1 archive, got %d): %s\n",
			pmGetProgname(), path, sts, body);
	exit(1);
    }

    return bytes;
}

static void
pushFile(const char *endpoint,
	struct http_client *cp, size_t start, __pmFILE *fp, int archive)
{
    /* see note above to explain +32 */
    char		conn[MAXHOSTNAMELEN+32], path[64];
    char		buffer[BUFSIZ];
    size_t		bytes;
    int			sts, count = 0;

    build_conn(conn, sizeof(conn));
    pmsprintf(path, sizeof(path), "/logger/%s/%u", endpoint, archive);

    __pmFseek(fp, (long)start, SEEK_SET);
    for ( ; ; ) {
	if ((bytes = __pmFread(buffer, 1, sizeof(buffer), fp)) <= 0) {
	    if (!__pmFerror(fp))
		break;
	    sts = -oserror();
	    fprintf(stderr, "%s: %s read failed: %s\n",
		    pmGetProgname(), endpoint, pmErrStr(sts));
	    exit(1);
	}

	if (verbose)
	    fprintf(stderr, "HTTP %s POST (archive=%u, %zu bytes)\n",
		    endpoint, archive, bytes);

	if ((sts = pmhttpClientPost(cp, conn, path,
				buffer, bytes, AO_STR,
				&body, &body_bytes,
				&type, &type_bytes)) < 0) {
	    fprintf(stderr, "%s: %s %s POST failed: %s\n",
		    pmGetProgname(), conn, path, pmErrStr(sts));
	    exit(1);
	}
	if (httpError(sts, conn, sizeof(conn))) {
	    fprintf(stderr, "%s: %s HTTP error: %s\n",
			    pmGetProgname(), path, conn);
	    exit(1);
	}
	if (type_bytes != AJ_LEN || strcmp(type, AJ_STR) != 0) {
	    fprintf(stderr, "%s: %s unexpected response type: %s (%zu)\n",
			    pmGetProgname(), path, type, type_bytes);
	    exit(1);
	}
	if (body_bytes < 10) {
	    fprintf(stderr, "%s: %s unexpected short result\n",
			    pmGetProgname(), path);
	    exit(1);
	}

	if (verbose)
	    fprintf(stderr, "%s: %s POST [%d] result: %s\n",
			    __FUNCTION__, endpoint, ++count, body);
    }
}

static void
pushMeta(struct http_client *cp, size_t start, __pmFILE *fp, int log)
{
    pushFile("meta", cp, start, fp, log);
}

static void
pushIndex(struct http_client *cp, size_t start, __pmFILE *fp, int log)
{
    pushFile("index", cp, start, fp, log);
}

static void
pushVolume(struct http_client *cp, size_t start, __pmFILE *fp, int vol, int log)
{
    char	volume[64];

    pmsprintf(volume, sizeof(volume), "volume/%d", vol);
    pushFile(volume, cp, start, fp, log);
}

int
main(int argc, char *argv[])
{
    int			c, ctx, id;
    int			vol, sts;
    char		*pathname;
    size_t		offset;
    struct http_client	*client;
    __pmLogLabel	label = {0};
    __pmContext		*ctxp;
    __pmArchCtl		*acp;
    __pmLogCtl		*lcp;

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'h':
	    hostname = opts.optarg;
	    break;
	case 'p':
	    port = atoi(opts.optarg);
	    break;
	case 's':
	    unix_socket = opts.optarg;
	    break;
	case 'S':	/* secure (TLS) https connection */
	    secure = 1;
	    break;
	case 'U':	/* HTTP Basic auth user name */
	    username = opts.optarg;
	    break;
	case 'P':	/* file holding the HTTP Basic auth password */
	    passfile = opts.optarg;
	    break;
	case 'v':	/* verbose diagnostics */
	    verbose = 1;
	    break;
	}
    }

    if (opts.errors ||
	(opts.flags & PM_OPTFLAG_EXIT) ||
	(opts.optind > argc - 1 && !opts.narchives)) {
	sts = !(opts.flags & PM_OPTFLAG_EXIT);
	pmUsageMessage(&opts);
	exit(sts);
    }

    /* delay option end processing until now that we have the archive name */
    if (opts.narchives == 0)
	__pmAddOptArchive(&opts, argv[opts.optind++]);
    opts.flags &= ~PM_OPTFLAG_DONE;
    __pmEndOptions(&opts);

    pathname = opts.archives[0];
    if ((sts = ctx = pmNewContext(PM_CONTEXT_ARCHIVE, pathname)) < 0) {
	fprintf(stderr, "%s: Cannot open archive \"%s\": %s\n",
		pmGetProgname(), pathname, pmErrStr(sts));
	exit(1);
    }
    if (pmGetContextOptions(ctx, &opts)) {
	pmflush();
	exit(1);
    }

    /* pmlogpush is single threaded but this returns with context lock */
    if ((ctxp = __pmHandleToPtr(ctx)) == NULL) {
	fprintf(stderr, "%s: botch: __pmHandleToPtr(%d) returns NULL!\n",
		pmGetProgname(), ctx);
	exit(1);
    }
    PM_UNLOCK(ctxp->c_lock);
    acp = ctxp->c_archctl;
    lcp = acp->ac_log;

    if (acp->ac_log_list != NULL && acp->ac_num_logs > 1) {
	fprintf(stderr, "%s: multi-archive contexts not supported\n",
		pmGetProgname());
	exit(1);
    }

    if ((sts = __pmLogLoadLabel(acp->ac_mfp, &label)) < 0) {
	fprintf(stderr, "%s: Cannot get archive label record: %s\n",
		pmGetProgname(), pmErrStr(sts));
	exit(1);
    }

    client = pmhttpNewClient();
    setup_credentials(client);
    offset = pushLabel(client, &label, &id);
    __pmLogFreeLabel(&label);

    pushMeta(client, offset, lcp->mdfp, id);
    pushIndex(client, offset, lcp->tifp, id);
    for (vol = lcp->minvol; vol <= lcp->maxvol; vol++) {
	if ((sts = __pmLogChangeVol(acp, vol)) < 0)
	    continue;
	pushVolume(client, offset, acp->ac_mfp, vol, id);
    }

    pmhttpFreeClient(client);
    return 0;
}
