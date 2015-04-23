/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2015 Red Hat.
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

#define _XOPEN_SOURCE 600
#include "pmwebapi.h"

#include <string>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

string uriprefix = "pmapi";
string resourcedir;		/* set by -R option */
string archivesdir = ".";	/* set by -A option */
unsigned verbosity;		/* set by -v option */
unsigned maxtimeout = 300;	/* set by -t option */
int dumpstats = 300;            /* set by -d option */
map<string,unsigned> clients_usage;
unsigned perm_context = 1;	/* set by -c option, changed by -h/-a/-L */
unsigned new_contexts_p = 1;	/* cleared by -N option */
unsigned graphite_p;		/* set by -G option */
unsigned exit_p;		/* counted by SIG* handler */
static __pmServerPresence *presence;
unsigned multithread = 0;       /* set by -M option */
unsigned graphite_timestep = 60;  /* set by -i option */
unsigned graphite_archivedir = 0; /* set by -I option */
string logfile = "";		/* set by -l option */
string fatalfile = "/dev/tty";	/* fatal messages at startup go here */



/* Print a best-effort message as a plain-text http response; anything
   to avoid a dreaded MHD_NO, which results in a 500 return code.
   That in turn could be interpreted by a web client as an invitation
   to try, try again. */
int
mhd_notify_error (struct MHD_Connection *connection, int rc)
{
    char error_message[1000];
    char pmmsg[PM_MAXERRMSGLEN];
    struct MHD_Response *resp;

    (void) pmErrStr_r (rc, pmmsg, sizeof (pmmsg));
    (void) snprintf (error_message, sizeof (error_message), "PMWEBD error, code %d: %s", rc,
                     pmmsg);
    resp = MHD_create_response_from_buffer (strlen (error_message), error_message,
                                            MHD_RESPMEM_MUST_COPY);
    if (resp == NULL) {
        connstamp (cerr, connection) << "MHD_create_response_from_buffer failed" << endl;
        return MHD_NO;
    }

    (void) MHD_add_response_header (resp, "Content-Type", "text/plain");

    // ACAO here is desirable so that a browser can permit a webapp to
    // process the detailed error message.
    (void) MHD_add_response_header (resp, "Access-Control-Allow-Origin", "*");

    rc = MHD_queue_response (connection, MHD_HTTP_BAD_REQUEST, resp);
    MHD_destroy_response (resp);

    if (rc != MHD_YES) {
        connstamp (cerr, connection) << "MHD_queue_response failed" << endl;
        return MHD_NO;
    }

    return MHD_YES;
}



/* A POST- and GET- processor pair for MHD, allowing GET and POST parameters to be
   interchangeably used within a common http_params output structure. */
static int
mhd_post_iterator (void *cls, enum MHD_ValueKind kind, const char *key,
                   const char *filename, const char *content_type, const char *transfer_encoding,
                   const char *data, uint64_t off, size_t size)
{
    (void) kind;
    (void) filename;
    (void) content_type;
    (void) transfer_encoding;
    http_params *c = (http_params *) cls;
    assert (c);
    c->insert (make_pair (string (key), string (&data[off], size)));
    return MHD_YES;
}

static int
mhd_get_iterator (void *cls, enum MHD_ValueKind kind, const char *key, const char *value)
{
    (void) kind;
    http_params *c = (http_params *) cls;
    assert (c);
    c->insert (make_pair (string (key), string (value ? value : "")));
    return MHD_YES;
}



// An instance of this is associated with a mhd request-in-progress.
// We use this because mhd calls back into mhd_respond etc. several times
// during the processing of an http request, and we need to save state
// between them.

struct mhd_connection_context {
    struct MHD_PostProcessor *pp;
    http_params params;

    mhd_connection_context ():pp (0) {
    }
};


// A std::map<>-style operator[] for http_params; we just the value of
// an arbitrary named parameter.  Return "" if not found.
string http_params::operator [] (const string & s)
const
{
    http_params::const_iterator
    x = this->find (s);
    if (x == this->end ()) {
        return string ("");
    } else {
        return (x->second);
    }
}


vector <string> http_params::find_all (const string & s) const
{
    vector <string> all;
    pair <http_params::const_iterator, http_params::const_iterator> them = this->equal_range (s);
    for (http_params::const_iterator x = them.first; x != them.second; x++) {
        all.push_back (x->second);
    }

    return all;
}


