/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011-2012 Red Hat Inc.
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

char *pmnsfile = NULL;         /* set by -n option */
char *uriprefix = "/pmapi";    /* overridden by -a option */
char *resourcedir = NULL;      /* set by -r option */
unsigned verbosity = 0;        /* set by -v option */
unsigned maxtimeout = 300;     /* set by -t option */
unsigned exit_p;               /* counted by SIG* handler */


/* Respond to a new incoming HTTP request.  It may be
   one of three general categories:
   (a) creation of a new PMAPI context: do it
   (b) operation an existing context: do it
   (c) access to some non-API URI: serve it from $resourcedir/ if configured.
*/
int mhd_respond (void *cls, struct MHD_Connection *connection,
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


  if (verbosity > 1) {
    struct sockaddr *so;
    char hostname[128];
    char servname[128];
    int rc = -1;

    so = (struct sockaddr *)
      MHD_get_connection_info (connection,MHD_CONNECTION_INFO_CLIENT_ADDRESS)->client_addr;
    if (so && so->sa_family == AF_INET)
      rc = getnameinfo (so, sizeof(struct sockaddr_in),
                        hostname, sizeof(hostname),
                        servname, sizeof(servname),
                        0);
    else if (so && so->sa_family == AF_INET6)
      rc = getnameinfo (so, sizeof(struct sockaddr_in6),
                        hostname, sizeof(hostname),
                        servname, sizeof(servname),
                        0);
    if (rc != 0)
      hostname[0] = servname[0] = '\0';

    __pmNotifyErr (LOG_INFO, "%s:%s %s %s %s", hostname, servname, version, method, url);
  }

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

static char *options = "p:n:K:t:a:r:h?v";
static char usage[] =
  "Usage: %s [options]\n\n"
  "Options:\n"
  "  -p N          listen on TCP port N\n"
  "  -K spec       optional additional PMDA spec for local connection\n"
  "                spec is of the form op,domain,dso-path,init-routine\n"
  "  -n pnmsfile   use an alternative PMNS\n"
  "  -a prefix     serve API requests with given prefix, default /pmapi\n"
  "  -r resdir     serve non-API files from given directory, no default\n"
  "  -t timeout    max time (seconds) for pmapi polling, default 300\n"
  "  -v            increase verbosity\n"
  "  -h -?         help\n";


void handle_signals (int sig)
{
  exit_p ++;
}



/* Main loop of pmwebapi server. */
int main(int argc, char *argv[])
{
  struct MHD_Daemon *d = NULL;
  unsigned short port = 44323; /* pmcdproxy + 1 */
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
        pn = strtol(optarg, &endptr, 0);
        if (*endptr != '\0' || pn < 0 || pn > 65535) {
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
        tn = strtol(optarg, &endptr, 0);
        if (*endptr != '\0' || tn < 0 || tn > UINT_MAX) {
          fprintf(stderr, "%s: invalid -t timeoutr %s\n", pmProgname, optarg);
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

    case 'n':
      pmnsfile = optarg;
      break;

    case 'a':
      uriprefix = optarg;
      break;

    case 'r':
      resourcedir = optarg;
      break;

    case 'v':
      verbosity ++;
      break;

    default:
    case '?':
    case 'h':
      fprintf(stderr, usage, pmProgname);
      exit(EXIT_FAILURE);
    }

  if (errflag)
    exit(EXIT_FAILURE);

  /* Start microhttpd daemon.  Use the application-driven threading
     model, so we don't complicate things with threads.  In the
     future, if PMAPI becomes safe to invoke from a multithreaded
     application, consider MHD_USE_THREAD_PER_CONNECTION, with
     ample locking over the contexts etc. */
  d = MHD_start_daemon(0 /* | MHD_USE_IPv6 */,
                       port,
                       NULL, NULL,              /* default accept policy */
                       &mhd_respond, NULL,      /* handler callback */
                       MHD_OPTION_CONNECTION_TIMEOUT, maxtimeout,
                       MHD_OPTION_END);
  if (d == NULL) {
    fprintf(stderr, "%s: error starting microhttpd thread\n", pmProgname);
    exit(EXIT_FAILURE);
  }

  __pmNotifyErr (LOG_INFO, "Started daemon on tcp port %d, pmapi url %s\n", port, uriprefix);
  if (resourcedir) __pmNotifyErr (LOG_INFO, "Serving other urls under directory %s", resourcedir);

  /* Set up signal handlers. */
  signal (SIGINT, handle_signals);
  signal (SIGHUP, handle_signals);
  signal (SIGPIPE, handle_signals);
  signal (SIGTERM, handle_signals);
  signal (SIGQUIT, handle_signals);

  srandom (getpid() ^ (unsigned int) time (NULL));

  /* Block indefinitely. */
  while (! exit_p) {
    struct timeval tv;
    fd_set rs;
    fd_set ws;
    fd_set es;
    int max;

    /* Based upon MHD fileserver_example_external_select.c */
    FD_ZERO (& rs);
    FD_ZERO (& ws);
    FD_ZERO (& es);
    if (MHD_YES != MHD_get_fdset (d, &rs, &ws, &es, &max))
      break; /* fatal internal error */

    /* Always block for at most half the context maxtimeout, so on
       average we garbage-collect contexts at no more than 50% past
       their expiry times.  We don't need to bound it by
       MHD_get_timeout, since we don't use a small
       MHD_OPTION_CONNECTION_TIMEOUT. */
    tv.tv_sec = maxtimeout/2;
    tv.tv_usec = (maxtimeout*1000000)/2 % 1000000;

    (void) select (max+1, &rs, &ws, &es, &tv);

    MHD_run (d);
    pmwebapi_gc ();
  }

  /* Shut down cleanly, out of a misplaced sense of propriety. */
  MHD_stop_daemon (d);

  /* We could pmDestroyContext() all the active contexts. */
  /* We could free() the contexts too. */
  /* But the OS will do all that for us anyway. */
  return 0;
}

