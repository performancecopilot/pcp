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
#include <stdio.h>
#include <stdarg.h>


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
  int webapi_ctx;
  int iterations = 0;

  val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "hostname");
  if (val) {
    context = pmNewContext (PM_CONTEXT_HOST, val); /* XXX: limit access */
  } else {
    val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "archivefile");
    if (val) {
      context = pmNewContext (PM_CONTEXT_ARCHIVE, val); /* XXX: limit access */
    } else {
      context = pmNewContext (PM_CONTEXT_LOCAL, NULL);
    }
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

  if (context < 0) {
    __pmNotifyErr (LOG_ERR, "new context failed\n");
    goto out;
  }

  /* Create a new context key for the webapi.  We just use a random integer within
     a reasonable range: 1..INT_MAX */
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
  if (resp == NULL) {
    __pmNotifyErr (LOG_ERR, "MHD_create_response_from_buffer failed\n");
    goto out;
  }
  rc = MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
  if (rc != MHD_YES) {
    __pmNotifyErr (LOG_ERR, "MHD_add_response_header failed\n");
  }
  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  if (rc != MHD_YES) __pmNotifyErr (LOG_ERR, "MHD_queue_response failed\n");
  MHD_destroy_response (resp);
  return rc;

 out:
  return MHD_NO;
}


/* ------------------------------------------------------------------------ */

/* Buffer for building a MHD_Response incrementally.  libmicrohttpd does not
   provide such a facility natively. */
struct mhdb {
  size_t buf_size;
  size_t buf_used;
  char *buf;   /* malloc/realloc()'d.  If NULL, mhdbuf is a loser: 
                  Attempt no landings there. */
};


/* Create a MHD response buffer of given initial size.  Upon failure,
   leave the buffer unallocated, which will block any mhdb_printfs,
   and cause mhdb_fini_response to fail. */
void mhdb_init (struct mhdb *md, size_t size)
{
  md->buf_size = size;
  md->buf_used = 0;
  md->buf = malloc (md->buf_size); /* may be NULL => loser */
}


/* Add a given formatted string to the end of the MHD response buffer.
   Extend/realloc the buffer if needed.  If unable, free the buffer,
   which will block any further mhdb_vsprintfs, and cause the
   mhdb_fini_response to fail. */
