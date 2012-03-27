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


#define JSON_MIMETYPE "application/json"



struct webcontext {
  /* __pmHashNode key is     unique randomized web context key, [1,INT_MAX] */
  /* XXX: optionally bind session to a particular IP address? */
  unsigned  mypolltimeout;
  time_t    expires;      /* poll timeout */
  int       context;      /* PMAPI context handle, 0 if deleted/free */
};

/* if-threaded: pthread_mutex_t context_lock; */
static __pmHashCtl contexts;



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



int pmwebapi_respond_new_context (struct MHD_Connection *connection)
{
  /* Create a context. */
  const char *val;
  int rc;
  int context = -1;
  static char http_response [512];  /* if-threaded: not static */
  unsigned polltimeout;
  struct MHD_Response *resp;
    
  val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "hostname");
  if (val) context = pmNewContext (PM_CONTEXT_HOST, val); /* XXX: limit access */
  else {
    val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "archivefile");
    if (val) context = pmNewContext (PM_CONTEXT_ARCHIVE, val); /* XXX: limit access */
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
    /* Create a new context key for the webapi.  We just use a random integer within
       a reasonable range: 1..INT_MAX */
    int webapi_ctx;
    int iterations = 0;
    do {
      /* Preclude infinite looping here, for example due to a badly behaving
         random(3) implementation. */
      iterations ++;
      if (iterations > 100)
        {
          __pmNotifyErr (LOG_ERR, "webapi_ctx allocation failed\n");
          pmDestroyContext (context);
          goto out;
        }

      webapi_ctx = random(); /* we hope RAND_MAX is large enough */
      if (webapi_ctx <= 0 || webapi_ctx > INT_MAX) continue;
      /* This may already exist.  We loop in case the key id already exists. */
    } while(__pmHashSearch (webapi_ctx, & contexts) != NULL);

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
    c->mypolltimeout = polltimeout;
    c->expires += c->mypolltimeout;
    rc = __pmHashAdd(webapi_ctx, c, & contexts);
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
                     webapi_ctx, context, polltimeout);

    rc = snprintf (http_response, sizeof(http_response), "{ \"context\": %d }", webapi_ctx);
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

 out:
  return MHD_NO;
}



int pmwebapi_respond_metric_list (struct MHD_Connection *connection,
                                  struct webcontext *c)
{
  const char *val;

  val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "name");
  if (val)
    {
    }

 out:
  return MHD_NO;
}


int pmwebapi_respond (void *cls, struct MHD_Connection *connection,
                      const char* url,
                      const char* method, const char* upload_data, size_t *upload_data_size)
{
  /* NB: url is already edited to remove the /pmapi/ prefix. */
  long webapi_ctx;
  __pmHashNode *chn;
  struct webcontext *c;
  char *context_command;

  /* Decode the calls to the web API. */

  /* ------------------------------------------------------------------------ */
  /* context creation */
  /* if-multithreaded: write-lock contexts */
  if (0 == strcmp (url, "context") &&
      (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
    return pmwebapi_respond_new_context (connection);

  /* ------------------------------------------------------------------------ */
  /* All other calls use context/$CTX/command, so we parse $CTX
     generally and map it to the webcontext* */
  if (! (0 == strncmp (url, "context/", sizeof("context/")) &&
         (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET"))))
    goto out;
  url += sizeof("context/");
  webapi_ctx = strtol (url, & context_command, 10); /* matches %d above */
  if (webapi_ctx <= 0 /* range check, plus string-nonemptyness check */
      || webapi_ctx > INT_MAX /* matches random() loop above */
      || *context_command != '/')  /* parsed up to the next slash */
    goto out;
  /* if-multithreaded: read-lock contexts */
  chn = __pmHashSearch ((int)webapi_ctx, & contexts);
  if (chn == NULL)
    goto out;
  c = (struct webcontext *) chn->data;
  assert (c != NULL);

  /* Update last-use of this connection. */
  time (& c->expires);
  c->expires += c->mypolltimeout;

  /* ------------------------------------------------------------------------ */
  /* context duplication: /context/$ID/copy */

  /* ------------------------------------------------------------------------ */
  /* metric enumeration: /context/$ID/_metric */
  if (0 == strcmp (context_command, "_metric") &&
      (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
    return pmwebapi_respond_metric_list (connection, c);

out:
  return MHD_NO;
}
