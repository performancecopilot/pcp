/*
 * JSON web bridge for PMAPI.
 *
 * Copyright (c) 2011 Red Hat Inc.
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

#define _XOPEN_SOURCE 600
#define _FILE_OFFSET_BITS 64

#include <microhttpd.h>

#include "pmapi.h"
#include "impl.h"

#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/socket.h>
#include <netdb.h>
#include <limits.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* ------------------------------------------------------------------------ */

char *pmnsfile = NULL;         /* set by -n option */
char *uriprefix = "/pmapi";    /* overridden by -a option */
char *resourcedir = NULL;      /* set by -r option */
unsigned verbosity = 0;        /* set by -v option */
unsigned maxtimeout = 300;     /* set by -t option */
unsigned exit_p;               /* counted by SIG* handler */

struct webcontext {
  /* __pmHashNode key is     unique randomized web context key */
  time_t    expires;      /* poll timeout */
  int       context;      /* PMAPI context handle, 0 if deleted/free */
};

/* if-threaded: pthread_mutex_t context_lock; */
__pmHashCtl contexts;

#define JSON_MIMETYPE "text/plain" /* "application/json" */


/* ------------------------------------------------------------------------ */


/* A __pmHashCtl hash-table iteration function that would better
   belong to libpcp/src/logmeta.c */
typedef enum { PM_PHI_CONTINUE, PM_PHI_CONTINUE_DELETE,
               PM_PHI_STOP, PM_PHI_STOP_DELETE } pmHashIterResult_t;
typedef pmHashIterResult_t (*pmHashIterFn_t) (const __pmHashCtl *hcp, void *cdata, 
                                              const __pmHashNode *kv);

/* Iterate over the entire hash table.  For each entry, call *fn,
   passing *cdata and the current key/value pair.  The function's
   return value decides how to continue or abort iteration.  The
   callback function must not modify the hash table. */
void __pmHashIter(__pmHashCtl *hcp, pmHashIterFn_t fn, void *cdata)
{
  int n;

  for (n = 0; n < hcp->hsize; n++) {
    __pmHashNode *tp = hcp->hash[n];
    __pmHashNode **tpp = & hcp->hash[n];

    while (tp != NULL) {
      pmHashIterResult_t res = (*fn)(hcp, cdata, tp);

      switch (res) {
      case PM_PHI_STOP:
        return;
      case PM_PHI_STOP_DELETE:
        *tpp = tp->next;  /* unlink */
        free (tp);
        return;
      case PM_PHI_CONTINUE:
        tpp = & tp->next;
        tp = *tpp;
        break;
      case PM_PHI_CONTINUE_DELETE:
        *tpp = tp->next;  /* unlink */
        /* NB: do not change tpp.  It will still point at the previous
           node's "next" pointer.  Consider consecutive CONTINUE_DELETEs. */
        free (tp);
        tp = *tpp; /* == tp->next, except that tp is already freed. */
        break;
      default:
        abort();
      }
    }
  }
}


pmHashIterResult_t pmwebapi_gc_fn (const __pmHashCtl *hcp, void *cdata, 
                                   const __pmHashNode *kv) 
{
  const struct webcontext *value = kv->data;
  time_t now = * (time_t *) cdata;

  if (value->expires < now)
    {
      int rc = pmDestroyContext (value->context);
      if (verbosity)
        __pmNotifyErr (LOG_INFO, "context (%d=%d) expired.\n", kv->key, value->context);
      if (rc) __pmNotifyErr (LOG_ERR, "pmDestroyContext (%d) failed: %d\n",
                             value->context, rc);
      return PM_PHI_CONTINUE_DELETE;
    }
  else
    return PM_PHI_CONTINUE;
}


/* Check whether any contexts have been unpolled so long that they
   should be considered abandoned.  If so, close 'em, free 'em, yak
   'em, smack 'em. */
void pmwebapi_gc ()
{
  time_t now;
  (void) time (& now);

  /* if-multithread: Lock contexts. */
  __pmHashIter (& contexts, pmwebapi_gc_fn, & now);
  /* if-multithread: Unlock contexts. */  
}


/* ------------------------------------------------------------------------ */


