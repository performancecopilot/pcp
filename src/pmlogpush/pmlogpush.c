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
    case 'h': case 'p': case 's':
	return 1;
    }
    return 0;
}           

static pmOptions opts = {
    .version = PMAPI_VERSION_3,
    .flags = PM_OPTFLAG_DONE,
    .short_options = "D:h:p:s:Vv?",
    .long_options = longopts,
    .short_usage = "[options] archive",
    .override = overrides,
};

static int verbose;
static char *hostname = "localhost";
static char *unix_socket;
static int port = 44322;
static char *body, *type;
static size_t body_bytes, type_bytes;

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
    case 500: /* INTERNAL_SERVER_ERROR*/
    default:
	if (code == 200)	/* OK */
	    break;
	pmsprintf(buf, buflen, "server error (%d)", code);
	return code;
    }
    return 0;
}

static size_t
pushLabel(struct http_client *client, __pmLogLabel *lp, int *archive)
{
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

    if (unix_socket == NULL)
	pmsprintf(conn, sizeof(conn), "http://%s:%u", hostname, port);
    else
	pmsprintf(conn, sizeof(conn), "unix:/%s", unix_socket);
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
    char		conn[MAXHOSTNAMELEN+32], path[64];
    char		buffer[BUFSIZ];
    size_t		bytes;
    int			sts, count = 0;

    if (unix_socket == NULL)
	pmsprintf(conn, sizeof(conn), "http://%s:%u", hostname, port);
    else
	pmsprintf(conn, sizeof(conn), "unix:/%s", unix_socket);
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