/*
 * Respond to a new incoming HTTP request.  It may be
 * one of three general categories:
 * (a) creation of a new PMAPI context: do it
 * (b) operation on an existing context: do it
 * (c) access to some non-API URI: serve it from $resourcedir/ if configured.
 */
static int
mhd_respond (void *cls, struct MHD_Connection *connection, const char *url0,
             const char *method0, const char *version, const char *upload_data,
             size_t * upload_data_size, void **con_cls)
{
    try {
        (void) cls;			// closure parameter unused

        string url = url0;
        string method = method0 ? string (method0) : "";

        // First call?  Create a context.
        if (*con_cls == NULL) {
            // create a context
            mhd_connection_context *
            mhd_cc = new mhd_connection_context ();
            *con_cls = (void *) mhd_cc;	// deleted later
            if (method == "POST") {
                mhd_cc->pp = MHD_create_post_processor (connection, 1024, &mhd_post_iterator,
                                                        (void *) &mhd_cc->params);
                if (mhd_cc->pp == 0) {
                    connstamp (cerr, connection) << "error setting up POST processor" << endl;
                    return MHD_NO;	// internal error
                }
            }
            return MHD_YES;		// expect another call shortly
        }

        // Collect simple utilization info if desired
        if (dumpstats > 0) {
            clients_usage[conninfo (connection, false)] ++;
        }

        // Get our context
        mhd_connection_context * mhd_cc = (mhd_connection_context *) (*con_cls);
        assert (mhd_cc);

        // Intermediate call?  Store away POST parameters, if any.
        if (method == "POST") {
            MHD_post_process (mhd_cc->pp, upload_data, *upload_data_size);
            if (*upload_data_size != 0) {
                // intermediate call
                *upload_data_size = 0;
                return MHD_YES;
            }

            // final call, fall through
            assert (mhd_cc->pp);
            MHD_destroy_post_processor (mhd_cc->pp);
            mhd_cc->pp = 0;
        }

        // extract GET arguments
        (void) MHD_get_connection_values (connection, MHD_GET_ARGUMENT_KIND, &mhd_get_iterator,
                                          (void *) &mhd_cc->params);

        // Trace request
        if (verbosity) {
            stringstream str;
            str << version << " " << method << " " << url;
            if (verbosity > 1) {
                for (http_params::iterator it = mhd_cc->params.begin (); it != mhd_cc->params.end ();
                        it++) {
                    str << " " << it->first << "=" << it->second;
                }
            }
            connstamp (clog, connection) << str.str () << endl;
        }

        // first component (or the whole remainder)
        vector <string> url_tokens = split (url, '/');
        string url1 = (url_tokens.size () >= 2) ? url_tokens[1] : "";
        string url2 = (url_tokens.size () >= 3) ? url_tokens[2] : "";
        string url3 = (url_tokens.size () >= 4) ? url_tokens[3] : "";

        /* pmwebapi? */
        if (url1 == uriprefix) {
            return pmwebapi_respond (connection, mhd_cc->params, url_tokens);
        }

        /* graphite? */
        else if (graphite_p && (method == "GET" || method == "POST") && (url1 == "graphite")
                 && ((url2 == "render") || (url2 == "metrics") || (url2 == "rawdata")
                     || (url2 == "browser") || (url2 == "graphlot" && url3 == "findmetric"))) {
            return pmgraphite_respond (connection, mhd_cc->params, url_tokens);
        }
        // graphite dashboard idiosyncracy; note absence of /graphite top level
        else if (graphite_p && (method == "GET" || method == "POST") && 
                 ((url1 == "metrics" && url2 == "find") ||
                  (url1 == "render"))) {
            url_tokens.insert (url_tokens.begin() + 1 /* empty #0 */,
                               string("graphite"));
            return pmgraphite_respond (connection, mhd_cc->params, url_tokens);
        }

        /* pmresapi? */
        else if ((resourcedir != "") && (method == "GET")) {
            return pmwebres_respond (connection, mhd_cc->params, url);
        }

        /* fall through */
        return mhd_notify_error (connection, -EINVAL);

    } catch (...) {
        connstamp (cerr, connection) << "c++ exception caught" << endl;
        return MHD_NO;
    }
}

static void
handle_signals (int sig)
{
    (void) sig;
    exit_p++;
    // NB: invoke no async-signal-unsafe functions!
}