int pmwebapi_respond (void *cls, struct MHD_Connection *connection,
                      const char* url,
                      const char* method, const char* upload_data, size_t *upload_data_size)
{
  /* Decode the calls to the web API. */
  if (0 == strcmp (url, "context") && 
      (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET"))) {
    /* Create a context. */
    const char *val;
    int rc;
    int context = -1;
    static char http_response [512];  /* if-threaded: not static */
    unsigned polltimeout;
    struct MHD_Response *resp;

    val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "hostname");
    if (val) context = pmNewContext (PM_CONTEXT_HOST, val);
    else {
      val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "archivefile");
      if (val) context = pmNewContext (PM_CONTEXT_ARCHIVE, val);
      else context = pmNewContext (PM_CONTEXT_LOCAL, NULL);
    }

    /* Process optional ?polltimeout=SECONDS field.  If broken/missing, assume maxtimeout. */
    val = MHD_lookup_connection_value (connection,  MHD_GET_ARGUMENT_KIND, "polltimeout");
    if (val) {
        long pt;
        char *endptr;
        pt = strtol(optarg, &endptr, 0);
        if (*endptr != '\0' || pt <= 0 || pt > maxtimeout) {
          polltimeout = maxtimeout;
        } 
        else
          polltimeout = (unsigned) pt;
    } else {
      polltimeout = maxtimeout;
    }

    if (context >= 0) { /* success! */
      /* Create a new context key for the webapi.  We just use a random integer.  */
      int new_contextid;
      do {
        new_contextid = random(); /* we hope RAND_MAX is large enough */
        /* This may already exist.  We loop in case the key id already exists. */
      } while(__pmHashSearch (new_contextid, & contexts) != NULL);

      /* Allocate & fill in our webapi context. */
      struct webcontext *c = malloc(sizeof(struct webcontext));
      if (! c)
        {
          __pmNotifyErr (LOG_ERR, "context malloc failed\n");
          pmDestroyContext (context);
          goto out;
        }
      c->context = context;
      time (& c->expires);
      c->expires += polltimeout;
      rc = __pmHashAdd(new_contextid, c, & contexts);
      if (rc < 0)
        {
          __pmNotifyErr (LOG_ERR, "pmHashAdd failed (%d)\n", rc);
          free (c);
          pmDestroyContext (context);
          goto out;
        }

      /* Errors beyond this point don't require instant cleanup; the
         periodic context GC will do it all. */

      if (verbosity)
        __pmNotifyErr (LOG_INFO, "context (%d=%d) created, expires in %us.\n",
                       new_contextid, context, polltimeout);

      rc = snprintf (http_response, sizeof(http_response), "{ \"context\": %d }", new_contextid);
      assert (rc >= 0 && rc < sizeof(http_response));
      resp = MHD_create_response_from_buffer (strlen(http_response), http_response,
                                              MHD_RESPMEM_PERSISTENT);
      if (resp == NULL) __pmNotifyErr (LOG_ERR, "MHD_create_response_from_buffer failed\n");
      MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
      rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
      if (rc != MHD_YES) __pmNotifyErr (LOG_ERR, "MHD_queue_response failed\n");
      MHD_destroy_response (resp);
      return rc;
    }
  } else if(0 == strcmp (method, "GET")) {
  }

  out:
 return MHD_NO;
}


/* ------------------------------------------------------------------------ */


static const char *guess_content_type (const char* filename)
{
  const char *extension = rindex (filename, '.');
  if (extension == NULL) return NULL;

  /* One could go all out and parse /etc/mime.types, or one can do this ... */
  if (0 == strcasecmp (extension, "html")) return "text/html";
  if (0 == strcasecmp (extension, "js")) return "text/javascript";
  if (0 == strcasecmp (extension, "json")) return "application/json";
  if (0 == strcasecmp (extension, "txt")) return "text/plain";
  if (0 == strcasecmp (extension, "xml")) return "text/xml";
  if (0 == strcasecmp (extension, "svg")) return "image/svg+xml";
  if (0 == strcasecmp (extension, "png")) return "image/png";
  if (0 == strcasecmp (extension, "jpg")) return "image/jpg";

  return NULL;
}


static const char *create_rfc822_date (time_t t)
{
  static char datebuf[512];
  struct tm *now = gmtime (& t);
  size_t rc = strftime (datebuf, sizeof(datebuf), "%a, %d %b %Y %T %z", now);
  if (rc <= 0 || rc >= sizeof(datebuf)) return NULL;
  return datebuf;
}


/* Respond to a GET request, not under the pmwebapi URL prefix.  This
   is a mini fileserver, just for small standalone installations of
   pmwebapi-based web front-ends. */
int pmwebres_respond (void *cls, struct MHD_Connection *connection,
                      const char* url)
{
  int fd;
  int rc;
  char filename [PATH_MAX];
  struct stat fds;
  struct MHD_Response *resp;  
  const char *ctype;

  assert (resourcedir != NULL); /* facility is enabled at all */

  /* Reject some obvious ways of escaping resourcedir. */
  if (NULL != strstr (url, "/..")) {
    __pmNotifyErr (LOG_ERR, "pmwebres suspicious url %s\n", url);
    goto out;
  }

  assert (url[0] == '/');
  rc = snprintf (filename, sizeof(filename), "%s%s", resourcedir, url);
  if (rc < 0 || rc >= sizeof(filename))
    goto out;

  fd = open (filename, O_RDONLY);
  if (fd < 0) {
    __pmNotifyErr (LOG_ERR, "pmwebres open %s failed (%d)\n", filename, fd);
    goto out; /* unceremonious; consider 404 HTTP instead. */
  }

  rc = fstat (fd, &fds);
  if (rc < 0) {
    __pmNotifyErr (LOG_ERR, "pmwebres stat %s failed (%d)\n", filename, rc);
    close (fd);
    goto out;
  }

  if (! S_ISREG (fds.st_mode)) { /* consider directory-listing instead. */
    __pmNotifyErr (LOG_ERR, "pmwebres non-file %s attempted\n", filename);
    close (fd);
    goto out;
  }

  if (verbosity)
    __pmNotifyErr (LOG_INFO, "pmwebres serving file %s.\n", filename);

  resp = MHD_create_response_from_fd_at_offset (fds.st_size, fd, 0);    /* auto-closes fd */
  if (resp == NULL) {
    __pmNotifyErr (LOG_ERR, "MHD_create_response_from_callback failed\n");
    close (fd);
    goto out;
  }

  /* Guess at a suitable MIME content-type. */
  ctype = guess_content_type (filename);
  if (ctype)
    (void) MHD_add_response_header (resp, "Content-Type", ctype);

  /* And since we're generous to a fault, supply a timestamp field to
     assist caching. */
  /* if-threaded: make non-static */
  ctype = create_rfc822_date (fds.st_mtime);
  if (ctype)
    (void) MHD_add_response_header (resp, "Last-Modified", ctype);

  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  MHD_destroy_response (resp);
  return rc;

 out:  
  return MHD_NO;
}


/* ------------------------------------------------------------------------ */

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
