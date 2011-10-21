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

#include "pmwebapi.h"

/* ------------------------------------------------------------------------ */


#define JSON_MIMETYPE "application/json"



struct webcontext {
  /* __pmHashNode key is     unique randomized web context key */
  /* XXX: optionally bind session to a particular IP address? */
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