static void
mhd_respond_completed (void *cls, struct MHD_Connection *connection, void **con_cls,
                       enum MHD_RequestTerminationCode toe)
{
    (void) cls;
    (void) connection;
    (void) toe;

    mhd_connection_context *
    mhd_cc = (mhd_connection_context *) * con_cls;

    if (mhd_cc == 0) {
        return;
    }

    if (mhd_cc->pp != 0) {
        MHD_destroy_post_processor (mhd_cc->pp);
    }

    delete mhd_cc;
    * con_cls = 0; // Don't re-delete if we're called again
}




static void
pmweb_dont_start (void)
{
    timestamp (cerr) << "pmwebd not started due to errors!" << endl;

    ofstream tty (fatalfile.c_str ());
    if (tty.good ()) {
        timestamp (tty) << "NOTE: pmwebd not started due to errors!" << endl;

        // copy logfile to tty, if it was specified
        if (logfile != "") {
            tty << "Log file \"" << logfile << "\" contains ..." << endl;
            ifstream log (logfile.c_str ());
            if (log.good ()) {
                tty << log.rdbuf ();
            } else {
                tty << "Log file \"" << logfile << "\" has vanished ..." << endl;
            }
        }
    }
    exit (1);
}

/*
 * Local variant of __pmServerDumpRequestPorts - remove this
 * when an NSS-based pmweb daemon is built.
 */
static void
server_dump_request_ports (int ipv4, int ipv6, int port)
{
    if (ipv4) {
        clog << "\tStarted daemon on IPv4 TCP port " << port << endl;
    }
    if (ipv6) {
        clog << "\tStarted daemon on IPv6 TCP port " << port << endl;
    }
}

static void
server_dump_configuration ()
{
    char *cwd;
    char cwdpath[MAXPATHLEN];
    char sep = __pmPathSeparator ();

    // Assume timestamp() already just called, so we
    // don't have to repeat.

    clog << "\tVerbosity level " << verbosity << endl;
    clog << "\tUsing libmicrohttpd " << MHD_get_version () << endl;

    cwd = getcwd (cwdpath, sizeof (cwdpath));

    clog << "\tPMAPI prefix /" << uriprefix << endl;
    if (resourcedir != "") {
        clog << "\tServing non-pmwebapi URLs under directory ";
        // (NB: __pmAbsolutePath() should take const args)
        if (__pmAbsolutePath ((char *) resourcedir.c_str ()) || !cwd) {
            clog << resourcedir << endl;
        } else {
            clog << cwd << sep << resourcedir << endl;
        }
    }

    if (new_contexts_p) {
        clog << "\tRemote context creation requests enabled" << endl;
        clog << "\tArchive base directory: " << archivesdir << endl;
        /* XXX: network outbound ACL */
    } else {
        clog << "\tRemote context creation requests disabled" << endl;
    }

    clog << "\tGraphite API " << (graphite_p ? "enabled" : "disabled") << endl;
    clog << "\tGraphite API Cairo graphics rendering "
#ifdef HAVE_CAIRO
         << "compiled-in"
#else
         << "unavailable"
#endif
         << endl;

    if (dumpstats > 0) {
        clog << "\tPeriodic client statistics dumped roughly every " << dumpstats << "s" << endl;
    } else {
        clog << "\tPeriodic client statistics not dumped" << endl;
    }
#if HAVE_PTHREAD_H
    clog << "\tUsing up to " << multithread << " auxiliary threads" << endl;
#endif
}


static void
pmweb_init_random_seed (void)
{
    struct timeval tv;

    /* XXX: PR_GetRandomNoise() */
    gettimeofday (&tv, NULL);
    srandom ((unsigned int) getpid () ^ (unsigned int) tv.tv_sec ^ (unsigned int) tv.tv_usec);
}


static void
pmweb_shutdown (struct MHD_Daemon *d4, struct MHD_Daemon *d6)
{
    /* Shut down cleanly, out of a misplaced sense of propriety. */
    if (d4) {
        MHD_stop_daemon (d4);
    }
    if (d6) {
        MHD_stop_daemon (d6);
    }

    /* No longer advertise pmwebd presence on the network. */
    __pmServerUnadvertisePresence (presence);

    /*
     * Let's politely clean up all the active contexts.
     * The OS will do all that for us anyway, but let's make valgrind happy.
     */
    pmwebapi_deallocate_all ();

    timestamp (clog) << "pmwebd shutdown" << endl;
    fflush (stderr);
}

