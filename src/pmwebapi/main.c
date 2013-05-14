/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2013 Red Hat Inc.
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

/* ------------------------------------------------------------------------ */

const char uriprefix[] = "/pmapi";
char *resourcedir;             /* set by -R option */
char *archivesdir = ".";       /* set by -A option */
unsigned verbosity;            /* set by -v option */
unsigned maxtimeout = 300;     /* set by -t option */
unsigned perm_context = 1;     /* set by -c option, changed by -h/-a/-L */
unsigned new_contexts_p = 1;   /* set by -N option */
unsigned exit_p;               /* counted by SIG* handler */
static char *logfile = "pmwebd.log";
static char *fatalfile = "/dev/tty"; /* fatal messages at startup go here */
static char *username;


static int mhd_log_args (void *connection, enum MHD_ValueKind kind, 
                         const char *key, const char *value)
{
    (void) kind;
    pmweb_notify (LOG_DEBUG, connection,
                  "%s%s%s", key, (value ? "=" : ""), (value ? value : ""));
    return MHD_YES;
}



/* Respond to a new incoming HTTP request.  It may be
   one of three general categories:
   (a) creation of a new PMAPI context: do it
   (b) operation on an existing context: do it
   (c) access to some non-API URI: serve it from $resourcedir/ if configured.
*/
static int mhd_respond (void *cls, struct MHD_Connection *connection,
                 const char *url,
                 const char *method, const char *version,
                 const char *upload_data,
                 size_t *upload_data_size, void **con_cls)
{
    static int dummy;
    struct MHD_Response* resp = NULL;
    int rc;
    static const char error_page[] =
        "PMWEBD error"; /* could also be an actual error page... */

    /* MHD calls us at least twice per request.  Skip the first one,
       since it only gives us headers, and not any POST content. */
    if (& dummy != *con_cls) {
        *con_cls = &dummy;
        return MHD_YES;
    }
    *con_cls = NULL;

    if (verbosity > 1)
        pmweb_notify (LOG_INFO, connection, "%s %s %s", version, method, url);
    if (verbosity > 2) /* Print arguments too. */
        (void) MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND,
                                          &mhd_log_args, connection);

    /* pmwebapi? */
    if (0 == strncmp(url, uriprefix, strlen(uriprefix)))
        return pmwebapi_respond (cls, connection,
                                 & url[strlen(uriprefix)+1], /* strip prefix */
                                 method, upload_data, upload_data_size);

    /* pmresapi? */
    else if (0 == strcmp(method, "GET") && resourcedir != NULL)
        return pmwebres_respond (cls, connection, url);

    /* junk? */
    (void) MHD_add_response_header (resp, "Content-Type", "text/plain");
    resp = MHD_create_response_from_buffer (strlen(error_page),
                                            (char*)error_page, 
                                            MHD_RESPMEM_PERSISTENT);
    if (resp == NULL) {
        pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_callback failed\n");
        return MHD_NO;
    }

    rc = MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, resp);
    MHD_destroy_response (resp);
    if (rc != MHD_YES) {
        pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
        return MHD_NO;
    }

    return MHD_YES;
}


/* ------------------------------------------------------------------------ */


/* NB: see also ../../man/man1/pmwebd.1 */
#define OPTIONS "fl:p:46K:R:t:c:h:a:LA:NU:vx:?"
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
static char usage[] = 
    "Usage: %s [options]\n\n"
    "Network options:\n"
    "  -p N          listen on given TCP port, default " STRINGIFY(PMWEBD_PORT) "\n"
    "  -4            listen on IPv4 only\n"
    "  -6            listen on IPv6 only\n"
    "  -t timeout    max time (seconds) for pmapi polling, default 300\n"
    "  -R resdir     serve non-API files from given directory, no default\n"
    "Context options:\n"
    "  -c number     set next permanent-binding context number\n"
    "  -h hostname   permanent-bind next context to PMCD on host\n"
    "  -a archive    permanent-bind next context to archive\n"
    "  -L            permanent-bind next context to local PMDAs\n"
    "  -N            disable remote new-context requests\n"
    "  -K spec       optional additional PMDA spec for local connection\n"
    "  -A archdir    permit remote new-archive-context under archdir, CWD default\n"
    "Other options:\n"
    "  -f            run in the foreground\n"
    "  -l logfile    redirect diagnostics and trace output\n"
    "  -v            increase verbosity\n"
    "  -U username   assume identity of username (only when run as root)\n"
    "  -x file       fatal messages at startup sent to file [default /dev/tty]\n"
    "  -?            help\n";


static void handle_signals (int sig)
{
    (void) sig;
    exit_p ++;
}


