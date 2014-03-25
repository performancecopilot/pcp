/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2014 Red Hat.
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

#include "pmwebapi.h"

const char uriprefix[] = "/pmapi";
char *resourcedir;	     /* set by -R option */
char *archivesdir = ".";       /* set by -A option */
unsigned verbosity;	    /* set by -v option */
unsigned maxtimeout = 300;     /* set by -t option */
unsigned perm_context = 1;     /* set by -c option, changed by -h/-a/-L */
unsigned new_contexts_p = 1;   /* set by -N option */
unsigned exit_p;	       /* counted by SIG* handler */
static char *logfile = "pmwebd.log";
static char *fatalfile = "/dev/tty"; /* fatal messages at startup go here */
static char *username;

static int
mhd_log_args(void *connection, enum MHD_ValueKind kind, 
		const char *key, const char *value)
{
    (void)kind;
    pmweb_notify(LOG_DEBUG, connection, "%s%s%s",
		 key, value ? "=" : "", value ? value : "");
    return MHD_YES;
}

/*
 * Respond to a new incoming HTTP request.  It may be
 * one of three general categories:
 * (a) creation of a new PMAPI context: do it
 * (b) operation on an existing context: do it
 * (c) access to some non-API URI: serve it from $resourcedir/ if configured.
 */
static int
mhd_respond(void *cls, struct MHD_Connection *connection,
	   const char *url, const char *method, const char *version,
	   const char *upload_data, size_t *upload_data_size, void **con_cls)
{
   /* "error_page" could also be an actual error page... */
    static const char	error_page[] = "PMWEBD error";
    static int		dummy;

    struct MHD_Response	*resp = NULL;
    int			sts;

    /*
     * MHD calls us at least twice per request.  Skip the first one,
     * since it only gives us headers, and not any POST content.
     */
    if (& dummy != *con_cls) {
	*con_cls = &dummy;
	return MHD_YES;
    }
    *con_cls = NULL;

    if (verbosity > 1)
	pmweb_notify(LOG_INFO, connection, "%s %s %s", version, method, url);
    if (verbosity > 2) /* Print arguments too. */
	(void) MHD_get_connection_values(connection, MHD_GET_ARGUMENT_KIND,
					 &mhd_log_args, connection);

    /* pmwebapi? */
    if (0 == strncmp(url, uriprefix, strlen(uriprefix)))
	return pmwebapi_respond(cls, connection,
				&url[strlen(uriprefix)+1], /* strip prefix */
				method, upload_data, upload_data_size);
    /* pmresapi? */
    else if (0 == strcmp(method, "GET") && resourcedir != NULL)
	return pmwebres_respond(cls, connection, url);

    /* junk? */
    MHD_add_response_header(resp, "Content-Type", "text/plain");
    resp = MHD_create_response_from_buffer(strlen(error_page),
					   (char*)error_page, 
					   MHD_RESPMEM_PERSISTENT);
    if (resp == NULL) {
	pmweb_notify(LOG_ERR, connection,
		     "MHD_create_response_from_callback failed\n");
	return MHD_NO;
    }

    sts = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, resp);
    MHD_destroy_response(resp);
    if (sts != MHD_YES) {
	pmweb_notify(LOG_ERR, connection, "MHD_queue_response failed\n");
	return MHD_NO;
    }
    return MHD_YES;
}

static void
handle_signals(int sig)
{
    (void)sig;
    exit_p++;
}

static void
pmweb_dont_start(void)
{
    FILE	*tty;
    FILE	*log;

    __pmNotifyErr(LOG_ERR, "pmwebd not started due to errors!\n");

    if ((tty = fopen(fatalfile, "w")) != NULL) {
	fflush(stderr);
	fprintf(tty, "NOTE: pmwebd not started due to errors!  ");
	if ((log = fopen(logfile, "r")) != NULL) {
	    int	 c;

	    fprintf(tty, "Log file \"%s\" contains ...\n", logfile);
	    while ((c = fgetc(log)) != EOF)
		fputc(c, tty);
	    fclose(log);
	}
	else
	    fprintf(tty, "Log file \"%s\" has vanished!\n", logfile);
	fclose(tty);
    }
    exit(1);
}

/*
 * Local variant of __pmServerDumpRequestPorts - remove this
 * when an NSS-based pmweb daemon is built.
 */
static void
server_dump_request_ports(FILE *f, int ipv4, int ipv6, int port)
{
    if (ipv4)
	fprintf(f, "Started daemon on IPv4 TCP port %d\n", port);
    if (ipv6)
	fprintf(f, "Started daemon on IPv6 TCP port %d\n", port);
}