static int
option_overrides (int opt, pmOptions * opts)
{
    (void) opts;

    switch (opt) {
    case 'A':
    case 'a':
    case 'h':
    case 'G':
    case 'L':
    case 'N':
    case 'M':
    case 'p':
    case 'd':
    case 't':
    case 'i':
    case 'I':
        return 1;
    }
    return 0;
}

static pmLongOptions
longopts[] = {
    PMAPI_OPTIONS_HEADER ("Network options"),
    {"port", 1, 'p', "NUM", "listen on given TCP port [default 44323]"},
    {"ipv4", 0, '4', 0, "listen on IPv4 only"},
    {"ipv6", 0, '6', 0, "listen on IPv6 only"},
    PMAPI_OPTIONS_HEADER ("Graphite options"),
    {"graphite", 0, 'G', 0, "enable graphite 0.9 API/backend emulation"},
    {"graphite-timestamp", 1, 'i', "SEC", "minimum graphite timestep (s) [default 60]"},
    {"graphite-archivedir", 0, 'I', 0, "prefer archive directories [default OFF]"},
    PMAPI_OPTIONS_HEADER ("Context options"),
    {"timeout", 1, 't', "SEC", "max time (seconds) for PMAPI polling [default 300]"},
    {"context", 1, 'c', "NUM", "set next permanent-binding context number"},
    {"host", 1, 'h', "HOST", "permanent-bind next context to PMCD on host"},
    {"archive", 1, 'a', "FILE", "permanent-bind next context to archive"},
    {"local-PMDA", 0, 'L', 0, "permanent-bind next context to local PMDAs"},
    {"", 0, 'N', 0, "disable remote new-context requests"},
    {"", 1, 'A', "DIR", "permit remote new-archive-context under dir [default CWD]"},
    PMAPI_OPTIONS_HEADER ("Other"),
    PMOPT_DEBUG,
    {"resources", 1, 'R', "DIR", "serve non-API files from given directory"},
    {"log", 1, 'l', "FILE", "redirect diagnostics and trace output"},
    {"verbose", 0, 'v', 0, "increase verbosity"},
#ifdef HAVE_PTHREAD_H
    {"threads", 1, 'M', 0, "allow multiple threads [default 0]"},
#endif
    {"dumpstats", 1, 'd', 0, "dump client stats roughly every N seconds [default 300]"},
    {"username", 1, 'U', "USER", "decrease privilege from root to user [default pcp]"},
    {"", 1, 'x', "PATH", "fatal messages at startup sent to file [default /dev/tty]"},
    PMOPT_HELP,
    PMAPI_OPTIONS_END
};

static pmOptions opts;

