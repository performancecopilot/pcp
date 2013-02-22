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
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <ctype.h>


/* ------------------------------------------------------------------------ */


#define JSON_MIMETYPE "application/json"


struct webcontext {
  /* __pmHashNode key is     unique randomized web context key, [1,INT_MAX] */
  /* XXX: optionally bind session to a particular IP address? */
  unsigned  mypolltimeout;
  time_t    expires;      /* poll timeout, 0 if never expires */
  int       context;      /* PMAPI context handle, 0 if deleted/free */
};

/* if-threaded: pthread_mutex_t context_lock; */
static __pmHashCtl contexts;



pmHashIterResult_t pmwebapi_gc_fn (const __pmHashCtl *hcp, void *cdata,
                                   const __pmHashNode *kv)
{
  const struct webcontext *value = kv->data;
  time_t now = * (time_t *) cdata;

  if (value->expires && value->expires < now)
    {
      int rc;
      if (verbosity)
        __pmNotifyErr (LOG_INFO, "context (web%d=pm%d) expired.\n", kv->key, value->context);
      rc = pmDestroyContext (value->context);
      if (rc)
        __pmNotifyErr (LOG_ERR, "pmDestroyContext (%d) failed: %d\n",
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



int webcontext_allocate (int webapi_ctx, struct webcontext** wc)
{
  if (__pmHashSearch (webapi_ctx, & contexts) != NULL)
    return -EEXIST;
  
  /* Allocate & fill in our webapi context. */
  assert (wc);
  *wc = malloc(sizeof(struct webcontext));
  if (! *wc)
    return -ENOMEM;

  int rc = __pmHashAdd(webapi_ctx, *wc, & contexts);
  if (rc < 0) {
    free (*wc);
    return rc;
  }

  return 0;
}


int pmwebapi_bind_permanent (int webapi_ctx, int pcp_context)
{
  struct webcontext* c;
  int rc = webcontext_allocate (webapi_ctx, &c);

  if (rc < 0)
    return rc;

  assert (c);
  assert (pcp_context >= 0);
  c->context = pcp_context;
  c->mypolltimeout = ~0;
  c->expires = 0;

  return 0;
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
    errno = 0;
    pt = strtol(optarg, &endptr, 0);
    if (errno != 0 || *endptr != '\0' || pt <= 0 || pt > maxtimeout) {
      polltimeout = maxtimeout;
    }
    else
      polltimeout = (unsigned) pt;
  } else {
    polltimeout = maxtimeout;
  }

  if (context < 0) {
    pmweb_notify (LOG_ERR, connection, "new context failed\n");
    goto out;
  }

  struct webcontext* c = NULL;
  /* Create a new context key for the webapi.  We just use a random integer within
     a reasonable range: 1..INT_MAX */
  while (1) {
    /* Preclude infinite looping here, for example due to a badly behaving
       random(3) implementation. */
    iterations ++;
    if (iterations > 100)
      {
        pmweb_notify (LOG_ERR, connection, "webapi_ctx allocation failed\n");
        pmDestroyContext (context);
        goto out;
      }
    
    webapi_ctx = random(); /* we hope RAND_MAX is large enough */
    if (webapi_ctx <= 0 || webapi_ctx > INT_MAX) continue;

    rc = webcontext_allocate (webapi_ctx, & c);
    if (rc == 0)
      break;
    /* This may already exist.  We loop in case the key id already exists. */
  }

  assert (c);
  c->context = context;
  time (& c->expires);
  c->mypolltimeout = polltimeout;
  c->expires += c->mypolltimeout;
  
  /* Errors beyond this point don't require instant cleanup; the
     periodic context GC will do it all. */
  
  if (verbosity)
    pmweb_notify (LOG_INFO, connection, "context (web%d=pm%d) created, expires in %us.\n",
                   webapi_ctx, context, polltimeout);
  
  rc = snprintf (http_response, sizeof(http_response), "{ \"context\": %d }", webapi_ctx);
  assert (rc >= 0 && rc < sizeof(http_response));
  resp = MHD_create_response_from_buffer (strlen(http_response), http_response,
                                          MHD_RESPMEM_PERSISTENT);
  if (resp == NULL) {
    pmweb_notify (LOG_ERR, connection, "MHD_create_response_from_buffer failed\n");
    goto out;
  }
  rc = MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
  if (rc != MHD_YES) {
    pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
  }
  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  if (rc != MHD_YES) pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
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


/* Print a string with JSON quoting.  Replace non-ASCII characters
   with \uFFFD "REPLACEMENT CHARACTER". */
void mhdb_print_qstring(struct mhdb *md, const char *value)
{
  const char *c;

  mhdb_printf(md, "\"");
  for (c = value; *c; c++) {
    if (! isascii(*c)) { mhdb_printf(md, "\\uFFFD"); continue; }
    if (isalnum(*c)) { mhdb_printf(md, "%c", *c); continue; }
    mhdb_printf(md, "\\u00%02x", *c);
  }
  mhdb_printf(md, "\"");
}


/* A convenience function to print a vanilla-ascii key and an
   unknown ancestry value as a JSON pair.  Add given suffix,
   which is likely to be a comma or a \n. */
void mhdb_print_key_value(struct mhdb *md, const char *key, const char *value,
                          const char *suffix)
{
  mhdb_printf(md, "\"%s\":", key);
  mhdb_print_qstring(md, value);
  mhdb_printf(md, "%s", suffix);
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
  char *metric_text;

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
  
  mhdb_printf (& mltc->mhdb, "{ ");
  mhdb_print_key_value (& mltc->mhdb, "name", metric, ",");
  rc = pmLookupText (metric_id, PM_TEXT_ONELINE, & metric_text);
  if (rc == 0) {
    mhdb_print_key_value (& mltc->mhdb, "text-oneline", metric_text, ",");
    free (metric_text);
  }
  rc = pmLookupText (metric_id, PM_TEXT_HELP, & metric_text);
  if (rc == 0) {
    mhdb_print_key_value (& mltc->mhdb, "text-help", metric_text, ",");
    free (metric_text);
  }
  mhdb_printf (& mltc->mhdb, "\"pmid\":%lu, ", (unsigned long) metric_id);
  if (metric_desc.indom != PM_INDOM_NULL)
    mhdb_printf (& mltc->mhdb, "\"indom\":%lu, ", (unsigned long) metric_desc.indom);
  mhdb_print_key_value (& mltc->mhdb, "sem",
                        (metric_desc.sem == PM_SEM_COUNTER ? "counter" :
                         metric_desc.sem == PM_SEM_INSTANT ? "instant" :
                         metric_desc.sem == PM_SEM_DISCRETE ? "discrete" :
                         "unknown"),
                        ",");
  mhdb_print_key_value (& mltc->mhdb, "units", 
                        pmUnitsStr (& metric_desc.units), ",");
  mhdb_print_key_value (& mltc->mhdb, "type",
                        pmTypeStr (metric_desc.type), "");
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
  pmTraversePMNS_r (val, & metric_list_traverse, & mltc);
  /* XXX: also handle pmids=... */
  /* XXX: also handle names=... */

  mhdb_printf(&mltc.mhdb, "] }");
  resp = mhdb_fini_response (& mltc.mhdb);
  if (resp == NULL) {
    pmweb_notify (LOG_ERR, connection, "mhdb_response failed\n");
    goto out;
  }
  rc = MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
  if (rc != MHD_YES) {
    pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
    goto out1;
  }
  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  if (rc != MHD_YES) {
    pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
    goto out1;
  }

  MHD_destroy_response (resp);
  return MHD_YES;

 out1:
  MHD_destroy_response (resp);
 out:
  return MHD_NO;
}


/* ------------------------------------------------------------------------ */


int pmwebapi_respond_metric_fetch (struct MHD_Connection *connection,
                                  struct webcontext *c)
{
  const char* val;
  const char* val2;
  struct MHD_Response* resp;
  int rc;
  int max_num_metrics;
  int num_metrics;
  int printed_metrics; /* exclude skipped ones */
  pmID *metrics;
  struct mhdb output;
  pmResult *results;
  int i;

  val = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "pmids");
  if (val == NULL) val = "";
  val2 = MHD_lookup_connection_value (connection, MHD_GET_ARGUMENT_KIND, "names");
  if (val2 == NULL) val2 = "";

  /* Pessimistically overestimate maximum number of pmID elements
     we'll need, to allocate the metrics[] array just once, and not have
     to range-check. */
  max_num_metrics = strlen(val)+strlen(val2); /* The real minimum is actually
                                                 closer to strlen()/2, to account
                                                 for commas. */
  num_metrics = 0;
  metrics = calloc ((size_t) max_num_metrics, sizeof(pmID));
  if (metrics == NULL) {
    pmweb_notify (LOG_ERR, connection, "calloc pmIDs[%d] oom\n", max_num_metrics);
    goto out;
  }

  /* Loop over names= names in val2, collect them in metrics[]. */
  while (*val2 != '\0') {
    char *name;
    char *name_end = strchr (val2, ',');
    char *names[1];
    pmID found_pmid;
    int num;

    /* Ignore plain "," XXX: elsewhere too? */
    if (*val2 == ',') {
      val2 ++;
      continue;
    }
      
    /* Copy just this name piece. */
    if (name_end) {
      name = strndup (val2, (name_end - val2));
      val2 = name_end + 1; /* skip past , */
    } else {
      name = strdup (val2);
      val2 += strlen (val2); /* skip onto \0 */
    }
    names[0] = name;

    num = pmLookupName (1, names, & found_pmid);

    if (num == 1) {
      assert (num_metrics < max_num_metrics);
      metrics[num_metrics++] = found_pmid;
    }      
  }


  /* Loop over pmids= numbers in val, append them to metrics[]. */
  while (*val) {
    char *numend; 
    unsigned long pmid = strtoul (val, & numend, 10); /* matches pmid printing above */
    if (numend == val) break; /* invalid contents */

    assert (num_metrics < max_num_metrics);
    metrics[num_metrics++] = pmid;

    if (*numend == '\0') break; /* end of string */
    if (*numend == ',')
      val = numend+1; /* advance to next string */
  }  

  /* Time to fetch the metric values. */
  if (num_metrics == 0) {
    free (metrics);
    pmweb_notify (LOG_ERR, connection, "no metrics requested\n");
    goto out;
  }

  rc = pmFetch (num_metrics, metrics, & results);
  free (metrics); /* don't need any more */
  if (rc < 0) {
    pmweb_notify (LOG_ERR, connection, "pmFetch failed\n");
    goto out;
  }
  /* NB: we don't care about the possibility of PMCD_*_AGENT bits
     being set, so rc > 0. */

  /* We need to construct a copy of the entire JSON metric value
     string, in one long malloc()'d buffer.  We size it generously to
     avoid having to realloc the bad boy and cause copies. */
  mhdb_init (& output, num_metrics * 200); /* WAG: per-metric size */
  mhdb_printf(& output, "{ \"timestamp\": { \"s\":%lu, \"us\":%lu },\n", 
              results->timestamp.tv_sec,
              results->timestamp.tv_usec);

  mhdb_printf(& output, "\"values\": [\n");

  assert (results->numpmid == num_metrics);
  printed_metrics = 0;
  for (i=0; i<results->numpmid; i++) {
    int j;
    pmValueSet *pvs = results->vset[i];
    char *metric_name;
    pmDesc desc;

    if (pvs->numval <= 0) continue; /* error code; skip metric */

    rc = pmLookupDesc (pvs->pmid, &desc); /* need to find desc.type only */
    if (rc < 0) continue;

    if (printed_metrics >= 1)
      mhdb_printf (& output, ",\n");
    mhdb_printf (& output, "{ ");

    mhdb_printf (& output, "\"pmid\":%lu, ", (unsigned long) pvs->pmid);
    rc = pmNameID (pvs->pmid, &metric_name);
    if (rc == 0) {
      mhdb_print_key_value (& output, "name", metric_name, ",");
      free (metric_name);
    }
    mhdb_printf (& output, "\"instances\": [\n");
    for (j=0; j<pvs->numval; j++) {
      pmAtomValue a;
      pmValue* val = & pvs->vlist[j];
      int printed_value = 1;

      if (desc.type == PM_TYPE_EVENT) continue;

      mhdb_printf (& output, "{");

      rc = pmExtractValue (pvs->valfmt, val, desc.type, &a, desc.type);
      if (rc == 0)
        switch (desc.type) {
        case PM_TYPE_32:
          mhdb_printf (& output, "\"value\":%i", a.l);
          break;

        case PM_TYPE_U32:
          mhdb_printf (& output, "\"value\":%u", a.ul);
          break;

        case PM_TYPE_64:
          mhdb_printf (& output, "\"value\":%" PRIi64, a.ll);
          break;

        case PM_TYPE_U64:
          mhdb_printf (& output, "\"value\":%" PRIu64, a.ull);
          break;

        case PM_TYPE_FLOAT:
          mhdb_printf (& output, "\"value\":%g", (double)a.f);
          break;

        case PM_TYPE_DOUBLE:
          mhdb_printf (& output, "\"value\":%g", a.d);
          break;

        case PM_TYPE_STRING:
          mhdb_print_key_value (& output, "value", a.cp, "");
          free (a.cp);
          break;

          /* XXX: base64- PM_TYPE_AGGREGATE etc., as per pmPrintValue */

        default:
          /* ... just a complete unknown ... */
          printed_value = 0;
        }

      if (desc.indom != PM_INDOM_NULL)
        mhdb_printf (& output, "%c \"instance\":%d", 
                     printed_value ? ',' : ' ', /* comma separation */
                     val->inst);
      mhdb_printf (& output, "}");
      if (j+1 < pvs->numval)
        mhdb_printf (& output, ","); /* comma separation */
    }
    mhdb_printf(& output, "] }"); /* iteration over instances */
    printed_metrics ++; /* comma separation at beginning of loop */
  }
  mhdb_printf(& output, "] }"); /* iteration over metrics */

  pmFreeResult (results); /* not needed any more */

  resp = mhdb_fini_response (& output);
  if (resp == NULL) {
    pmweb_notify (LOG_ERR, connection, "mhdb_response failed\n");
    goto out;
  }
  rc = MHD_add_response_header (resp, "Content-Type", JSON_MIMETYPE);
  if (rc != MHD_YES) {
    pmweb_notify (LOG_ERR, connection, "MHD_add_response_header failed\n");
    goto out1;
  }
  rc = MHD_queue_response (connection, MHD_HTTP_OK, resp);
  if (rc != MHD_YES) {
    pmweb_notify (LOG_ERR, connection, "MHD_queue_response failed\n");
    goto out1;
  }
  MHD_destroy_response (resp);
  return MHD_YES;

 out1:
  MHD_destroy_response (resp);
 out:
  return MHD_NO;
}


/* ------------------------------------------------------------------------ */


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
    pmweb_notify (LOG_WARNING, connection, "unknown method %s\n", method);
    goto out;
  }
  errno = 0;
  webapi_ctx = strtol (url, & context_command, 10); /* matches %d above */
  if (errno != 0
      || webapi_ctx <= 0 /* range check, plus string-nonemptyness check */
      || webapi_ctx > INT_MAX /* matches random() loop above */
      || *context_command != '/') { /* parsed up to the next slash */
    pmweb_notify (LOG_WARNING, connection, "unrecognized %s url %s \n", method, url);
    goto out;
  }
  context_command ++; /* skip the / */

  /* if-multithreaded: read-lock contexts */
  chn = __pmHashSearch ((int)webapi_ctx, & contexts);
  if (chn == NULL) {
    pmweb_notify (LOG_WARNING, connection, "unknown web context #%ld\n", webapi_ctx);
    goto out;
  }
  c = (struct webcontext *) chn->data;
  assert (c != NULL);

  /* Update last-use of this connection. */
  if (c->expires != 0) {
    time (& c->expires);
    c->expires += c->mypolltimeout;
  }

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

  /* ------------------------------------------------------------------------ */
  /* metric fetch: /context/$ID/_fetch */
  if (0 == strcmp (context_command, "_fetch") &&
      (0 == strcmp (method, "POST") || 0 == strcmp (method, "GET")))
    return pmwebapi_respond_metric_fetch (connection, c);


  pmweb_notify (LOG_WARNING, connection, "unrecognized %s context command %s \n", method, context_command);

out:
  return MHD_NO;
}