static void
server_dump_configuration(FILE *f)
{
    char	*cwd;
    char	path[MAXPATHLEN];
    char	cwdpath[MAXPATHLEN];
    int		sep = __pmPathSeparator();
    int		len;

    cwd = getcwd(cwdpath, sizeof(cwdpath));
    if (resourcedir) {
	len = (__pmAbsolutePath(resourcedir) || !cwd) ?
	    snprintf(path, sizeof(path), "%s", resourcedir) :
	    snprintf(path, sizeof(path), "%s%c%s", cwd, sep, resourcedir);
	while (len-- > 1) {
	    if (path[len] != '.' && path[len] != sep) break;
	    path[len] = '\0';
	}
	fprintf (f, "Serving non-pmwebapi URLs under directory %s\n", path);
    }
    if (new_contexts_p) {
	len = (__pmAbsolutePath(archivesdir) || !cwd) ?
	    snprintf(path, sizeof(path), "%s", archivesdir) :
	    snprintf(path, sizeof(path), "%s%c%s", cwd, sep, archivesdir);
	while (len-- > 1) {
	    if (path[len] != '.' && path[len] != sep) break;
	    path[len] = '\0';
	}
	/* XXX: network outbound ACL */
	fprintf(f, "Serving PCP archives under directory %s\n", path);
    } else {
	fprintf(f, "Remote context creation requests disabled\n");
    }
}

static void
pmweb_init_random_seed(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    srandom((unsigned int) getpid() ^
	    (unsigned int) tv.tv_sec ^
	    (unsigned int) tv.tv_usec);
}

static void
pmweb_shutdown(struct MHD_Daemon *d4, struct MHD_Daemon *d6)
{
    /* Shut down cleanly, out of a misplaced sense of propriety. */
    if (d4)
	MHD_stop_daemon(d4);
    if (d6)
	MHD_stop_daemon(d6);

    /*
     * Let's politely clean up all the active contexts.
     * The OS will do all that for us anyway, but let's make valgrind happy.
     */
    pmwebapi_deallocate_all();

    __pmNotifyErr(LOG_INFO, "pmwebd Shutdown\n");
    fflush(stderr);
}

static int
option_overrides(int opt, pmOptions *opts)
{
    (void)opts;

    switch (opt) {
    case 'A': case 'a': case 'h': case 'L': case 'N': case 'p': case 't':
	return 1;
    }
    return 0;
}