int
main (int argc, char *argv[])
{
    int     c;
    int     sts;
    int     ctx;
    int     mhd_ipv4 = 1;
    int     mhd_ipv6 = 1;
    int    port = PMWEBD_PORT;
    char *   endptr;
    struct MHD_Daemon * d4 = NULL;
    struct MHD_Daemon * d6 = NULL;
    time_t last_dumpstats = 0;

    // NB: important to standardize on a single default timezone, since
    // we'll be interacting with web clients from anywhere, and dealing
    // with pcp servers/archvies from anywhere else.
    (void) setenv ("TZ", "UTC", 1);

    umask (022);
    char * username_str;
    __pmGetUsername (&username_str);

    opts.short_options = "A:a:c:D:h:Ll:NM:p:R:Gi:It:U:vx:d:46?";
    opts.long_options = longopts;
    opts.override = option_overrides;

    while ((c = pmGetOptions (argc, argv, &opts)) != EOF) {
        switch (c) {
        case 'p':
            port = (int) strtol (opts.optarg, &endptr, 0);
            if (*endptr != '\0' || port < 0 || port > 65535) {
                pmprintf ("%s: invalid port number %s\n", pmProgname, opts.optarg);
                opts.errors++;
            }
            break;

        case 't':
            maxtimeout = strtoul (opts.optarg, &endptr, 0);
            if (*endptr != '\0') {
                pmprintf ("%s: invalid timeout %s\n", pmProgname, opts.optarg);
                opts.errors++;
            }
            break;

        case 'R':
            resourcedir = opts.optarg;
            break;

        case 'G':
            graphite_p = 1;
            break;

        case 'i':
            graphite_timestep = atoi (opts.optarg);
            if (graphite_timestep <= 0) {
                pmprintf ("%s: timestep too small %s\n", pmProgname, opts.optarg);
                opts.errors++;
            }
            break;

        case 'I':
            graphite_archivedir = 1;
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

        case 'd':
            dumpstats = atoi (opts.optarg);
            if (dumpstats < 0) {
                dumpstats = 0;
            }
            break;

        case 'c':
            perm_context = strtoul (opts.optarg, &endptr, 0);
            if (*endptr != '\0' || perm_context >= INT_MAX) {
                pmprintf ("%s: invalid context number %s\n", pmProgname, opts.optarg);
                opts.errors++;
            }
            break;

        case 'h':
            if ((ctx = pmNewContext (PM_CONTEXT_HOST, opts.optarg)) < 0) {
                __pmNotifyErr (LOG_ERR, "new context failed\n");
                exit (EXIT_FAILURE);
            }
            if ((sts = pmwebapi_bind_permanent (perm_context++, ctx)) < 0) {
                __pmNotifyErr (LOG_ERR, "permanent bind failed\n");
                exit (EXIT_FAILURE);
            }
            __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, host %s, permanent\n",
                           perm_context - 1, ctx, opts.optarg);
            break;

        case 'a':
            if ((ctx = pmNewContext (PM_CONTEXT_ARCHIVE, opts.optarg)) < 0) {
                __pmNotifyErr (LOG_ERR, "new context failed\n");
                exit (EXIT_FAILURE);
            }
            if ((sts = pmwebapi_bind_permanent (perm_context++, ctx)) < 0) {
                __pmNotifyErr (LOG_ERR, "permanent bind failed\n");
                exit (EXIT_FAILURE);
            }
            __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, archive %s, permanent\n",
                           perm_context - 1, ctx, opts.optarg);
            break;

        case 'L':
            if ((ctx = pmNewContext (PM_CONTEXT_LOCAL, NULL)) < 0) {
                __pmNotifyErr (LOG_ERR, "new context failed\n");
                exit (EXIT_FAILURE);
            }
            if ((sts = pmwebapi_bind_permanent (perm_context++, ctx)) < 0) {
                __pmNotifyErr (LOG_ERR, "permanent bind failed\n");
                exit (EXIT_FAILURE);
            }
            __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) created, local, permanent\n",
                           perm_context - 1, ctx);
            break;

        case 'N':
            new_contexts_p = 0;
            break;

        case 'M':
            multithread = (unsigned) atoi (opts.optarg);
            break;

        case 'l':
            /* log file name */
            logfile = opts.optarg;
            break;

        case 'U':
            /* run as user username */
            username_str = opts.optarg;
            break;

        case 'x':
            fatalfile = opts.optarg;
            break;
        }
    }

    if (opts.errors) {
        pmUsageMessage (&opts);
        exit (EXIT_FAILURE);
    }

    /*
     * Start microhttp daemon.  Use the application-driven threading
     * model, so we don't complicate things with threads.  In the
     * future, if this daemon becomes safe to run multithreaded,
     * we could make use of MHD_USE_THREAD_PER_CONNECTION; we'd need
     * to add ample locking over pmwebd context structures etc.
     */
    if (mhd_ipv4)
        d4 = MHD_start_daemon (0, port, NULL, NULL,	/* default accept policy */
                               &mhd_respond, NULL,	/* handler callback */
                               MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout, MHD_OPTION_NOTIFY_COMPLETED,
                               &mhd_respond_completed, NULL, MHD_OPTION_END);
    if (mhd_ipv6)
        d6 = MHD_start_daemon (MHD_USE_IPv6, port, NULL, NULL,	/* default accept policy */
                               &mhd_respond, NULL,	/* handler callback */
                               MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout, MHD_OPTION_NOTIFY_COMPLETED,
                               &mhd_respond_completed, NULL, MHD_OPTION_END);
    if (d4 == NULL && d6 == NULL) {
        timestamp (cerr) << "Error starting microhttpd daemons on port " << port << endl;
        pmweb_dont_start ();
    }

    /* lose root privileges if we have them */
    if (geteuid () == 0) {
        __pmSetProcessIdentity (username_str);
    }

    /* tell the world we have arrived */
    __pmServerCreatePIDFile (PM_SERVER_WEBD_SPEC, 0);
    presence = __pmServerAdvertisePresence (PM_SERVER_WEBD_SPEC, port);

    // (re)create log file, redirect stdout/stderr
    // NB: must be done after __pmSetProcessIdentity() for proper file permissions
    if (logfile != "") {
        int
        fd;
        (void) unlink (logfile.c_str ());	// in case one's left over from a previous other-uid run
        fd = open (logfile.c_str (), O_WRONLY | O_APPEND | O_CREAT | O_TRUNC, 0666);
        if (fd < 0) {
            timestamp (cerr) << "Cannot re-create logfile " << logfile << endl;
        } else {
            int
            rc;
            // Move the new file descriptors on top of stdout/stderr
            rc = dup2 (fd, STDOUT_FILENO);
            if (rc < 0) {
                // rather unlikely
                timestamp (cerr) << "Cannot redirect logfile to stdout" << endl;
            }
            rc = dup2 (fd, STDERR_FILENO);
            if (rc < 0) {
                // rather unlikely
                timestamp (cerr) << "Cannot redirect logfile to stderr" << endl;
            }
            rc = close (fd);
            if (rc < 0) {
                // rather unlikely
                timestamp (cerr) << "Cannot close logfile fd" << endl;
            }
        }
    }

    timestamp (clog) << pmProgname << endl;
    server_dump_request_ports (d4 != NULL, d6 != NULL, port);
    server_dump_configuration ();

    /* Set up signal handlers. */
    __pmSetSignalHandler (SIGHUP, SIG_IGN);
    __pmSetSignalHandler (SIGINT, handle_signals);
    __pmSetSignalHandler (SIGTERM, handle_signals);
    __pmSetSignalHandler (SIGQUIT, handle_signals);
    /* Not this one; might get it from pmcd momentary disconnection. */
    /* __pmSetSignalHandler(SIGPIPE, handle_signals); */

    /* Setup randomness for calls to random() */
    pmweb_init_random_seed ();

    // A place to track utilization
    /* Block indefinitely. */
    while (!exit_p) {
        struct timeval tv;
        fd_set        rs;
        fd_set        ws;
        fd_set        es;
        int        max = 0;

        /* Based upon MHD fileserver_example_external_select.c */
        FD_ZERO (&rs);
        FD_ZERO (&ws);
        FD_ZERO (&es);
        if (d4 && MHD_YES != MHD_get_fdset (d4, &rs, &ws, &es, &max)) {
            break;		/* fatal internal error */
        }
        if (d6 && MHD_YES != MHD_get_fdset (d6, &rs, &ws, &es, &max)) {
            break;		/* fatal internal error */
        }

        /*
         * Find the next expiry.  We don't need to bound it by
         * MHD_get_timeout, since we don't use a small
         * MHD_OPTION_CONNECTION_TIMEOUT.
         */
        tv.tv_sec = pmwebapi_gc ();
        tv.tv_usec = 0;
        // NB: we could clamp tv.tv_sec to dumpstats too, but that's pointless:
        // it would only fire if there were no clients during the whole interval,
        // in which case there are no stats to dump.
        // NB: this is not actually true; we could have a bunch of clients during
        // the first N-1 seconds of the dumpstats interval, then nothing for the
        // pmwebapi gc-clamp limit; and those N-1-second clients would not be listed
        // in a timely manner.  So clamp clamp clamp.
        // NB: we -could- estimate how long till the next dumpstats interval closes
        // (ie. now - (last_dumpstats + dumpstats)), but that could be negative if
        // we've fallen behind.  Let's not worry about reporting on an exact schedule.
        if (dumpstats > 0 && tv.tv_sec > dumpstats && ! clients_usage.empty ()) {
            tv.tv_sec = dumpstats;
        }

        select (max + 1, &rs, &ws, &es, &tv);

        if (d4) {
            MHD_run (d4);
        }
        if (d6) {
            MHD_run (d6);
        }

        if (dumpstats > 0) {
            time_t now = time (NULL);

            if (last_dumpstats == 0) { // don't report immediately after startup
                last_dumpstats = now;
            } else if ((now - last_dumpstats) >= dumpstats) {
                last_dumpstats = now;
                if (! clients_usage.empty ()) {
                    timestamp (clog) << "Client request counts:" << endl;
                }
                for (map<string,unsigned>::iterator it = clients_usage.begin ();
                        it != clients_usage.end ();
                        it++) {
                    clog << "\t" << it->first << "\t" << it->second << endl;
                }

                clients_usage.clear ();
            }
        }
    }

    pmweb_shutdown (d4, d6);
    return 0;
}