void mhdb_printf(struct mhdb *md, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void mhdb_printf(struct mhdb *md, const char *fmt, ...)
{
  va_list vl;
  int n;
  int buf_remaining = md->buf_size - md->buf_used;

  if (md->buf == NULL) return; /* reject loser */

  va_start (vl, fmt);
  n = vsnprintf (md->buf + md->buf_used, buf_remaining, fmt, vl);
  va_end (vl);

  if (n >= buf_remaining) { /* complex case: buffer overflow */
    void *nbuf;

    md->buf_size += n*128; /* pad it to discourage reoffense */
    buf_remaining += n*128;
    nbuf = realloc (md->buf, md->buf_size); 

    if (nbuf == NULL) { /* ENOMEM */
      free (md->buf); /* realloc left it alone */
      md->buf = NULL; /* tag loser */
    } else { /* success */
      md->buf = nbuf;
      /* Try vsnprintf again into the new buffer. */
      va_start (vl, fmt);
      n = vsnprintf (md->buf + md->buf_used, buf_remaining, fmt, vl);
      va_end (vl);
      assert (n >= 0 && n < buf_remaining); /* should not fail */
      md->buf_used += n;
    }
  } else if (n < 0) { /* simple case: other vsprintf error */
    free (md->buf);
    md->buf = NULL; /* tag loser */
  } else { /* normal case: vsprintf success */
    md->buf_used += n;
  }

}



/* Ensure that any recent mhdb_printfs are canceled, and that the
   final mhdb_fini_response will fail. */
void mhdb_unprintf(struct mhdb *md)
{
  if (md->buf) {
    free (md->buf);
    md->buf = NULL;
  }
}



/* Create a MHD_Response from the mhdb, now that we're done with it.
   If the buffer overflowed earlier (was unable to be extended), or if
   the MHD_create_response* function fails, return NULL.  Ensure that
   the buffer will be freed either by us here, or later by a
   successful MHD_create_response* function. */
struct MHD_Response* mhdb_fini_response(struct mhdb *md)
{
  struct MHD_Response *r;

  if (md->buf == NULL) return NULL; /* reject loser */
  r = MHD_create_response_from_buffer (md->buf_used, md->buf, MHD_RESPMEM_MUST_FREE);
  if (r == NULL) {
    free (md->buf); /* we need to free it ourselves */
    /* fall through */
  }
  return r;
}




/* ------------------------------------------------------------------------ */

struct metric_list_traverse_closure {
  struct MHD_Connection *connection;
  struct webcontext *c;
  struct mhdb mhdb;
  unsigned num_metrics;
};



void metric_list_traverse (const char* metric, void *closure)
{
  struct metric_list_traverse_closure *mltc = closure;
  pmID metric_id;
  pmDesc metric_desc;
  int rc;

  assert (mltc != NULL);

  rc = pmLookupName (1, (char **)& metric, & metric_id);
  if (rc != 1) {
    /* Quietly skip this metric. */
    return;
  }
  assert (metric_id != PM_ID_NULL);

  rc = pmLookupDesc (metric_id, & metric_desc);
  if (rc != 0) {
    /* Quietly skip this metric. */
    return;
  }
  
  if (mltc->num_metrics > 0)
    mhdb_printf (& mltc->mhdb, ",\n");
  
  mhdb_printf (& mltc->mhdb, "{");
  mhdb_printf (& mltc->mhdb, "\"name\":\"%s\", ", metric);
  mhdb_printf (& mltc->mhdb, "\"pmID\":%lu, ", (unsigned long) metric_id);
  mhdb_printf (& mltc->mhdb, "\"type\":%d, ", metric_desc.type); /* XXX: asciify */
  mhdb_printf (& mltc->mhdb, "\"indom\":%lu", (unsigned long) metric_desc.indom);
  /* XXX:units */
  mhdb_printf (& mltc->mhdb, "}");
  
  mltc->num_metrics ++;
}


int pmwebapi_respond_metric_list (struct MHD_Connection *connection,
                                  struct webcontext *c)
{
  struct metric_list_traverse_closure mltc;
  const char* val;
  struct MHD_Response* resp;
  int rc;

  mltc.connection = connection;
  mltc.c = c;
  mltc.num_metrics = 0;

  /* We need to construct a copy of the entire JSON metric metadata string,
     in one long malloc()'d buffer.  We size it generously to avoid having
     to realloc the bad boy and cause copies. */
  mhdb_init (& mltc.mhdb, 200000); /* 1000 pmns entries * 200 bytes each */
  mhdb_printf(& mltc.mhdb, "{ \"metrics\":[\n");

  val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "prefix");
  if (val == NULL) val = "";
  /* XXX: which PMAPI context is used? */
  pmTraversePMNS_r (val, & metric_list_traverse, & mltc);

  mhdb_printf(&mltc.mhdb, "] }");
  resp = mhdb_fini_response (& mltc.mhdb);
  if (resp == NULL) {
    __pmNotifyErr (LOG_ERR, "mhdb_response failed\n");
    goto out;
  }
  rc = MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
  if (rc != MHD_YES) {
    __pmNotifyErr (LOG_ERR, "MHD_add_response_header failed\n");
    /* FALLTHROUGH */
  }
  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  if (rc != MHD_YES) {
    __pmNotifyErr (LOG_ERR, "MHD_queue_response failed\n");
    /* FALLTHROUGH */
  }
  MHD_destroy_response (resp);
  return MHD_YES;

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
  /* All other calls use $CTX/command, so we parse $CTX
     generally and map it to the webcontext* */
  if (! (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET"))) {
    __pmNotifyErr (LOG_WARNING, "unknown method %s\n", method);
    goto out;
  }
  webapi_ctx = strtol (url, & context_command, 10); /* matches %d above */
  if (webapi_ctx <= 0 /* range check, plus string-nonemptyness check */
      || webapi_ctx > INT_MAX /* matches random() loop above */
      || *context_command != '/') { /* parsed up to the next slash */
    __pmNotifyErr (LOG_WARNING, "unrecognized %s url %s \n", method, url);
    goto out;
  }
  context_command ++; /* skip the / */

  /* if-multithreaded: read-lock contexts */
  chn = __pmHashSearch ((int)webapi_ctx, & contexts);
  if (chn == NULL) {
    __pmNotifyErr (LOG_WARNING, "unknown web context #%ld\n", webapi_ctx);
    goto out;
  }
  c = (struct webcontext *) chn->data;
  assert (c != NULL);

  /* Update last-use of this connection. */
  time (& c->expires);
  c->expires += c->mypolltimeout;

  /* Switch to this context for subsequent operations. */
  /* if-multithreaded: watch out. */
  pmUseContext (c->context);

  /* ------------------------------------------------------------------------ */
  /* context duplication: /context/$ID/copy */

  /* ------------------------------------------------------------------------ */
  /* metric enumeration: /context/$ID/_metric */
  if (0 == strcmp (context_command, "_metric") &&
      (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
    return pmwebapi_respond_metric_list (connection, c);


  __pmNotifyErr (LOG_WARNING, "unrecognized %s context command %s \n", method, context_command);

out:
  return MHD_NO;
}