static void
pmweb_dont_start(void)
{
    FILE        *tty;
    FILE        *log;

    __pmNotifyErr(LOG_ERR, "pmwebd not started due to errors!\n");

    if ((tty = fopen(fatalfile, "w")) != NULL) {
        fflush(stderr);
        fprintf(tty, "NOTE: pmwebd not started due to errors!  ");
        if ((log = fopen(logfile, "r")) != NULL) {
            int         c;
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
        fprintf (f, "Started daemon on IPv4 TCP port %d\n", port);
    if (ipv6)
        fprintf (f, "Started daemon on IPv6 TCP port %d\n", port);
}


static void
server_dump_configuration(FILE *f)
{
    char *cwd;
    char path[MAXPATHLEN];
    char cwdpath[MAXPATHLEN];
    int sep = __pmPathSeparator();
    int len;

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
        fprintf (f, "Serving PCP archives under directory %s\n", path);
    } else {
        fprintf (f, "Remote context creation requests disabled\n");
    }
}


static void
pmweb_init_random_seed(void)
{
    struct timeval tv;

    gettimeofday (&tv, NULL);
    srandom ((unsigned int) getpid() ^
             (unsigned int) tv.tv_sec ^
             (unsigned int) tv.tv_usec);
}


static void
pmweb_shutdown (struct MHD_Daemon *d4, struct MHD_Daemon *d6)
{
    /* Shut down cleanly, out of a misplaced sense of propriety. */
    if (d4) MHD_stop_daemon (d4);
    if (d6) MHD_stop_daemon (d6);

    /* Let's politely clean up all the active contexts. */
    /* The OS could do all that for us anyway, but let's make valgrind happy. */
    pmwebapi_deallocate_all ();

    __pmNotifyErr (LOG_INFO, "pmwebd Shutdown\n");
    fflush(stderr);
}


int main(int argc, char *argv[])
{
    struct MHD_Daemon *d4 = NULL;
    int mhd_ipv4 = 1;
    struct MHD_Daemon *d6 = NULL;
    int mhd_ipv6 = 1;
    int c;
    int sts;
    int run_daemon = 1;
    char *errmsg = NULL;
    unsigned errflag = 0;
    unsigned short port = PMWEBD_PORT;

    umask(022);
    __pmSetProgname(argv[0]);
    __pmGetUsername(&username);

    while ((c = getopt(argc, argv, "D:" OPTIONS)) != EOF)
        switch (c) {
        case 'p':
            {
                long pn;
                char *endptr;
                errno = 0;
                pn = strtol(optarg, &endptr, 0);
                if (errno != 0 || *endptr != '\0' || pn < 0 || pn > 65535) {
                    fprintf(stderr, "%s: invalid -p port number %s\n", pmProgname, optarg);
                    errflag ++;
                }
                else port = (unsigned short) pn;
            }
            break;

        case 't':
            {
                long tn;
                char *endptr;
                errno = 0;
                tn = strtol(optarg, &endptr, 0);
                /* NB: strtoul would accept negative values. */
                if (errno != 0 || *endptr != '\0' || tn < 0 || tn > (long)UINT_MAX) {
                    fprintf(stderr, "%s: invalid -t timeout %s\n", pmProgname, optarg);
                    errflag ++;
                }
                else maxtimeout = (unsigned) tn;
            }
            break;

        case 'K':
            if ((errmsg = __pmSpecLocalPMDA(optarg)) != NULL) {
                fprintf(stderr, "%s: __pmSpecLocalPMDA failed\n%s\n", pmProgname, errmsg);
                errflag++;
            }
            break;

        case 'R':
            resourcedir = optarg;
            break;

        case 'A':
            archivesdir = optarg;
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
            verbosity ++;
            break;

        case 'c':
            assert (optarg);
            {
                long pc;
                char *endptr;
                errno = 0;
                pc = strtol(optarg, &endptr, 0);
                if (errno != 0 || *endptr != '\0' || pc <= 0 || pc >= INT_MAX) {
                    fprintf(stderr, "%s: invalid -c context number %s\n", pmProgname, optarg);
                    errflag ++;
                }
                else perm_context = (unsigned) pc;
            }
            break;

        case 'h':
            assert (optarg);
            {
                int pcp_context = pmNewContext (PM_CONTEXT_HOST, optarg);
                int rc;

                if (pcp_context < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }

                rc = pmwebapi_bind_permanent (perm_context++, pcp_context);
                if (rc < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }
                
                __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, host %s, permanent\n", 
                               perm_context-1, pcp_context, optarg);
            }
            break;

        case 'a':
            assert (optarg);
            {
                int pcp_context = pmNewContext (PM_CONTEXT_ARCHIVE, optarg);
                int rc;

                if (pcp_context < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }

                rc = pmwebapi_bind_permanent (perm_context++, pcp_context);
                if (rc < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }

                __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, archive %s, permanent\n", 
                               perm_context-1, pcp_context, optarg);
            }
            break;

        case 'L':
            {
                int pcp_context = pmNewContext (PM_CONTEXT_LOCAL, NULL);
                int rc;

                if (pcp_context < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }

                rc = pmwebapi_bind_permanent (perm_context++, pcp_context);
                if (rc < 0) {
                    __pmNotifyErr (LOG_ERR, "new context failed\n");
                    exit(EXIT_FAILURE);
                }

                __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, local, permanent\n", 
                               perm_context-1, pcp_context);
            }
            break;

        case 'N':
            new_contexts_p = 0;
            break;

        case 'D':       /* debug flag */
            sts = __pmParseDebug(optarg);
            if (sts < 0) {
                fprintf(stderr, "%s: unrecognized debug flag specification (%s)\n",
                        pmProgname, optarg);
                errflag++;
            }
            pmDebug |= sts;
            break;

        case 'f':
            /* foreground, i.e. do _not_ run as a daemon */
            run_daemon = 0;
            break;

        case 'l':
            /* log file name */
            logfile = optarg;
            break;

        case 'U':
            /* run as user username */
            username = optarg;
            break;

        case 'x':
            fatalfile = optarg;
            break;

        default:
        case '?':
            fprintf(stderr, usage, pmProgname);
            exit(EXIT_FAILURE);
        }

    if (errflag)
        exit(EXIT_FAILURE);

    if (run_daemon) {
        fflush(stderr);
        pmweb_start_daemon(argc, argv);
    }

    /* Start microhttp daemon.  Use the application-driven threading
       model, so we don't complicate things with threads.  In the
       future, if this daemon becomes safe to run multithreaded (now
       that libpcp is), consider MHD_USE_THREAD_PER_CONNECTION; need
       to add ample locking over pmwebd context structures etc. */
    if (mhd_ipv4)
        d4 = MHD_start_daemon(0,
                              port,
                              NULL, NULL,              /* default accept policy */
                              &mhd_respond, NULL,      /* handler callback */
                              MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout,
                              MHD_OPTION_END);
    if (mhd_ipv6)
        d6 = MHD_start_daemon(MHD_USE_IPv6,
                              port,
                              NULL, NULL,              /* default accept policy */
                              &mhd_respond, NULL,      /* handler callback */
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
    if (dup(fileno(stderr)) == -1) {
        fprintf(stderr, "Warning: dup() failed: %s\n", pmErrStr(-oserror()));
    }
    fprintf(stderr, "pmwebd: PID = %" FMT_PID ", PMAPI URL = %s\n", getpid(), uriprefix);
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
        FD_ZERO (& rs);
        FD_ZERO (& ws);
        FD_ZERO (& es);
        if (d4 && MHD_YES != MHD_get_fdset (d4, &rs, &ws, &es, &max))
            break; /* fatal internal error */
        if (d6 && MHD_YES != MHD_get_fdset (d6, &rs, &ws, &es, &max))
            break; /* fatal internal error */

        /* Find the next expiry.  We don't need to bound it by
           MHD_get_timeout, since we don't use a small
           MHD_OPTION_CONNECTION_TIMEOUT. */
        tv.tv_sec = pmwebapi_gc ();
        tv.tv_usec = 0;

        (void) select (max+1, &rs, &ws, &es, &tv);

        if (d4) MHD_run (d4);
        if (d6) MHD_run (d6);
    }

    pmweb_shutdown (d4, d6);
    return 0;
}


