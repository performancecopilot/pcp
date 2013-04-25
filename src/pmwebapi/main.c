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
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include "pmwebapi.h"

/* ------------------------------------------------------------------------ */

const char uriprefix[] = "/pmapi";
char *resourcedir = NULL;      /* set by -r option */
unsigned verbosity = 0;        /* set by -v option */
unsigned maxtimeout = 300;     /* set by -t option */
unsigned perm_context = 1;     /* set by -C option, changed by -h/-a/-L */
unsigned new_contexts_p = 1;   /* set by -R option */
unsigned exit_p;               /* counted by SIG* handler */



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

    /* Determine whether request is a pmapi or a resource call. */
    if (0 == strncmp(url, uriprefix, strlen(uriprefix)))
        return pmwebapi_respond (cls, connection,
                                 & url[strlen(uriprefix)+1], /* strip prefix */
                                 method, upload_data, upload_data_size);

    else if (0 == strcmp(method, "GET") && resourcedir != NULL)
        return pmwebres_respond (cls, connection, url);

    return MHD_NO;
}


/* ------------------------------------------------------------------------ */

/* NB: see also ../../man/man1/pmwebd.1 */
static char *options = "p:46K:r:t:C:h:a:LRv?";
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
static char usage[] = 
    "Usage: %s [options]\n\n"
    "Options:\n"
    "  -p N          listen on given TCP port, default " STRINGIFY(PMWEBD_PORT) "\n"
    "  -4            listen on IPv4 only\n"
    "  -6            listen on IPv6 only\n"
    "  -K spec       optional additional PMDA spec for local connection\n"
    "                spec is of the form op,domain,dso-path,init-routine\n"
    "  -r resdir     serve non-API files from given directory, no default\n"
    "  -t timeout    max time (seconds) for pmapi polling, default 300\n"
    "  -C number     set next permanent-binding context identifier\n"
    "  -h hostname   permanently bind next context to metrics on PMCD on host\n"
    "  -a archive    permanently bind next context to metrics in archive\n"
    "  -L            permanently bind next context to metrics in local PMDAs\n"
    "  -R            disable remote new-context requests\n"
    "  -v            increase verbosity\n"
    "  -?            help\n";



static void handle_signals (int sig)
{
    (void) sig;
    exit_p ++;
}



/* Main loop of pmwebapi server. */
int main(int argc, char *argv[])
{
    struct MHD_Daemon *d4 = NULL;
    int mhd_ipv4 = 1;
    struct MHD_Daemon *d6 = NULL;
    int mhd_ipv6 = 1;
    unsigned short port = PMWEBD_PORT;
    int c;
    char *errmsg = NULL;
    unsigned errflag = 0;

    __pmSetProgname(argv[0]);

    /* Parse options */

    while ((c = getopt(argc, argv, options)) != EOF)
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
                if (errno != 0 || *endptr != '\0' || tn < 0 || tn > UINT_MAX) {
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

        case 'r':
            resourcedir = optarg;
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

        case 'C':
            assert (optarg);
            {
                long pc;
                char *endptr;
                errno = 0;
                pc = strtol(optarg, &endptr, 0);
                if (errno != 0 || *endptr != '\0' || pc <= 0 || pc >= INT_MAX) {
                    fprintf(stderr, "%s: invalid -C context number %s\n", pmProgname, optarg);
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
                
                if(verbosity)
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

                if(verbosity)
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

                if(verbosity)
                    __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, local, permanent\n", 
                                   perm_context-1, pcp_context);
            }
            break;

        case 'R':
            new_contexts_p = 0;
            break;

        default:
        case '?':
            fprintf(stderr, usage, pmProgname);
            exit(EXIT_FAILURE);
        }

    if (errflag)
        exit(EXIT_FAILURE);

    /* Start microhttpd daemon.  Use the application-driven threading
       model, so we don't complicate things with threads.  In the
       future, if PMAPI becomes safe to invoke from a multithreaded
       application, consider MHD_USE_THREAD_PER_CONNECTION, with
       ample locking over our context structures etc. */
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
        exit(EXIT_FAILURE);
    }
    
    if (d4)
        __pmNotifyErr (LOG_INFO, "Started daemon on IPv4 tcp port %d, pmapi url %s\n",
                       port, uriprefix);
    if (d6)
        __pmNotifyErr (LOG_INFO, "Started daemon on IPv6 tcp port %d, pmapi url %s\n",
                       port, uriprefix);

    if (resourcedir)
        __pmNotifyErr (LOG_INFO, "Serving other urls under directory %s", resourcedir);

    /* Set up signal handlers. */
    signal (SIGINT, handle_signals);
    signal (SIGHUP, handle_signals);
    signal (SIGPIPE, handle_signals);
    signal (SIGTERM, handle_signals);
    signal (SIGQUIT, handle_signals);

    {
        struct timeval tv;
        gettimeofday (&tv, NULL);
        srandom ((unsigned int) getpid() ^
                 (unsigned int) tv.tv_sec ^
                 (unsigned int) tv.tv_usec);
    }

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

    __pmNotifyErr (LOG_INFO, "Stopping\n");

    /* Shut down cleanly, out of a misplaced sense of propriety. */
    if (d4) MHD_stop_daemon (d4);
    if (d6) MHD_stop_daemon (d6);

    /* Let's politely clean up all the active contexts. */
    /* The OS could do all that for us anyway, but let's make valgrind happy. */
    pmwebapi_deallocate_all ();

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