static pmLongOptions longopts[] = {
    PMAPI_OPTIONS_HEADER("Network options"),
    { "port", 1, 'p', "NUM", "listen on given TCP port [default 44323]" },
    { "ipv4", 0, '4', 0, "listen on IPv4 only" },
    { "ipv6", 0, '6', 0, "listen on IPv6 only" },
    { "timeout", 1, 't', "SEC", "max time (seconds) for PMAPI polling [default 300]" },
    { "resources", 1, 'R', "DIR", "serve non-API files from given directory" },
    PMAPI_OPTIONS_HEADER("Context options"),
    { "context", 1, 'c', "NUM", "set next permanent-binding context number" },
    { "host", 1, 'h', "HOST", "permanent-bind next context to PMCD on host" },
    { "archive", 1, 'a', "FILE", "permanent-bind next context to archive" },
    { "local-PMDA", 0, 'L', 0, "permanent-bind next context to local PMDAs" },
    PMOPT_SPECLOCAL,
    PMOPT_LOCALPMDA,
    { "", 0, 'N', 0, "disable remote new-context requests" },
    { "", 1, 'A', "DIR", "permit remote new-archive-context under dir [default CWD]" },
    PMAPI_OPTIONS_HEADER("Other"),
    PMOPT_DEBUG,
    { "foreground", 0, 'f', 0, "run in the foreground" },
    { "log", 1, 'l', "FILE", "redirect diagnostics and trace output" },
    { "verbose", 0, 'v', 0, "increase verbosity" },
    { "username", 1, 'U', "USER", "assume identity of username [default pcp]" },
    { "", 1, 'x', "PATH", "fatal messages at startup sent to file [default /dev/tty]" },
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts = {
    .short_options = "A:a:c:D:fh:K:Ll:Np:R:t:U:vx:46?",
    .long_options = longopts,
    .override = option_overrides,
};

int
main(int argc, char *argv[])
{
    int		c;
    int		sts;
    int		ctx;
    int		mhd_ipv4 = 1;
    int		mhd_ipv6 = 1;
    int		run_daemon = 1;
    int		port = PMWEBD_PORT;
    char	*endptr;
    struct MHD_Daemon *d4 = NULL;
    struct MHD_Daemon *d6 = NULL;

    umask(022);
    __pmGetUsername(&username);

    while ((c = pmGetOptions(argc, argv, &opts)) != EOF) {
	switch (c) {
	case 'p':
	    port = (int)strtol(opts.optarg, &endptr, 0);
	    if (*endptr != '\0' || port < 0 || port > 65535) {
		pmprintf("%s: invalid port number %s\n",
			 pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 't':
	    maxtimeout = strtoul(opts.optarg, &endptr, 0);
	    if (*endptr != '\0') {
		pmprintf("%s: invalid timeout %s\n", pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'R':
	    resourcedir = opts.optarg;
	    break;

	case 'A':
	    archivesdir = opts.optarg;
	    break;

	case '6':
	    mhd_ipv6 = 1;
	    mhd_ipv4 = 0;
	    break;

	case '4':
	    mhd_ipv4 = 1;
	    mhd_ipv6 = 0;
	    break;

	case 'v':
	    verbosity++;
	    break;

	case 'c':
	    perm_context = strtoul(opts.optarg, &endptr, 0);
	    if (*endptr != '\0' || perm_context >= INT_MAX) {
		pmprintf("%s: invalid context number %s\n",
			pmProgname, opts.optarg);
		opts.errors++;
	    }
	    break;

	case 'h':
	    if ((ctx = pmNewContext(PM_CONTEXT_HOST, opts.optarg)) < 0) {
		__pmNotifyErr(LOG_ERR, "new context failed\n");
		exit(EXIT_FAILURE);
	    }
	    if ((sts = pmwebapi_bind_permanent(perm_context++, ctx)) < 0) {
		__pmNotifyErr(LOG_ERR, "permanent bind failed\n");
		exit(EXIT_FAILURE);
	    }
	    __pmNotifyErr(LOG_INFO,
			"context (web%d=pm%d) created, host %s, permanent\n", 
			perm_context - 1, ctx, opts.optarg);
	    break;

	case 'a':
	    if ((ctx = pmNewContext(PM_CONTEXT_ARCHIVE, opts.optarg)) < 0) {
		__pmNotifyErr(LOG_ERR, "new context failed\n");
		exit(EXIT_FAILURE);
	    }
	    if ((sts = pmwebapi_bind_permanent(perm_context++, ctx)) < 0) {
		__pmNotifyErr(LOG_ERR, "permanent bind failed\n");
		exit(EXIT_FAILURE);
	    }
	    __pmNotifyErr(LOG_INFO,
			"context (web%d=pm%d) created, archive %s, permanent\n",
			perm_context - 1, ctx, opts.optarg);
	    break;

	case 'L':
	    if ((ctx = pmNewContext(PM_CONTEXT_LOCAL, NULL)) < 0) {
		__pmNotifyErr(LOG_ERR, "new context failed\n");
		exit(EXIT_FAILURE);
	    }
	    if ((sts = pmwebapi_bind_permanent(perm_context++, ctx)) < 0) {
		__pmNotifyErr(LOG_ERR, "permanent bind failed\n");
		exit(EXIT_FAILURE);
	    }
	    __pmNotifyErr(LOG_INFO,
			"context (web%d=pm%d) created, local, permanent\n", 
			perm_context - 1, ctx);
	    break;

	case 'N':
	    new_contexts_p = 0;
	    break;

	case 'f':
	    /* foreground, i.e. do _not_ run as a daemon */
	    run_daemon = 0;
	    break;

	case 'l':
	    /* log file name */
	    logfile = opts.optarg;
	    break;

	case 'U':
	    /* run as user username */
	    username = opts.optarg;
	    break;

	case 'x':
	    fatalfile = opts.optarg;
	    break;
	}
    }

    if (opts.errors) {
	pmUsageMessage(&opts);
	exit(EXIT_FAILURE);
    }

    if (run_daemon) {
	fflush(stderr);
	pmweb_start_daemon(argc, argv);
    }

    /*
     * Start microhttp daemon.  Use the application-driven threading
     * model, so we don't complicate things with threads.  In the
     * future, if this daemon becomes safe to run multithreaded,
     * we could make use of MHD_USE_THREAD_PER_CONNECTION; we'd need
     * to add ample locking over pmwebd context structures etc.
     */
    if (mhd_ipv4)
	d4 = MHD_start_daemon(0,
			      port,
			      NULL, NULL, /* default accept policy */
			      &mhd_respond, NULL, /* handler callback */
			      MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout,
			      MHD_OPTION_END);
    if (mhd_ipv6)
	d6 = MHD_start_daemon(MHD_USE_IPv6,
			      port,
			      NULL, NULL, /* default accept policy */
			      &mhd_respond, NULL, /* handler callback */
			      MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout,
			      MHD_OPTION_END);
    if (d4 == NULL && d6 == NULL) {
	__pmNotifyErr(LOG_ERR, "error starting microhttpd daemons\n");
	pmweb_dont_start();
    }

    __pmOpenLog(pmProgname, logfile, stderr, &sts);
    /* close old stdout, and force stdout into same stream as stderr */
    fflush(stdout);
    close(fileno(stdout));
    if (dup(fileno(stderr)) == -1)
	fprintf(stderr, "Warning: dup() failed: %s\n", pmErrStr(-oserror()));
    fprintf(stderr, "%s: PID = %" FMT_PID ", PMAPI URL = %s\n",
	    pmProgname, getpid(), uriprefix);
    server_dump_request_ports(stderr, d4 != NULL, d6 != NULL, port);
    server_dump_configuration(stderr);
    fflush(stderr);

    /* Set up signal handlers. */
    __pmSetSignalHandler(SIGHUP, SIG_IGN);
    __pmSetSignalHandler(SIGINT, handle_signals);
    __pmSetSignalHandler(SIGTERM, handle_signals);
    __pmSetSignalHandler(SIGQUIT, handle_signals);
    /* Not this one; might get it from pmcd momentary disconnection. */
    /* __pmSetSignalHandler(SIGPIPE, handle_signals); */

    /* lose root privileges if we have them */
    __pmSetProcessIdentity(username);

    /* Setup randomness for calls to random() */
    pmweb_init_random_seed();

    /* Block indefinitely. */
    while (! exit_p) {
	struct timeval tv;
	fd_set rs;
	fd_set ws;
	fd_set es;
	int max = 0;

	/* Based upon MHD fileserver_example_external_select.c */
	FD_ZERO(&rs);
	FD_ZERO(&ws);
	FD_ZERO(&es);
	if (d4 && MHD_YES != MHD_get_fdset(d4, &rs, &ws, &es, &max))
	    break; /* fatal internal error */
	if (d6 && MHD_YES != MHD_get_fdset(d6, &rs, &ws, &es, &max))
	    break; /* fatal internal error */

	/*
	 * Find the next expiry.  We don't need to bound it by
	 * MHD_get_timeout, since we don't use a small
	 * MHD_OPTION_CONNECTION_TIMEOUT.
	 */
	tv.tv_sec = pmwebapi_gc();
	tv.tv_usec = 0;

	select(max+1, &rs, &ws, &es, &tv);

	if (d4)
	    MHD_run(d4);
	if (d6)
	    MHD_run(d6);
    }

    pmweb_shutdown(d4, d6);
    return 0;
}

/*
 * Generate a __pmNotifyErr with the given arguments,
 * but also adding some per-connection metadata info.
 */
void
pmweb_notify(int priority, struct MHD_Connection *conn, const char *fmt, ...)
{
    struct sockaddr *so;
    va_list	arg;
    char	message_buf[2048]; /* size similar to __pmNotifyErr */
    char	*message_tail;
    size_t	message_len;
    char	hostname[128];
    char	servname[128];
    int		sts = -1;

    /* Look up client address data. */
    so = (struct sockaddr *) MHD_get_connection_info(conn,
		MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;

    if (so && so->sa_family == AF_INET)
	sts = getnameinfo(so, sizeof(struct sockaddr_in),
			  hostname, sizeof(hostname),
			  servname, sizeof(servname),
			  NI_NUMERICHOST|NI_NUMERICSERV);
    else if (so && so->sa_family == AF_INET6)
	sts = getnameinfo(so, sizeof(struct sockaddr_in6),
			  hostname, sizeof(hostname),
			  servname, sizeof(servname),
			  NI_NUMERICHOST|NI_NUMERICSERV);
    if (sts != 0)
	hostname[0] = servname[0] = '\0';

    /* Add the [hostname:port] as a prefix */
    sts = snprintf(message_buf, sizeof(message_buf), "[%s:%s] ",
		   hostname, servname);

    if (sts > 0 && sts < (int)sizeof(message_buf)) {
	message_tail = message_buf + sts; /* Keep it only if successful. */
	message_len = sizeof(message_buf) - sts;
    } else {
	message_tail = message_buf;
	message_len = sizeof(message_buf);
    }

    /* Add the remaining incoming text. */
    va_start(arg, fmt);
    sts = vsnprintf(message_tail, message_len, fmt, arg);
    va_end(arg);

    /*
     * Delegate, but avoid format-string vulnerabilities.  Drop the
     * trailing \n, if there is one, since __pmNotifyErr will add one
     * for us (since it is missing from the %s format string).
     */
    if (sts >= 0 && sts < (int)message_len)
	if (message_tail[sts-1] == '\n')
	    message_tail[sts-1] = '\0';
    __pmNotifyErr(priority, "%s", message_buf);
}