/* Generate a __pmNotifyErr with the given arguments, but also adding some per-connection
   metadata info. */
void
pmweb_notify (int priority, struct MHD_Connection* connection, const char *fmt, ...)
{
    va_list arg;
    char message_buf [2048]; /* size similar to that used in __pmNotifyErr itself. */
    char *message_tail;
    size_t message_len;
    struct sockaddr *so;
    char hostname[128];
    char servname[128];
    int rc = -1;

    /* Look up client address data. */
    so = (struct sockaddr *)
        MHD_get_connection_info (connection,MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
    if (so && so->sa_family == AF_INET)
        rc = getnameinfo (so, sizeof(struct sockaddr_in),
                          hostname, sizeof(hostname),
                          servname, sizeof(servname),
                          NI_NUMERICHOST|NI_NUMERICSERV);
    else if (so && so->sa_family == AF_INET6)
        rc = getnameinfo (so, sizeof(struct sockaddr_in6),
                          hostname, sizeof(hostname),
                          servname, sizeof(servname),
                          NI_NUMERICHOST|NI_NUMERICSERV);
    if (rc != 0)
        hostname[0] = servname[0] = '\0';

    /* Add the [hostname:port] as a prefix */
    rc = snprintf (message_buf, sizeof(message_buf), "[%s:%s] ", hostname, servname);
    if (rc > 0 && rc < (int)sizeof(message_buf))
        {
            message_tail = message_buf + rc; /* Keep it only if successful. */
            message_len = sizeof(message_buf)-rc;
        }
    else
        {
            message_tail = message_buf;
            message_len = sizeof(message_buf);
        }

    /* Add the remaining incoming text. */
    va_start (arg, fmt);
    rc = vsnprintf (message_tail, message_len, fmt, arg);
    va_end (arg);
    (void) rc; /* If this fails, we can't do much really. */

    /* Delegate, but avoid format-string vulnerabilities.  Drop the
       trailing \n, if there is one, since __pmNotifyErr will add one for
       us (since it is missing from the %s format string). */
    if (rc >= 0 && rc < (int)message_len)
        if (message_tail[rc-1] == '\n')
            message_tail[rc-1] = '\0';
    __pmNotifyErr (priority, "%s", message_buf);
}
